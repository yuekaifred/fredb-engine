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
  const std::string DIR = "./testdata/test_empty_key";
  std::filesystem::create_directories(DIR);
  Fredb db(DIR);
  db.clear();

  check(throws([&] { db.put("", "v"); }), "put empty key throws");
  check(throws([&] { db.get(""); }), "get empty key throws");
  check(throws([&] { db.remove(""); }), "remove empty key throws");

  // non-empty keys still work
  db.put("a", "1");
  check(db.get("a").has_value() && *db.get("a") == "1",
        "normal key works after empty-key attempts");

  return report();
}
