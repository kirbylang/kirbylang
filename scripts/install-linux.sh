#!/usr/bin/env bash

set -euo pipefail

cp ./build/clox /usr/local/bin/clox

echo "Installed at: '/usr/local/bin/clox'"

mkdir -p /usr/local/share/man/man1
cp ./docs/man/man1/clox.1 /usr/local/share/man/man1/clox.1
