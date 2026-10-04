# CLI

```
kirby 0.4.0

Usage: krb [-h] [-v] <command> [args]

Commands:

  init                   Initialize a new kirby project in the current directory
  run [path] [args...]   Run a file. Without a path, runs the "bin" file
                         from kirby.project.toml. Extra args are passed
                         to the script
  example <name> [args...]
                         Run <name>.krb from the project's examples
                         directory ("examples" in kirby.project.toml,
                         defaults to "examples")
  config                 Print config in 'kirby.project.toml'
  config [key]           Print config by key
                         Supported: bin, examples
  repl                   Start the interactive REPL
  exec <source>          Run source code given as a string
  compile <path>         Compile a file without running it
  build <path> -o <output> [--runtime <path>]
                         Build a file into an executable that runs it.
                         The runtime is krb-runtime next to krb, unless
                         --runtime says otherwise
  lex <path>             Print the tokens of a file
  parse <path>           Print the AST of a file

Examples:

krb --help                        # -h is the short option
krb --version                     # -v is the short option
krb init                          # initialize a new Kirby project
krb run path/to/file.krb          # runs the file
krb run                           # runs the project's "bin" file
krb run -- arg1 arg2              # passes args to the project's "bin"
krb example hello                 # runs examples/hello.krb
krb config                        # print kirby.project.toml
krb config bin                    # print 'bin' kirby.project.toml
krb config examples               # print 'examples' kirby.project.toml
krb repl
krb compile path/to/file.krb
krb build path/to/file.krb -o app # builds ./app, which runs the file
krb parse path/to/file.krb
krb exec '@println("Hello World");'
```

## Script Arguments

However a script is started, `@argv(0)` is the program and `@argv(1)` onward are
the script's own arguments.

| Started with                | `@argv(0)`                   | `@argc()` for `a b` |
| --------------------------- | ---------------------------- | ------------------- |
| `krb run file.krb a b`      | `file.krb`                   | 3                   |
| `krb run -- a b`            | the project's `bin` file     | 3                   |
| `krb example name a b`      | `<examples>/name.krb`        | 3                   |
| `krb exec '<code>' a b`     | `exec`                       | 3                   |
| `krb repl`                  | `repl`                       | 1                   |
| `./app a b` (`krb build`)   | `./app`, as it was typed     | 3                   |
