Copies an array to a new list, which can grow and shrink.

```kirby
let a = [1, 2, 3];
let list = @arrToList(a);

@listPush(list, 4);

@println(list); // [1, 2, 3, 4]
```

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#arrtolist
