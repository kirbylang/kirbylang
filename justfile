cli := "build/debug/krb"
docker_image := "kirby"

default:
    just --list

# Build the VS Code language extension
build-vsc:
    ./scripts/build-vsc.sh

# Open the project with the VS Code language extension enabled
open-vsc: build-vsc
    ./scripts/open-vsc.sh

# Run the kirby CLI
run *args: build
    ./{{ cli }} {{ args }}

# Generate cmake files
cmake:
    ./scripts/cmake.sh

# Build the code
build:
    ./scripts/build.sh

# Build the code for release
build-release:
    ./scripts/build-release.sh

# Build the code with memory checking enabled
build-memcheck:
    ./scripts/build-memcheck.sh

build-tests:
    ./scripts/build-tests.sh

# Clean the build artifacts
clean:
    ./scripts/clean.sh

# Install the debug build, made by 'just build'
[linux]
install:
    ./scripts/install-linux.sh

# Install the release build, made by 'just build-release'
[linux]
install-release:
    BUILD_DIR=build/release ./scripts/install-linux.sh

[windows]
install:
    .\scripts\install-windows.cmd

[windows]
install-release:
    .\scripts\install-windows.cmd

# Install the debug build, made by 'just build'
[macos]
install:
    ./scripts/install-macos.sh

# Install the release build, made by 'just build-release'
[macos]
install-release:
    BUILD_DIR=build/release ./scripts/install-macos.sh

# Format the C lines changed since HEAD
fmt *paths:
    ./scripts/format.sh {{ paths }}

# Format all C files
fmt-all *paths:
    ./scripts/format-all.sh

# Run the tests
test *args: build
    ./scripts/tests.sh {{ args }}

# Run the tests, showing only failures and the summary
check *args: build
    #!/usr/bin/env bash
    set -o pipefail
    ./scripts/tests.sh {{ args }} | awk '/^🔬/ {h=$0; next} /^$|^Total:/ {h=""} h {print h; h=""} {print}'

# Run a set of the tests again, as programs built into executables
test-packaged *args: build
    ./scripts/tests-packaged.sh {{ args }}

# Run the tests of 'krb build'
test-build: build
    ./scripts/test-build.sh

# Run the tests of the arguments a script sees

# Run the tests with memory checking enabled
test-with-memcheck *args: build-memcheck
    BIN=./build/memcheck/kirby-test ./scripts/tests.sh {{ args }}

# Run the tests with coverage enabled
coverage *args:
    ./scripts/coverage.sh {{ args }}

# Update the test snapshots
test-update *args: build
    ./scripts/tests.sh --update {{ args }}

# Create a new E2E test: just new-test tests/run/dir/name.krb
new-test +paths:
    ./scripts/new-test.sh {{ paths }}

# Run unit tests
unit: build-tests
    ./scripts/unit.sh

verify: build build-tests
    ./scripts/verify.sh

# Build the docker image
docker-build:
    docker build -t {{ docker_image }} .

# Run kirby CLI in docker container
docker-run:
    docker run -it --init --name {{ docker_image }} --rm {{ docker_image }} 

increment-version *args:
    ./scripts/increment-version.sh {{ args }}

# Run one of the example programs
examples file *args: build
    ./scripts/run-example.sh {{ file }} {{ args }}

# Run one of the example programs with the release build
examples-release file *args: build-release
    BUILD_DIR=build/release ./scripts/run-example.sh {{ file }} {{ args }}

man:
    man ./docs/man/man1/krb.1

# Cut a new release. bump = major | minor | patch
release bump: (_release bump "")

# Preview a release without changing anything
release-dry bump: (_release bump "--dry-run")

_release bump *flags:
    ./scripts/increment-version.sh {{ bump }} {{ flags }}

project *args:
    cd example_project && just {{ args }}

book-serve:
    ./scripts/book-serve.sh
