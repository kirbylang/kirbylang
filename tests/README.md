# Tests

The language's E2E tests are file based:

| Example File    | Description              |
| --------------- | ------------------------ |
| `test.krb`      | Source test file         |
| `test.krb.out`  | Expected `stdout` output |
| `test.krb.err`  | Expected `stderr` output |
| `test.krb.exit` | Expected exit code       |
| `test.krb.in`   | Text sent to `stdin`     |
| `test.krb.env`  | Environment Variables    |
| `test.krb.skip-packaged` | Skip this test with `--packaged` |

## Writing A Test

1. Create a new file in the [`tests`](./) directory: `./tests/new_test.krb`
2. Add test kirby code
3. Implement feature being tested
4. [Update snapshots](#update-snapshots) which will create `./tests/new_test.krb.out`, `./tests/new_test.krb.err`, `./tests/new_test.krb.exit` files
5. Validate new snapshots. Confirm no other snapshots updated.
6. Commit snapshots if everything is verified

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

## Packaged Programs

`--packaged` runs every test again, but builds each one into an executable with
`krb build` and runs that, then compares with the same snapshots.

```shell
./scripts/tests.sh --packaged
```

A test whose output depends on how it was started, such as one that prints
`@argv(0)`, can opt out by adding a `test.krb.skip-packaged` file. Say why in the
file.

If the build fails, the build's `stderr` must be the end of the snapshot's,
because `krb run` prints the standard library's bytecode before a compile error
and `krb build` does not. `--packaged` can't be used with `--update`.

`./scripts/test-build.sh` tests `krb build` itself: its arguments, its errors,
and damaged programs.

## Update Snapshots

Run all tests and update any changed snapshot files.

```shell
./scripts/tests.sh --update
```
