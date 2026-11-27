#!/usr/bin/env bash
set -e

cd build/debug
ctest --verbose --no-tests=error
