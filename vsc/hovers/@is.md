Returns whether a value is of the named type.

The second argument is a name that `@typeof` returns, such as `"bool"`, `"string"`, `"i64"`, `"function"` or `"nil"`. There is no name that matches every number type, so use `@isNumber` for that.

## Usage

```kirby
@println(@is(true, "bool")); // true
@println(@is(12345, "i64")); // true
@println(@is(12345, "string")); // false
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#is
