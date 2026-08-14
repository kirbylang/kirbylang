cli := "build/krb"
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
build: generate_version_c
    ./scripts/build.sh

# Build the code for release
build-release: generate_version_c
    ./scripts/build-release.sh

# Build the code with memory checking enabled
build-memcheck: generate_version_c
    ./scripts/build-memcheck.sh

build-tests: generate_version_c
    ./scripts/build-tests.sh

# Clean the build artifacts
clean:
    ./scripts/clean.sh

[linux]
install:
    ./scripts/install-linux.sh

[windows]
install:
    .\scripts\install-windows.cmd

[macos]
install:
    ./scripts/install-macos.sh

# Run the tests
test *args: build
    ./scripts/tests.sh {{ args }}

# Run the tests with memory checking enabled
test-with-memcheck *args: build-memcheck
    ./scripts/tests.sh {{ args }}

# Run the tests with coverage enabled
coverage *args:
    ./scripts/coverage.sh {{ args }}

# Update the test snapshots
test-update: build
    ./scripts/tests.sh --update

# Run unit tests
unit: build-tests
    ./scripts/unit.sh

verify:
    ./scripts/verify.sh

# Build the docker image
docker-build:
    docker build -t {{ docker_image }} .

# Run kirby CLI in docker container
docker-run:
    docker run -it --init --name {{ docker_image }} --rm {{ docker_image }} 

# VERSION.txt => src/version.c
generate_version_c:
    ./scripts/generate_version_c.sh ./build/generated/version.c VERSION.txt

increment-version *args:
    ./scripts/increment-version.sh {{ args }}

# Run one of the example programs
examples file *args:
    ./scripts/run-example.sh {{ file }} {{ args }}

man:
    man ./docs/man/man1/krb.1

# Cut a new release. bump = major | minor | patch
release bump: (_release bump "")

# Preview a release without changing anything
release-dry bump: (_release bump "--dry-run")

_release bump *flags:
    ./scripts/increment-version.sh {{ bump }} {{ flags }}
