#!/usr/bin/env bash

# Stands in for BIN when tests.sh is run by tests-packaged.sh. Given
# `run <file> [args...]` it builds <file> into an executable with `krb build`,
# runs that with the arguments and exits with its exit code.
#
# REAL_BIN is the program that builds. RUNTIME_BIN is the runtime the
# executable is made with.

: "${REAL_BIN:?REAL_BIN must be set}"
: "${RUNTIME_BIN:?RUNTIME_BIN must be set}"

if [[ "${1:-}" != "run" || $# -lt 2 ]]; then
    echo "usage: run-packaged.sh run <file> [args...]" >&2
    exit 64
fi

file="$2"
shift 2

dir=$(mktemp -d)
trap 'rm -rf "$dir"' EXIT

"$REAL_BIN" build "$file" -o "$dir/app" --runtime "$RUNTIME_BIN" \
    >/dev/null 2>"$dir/build.err" </dev/null
build_exit=$?

if [[ $build_exit -ne 0 ]]; then
    cat "$dir/build.err" >&2
    exit $build_exit
fi

"$dir/app" "$@"
