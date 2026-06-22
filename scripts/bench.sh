#!/usr/bin/env bash
set -e
cd "$(dirname "$0")/.."

cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -S . -Wno-dev > /dev/null
cmake --build build --target db_bench -j$(nproc)
exec ./build/benchmark/db_bench "$@"
