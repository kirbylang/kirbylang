#!/usr/bin/env bash
set -e

BUILD_DIR=build
ENABLE_COVERAGE=${ENABLE_COVERAGE:-OFF}
# Enable memory checking memory issues in code
ENABLE_MEMCHECK=${ENABLE_MEMCHECK:-OFF}

cmake -S . -B "$BUILD_DIR" -DENABLE_COVERAGE=$ENABLE_COVERAGE -DENABLE_MEMCHECK=$ENABLE_MEMCHECK
cmake --build "$BUILD_DIR" --config Release