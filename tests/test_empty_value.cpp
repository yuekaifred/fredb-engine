#include "../fredb.h"
#include "test_utils.h"
#include <filesystem>

int main() {
  const std::string DIR = "./testdata/test_empty_value";
  std::filesystem::create_directories(DIR);
  Fredb db(DIR);
  db.clear();

  db.put("k", "");
  auto r1 = db.get("k");
  check(r1.has_value(), "empty-value key exists");
  check(r1.has_value() && r1->empty(), "empty-value key has empty string");

  db.remove("k");
  check(!db.get("k").has_value(), "removed empty-value key is gone");

  return report();
}
