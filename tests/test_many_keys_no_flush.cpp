#include "../fredb.h"
#include "test_utils.h"
#include <filesystem>

int main() {
  const std::string DIR = "./testdata/test_many_keys_no_flush";
  std::filesystem::create_directories(DIR);
  Fredb db(DIR, Config{.memtable_limit = 16 * 1024 * 1024});
  db.clear();

  for (int i = 0; i < 1000; i++)
    db.put("key_" + std::to_string(i), "val_" + std::to_string(i));

  bool all_ok = true;
  for (int i = 0; i < 1000; i++) {
    auto r = db.get("key_" + std::to_string(i));
    if (!r.has_value() || *r != "val_" + std::to_string(i)) {
      all_ok = false;
      break;
    }
  }
  check(all_ok, "all 1000 keys readable from memtable");
  check(!db.get("key_1000").has_value(), "never-put key is absent");

  return report();
}
