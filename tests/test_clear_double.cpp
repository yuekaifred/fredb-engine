#include "../fredb.h"
#include "test_utils.h"
#include <filesystem>

int main() {
  const std::string DIR = "./testdata/test_clear_double";
  std::filesystem::create_directories(DIR);
  Fredb db(DIR);
  db.clear();

  for (int i = 0; i < 10; i++)
    db.put("k" + std::to_string(i), "v");
  db.flush();

  db.clear();
  db.clear(); // second clear must not crash

  db.put("a", "1");
  check(db.get("a").has_value() && *db.get("a") == "1",
        "db usable after double clear");

  return report();
}
