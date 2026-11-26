```kirby
native fun @listEqual[T](a: [T], b: [T]): bool;
```

Shallow compares two lists' values for equality.

## Usage

```kirby
var a: [i64] = [1, 2, 3];
var b = a;
var c = @listCopy(a);

@println(@listEqual(a, b)); // true
@println(@listEqual(b, c)); // true
@println(@listEqual(a, c)); // true
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#listequal
