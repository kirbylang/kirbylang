```kirby
native fun @strStartsWith(text: string, prefix: string): bool;
```

Returns if a string starts with another string.

## Usage

```kirby
@println(@strStartsWith("hello", "he")); // true
@println(@strStartsWith("hello", "lo")); // false
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#strstartswith
