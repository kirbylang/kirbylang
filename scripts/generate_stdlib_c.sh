#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 2 ]; then
    echo "usage: $0 <output.c> <stdlib.krb>" >&2
    exit 1
fi

OUT="$1"
STDLIB_FILE="$2"

mkdir -p "$(dirname "$OUT")"

LENGTH=$(wc -c < "$STDLIB_FILE" | tr -d ' ')

{
    echo "// This is a generated file. Do not edit!"
    echo "// Generated from stdlib/stdlib.krb"
    echo
    echo "// Written as bytes, so any character in the source is safe."
    echo "const char KIRBY_STDLIB[] = {"
    od -An -v -tx1 "$STDLIB_FILE" | sed -e 's/  */ /g' -e 's/^ //' -e 's/ *$//' \
        -e '/^$/d' -e 's/\([0-9a-f][0-9a-f]\)/0x\1,/g' -e 's/^/    /'
    echo "    0x00,"
    echo "};"
    printf 'const unsigned int KIRBY_STDLIB_len = %s;\n' "$LENGTH"
} > "$OUT"
