#include "../fredb.h"
#include "test_utils.h"
#include <filesystem>
#include <fstream>
#include <random>

int main() {
  const std::string DIR = "./testdata/test_reopen_with_corrupted_wal";
  std::filesystem::create_directories(DIR);
  {
    Fredb db(DIR, Config{.memtable_limit = 16 * 1024 * 1024});
    db.clear();
    for (int i = 0; i < 5; i++)
      db.put("k" + std::to_string(i), "v" + std::to_string(i));
    // no flush — keys stay in WAL
  }

  // append 1KB of random garbage to WAL
  std::ofstream wal(DIR + "/WAL", std::ios::binary | std::ios::app);
  std::mt19937 rng(42);
  for (int i = 0; i < 1024; i++)
    wal.put((char)(rng() & 0xFF));
  wal.close();

  // reopen must not crash or OOM
  bool ok = true;
  try {
    Fredb db2(DIR, Config{.memtable_limit = 16 * 1024 * 1024});
    // pre-corruption keys must survive
    int found = 0;
    for (int i = 0; i < 5; i++)
      if (db2.get("k" + std::to_string(i)).has_value())
        found++;
    check(found == 5, "pre-corruption keys survive WAL garbage tail");
  } catch (...) {
    ok = false;
  }

  check(ok, "reopen with corrupted WAL does not crash");

  return report();
}
