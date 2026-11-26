Returns if value is a function or not.

## Usage

```kirby
fun sum(a: i64, b: i64): i64 = a + b;

@println(@isFunction(@ceil)); // true
@println(@isFunction(sum)); // true
@println(@isFunction(123)); // false
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#isfunction
