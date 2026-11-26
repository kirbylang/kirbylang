```kirby
native fun @listContains[T](list: [T], item: T): bool;
```

Returns if a list contains a value.

## Usage

```kirby
var list: [i64] = [1, 2, 3];

@println(@listContains(list, 2)); // true
@println(@listContains(list, 99)); // false
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#listcontains
