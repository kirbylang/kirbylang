#!/usr/bin/env bash

set -euo pipefail

./build/krb run examples/$1.krb ${@:2}