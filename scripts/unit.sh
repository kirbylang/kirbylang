#!/usr/bin/env bash
set -e

cd build
ctest --verbose --no-tests=error
