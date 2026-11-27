#!/usr/bin/env bash
set -e

BUILD_DIR=build/release
BIN="krb"
RUNTIME_BIN="krb-runtime"

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --config Release

strip "$BUILD_DIR/$BIN"
strip "$BUILD_DIR/$RUNTIME_BIN"
