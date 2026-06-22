#include "../fredb.h"
#include "test_utils.h"
#include <chrono>
#include <filesystem>
#include <thread>

int main() {
  const std::string DIR = "./testdata/test_compaction_preserves_updates";
  std::filesystem::create_directories(DIR);
  Fredb db(DIR, Config{.memtable_limit = 64});
  db.clear();

  for (int i = 1; i <= 5; i++) {
    db.put("k", "v" + std::to_string(i));
    db.flush();
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  auto r = db.get("k");
  check(r.has_value() && *r == "v5", "newest value survives compaction");

  return report();
}
