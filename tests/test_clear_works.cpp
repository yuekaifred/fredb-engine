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
  const std::string DIR = "./testdata/test_clear_works";
  std::filesystem::create_directories(DIR);
  Fredb db(DIR);
  db.clear();

  // put some in SST and some only in memtable
  for (int i = 0; i < 100; i++)
    db.put("k" + std::to_string(i), "v" + std::to_string(i));
  db.flush();
  for (int i = 100; i < 150; i++)
    db.put("k" + std::to_string(i), "v" + std::to_string(i));

  db.clear();

  bool all_gone = true;
  for (int i = 0; i < 150; i++)
    if (db.get("k" + std::to_string(i)).has_value()) {
      all_gone = false;
      break;
    }
  check(all_gone, "all 150 keys gone after clear");
  check(bin_count(DIR) == 0, "no SST files remain after clear");

  // new puts must work
  for (int i = 0; i < 10; i++)
    db.put("new_" + std::to_string(i), "nv_" + std::to_string(i));

  bool new_ok = true;
  for (int i = 0; i < 10; i++) {
    auto r = db.get("new_" + std::to_string(i));
    if (!r.has_value() || *r != "nv_" + std::to_string(i)) {
      new_ok = false;
      break;
    }
  }
  check(new_ok, "new puts work after clear");

  // reopen: new keys survive, old keys still gone
  db.flush();
  {
    Fredb db2(DIR);
    bool new_persist = true;
    for (int i = 0; i < 10; i++) {
      auto r = db2.get("new_" + std::to_string(i));
      if (!r.has_value() || *r != "nv_" + std::to_string(i)) {
        new_persist = false;
        break;
      }
    }
    check(new_persist, "post-clear puts survive reopen");

    bool old_still_gone = true;
    for (int i = 0; i < 150; i++)
      if (db2.get("k" + std::to_string(i)).has_value()) {
        old_still_gone = false;
        break;
      }
    check(old_still_gone, "pre-clear keys still gone after reopen");
  }

  return report();
}
