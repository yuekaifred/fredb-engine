#pragma once
#include "bloom.h"
#include "memtable.h"
#include "wal.h"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

struct Config {
  size_t memtable_limit   = 4ULL * 1024 * 1024; // flush memtable at this size
  int    l0_threshold     = 4;                   // L0 file count before compaction
  size_t sparse_interval  = 4096;                // bytes between sparse index samples
  size_t sst_target_size  = 2ULL * 1024 * 1024; // split compaction output at this size
  size_t l1_max_bytes     = 10ULL * 1024 * 1024; // L1 size limit; each level 10x larger
};

class Fredb {
public:
  Fredb(const std::string &dir, Config cfg = {});
  ~Fredb();
  void put(const std::string &key, const std::string &value);
  std::optional<std::string> get(const std::string &key);
  void remove(const std::string &key);
  std::vector<std::pair<std::string, std::string>> get_range(const std::string &start, const std::string &end);
  void flush();
  void clear();

private:
  struct SSTInfo {
    int level;
    std::string min_key;
    std::string max_key;
    uint64_t file_size{0};
  };

  std::string dir;
  Config cfg;
  int next_sst_id;
  std::vector<std::vector<int>> levels;
  std::unordered_map<int, std::map<std::string, uint64_t>> sparse_indexes;
  std::unordered_map<int, SSTInfo> sst_info;
  std::unordered_map<int, size_t> level_sst_compact_pointer;
  std::vector<uint64_t> level_sizes;
  static constexpr uint8_t FLAGS_NORMAL = 0;
  static constexpr uint8_t FLAGS_TOMBSTONE = 1;
  uint64_t level_max_bytes(int l) const;

  Memtable current_memtable;
  std::optional<Memtable> immutable_memtable;
  WAL wal;
  BloomFilter bloom_;

  std::mutex mu;
  std::condition_variable compact_cv;
  std::condition_variable immutable_cv;
  std::atomic<bool> stop_flag{false};
  std::thread flush_thread;
  std::thread compact_thread;

  std::string sst_path(int id);
  // manifest stores sst_id level pairs. used for durability.
  std::string manifest_path();
  void write_manifest();
  void load_ssts();
  void rotate_memtable(std::unique_lock<std::mutex> &lock);
  void flush_immutable_memtable();
  void build_sparse_index(int id);
  void insert_into_level(int sst_id, int level);
  void read_sst_into(int id, std::map<std::string, std::optional<std::string>> &out);
  std::optional<std::optional<std::string>> scan_sst(int id, const std::string &key);
  void scan_sst_range(int id, const std::string &start, const std::string &end,
                      std::map<std::string, std::optional<std::string>> &out);
  bool needs_compaction();
  std::pair<std::vector<int>, int> pick_compaction();
  void compact(std::unique_lock<std::mutex> &lock, const std::vector<int> &ids, int target_level);
  void flush_worker();
  void compact_worker();
};
