#include "fredb.h"
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

static std::ofstream g_log;

static void tee(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
static void tee(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    std::fputs(buf, stdout);
    if (g_log.is_open()) g_log << buf;
}

static const int    NUM_ENTRIES  = 1000000;
static const int    SYNC_ENTRIES =   10000;
static const int    KEY_SIZE     =      16;
static const int    VAL_SIZE     =     100;
static const double MB           = 1024.0 * 1024.0;

static const std::string DB_DIR = "/tmp/db_bench_fredb";

static void wipe_db() {
    std::filesystem::remove_all(DB_DIR);
    std::filesystem::create_directories(DB_DIR);
}

static std::string make_key(int n) {
    char buf[KEY_SIZE + 1];
    std::snprintf(buf, sizeof(buf), "%016d", n);
    return std::string(buf, KEY_SIZE);
}

static std::string make_value() {
    return std::string(VAL_SIZE, 'v');
}

static void report(const char* name, int ops, double elapsed_sec) {
    double micros_per_op = elapsed_sec * 1e6 / ops;
    double bytes = (double)ops * (KEY_SIZE + VAL_SIZE);
    double mb_per_sec = (bytes / MB) / elapsed_sec;
    tee("%-14s : %10.3f micros/op;  %5.1f MB/s\n",
        name, micros_per_op, mb_per_sec);
}

using Clock = std::chrono::steady_clock;

static void bench_fillseq() {
    wipe_db();
    Fredb db(DB_DIR);
    std::string val = make_value();
    auto t0 = Clock::now();
    for (int i = 0; i < NUM_ENTRIES; i++)
        db.put(make_key(i), val);
    double elapsed = std::chrono::duration<double>(Clock::now() - t0).count();
    report("fillseq", NUM_ENTRIES, elapsed);
}

// flush() after each op — heavier than LevelDB's fsync-only fillsync,
// so we use 10 000 ops to keep runtime reasonable (matches reference.md count).
static void bench_fillsync() {
    wipe_db();
    Fredb db(DB_DIR);
    std::string val = make_value();
    auto t0 = Clock::now();
    for (int i = 0; i < SYNC_ENTRIES; i++) {
        db.put(make_key(i), val);
        db.flush();
    }
    double elapsed = std::chrono::duration<double>(Clock::now() - t0).count();
    report("fillsync", SYNC_ENTRIES, elapsed);
}

static void bench_fillrandom() {
    wipe_db();
    Fredb db(DB_DIR);
    std::string val = make_value();
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, NUM_ENTRIES - 1);
    auto t0 = Clock::now();
    for (int i = 0; i < NUM_ENTRIES; i++)
        db.put(make_key(dist(rng)), val);
    double elapsed = std::chrono::duration<double>(Clock::now() - t0).count();
    report("fillrandom", NUM_ENTRIES, elapsed);
}

static void bench_overwrite() {
    wipe_db();
    Fredb db(DB_DIR);
    std::string val = make_value();
    for (int i = 0; i < NUM_ENTRIES; i++)
        db.put(make_key(i), val);
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, NUM_ENTRIES - 1);
    auto t0 = Clock::now();
    for (int i = 0; i < NUM_ENTRIES; i++)
        db.put(make_key(dist(rng)), val);
    double elapsed = std::chrono::duration<double>(Clock::now() - t0).count();
    report("overwrite", NUM_ENTRIES, elapsed);
}

static void bench_readrandom() {
    wipe_db();
    Fredb db(DB_DIR);
    std::string val = make_value();
    for (int i = 0; i < NUM_ENTRIES; i++)
        db.put(make_key(i), val);
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, NUM_ENTRIES - 1);
    auto t0 = Clock::now();
    for (int i = 0; i < NUM_ENTRIES; i++)
        db.get(make_key(dist(rng)));
    double elapsed = std::chrono::duration<double>(Clock::now() - t0).count();
    report("readrandom", NUM_ENTRIES, elapsed);
}

static void bench_readseq() {
    wipe_db();
    Fredb db(DB_DIR);
    std::string val = make_value();
    for (int i = 0; i < NUM_ENTRIES; i++)
        db.put(make_key(i), val);
    auto t0 = Clock::now();
    auto results = db.get_range(make_key(0), make_key(NUM_ENTRIES));
    int ops = (int)results.size();
    double elapsed = std::chrono::duration<double>(Clock::now() - t0).count();
    report("readseq", ops, elapsed);
}

static void bench_readreverse() {
    wipe_db();
    Fredb db(DB_DIR);
    std::string val = make_value();
    for (int i = 0; i < NUM_ENTRIES; i++)
        db.put(make_key(i), val);
    auto t0 = Clock::now();
    auto results = db.get_range(make_key(0), make_key(NUM_ENTRIES));
    int ops = (int)results.size();
    double elapsed = std::chrono::duration<double>(Clock::now() - t0).count();
    report("readreverse", ops, elapsed);
}

int main() {
    std::filesystem::create_directories("logs");
    std::time_t now = std::time(nullptr);
    char ts[32];
    std::strftime(ts, sizeof(ts), "%Y-%m-%d_%H-%M-%S", std::localtime(&now));
    std::string log_path = std::string("logs/") + ts + ".log";
    g_log.open(log_path);

    tee("Keys:    %d bytes each\n", KEY_SIZE);
    tee("Values:  %d bytes each\n", VAL_SIZE);
    tee("Entries: %d\n\n", NUM_ENTRIES);

    bench_fillseq();
    bench_fillsync();
    bench_fillrandom();
    bench_overwrite();
    bench_readrandom();
    bench_readseq();
    bench_readreverse();

    std::filesystem::remove_all(DB_DIR);
    return 0;
}
