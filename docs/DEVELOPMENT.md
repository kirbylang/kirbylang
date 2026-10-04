# Development

## Prerequisites

- [CMake](https://cmake.org/)
- [Make](https://www.make.com/en)
- [Just](https://just.systems)
- [readline](https://tiswww.case.edu/php/chet/readline/rltop.html) for the REPL.
  On macOS run `brew install readline`; CMake finds it. The system `libedit`
  builds too, but it shows no prompt or echo for piped input, so the REPL test
  fails with it
- Bash 4+ (test script uses globstar)

### Optional

- lcov for code coverage

## Scripts

Development scripts [scripts](../scripts/README.md)
