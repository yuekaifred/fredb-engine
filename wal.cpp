#include "wal.h"
#include <filesystem>
#include <fstream>

// format: [type: 1][klen: 4][vlen: 4][key][value]

void WAL::open_from_path(const std::string &p) {
  path = p;
  out.open(path, std::ios::binary | std::ios::app);
}

void WAL::append(uint8_t type, const std::string &key,
                 const std::string &value) {
  uint32_t klen = key.size();
  uint32_t vlen = value.size();
  out.write(reinterpret_cast<const char *>(&type), sizeof(type));
  out.write(reinterpret_cast<const char *>(&klen), sizeof(klen));
  out.write(reinterpret_cast<const char *>(&vlen), sizeof(vlen));
  out.write(key.data(), klen);
  out.write(value.data(), vlen);
  out.flush();
}

std::vector<std::tuple<uint8_t, std::string, std::string>> WAL::recover() {
  std::vector<std::tuple<uint8_t, std::string, std::string>> entries;
  std::ifstream f(path, std::ios::binary);
  if (!f.good())
    return entries;

  uint8_t type;
  uint32_t klen, vlen;
  while (f.read(reinterpret_cast<char *>(&type), sizeof(type))) {
    if (!f.read(reinterpret_cast<char *>(&klen), sizeof(klen)))
      break;
    if (!f.read(reinterpret_cast<char *>(&vlen), sizeof(vlen)))
      break;
    // anything bigger than this is obviously corrupt
    if (klen > 1024 * 1024 || vlen > 64 * 1024 * 1024)
      break;
    std::string key(klen, '\0');
    if (!f.read(key.data(), klen))
      break;
    std::string value(vlen, '\0');
    if (vlen > 0 && !f.read(value.data(), vlen))
      break;
    entries.emplace_back(type, std::move(key), std::move(value));
  }
  return entries;
}

void WAL::clear() {
  out.close();
  out.open(path, std::ios::binary | std::ios::trunc);
  out.close();
  out.open(path, std::ios::binary | std::ios::app);
}

void WAL::rotate(const std::string &archive_path) {
  out.flush();
  out.close();
  std::filesystem::rename(path, archive_path);
  out.open(path, std::ios::binary | std::ios::trunc);
  out.close();
  out.open(path, std::ios::binary | std::ios::app);
}
