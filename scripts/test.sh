#!/usr/bin/env bash
set -u
cd "$(dirname "$0")/.."

cmake -B build -Wno-dev > /dev/null || { echo "cmake configure FAILED"; exit 2; }
cmake --build build -j"$(nproc)" || { echo "build FAILED"; exit 2; }

fail=0
pass=0
for bin in build/tests/test_*; do
  [ -x "$bin" ] || continue
  name=$(basename "$bin")
  case "$name" in
    test_spam|test_chaos_monkey|test_oversized_key_value) continue ;;
  esac
  printf "%-40s " "$name"
  if "./$bin" > /tmp/${name}.log 2>&1; then
    echo "PASS"
    pass=$((pass+1))
  else
    echo "FAIL (log: /tmp/${name}.log)"
    fail=$((fail+1))
  fi
done

echo
echo "=== ${pass} passed, ${fail} failed ==="
[ "$fail" -eq 0 ] && rm -rf testdata/ && exit 0 || exit 1
