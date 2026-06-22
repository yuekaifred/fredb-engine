#include "../fredb.h"
#include "test_utils.h"
#include <filesystem>
#include <string>
#include <vector>

int main() {
  const std::string DIR = "./testdata/test_bloom_no_false_negatives";
  std::filesystem::create_directories(DIR);
  Fredb db(DIR, Config{.memtable_limit = 64 * 1024});
  db.clear();

  std::vector<std::string> keys;
  for (int i = 0; i < 5000; i++) {
    std::string k = "bloom_key_" + std::to_string(i);
    db.put(k, "val_" + std::to_string(i));
    keys.push_back(k);
  }
  db.flush();

  int missing = 0;
  for (auto &k : keys)
    if (!db.get(k).has_value())
      missing++;

  check(missing == 0, "no false negatives: all 5000 put keys are found");

  return report();
}
