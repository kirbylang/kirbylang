#!/usr/bin/env bash

set -euo pipefail

cp ./build/krb /usr/local/bin/krb
cp ./build/krb-runtime /usr/local/bin/krb-runtime

echo "Installed at: '/usr/local/bin/krb' and '/usr/local/bin/krb-runtime'"

mkdir -p /usr/local/share/man/man1
cp ./docs/man/man1/krb.1 /usr/local/share/man/man1/krb.1
