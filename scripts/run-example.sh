#!/usr/bin/env bash

set -euo pipefail

./build/debug/krb example $1 ${@:2}