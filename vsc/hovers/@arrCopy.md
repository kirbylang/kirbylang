```kirby
native fun @arrCopy[T](array: [T; N]): [T; N];
```

Copy an array to a new array of the same length.

## Usage

```kirby
let a = [1, 2, 3];
let b = @arrCopy(a);

b[0] = 100;

@println(a); // [1, 2, 3]
@println(b); // [100, 2, 3]
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#arrcopy
