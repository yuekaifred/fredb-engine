#include "../fredb.h"
#include "test_utils.h"
#include <filesystem>

int main() {
  const std::string DIR = "./testdata/test_remove_range";
  std::filesystem::create_directories(DIR);
  Fredb db(DIR);
  db.clear();

  // basic range removal
  db.put("a", "1");
  db.put("b", "2");
  db.put("c", "3");
  db.put("d", "4");
  db.remove_range("b", "c");
  auto r1 = db.get_range("a", "d");
  check(r1.size() == 2, "basic remove_range leaves 2 keys");
  check(r1[0].first == "a" && r1[1].first == "d",
        "remaining keys are a and d");
  check(!db.get("b").has_value(), "b removed");
  check(!db.get("c").has_value(), "c removed");

  // single key range
  db.clear();
  db.put("a", "1");
  db.put("b", "2");
  db.remove_range("a", "a");
  check(!db.get("a").has_value(), "single-key range removes a");
  check(db.get("b").has_value(), "b untouched by single-key range");

  // inverted range is a no-op
  db.clear();
  db.put("a", "1");
  db.put("b", "2");
  db.remove_range("z", "a");
  check(db.get("a").has_value() && db.get("b").has_value(),
        "inverted range removes nothing");

  // across flush
  db.clear();
  db.put("a", "1");
  db.put("c", "3");
  db.flush();
  db.put("b", "2");
  db.put("d", "4");
  db.remove_range("b", "c");
  auto r2 = db.get_range("a", "d");
  check(r2.size() == 2, "remove_range spans flushed and in-memory data");
  check(r2[0].first == "a" && r2[1].first == "d",
        "remaining keys after cross-flush removal are a and d");

  return report();
}
