Pop a value onto the end of a list and return it.

```kirby
var list: [i64] = [10, 20, 30, 40, 50];

var slice = @listSlice(list, 1, 4);

@println(@listEqual([20, 30, 40], slice));
```

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#listslice
