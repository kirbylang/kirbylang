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

`Display`, `Eq`, `Ord` (a supertrait of `Eq`), and `Default` are always in
scope:

```
trait Display { fun toString(self): string; }
trait Eq { fun equals(self, other: Self): bool; }
trait Ord: Eq { fun cmp(self, other: Self): f64; }
trait Default { fun default(): Self; }
```

`Into`, `From`, `Iter`, and `Len` need generics and aren't available yet.

`==` and `!=` on two structs require the struct to implement `Eq` --
comparing two structs of a type with no `Eq` impl is a compile error.
Primitives, arrays, and functions are unaffected; they never needed an
`Eq` impl and still don't. Note that implementing `Eq` only makes `==`
type-check -- the runtime comparison itself is unchanged (structural
identity for structs), since `==` doesn't yet dispatch to `.equals()`. Call
`.equals()` directly for a real structural comparison.

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

#### Trait methods are implicitly public

A method inside `impl Trait for X` doesn't need `pub` -- implementing a
trait's required method makes it public automatically, since that's the
whole point of implementing the trait. Writing `pub` explicitly is still
allowed and has no effect.

```
impl Cloneable for Point {
    fun clone(self): Self = Self { x: self.x, y: self.y }; // no pub needed
}
```

This is specific to trait impls. A plain `impl Struct { ... }` block still
defaults its methods to private, same as always -- `pub` there means what
it always has.

#### Limitations

- `impl Trait for` a primitive type (`f64`, `string`, `bool`, `unit`) isn't
  supported yet -- primitives have no runtime object to attach methods to,
  so this needs static call resolution the compiler doesn't have yet.
- Operators (`+`, `<`, etc.) don't dispatch to trait methods on structs
  yet -- `Add`, `Ord`-derived comparisons via operators, and friends need
  the same static-resolution work as primitive impls.
- Traits themselves can't be generic (`trait Into[T]`), and a struct
  implementing a trait can't be generic either, following the same
  generics restriction as everywhere else in the type system.
- Methods defined in trait impl blocks do not shadow methods of the same name in the target's impl block. This doesn't error, it just calls the target's impl version of it. In the future there will be a way to fully qualify which method to call e.g. `Trait.method(obj);`

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
- `impl Trait for` a primitive type isn't supported yet, and operators
  don't dispatch to trait methods -- see [Traits](#traits)
