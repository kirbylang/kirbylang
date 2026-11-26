```kirby
native fun @arrContains[T, const N](array: [T; N], item: T): bool;
```

Returns if an array contains a value.

## Usage

```kirby
let a = [1, 2, 3];

@println(@arrContains(a, 2)); // true
@println(@arrContains(a, 99)); // false
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#arrcontains
