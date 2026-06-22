#include "../fredb.h"
#include "test_utils.h"
#include <filesystem>

int main() {
  const std::string DIR = "./testdata/test_many_tombstones";
  std::filesystem::create_directories(DIR);
  Fredb db(DIR);
  db.clear();

  for (int i = 0; i < 5000; i++)
    db.remove("ghost_" + std::to_string(i));

  db.put("real", "yes");
  db.flush();

  check(db.get("real").has_value() && *db.get("real") == "yes",
        "real key intact");

  int false_pos = 0;
  for (int i = 0; i < 100; i++)
    if (db.get("ghost_" + std::to_string(i)).has_value())
      false_pos++;
  check(false_pos == 0, "ghost keys all absent");

  return report();
}
