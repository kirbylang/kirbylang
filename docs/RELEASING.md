# Releasing

A release is one command. It bumps the version, updates the changelog, runs the
tests, tags, and pushes. GitHub Actions then builds the binaries and publishes
the release.

## Before you release

1. Every change worth mentioning has an entry under `## Next` in
   [`CHANGELOG.md`](./CHANGELOG.md). Add entries as you work, not at release
   time. The release stops if `## Next` is empty.
2. You are on `main`, it is clean, and it matches `origin/main`.
3. You have decided the bump: major, minor, or patch.

## Cut the release

Preview first. This changes nothing:

```shell
./scripts/increment-version.sh -m --dry-run
```

Then run it for real:

```shell
./scripts/increment-version.sh -M   # major: 0.0.0 -> 1.0.0
./scripts/increment-version.sh -m   # minor: 0.0.0 -> 0.1.0
./scripts/increment-version.sh -p   # patch: 0.0.0 -> 0.0.1
```

Options:

| Flag              | Effect                                        |
| ----------------- | --------------------------------------------- |
| `-e`, `--message` | Tag message (default: `Release vX.Y.Z`)       |
| `-n`, `--dry-run` | Show what would happen, change nothing        |
| `-y`, `--yes`     | Skip the confirmation prompt                  |
| `--gh-release`    | Do not use. The workflow creates the release. |

## Script

1. Checks you are on `main`, the tree is clean, and you match `origin/main`.
2. Checks `## Next` in the changelog has entries and the tag is not taken.
3. Asks you to confirm.
4. Writes the new version to `VERSION.txt`.
5. Moves the `## Next` entries under a new `## X.Y.Z` heading and leaves an
   empty `## Next` above it.
6. Builds, which regenerates `version.c` from `VERSION.txt`.
7. Runs the full test suite.
8. Refuses to continue if any file other than `VERSION.txt` and
   `docs/CHANGELOG.md` changed.
9. Commits as `Increment version to vX.Y.Z` and creates the tag `vX.Y.Z`.
10. Pushes the commit and the tag together. Both land or neither does.

If anything fails before the push, the script puts the repo back exactly as it
found it: no leftover edits, commit, or tag.

## Github

Pushing a tag like `v0.4.0` starts [`release.yml`](../.github/workflows/release.yml):

1. Checks the tag matches `VERSION.txt`.
2. Builds and tests on Linux (x86_64) and macOS (arm64).
3. Packages each binary as a `.tar.gz` with a `.sha256` checksum.
4. Creates the GitHub release with those files and auto-generated notes.

The release is only created if both builds pass. Watch the run in the
repository's Actions tab.

## If the release workflow fails

The tag is already pushed, but no release exists. Fix the problem on `main`,
then move the tag to the fixed commit:

```shell
git tag -d v0.4.0
git push origin :refs/tags/v0.4.0
# ...commit and push the fix to main...
git tag -a v0.4.0 -m "Release v0.4.0"
git push origin v0.4.0
```
