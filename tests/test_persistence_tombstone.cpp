#include "../fredb.h"
#include "test_utils.h"
#include <filesystem>

int main() {
  const std::string DIR = "./testdata/test_persistence_tombstone";
  std::filesystem::create_directories(DIR);
  {
    Fredb db(DIR);
    db.clear();
    db.put("a", "1");
    db.put("b", "2");
    db.remove("a");
  }

  Fredb db2(DIR);
  check(!db2.get("a").has_value(), "removed key stays gone after reopen");
  auto r = db2.get("b");
  check(r.has_value() && *r == "2", "surviving key intact after reopen");

  return report();
}
