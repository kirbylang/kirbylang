```kirby
native fun @clock(): f64;
```

Get the processor time the program has used, in seconds. Time spent waiting, such as for input, isn't counted.

## Usage

```kirby
@println(@clock());
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#clock
