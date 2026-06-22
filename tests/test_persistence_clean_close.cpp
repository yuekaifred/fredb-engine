#include "../fredb.h"
#include "test_utils.h"
#include <filesystem>

int main() {
  const std::string DIR = "./testdata/test_persistence_clean_close";
  std::filesystem::create_directories(DIR);
  {
    Fredb db(DIR);
    db.clear();
    for (int i = 0; i < 100; i++)
      db.put("k" + std::to_string(i), "v" + std::to_string(i));
  }

  Fredb db2(DIR);
  bool all_ok = true;
  for (int i = 0; i < 100; i++) {
    auto r = db2.get("k" + std::to_string(i));
    if (!r.has_value() || *r != "v" + std::to_string(i)) {
      all_ok = false;
      break;
    }
  }
  check(all_ok, "all 100 keys survive clean close + reopen");

  return report();
}
