#include "../fredb.h"
#include "test_utils.h"
#include <filesystem>

int main() {
  const std::string DIR = "./testdata/test_reopen_with_no_manifest";
  std::filesystem::create_directories(DIR);
  {
    Fredb db(DIR, Config{.memtable_limit = 64});
    db.clear();
    for (int i = 0; i < 50; i++)
      db.put("k" + std::to_string(i), "v" + std::to_string(i));
    db.flush();
  }

  std::filesystem::remove(DIR + "/MANIFEST");

  // reopen without manifest — must not crash
  Fredb db2(DIR, Config{.memtable_limit = 64});
  int found = 0;
  for (int i = 0; i < 50; i++)
    if (db2.get("k" + std::to_string(i)).has_value())
      found++;

  check(found == 50, "all 50 keys survive reopen with no manifest");

  return report();
}
