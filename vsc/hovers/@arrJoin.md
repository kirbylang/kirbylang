Concatenates every element of an array into one string, separated by a separator string. Every element must already be a string.

```kirby
var parts = ["kirby", "is", "a", "language"];

@println(@arrJoin(parts, " ")); // kirby is a language
@println(@arrJoin(parts, ", ")); // kirby, is, a, language
@println(@arrJoin(parts, "")); // kirbyisalanguage
```

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#arrjoin
