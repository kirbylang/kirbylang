```kirby
native fun @parseInt(text: string): i64;
```

Parse a string of digits, with an optional `+` or `-`, into an `i64`. Anything else in the string, including spaces, is a runtime error.

## Usage

```kirby
@println(@parseInt("123")); // 123
@println(@parseInt("-45")); // -45
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#parseint
