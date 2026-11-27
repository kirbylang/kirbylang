#!/usr/bin/env bash

set -euo pipefail

BUILD_DIR=build/memcheck

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug -DENABLE_MEMCHECK=ON
cmake --build "$BUILD_DIR"

BIN="$BUILD_DIR/kirby-test" ./scripts/tests.sh
