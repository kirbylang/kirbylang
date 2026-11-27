```kirby
native fun @arrConcat[T, const N, const M](a: [T; N], b: [T; M]): [T];
```

Concatenates two arrays into a new list. The result is a list because its length isn't known when the program is compiled.

## Usage

```kirby
let a = [1, 2, 3];
let b = [4, 5, 6];

@println(@arrConcat(a, b)); // [1, 2, 3, 4, 5, 6]
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#arrconcat
