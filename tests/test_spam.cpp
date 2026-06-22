#include "../fredb.h"
#include <filesystem>
#include <iostream>

int main() {
  const std::string DIR = "./testdata/test_spam";
  std::filesystem::create_directories(DIR);
  Fredb db(DIR, Config{
                    .memtable_limit = 4 * 1024 * 1024,
                    .l0_threshold = 4,
                    .sparse_interval = 4096,
                    .sst_target_size = 8 * 1024 * 1024,
                    .l1_max_bytes = 32 * 1024 * 1024,
                });
  db.clear();

  int i = 0;
  while (true) {
    db.put("key" + std::to_string(i), std::to_string(i));
    std::cout << i << "\n";
    i++;
  }
}
