#include "fredb.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>

uint64_t Fredb::level_max_bytes(int l) const {
  uint64_t bytes = cfg.l1_max_bytes;
  for (int i = 1; i < l; i++)
    bytes *= 10;
  return bytes;
}

bool Fredb::needs_compaction() {
  // L0 check
  if (!levels.empty() && (int)levels[0].size() >= cfg.l0_threshold)
    return true;
  // L1+ check
  for (size_t l = 1; l < levels.size(); l++) {
    if (level_sizes[l] >= level_max_bytes((int)l))
      return true;
  }
  return false;
}

std::pair<std::vector<int>, int> Fredb::pick_compaction() {
  // L0 full? compact all L0 and overlapping L1
  if (!levels.empty() && (int)levels[0].size() >= cfg.l0_threshold) {
    std::vector<int> to_compact_ids = levels[0];
    int target = 1;

    if ((int)levels.size() > 1) {
      std::string min_k = sst_info[to_compact_ids[0]].min_key;
      std::string max_k = sst_info[to_compact_ids[0]].max_key;
      for (size_t i = 1; i < to_compact_ids.size(); ++i) {
        int id = to_compact_ids[i];
        if (sst_info[id].min_key < min_k)
          min_k = sst_info[id].min_key;
        if (sst_info[id].max_key > max_k)
          max_k = sst_info[id].max_key;
      }
      for (int id : levels[1]) {
        auto &info = sst_info[id];
        if (info.min_key <= max_k && info.max_key >= min_k)
          to_compact_ids.push_back(id);
      }
    }
    return {to_compact_ids, target};
  }

  // L1+ full? compact one (round robin) file and overlapping L+1
  for (size_t l = 1; l < levels.size(); ++l) {
    auto &cur_level = levels[l];
    if (level_sizes[l] < level_max_bytes((int)l))
      continue;

    size_t &sst_id = level_sst_compact_pointer[l];
    if (sst_id >= cur_level.size())
      sst_id = 0;
    int picked = cur_level[sst_id++];

    std::vector<int> ids = {picked};
    int target = (int)l + 1;

    if ((int)levels.size() > target) {
      const std::string &min_k = sst_info[picked].min_key;
      const std::string &max_k = sst_info[picked].max_key;
      for (int id : levels[target]) {
        auto &info = sst_info[id];
        if (info.min_key <= max_k && info.max_key >= min_k)
          ids.push_back(id);
      }
    }
    return {ids, target};
  }

  return {{}, -1};
}

void Fredb::compact(std::unique_lock<std::mutex> &lock,
                    const std::vector<int> &sst_ids, int target_level) {
  if (sst_ids.empty())
    return;

  std::set<int> sst_id_set(sst_ids.begin(), sst_ids.end());

  // only drop tombstones if no SSTs exist in levels below target
  // (SSTs only compact upward, so higher levels are never in sst_id_set)
  bool can_drop_tombstones = true;
  for (size_t l = target_level + 1; l < levels.size() && can_drop_tombstones;
       l++) {
    if (!levels[l].empty())
      can_drop_tombstones = false;
  }

  // sort ids
  std::vector<int> sorted = sst_ids;
  std::sort(sorted.begin(), sorted.end(), [this](int a, int b) {
    int la = sst_info[a].level, lb = sst_info[b].level;
    if (la != lb)
      return la > lb;
    return a < b;
  });

  uint64_t total_input_bytes = 0;
  for (int id : sst_ids)
    total_input_bytes += sst_info[id].file_size;
  // +1 truncated integer division or 0 total_input_bytes (if all tombstones)
  int max_outputs = (int)(total_input_bytes / cfg.sst_target_size) + 1;
  std::vector<int> id_pool;
  for (int i = 0; i < max_outputs; i++)
    id_pool.push_back(next_sst_id++);

  lock.unlock();

  std::map<std::string, std::optional<std::string>> merged;
  for (int id : sorted)
    read_sst_into(id, merged);

  struct NewSST {
    std::map<std::string, uint64_t> idx;
    SSTInfo info;
  };
  std::vector<std::pair<int, NewSST>> new_sst_data;
  int pool_idx = 0;
  int cur_id = -1;
  std::ofstream out;
  uint64_t cur_size = 0;
  uint64_t last_sampled = std::numeric_limits<uint64_t>::max();

  // [&] capture everything in enclosing scope for ref
  auto finish_file = [&]() {
    if (cur_id < 0)
      return;
    out.close();
    new_sst_data.back().second.info.file_size = cur_size;
    cur_id = -1;
    cur_size = 0;
    last_sampled = std::numeric_limits<uint64_t>::max();
  };

  for (auto &[key, val] : merged) {
    if (!val.has_value() && can_drop_tombstones)
      continue;
    if (cur_id < 0) {
      cur_id = id_pool[pool_idx++];
      NewSST d;
      d.info.level = target_level;
      new_sst_data.push_back({cur_id, std::move(d)});
      out.open(sst_path(cur_id), std::ios::binary);
    }
    uint64_t entry_offset = cur_size;
    uint32_t klen = key.size();
    uint32_t vlen = val.has_value() ? val->size() : 0;
    uint8_t flags = val.has_value() ? FLAGS_NORMAL : FLAGS_TOMBSTONE;
    out.write(reinterpret_cast<char *>(&klen), sizeof(klen));
    out.write(reinterpret_cast<char *>(&vlen), sizeof(vlen));
    out.write(reinterpret_cast<char *>(&flags), sizeof(flags));
    out.write(key.data(), klen);
    if (val.has_value())
      out.write(val->data(), vlen);
    cur_size += sizeof(klen) + sizeof(vlen) + sizeof(flags) + klen + vlen;

    auto &d = new_sst_data.back().second;
    if (d.info.min_key.empty())
      d.info.min_key = key;
    d.info.max_key = key;
    if (d.idx.empty() || entry_offset - last_sampled >= cfg.sparse_interval) {
      d.idx[key] = entry_offset;
      last_sampled = entry_offset;
    }

    if (cur_size >= cfg.sst_target_size)
      finish_file();
  }
  finish_file();

  lock.lock();

  for (auto &[id, d] : new_sst_data) {
    sparse_indexes[id] = std::move(d.idx);
    sst_info[id] = d.info;
    insert_into_level(id, target_level);
  }

  for (int id : sst_ids) {
    level_sizes[sst_info[id].level] -= sst_info[id].file_size;
    std::filesystem::remove(sst_path(id));
    sparse_indexes.erase(id);
    sst_info.erase(id);
  }
  for (auto &lvl : levels)
    lvl.erase(
        std::remove_if(lvl.begin(), lvl.end(),
                       [&sst_id_set](int id) { return sst_id_set.count(id); }),
        lvl.end());

  write_manifest();
}

void Fredb::flush_worker() {
  while (true) {
    std::unique_lock<std::mutex> lock(mu);
    immutable_cv.wait(lock, [this] {
      return stop_flag.load() || immutable_memtable.has_value();
    });
    if (stop_flag && !immutable_memtable.has_value())
      return;
    lock.unlock();
    flush_immutable_memtable();
    compact_cv.notify_one();
  }
}

void Fredb::compact_worker() {
  while (true) {
    std::unique_lock<std::mutex> lock(mu);
    compact_cv.wait(lock,
                    [this] { return stop_flag.load() || needs_compaction(); });

    if (stop_flag)
      return;

    auto [ids, target] = pick_compaction();
    if (!ids.empty())
      compact(lock, ids, target);
  }
}
