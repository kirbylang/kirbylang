Convert a string to an array of `u8` bytes, one element per byte, in order. The elements are `u8`, so bytes 128..255 are preserved.

```kirby
let bytes: [u8] = @strToBytes("AB");
@println(bytes[0]); // 65
@println(bytes[1]); // 66
```

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#strtobytes
