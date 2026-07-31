#!/usr/bin/env bash
set -euo pipefail

# -----------------------------------------------------------------------------
# Bump repository version and tag it.
#
# Usage:  ./increment-version.sh -M|-m|-p
#   -M   Increment major version
#   -m   Increment minor version
#   -p   Increment patch version
# -----------------------------------------------------------------------------

if [[ $# -eq 0 ]] || ! [[ "$@" =~ (^|[[:space:]])-([MmPp])($|[[:space:]]) ]]; then
    echo "⚠️ You must specify one of -M, -m, or -p."
    echo "Usage: ./increment-version.sh -M|-m|-p"
    exit 1
fi

VERSION="$(./tools/semver/semver.sh "$@")"
TAG="v${VERSION}"

printf '%s\n' "${VERSION}" > VERSION.txt

# Update test snapshots
# This updates the __version__ native function call test
./scripts/tests.sh --update

git add VERSION.txt tests/native_functions/native_fn_version_call.krb.out
git commit -m "Increment version to ${TAG}"

# Create new tag. This will prompt for the tag's message
if ! git tag -a "${TAG}"; then
    echo "⚠️ Tag creation failed, rolling back the commit"
    git reset --hard HEAD~1
    exit 1
fi

echo "Releasing ${TAG}"

git push origin "${TAG}"
git push

echo "Released ${TAG}"
