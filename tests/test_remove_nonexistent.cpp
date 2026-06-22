#include "../fredb.h"
#include "test_utils.h"
#include <filesystem>

int main() {
  const std::string DIR = "./testdata/test_remove_nonexistent";
  std::filesystem::create_directories(DIR);
  Fredb db(DIR);
  db.clear();

  db.put("a", "1");
  db.remove("nonexistent_key");

  check(db.get("a").has_value() && *db.get("a") == "1", "other key unaffected");
  check(!db.get("nonexistent_key").has_value(), "never-put key still absent");

  return report();
}
