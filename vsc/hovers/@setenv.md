```kirby
native fun @setenv(name: string, value: string): unit;
```

Set an environment variable to a string value.

## Usage

```kirby
@setenv("NAME", "WORLD");

@println("Hello " + @getenv("NAME"));
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#setenv
