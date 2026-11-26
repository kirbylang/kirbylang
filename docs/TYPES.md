# Types

Kirby is a statically typed language.

## Primitive Types

Literal values have primitive types.

| Type     | Example          |
| -------- | ---------------- |
| `string` | `"Hello World"`  |
| `i64`    | `42`             |
| `f64`    | `1.2345`         |
| `bool`   | `true` / `false` |
| `unit`   | `()`             |

### Number Literals

A whole number literal is an `i64` unless it is used where another number type
is expected. Then it becomes that type, when its value fits exactly.

The expected type comes from an annotation, a parameter or return type, or the
other side of an arithmetic or comparison operator. It reaches through
brackets, `if`/`else` branches, a block's last value and arithmetic made only
of literals. The other branch of an `if`/`else` and the other elements of an
array literal count too.

```
let half: f64 = 7 / 2;     // 3.5
let byte: u8 = 200 + 55;   // 255
let mixed = [1, 2.5];      // [f64; 2]
```

Variables, suffixed literals (`2i64`) and decimal literals keep their own type.

### Lists

A list holds items that all share one type, and can grow and shrink. The type is written as the item type in brackets: `[i64]` is a list of `i64`, and `[[string]]` is a list of lists of `string`.

```
fun main(): unit {
    let numbers: [i64] = [1, 2, 3];
    let names: [string] = [];

    @println(numbers[0]);
}
```

### Arrays

An array holds a fixed number of items that all share one type. The length is part of the type and is written after the item type: `[i64; 3]` is an array of three `i64`s. The length is a whole number literal, and `[i64; 0]` is allowed.

```
fun first(values: [i64; 3]): i64 {
    return values[0];
}
```

A literal such as `[1, 2, 3]` is an array of that many items, unless its type is asked for. With a list type expected, the same literal is a list, and with an array type expected it must have exactly that many items. An empty literal `[]` has no items to learn a type from, so it needs a type to be expected, and a list or an array type both work.

```
let a = [1, 2, 3];              // [i64; 3]
let b: [i64] = [1, 2, 3];       // [i64]
let c: [i64; 3] = [1, 2, 3];    // [i64; 3]
let d: [i64] = [];              // an empty list
let f = [];                     // error: no item type
let e: [i64; 2] = [1, 2, 3];    // error: 3 items
```

An index that is a number literal past the end of an array is an error when the program is compiled. Any other bad index is an error when the program runs.

Two array types are the same type when both the item type and the length match. `[i64; 3]` is not `[i64; 4]`, and neither is `[i64]`. A list is never accepted where an array is expected, and an array is never accepted where a list is expected.

### Structs

Structs are referred to as a type by name. Fields are declared with `var`.

```
struct Box {
    pub var value: f64;
}

fun main(): unit {
    let box: Box = Box { value: 100 };

    @println(box.value);
}
```

#### Generic Structs

A struct can declare type parameters in brackets. They are written as
arguments when the type is used. `Box[f64]` and `Box[string]` are different
types, and the number of arguments has to match.

```
struct Box[T] {
    pub var value: T;
}

let a: Box[f64] = Box { value: 1 };
let b = Box { value: "hi" };     // Box[string]
let c: Box[string] = a;          // error: Expected Box[string], got Box[f64].
```

A literal works out its arguments from its fields. When a type is asked for,
the fields are checked against it, so the `1` above can be an `f64`. A
literal that can't work out an argument is an error.

An `impl` declares the same number of parameters, and they stand for the
struct's parameters in order. A method can declare more, which are numbered
after the impl's.

```
impl Box[T] {
    pub fun new(value: T): Self[T] = Self { value: value };
    pub fun get(self): T = self.value;
    pub fun map[U](self, f: fun (T) => U): Self[U] = Self.new(f(self.value));
}

fun main(): unit {
    let box = Box.new(5);            // Box[i64]
    let text = box.map(fun (n: i64): string { "n" });   // Box[string]
}
```

In a type, `Self[T]` is the struct with arguments and a plain `Self` is
`Box[T]`. In an expression, `Self` and `Box` are the struct itself, and a
call or literal works out the arguments.

`impl Trait for` a generic struct isn't supported yet.

### Generic Functions

A function can declare type parameters after its name. A call fills each one
in from its arguments, the first argument that mentions it fills it, and every
later argument has to agree.

```
fun id[T](x: T): T = x;
fun first[T](xs: [T]): T = xs[0];
fun apply[T, U](x: T, f: fun (T) => U): U = f(x);

fun main(): unit {
    let a = id(5);                  // i64
    let b: string = id(5);          // error: Expected string, got i64.
}
```

A parameter that appears in the signature but that no argument fills in is an
error: `Can't tell what 'T' is from the arguments.`. A parameter that isn't in
the signature at all is never asked for.

A parameter only the return type mentions is filled in from the type the call
is expected to return, when there is one:

```
fun empty[T](): [T] = [];

fun main(): unit {
    let a: [i64] = empty();    // T is i64
    let b = empty();           // error: Can't tell what 'T' is from the arguments.
}
```

A method works the same way, so `let s: Stack[i64] = Stack.new();` fills in
the `T` of `fun new(): Self[T]`.

Inside the function, each parameter is one type that is only equal to itself.
A value of that type can be passed around, returned and stored, but it can't
be added, indexed or have a method called on it, because nothing says its type
supports that. A [trait bound](#trait-bounds) is the exception: it allows the
methods of its trait. The error is reported in the function, not at the call.

```
fun add[T](a: T, b: T): T = a + b;   // error: '+' needs two numbers ... got T and T.
fun bad[A, B](a: A): B = a;          // error: Expected B, got A.
```

A generic function can also be used as a value where a function type is
expected. The expected type fills in the function's type parameters, and the
result has to match. Here `id` is the function declared above, and its `T`
becomes `i64`.

```
fun id[T](x: T): T = x;

let f: fun (i64) => i64 = id;       // ok
let g: fun (i64) => string = id;    // error: Expected fun (i64) => string, got fun (i64) => i64.
```

A generic method works the same way, such as `Box.new` for a method declared
`fun new(value: T): Self[T]`.

With no expected type, as in `let h = id;`, the value has no type and isn't
checked.

A generic function is compiled once, and the same code runs for every type.

### Methods On Primitive Types

A primitive type has methods that are written in Kirby, in an `impl` block in
the standard library:

```
impl string {
    pub fun len(self): i64 = @strLen(self);
}
```

`"abc".len()` calls it. Only the standard library can write these blocks, and
the parser rejects them in a user file with `You can't write an impl for a
primitive type in user code.`. They
work for `string`, `bool`, `unit` and every number kind (`i64`, `u8`, `f64` and
the rest), with methods that take `self`. Lists and arrays have them too, see
[Methods On Lists And Arrays](#methods-on-lists-and-arrays). A method belongs to its own type, so a method of `f64` can't be called
on an `i64`. A method has to be called, so `let f = "abc".len;` is an error. A
call to one is compiled as a plain function call with the value as the first
argument.

The number types have these methods:

| Type                       | Methods                                                              |
| -------------------------- | -------------------------------------------------------------------- |
| `f64`                      | `abs`, `ceil`, `floor`, `max`, `min`, `pow`, `round`, `sqrt`, `trunc` |
| `f32`                      | the same as `f64`                                                    |
| `i8`, `i16`, `i32`, `i64`  | `abs`, `max`, `min`                                                  |
| `u8`, `u16`, `u32`, `u64`  | `max`, `min`                                                         |

The `f64` methods do what the native functions of the same name do. An `f32`
method does its math as an `f64` and converts the result back, so a result that
doesn't fit in an `f32` is an error. An integer `abs` of the smallest value, such
as `-128` for an `i8`, is an overflow error. `bool` has no methods yet.

Inside the block, `Self` is the primitive type, so `other: Self` is a `u8`
in `impl u8`. A primitive has no fields or static methods, so `Self { }`
and `Self.name()` are errors.

The standard library can also implement a trait for these types:

```
impl Display for string {
    fun toString(self): string = self;
}
```

`"abc".toString()` is a plain call of the function `string.Display.toString`.
The impl must give every method the trait asks for, with the trait's signature,
and a type implements a trait once. A trait with a supertrait needs an impl of
the supertrait too. A method of the type itself wins over a trait method of the
same name. A static method of a trait is called on the type's name: `f64.default()` is `0`. Inside the impl, `Self.default()` does the same. A variable with the same name as the type wins over the type name.

The standard library implements these traits:

| Type                          | Traits                                                  |
| ----------------------------- | ------------------------------------------------------- |
| every number type             | `Display`, `Eq`, `Ord`, `Default`, `Add`, `Sub`, `Mul`, `Div`, `Mod` |
| signed integers, `f64`, `f32` | `Neg`                                                   |
| `string`                      | `Display`, `Eq`, `Default`, `Add`                       |
| `bool`                        | `Display`, `Eq`, `Default`                              |
| `unit`                        | `Display` (`nil`), `Eq`, `Ord`, `Default`               |

`string` has no `Ord` because there is no `<` on strings yet, and `bool` has no `Ord`. `cmp` returns `-1`, `0` or `1`.
Trait impls on a primitive can't be used with a bounded type parameter yet.

#### Methods On Lists And Arrays

A list or an array has methods in the same way. The impl declares its
parameters right after `impl`, then names the type it is for. `impl[T] [T]` is
for every list and `impl[T, const N] [T; N]` for every array:

```
impl[T] [T] {
    pub fun len(self): i64 = @listLen(self);
    pub fun copy(self): [T] = @listCopy(self);
}

impl[T, const N] [T; N] {
    pub fun len(self): i64 = @arrLen(self);
    pub fun copy(self): [T; N] = @arrCopy(self);
    pub fun concat[const M](self, other: [T; M]): [T] = @arrConcat(self, other);
}
```

`T` stands for the item type of the value the method is called on, so `first`
of `[string]` gives a `string`. `const N` declares a parameter that is an array
length instead of a type, so `copy` of a `[i64; 3]` gives a `[i64; 3]`. An
impl that doesn't need the length writes `_` (`impl[T] [T; _]`), and then no
signature can mention it. An array impl can't be for one length, such as
`[T; 3]`. Every parameter the impl declares has to be used by its type, and the
names are free to choose.

A method declares its own parameters after its name, as in
`fun pick[U](self, extra: U): U`, and can't reuse a name of its impl. `Self` is
the receiver's type. `impl[T] [T]` and `impl[T, const N] [T; N]` are separate,
so the same method name can be in both.

A list gets `len`, `isEmpty`, `push`, `pop`, `insert`, `remove`, `clear`,
`contains`, `copy`, `reverse`, `slice` and `concat`. An array gets `len`,
`contains`, `copy`, `reverse`, `slice`, `concat` and `toList`. Each calls the
native function of the same name. The ones that return a list (`slice`,
`concat`, `toList` of an array) return a list and not an array.

The same limits as the other primitives apply. Only the standard library can
write these blocks, and a user file gets `You can't write an impl for a
primitive type in user code.`. The item has to be a type parameter, so
`impl[T] [string]` is an error. There are no static methods, and a list or array
can't fill a bounded type parameter yet. A trait impl, such as
`impl[T] Display for [T]`, gives every list the trait with no condition on `T`,
because these parameters can't have bounds.

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

fun main(): unit {
    @println((Point { x: 1, y: 2 }).toString());
}
```

#### Calling A Trait's Method By Name

When a type has a method of its own with the same name as a trait's method, the
type's own method wins, so `point.toString()` calls it. `Trait.method(value)`
calls the trait's version instead:

```
struct Point {
    pub var x: f64;
    pub var y: f64;
}

impl Point {
    pub fun toString(self): string = "own";
}

impl Display for Point {
    fun toString(self): string = "trait";
}

fun main(): unit {
    let p = Point { x: 1, y: 2 };

    @println(p.toString());           // own
    @println(Display.toString(p));    // trait
}
```

The value is the first argument, and the others follow it, so
`Scale.scale(a, 3)` is the trait's `scale` called on `a` with `3`. It works for
a struct, for a primitive that implements the trait (`Display.toString(7)`), and
for a value of a type parameter whose bound includes the trait. It is an error
when the value's type doesn't implement the trait (`i64 doesn't implement
Greet.`), when the trait has no such method, and for a static method of the
trait, which is called on a type. A trait's method has to be called, so
`let f = Greet.hello;` is an error.

Code that is written for a trait always runs the trait's version. A call
through a type parameter's bound (`x.toString()` with `T: Display`), an
operator (`a + b` runs `Add`'s `add`, `==` runs `Eq`'s `equals`) and printing
(`@println(point)` and `$"{point}"` run `Display`'s `toString`) all do, even
when the type has its own method with the same name. At run time a struct keeps
each trait method under its plain name and also under the trait's name, such as
`Display.toString`, and these calls use the second.

#### Builtin traits

The builtin, always in scope, traits are `Display`, `Eq`, `Ord`, `Default`, and the operator traits `Add`, `Sub`, `Mul`, `Div`, `Mod` and `Neg`.

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

fun main(): unit {
    let a = Box { value: 100 };
    let b = Box { value: 100 };

    a.equals(b); // true
}
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
    fun toString(self): string = @numberToString(self.value);
}

fun main(): unit {
    let box = Box { value: 123 };

    box.toString(); // "123"
}
```

##### Ord

```
trait Ord: Eq {
    // Negative when self comes first, zero when equal, positive when other
    // comes first
    fun cmp(self, other: Self): i64;
}

trait Eq {
    fun equals(self, other: Self): bool;
}

struct Box {
    pub var value: i64;
}

impl Eq for Box {
    fun equals(self, other: Self): bool = self.value == other.value;
}

impl Ord for Box {
    fun cmp(self, other: Self): i64 = self.value - other.value;
}

fun main(): unit {
    let a = Box { value: 100 };
    let b = Box { value: 123 };

    a.cmp(b); // -23
}
```

##### Operator traits

An operator on structs calls a method of a trait. `a + b` calls `a.add(b)`, so
a struct supports `+` by implementing `Add`.

| Operator              | Trait  | Method                          |
| --------------------- | ------ | ------------------------------- |
| `+`                   | `Add`  | `fun add(self, other: Self): Self` |
| `-`                   | `Sub`  | `fun sub(self, other: Self): Self` |
| `*`                   | `Mul`  | `fun mul(self, other: Self): Self` |
| `/`                   | `Div`  | `fun div(self, other: Self): Self` |
| `%`                   | `Mod`  | `fun mod(self, other: Self): Self` |
| `-` (negate)          | `Neg`  | `fun neg(self): Self`           |
| `==` `!=`             | `Eq`   | `fun equals(self, other: Self): bool` |
| `<` `>` `<=` `>=`     | `Ord`  | `fun cmp(self, other: Self): i64` |

`a != b` is `!a.equals(b)`. `a < b` is `a.cmp(b) < 0`, and `<=`, `>` and `>=`
compare the result of `cmp` with 0 the same way.

```
struct Vec2 {
    pub var x: f64;
    pub var y: f64;
}

impl Add for Vec2 {
    fun add(self, other: Self): Self =
        Vec2 { x: self.x + other.x, y: self.y + other.y };
}

fun main(): unit {
    let sum = Vec2 { x: 1, y: 2 } + Vec2 { x: 3, y: 4 };   // Vec2 { x: 4, y: 6 }
}
```

Both sides have to be the same struct. A struct without the trait is an error:
`Money doesn't implement Add, required by '+'.`

Numbers have these traits built in, so a number can fill a parameter bounded by
one of them (see [Trait Bounds](#trait-bounds)), and `neg` is only for signed
numbers. Number operators that don't go through a bound still compile to the
built-in instructions, so plain number arithmetic costs nothing extra.

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

fun main(): unit {
    var p = Point.origin().translate(3, 4);
}
```

#### Supertraits

`trait Sub: Super` declares that an `impl Super for X` must exist for any
`impl Sub for X`.

```
trait Ord: Eq {
    fun cmp(self, other: Self): i64;
}
```

#### Limitations

- Traits can be implemented for structs, and by the standard library for `string`, `bool`, `unit`, the number kinds, lists and arrays. Other code can't implement a trait for a primitive, and a primitive can't yet be used where a type parameter has a trait bound (except numbers and the operator traits)
- Only numbers have the operator traits built in. `string` and `bool` implement `Eq` in the standard library, but can't be used where a type parameter has a bound yet. Neither has `Ord`
- If a struct's impl block implements a method of the same name as a trait, `value.method()` calls the struct's own method. Use `Trait.method(value)` to call the trait's version, see [Calling A Trait's Method By Name](#calling-a-traits-method-by-name).
- Two traits can't both provide a method of the same name to one struct, and a name can't be both an instance method and a static method on one type. Both are compile errors.

#### Known Bugs

- `impl Trait for UndefinedStruct` is a runtime error

### Trait Bounds

A type parameter can name a trait that its type must implement. The trait is
written after a colon, and `+` asks for more than one. Functions, methods,
structs and type aliases can have bounds.

```
trait Shape {
    fun area(self): f64;
}

struct Square {
    pub var side: f64;
}

impl Shape for Square {
    fun area(self): f64 = self.side * self.side;
}

struct Rock {
    pub var weight: f64;
}

fun describe[T: Shape](shape: T): f64 = shape.area();

fun main(): unit {
    describe(Square { side: 2 });   // ok
    describe(Rock { weight: 1 });   // error: Rock doesn't implement Shape, required by T.
    describe(5);                    // error: i64 doesn't implement Shape, required by T.
}
```

A call is checked once its arguments have filled in the parameter. The error
points at the first argument that mentions the parameter, or at the call when
only the type the call is expected to return filled it. A generic function used
as a value is checked the same way, so `let f: fun (Rock) => f64 = describe;` is
the same error.

Inside the function, a value of the parameter's type has the methods of its
traits and nothing else. It also has the methods of the traits' supertraits, so
`T: Ord` gives `equals` from `Eq`. `Self` in a trait method stands for the
parameter, so with `T: Cloneable`, `x.clone()` is a `T`.

```
fun largest[T: Ord](a: T, b: T): T {
    if (a > b) {
        return a;
    }

    return b;
}

fun compare[T: Eq](a: T, b: T): i64 = a.cmp(b);
// error: T's bound 'Eq' has no method 'cmp'.
```

A method the trait doesn't declare is an error in the function, even when every
type used with it has that method. Fields can't be read either.

A value whose bound is `Display` can also be given to `@println` and put in a
string with `$"{x}"`, the same as a struct that implements `Display`.

A bounded parameter can be given to another bounded function when its own bounds
have the trait that function needs. `fun f[T: Ord](x: T) = g(x)` works when `g`
needs `Ord` or `Eq`. It is an error when it doesn't, as when `T` has no bound:
`T doesn't implement Eq, required by U.`

#### Multiple bounds

`T: Display + Eq` needs both traits. The value has the methods of every trait
listed. If two of the traits have a method of the same name the call is
ambiguous: `Method 'label' is in both Named and Titled.` A call that fills the
parameter with a type missing one of them names that trait.

#### Structs and type aliases

A struct's type parameter can have a bound. Every use of the struct is checked,
whether it is written with a type (`Labeled[Rock]`), made by a literal
(`Labeled { value: rock }`), or made by a static method (`Labeled.new(rock)`).

```
struct Labeled[T: Display] {
    pub var value: T;
}

impl Labeled[T] {
    pub fun label(self): string = $"<{self.value}>";
}

let a: Labeled[Rock] = ...;   // error: Rock doesn't implement Display, required by Labeled.
```

An `impl` takes the bounds of its struct, so `impl Labeled[T]` has `T: Display`
and the methods can use it. Writing a bound on an `impl` is an error: `An impl's
type parameters take their bounds from the struct.`

A type alias can have bounds too, and they are checked where the alias is used.
An alias or function that passes its own parameter to a bounded struct needs the
same bound: `type Plain[T] = Labeled[T];` is an error, but
`type Plain[T: Display] = Labeled[T];` is fine.

#### Operators

A bounded value can use the operators of its traits. `T: Add` allows `a + b`,
`T: Ord` allows `<`, `>`, `<=`, `>=`, `==` and `!=`, and so on, see
[Operator traits](#operator-traits). The checker requires the bound:

```
fun sum[T: Add](a: T, b: T): T = a + b;

fun same[T](a: T, b: T): bool = a == b;
// error: T doesn't implement Eq, required by '=='.
```

A number can fill a parameter bounded by an operator trait, so `sum(1, 2)` and
`sum(1.5, 2.5)` work, as does `sum(vec1, vec2)` for a struct with `impl Add`.
Because the function is compiled once for every type, these operators call the
trait method at runtime, which for a number is a built-in method (`add`, `sub`,
`mul`, `div`, `mod`, `neg`, `equals` and `cmp`).

#### Limitations

- The trait has to exist, and a struct name isn't a trait:
  `Unknown trait 'Nope'.` A trait can be declared after the function that uses it.
- A primitive can't fill a bounded parameter yet, even though the standard
  library implements traits for `string`, `bool` and every number kind
  (`string implements Display, but a primitive can't be used with a bound
  yet`). Numbers can fill a parameter bounded by an operator trait only.
- A static trait method can't be called through a bound. `T.default()` is an
  error: `Can't call a static method on type parameter 'T'.` This needs a
  separate copy of the function for each type, which is not implemented.
- A bound can't be added to an `impl` beyond its struct's bounds.
- A bounded generic function used as a value with no expected type, such as
  `let f = describe;`, has no type, so its bounds aren't checked. See
  [Generic Functions](#generic-functions).

### Functions

Functions are referred to as a type using the `fun ([T0,] [T1,]) => U`
syntax. Every parameter and the return type must be annotated.

```
fun sum(a: f64, b: f64): f64 = a + b;

fun main(): unit {
    let sum2: fun (f64, f64) => f64 = sum;

    @println(sum2(1, 2));
}
```

### Lambdas

Lambdas use the same type syntax.

```
fun main(): unit {
    let sum: fun (f64, f64) => f64 = fun (a: f64, b: f64): f64 { a + b };

    @println(sum(1, 2));
}
```

A lambda's parameter types can be left off when the type it is checked
against already supplies them.

```
fun main(): unit {
    let double: fun (f64) => f64 = fun (n) { n * 2 };

    @println(double(21));
}
```

### Native Functions

Native functions start with `@` (a reserved character in user code).

```
fun main(): unit {
    let round: fun (f64) => f64 = @ceil;

    @println(round(1.2));
}
```

The standard library declares each native's type with `native`, which is a
function declaration with no body, for a function written in C:

```
native fun @listPop[T](list: [T]): T;
```

Only the standard library can use `native`, and the name starts with `@`.

List natives are typed by their element type, written `T` in their
signatures. `@listPop` is `fun ([T]) => T`, so popping from a list of `i64`
gives an `i64`, and `@listPush(numbers, 2.5)` is an error when `numbers` holds
`i64`s. Positions, such as the index of `@listInsert`, take an integer of any
kind.

```
let words: [string] = ["a", "b"];
let last = @listPop(words);   // string
@listPush(words, 1);          // Error: Expected string, got i64.
```

Array natives, such as `@arrLen` and `@arrCopy`, also take the array's length
as a parameter, declared with `const`:

```
native fun @arrCopy[T, const N](array: [T; N]): [T; N];
native fun @arrEqual[T, const N](a: [T; N], b: [T; N]): bool;
native fun @arrConcat[T, const N, const M](a: [T; N], b: [T; M]): [T];
```

Copying a `[i64; 3]` gives a `[i64; 3]`, and comparing arrays of different
lengths is an error. `@arrSlice` and `@arrConcat` give a list, because the
length of the result isn't a length the type system can work out.

Inside the body of a function or method, a `const` parameter is one fixed
length that only equals itself, so a `[T; M]` can't be returned as a `[T; N]`.
Only the standard library can declare `const` parameters.

A native whose type can't be spelled yet has no declaration, and calls to it
are checked at runtime instead. That covers `@typeof`, `@instanceOf`, the
`@is*` family and the print natives, which take a value of any type, and
`@argv`, `@prompt` and `@stdin`, which return nothing on some paths and so
need `Option[T]`.

## Type Aliases

```
type Number = f64;

fun main(): unit {
    let count: Number = 42;

    @println(count);
}
```

An alias can take type parameters. Using it gives the arguments, and the alias
stands for its target with them filled in, so `Wrapped[i64]` below is exactly
`Box[i64]`.

```
struct Box[T] {
    pub var value: T;
}

type Wrapped[T] = Box[T];
type Callback[T] = fun (T) => T;

let a: Wrapped[i64] = Box { value: 5 };
let b: Box[i64] = a;                  // same type
let c: Wrapped = a;                   // error: 'Wrapped' needs 1 type argument(s).
```

The number of arguments has to match, and an alias with no parameters takes
none.

## Operators

| Operator          | Operands                        | Result    |
| ----------------- | ------------------------------- | --------- |
| `+`               | two of one number type          | that type |
| `+`               | two `string`                    | `string`  |
| `+` `-` `*` `/` `%` | two of one struct, or one bounded type, with `Add` `Sub` `Mul` `Div` `Mod` | that type |
| `-` `*` `/` `%`   | two of one number type          | that type |
| `<` `>` `<=` `>=` | two of one number type          | `bool`    |
| `<` `>` `<=` `>=` | two of one struct, or one bounded type, with `Ord` | `bool` |
| `==` `!=`         | two of one type                 | `bool`    |
| `and` `or`        | two `bool`                      | `bool`    |
| `!`               | any                             | `bool`    |
| `-` (negate)      | a signed number                 | that type |
| `-` (negate)      | a struct, or bounded type, with `Neg` | that type |

`==`/`!=` between two structs, or two values of a generic parameter, requires
`Eq`, and calls its `equals` method. A struct operator calls the trait method, so
`a == b` and `a.equals(b)` always agree. See [Traits](#traits) and
[Trait Bounds](#trait-bounds).

# Types

Kirby is a statically typed language.

## Primitive Types

Literal values have primitive types.

| Type     | Example          |
| -------- | ---------------- |
| `string` | `"Hello World"`  |
| `i64`    | `42`             |
| `f64`    | `1.2345`         |
| `bool`   | `true` / `false` |
| `unit`   | `()`             |

### Number Literals

A whole number literal is an `i64` unless it is used where another number type
is expected. Then it becomes that type, when its value fits exactly.

The expected type comes from an annotation, a parameter or return type, or the
other side of an arithmetic or comparison operator. It reaches through
brackets, `if`/`else` branches, a block's last value and arithmetic made only
of literals. The other branch of an `if`/`else` and the other elements of an
array literal count too.

```
let half: f64 = 7 / 2;     // 3.5
let byte: u8 = 200 + 55;   // 255
let mixed = [1, 2.5];      // [f64; 2]
```

Variables, suffixed literals (`2i64`) and decimal literals keep their own type.

### Lists

A list holds items that all share one type, and can grow and shrink. The type is written as the item type in brackets: `[i64]` is a list of `i64`, and `[[string]]` is a list of lists of `string`.

```
fun main(): unit {
    let numbers: [i64] = [1, 2, 3];
    let names: [string] = [];

    @println(numbers[0]);
}
```

### Arrays

An array holds a fixed number of items that all share one type. The length is part of the type and is written after the item type: `[i64; 3]` is an array of three `i64`s. The length is a whole number literal, and `[i64; 0]` is allowed.

```
fun first(values: [i64; 3]): i64 {
    return values[0];
}
```

A literal such as `[1, 2, 3]` is an array of that many items, unless its type is asked for. With a list type expected, the same literal is a list, and with an array type expected it must have exactly that many items. An empty literal `[]` has no items to learn a type from, so it needs a type to be expected, and a list or an array type both work.

```
let a = [1, 2, 3];              // [i64; 3]
let b: [i64] = [1, 2, 3];       // [i64]
let c: [i64; 3] = [1, 2, 3];    // [i64; 3]
let d: [i64] = [];              // an empty list
let f = [];                     // error: no item type
let e: [i64; 2] = [1, 2, 3];    // error: 3 items
```

An index that is a number literal past the end of an array is an error when the program is compiled. Any other bad index is an error when the program runs.

Two array types are the same type when both the item type and the length match. `[i64; 3]` is not `[i64; 4]`, and neither is `[i64]`. A list is never accepted where an array is expected, and an array is never accepted where a list is expected.

### Structs

Structs are referred to as a type by name. Fields are declared with `var`.

```
struct Box {
    pub var value: f64;
}

fun main(): unit {
    let box: Box = Box { value: 100 };

    @println(box.value);
}
```

#### Generic Structs

A struct can declare type parameters in brackets. They are written as
arguments when the type is used. `Box[f64]` and `Box[string]` are different
types, and the number of arguments has to match.

```
struct Box[T] {
    pub var value: T;
}

let a: Box[f64] = Box { value: 1 };
let b = Box { value: "hi" };     // Box[string]
let c: Box[string] = a;          // error: Expected Box[string], got Box[f64].
```

A literal works out its arguments from its fields. When a type is asked for,
the fields are checked against it, so the `1` above can be an `f64`. A
literal that can't work out an argument is an error.

An `impl` declares the same number of parameters, and they stand for the
struct's parameters in order. A method can declare more, which are numbered
after the impl's.

```
impl Box[T] {
    pub fun new(value: T): Self[T] = Self { value: value };
    pub fun get(self): T = self.value;
    pub fun map[U](self, f: fun (T) => U): Self[U] = Self.new(f(self.value));
}

fun main(): unit {
    let box = Box.new(5);            // Box[i64]
    let text = box.map(fun (n: i64): string { "n" });   // Box[string]
}
```

In a type, `Self[T]` is the struct with arguments and a plain `Self` is
`Box[T]`. In an expression, `Self` and `Box` are the struct itself, and a
call or literal works out the arguments.

`impl Trait for` a generic struct isn't supported yet.

### Generic Functions

A function can declare type parameters after its name. A call fills each one
in from its arguments, the first argument that mentions it fills it, and every
later argument has to agree.

```
fun id[T](x: T): T = x;
fun first[T](xs: [T]): T = xs[0];
fun apply[T, U](x: T, f: fun (T) => U): U = f(x);

fun main(): unit {
    let a = id(5);                  // i64
    let b: string = id(5);          // error: Expected string, got i64.
}
```

A parameter that appears in the signature but that no argument fills in is an
error: `Can't tell what 'T' is from the arguments.`. A parameter that isn't in
the signature at all is never asked for.

A parameter only the return type mentions is filled in from the type the call
is expected to return, when there is one:

```
fun empty[T](): [T] = [];

fun main(): unit {
    let a: [i64] = empty();    // T is i64
    let b = empty();           // error: Can't tell what 'T' is from the arguments.
}
```

A method works the same way, so `let s: Stack[i64] = Stack.new();` fills in
the `T` of `fun new(): Self[T]`.

Inside the function, each parameter is one type that is only equal to itself.
A value of that type can be passed around, returned and stored, but it can't
be added, indexed or have a method called on it, because nothing says its type
supports that. A [trait bound](#trait-bounds) is the exception: it allows the
methods of its trait. The error is reported in the function, not at the call.

```
fun add[T](a: T, b: T): T = a + b;   // error: '+' needs two numbers ... got T and T.
fun bad[A, B](a: A): B = a;          // error: Expected B, got A.
```

A generic function can also be used as a value where a function type is
expected. The expected type fills in the function's type parameters, and the
result has to match. Here `id` is the function declared above, and its `T`
becomes `i64`.

```
fun id[T](x: T): T = x;

let f: fun (i64) => i64 = id;       // ok
let g: fun (i64) => string = id;    // error: Expected fun (i64) => string, got fun (i64) => i64.
```

A generic method works the same way, such as `Box.new` for a method declared
`fun new(value: T): Self[T]`.

With no expected type, as in `let h = id;`, the value has no type and isn't
checked.

A generic function is compiled once, and the same code runs for every type.

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

fun main(): unit {
    @println((Point { x: 1, y: 2 }).toString());
}
```

#### Builtin traits

The builtin, always in scope, traits are `Display`, `Eq`, `Ord`, `Default`, and the operator traits `Add`, `Sub`, `Mul`, `Div`, `Mod` and `Neg`.

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

fun main(): unit {
    let a = Box { value: 100 };
    let b = Box { value: 100 };

    a.equals(b); // true
}
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
    fun toString(self): string = @numberToString(self.value);
}

fun main(): unit {
    let box = Box { value: 123 };

    box.toString(); // "123"
}
```

##### Ord

```
trait Ord: Eq {
    // Negative when self comes first, zero when equal, positive when other
    // comes first
    fun cmp(self, other: Self): i64;
}

trait Eq {
    fun equals(self, other: Self): bool;
}

struct Box {
    pub var value: i64;
}

impl Eq for Box {
    fun equals(self, other: Self): bool = self.value == other.value;
}

impl Ord for Box {
    fun cmp(self, other: Self): i64 = self.value - other.value;
}

fun main(): unit {
    let a = Box { value: 100 };
    let b = Box { value: 123 };

    a.cmp(b); // -23
}
```

##### Operator traits

An operator on structs calls a method of a trait. `a + b` calls `a.add(b)`, so
a struct supports `+` by implementing `Add`.

| Operator              | Trait  | Method                          |
| --------------------- | ------ | ------------------------------- |
| `+`                   | `Add`  | `fun add(self, other: Self): Self` |
| `-`                   | `Sub`  | `fun sub(self, other: Self): Self` |
| `*`                   | `Mul`  | `fun mul(self, other: Self): Self` |
| `/`                   | `Div`  | `fun div(self, other: Self): Self` |
| `%`                   | `Mod`  | `fun mod(self, other: Self): Self` |
| `-` (negate)          | `Neg`  | `fun neg(self): Self`           |
| `==` `!=`             | `Eq`   | `fun equals(self, other: Self): bool` |
| `<` `>` `<=` `>=`     | `Ord`  | `fun cmp(self, other: Self): i64` |

`a != b` is `!a.equals(b)`. `a < b` is `a.cmp(b) < 0`, and `<=`, `>` and `>=`
compare the result of `cmp` with 0 the same way.

```
struct Vec2 {
    pub var x: f64;
    pub var y: f64;
}

impl Add for Vec2 {
    fun add(self, other: Self): Self =
        Vec2 { x: self.x + other.x, y: self.y + other.y };
}

fun main(): unit {
    let sum = Vec2 { x: 1, y: 2 } + Vec2 { x: 3, y: 4 };   // Vec2 { x: 4, y: 6 }
}
```

Both sides have to be the same struct. A struct without the trait is an error:
`Money doesn't implement Add, required by '+'.`

Numbers have these traits built in, so a number can fill a parameter bounded by
one of them (see [Trait Bounds](#trait-bounds)), and `neg` is only for signed
numbers. Number operators that don't go through a bound still compile to the
built-in instructions, so plain number arithmetic costs nothing extra.

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

fun main(): unit {
    var p = Point.origin().translate(3, 4);
}
```

#### Supertraits

`trait Sub: Super` declares that an `impl Super for X` must exist for any
`impl Sub for X`.

```
trait Ord: Eq {
    fun cmp(self, other: Self): i64;
}
```

#### Limitations

- Some native functions have no signature yet, so calls to them aren't
  checked
- A value of a generic type can't be indexed, and can only be added, compared
  or called through a bound -- see [Trait Bounds](#trait-bounds)
- A call can't be given type arguments (`parse[f64](s)`)
- Lists have no type annotation syntax
- `impl Trait for` a primitive type isn't supported yet -- see
  [Traits](#traits)
