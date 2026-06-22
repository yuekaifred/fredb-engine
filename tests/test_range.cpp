#include "../fredb.h"
#include "test_utils.h"
#include <filesystem>

int main() {
  const std::string DIR = "./testdata/test_range";
  std::filesystem::create_directories(DIR);
  Fredb db(DIR);
  db.clear();

  // basic range
  db.put("a", "1");
  db.put("b", "2");
  db.put("c", "3");
  db.put("d", "4");
  auto r1 = db.get_range("b", "c");
  check(r1.size() == 2, "basic range returns 2 keys");
  check(r1[0].first == "b" && r1[0].second == "2",
        "basic range first key correct");
  check(r1[1].first == "c" && r1[1].second == "3",
        "basic range second key correct");

  // single key
  auto r2 = db.get_range("b", "b");
  check(r2.size() == 1 && r2[0].first == "b",
        "single-key range returns one entry");

  // inverted range
  auto r3 = db.get_range("z", "a");
  check(r3.empty(), "inverted range returns empty");

  // tombstones excluded
  db.remove("b");
  auto r4 = db.get_range("a", "c");
  check(r4.size() == 2, "tombstoned key excluded from range");
  check(r4[0].first == "a" && r4[1].first == "c", "remaining keys are a and c");

  // across flush
  db.clear();
  db.put("a", "1");
  db.put("c", "3");
  db.flush();
  db.put("b", "2");
  db.put("d", "4");
  auto r5 = db.get_range("a", "d");
  check(r5.size() == 4, "range spans flushed and in-memory data");

  return report();
}
