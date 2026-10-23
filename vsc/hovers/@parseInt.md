Parse a string of digits, with an optional `+` or `-`, into an `i64`. Anything else in the string, including spaces, is a runtime error.

```kirby
@println(@parseInt("123")); // 123
@println(@parseInt("-45")); // -45
```

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#parseint
