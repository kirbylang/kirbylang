```kirby
native fun @arrToList[T, const N](array: [T; N]): [T];
```

Copies an array to a new list, which can grow and shrink.

## Usage

```kirby
let a = [1, 2, 3];
let list = @arrToList(a);

@listPush(list, 4);

@println(list); // [1, 2, 3, 4]
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#arrtolist
