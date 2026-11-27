# Development

## Prerequisites

- [CMake](https://cmake.org/)
- [Make](https://www.gnu.org/software/make/)
- [Just](https://just.systems)
- [readline](https://tiswww.case.edu/php/chet/readline/rltop.html) for the REPL.
  On macOS run `brew install readline`; CMake finds it. The system `libedit`
  builds too, but it shows no prompt or echo for piped input, so the REPL test
  fails with it
- Bash 4+ (test script uses globstar)

### Optional

- lcov for code coverage

## Scripts

Development [scripts](../scripts/README.md)

- [Build](../scripts/README.md#build)
- [Install](../scripts/README.md#install)
- [Test](../scripts/README.md#test)
- [Verify](../scripts/README.md#verify)

## Important Files

| Path                             | What it holds                                                    |
| -------------------------------- | ---------------------------------------------------------------- |
| `src/scanner.c`, `src/token.h`   | Source text to tokens                                            |
| `src/parser.c`, `src/ast.h`      | Tokens to the AST                                                |
| `src/typecheck.c`, `src/types.c` | The type checker                                                 |
| `src/compiler.c`, `src/opcode.h` | AST to bytecode                                                  |
| `src/loader.c`                   | Turns compiled code into the objects the VM runs                 |
| `src/vm.c`                       | Runs the bytecode                                                |
| `src/gc.c`                       | Garbage collector                                                |
| `src/native.c`                   | Functions written in C, such as `@strLen`                        |
| `src/main.c`, `src/runtime.c`    | The `krb` command line, and the runtime `krb build` copies       |
| `stdlib/stdlib.krb`              | The standard library, including the type of each native function |
| `tests/`, `unit/`                | [End to end tests](../tests/README.md) and C unit tests          |
| `book/`                          | The documentation site                                           |
| `vsc/`                           | The VS Code extension                                            |

## Create A Native Function

1. [Write the tests](../tests/README.md#writing-a-test) in
   `tests/run/native_functions/`.
2. Write the C function in `src/native.c`, check its arguments with the helpers in
   `src/asserts.h`, and add it to `nativeDefinitions` in the same file.
3. Declare its type in `stdlib/stdlib.krb`, such as
   `native fun @strTrim(text: string): string;`. The declarations are in
   alphabetical order. If its type can't be written yet, add its name to a list at
   the top of `unit/native_signatures.c` instead.
4. If the type checker should reject some constant values, add a rule to
   `src/native_signatures.c`.
5. Document it in `book/src/reference/builtin-functions.md`, `vsc/hovers/`,
   `vsc/snippets.json` and under `## Next` in [CHANGELOG.md](./CHANGELOG.md).
6. Run `just verify`.
