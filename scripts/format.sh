#!/usr/bin/env bash
# Format only the C lines changed since HEAD, plus new untracked C files.
# Paths limit the files considered: ./scripts/format.sh src/vm.c
set -euo pipefail

if ! command -v git-clang-format >/dev/null 2>&1; then
    echo "error: git-clang-format not found (install clang-format)" >&2
    exit 1
fi

git clang-format --force --quiet --extensions c,h -- "$@" || true

if [ "$#" -eq 0 ]; then
    set -- src unit
fi

git ls-files --others --exclude-standard -- "$@" |
    { grep -E '\.(c|h)$' || true; } |
    while read -r file; do
        clang-format -i "$file"
    done
