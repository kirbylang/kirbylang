# kirby

[![Verify](https://github.com/kyleect/kirby/actions/workflows/ci.yml/badge.svg)](https://github.com/kyleect/kirby/actions/workflows/ci.yml)

Yet another kirby implementation.

```kirby
struct StringBuilder {
    var string = "";
}

impl StringBuilder {
    fun new() {
        StringBuilder {}
    }

    fun new_with_init(init) {
        StringBuilder { string: init }
    }

    fun add(self, string) {
        self.string = self.string + string;

        self
    }

    fun to_string(self) {
        self.string
    }
}

var builder = StringBuilder.new()
    .add("Hello")
    .add(" ")
    .add("World");

print builder.to_string();
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
