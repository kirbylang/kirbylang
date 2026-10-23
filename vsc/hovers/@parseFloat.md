Parse a number such as `1.5`, `-2` or `2.5e-4` into an `f64`. The string is an optional `+` or `-`, digits, an optional `.` followed by digits, and an optional exponent (`e` or `E`, an optional sign, then digits). Anything else in the string, including spaces, is a runtime error.

```kirby
@println(@parseFloat("1.5")); // 1.5
@println(@parseFloat("42")); // 42
@println(@parseFloat("2.5e-4")); // 0.00025
```

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#parsefloat
