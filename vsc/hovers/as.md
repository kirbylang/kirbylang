Cast a number to another number type, such as `i64`, `u8` or `f32`.

Casting to an integer type checks the value at runtime: it must be a whole number in the range of the type, otherwise the program fails. Casting to a float type rounds to the nearest value it can hold, and a value out of its range is an error.

```
let value: f64 = 42.0;
let byte: u8 = value as u8;

let widened: f64 = byte as f64;
```

`as` binds looser than arithmetic, comparison, and unary operators, so it applies to the whole expression on its left.

```
let a: u8 = 1;
let b: i64 = 2;

let sum: u8 = 2 * 3 as u8;      // (2 * 3) as u8
let total: u8 = a + (b as u8);  // cast only b
```
