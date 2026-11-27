# Scripts

## Build

Run `./scripts/build.sh` or `just build` to build the project into the `./build` folder.

## Install

Run `just install` or the OS specific script:

- **linux:** `./scripts/install-linux.sh`
- **macos:** `./scripts/install-macos.sh`
- **windows:** `.\scripts\install-windows.cmd`

## Test

Run `./scripts/tests.sh` or `just test` to run the tests in `./tests` folder.

### Filtering Tests

```sh
./scripts/tests.sh PATTERN [PATTERN...]
```

## Test (Packaged Programs)

Run `./scripts/tests-packaged.sh` or `just test-packaged` to run a set of the
tests again as executables built with `krb build`. The list of tests, and what
each group protects, is in the script. `./scripts/run-packaged.sh` is what it
gives `tests.sh` to build and run each program.

Run `./scripts/test-build.sh` or `just test-build` to test `krb build` itself
and the programs it makes.

## Test (Update Snapshots)

Run `./scripts/tests.sh --update` or `just test-update` to run the tests and update the snapshot files.

### Filtering Updates

```sh
./scripts/tests.sh PATTERN --update
```

## Coverage

Run `./scripts/coverage.sh` or `just coverage` to generate code coverage data.

**Requires:** `lcov`

```sh
./scripts/coverage.sh
```

Outputs `lcov.info` at the root of the project.

## Unit Tests

Run `./scripts/build-tests.sh && ./scripts/unit.sh` or `just unit` to run the unit tests.

## Build Unit Tests

Run `./scripts/build-tests.sh` or `just build-tests` to build the [unit tests](#unit-tests).

## Run Examples

```sh
./scripts/run-example.sh fib 30
./scripts/run-example.sh hello World
```

## Verify

Run `./scripts/verify.sh` or `just verify` to run the tests, the packaged tests,
the `krb build` tests and the unit tests.

## New Test

Run `just new-test tests/run/dir/name.krb` to create a test and its `.argv` file.

## Format

Run `just fmt` to format the C lines changed since the last commit, or
`just fmt-all` to format every C file. **Requires:** `clang-format`

## Memory Check

Run `just test-with-memcheck` to run the tests with AddressSanitizer.

## Release Build

Run `./scripts/build-release.sh` or `just build-release`.

## VS Code Extension

Run `just build-vsc` to build the extension, or `just open-vsc` to open the project
with it. **Requires:** `nvm`

## Book

Run `./scripts/book-serve.sh` or `just book-serve` to build the documentation site
and serve it locally. **Requires:** `mdbook`

## Clean

Run `./scripts/clean.sh` or `just clean` to remove build artifacts.

## Increment Language Version

Cuts a release: updates [`VERSION.txt`](../VERSION.txt) and the changelog, runs
the tests, commits, tags `vX.Y.Z` and pushes. See
[Releasing](../docs/RELEASING.md).

```shell
./scripts/increment-version.sh -M # Increment major
./scripts/increment-version.sh -m # Increment minor
./scripts/increment-version.sh -p # Increment patch
```

## Setup Scripts

### CMake

Run `./scripts/cmake.sh` or `just cmake` to generate make files for the project.

#### Definitions

Different build definitions can be updated in [CMakeLists.txt](../CMakeLists.txt). Run `just cmake && just build` after changing them.

##### DEBUG_PRINT_CODE

Setting `DEBUG_PRINT_CODE` will print disassembled bytecode.

##### DEBUG_TRACE_EXECUTION

Setting `DEBUG_TRACE_EXECUTION` will print execution trace logs.

### Generating The Language Version C File

The current language version is defined in the [VERSION.txt](../VERSION.txt) file.

`build/generated/version.c` is generated automatically when the project is built.
You can also generate it explicitly.

```sh
./scripts/generate_version_c.sh
just generate_version_c
```
