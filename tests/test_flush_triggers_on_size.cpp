#include "../fredb.h"
#include "test_utils.h"
#include <chrono>
#include <filesystem>
#include <thread>

static int bin_count(const std::string &dir) {
  int n = 0;
  for (auto &e : std::filesystem::directory_iterator(dir))
    if (e.path().extension() == ".bin")
      n++;
  return n;
}

int main() {
  const std::string DIR = "./testdata/test_flush_triggers_on_size";
  std::filesystem::create_directories(DIR);
  Fredb db(DIR, Config{.memtable_limit = 1024});
  db.clear();

  int before = bin_count(DIR);

  std::string val(100, 'x');
  for (int i = 0; i < 50; i++)
    db.put("k" + std::to_string(i), val);

  // give background flush thread a moment
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  int after = bin_count(DIR);
  check(after > before, "flush triggered: SST files appeared on disk");

  bool all_ok = true;
  for (int i = 0; i < 50; i++) {
    auto r = db.get("k" + std::to_string(i));
    if (!r.has_value() || *r != val) {
      all_ok = false;
      break;
    }
  }
  check(all_ok, "all keys readable after auto-flush");

  return report();
}
