#!/usr/bin/env bash

set -euo pipefail

cp ./build/krb /usr/local/bin/krb

echo "Installed at: '/usr/local/bin/krb'"

mkdir -p /usr/local/share/man/man1
cp ./docs/man/man1/krb.1 /usr/local/share/man/man1/krb.1
