```kirby
native fun @listPop[T](list: [T]): T;
```

Remove and return the last value in a list. It is an error if the list is empty.

## Usage

```kirby
var list: [string] = ["Hello", "World"];

var poppedValue = @listPop(list);

@println(list); // [Hello]
@println(poppedValue); // World
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#listpop
