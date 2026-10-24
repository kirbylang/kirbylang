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

## Build Standalone Executable

A standalone executable can be built from a `*.krb` file.

```shell
krb build path/to/file.krb -o ./out
```
