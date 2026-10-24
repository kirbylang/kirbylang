Shallow compares two lists' values for equality.

```kirby
var a: [i64] = [1, 2, 3];
var b = a;
var c = @listCopy(a);

@println(@listEqual(a, b)); // true
@println(@listEqual(b, c)); // false
@println(@listEqual(a, c)); // false
```

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#listequal
