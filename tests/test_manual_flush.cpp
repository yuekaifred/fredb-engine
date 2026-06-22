#include "../fredb.h"
#include "test_utils.h"
#include <filesystem>

static int bin_count(const std::string &dir) {
  int n = 0;
  for (auto &e : std::filesystem::directory_iterator(dir))
    if (e.path().extension() == ".bin")
      n++;
  return n;
}

int main() {
  const std::string DIR = "./testdata/test_manual_flush";
  std::filesystem::create_directories(DIR);
  Fredb db(DIR, Config{.memtable_limit = 16 * 1024 * 1024});
  db.clear();

  for (int i = 0; i < 10; i++)
    db.put("k" + std::to_string(i), "v" + std::to_string(i));

  check(bin_count(DIR) == 0, "no SST before manual flush");

  db.flush();
  check(bin_count(DIR) >= 1, "SST appeared after manual flush");

  bool all_ok = true;
  for (int i = 0; i < 10; i++) {
    auto r = db.get("k" + std::to_string(i));
    if (!r.has_value() || *r != "v" + std::to_string(i)) {
      all_ok = false;
      break;
    }
  }
  check(all_ok, "all keys readable after manual flush");

  return report();
}
