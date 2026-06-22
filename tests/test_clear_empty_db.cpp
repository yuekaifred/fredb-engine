#include "../fredb.h"
#include "test_utils.h"
#include <filesystem>

int main() {
  const std::string DIR = "./testdata/test_clear_empty_db";
  std::filesystem::create_directories(DIR);
  Fredb db(DIR);
  db.clear(); // clear once to reset
  db.clear(); // clear on empty — must not crash

  db.put("a", "1");
  auto r = db.get("a");
  check(r.has_value() && *r == "1", "db usable after clear on empty db");

  return report();
}
