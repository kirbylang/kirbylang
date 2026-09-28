#!/usr/bin/env bash

set -euo pipefail

./build/krb example $1 ${@:2}