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

An array holds items that all share one type. Generic types aren't supported yet so the type is just `Array`.

```
let arr: Array = [1, 2, 3];

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
    fun default(): Self = Self { value = 0 };
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

#### Supertraits

`trait Sub: Super` declares that an `impl Super for X` must exist for any
`impl Sub for X`.

```
trait Ord: Eq {
    fun cmp(self, other: Self): f64;
}
```

#### Impl blocks on primitives

`f64`, `string`, `bool`, and `unit` can have both plain `impl` blocks and
trait `impl` blocks, the same as a struct -- but only in
`stdlib/stdlib.krb`. Declaring one anywhere else is a compile error;
calling a method one of these already declares has no such restriction.

```
impl f64 {
    pub fun double(self): f64 = self * 2;
}

impl Display for f64 {
    pub fun toString(self): string = numberToString(self);
}
```

A primitive has no runtime instance to attach a method to or dispatch
through, so a call like `x.double()` is resolved to a specific function
at compile time instead of the dynamic dispatch a struct method call
uses. `pub`/private visibility is enforced right there, at compile time
-- stronger than a struct's, which is only caught at runtime: a private
method is only callable from another method in an impl block for that
same primitive, regardless of which impl block declared either one.

`Self` works the same way it does for a struct:

```
impl Default for f64 {
    pub fun default(): Self = 0;
}

print f64.default(); // 0
```

#### Limitations

- Traits can only be implemented for structs and the scalar primitives
  (`f64`, `string`, `bool`, `unit`) -- not `Array` yet, which needs
  generics first
- Impl blocks on primitives can only be declared in `stdlib/stdlib.krb`,
  regardless of `pub`
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

## Limitations

- Some native functions have no signature yet, so calls to them aren't
  checked
- Generic types parse but aren't checked
- Lists have no type annotation syntax
- Operators don't dispatch to trait methods yet (`+`, `<`, etc. always use
  compiler logic, even for structs implementing a matching trait) -- see
  [Traits](#traits)
- Impl blocks on primitive types are restricted to `stdlib/stdlib.krb` --
  see [Impl blocks on primitives](#impl-blocks-on-primitives)
