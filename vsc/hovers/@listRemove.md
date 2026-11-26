```kirby
native fun @listRemove[T](list: [T], index: integer): T;
```

Remove and return the value at a specific index. It is an error if the index is outside the list.

## Usage

```kirby
var list: [i64] = [1, 2, 3];
var removed = @listRemove(list, 0);

@println(list); // [2, 3]
@println(removed); // 1
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#listremove
