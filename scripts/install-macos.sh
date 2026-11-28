#!/usr/bin/env bash

set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build/debug}"

for name in krb krb-runtime; do
    if [ ! -f "$BUILD_DIR/$name" ]; then
        echo "No file found: $BUILD_DIR/$name; Run the build first." >&2
        exit 1
    fi
done

cp "$BUILD_DIR/krb" /usr/local/bin/krb
cp "$BUILD_DIR/krb-runtime" /usr/local/bin/krb-runtime

echo "Installed $BUILD_DIR at: '/usr/local/bin/krb' and '/usr/local/bin/krb-runtime'"

mkdir -p /usr/local/share/man/man1
cp ./docs/man/man1/krb.1 /usr/local/share/man/man1/krb.1
