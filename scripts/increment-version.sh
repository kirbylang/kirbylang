#!/usr/bin/env bash
set -euo pipefail

# -----------------------------------------------------------------------------
# Cut a new kirby release.
#
# Usage: ./scripts/increment-version.sh (-M | -m | -p) [options]
#
#   -M, --major         Increment the major version
#   -m, --minor         Increment the minor version
#   -p, --patch         Increment the patch version
#
#   -e, --message MSG   Tag message (default: "Release vX.Y.Z")
#   -n, --dry-run       Show what would happen; change nothing
#   -y, --yes           Skip the confirmation prompt
#       --gh-release    Also create the GitHub release locally via `gh`
#                       (omit this if .github/workflows/release.yml does it)
#   -h, --help          Show this help
#
# The release commit contains VERSION.txt and docs/CHANGELOG.md. The changelog
# entries under "## Next" move under a new "## X.Y.Z" heading.
#
# Env: REMOTE (default: origin), RELEASE_BRANCH (default: main)
# -----------------------------------------------------------------------------

REMOTE="${REMOTE:-origin}"
RELEASE_BRANCH="${RELEASE_BRANCH:-main}"

BUMP=""
MESSAGE=""
DRY_RUN=false
ASSUME_YES=false
GH_RELEASE=false

# Rollback bookkeeping
CHANGES_STARTED=false
TAG_CREATED=false
PUSHED=false
TAG=""
START_HEAD=""

die() {
    printf '⚠️  %s\n' "$*" >&2
    exit 1
}

usage() {
    sed -n '5,20p' "$0" | sed 's/^#\{1,\} \{0,1\}//'
}

run() {
    if $DRY_RUN; then
        printf '   [dry-run] %s\n' "$*"
    else
        "$@"
    fi
}

set_bump() {
    [[ -z "$BUMP" ]] || die "Specify exactly one of -M, -m, or -p."
    BUMP="$1"
}

CHANGELOG="docs/CHANGELOG.md"

changelog_has_next_heading() {
    grep -qE '^## Next[[:space:]]*$' "$CHANGELOG"
}

# Succeeds when the "## Next" section has at least one non-blank line.
changelog_next_has_entries() {
    awk '
        /^## Next[[:space:]]*$/ { in_next = 1; next }
        in_next && /^## /       { exit }
        in_next && NF           { found = 1 }
        END                     { exit !found }
    ' "$CHANGELOG"
}

# Keep an empty "## Next" and put the old entries under "## $1".
rotate_changelog() {
    local tmp
    tmp="$(mktemp)"
    awk -v version="$1" '
        !done && /^## Next[[:space:]]*$/ {
            print "## Next"
            print ""
            print "## " version
            done = 1
            next
        }
        { print }
    ' "$CHANGELOG" >"$tmp"
    cat "$tmp" >"$CHANGELOG"
    rm -f "$tmp"
}

rollback() {
    local status=$?
    # Nothing to undo once the push has landed.
    if $PUSHED || $DRY_RUN || [[ $status -eq 0 ]]; then
        return
    fi
    echo
    echo "⚠️  Release failed; rolling back local state."
    if $TAG_CREATED; then
        git tag -d "$TAG" >/dev/null 2>&1 || true
        echo "   Deleted tag ${TAG}"
    fi
    if $CHANGES_STARTED; then
        # Safe because we required a clean tree before starting.
        git reset --hard "$START_HEAD" >/dev/null
        git clean -fdq -- tests/ docs/CHANGELOG.md
        echo "   Restored the tree to ${START_HEAD:0:8}"
    fi
}

# -----------------------------------------------------------------------------
# Parse arguments
# -----------------------------------------------------------------------------
while [[ $# -gt 0 ]]; do
    case "$1" in
        -M | --major) set_bump "-M" ;;
        -m | --minor) set_bump "-m" ;;
        -p | --patch) set_bump "-p" ;;
        -e | --message)
            [[ $# -ge 2 ]] || die "--message requires an argument."
            MESSAGE="$2"
            shift
            ;;
        -n | --dry-run) DRY_RUN=true ;;
        -y | --yes) ASSUME_YES=true ;;
        --gh-release) GH_RELEASE=true ;;
        -h | --help)
            usage
            exit 0
            ;;
        *) die "Unknown argument: $1"$'\n'"$(usage)" ;;
    esac
    shift
done

[[ -n "$BUMP" ]] || {
    echo "⚠️  You must specify one of -M, -m, or -p." >&2
    usage >&2
    exit 1
}

# -----------------------------------------------------------------------------
# Preflight
# -----------------------------------------------------------------------------
command -v git >/dev/null || die "git is not installed."
git rev-parse --git-dir >/dev/null 2>&1 || die "Not inside a git repository."

# Every path below is repo-relative, so anchor there.
cd "$(git rev-parse --show-toplevel)"

if $GH_RELEASE; then
    command -v gh >/dev/null || die "gh (GitHub CLI) is not installed."
    gh auth status >/dev/null 2>&1 || die "gh is not authenticated. Run: gh auth login"
fi

CURRENT_BRANCH="$(git rev-parse --abbrev-ref HEAD)"
[[ "$CURRENT_BRANCH" == "$RELEASE_BRANCH" ]] ||
    die "Releases must be cut from '${RELEASE_BRANCH}' (currently on '${CURRENT_BRANCH}')."

if [[ -n "$(git status --porcelain)" ]]; then
    if $DRY_RUN; then
        echo "⚠️  Working tree is dirty — a real run would stop here."
    else
        die "Working tree is dirty. Commit or stash your changes first."
    fi
fi

echo "==> Fetching ${REMOTE}"
git fetch --tags --quiet "$REMOTE"

LOCAL_HEAD="$(git rev-parse HEAD)"
REMOTE_HEAD="$(git rev-parse "${REMOTE}/${RELEASE_BRANCH}")"
[[ "$LOCAL_HEAD" == "$REMOTE_HEAD" ]] ||
    die "Local ${RELEASE_BRANCH} differs from ${REMOTE}/${RELEASE_BRANCH}. Pull or push first."

[[ -f "$CHANGELOG" ]] || die "${CHANGELOG} not found."
changelog_has_next_heading ||
    die "${CHANGELOG} has no '## Next' heading."
changelog_next_has_entries ||
    die "The '## Next' section in ${CHANGELOG} is empty. Nothing to release."

# -----------------------------------------------------------------------------
# Compute the new version
# -----------------------------------------------------------------------------
OLD_VERSION="$(cat VERSION.txt)"
VERSION="$(./tools/semver/semver.sh "$BUMP")"

[[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] ||
    die "semver.sh returned something that isn't a version: '${VERSION}'"

TAG="v${VERSION}"

if git rev-parse -q --verify "refs/tags/${TAG}" >/dev/null; then
    die "Tag ${TAG} already exists locally."
fi
if git ls-remote --exit-code --tags "$REMOTE" "refs/tags/${TAG}" >/dev/null 2>&1; then
    die "Tag ${TAG} already exists on ${REMOTE}."
fi

MESSAGE="${MESSAGE:-Release ${TAG}}"

echo
echo "  ${OLD_VERSION}  ->  ${VERSION}"
echo "  tag:     ${TAG}"
echo "  message: ${MESSAGE}"
echo "  branch:  ${REMOTE}/${RELEASE_BRANCH} @ ${LOCAL_HEAD:0:8}"
echo

if ! $ASSUME_YES && ! $DRY_RUN; then
    read -r -p "Cut this release? [y/N] " reply
    [[ "$reply" =~ ^[Yy]$ ]] || die "Aborted."
fi

trap rollback EXIT

# -----------------------------------------------------------------------------
# Bump, rebuild, verify
# -----------------------------------------------------------------------------
START_HEAD="$LOCAL_HEAD"
CHANGES_STARTED=true

echo "==> Writing VERSION.txt"
if $DRY_RUN; then
    printf '   [dry-run] echo %s > VERSION.txt\n' "$VERSION"
else
    printf '%s\n' "$VERSION" >VERSION.txt
fi

echo "==> Updating ${CHANGELOG}"
if $DRY_RUN; then
    printf '   [dry-run] move "## Next" entries under "## %s"\n' "$VERSION"
else
    rotate_changelog "$VERSION"
fi

# The build regenerates version.c from VERSION.txt, so the tests below see
# the new version.
echo "==> Building"
run ./scripts/build.sh

echo "==> Running test suite"
run ./scripts/tests.sh

# The release commit must hold exactly these two files. Anything else means
# a build or test step changed something it should not have.
if ! $DRY_RUN; then
    unexpected="$(git status --porcelain | grep -vE ' (VERSION\.txt|docs/CHANGELOG\.md)$' || true)"
    [[ -z "$unexpected" ]] ||
        die "Unexpected changes, refusing to commit:"$'\n'"${unexpected}"
fi

echo "==> Committing"
run git add VERSION.txt "$CHANGELOG"
if ! $DRY_RUN; then
    git commit -m "Increment version to ${TAG}"
fi

echo "==> Tagging"
if ! $DRY_RUN; then
    git tag -a "$TAG" -m "$MESSAGE"
    TAG_CREATED=true
fi

# -----------------------------------------------------------------------------
# Publish
# -----------------------------------------------------------------------------
echo "==> Pushing ${RELEASE_BRANCH} and ${TAG}"
# --atomic: both refs land or neither does. Never leave a tag on a commit that
# isn't reachable from the release branch.
if ! $DRY_RUN; then
    git push --atomic "$REMOTE" "$RELEASE_BRANCH" "refs/tags/${TAG}"
    PUSHED=true
else
    printf '   [dry-run] git push --atomic %s %s refs/tags/%s\n' \
        "$REMOTE" "$RELEASE_BRANCH" "$TAG"
fi

if $GH_RELEASE; then
    echo "==> Creating GitHub release"
    run gh release create "$TAG" \
        --title "$TAG" \
        --notes "$MESSAGE" \
        --generate-notes \
        --verify-tag
else
    echo "==> Tag pushed. release.yml will build artifacts and publish the release."
fi

echo
echo "✅ Released ${TAG}"
