```kirby
native fun @getenv(name: string): string;
```

Get an environment variable's value. It is an error if the variable isn't set.

## Usage

```kirby
@println(@getenv("PATH"));
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#getenv
