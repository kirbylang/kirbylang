#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 2 ]; then
    echo "usage: $0 <output.c> <VERSION.txt>" >&2
    exit 1
fi

NOW=$(date '+%Y-%m-%dT%T')

OUT="$1"
VERSION_FILE="$2"

mkdir -p "$(dirname "$OUT")"

VERSION=$(tr -d '\n' < "$VERSION_FILE")

{
    echo "// This is a generated file. Do not edit!"
    echo "// Generated from VERSION.txt"
    echo "// $NOW"
    echo
    echo "// clox $VERSION"
    echo

    printf 'const char CLOX_VERSION[] = "%s";\n' "$VERSION"
    printf 'const unsigned int CLOX_VERSION_len = %d;\n' "${#VERSION}"
} > "$OUT"