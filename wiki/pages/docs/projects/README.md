---
aliases:
  - Projects
---
Projects are directories with a `kirby.project.toml` file at the root.

```toml
# Default Kirby file to run
bin = "bin/main.krb"

# Directory of examples
examples = "examples"
```
## Example Project

This is the layout from the [example project](https://github.com/kirbylang/kirbylang/blob/main/example_project/README.md).

```
example_project
├── bin/
│ ├── main.krb
├── examples/
│ └── cat.krb
├── kirby.project.toml
└── README.md
```

## Commands

## Run

Run the project's bin defined in `kirby.project.toml`.

```shell
krb run # bin/main.krb
```

Other files in the project can also be ran.

```shell
krb run path/to/file.krb
```

## Example

```shell
krb example cat README.md # krb -f examples/cat.krb README.md
```
