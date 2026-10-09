Split a string into an array of strings at each place the separator appears. An empty separator splits the string into single character strings.

```kirby
@println(@strSplit("a,b,c", ",")); // [a, b, c]
@println(@strSplit("abc", "")); // [a, b, c]
```

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#strsplit
