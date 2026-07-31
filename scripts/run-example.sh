#!/usr/bin/env bash

set -euo pipefail

./build/krb -f examples/$1.krb -- ${@:2}