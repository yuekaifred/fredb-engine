#pragma once
#include <cstdint>
#include <string>
#include <vector>

class BloomFilter {
public:
  // TODO size bloomfilter dynamically somehow...
  BloomFilter(size_t expected_items = 1'000'067, double fp_rate = 0.01);
  void insert(const std::string &key);
  bool maybe_contains(const std::string &key) const;
  void clear();

private:
  std::vector<uint8_t> bits_;
  size_t num_bits_;
  int num_hashes_;

  void set_bit(size_t i);
  bool get_bit(size_t i) const;
  std::pair<uint64_t, uint64_t> hash_pair(const std::string &key) const;
};
