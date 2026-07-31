# kirby

[![Verify](https://github.com/kirbylang/kirbylang/actions/workflows/ci.yml/badge.svg)](https://github.com/kirbylang/kirbylang/actions/workflows/ci.yml)

An aspiring embeddable scripting language.

```kirby
#!/usr/bin/env krb -f

// Function with an expression body
fun fizzbuzz(n) =
    if (n % 5 == 0 and n % 3 == 0) "FizzBuzz"
    else if (n % 3 == 0) "Fizz"
    else if (n % 5 == 0) "Buzz"
    else n;

// Read from stdin
var limit = parseNumber(prompt("limit: "));

for (var i = 1; i <= limit; i = i + 1) {
    print fizzbuzz(i);
}
```

## Documentation

- [CLI](./docs/CLI.md)
- [Development](./docs/DEVELOPMENT.md)
- [Goals](./docs/GOALSV1.md)
- [Tests](./tests/README.md)
- [Scripts](./scripts/README.md)
- [Important Files](./docs/DEVELOPMENT.md#important-files)
- [Writing A Test](./tests/README.md#writing-a-test)
- [Create A Native Function](./docs/DEVELOPMENT.md#create-a-native-function)
