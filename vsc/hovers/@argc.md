```kirby
native fun @argc(): i64;
```

The number of arguments passed to the program, counting the program itself.

## Usage

```kirby
@println(@argc()); // 2, for: krb run file.krb one
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#argc
