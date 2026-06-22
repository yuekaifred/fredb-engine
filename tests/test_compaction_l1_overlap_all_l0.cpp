// Regression: pick_compaction must compute key range over ALL L0 SSTs, not just
// a subset. If it skips SSTs, it misses L1 files that overlap only those SSTs,
// leaving stale data in L1 that shadows the newer write.

#include "../fredb.h"
#include "test_utils.h"
#include <chrono>
#include <filesystem>
#include <thread>

int main() {
  const std::string DIR = "./testdata/test_compaction_l1_overlap_all_l0";
  std::filesystem::create_directories(DIR);
  Fredb db(DIR, Config{.memtable_limit = 64, .l0_threshold = 3});
  db.clear();

  // Phase 1: land "a"="old" in L1 via a 3-SST compaction.
  db.put("a", "old");
  db.flush();
  db.put("b", "x");
  db.flush();
  db.put("c", "x");
  db.flush();
  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  // Phase 2: three new L0 SSTs. "a"="NEW" is flushed LAST so it ends up at
  // ids[2] — the index a buggy i+=2 loop skips. The range computed from only
  // ids[0]="m" and ids[1]="n" is "m"-"n", which does NOT overlap the L1 SST
  // "a"-"c", so the old SST survives and shadows the newer write.
  db.put("m", "x");
  db.flush();
  db.put("n", "x");
  db.flush();
  db.put("a", "NEW");
  db.flush();
  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  auto r = db.get("a");
  check(r.has_value() && *r == "NEW",
        "newer write to 'a' must win after L0->L1 compaction");

  return report();
}
