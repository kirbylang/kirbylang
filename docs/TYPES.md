# Types

Kirby is a statically typed language.

## Primitive Types

Literal values have primitive types.

| Type     | Example          |
| -------- | ---------------- |
| `string` | `"Hello World"`  |
| `f64`    | `1.2345`         |
| `bool`   | `true` / `false` |
| `unit`   | `()`             |

## Complex Types

### Lists

A list holds items that all share one type. Its type is currenly only inferred from the
literal.

```
let list = [1, 2, 3];

print list[0];
```

### Structs

Structs are referred to as a type by name. Fields are declared with `var`.

```
struct Box {
    pub var value: f64;
}

let box: Box = Box { value: 100 };

print box.value;
```

#### Generics

Not supported yet. Generic parameters parse, but declaring or using one is
an error.

```
struct Box[T] {
    pub var value: T;
}
```

### Functions

Functions are referred to as a type using the `fun ([T0,] [T1,]) => U`
syntax. Every parameter and the return type must be annotated.

```
fun sum(a: f64, b: f64): f64 = a + b;

let sum2: fun (f64, f64) => f64 = sum;

print sum2(1, 2);
```

### Lambdas

Lambdas use the same type syntax.

```
let sum: fun (f64, f64) => f64 = fun (a: f64, b: f64): f64 { a + b };

print sum(1, 2);
```

A lambda's parameter types can be left off when the type it is checked
against already supplies them.

```
let double: fun (f64) => f64 = fun (n) { n * 2 };

print double(21);
```

## Type Aliases

```
type Number = f64;

let count: Number = 42;

print count;
```

## Operators

| Operator          | Operands        | Result   |
| ----------------- | --------------- | -------- |
| `+`               | two `f64`       | `f64`    |
| `+`               | two `string`    | `string` |
| `-` `*` `/` `%`   | two `f64`       | `f64`    |
| `<` `>` `<=` `>=` | two `f64`       | `bool`   |
| `==` `!=`         | two of one type | `bool`   |
| `and` `or`        | two `bool`      | `bool`   |
| `!`               | any             | `bool`   |
| `-` (negate)      | `f64`           | `f64`    |

## Limitations

- Native functions have no signatures, so calls to them aren't checked
- Generic types parse but aren't checked
- Lists have no type annotation syntax
