```kirby
native fun @strEndsWith(text: string, suffix: string): bool;
```

Returns if a string ends with another string.

## Usage

```kirby
@println(@strEndsWith("hello", "lo")); // true
@println(@strEndsWith("hello", "he")); // false
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#strendswith
