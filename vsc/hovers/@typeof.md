Get a value's type.

## Usage

```kirby
@println(@typeof(true)); // "bool"
@println(@typeof(123)); // "i64"
@println(@typeof("Hello World")); // "string"
let items: [i64] = [1, 2];
@println(@typeof(items)); // "list"
@println(@typeof([1, 2])); // "array"

struct Food {}

@println(@typeof(Food)); // struct

let food = Food {};

@println(@typeof(food)); // instance
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#typeof
