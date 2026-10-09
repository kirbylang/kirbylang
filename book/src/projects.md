# Projects

A project in Kirby is still an early concept.

## New Project

```shell
mkdir my_project
cd my_project

krb init
```

This creates the project config file: `kirby.project.toml`.

```toml
# bin = "bin/main.krb"
# examples = "examples"
```

## Config

The project's config file

```shell
krb config

# # bin = "bin/main.krb"
# # examples = "examples"

krb config bin

# bin/main.krb

krb config examples

# examples
```

## Run

By default `krb run` (no being file passed) will run what's configured at `krb config bin`.

```
krb run
```
