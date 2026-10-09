Returns if value is a function or not.

```kirby
fun sum(a, b) = a + b;

@println(@isFunction(@ceil)); // true
@println(@isFunction(sum)); // true
@println(@isFunction(123)); // false
```

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#isfunction
