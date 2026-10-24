#!/usr/bin/env bash
# Create an E2E test: <path>.krb and <path>.krb.argv that runs it.
set -euo pipefail

if [ "$#" -lt 1 ]; then
    echo "usage: $0 tests/run/<dir>/<name>.krb [more.krb...]" >&2
    exit 64
fi

for path in "$@"; do
    case "$path" in
        *.krb) ;;
        *)
            echo "error: '$path' must end in .krb" >&2
            exit 64
            ;;
    esac

    if [ -e "$path.argv" ]; then
        echo "error: '$path.argv' already exists" >&2
        exit 1
    fi

    mkdir -p "$(dirname "$path")"
    [ -e "$path" ] || : > "$path"
    printf 'run\n$file\n' > "$path.argv"

    echo "created $path"
    echo "created $path.argv"
done
