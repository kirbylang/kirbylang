Copy an array to a new array of the same length.

```kirby
let a = [1, 2, 3];
let b = @arrCopy(a);

b[0] = 100;

@println(a); // [1, 2, 3]
@println(b); // [100, 2, 3]
```

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#arrcopy
