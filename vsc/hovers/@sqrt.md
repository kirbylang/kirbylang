```kirby
native fun @sqrt(value: f64): f64;
```

Get the square root of a number. A negative number is an error. When the argument is a constant, such as `@sqrt(-1)`, the error is reported before the program runs.

## Usage

```kirby
@println(@sqrt(16)); // 4
@println(@sqrt(2.25)); // 1.5
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#sqrt
