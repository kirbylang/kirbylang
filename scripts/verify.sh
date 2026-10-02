#!/usr/bin/env bash
set -e

./scripts/tests.sh
./scripts/tests.sh --packaged
./scripts/test-build.sh
./scripts/test-args.sh
./scripts/unit.sh
