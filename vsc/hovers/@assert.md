```kirby
native fun @assert(condition: bool, message: string): unit;
```

Exit (code 70) with an error message if the asserted condition is `false`.

## Usage

```kirby
@assert(1 + 1 == 2, "math is broken"); // passes
@assert(false, "expected a positive number"); // assertion failed: expected a positive number
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#assert
