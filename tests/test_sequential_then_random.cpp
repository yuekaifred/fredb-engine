#include "../fredb.h"
#include "test_utils.h"
#include <filesystem>
#include <random>

int main() {
  const std::string DIR = "./testdata/test_sequential_then_random";
  std::filesystem::create_directories(DIR);
  Fredb db(DIR, Config{.memtable_limit = 64 * 1024});
  db.clear();

  // sequential keys
  for (int i = 0; i < 1000; i++) {
    std::string k = std::to_string(10000 + i); // zero-padded-ish
    db.put(k, "seq_" + k);
  }

  // random-looking keys mixed in
  std::mt19937 rng(99);
  std::vector<std::string> rand_keys;
  for (int i = 0; i < 200; i++) {
    std::string k = "rnd_" + std::to_string(rng());
    db.put(k, "rval_" + std::to_string(i));
    rand_keys.push_back(k);
  }

  db.flush();

  bool seq_ok = true;
  for (int i = 0; i < 1000; i++) {
    std::string k = std::to_string(10000 + i);
    auto r = db.get(k);
    if (!r.has_value() || *r != "seq_" + k) {
      seq_ok = false;
      break;
    }
  }
  check(seq_ok, "all sequential keys correct after flush");

  bool rnd_ok = true;
  std::mt19937 rng2(99);
  for (int i = 0; i < 200; i++) {
    std::string k = "rnd_" + std::to_string(rng2());
    auto r = db.get(k);
    if (!r.has_value() || *r != "rval_" + std::to_string(i)) {
      rnd_ok = false;
      break;
    }
  }
  check(rnd_ok, "all random keys correct after flush");

  return report();
}
