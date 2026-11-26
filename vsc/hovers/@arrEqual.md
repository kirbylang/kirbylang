```kirby
native fun @arrEqual[T, const N](a: [T; N], b: [T; N]): bool;
```

Shallow compares two arrays' values for equality. Both arrays must have the same length and item type.

## Usage

```kirby
let a = [1, 2, 3];
let b = [1, 2, 3];

@println(@arrEqual(a, b)); // true
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#arrequal
