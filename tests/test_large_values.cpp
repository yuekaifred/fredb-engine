#include "../fredb.h"
#include "test_utils.h"
#include <filesystem>

int main() {
  const std::string DIR = "./testdata/test_large_values";
  std::filesystem::create_directories(DIR);
  Fredb db(DIR, Config{.memtable_limit = 64 * 1024});
  db.clear();

  for (int i = 0; i < 5; i++) {
    std::string val(200 * 1024, (char)('A' + i));
    db.put("big_" + std::to_string(i), val);
  }

  bool all_ok = true;
  for (int i = 0; i < 5; i++) {
    std::string expected(200 * 1024, (char)('A' + i));
    auto r = db.get("big_" + std::to_string(i));
    if (!r.has_value() || *r != expected) {
      all_ok = false;
      break;
    }
  }
  check(all_ok, "5 x 200KB values round-trip correctly");

  return report();
}
