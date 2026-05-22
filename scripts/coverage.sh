#!/usr/bin/env bash

set -euo pipefail

BUILD_TESTS=ON ENABLE_COVERAGE=ON ./scripts/build.sh

./scripts/tests.sh

lcov --capture --directory . --output-file lcov.info 

genhtml lcov.info --output-directory build/coverage_html
