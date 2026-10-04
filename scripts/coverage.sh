#!/usr/bin/env bash

set -euo pipefail

ENABLE_COVERAGE=ON ./scripts/build-tests.sh

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
