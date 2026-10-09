# Getting Started

## Introduction

Kirby is a statically typed interpreted language. It's design was inspired by many programming languages I've used. Both the good (parts I wanted to include) and the bad (things I wanted to avoid).

## CLI

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
