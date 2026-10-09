Exit (code 70) with an error message if the asserted condition is `false`.

```kirby
@assert(1 + 1 == 2, "math is broken"); // passes
@assert(false, "expected a positive number"); // assertion failed: expected a positive number
```

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#assert
