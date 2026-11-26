```kirby
native fun @arrReverse[T, const N](array: [T; N]): [T; N];
```

Returns a new array with the items of an array in reverse order, with the same length.

## Usage

```kirby
let a = [1, 2, 3];

@println(@arrReverse(a)); // [3, 2, 1]
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#arrreverse
