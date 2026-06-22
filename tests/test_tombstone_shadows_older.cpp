#include "../fredb.h"
#include "test_utils.h"
#include <filesystem>

int main() {
  const std::string DIR = "./testdata/test_tombstone_shadows_older";
  std::filesystem::create_directories(DIR);
  Fredb db(DIR, Config{.memtable_limit = 64});
  db.clear();

  db.put("a", "old");
  db.flush();
  db.remove("a");
  db.flush();

  check(!db.get("a").has_value(), "tombstone in newer SST shadows older put");

  return report();
}
