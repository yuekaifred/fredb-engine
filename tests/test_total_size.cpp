#include "../fredb.h"
#include "test_utils.h"
#include <filesystem>

int main() {
  const std::string DIR = "./testdata/test_total_size";
  std::filesystem::create_directories(DIR);
  Fredb db(DIR, Config{.memtable_limit = 16 * 1024 * 1024});
  db.clear();

  check(db.total_size() == 0, "empty db has zero total size");

  db.put("k1", "v1");
  check(db.total_size() > 0, "total size grows after put (wal)");

  db.flush();
  uint64_t after_flush = db.total_size();
  check(after_flush > 0, "total size accounts for flushed sst bytes");

  for (int i = 0; i < 1000; i++)
    db.put("k" + std::to_string(i), "value_" + std::to_string(i));
  db.flush();

  check(db.total_size() > after_flush,
        "total size grows as more data is flushed");

  return report();
}
