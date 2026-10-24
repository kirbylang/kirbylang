# Basics

## Variables

Variables can be a mutable binding using the `var` keyword or an immutable binding using the `let` keyword.

```krb
let limit = 10;
var count = 0;

while (count <= limit) {
    @println($"Count: {count});

    count = count + 1;
}

limit = 100; // Error at 'value': Cannot assign to immutable binding
```

### Annotating types

Type annotations on variable declarations are optional (with a few exceptions).

```krb
let greeting: string = "Hello World";
```

## Builtin Types

| Type     |                          Examples | Notes                                                   |
| :------- | --------------------------------: | ------------------------------------------------------- |
| `unit`   |                              `()` |                                                         |
| `bool`   |                   `true`, `false` |                                                         |
| `nil`    |                             `nil` | This will be deprecated in the future.                  |
| `string` |                   `"Hello World"` | Strings are immutable                                   |
| `i64`    |                   `123`, `123i64` | Signed 64-bit integer. The default for whole numbers    |
| `i32`    |                  `123i32`, `7I32` | Signed 32-bit integer                                   |
| `i16`    |                          `123i16` | Signed 16-bit integer                                   |
| `i8`     |                           `123i8` | Signed 8-bit integer                                    |
| `u64`    |                          `123u64` | Unsigned 64-bit integer                                 |
| `u32`    |                          `123u32` | Unsigned 32-bit integer                                 |
| `u16`    |                          `123u16` | Unsigned 16-bit integer                                 |
| `u8`     |                  `123u8`, `255U8` | Unsigned 8-bit integer                                  |
| `f64`    |               `123f64`, `123.456` | 64-bit float. The default for decimal literals          |
| `f32`    |                `123f32`, `1.5f32` | 32-bit float. A decimal literal needs the suffix        |
| `[T]`    | `[1, 2, 3]`, `["Hello", "World"]` | A list of one type that can grow, such as `[i64]`       |
| `[T; n]` |                       `[1, 2, 3]` | An array of one type with `n` items, such as `[i64; 3]` |

### Number Literals

A whole number such as `42` is an `i64`, and a number with a decimal point such
as `1.5` is an `f64`.

```krb
let count = 42;    // i64
let ratio = 1.5;   // f64
```

Where a whole number literal is used as another number type, it becomes that
type. This also works for arithmetic made only of literals, `if`/`else`
branches and the last value of a block.

```krb
let byte: u8 = 65;
let half: f64 = 7 / 2;            // 3.5
let total: f64 = if (big) 100 else 10;

let price: f64 = 2.5;
let doubled = price * 2;          // 2 is an f64 here
let mixed = [1, 2.5, 3];          // an array of 3 f64
```

A variable keeps its own type. Cast it with `as` to use it as another type.

```krb
let count = 10;
let average: f64 = (count as f64) / 4;
```

Dividing two integers drops the remainder, rounding toward zero.

```krb
@println(7 / 2);    // 3
@println(-7 / 2);   // -3
@println(7 % 2);    // 1
@println(7.0 / 2);  // 3.5
```

Integer math whose result doesn't fit the type is a runtime error, and the
program stops.

```krb
let byte: u8 = 255;
@println(byte + 1); // Integer overflow: 255 + 1 doesn't fit in u8, which holds 0 to 255.
```

### Casting Number Types

Number types can be explicitly converted to another number type with the `as` keyword.

```krb
let value: f64 = 42.0;
let byte: u8 = value as u8;

let widened: f64 = byte as f64;
let int: i32 = value as i32;
```

| From          | To         | Result                                                            |
| ------------- | ---------- | ----------------------------------------------------------------- |
| a float       | an integer | Must be a whole number in the integer's range. Checked at runtime |
| an integer    | an integer | Must fit in the target. Checked at runtime                        |
| an integer    | a float    | Rounded to the nearest float. Exact when it fits the float        |
| the same type | itself     | Unchanged                                                         |

```krb
let too_big: f64 = 300.0;
let byte: u8 = too_big as u8; // Error: can't convert f64 300 to u8
```

Use parentheses to cast just a part of an expression.

```krb
let sum: u8 = 2 * 3 as u8;      // (2 * 3) as u8
let total: u8 = a + (b as u8);  // cast only `b`
```

## Flow Control

### `if`

```krb
let value = 123;
var even_or_odd = "";

if (value % 2 == 0) {
    even_or_odd = "Even";
} else {
    even_or_odd = "Even";
}
```

#### `if` expressions

When in an expression position, `if` is treated as an expression. All branches must return the same type.

```krb
let value = 123;
let even_or_odd = if (value % 2 == 0) "Even" else "Odd";
```

### `while`

```krb
var count = 0;

while (count <= 10) {
    count = count + 1;

    @println($"Count: {count}");
}
```

### `for`

```
for (var i = 0; i < 10; i = i + 1) {
    @println($"Count: {i}");
}
```

### `break`/`continue`

Available inside of `for` and `while` loops.

## Functions

Functions must have parameter types and a return type.

```krb
fun sum(a: f64, b: f64): f64 {
    return a + b;
}
```

### Lambdas

Excluding the function name creates a lambda instead.

```krb
let sum = fun (a: f64, b: f64): f64 {
    return a + b;
}
```

### Implicit Returns

Functions and lambdas can implicit return if the last line of the body is a bare expression (no semicolon).

```krb
fun sum(a: f64, b: f64): f64 {
    a + b
}
```

```krb
let sum = fun (a: f64, b: f64): f64 {
    a + b
}
```

### Expression Bodies

Functions even support expressions as a body.

```krb
fun sum(a: f64, b: f64): f64 = a + b;
```

This isn't supported for lambdas at this time.

### Generic Functions

A function can take a type that is chosen by each call. Declare it in brackets after the name. The call works out the type from its arguments.

```krb
fun first[T](items: [T]): T {
    return items[0];
}

@println(first([10, 20, 30]));   // 10
@println(first(["a", "b"]));     // a
```

Inside the function, nothing is known about `T`, so a `T` can be passed around, returned and stored, but not added or called. Every argument that uses the same `T` has to be the same type.

### Builtin Functions

Kirby has a number of builtin functions. These are prefixed with `@`.

## Custom Types

### Structs

Structs are a custom data types that group together a set of [variables](#variables) (fields) and [functions](#functions) (methods) that operator on those fields.

Fields and methods are private by default. The `pub` keyword declares them as public.

#### Fields

Fields are defined in the struct's declaration body.

```krb
struct Counter {
    let limit: f64;
    var count: f64;
}
```

#### Methods

Methods are defined in the struct's implementation body.

```krb
impl Counter {
    pub fun increment(self): unit {
        self.count = self.count + 1;
    }

    pub fun get_count(self): f64 = self.count;

    pub fun run(self): unit {
        while (self.count <= self.limit) {
            @println($"Count: {count});

            self.increment();
        }
    }
}
```

#### Generic Structs

A struct can take types too. Write them in brackets after the name, and give them when you use the type.

```krb
struct Box[T] {
    pub var value: T;
}

impl Box[T] {
    pub fun new(value: T): Self[T] = Self { value: value };

    pub fun get(self): T = self.value;

    pub fun map[U](self, f: fun (T) => U): Self[U] = Self.new(f(self.value));
}

let number = Box.new(5);           // Box[i64]
let text: Box[string] = Box { value: "hi" };

@println(number.get());            // 5
```

`Box[i64]` and `Box[string]` are different types. The `impl` repeats the struct's types, and a method can add more, like `U` in `map`.

## Traits

Traits are a way to define an interface for shared behavior.

```
trait Area {
    fun get_area(self): f64;
}

struct Square {
    pub let size: f64;
}

struct Rect {
    pub let width: f64;
    pub let height: f64;
}

impl Area for Square {
    fun get_area(self): f64 =
        self.width * self.height;
}

impl Area for Square {
    fun get_area(self): f64 =
        self.size * self.size;
}

let square = Square { size: 10 };
let rect = Rect { width: 5, height: 10 };

@println($"Square: {square.get_area()}");  // Square: 100
@println($"Rect: {rect.get_area()}");  // Rect: 50
```
