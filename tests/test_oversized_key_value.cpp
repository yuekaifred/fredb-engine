#include "../fredb.h"
#include "test_utils.h"
#include <filesystem>
#include <stdexcept>

// Excluded from scripts/test.sh's default run: each buffer here is >4GB
// (uint32_t max + 1) to cross the SST format's length limit, which needs
// several GB of RAM to materialize. Run manually with headroom to spare.

template <typename F> static bool throws(F fn) {
  try {
    fn();
    return false;
  } catch (const std::invalid_argument &) {
    return true;
  }
}

int main() {
  const std::string DIR = "./testdata/test_oversized_key_value";
  std::filesystem::create_directories(DIR);
  Fredb db(DIR);
  db.clear();

  std::string huge_key(size_t(std::numeric_limits<uint32_t>::max()) + 1, 'k');
  {
    std::string huge_val(size_t(std::numeric_limits<uint32_t>::max()) + 1, 'v');
    check(throws([&] { db.put("k", huge_val); }), "put oversized value throws");
  }
  check(throws([&] { db.put(huge_key, "v"); }), "put oversized key throws");
  check(throws([&] { db.get(huge_key); }), "get oversized key throws");
  check(throws([&] { db.remove(huge_key); }), "remove oversized key throws");

  return report();
}
