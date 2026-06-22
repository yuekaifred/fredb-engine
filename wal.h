#pragma once
#include <cstdint>
#include <fstream>
#include <string>
#include <tuple>
#include <vector>

class WAL {
public:
  static constexpr uint8_t PUT = 0;
  static constexpr uint8_t DEL = 1;

  WAL() = default;
  void open_from_path(const std::string &path);
  void append(uint8_t type, const std::string &key, const std::string &value);
  std::vector<std::tuple<uint8_t, std::string, std::string>> recover();
  void clear();
  void rotate(const std::string &archive_path);

private:
  std::string path;
  std::ofstream out;
};
