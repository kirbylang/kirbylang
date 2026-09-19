Get a value's type.

```kirby
print @typeof(true); // "bool"
print @typeof(123); // "number"
print @typeof("Hello World"); // "string"
print @typeof([]); // "array"

struct Food {}

print @typeof(Food); // struct

let food = Food {};

print @typeof(food); // instance
```
