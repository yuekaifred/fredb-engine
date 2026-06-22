#include "../fredb.h"
#include "test_utils.h"
#include <filesystem>

int main() {
  const std::string DIR = "./testdata/test_clear_resets_bloom";
  std::filesystem::create_directories(DIR);
  Fredb db(DIR);
  db.clear();

  db.put("ghost", "v");
  db.flush();
  db.clear();

  check(!db.get("ghost").has_value(), "cleared key is gone");

  // 100 never-put keys must not crash or return values
  int false_pos = 0;
  for (int i = 0; i < 100; i++)
    if (db.get("never_" + std::to_string(i)).has_value())
      false_pos++;
  check(false_pos == 0, "no ghost keys after clear");

  return report();
}
