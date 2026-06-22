#include "../fredb.h"
#include "test_utils.h"
#include <filesystem>

int main() {
  const std::string DIR = "./testdata/test_bloom_rejects_missing";
  std::filesystem::create_directories(DIR);
  Fredb db(DIR, Config{.memtable_limit = 64 * 1024});
  db.clear();

  for (int i = 0; i < 1000; i++)
    db.put("real_" + std::to_string(i), "v");
  db.flush();

  int false_positives = 0;
  for (int i = 0; i < 1000; i++)
    if (db.get("fake_" + std::to_string(i)).has_value())
      false_positives++;

  check(false_positives == 0, "no fake keys returned as present");

  return report();
}
