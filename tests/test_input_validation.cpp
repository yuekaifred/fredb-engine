#include "../fredb.h"
#include "test_utils.h"
#include <filesystem>
#include <stdexcept>

template <typename F> static bool throws(F fn) {
  try {
    fn();
    return false;
  } catch (const std::invalid_argument &) {
    return true;
  }
}

int main() {
  const std::string DIR = "./testdata/test_input_validation";
  std::filesystem::create_directories(DIR);

  // --- bad Config ---
  check(throws([&] { Fredb db(DIR, Config{.memtable_limit = 0}); }),
        "memtable_limit=0 throws");
  check(throws([&] { Fredb db(DIR, Config{.l0_threshold = 0}); }),
        "l0_threshold=0 throws");
  check(throws([&] { Fredb db(DIR, Config{.sst_target_size = 0}); }),
        "sst_target_size=0 throws");
  check(throws([&] { Fredb db(DIR, Config{.l1_max_bytes = 0}); }),
        "l1_max_bytes=0 throws");
  check(throws([&] { Fredb db(DIR, Config{.sparse_interval = 0}); }),
        "sparse_interval=0 throws");
  check(throws([&] {
          Fredb db(DIR, Config{.sst_target_size = 20 * 1024 * 1024,
                               .l1_max_bytes = 10 * 1024 * 1024});
        }),
        "sst_target_size > l1_max_bytes throws");

  // --- bad keys / values ---
  Fredb db(DIR);
  db.clear();

  check(throws([&] { db.put("", "v"); }), "put empty key throws");
  check(throws([&] { db.get(""); }), "get empty key throws");
  check(throws([&] { db.remove(""); }), "remove empty key throws");

  std::string huge_key(size_t(std::numeric_limits<uint32_t>::max()) + 1, 'k');
  std::string huge_val(size_t(std::numeric_limits<uint32_t>::max()) + 1, 'v');
  check(throws([&] { db.put(huge_key, "v"); }), "put oversized key throws");
  check(throws([&] { db.put("k", huge_val); }), "put oversized value throws");
  check(throws([&] { db.get(huge_key); }), "get oversized key throws");
  check(throws([&] { db.remove(huge_key); }), "remove oversized key throws");

  // valid ops still work after failed ones
  db.put("ok", "1");
  check(db.get("ok").has_value() && *db.get("ok") == "1",
        "db usable after invalid calls");

  return report();
}
