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

| Type     |                          Examples | Notes                                              |
| :------- | --------------------------------: | -------------------------------------------------- |
| `string` |                   `"Hello World"` |                                                    |
| `f64`    |               `123f64`, `123.456` | The `f64` suffix is optional                       |
| `unit`   |                              `()` |                                                    |
| `bool`   |                   `true`, `false` |                                                    |
| `Array`  | `[1, 2, 3]`, `["Hello", "World"]` | A dynamically sized collection of homonogous types |
| `nil`    |                             `nil` | This will be deprecated in the future.             |

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
