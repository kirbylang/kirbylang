Insert a value into a list at a specific index.

```kirby
var list: [string] = [];

@listInsert(list, 0, "Hello");
@listInsert(list, 1, "World");

@println(list); // [Hello, World]
```

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#listinsert
