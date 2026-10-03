# Tests

The language's E2E tests are file based. A test is a `.argv` file, and the
other files that belong to it are named after it:

| Example File    | Description                                                |
| --------------- | ---------------------------------------------------------- |
| `test.krb.argv` | Arguments for `krb`, one per line. This file is the test   |
| `test.krb`      | The program the arguments refer to, if any (`$file`)       |
| `test.krb.out`  | Expected `stdout` output                                   |
| `test.krb.err`  | Expected `stderr` output                                   |
| `test.krb.exit` | Expected exit code                                         |
| `test.krb.in`   | Text sent to `stdin`                                       |
| `test.krb.env`  | Environment Variables                                      |

Every file in `tests/` must belong to a test, so a program without an `.argv`
file is reported instead of being skipped. Dotfiles and this README are ignored.

The folders are only for organization. `tests/run` holds the tests of the
language and `tests/exec` holds the tests of `krb exec`.

## The `.argv` File

Each line is one argument for `krb`, subcommand included. A line is expanded
like text inside double quotes:

- `$file` is the path of the test's program: the path of the `.argv` file
  without `.argv`, such as `./tests/run/new_test.krb`
- `$(...)` runs a command, so `$(cat $file)` is the contents of the program
- Nothing is split, so a line with spaces is one argument and quotes are plain
  characters
- A blank line is an empty argument
- `\$` is a literal `$`

Run a file:

```
run
$file
```

Run the contents of a file as a string:

```
exec
$(cat $file)
```

`$(...)` removes trailing newlines, so the source `exec` receives has no final
newline. The bytecode listing in the `.err` snapshot shows that, and it differs
from `krb run` on the same file.

A test does not need a program. An `.argv` file with its snapshots is enough,
such as `exec` followed by a line of source.

If a line cannot be expanded, that test fails and the rest still run.

## Writing A Test

1. Create a new file in the [`tests/run`](./run) directory: `./tests/run/new_test.krb`
2. Create `./tests/run/new_test.krb.argv` with the arguments for `krb`:

   ```
   run
   $file
   ```

3. Add test kirby code
4. Implement feature being tested
5. [Update snapshots](#update-snapshots) which will create `./tests/run/new_test.krb.out`, `./tests/run/new_test.krb.err`, `./tests/run/new_test.krb.exit` files
6. Validate new snapshots. Confirm no other snapshots updated.
7. Commit snapshots if everything is verified

## Run Tests

The tests are run using the [tests.sh](../scripts/tests.sh) script.

```shell
./scripts/tests.sh
```

### Filtering Tests

```shell
./scripts/tests.sh pattern
```

### Verbose

Output additional information when running tests.

```shell
./scripts/tests.sh --verbose
```

## Update Snapshots

Run all tests and update any changed snapshot files.

```shell
./scripts/tests.sh --update
```
