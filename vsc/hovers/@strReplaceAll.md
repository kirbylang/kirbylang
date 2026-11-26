```kirby
native fun @strReplaceAll(text: string, part: string, replacement: string): string;
```

Replace every place a string appears in another string. Returns the string unchanged if it does not appear, or if the string to replace is empty.

## Usage

```kirby
@println(@strReplaceAll("a-b-c", "-", "+")); // a+b+c
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#strreplaceall
