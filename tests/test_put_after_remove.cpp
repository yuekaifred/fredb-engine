#include "../fredb.h"
#include "test_utils.h"
#include <filesystem>

int main() {
  const std::string DIR = "./testdata/test_put_after_remove";
  std::filesystem::create_directories(DIR);
  Fredb db(DIR);
  db.clear();

  db.put("a", "1");
  db.remove("a");
  check(!db.get("a").has_value(), "key gone after remove");

  db.put("a", "2");
  check(db.get("a").has_value() && *db.get("a") == "2",
        "key visible again after re-put");

  db.flush();
  check(db.get("a").has_value() && *db.get("a") == "2",
        "re-put survives flush");

  return report();
}
