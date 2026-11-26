```kirby
native fun @strRepeat(text: string, count: i64): string;
```

Repeat a string a number of times. The count must be a whole number, zero or more.

## Usage

```kirby
@println(@strRepeat("ab", 3)); // ababab
@println(@strRepeat("ab", 0)); // (empty string)
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#strrepeat
