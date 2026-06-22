#include "fredb.h"
#include "memtable.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>

// format: [klen: 4][vlen: 4][flags: 1][key][value]

std::string Fredb::sst_path(int id) {
  std::ostringstream ss;
  ss << dir << "/" << id << ".bin";
  return ss.str();
}

std::string Fredb::manifest_path() { return dir + "/MANIFEST"; }

void Fredb::write_manifest() {
  std::string tmp = manifest_path() + ".tmp";
  {
    std::ofstream output_file(tmp);
    for (auto &[id, info] : sst_info)
      output_file << id << " " << info.level << "\n";
  }
  std::filesystem::rename(tmp, manifest_path());
}

void Fredb::insert_into_level(int sst_id, int new_level) {
  while ((int)levels.size() <= new_level) {
    levels.push_back({});
    level_sizes.push_back(0);
  }
  level_sizes[new_level] += sst_info[sst_id].file_size;

  auto &lvl = levels[new_level];
  if (new_level == 0) {
    // L0 inserts by age
    lvl.push_back(sst_id);
  } else {
    // L1+ sorted by min_key
    auto it = std::lower_bound(lvl.begin(), lvl.end(), sst_info[sst_id].min_key,
                               [this](int a, const std::string &key) {
                                 return sst_info[a].min_key < key;
                               });
    lvl.insert(it, sst_id);
  }
}

void Fredb::load_ssts() {
  next_sst_id = 0;

  std::map<int, int> id_to_level;
  std::ifstream manifest_file(manifest_path());
  if (manifest_file.good()) {
    int sst_id, level;
    while (manifest_file >> sst_id >> level)
      id_to_level[sst_id] = level;
  }

  for (auto &entry : std::filesystem::directory_iterator(dir)) {
    std::string name = entry.path().filename().string();
    if (name == "MANIFEST")
      continue;
    if (name.size() > 4 && name.substr(name.size() - 4) == ".bin") {
      int sst_id = std::stoi(name.substr(0, name.size() - 4));
      next_sst_id = std::max(next_sst_id, sst_id + 1);
      int level = id_to_level.count(sst_id) ? id_to_level[sst_id] : 0;
      sst_info[sst_id].level = level;
      build_sparse_index(sst_id); // sets min/max keys
      insert_into_level(sst_id, level);
    }
  }

  // sort L0 for correct read order. remember we read newest first.
  if (!levels.empty())
    std::sort(levels[0].begin(), levels[0].end());
}

Fredb::Fredb(const std::string &dir, Config cfg)
    : dir(dir), cfg(cfg), next_sst_id(0) {
  if (cfg.memtable_limit == 0)
    throw std::invalid_argument(">:( memtable_limit must be > 0 or every put "
                                "will spin swapping the memtable");
  if (cfg.l0_threshold <= 0)
    throw std::invalid_argument(">:( l0_threshold must be > 0 or the "
                                "compaction worker will spin forever");
  if (cfg.sst_target_size == 0)
    throw std::invalid_argument(
        ">:( sst_target_size must be > 0 or every entry gets its own SST file");
  if (cfg.l1_max_bytes == 0)
    throw std::invalid_argument(
        ">:( l1_max_bytes must be > 0 or L1 will always be over budget");
  if (cfg.sparse_interval == 0)
    throw std::invalid_argument(">:( sparse_interval must be > 0 or every key "
                                "gets indexed, defeating the purpose");
  if (cfg.sst_target_size > cfg.l1_max_bytes)
    throw std::invalid_argument(
        ">:( sst_target_size must be <= l1_max_bytes or a single compaction "
        "output exceeds L1's budget, causing infinite compaction");

  std::filesystem::create_directories(dir);
  load_ssts();

  std::string imm_path = dir + "/WAL.imm";
  std::vector<std::tuple<uint8_t, std::string, std::string>> imm_entries;
  if (std::filesystem::exists(imm_path)) {
    WAL imm_wal;
    imm_wal.open_from_path(imm_path);
    imm_entries = imm_wal.recover();
  }

  wal.open_from_path(dir + "/WAL");
  auto cur_entries = wal.recover();

  if (!imm_entries.empty()) {
    // rewrite WAL with imm entries first so order is preserved
    wal.clear();
    for (auto &[type, key, value] : imm_entries)
      wal.append(type, key, value);
    for (auto &[type, key, value] : cur_entries)
      wal.append(type, key, value);
    std::filesystem::remove(imm_path);
  }

  // same here
  for (auto &[type, key, value] : imm_entries) {
    if (type == WAL::PUT)
      current_memtable.put(key, value);
    else
      current_memtable.remove(key);
    bloom_.insert(key);
  }
  for (auto &[type, key, value] : cur_entries) {
    if (type == WAL::PUT)
      current_memtable.put(key, value);
    else
      current_memtable.remove(key);
    bloom_.insert(key);
  }

  flush_thread = std::thread(&Fredb::flush_worker, this);
  compact_thread = std::thread(&Fredb::compact_worker, this);
}

Fredb::~Fredb() {
  {
    std::unique_lock<std::mutex> lock(mu);
    if (!current_memtable.entries().empty()) {
      immutable_cv.wait(lock,
                        [this] { return !immutable_memtable.has_value(); });
      immutable_memtable = std::move(current_memtable);
      current_memtable = Memtable{};
      wal.rotate(dir + "/WAL.imm");
      immutable_cv.notify_one();
    }
    immutable_cv.wait(lock, [this] { return !immutable_memtable.has_value(); });
  }
  stop_flag = true;
  immutable_cv.notify_all();
  compact_cv.notify_all();
  flush_thread.join();
  compact_thread.join();
}

void Fredb::build_sparse_index(int sst_id) {
  auto &idx = sparse_indexes[sst_id];
  auto &info = sst_info[sst_id];
  idx.clear();
  info.min_key.clear();
  info.max_key.clear();

  std::ifstream f(sst_path(sst_id), std::ios::binary);
  uint64_t last_sampled = std::numeric_limits<uint64_t>::max();
  bool first = true;
  uint32_t klen, vlen;

  // we COULD seek every SPARSE_INTERVAL offset and parse only those keys.
  // BUT linear read is 1. simpler and 2. sequential (which disks LOVE) so
  // performance gain is actually questionable
  // maybe have different logic for large sparse_interval
  while (true) {
    uint64_t offset = f.tellg();
    if (!f.read(reinterpret_cast<char *>(&klen), sizeof(klen)))
      break;
    if (!f.read(reinterpret_cast<char *>(&vlen), sizeof(vlen)))
      break;
    uint8_t flags;
    f.read(reinterpret_cast<char *>(&flags), sizeof(flags));
    std::string key(klen, '\0');
    if (!f.read(key.data(), klen))
      break;
    f.seekg(vlen, std::ios::cur);

    bloom_.insert(key);

    // remember sst is sorted!
    if (first) {
      info.min_key = key;
      first = false;
    }
    info.max_key = key;

    if (idx.empty() || offset - last_sampled >= cfg.sparse_interval) {
      idx[key] = offset;
      last_sampled = offset;
    }
  }
  info.file_size = std::filesystem::file_size(sst_path(sst_id));
}

void Fredb::rotate_memtable(std::unique_lock<std::mutex> &lock) {
  immutable_cv.wait(lock, [this] { return !immutable_memtable.has_value(); });
  immutable_memtable = std::move(current_memtable);
  current_memtable = Memtable{};
  wal.rotate(dir + "/WAL.imm");
  immutable_cv.notify_one();
}

void Fredb::flush_immutable_memtable() {

  std::unique_lock<std::mutex> lock(mu);
  if (!immutable_memtable.has_value())
    return;
  int sst_id = next_sst_id++;
  lock.unlock();

  {
    std::ofstream out(sst_path(sst_id), std::ios::binary);
    for (auto &[key, val] : immutable_memtable->entries()) {
      uint32_t klen = key.size();
      uint32_t vlen = val.has_value() ? val->size() : 0;
      uint8_t flags = val.has_value() ? FLAGS_NORMAL : FLAGS_TOMBSTONE;
      out.write(reinterpret_cast<char *>(&klen), sizeof(klen));
      out.write(reinterpret_cast<char *>(&vlen), sizeof(vlen));
      out.write(reinterpret_cast<char *>(&flags), sizeof(flags));
      out.write(key.data(), klen);
      if (val.has_value())
        out.write(val->data(), vlen);
    }
  }

  // build sparse index
  std::map<std::string, uint64_t> local_sparse_index;
  SSTInfo local_sst_info;
  local_sst_info.level = 0;
  {
    std::ifstream f(sst_path(sst_id), std::ios::binary);
    uint64_t last_sampled = std::numeric_limits<uint64_t>::max();
    bool first = true;
    uint32_t klen, vlen;
    while (true) {
      uint64_t offset = f.tellg();
      if (!f.read(reinterpret_cast<char *>(&klen), sizeof(klen)))
        break;
      if (!f.read(reinterpret_cast<char *>(&vlen), sizeof(vlen)))
        break;
      uint8_t flags;
      f.read(reinterpret_cast<char *>(&flags), sizeof(flags));
      std::string key(klen, '\0');
      if (!f.read(key.data(), klen))
        break;
      f.seekg(vlen, std::ios::cur);
      if (first) {
        local_sst_info.min_key = key;
        first = false;
      }
      local_sst_info.max_key = key;
      if (local_sparse_index.empty() ||
          offset - last_sampled >= cfg.sparse_interval) {
        local_sparse_index[key] = offset;
        last_sampled = offset;
      }
    }
    local_sst_info.file_size = std::filesystem::file_size(sst_path(sst_id));
  }

  lock.lock();
  sparse_indexes[sst_id] = std::move(local_sparse_index);
  sst_info[sst_id] = local_sst_info;
  insert_into_level(sst_id, 0);
  write_manifest();
  std::filesystem::remove(dir + "/WAL.imm");
  immutable_memtable = std::nullopt;
  immutable_cv.notify_all();
}

void Fredb::put(const std::string &key, const std::string &value) {
  if (key.empty())
    throw std::invalid_argument(">:( key must not be empty");
  if (key.size() > std::numeric_limits<uint32_t>::max())
    throw std::invalid_argument(
        ">:( key too large, SST format stores key length as uint32_t");
  if (value.size() > std::numeric_limits<uint32_t>::max())
    throw std::invalid_argument(
        ">:( value too large, SST format stores value length as uint32_t");

  std::unique_lock<std::mutex> lock(mu);
  wal.append(WAL::PUT, key, value);
  current_memtable.put(key, value);
  bloom_.insert(key);
  if (current_memtable.byte_size() >= cfg.memtable_limit)
    rotate_memtable(lock);
}

void Fredb::remove(const std::string &key) {
  if (key.empty())
    throw std::invalid_argument(">:( key must not be empty");
  if (key.size() > std::numeric_limits<uint32_t>::max())
    throw std::invalid_argument(
        ">:( key too large, SST format stores key length as uint32_t");

  std::unique_lock<std::mutex> lock(mu);
  wal.append(WAL::DEL, key, "");
  current_memtable.remove(key);
  if (current_memtable.byte_size() >= cfg.memtable_limit)
    rotate_memtable(lock);
}

std::optional<std::optional<std::string>>
Fredb::scan_sst(int id, const std::string &key) {
  auto &idx = sparse_indexes[id];

  // start right before the first item in sparse index bigger than key
  uint64_t start_offset = 0;
  if (!idx.empty()) {
    auto it = idx.upper_bound(key);
    if (it != idx.begin())
      start_offset = (--it)->second;
  }

  std::ifstream f(sst_path(id), std::ios::binary);
  f.seekg(start_offset);

  // find key
  uint32_t klen, vlen;
  while (f.read(reinterpret_cast<char *>(&klen), sizeof(klen)) &&
         f.read(reinterpret_cast<char *>(&vlen), sizeof(vlen))) {
    uint8_t flags;
    f.read(reinterpret_cast<char *>(&flags), sizeof(flags));
    std::string k(klen, '\0');
    if (!f.read(k.data(), klen))
      break;

    if (k > key)
      return std::nullopt;
    if (k == key) {
      if (flags == FLAGS_TOMBSTONE)
        return std::optional<std::string>{};
      std::string value(vlen, '\0');
      if (!f.read(value.data(), vlen))
        break;
      return value;
    }
    f.seekg(vlen, std::ios::cur);
  }
  return std::nullopt;
}

void Fredb::scan_sst_range(
    int id, const std::string &start, const std::string &end,
    std::map<std::string, std::optional<std::string>> &out) {
  // start one before upper bound
  auto &idx = sparse_indexes[id];
  uint64_t start_offset = 0;
  if (!idx.empty()) {
    // find last index entry whose key does not exceed start
    auto it = idx.upper_bound(start);
    if (it != idx.begin())
      start_offset = (--it)->second;
  }

  std::ifstream f(sst_path(id), std::ios::binary);
  f.seekg(start_offset);
  uint32_t klen, vlen;
  while (f.read(reinterpret_cast<char *>(&klen), sizeof(klen)) &&
         f.read(reinterpret_cast<char *>(&vlen), sizeof(vlen))) {
    uint8_t flags;
    f.read(reinterpret_cast<char *>(&flags), sizeof(flags));
    std::string key(klen, '\0');
    if (!f.read(key.data(), klen))
      break;
    if (key > end)
      break;
    if (key < start) {
      f.seekg(vlen, std::ios::cur);
      continue;
    } // vlen==0 for tombstones, seekg(0) is a no-op
    if (flags == FLAGS_TOMBSTONE) {
      out[key] = std::nullopt; // vlen==0, nothing to read
    } else {
      std::string val(vlen, '\0');
      if (!f.read(val.data(), vlen))
        break;
      out[key] = val;
    }
  }
}

std::vector<std::pair<std::string, std::string>>
Fredb::get_range(const std::string &start, const std::string &end) {
  if (start.empty() || end.empty())
    throw std::invalid_argument(">:( start and end keys must not be empty");
  if (start > end)
    return {};

  std::lock_guard<std::mutex> lock(mu);
  std::map<std::string, std::optional<std::string>> merged_sst_map;

  // scan levels, deepest first
  for (int l = (int)levels.size() - 1; l >= 1; l--) {
    auto &lvl = levels[l];
    auto it = std::lower_bound(lvl.begin(), lvl.end(), start,
                               [this](int id, const std::string &k) {
                                 return sst_info[id].max_key < k;
                               });
    for (; it != lvl.end() && sst_info[*it].min_key <= end; ++it)
      scan_sst_range(*it, start, end, merged_sst_map);
  }

  // L0: oldest (index 0) to newest (last)
  if (!levels.empty()) {
    for (int id : levels[0]) {
      auto &info = sst_info[id];
      if (info.max_key < start || info.min_key > end)
        continue;
      scan_sst_range(id, start, end, merged_sst_map);
    }
  }

  // immutable memtable
  if (immutable_memtable.has_value()) {
    auto &entries = immutable_memtable->entries();
    for (auto it = entries.lower_bound(start);
         it != entries.end() && it->first <= end; ++it)
      merged_sst_map[it->first] = it->second;
  }

  // active memtable
  {
    auto &entries = current_memtable.entries();
    for (auto it = entries.lower_bound(start);
         it != entries.end() && it->first <= end; ++it)
      merged_sst_map[it->first] = it->second;
  }

  std::vector<std::pair<std::string, std::string>> result;
  for (auto &[k, v] : merged_sst_map)
    if (v.has_value())
      result.emplace_back(k, *v);
  return result;
}

std::optional<std::string> Fredb::get(const std::string &key) {
  if (key.empty())
    throw std::invalid_argument(">:( key must not be empty");
  if (key.size() > std::numeric_limits<uint32_t>::max())
    throw std::invalid_argument(
        ">:( key too large, SST format stores key length as uint32_t");

  std::lock_guard<std::mutex> lock(mu);

  auto mem = current_memtable.get(key);
  if (mem.has_value())
    return *mem;

  if (immutable_memtable.has_value()) {
    auto imm = immutable_memtable->get(key);
    if (imm.has_value())
      return *imm;
  }

  if (!bloom_.maybe_contains(key))
    return std::nullopt;

  // L0 check
  if (!levels.empty()) {
    for (int i = (int)levels[0].size() - 1; i >= 0; i--) {
      auto result = scan_sst(levels[0][i], key);
      if (result.has_value())
        return *result;
    }
  }

  // L1+ check
  for (size_t l = 1; l < levels.size(); l++) {
    auto &level = levels[l];
    // find first file where min_key is > key, the previous file must contain it
    auto it = std::upper_bound(level.begin(), level.end(), key,
                               [this](const std::string &k, int id) {
                                 return k < sst_info[id].min_key;
                               });
    if (it == level.begin())
      continue;
    --it;
    int id = *it;
    if (sst_info[id].max_key < key)
      continue;
    auto result = scan_sst(id, key);
    if (result.has_value())
      return *result;
  }

  return std::nullopt;
}

void Fredb::read_sst_into(
    int id, std::map<std::string, std::optional<std::string>> &out) {
  std::ifstream f(sst_path(id), std::ios::binary);
  uint32_t klen, vlen;
  while (f.read(reinterpret_cast<char *>(&klen), sizeof(klen)) &&
         f.read(reinterpret_cast<char *>(&vlen), sizeof(vlen))) {
    uint8_t flags;
    f.read(reinterpret_cast<char *>(&flags), sizeof(flags));
    std::string key(klen, '\0');
    if (!f.read(key.data(), klen))
      break;
    if (flags == FLAGS_TOMBSTONE) {
      // just store value as nullopt if is tombstone.
      // we can never have value when tombstone and vice versa
      out[key] = std::nullopt;
    } else {
      std::string value(vlen, '\0');
      if (!f.read(value.data(), vlen))
        break;
      out[key] = value;
    }
  }
}

void Fredb::flush() {
  std::unique_lock<std::mutex> lock(mu);
  if (!current_memtable.entries().empty()) {
    immutable_cv.wait(lock, [this] { return !immutable_memtable.has_value(); });
    immutable_memtable = std::move(current_memtable);
    current_memtable = Memtable{};
    wal.rotate(dir + "/WAL.imm");
    immutable_cv.notify_one();
  }
  immutable_cv.wait(lock, [this] { return !immutable_memtable.has_value(); });
}

void Fredb::clear() {
  std::unique_lock<std::mutex> lock(mu);
  immutable_cv.wait(lock, [this] { return !immutable_memtable.has_value(); });
  for (auto &entry : std::filesystem::directory_iterator(dir))
    std::filesystem::remove(entry.path());
  wal.clear();
  current_memtable = Memtable{};
  immutable_memtable = std::nullopt;
  levels.clear();
  level_sizes.clear();
  sst_info.clear();
  sparse_indexes.clear();
  bloom_.clear();
  next_sst_id = 0;
}
