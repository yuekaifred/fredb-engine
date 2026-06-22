#pragma once
#include <map>
#include <optional>
#include <string>

class Memtable {
public:
  void put(const std::string &key, const std::string &value);
  void remove(const std::string &key);

  // returns nullopt if key not in memtable at all
  // returns optional<nullopt> (has_value=false) if key is a tombstone
  // returns optional<string> if key exists
  std::optional<std::optional<std::string>> get(const std::string &key) const;

  size_t byte_size() const;
  const std::map<std::string, std::optional<std::string>> &entries() const;

private:
  std::map<std::string, std::optional<std::string>> table;
  size_t bytes = 0;
};
