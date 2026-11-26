```kirby
native fun @round(value: f64): f64;
```

Round a number to the nearest whole number. Halves round away from zero.

## Usage

```kirby
@println(@round(2.4)); // 2
@println(@round(2.5)); // 3
@println(@round(-2.5)); // -3
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#round
