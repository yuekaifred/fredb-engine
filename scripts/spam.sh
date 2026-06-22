#!/usr/bin/env bash
set -e
cd "$(dirname "$0")/.."

cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -S . -Wno-dev > /dev/null
cmake --build build --target test_spam -j$(nproc)
exec ./build/tests/test_spam
