```kirby
native fun @strLen(text: string): i64;
```

Get the number of bytes in a string, as an `i64`.

## Usage

```kirby
@println(@strLen("Hello, World!")); // 13
@println(@strLen("")); // 0
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#strlen
