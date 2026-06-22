#include "../fredb.h"
#include "test_utils.h"
#include <chrono>
#include <filesystem>
#include <thread>

int main() {
  const std::string DIR = "./testdata/test_compaction_drops_tombstones";
  std::filesystem::create_directories(DIR);
  {
    Fredb db(DIR, Config{.memtable_limit = 64, .l0_threshold = 2});
    db.clear();
    db.put("a", "1");
    db.flush();
    db.remove("a");
    db.flush(); // 2 L0 files → triggers background compaction
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    check(!db.get("a").has_value(), "key gone after compact");
  }

  Fredb db2(DIR, Config{.memtable_limit = 64});
  check(!db2.get("a").has_value(), "key still gone after reopen");

  return report();
}
