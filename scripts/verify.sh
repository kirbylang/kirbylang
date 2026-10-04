#!/usr/bin/env bash
set -e

./scripts/tests.sh
./scripts/tests-packaged.sh
./scripts/test-build.sh
./scripts/unit.sh
