Push a value onto the end of a list.

```kirby
var list: [string] = [];

@listPush(list, "Hello");
@listPush(list, "World");

@println(list); // [Hello, World]
```

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#listpush
