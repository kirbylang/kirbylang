```kirby
native fun @listJoin(list: [string], separator: string): string;
```

Concatenates every element of a list into one string, separated by a separator string. Every element must already be a string.

## Usage

```kirby
var parts: [string] = ["kirby", "is", "a", "language"];

@println(@listJoin(parts, " ")); // kirby is a language
@println(@listJoin(parts, ", ")); // kirby, is, a, language
@println(@listJoin(parts, "")); // kirbyisalanguage
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#listjoin
