```kirby
native fun @listLen[T](list: [T]): i64;
```

Get the number of elements in a list, as an `i64`.

## Usage

```kirby
@println(@listLen([1, 2, 3])); // 3
@println(@listLen([])); // 0
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#listlen
