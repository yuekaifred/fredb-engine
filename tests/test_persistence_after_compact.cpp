#include "../fredb.h"
#include "test_utils.h"
#include <filesystem>

int main() {
  const std::string DIR = "./testdata/test_persistence_after_compact";
  std::filesystem::create_directories(DIR);
  {
    Fredb db(DIR, Config{.memtable_limit = 64});
    db.clear();
    for (int i = 0; i < 50; i++)
      db.put("k" + std::to_string(i), "v" + std::to_string(i));
    db.flush();
  }

  Fredb db2(DIR, Config{.memtable_limit = 64});
  bool all_ok = true;
  for (int i = 0; i < 50; i++) {
    auto r = db2.get("k" + std::to_string(i));
    if (!r.has_value() || *r != "v" + std::to_string(i)) {
      all_ok = false;
      break;
    }
  }
  check(all_ok, "all keys survive reopen after compaction");

  return report();
}
