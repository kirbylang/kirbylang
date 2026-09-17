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

### Arrays

```
let arr: Array[f64] = [1, 2, 3];

print arr[0];
```

Nesting works the same way any other generic type argument does.

```
let matrix: Array[Array[f64]] = [[1, 2], [3, 4]];

print matrix[0][1];
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

A struct can declare its own type parameters.

```
struct Box[T] {
    pub var value: T;
}

let box: Box[f64] = Box { value: 100 };

print box.value;
```

`Box[f64]` and `Box[string]` are different, incompatible types even though
they both come from the same generic struct.

```
struct Box[T] {
    pub var value: T;
}

var b: Box[string] = Box { value: "Hello World" };
var a: Box[f64] = b; // Error: Expected Box[f64], got Box[string].
```

Inside an `impl` block for a generic struct, `Self` can take the same type
arguments to refer to a specific instantiation.

```
struct Box[T] {
    pub var value: T;
}

impl Box[T] {
    pub fun new(value: T): Self[T] = Self { value: value };
}

let box = Box.new(5);
```

A method can also declare its own type parameter, separate from the
struct's own.

```
struct Box[T] {
    pub var value: T;
}

impl Box[T] {
    pub fun new(value: T): Self[T] = Self { value: value };

    pub fun map[U](self, transform: fun (T) => U): Self[U] =
        Self.new(transform(self.value));
}

fun double(value: f64): f64 = value * 2;

let box: Box[f64] = Box.new(5);
let doubled: Box[f64] = box.map(double);

print doubled.value; // 10
```

### Traits

A trait declares required methods for a struct to implement.

```
trait Display {
    fun toString(self): string;
}

struct Point {
    pub var x: f64;
    pub var y: f64;
}

impl Display for Point {
    fun toString(self): string = "Point";
}

print (Point { x: 1, y: 2 }).toString();
```

#### Builtin traits

The builtin, always in scope, traits are `Display`, `Eq`, `Ord`, and `Default`.

##### Eq

```
trait Eq {
    fun equals(self, other: Self): bool;
}

struct Box {
    pub var value: f64;
}

impl Eq for Box {
    fun equals(self, other: Self): bool = self.value == other.value;
}

let a = Box { value: 100 };
let b = Box { value: 100 };

a.equals(b); // true
```

##### Default

```
trait Default {
    fun default(): Self;
}

struct Box {
    pub var value: f64;
}

impl Default for Box {
    fun default(): Self = Self { value: 0 };
}

let box = Box.default();

box.value; // 0
```

##### Display

```
trait Display {
    fun toString(self): string;
}

struct Box {
    pub var value: f64;
}

impl Display for Box {
    fun toString(self): string = numberToString(self.value);
}

let box = Box { value: 123 };

box.toString(); // "123.000000"
```

##### Ord

```
trait Ord: Eq {
    fun cmp(self, other: Self): f64;
}

trait Eq {
    fun equals(self, other: Self): bool;
}

struct Box {
    pub var value: f64;
}

impl Eq for Box {
    fun equals(self, other: Self): bool = self.value == other.value;
}

impl Ord for Box {
    fun cmp(self, other: Self): f64 = self.value - other.value;
}

let a = Box { value: 100 };
let b = Box { value: 123 };

a.cmp(b); // -23
```

##### Add/Sub/Mul/Div

```
struct Point {
    pub var x: f64;
    pub var y: f64;
}

impl Point {
    pub fun new(x: f64, y: f64): Self {
        Self {
            x: x,
            y: y,
        }
    }
}

impl Display for Point {
    fun toString(self): string {
        "(" + numberToString(self.x) + "," + numberToString(self.y) + ")"
    }
}

impl Add for Point {
    fun add(self, other: Self): Self {
        Self {
            x: self.x + other.x,
            y: self.y + other.y,
        }
    }
}

impl Sub for Point {
    fun sub(self, other: Self): Self {
        Self {
            x: self.x - other.x,
            y: self.y - other.y,
        }
    }
}

impl Div for Point {
    fun div(self, other: Self): Self {
        Self {
            x: self.x / other.x,
            y: self.y / other.y,
        }
    }
}

impl Mul for Point {
    fun mul(self, other: Self): Self {
        Self {
            x: self.x * other.x,
            y: self.y * other.y,
        }
    }
}

let point_a = Point.new(10, 20);
let point_b = Point.new(30, 40);
let point_c = point_a + point_b;
let point_d = point_c - Point.new(5, 5);
let point_e = point_d / Point.new(5, 5);
let point_f = point_e * Point.new(3, 3);

print point_a.toString();
print point_b.toString();
print point_c.toString();
print point_d.toString();
print point_e.toString();
print point_f.toString();
```

#### `Self`

`Self` is a special type/value only available in `impl` blocks and is an alias to the impl block's target struct.

```
struct Point {
    pub var x: f64;
    pub var y: f64;
}

impl Point {
    pub fun origin(): Self = Self { x: 0, y: 0 };

    pub fun translate(self, dx: f64, dy: f64): Self =
        Self { x: self.x + dx, y: self.y + dy };
}

var p = Point.origin().translate(3, 4);
```

##### Generics

`Self` takes the same generics as it's impl target.

```
struct Wrapper[T] {
    pub var value: T;
}

impl Wrapper[T] {
    pub fun new(value: T): Self[T] = Self { value: value };
}

let wrapper = Wrapper.new(123);

wrapper.value = "Hello World"; // Error: Expected f64, got string.
```

#### Supertraits

`trait Sub: Super` declares that an `impl Super for X` must exist for any
`impl Sub for X`.

```
trait Ord: Eq {
    fun cmp(self, other: Self): f64;
}
```

#### Limitations

- Traits can only be implemented for structs currently
- The `Eq` and `Ord` traits are typechecked only. At runtime `==` and `<` still use compiler logic. This is a future change.
- If a struct's impl block implements a method of the same name as a trait, the struct's impl method is what is called. A future change will allow `Trait.method(struct)` to be used to fully qualify the trait versions of the method.

#### Known Bugs

- `impl Trait for UndefinedStruct` is a runtime error

### Functions

Functions are referred to as a type using the `fun ([T0,] [T1,]) => U`
syntax. Every parameter and the return type must be annotated.

```
fun sum(a: f64, b: f64): f64 = a + b;

let sum2: fun (f64, f64) => f64 = sum;

print sum2(1, 2);
```

#### Generics

A function can declare its own type parameters, resolved from the
arguments at each call site.

```
fun id[T](value: T): T = value;

print id(5);    // 5
print id("hi"); // hi
```

Using `+`, `-`, `*`, or `/` inside a generic function body requires every
type the function is ever called with to implement the matching trait
(`Add`, `Sub`, `Mul`, `Div`). `f64` implements all four and `string`
implements `Add`; a user struct needs its own `impl` block -- see
[Add/Sub/Mul/Div](#addsubmuldiv).

```
fun sum[T](a: T, b: T): T = a + b;

print sum(1, 2);     // 3
print sum("a", "b"); // ab
```

```
fun sub[T](a: T, b: T): T = a - b;

print sub("a", "b"); // Error: string doesn't implement Sub
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

### Native Functions

Most native functions have a type, so calls to them are checked at compile
time and they can be used as values.

```
let round: fun (f64) => f64 = ceil;

print round(1.2);
```

A native whose type can't be spelled yet has no signature, and calls to it
are checked at runtime instead. That covers `len`, `typeof`, `instanceOf`,
the `is*` family and the `arr*` family, which need generics, and `argv`,
`prompt` and `stdin`, which return nothing on some paths and so need
`Option[T]`.

Declaring a function with the same name as a native shadows it.

## Type Aliases

```
type Number = f64;

let count: Number = 42;

print count;
```

An alias can take its own type parameters too.

```
type Wrapper[T] = T;

let value: Wrapper[f64] = 5;

print value;
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

`==`/`!=` between two structs additionally requires the struct to
implement `Eq` -- see [Traits](#traits).

`+` `-` `*` `/` between two structs of the same type additionally requires
that struct to implement `Add`/`Sub`/`Mul`/`Div` respectively -- see
[Add/Sub/Mul/Div](#addsubmuldiv). `f64` and `string` don't go through
trait dispatch for these operators; they still use dedicated op codes.

## Limitations

- Some native functions have no signature yet, so calls to them aren't
  checked
- `impl Trait for` a primitive type (`f64`, `string`, `bool`, `unit`) isn't
  supported yet -- see [Traits](#traits)
- Operators only dispatch to trait methods for structs. `f64` and `string`
  still use dedicated op codes for `+`, `-`, `*`, `/`, `==`, and `<` rather
  than their trait implementations
