```kirby
native fun @listInsert[T](list: [T], index: integer, item: T): unit;
```

Insert a value into a list at a specific index.

## Usage

```kirby
var list: [string] = [];

@listInsert(list, 0, "Hello");
@listInsert(list, 1, "World");

@println(list); // [Hello, World]
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#listinsert
