#include "../fredb.h"
#include "test_utils.h"
#include <filesystem>

int main() {
  const std::string DIR = "./testdata/test_reopen_with_truncated_sst";
  std::filesystem::create_directories(DIR);
  {
    Fredb db(DIR, Config{.memtable_limit = 64});
    db.clear();
    for (int i = 0; i < 50; i++)
      db.put("k" + std::to_string(i), "v" + std::to_string(i));
    db.flush();
  }

  // truncate one .bin file to half its size
  for (auto &e : std::filesystem::directory_iterator(DIR)) {
    if (e.path().extension() == ".bin") {
      auto sz = std::filesystem::file_size(e.path());
      std::filesystem::resize_file(e.path(), sz / 2);
      break;
    }
  }

  // reopen must not crash
  bool ok = true;
  try {
    Fredb db2(DIR, Config{.memtable_limit = 64});
    // some keys may be lost — that's expected. just must not crash.
    (void)db2.get("k0");
  } catch (...) {
    ok = false;
  }

  check(ok, "reopen with truncated SST does not crash");

  return report();
}
