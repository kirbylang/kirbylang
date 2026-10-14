Cast a number to another number type. Only `f64` and `u8` can be cast for now.

Casting to `f64` is always exact. Casting to `u8` checks the value at runtime: it must be an integer from 0 to 255, otherwise the program fails.

```
let value: f64 = 42.0;
let byte: u8 = value as u8;

let widened: f64 = byte as f64;
```

`as` binds looser than arithmetic, comparison, and unary operators, so it applies to the whole expression on its left.

```
let sum: u8 = 2 * 3 as u8;      // (2 * 3) as u8
let total: u8 = a + (b as u8);  // cast only b
```
