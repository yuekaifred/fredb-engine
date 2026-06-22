// EXAMPLE TEST — basic put / get / overwrite / remove.

#include "../fredb.h"
#include "test_utils.h"
#include <filesystem>

int main() {
  const std::string DIR = "./testdata/test_basic_crud";
  std::filesystem::create_directories(DIR);

  Fredb db(DIR);
  db.clear();

  // 1. put + get
  db.put("a", "1");
  auto r1 = db.get("a");
  check(r1.has_value() && *r1 == "1", "put then get returns inserted value");

  // 2. overwrite
  db.put("a", "2");
  auto r2 = db.get("a");
  check(r2.has_value() && *r2 == "2", "newer put shadows older put");

  // 3. remove
  db.remove("a");
  auto r3 = db.get("a");
  check(!r3.has_value(), "removed key returns empty optional");

  // 4. missing key
  auto r4 = db.get("never_inserted");
  check(!r4.has_value(), "never-inserted key returns empty optional");

  return report();
}
