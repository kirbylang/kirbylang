```kirby
native fun @listEqual[T](a: [T], b: [T]): bool;
```

Shallow compares two lists' values for equality. A list or struct held inside is only equal to itself, not to a copy with the same contents.

## Usage

```kirby
var a: [i64] = [1, 2, 3];
var b = a;
var c = @listCopy(a);

@println(@listEqual(a, b)); // true
@println(@listEqual(b, c)); // true
@println(@listEqual(a, c)); // true

c[0] = 100;

@println(@listEqual(a, c)); // false
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#listequal
