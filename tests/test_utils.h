#pragma once
#include <iostream>

inline bool g_ok = true;

inline void check(bool cond, const char *msg) {
  if (!cond) {
    std::cout << "FAIL: " << msg << "\n";
    g_ok = false;
  }
}

inline int report() {
  std::cout << (g_ok ? "ALL TESTS PASSED\n" : "SOME TESTS FAILED\n");
  return g_ok ? 0 : 1;
}
