#!/usr/bin/env bash

set -euo pipefail

cp ./build/kirby /usr/local/bin/kirby

echo "Installed at: '/usr/local/bin/kirby'"

mkdir -p /usr/local/share/man/man1
cp ./docs/man/man1/kirby.1 /usr/local/share/man/man1/kirby.1
