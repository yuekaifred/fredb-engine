#include "../fredb.h"
#include "test_utils.h"
#include <filesystem>

int main() {
  const std::string DIR = "./testdata/test_ordering_newest_wins";
  std::filesystem::create_directories(DIR);
  Fredb db(DIR, Config{.memtable_limit = 1024});
  db.clear();

  db.put("x", "v1");
  db.flush();
  db.put("x", "v2");
  db.flush();
  db.put("x", "v3");
  db.flush();

  auto r = db.get("x");
  check(r.has_value() && *r == "v3", "newest put wins across flushes");

  return report();
}
