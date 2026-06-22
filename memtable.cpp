#include "memtable.h"

void Memtable::put(const std::string &key, const std::string &value) {
  auto it = table.find(key);
  if (it != table.end()) {
    if (it->second)
      bytes -= it->second->size();
  } else {
    bytes += key.size();
  }
  bytes += value.size();
  table[key] = value;
}

void Memtable::remove(const std::string &key) {
  auto it = table.find(key);
  if (it != table.end()) {
    if (it->second)
      bytes -= it->second->size();
  } else {
    bytes += key.size();
  }
  table[key] = std::nullopt;
}

std::optional<std::optional<std::string>>
Memtable::get(const std::string &key) const {
  auto it = table.find(key);
  if (it == table.end())
    return std::nullopt;
  return it->second;
}

size_t Memtable::byte_size() const { return bytes; }

const std::map<std::string, std::optional<std::string>> &
Memtable::entries() const {
  return table;
}
