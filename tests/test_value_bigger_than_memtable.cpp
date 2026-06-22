#include "../fredb.h"
#include "test_utils.h"
#include <filesystem>

int main() {
  const std::string DIR = "./testdata/test_value_bigger_than_memtable";
  std::filesystem::create_directories(DIR);
  Fredb db(DIR, Config{.memtable_limit = 4 * 1024});
  db.clear();

  std::string big(64 * 1024, 'X');
  db.put("big", big);

  auto r = db.get("big");
  check(r.has_value(), "oversized value is retrievable");
  check(r.has_value() && r->size() == big.size(),
        "oversized value has correct size");
  check(r.has_value() && *r == big, "oversized value has correct contents");

  // close + reopen
  {
    Fredb db2(DIR, Config{.memtable_limit = 4 * 1024});
    auto r2 = db2.get("big");
    check(r2.has_value() && *r2 == big, "oversized value survives reopen");
  }

  return report();
}
