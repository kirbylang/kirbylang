# Basics

## Programs

A file holds declarations: `fun`, `struct`, `impl`, `trait`, `type`, and `let` /
`var` bindings. Loading a file runs none of your code. To run a file, it needs a
`main` function. Kirby calls it after the file has loaded.

```krb
fun main(): unit {
    @println("Hello, World!");
}
```

A top-level `let` or `var` must be given a _comptime value_: a value fixed by the
source text alone. These are literals, operators over comptime values, array and
struct literals of comptime values, lambdas, the name of a top-level `fun`, and
the name of an earlier comptime `let`. A call, field access, indexing, or the name
of a `var` is not a comptime value.

```krb
let secondsPerDay = 24 * 60 * 60;
var count = 0;

var start = @clock(); // Error: A top-level 'var' can only be assigned a comptime value.

fun main(): unit {
    count = count + 1;
}
```

Calls and other statements at the top level are an error. Put them in a function.

```krb
@println("loaded"); // Error: Only declarations can appear at the top level of a file.
```

The REPL and `krb exec` take snippets, which can hold statements and need no
`main`.

The examples on the rest of this page are snippets. To run one from a file, put its
statements inside `main`.

## Comments

A comment starts with `//` and runs to the end of the line. There are no block
comments.

```krb
// A comment on its own line
fun main(): unit {} // A comment after code
```

## Variables

A variable is either a mutable binding, using the `var` keyword, or an immutable binding, using the `let` keyword.

```krb
let limit = 10;
var count = 0;

while (count <= limit) {
    @println($"Count: {count}");

    count = count + 1;
}

limit = 100; // Error at 'limit': Cannot assign to immutable binding
```

A `let` needs a value when it's declared. A `var` can be given its value later,
and must have one on every path before it's read.

```krb
fun main(): unit {
    var size: string;

    if (@argc() > 1) {
        size = "has arguments";
    } else {
        size = "no arguments";
    }

    @println(size);
}
```

A `var` can be declared again, in the same block or an inner one, and the new one
hides it. A `let` inside a function can't be declared again in its block or in the
blocks inside it. At the top level a `let` can't be declared twice, but a function
can use its name for a variable of its own.

```krb
let limit = 10;

fun main(): unit {
    var size = 1;
    var size = "small"; // a new variable

    let limit = 20; // hides the top-level `limit`

    @println($"{size} {limit}"); // small 20
}
```

### Annotating types

Type annotations on variable declarations are optional when the value says what
the type is. They are needed when a `var` has no value yet, and for an empty list
or array literal.

```krb
let greeting: string = "Hello World";
var total: i64;
let names: [string] = [];
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

### Unit

Kirby has a unit type `unit` using the literal `()`, which only has a single value that represents a function that doesn't return a value. Printing it writes `nil`.

### Booleans

#### Truthiness

Kirby doesn't support coercion to booleans. A condition has to be a `bool`.

```kirby
fun main(): unit {
    let values: [bool] = [];

    @println(if (values) $"Has {@listLen(values)} items" else "List is empty!");
}
```

This is invalid.

> Error: Expected bool, got [bool].

### nil

`nil` is a holdover from Kirby's origins from the language [clox](./getting-started.md#special-thanks). It will be replaced by `Result[T, E]` when that's implemented.

### Strings

Strings are written using double quotes.

```kirby
fun main(): unit {
    @println("Hello World");
}
```

#### String Methods

`len` gives the length of a string in bytes. Other work on strings is done by the
`@str` [builtin functions](./reference/builtin-functions.md#strings).

```kirby
fun main(): unit {
    @println("Hello".len()); // 5
    @println("Hello " + "World"); // Hello World
    @println(@strToUpper("Hello")); // HELLO
}
```

A string can't be indexed, and strings can't be ordered with `<`.

The escapes are `\n`, `\r`, `\t`, `\"` and `\\`.

#### Multiline Strings

Multiline strings are supported.

```kirby
fun main(): unit {
    @println("Hello

    World");
}
```

#### String Interpolation

Interpolated strings are prefixed with `$` and use `{expr}` for interpolated values.

```kirby
fun main(): unit {
    let name: string = @prompt("Name? ");

    @println($"Hello {name}");
}
```

A placeholder can hold a `string`, a number, a `bool`, or a value whose type
implements [`Display`](#traits). Anything else, such as a list or `()`, is an
error. Write `{{` and `}}` for a literal brace.

### Numbers

Numbers are either signed/unsigned integers (`i64`, `u32`, `u8`) or floats (`f64`, `f32`).

#### Casting Number Types

Number types can be explicitly converted to another number type with the `as` keyword.

```krb
fun main(): unit {
    let value: f64 = 42.0;
    let byte: u8 = value as u8;

    let widened: f64 = byte as f64;
    let int: i32 = value as i32;
}
```

| From          | To         | Result                                                             |
| ------------- | ---------- | ------------------------------------------------------------------ |
| a float       | an integer | Must be a whole number in the integer's range. Checked at runtime  |
| an integer    | an integer | Must fit in the target. Checked at runtime                         |
| an integer    | a float    | Rounded to the nearest float. Exact when it fits the float         |
| `f64`         | `f32`      | Rounded to the nearest `f32`. A value out of its range is an error |
| `f32`         | `f64`      | Unchanged                                                          |
| the same type | itself     | Unchanged                                                          |

```krb
fun main(): unit {
    let too_big: f64 = 300.0;
    let byte: u8 = too_big as u8; // Error: can't convert f64 300 to u8
}
```

Use parentheses to cast just a part of an expression.

```krb
fun main(): unit {
    let a: u8 = 1;
    let b: i64 = 2;

    let sum: u8 = 2 * 3 as u8;      // (2 * 3) as u8
    let total: u8 = a + (b as u8);  // cast only `b`
}
```

#### Number Literals

Number literals are digits + an optional decimal point and suffix indicating the number's kind.

- `123`
- `128u8`
- `146.48290f32`
- `105725.278123567f64`

A suffix can be written in upper case, such as `255U8`.

A whole number literal is an `i64`, and a decimal literal is an `f64`. A whole
number literal without a suffix takes the type that is expected where it's used,
when the value fits.

```krb
fun main(): unit {
    let byte: u8 = 200 + 55;   // u8
    let half: f64 = 7 / 2;     // 3.5
    let whole = 7 / 2;         // i64, which is 3
    let mixed = [1, 2.5];      // [f64; 2]
}
```

Both sides of an operator have to be the same number type. Cast one with `as` to
mix them. Integer math that doesn't fit its type, and dividing any number by
zero, stop the program with an error.

#### Number Methods

Numbers have methods.

| Type                      | Methods                                                               |
| ------------------------- | --------------------------------------------------------------------- |
| `f64`, `f32`              | `abs`, `ceil`, `floor`, `max`, `min`, `pow`, `round`, `sqrt`, `trunc` |
| `i8`, `i16`, `i32`, `i64` | `abs`, `max`, `min`                                                   |
| `u8`, `u16`, `u32`, `u64` | `max`, `min`                                                          |

The float methods do what the builtin functions of the same name do.

```kirby
fun main(): unit {
    let x: f64 = -2.5;
    @println(x.abs()); // 2.5
    @println(x.round()); // -3
    @println((16.0).sqrt()); // 4

    let n: i32 = -7;
    @println(n.abs()); // 7
    @println(n.max(3)); // 3

    let byte: u8 = 200;
    @println(byte.min(100)); // 100
}
```

The `abs` of the smallest integer, such as `-128` for an `i8`, is an overflow error, because the result doesn't fit.

Numbers, strings, bools and `()` also have the methods of the standard traits, which can be called like any method:

| Type                          | Methods                                                        |
| ----------------------------- | -------------------------------------------------------------- |
| every number type             | `toString`, `equals`, `cmp`, `add`, `sub`, `mul`, `div`, `mod` |
| signed integers, `f64`, `f32` | `neg`                                                          |
| `string`                      | `toString`, `equals`, `add`                                    |
| `bool`                        | `toString`, `equals`                                           |
| `unit`                        | `toString`, `equals`, `cmp`                                    |

`cmp` gives `-1`, `0` or `1`.

```kirby
fun main(): unit {
    let a: u8 = 7;
    @println(a.toString()); // 7
    @println(a.cmp(3)); // 1
    @println(a.add(3)); // 10
    @println("ab".add("cd")); // abcd
}
```

The `Default` trait's static method is called on the type's name.

```kirby
fun main(): unit {
    @println(f64.default()); // 0
    @println(bool.default()); // false
}
```

These methods come from the standard library. You can't add your own to a
primitive type: `impl i64 { ... }` is an error.

### Arrays

Arrays, `[T; n]`, are fixed size collections of homogeneous types. The length is
part of the type, so `[i64; 3]` and `[i64; 4]` are different types.

```kirby
fun main(): unit {
    let grid: [[bool; 3]; 3] = [[true, false, false], [true, true, true], [false, true, false]];

    @println(grid[0][1]); // false
}
```

A literal such as `[1, 2, 3]` makes an array, unless a [list](#lists) type is
expected. An empty literal `[]` is only allowed where a list or array type is
expected. An array's items can be changed, but its length can't.

```krb
fun main(): unit {
    let a = [1, 2, 3];           // [i64; 3]
    let b: [i64] = [1, 2, 3];    // [i64]

    a[0] = 9;
    @println(a); // [9, 2, 3]
}
```

An index written as a number that is past the end, such as `a[3]`, is reported
before the program runs. Any other bad index stops the program with an error.

#### Array Methods

An array has `len`, `contains`, `copy`, `reverse`, `slice`, `concat` and
`toList`. `copy` and `reverse` give an array of the same length, and `slice`,
`concat` and `toList` give a list.

```kirby
fun main(): unit {
    let scores = [100, 53, 77];

    @println(scores.len()); // 3
    @println(scores.contains(53)); // true
    @println(scores.reverse()[0]); // 77
}
```

#### Array Lengths

The standard library's array methods and builtins work for an array of any length.
They declare the length as a parameter with `const`, in the same way `T` stands
for any item type:

```
impl[T, const N] [T; N] {
    pub fun copy(self): [T; N] = @arrCopy(self);
    pub fun concat[const M](self, other: [T; M]): [T] = @arrConcat(self, other);
}
```

So `copy` of a `[i64; 3]` is a `[i64; 3]`, and `concat` accepts an array of any
other length and gives a list.

Only the standard library can declare `const` parameters, and `const` is a
reserved word. A function of your own names one length, such as `[i64; 3]`. To
accept any number of items, take a list and pass `a.toList()`.

```krb
fun total(numbers: [i64]): i64 {
    var sum = 0;

    for (var i = 0; i < numbers.len(); i = i + 1) {
        sum = sum + numbers[i];
    }

    return sum;
}

fun main(): unit {
    let three = [1, 2, 3];

    @println(total(three.toList())); // 6
}
```

### Lists

Lists, `[T]`, are growable collections of homogeneous types.

```krb
fun main(): unit {
    let scores: [i64] = [100, 53, 77, 41];

    @listPush(scores, 87);
}
```

`let` stops the name from being given a new value. The list it holds can still be
changed.

#### List Methods

A list has `len`, `isEmpty`, `push`, `pop`, `insert`, `remove`, `clear`,
`contains`, `copy`, `reverse`, `slice` and `concat`.

```kirby
fun main(): unit {
    let scores: [i64] = [100, 53, 77, 41];

    scores.push(87);
    @println(scores.len()); // 5
    @println(scores.pop()); // 87
    @println(scores.contains(77)); // true
}
```

`==` on two lists, or two arrays, is only `true` when both sides are the same
object. To compare the items use `@listEqual` or `@arrEqual`.

## Operators

| Operators               | Work on                                                             |
| ----------------------- | ------------------------------------------------------------------- |
| `+` `-` `*` `/` `%`     | two numbers of one type, or a struct with the [trait](#operators-1) |
| `+`                     | two strings                                                         |
| `-` (negate)            | a signed number, or a struct with `Neg`                             |
| `<` `>` `<=` `>=`       | two numbers of one type, or a struct with `Ord`                     |
| `==` `!=`               | two values of one type. A struct needs `Eq`                         |
| `and` `&&`, `or` `\|\|` | two `bool`s                                                         |
| `!`                     | any value. `!false` and `!nil` are `true`                           |
| `??`                    | gives the right side when the left side is `nil`                    |
| `as`                    | [casts a number](#casting-number-types)                             |

Dividing two integers drops the remainder.

## Blocks

A block can be used as a value. It is the value of its last expression, written
without a semicolon.

```krb
fun main(): unit {
    let area = {
        let side = 4;
        side * side
    };

    @println(area); // 16
}
```

## Flow Control

### `if`

```krb
fun main(): unit {
    let value = 123;
    var even_or_odd = "";

    if (value % 2 == 0) {
        even_or_odd = "Even";
    } else {
        even_or_odd = "Odd";
    }
}
```

#### `if` expressions

When in an expression position, `if` is treated as an expression. All branches must return the same type.

```krb
fun main(): unit {
    let value = 123;
    let even_or_odd = if (value % 2 == 0) "Even" else "Odd";
}
```

### `while`

```krb
fun main(): unit {
    var count = 0;

    while (count <= 10) {
        count = count + 1;

        @println($"Count: {count}");
    }
}
```

### `for`

```krb
fun main(): unit {
    for (var i = 0; i < 10; i = i + 1) {
        @println($"Count: {i}");
    }
}
```

### `break`/`continue`

Available inside of `for` and `while` loops. `break` leaves the loop, and
`continue` starts its next turn.

```krb
fun main(): unit {
    for (var i = 0; i < 10; i = i + 1) {
        if (i == 1) {
            continue;
        }

        if (i == 3) {
            break;
        }

        @println(i); // 0, then 2
    }
}
```

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
};
```

The type of a function is written `fun (f64, f64) => f64`. When that type is
already known, a lambda can leave off its parameter and return types.

```krb
fun main(): unit {
    let double: fun (i64) => i64 = fun (n) { n * 2 };

    @println(double(21)); // 42
}
```

A lambda can read and change the variables around it.

```krb
fun main(): unit {
    var count = 0;
    let increment = fun () { count = count + 1; };

    increment();
    increment();

    @println(count); // 2
}
```

### Implicit Returns

Functions and lambdas can implicitly return if the last line of the body is a bare expression (no semicolon).

```krb
fun sum(a: f64, b: f64): f64 {
    a + b
}
```

```krb
let sum = fun (a: f64, b: f64): f64 {
    a + b
};
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

fun main(): unit {
    @println(first([10, 20, 30]));   // 10
    @println(first(["a", "b"]));     // a
}
```

Inside the function, nothing is known about `T`, so a `T` can be passed around, returned and stored, but not added, compared or called. Every argument that uses the same `T` has to be the same type. A [trait bound](#trait-bounds) lets the function call a trait's methods on a `T`.

A type parameter hides any type of the same name inside its declaration, so in
`fun id[string](x: string): string` the function takes any type. `Self` can't be
the name of a type parameter.

### Builtin Functions

Kirby has a number of [builtin functions](./reference/builtin-functions.md).
These are prefixed with `@`.

```krb
fun main(): unit {
    @assert(1 + 1 == 2, "math is broken");

    @println(@parseInt("42") + 1); // 43

    @panic("stop here"); // panic: stop here
}
```

There are no exceptions to catch. `@assert`, `@panic` and other runtime errors
stop the program.

## Custom Types

### Structs

Structs are custom data types that group together a set of [variables](#variables) (fields) and [functions](#functions) (methods) that operate on those fields.

#### Fields

Fields are defined in the struct's declaration body.

```krb
struct Counter {
    // Immutable
    initial: f64;
    let limit: f64;

    // Mutable
    var count: f64;
}
```

Fields are private and immutable by default.

- Use the `var` keyword to make fields mutable. This only controls whether the field can be reassigned, not whether its value can change.
- Use the `pub` keyword to make fields public.

Every field needs a type. Fields can't have default values.

#### Creating A Struct

A struct literal names the struct and gives every field a value. Code outside the
struct's `impl` can only set and read its public fields.

```krb
struct Size {
    pub width: i64;
    pub var height: i64;
}

fun main(): unit {
    let size = Size { width: 2, height: 3 };

    size.height = 4;

    @println(size.width * size.height); // 8
}
```

A private field is set by a static method of the struct.

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
            @println($"Count: {self.count}");

            self.increment();
        }
    }
}
```

Methods are private by default. Use the `pub` keyword to make methods public.

A method whose first parameter is `self` is called on a value. A method without
`self` is a static method, called on the struct's name. Inside an `impl`, `Self`
is the struct.

```krb
struct Account {
    var balance: i64;
}

impl Account {
    pub fun open(): Self = Self { balance: 0 };

    pub fun deposit(self, amount: i64): unit {
        self.balance = self.balance + amount;
    }

    pub fun total(self): i64 = self.balance;
}

fun main(): unit {
    let account = Account.open();

    account.deposit(5);

    @println(account.total()); // 5
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

fun main(): unit {
    let number = Box.new(5);           // Box[i64]
    let text: Box[string] = Box { value: "hi" };

    @println(number.get());            // 5
}
```

`Box[i64]` and `Box[string]` are different types. The `impl` repeats the struct's types, and a method can add more, like `U` in `map`.

## Type Aliases

`type` gives a type another name. The two names are the same type.

```krb
type Meters = f64;
type Callback[T] = fun (T) => T;

struct Box[T] {
    pub var value: T;
}

type Number = Box[i64];

fun main(): unit {
    let distance: Meters = 5;
    let same: Callback[i64] = fun (n) { n };

    let box = Number { value: 1 };        // an alias of a struct works in a literal

    @println(same(box.value));            // 1
    @println(Meters.default() + distance); // 5
}
```

An alias of a struct works wherever the struct's name does: in a literal and to
call a static method. An alias of a primitive type can call its static methods.
Type arguments that an alias fixes stay fixed, so `Number { value: "hi" }` is an
error.

## Traits

Traits are a way to define an interface for shared behavior.

```krb
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

impl Area for Rect {
    fun get_area(self): f64 =
        self.width * self.height;
}

impl Area for Square {
    fun get_area(self): f64 =
        self.size * self.size;
}

fun main(): unit {
    let square = Square { size: 10 };
    let rect = Rect { width: 5, height: 10 };

    @println($"Square: {square.get_area()}");  // Square: 100
    @println($"Rect: {rect.get_area()}");  // Rect: 50
}
```

#### Calling A Trait's Method By Name

A type's own method wins over a trait's method with the same name.
`Trait.method(value)` calls the trait's version.

```kirby
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

    @println(p.toString()); // own
    @println(Display.toString(p)); // trait
}
```

### Builtin Traits

These traits are always in scope:

| Trait     | Method                                | Used by               |
| --------- | ------------------------------------- | --------------------- |
| `Display` | `fun toString(self): string`          | printing and `$"{x}"` |
| `Eq`      | `fun equals(self, other: Self): bool` | `==` `!=`             |
| `Ord`     | `fun cmp(self, other: Self): i64`     | `<` `>` `<=` `>=`     |
| `Default` | `fun default(): Self`                 | `Type.default()`      |
| `Add`     | `fun add(self, other: Self): Self`    | `+`                   |
| `Sub`     | `fun sub(self, other: Self): Self`    | `-`                   |
| `Mul`     | `fun mul(self, other: Self): Self`    | `*`                   |
| `Div`     | `fun div(self, other: Self): Self`    | `/`                   |
| `Mod`     | `fun mod(self, other: Self): Self`    | `%`                   |
| `Neg`     | `fun neg(self): Self`                 | `-` (negate)          |

```krb
struct Point {
    pub x: i64;
}

impl Display for Point {
    fun toString(self): string = $"Point({self.x})";
}

fun main(): unit {
    @println(Point { x: 1 }); // Point(1)
}
```

A generic struct can't implement a trait yet.

### Super Traits

Super traits are a way to declare that if you implement one trait, you must implement another.

```krb
trait Super {
    fun super(self): string;
}

trait Regular: Super {
    fun regular(self): string;
}

struct Value {}

impl Super for Value {
    fun super(self): string = "Super!";
}

impl Regular for Value {
    fun regular(self): string = "Regular!";
}

fun test[T: Regular](value: T): unit {
    @eprintln(value.super());
    @eprintln(value.regular());
}

fun main(): unit {
    let value = Value {};

    test(value);
}
```

`Ord` has a super trait of `Eq`.

```krb
struct Money {
  pub var cents: i64;
}

impl Eq for Money {
  fun equals(self, other: Self): bool = self.cents == other.cents;
}

impl Ord for Money {
  fun cmp(self, other: Self): i64 = self.cents - other.cents;
}

fun largest[T: Ord](a: T, b: T): T {
  if (a.cmp(b) >= 0) {
    return a;
  }

  return b;
}

fun same[T: Ord](a: T, b: T): bool = a.equals(b);

fun main(): unit {
  let m = largest(Money { cents: 5 }, Money { cents: 9 });
  @println(m.cents);
  @println(same(Money { cents: 1 }, Money { cents: 1 }));
  @println(same(Money { cents: 1 }, Money { cents: 2 }));
}
```

### Trait Bounds

A [generic function](#generic-functions) can say that its type has to implement a trait. Write the trait after a colon. The function can then call the trait's methods on that type.

```
// `Area` and `Rect` are declared above, under Traits
fun double_area[T: Area](shape: T): f64 = shape.get_area() * 2;

fun main(): unit {
    @println(double_area(Rect { width: 5, height: 10 }));  // 100
}
```

A call with a type that doesn't implement the trait is an error: `Rock doesn't implement Area, required by T.`

Use `+` to ask for more than one trait.

```
fun describe[T: Display + Eq](a: T, b: T): string {
    if (a.equals(b)) {
        return $"{a} is the same as {b}";
    }

    return $"{a} differs from {b}";
}
```

Structs and type aliases can have bounds too. An `impl` takes the bounds of its struct, so you don't write them again.

```
struct Labeled[T: Display] {
    pub var value: T;
}

impl Labeled[T] {
    pub fun label(self): string = $"<{self.value}>";
}
```

#### Operators

An operator on a struct calls a method of a trait: `a + b` calls `a.add(b)` from the `Add` trait. The traits are `Add`, `Sub`, `Mul`, `Div`, `Mod` (`+ - * / %`), `Neg` (unary `-`), `Eq` (`==` and `!=`) and `Ord` (`<`, `>`, `<=` and `>=`). A bound lets generic code use them, and numbers can be used with any of these bounds.

```
struct Vec2 {
    pub var x: f64;
    pub var y: f64;
}

impl Add for Vec2 {
    fun add(self, other: Self): Self =
        Vec2 { x: self.x + other.x, y: self.y + other.y };
}

fun sum[T: Add](a: T, b: T): T = a + b;

fun main(): unit {
    @println(sum(1, 2));                                   // 3
    let v = sum(Vec2 { x: 1, y: 2 }, Vec2 { x: 3, y: 4 }); // Vec2 { x: 4, y: 6 }
}
```

`==` on two structs calls their `equals`, so it follows the struct's `Eq` impl.

Calling a static trait method through a bound, like `T.default()`, isn't supported yet.

A primitive type implements its traits, but can't fill a bounded parameter yet:
passing `"hi"` where `T: Display` is expected is an error. The exception is a
number with an operator trait bound (`Add`, `Eq`, `Ord` and the others above).
