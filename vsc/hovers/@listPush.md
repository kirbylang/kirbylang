```kirby
native fun @listPush[T](list: [T], item: T): unit;
```

Push a value onto the end of a list.

## Usage

```kirby
var list: [string] = [];

@listPush(list, "Hello");
@listPush(list, "World");

@println(list); // [Hello, World]
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#listpush
