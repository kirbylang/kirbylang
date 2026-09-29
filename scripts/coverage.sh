#!/usr/bin/env bash

set -euo pipefail

BUILD_TESTS=ON ENABLE_COVERAGE=ON ./scripts/build.sh

./scripts/verify.sh

lcov \
    --capture\
    --directory .\
    --exclude 'lib/*'\
    --exclude 'unit/*'\
    --exclude '/Library/Developer/CommandLineTools/*'\
    --ignore-errors unused \
    --output-file lcov.info 

genhtml lcov.info --output-directory build/coverage_html
