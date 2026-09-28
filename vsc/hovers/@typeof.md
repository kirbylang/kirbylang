Get a value's type.

```kirby
@println(@typeof(true)); // "bool"
@println(@typeof(123)); // "number"
@println(@typeof("Hello World")); // "string"
@println(@typeof([])); // "array"

struct Food {}

@println(@typeof(Food)); // struct

let food = Food {};

@println(@typeof(food)); // instance
```
