```kirby
native fun @listSlice[T](list: [T], start: integer, end: integer): [T];
```

Copy the items from `start` up to, not including, `end` into a new list.

## Usage

```kirby
var list: [i64] = [10, 20, 30, 40, 50];

var slice = @listSlice(list, 1, 4);

@println(slice); // [20, 30, 40]
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#listslice
