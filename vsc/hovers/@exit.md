```kirby
native fun @exit(code: i64): unit;
```

Exit with an exit code, from `0` to `255`.

## Usage

```kirby
var exitCode = 1;
@exit(exitCode);
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#exit
