# CLI

```
kirby 0.3.0

Usage: krb [-h] [-v] <command> [args]

Commands:

  run [path] [args...]   Run a file. Without a path, runs the "bin" file
                         from kirby.project.toml. Extra args are passed
                         to the script
  example <name> [args...]
                         Run <name>.krb from the project's examples
                         directory ("examples" in kirby.project.toml,
                         defaults to "examples")
  repl                   Start the interactive REPL
  exec <source>          Run source code given as a string
  compile <path>         Compile a file without running it
  lex <path>             Print the tokens of a file
  parse <path>           Print the AST of a file

Examples:

krb --help                        # -h is the short option
krb --version                     # -v is the short option
krb run path/to/file.krb
krb run                           # runs the project's "bin" file
krb run -- arg1 arg2              # passes args to the project's "bin"
krb example hello                 # runs examples/hello.krb
krb repl
krb compile path/to/file.krb
krb parse path/to/file.krb
krb exec 'print "Hello World";'
```
