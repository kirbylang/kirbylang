#!/usr/bin/env bash

set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build/debug}"

"$BUILD_DIR/krb" example "$1" "${@:2}"