#include "bloom.h"
#include <cmath>

BloomFilter::BloomFilter(size_t expected_items, double fp_rate) {
  double ln2 = std::log(2.0);
  num_bits_ = (size_t)std::ceil(-(double)expected_items * std::log(fp_rate) /
                                (ln2 * ln2));
  num_bits_ = std::max(num_bits_, (size_t)1);
  num_hashes_ =
      std::max(1, (int)std::round((double)num_bits_ / expected_items * ln2));
  bits_.assign((num_bits_ + 7) / 8, 0);
}

std::pair<uint64_t, uint64_t>
BloomFilter::hash_pair(const std::string &key) const {
  uint64_t h1 = 14695981039346656037ULL;
  uint64_t h2 = 2166136261ULL;
  for (unsigned char c : key) {
    h1 = (h1 ^ c) * 1099511628211ULL;
    h2 = (h2 ^ c) * 16777619ULL;
  }
  return {h1, h2 | 1};
}

void BloomFilter::set_bit(size_t i) { bits_[i / 8] |= (1u << (i % 8)); }
bool BloomFilter::get_bit(size_t i) const {
  return bits_[i / 8] & (1u << (i % 8));
}

void BloomFilter::insert(const std::string &key) {
  auto [h1, h2] = hash_pair(key);
  for (int i = 0; i < num_hashes_; i++)
    set_bit((h1 + (uint64_t)i * h2) % num_bits_);
}

bool BloomFilter::maybe_contains(const std::string &key) const {
  auto [h1, h2] = hash_pair(key);
  for (int i = 0; i < num_hashes_; i++)
    if (!get_bit((h1 + (uint64_t)i * h2) % num_bits_))
      return false;
  return true;
}

void BloomFilter::clear() { std::fill(bits_.begin(), bits_.end(), 0); }
