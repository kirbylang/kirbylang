Concatenates two lists in to a new list.

```kirby
var a: [i64] = [1, 2, 3];
var b: [i64] = [4, 5, 6];
var c = @listConcat(a, b);

@println(a); // [1, 2, 3]
@println(b); // [4, 5, 6]
@println(c); // [1, 2, 3, 4, 5, 6]
```

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#listconcat
