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
let numbers: [i64] = [1, 2, 3];
let names: [string] = [];

@println(numbers[0]);
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

let box: Box = Box { value: 100 };

@println(box.value);
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

let box = Box.new(5);            // Box[i64]
let text = box.map(fun (n: i64): string { "n" });   // Box[string]
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

let a = id(5);                  // i64
let b: string = id(5);          // error: Expected string, got i64.
```

A parameter that appears in the signature but that no argument fills in is an
error: `Can't tell what 'T' is from the arguments.`. A parameter that isn't in
the signature at all is never asked for.

A parameter only the return type mentions is filled in from the type the call
is expected to return, when there is one:

```
fun empty[T](): [T] = [];

let a: [i64] = empty();    // T is i64
let b = empty();           // error: Can't tell what 'T' is from the arguments.
```

A method works the same way, so `let s: Stack[i64] = Stack.new();` fills in
the `T` of `fun new(): Self[T]`.

Inside the function, each parameter is one type that is only equal to itself.
A value of that type can be passed around, returned and stored, but it can't
be added, indexed or have a method called on it, because nothing says its type
supports that. The error is reported in the function, not at the call.

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

@println((Point { x: 1, y: 2 }).toString());
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
    fun toString(self): string = @numberToString(self.value);
}

let box = Box { value: 123 };

box.toString(); // "123"
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
    fun cmp(self, other: Self): i64;
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

@println(sum2(1, 2));
```

### Lambdas

Lambdas use the same type syntax.

```
let sum: fun (f64, f64) => f64 = fun (a: f64, b: f64): f64 { a + b };

@println(sum(1, 2));
```

A lambda's parameter types can be left off when the type it is checked
against already supplies them.

```
let double: fun (f64) => f64 = fun (n) { n * 2 };

@println(double(21));
```

### Native Functions

Native functions start with `@` (a reserved character in user code).

```
let round: fun (f64) => f64 = @ceil;

@println(round(1.2));
```

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

Array natives, such as `@arrLen` and `@arrCopy`, also name the array's length
in their signatures with the placeholders `N` and `M`. `@arrCopy` is
`fun ([T; N]) => [T; N]`, so copying a `[i64; 3]` gives a `[i64; 3]`.
`@arrEqual` is `fun ([T; N], [T; N]) => bool`, so comparing arrays of different
lengths is an error. `@arrSlice` and `@arrConcat` give a list, because the
length of the result isn't known until the program runs. Only native signatures
can use `N` and `M`.

A native whose type can't be spelled yet has no signature, and calls to it
are checked at runtime instead. That covers `@typeof`, `@instanceOf`, the
`@is*` family and the print natives, which take a value of any type, and
`@argv`, `@prompt` and `@stdin`, which return nothing on some paths and so
need `Option[T]`.

## Type Aliases

```
type Number = f64;

let count: Number = 42;

@println(count);
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

| Operator          | Operands               | Result    |
| ----------------- | ---------------------- | --------- |
| `+`               | two of one number type | that type |
| `+`               | two `string`           | `string`  |
| `-` `*` `/` `%`   | two of one number type | that type |
| `<` `>` `<=` `>=` | two of one number type | `bool`    |
| `==` `!=`         | two of one type        | `bool`    |
| `and` `or`        | two `bool`             | `bool`    |
| `!`               | any                    | `bool`    |
| `-` (negate)      | a signed number        | that type |

`==`/`!=` between two structs additionally requires the struct to
implement `Eq` -- see [Traits](#traits).

## Limitations

- Some native functions have no signature yet, so calls to them aren't
  checked
- Generic parameters have no trait bounds yet, so a value of a generic type
  can't be added, indexed or have methods called on it
- A call can't be given type arguments (`parse[f64](s)`)
- Lists have no type annotation syntax
- `impl Trait for` a primitive type isn't supported yet, and operators
  don't dispatch to trait methods -- see [Traits](#traits)
