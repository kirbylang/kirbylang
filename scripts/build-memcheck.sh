#!/usr/bin/env bash

set -euo pipefail

BUILD_TESTS=ON ENABLE_MEMCHECK=ON ./scripts/build.sh

./scripts/tests.sh
