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

Remove the `#` in front of a setting to use it.

```toml
bin = "bin/main.krb"
examples = "examples"
```

`krb config` prints the file, and `krb config <key>` prints one setting.

```shell
krb config bin

# bin/main.krb

krb config examples

# examples
```

## Run

By default `krb run` (no file being passed) will run what's configured at `krb config bin`. Like any file that is run, it needs a `fun main(): unit`.

```shell
krb run
```

To pass arguments to that file, put `--` in place of the path:
`krb run -- one two`.

## Examples

`krb example <name>` runs `<name>.krb` from the `examples` folder. The folder is
`examples` when the setting isn't given.

```shell
krb example hello
```
