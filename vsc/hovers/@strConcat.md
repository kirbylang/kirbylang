```kirby
native fun @strConcat(parts: [string]): string;
```

Concatenate a list of strings into one string. Every element must already be a string.

## Usage

```kirby
@println(@strConcat(["Hello", ", ", "World"])); // Hello, World
@println(@strConcat([])); // an empty string
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#strconcat
