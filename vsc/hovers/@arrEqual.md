Shallow compares two arrays' values for equality.

```kirby
var a = [1, 2, 3];
var b = a;
var c = @arrCopy(a);

@println(@arrEqual(a, b)); // true
@println(@arrEqual(b, c)); // false
@println(@arrEqual(a, c)); // false
```

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#arrequal
