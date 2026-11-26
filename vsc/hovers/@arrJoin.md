```kirby
native fun @arrJoin(array: [string; N], separator: string): string;
```

Concatenates every element of an array of strings into one string, separated by a separator string.

## Usage

```kirby
let parts = ["kirby", "is", "a", "language"];

@println(@arrJoin(parts, " ")); // kirby is a language
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#arrjoin
