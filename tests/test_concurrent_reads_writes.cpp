#include "../fredb.h"
#include "test_utils.h"
#include <atomic>
#include <filesystem>
#include <random>
#include <thread>

int main() {
  const std::string DIR = "./testdata/test_concurrent_reads_writes";
  std::filesystem::create_directories(DIR);
  Fredb db(DIR, Config{.memtable_limit = 8 * 1024});
  db.clear();

  const int N = 1000;
  std::atomic<bool> done{false};
  std::atomic<int> read_errors{0};

  // writer: puts k0..k999
  std::thread writer([&] {
    for (int i = 0; i < N; i++)
      db.put("w_" + std::to_string(i), "v_" + std::to_string(i));
  });

  // 4 readers: get random keys, tolerate empty (key may not be written yet)
  std::vector<std::thread> readers;
  for (int t = 0; t < 4; t++) {
    readers.emplace_back([&, t] {
      std::mt19937 rng(t);
      std::uniform_int_distribution<int> dist(0, N - 1);
      while (!done) {
        int i = dist(rng);
        auto r = db.get("w_" + std::to_string(i));
        if (r.has_value() && *r != "v_" + std::to_string(i))
          read_errors++;
      }
    });
  }

  writer.join();
  done = true;
  for (auto &r : readers)
    r.join();

  check(read_errors == 0, "no corrupt reads during concurrent writes");

  db.flush();
  int missing = 0;
  for (int i = 0; i < N; i++)
    if (!db.get("w_" + std::to_string(i)).has_value())
      missing++;
  check(missing == 0, "all keys present after concurrent writes");

  return report();
}
