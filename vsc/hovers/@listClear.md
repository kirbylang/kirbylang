```kirby
native fun @listClear[T](list: [T]): unit;
```

Clears all values from a list.

## Usage

```kirby
var list: [i64] = [1, 2, 3];

@listClear(list);

@println(list); // []
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#listclear
