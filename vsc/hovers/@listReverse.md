```kirby
native fun @listReverse[T](list: [T]): [T];
```

Get a new list with the items in the opposite order.

## Usage

```kirby
var list: [i64] = [1, 2, 3];

let reversed = @listReverse(list);

@println(reversed); // [3, 2, 1]
@println(list); // [1, 2, 3]
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#listreverse
