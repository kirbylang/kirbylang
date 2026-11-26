```kirby
native fun @strSplit(text: string, separator: string): [string];
```

Split a string into a list of strings at each place the separator appears. An empty separator splits the string into single character strings.

## Usage

```kirby
@println(@strSplit("a,b,c", ",")); // [a, b, c]
@println(@strSplit("abc", "")); // [a, b, c]
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#strsplit
