Parse a number such as `1.5` or `-2` into an `f64`. The string is written like a Kirby number literal without a suffix. Anything else in the string, including spaces, is a runtime error.

```kirby
@println(@parseFloat("1.5")); // 1.5
@println(@parseFloat("42")); // 42
```

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#parsefloat
