# CLI

## Binaries

| File          | Description                                         |
| ------------- | --------------------------------------------------- |
| `krb`         | Main CLI                                            |
| `krb-runtime` | Kirby runtime used to build standalone executables. |

### Prerequisites

- Linux/MacOS

### Installation

Download the latest release: [https://github.com/kirbylang/kirbylang/releases](https://github.com/kirbylang/kirbylang/releases)

## REPL

Start an interactive prompt to run Kirby code.

```shell
krb repl
```

## Run A File

```shell
krb run path/to/file.krb
```

Kirby loads the file and then calls its `main` function. A file needs
`fun main(): unit` to be run. See [Programs](./basics.md#programs).

Anything after the path is passed to the program, which reads it with `@argc` and
`@argv`.

Without a path, `krb run` runs the project's `bin` file. See
[Projects](./projects.md).

## Run Code From A String

```shell
krb exec '@println("Hello World");'
```

`krb exec` and the REPL run snippets. A snippet can hold statements and needs no
`main`.

## Build Standalone Executable

A standalone executable can be built from a `*.krb` file.

```shell
krb build path/to/file.krb -o ./out
./out
```

The executable needs neither `krb` nor the source file. `krb build` copies
`krb-runtime` from the folder `krb` is in, unless `--runtime <path>` names another
one.

## Other Commands

| Command                        | Description                            |
| ------------------------------ | -------------------------------------- |
| `krb init`                     | Create a [project](./projects.md)      |
| `krb config [bin\|examples]`   | Print the project's config             |
| `krb example <name> [args...]` | Run a file from the project's examples |
| `krb compile <path>`           | Compile a file without running it      |
| `krb lex <path>`               | Print a file's tokens                  |
| `krb parse <path>`             | Print a file's syntax tree             |
| `krb docs`                     | Open this documentation in a browser   |
| `krb --help`, `krb --version`  | Print the usage summary or the version |
