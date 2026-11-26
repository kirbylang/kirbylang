Get the name of a value's type, as a string. A number gives its number type, such as `i64`, `u8` or `f64`.

## Usage

```kirby
struct Food {}

@println(@typeof(true)); // bool
@println(@typeof(123)); // i64
@println(@typeof(1.5)); // f64
@println(@typeof("Hello World")); // string
@println(@typeof(nil)); // nil

let items: [i64] = [1, 2];
@println(@typeof(items)); // list
@println(@typeof([1, 2])); // array

@println(@typeof(@ceil)); // function
@println(@typeof(Food)); // struct

let food = Food {};

@println(@typeof(food)); // instance
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#typeof
