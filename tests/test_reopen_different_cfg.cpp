#include "../fredb.h"
#include "test_utils.h"
#include <filesystem>
#include <stdexcept>

static const std::string DIR = "./testdata/test_reopen_different_cfg";

static void seed(const std::string &dir, Config cfg, int n) {
  std::filesystem::create_directories(dir);
  Fredb db(dir, cfg);
  db.clear();
  for (int i = 0; i < n; i++)
    db.put("k" + std::to_string(i), "v" + std::to_string(i));
}

static bool verify(const std::string &dir, Config cfg, int n) {
  Fredb db(dir, cfg);
  for (int i = 0; i < n; i++) {
    auto r = db.get("k" + std::to_string(i));
    if (!r.has_value() || *r != "v" + std::to_string(i))
      return false;
  }
  return true;
}

int main() {
  // larger memtable_limit on reopen — data still readable
  {
    std::string dir = DIR + "/memtable_limit";
    Config small_cfg;
    small_cfg.memtable_limit = 512;
    seed(dir, small_cfg, 50);

    Config big_cfg;
    big_cfg.memtable_limit = 64 * 1024 * 1024;
    check(verify(dir, big_cfg, 50),
          "larger memtable_limit on reopen: data survives");
  }

  // smaller memtable_limit on reopen — data still readable
  {
    std::string dir = DIR + "/memtable_limit_smaller";
    Config big_cfg;
    big_cfg.memtable_limit = 64 * 1024 * 1024;
    seed(dir, big_cfg, 50);

    Config small_cfg;
    small_cfg.memtable_limit = 512;
    check(verify(dir, small_cfg, 50),
          "smaller memtable_limit on reopen: data survives");
  }

  // different l0_threshold — data still readable
  {
    std::string dir = DIR + "/l0_threshold";
    Config cfg1;
    cfg1.memtable_limit = 512;
    cfg1.l0_threshold = 2;
    seed(dir, cfg1, 50);

    Config cfg2;
    cfg2.l0_threshold = 8;
    check(verify(dir, cfg2, 50),
          "different l0_threshold on reopen: data survives");
  }

  // different sparse_interval — index rebuilt, data still readable
  {
    std::string dir = DIR + "/sparse_interval";
    Config cfg1;
    cfg1.memtable_limit = 512;
    cfg1.sparse_interval = 256;
    seed(dir, cfg1, 50);

    Config cfg2;
    cfg2.sparse_interval = 8192;
    check(verify(dir, cfg2, 50),
          "different sparse_interval on reopen: data survives");
  }

  // different sst_target_size — old SSTs untouched, data still readable
  {
    std::string dir = DIR + "/sst_target_size";
    Config cfg1;
    cfg1.memtable_limit = 512;
    cfg1.sst_target_size = 1024;
    seed(dir, cfg1, 50);

    Config cfg2;
    cfg2.sst_target_size = 8 * 1024 * 1024;
    check(verify(dir, cfg2, 50),
          "different sst_target_size on reopen: data survives");
  }

  // tighter l1_max_bytes on reopen — may trigger compaction but data survives
  {
    std::string dir = DIR + "/l1_max_bytes_tighter";
    Config cfg1;
    cfg1.memtable_limit = 512;
    seed(dir, cfg1, 100);

    Config cfg2;
    cfg2.sst_target_size = 512;
    cfg2.l1_max_bytes = 1024; // very tight, will trigger compaction immediately
    check(verify(dir, cfg2, 100),
          "tighter l1_max_bytes on reopen: data survives compaction");
  }

  // invalid cfg: sst_target_size > l1_max_bytes must throw regardless of
  // existing data
  {
    std::string dir = DIR + "/invalid_cfg";
    seed(dir, Config{}, 10);

    Config bad_cfg;
    bad_cfg.sst_target_size = 50 * 1024 * 1024;
    bad_cfg.l1_max_bytes = 10 * 1024 * 1024;
    bool threw = false;
    try {
      Fredb db(dir, bad_cfg);
    } catch (const std::invalid_argument &) {
      threw = true;
    }
    check(threw, "sst_target_size > l1_max_bytes throws on reopen");
  }

  return report();
}
