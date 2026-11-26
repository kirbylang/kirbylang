```kirby
native fun @listRemove[T](list: [T], index: integer): T;
```

Remove and return the value at a specific index.

## Usage

```kirby
var list: [i64] = [1, 2, 3];

@listRemove(list, 0);

@println(list); // [2, 3]
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#listremove
