#include "../fredb.h"
#include "test_utils.h"
#include <chrono>
#include <filesystem>
#include <thread>

int main() {
  const std::string DIR = "./testdata/test_l0_triggers_compaction";
  std::filesystem::create_directories(DIR);
  Fredb db(DIR, Config{.memtable_limit = 64, .l0_threshold = 4});
  db.clear();

  // tiny memtable → many flushes → L0 fills up → compaction
  for (int i = 0; i < 200; i++)
    db.put("k" + std::to_string(i), std::string(30, 'a'));

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  bool all_ok = true;
  for (int i = 0; i < 200; i++) {
    auto r = db.get("k" + std::to_string(i));
    if (!r.has_value()) {
      all_ok = false;
      break;
    }
  }
  check(all_ok, "all keys present after L0 compaction");

  return report();
}
