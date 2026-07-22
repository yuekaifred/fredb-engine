#include "fredb_c.h"
#include "fredb.h"
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
thread_local std::string g_last_error;

void set_last_error(const std::exception &e) { g_last_error = e.what(); }

char *dup_bytes(const char *data, size_t len) {
  char *buf = static_cast<char *>(std::malloc(len));
  if (buf && len > 0)
    std::memcpy(buf, data, len);
  return buf;
}
} // namespace

extern "C" {

fredb_handle fredb_open(const char *dir, size_t dir_len) {
  try {
    return new Fredb(std::string(dir, dir_len));
  } catch (const std::exception &e) {
    set_last_error(e);
    return nullptr;
  }
}

void fredb_close(fredb_handle h) { delete static_cast<Fredb *>(h); }

int fredb_put(fredb_handle h, const char *key, size_t klen, const char *val,
              size_t vlen) {
  try {
    static_cast<Fredb *>(h)->put(std::string(key, klen),
                                 std::string(val, vlen));
    return 0;
  } catch (const std::exception &e) {
    set_last_error(e);
    return -1;
  }
}

int fredb_get(fredb_handle h, const char *key, size_t klen, char **out_val,
              size_t *out_vlen) {
  try {
    auto val = static_cast<Fredb *>(h)->get(std::string(key, klen));
    if (!val) {
      *out_val = nullptr;
      *out_vlen = 0;
      return 0;
    }
    *out_val = dup_bytes(val->data(), val->size());
    *out_vlen = val->size();
    return 1;
  } catch (const std::exception &e) {
    set_last_error(e);
    return -1;
  }
}

int fredb_remove(fredb_handle h, const char *key, size_t klen) {
  try {
    static_cast<Fredb *>(h)->remove(std::string(key, klen));
    return 0;
  } catch (const std::exception &e) {
    set_last_error(e);
    return -1;
  }
}

int64_t fredb_total_size(fredb_handle h) {
  try {
    return static_cast<int64_t>(static_cast<Fredb *>(h)->total_size());
  } catch (const std::exception &e) {
    set_last_error(e);
    return -1;
  }
}

int fredb_get_range(fredb_handle h, const char *start, size_t start_len,
                    const char *end, size_t end_len, char **out_buf,
                    size_t *out_len) {
  try {
    auto pairs = static_cast<Fredb *>(h)->get_range(
        std::string(start, start_len), std::string(end, end_len));

    size_t total = sizeof(uint32_t);
    for (auto &[k, v] : pairs)
      total += sizeof(uint32_t) + k.size() + sizeof(uint32_t) + v.size();

    char *buf = static_cast<char *>(std::malloc(total));
    char *p = buf;

    uint32_t count = static_cast<uint32_t>(pairs.size());
    std::memcpy(p, &count, sizeof(count));
    p += sizeof(count);

    for (auto &[k, v] : pairs) {
      uint32_t klen = static_cast<uint32_t>(k.size());
      std::memcpy(p, &klen, sizeof(klen));
      p += sizeof(klen);
      std::memcpy(p, k.data(), k.size());
      p += k.size();

      uint32_t vlen = static_cast<uint32_t>(v.size());
      std::memcpy(p, &vlen, sizeof(vlen));
      p += sizeof(vlen);
      std::memcpy(p, v.data(), v.size());
      p += v.size();
    }

    *out_buf = buf;
    *out_len = total;
    return 0;
  } catch (const std::exception &e) {
    set_last_error(e);
    return -1;
  }
}

void fredb_free_buf(char *buf) { std::free(buf); }

const char *fredb_last_error(void) { return g_last_error.c_str(); }
}
