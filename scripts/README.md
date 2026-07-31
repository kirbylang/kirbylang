# Scripts

## Build

Run `./scripts/build.sh` or `just build` to build to project in to `./build` folder.

## Install

Run `just install` or the OS specific script:

- **linux:** `./scripts/install-linux.sh`
- **macos:** `./scripts/install-macos.sh`
- **windows:** `.\scripts\install-windows.cmd`

## Test

Run `./scripts/test.sh` or `just test` to run the tests in `./test` folder.

### Filtering Tests

```sh
./scripts/test.sh PATTERN
```

## Test (Update Snapshots)

Run `./scripts/test.sh --update` or `just test-update` to run the tests and update the snapshot files.

### Filtering Updates

```sh
./scripts/test.sh PATTERN --update
```

## Coverage

Run `./scripts/coverage.sh` or `just coverage` to generate code coverage data.

**Requires:** `lcov`

```sh
./scripts/coverage.sh
```

Outputs `lcov.info` at the root of the project.

## Unit Tests

Run `./sciprts/build-tests.sh && ./scripts/unit.sh` or `just unit` to run the unit tests.

## Build Unit Tests

Run `./scripts/build-tests.sh` or `just build-tests` to build the [unit tests](#unit-tests).

## Run Examples

```sh
./scripts/run-example.sh fib 30
./scripts/run-example.sh hello World
```

## Clean

Run `./scripts/clean.sh` or `just clean` to remove build artifacts.

## Increment Language Version

- Updates [`VERSION.txt`](../VERSION.txt)
- Updates [`tests/native_functions/native_fn_version_call.krb.out`](../tests/native_functions/native_fn_version_call.krb.out) test snapshot
- Creates `vVERSION` tag e.g. `v0.1.0`, `v2.5.0`
- Commits updated `VERSION.txt` and `tests/native_functions/native_fn_version_call.krb.out`
- Push tag and commits

```shell
./scripts/increment-version.sh -M # Increment major
./scripts/increment-version.sh -m # Increment minor
./scripts/increment-version.sh -p # Increment patch
```

## Setup Scripts

### CMake

Run `./scripts/cmake.sh` or `just cmake` to generate make files for the project.

#### Definitions

Different build definitions can be updated in [CMakeLists.txt](./CMakeLists.txt) run `just cmake && just build`.

##### DEBUG_PRINT_CODE

Setting `DEBUG_PRINT_CODE` will print disassembled bytecode.

##### DEBUG_TRACE_EXECUTION

Setting `DEBUG_TRACE_EXECUTION` will print execution trace logs.

### Generating The Language Version C File

The current language version is defined in the [VERSION.txt](./VERSION.txt) file.

The `src/version.c` is generated automatically when the project is built. You can also generate/update the [`src/version.c`](./src/version.c) file explicit.

```sh
./scripts/generate_version_c.sh
just generate_version_c
```
