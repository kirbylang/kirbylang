Declare a function that is written in C. Only the standard library can use `native`.

The declaration gives the function's type and has no body. The name starts with `@`.

```kirby
native fun @strLen(text: string): i64;
native fun @listPop[T](list: [T]): T;
native fun @arrLen[T, const N](array: [T; N]): i64;
native fun @numberToString(value: number): string;
native fun @strSlice(text: string, start: integer, end: integer): string;
```

`const N` declares an array length as a parameter, in the same way `T` declares a type.

`number` accepts any number kind, and `integer` any integer kind. Both can only be used in a native declaration.
