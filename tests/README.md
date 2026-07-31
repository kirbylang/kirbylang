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

## Update Snapshots

Run all tests and update any changed snapshot files.

```shell
./scripts/tests.sh --update
```
