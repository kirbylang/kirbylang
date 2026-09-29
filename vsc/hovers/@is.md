Returns whether a value is of the named type.

The second argument is one of `"bool"`, `"string"`, `"number"`, `"function"`
or `"nil"`.

```kirby
@println(@is(true, "bool")); // true
@println(@is(12345, "string")); // false
```
