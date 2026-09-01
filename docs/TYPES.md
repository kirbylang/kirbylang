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

`List[T]` is a collection holding one or more `T` items.

```
let list: List[f64] = [1, 2, 3];
```

### Structs

Structs are referred as a type by name.

```
struct Box {
    pub let value: f64;
}

let box: Box = Box { value: 100 };

print box.value;
```

#### Generics

Not fully supported. Generic types are ignored currently.

```
struct Box[T] {
    pub let value: T;
}

let box: Box[f64] = Box { value: 100 };

print box.value;
```

### Functions

Functions are referred as a type using the `fun ([T0,] [T1,]) => U` syntax.

```
fun sum(a: f64, b:64): f64 = a + b;

let sum2: fun (f64, f64) => f64 = sum;

print sum2(1, 2);
```

### Lambdas

Lambas are referred as a type using the `fun ([T0,] [T1,]) => U` syntax.

```
let sum: fun (f64, f64) => f64 = fun (a: f64, b:64): f64 { a + b };

print sum(1, 2);
```

## Type Aliases

```
type number = f64;
```
