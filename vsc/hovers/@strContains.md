```kirby
native fun @strContains(text: string, part: string): bool;
```

Returns if a string contains another string.

## Usage

```kirby
@println(@strContains("hello world", "lo wo")); // true
@println(@strContains("hello", "xyz")); // false
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#strcontains
