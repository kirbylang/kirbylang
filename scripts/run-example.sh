#!/usr/bin/env bash

set -euo pipefail

./build/kirby -f examples/$1.krb -- ${@:2}