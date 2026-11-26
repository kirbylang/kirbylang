```kirby
native fun @listCopy[T](list: [T]): [T];
```

Copy a list to a new list.

## Usage

```kirby
var a: [i64] = [1, 2, 3];
var b = a;
var c = @listCopy(a);

a[0] = 100;

@println(a); // [100, 2, 3]
@println(b); // [100, 2, 3]
@println(c); // [1, 2, 3]
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#listcopy
