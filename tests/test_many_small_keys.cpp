#include "../fredb.h"
#include "test_utils.h"
#include <filesystem>
#include <random>

int main() {
  const std::string DIR = "./testdata/test_many_small_keys";
  std::filesystem::create_directories(DIR);
  Fredb db(DIR, Config{.memtable_limit = 256 * 1024});
  db.clear();

  const int N = 100000;
  for (int i = 0; i < N; i++)
    db.put("k" + std::to_string(i), "v" + std::to_string(i));

  std::mt19937 rng(42);
  std::uniform_int_distribution<int> dist(0, N - 1);

  int errors = 0;
  for (int s = 0; s < 1000; s++) {
    int i = dist(rng);
    auto r = db.get("k" + std::to_string(i));
    if (!r.has_value() || *r != "v" + std::to_string(i))
      errors++;
  }
  check(errors == 0, "1000 random samples from 100k keys all correct");

  return report();
}
