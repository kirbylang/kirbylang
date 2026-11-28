# Change Log

## Future State

- [Array comprehensions](https://github.com/kirbylang/kirbylang/issues/18)
- [Modules](https://github.com/kirbylang/kirbylang/issues/14)
- Documentation Comments
- Tooling
  - Version Manager `krb version use 1.2.3`
  - Linter `krb lint`
  - Formatter `krb format`
  - [Test Framework/Runner](https://github.com/kirbylang/kirbylang/issues/34) /w Code Coverage `krb test`
- Debugger (compatible with VS Code)
- Tuples
- [Type System](https://github.com/kirbylang/kirbylang/issues/17)
- Lambda body expressions `var sum = fun (a, b) a + b;`
- Native Functions
  - [ ] `arrSort(array, fn)`
  - [ ] `arrMap(array, fn)`
- Macros

## Next

- Allow a method of a built-in type to be used as a value, as in `let length = "abc".len;`
- Fix a method used as a value in a generic function. On a struct it could run the type's own method instead of the trait's, and on a number it was a runtime error
- Allow a program to implement its own traits on `string`, `bool`, `unit` and the number types, as in `impl Describe for i64`. Only the standard library can implement the builtin traits for them
- Allow two traits to give one type instance methods of the same name. A call such as `value.label()` is then ambiguous and is an error, so write `Named.label(value)`. It was an error to implement the second trait
- **Breaking**: `cmp` on floats sorts `NaN` below every number and level with `NaN`. It returned `0`, which means equal. In a generic function, where `<` and the other ordering operators use `cmp`, `NaN < 1.0` is now `true`
- Change where a source build puts `krb`: `build/debug` for `just build` and `build/release` for `just build-release`, which is now optimized. `just install` makes a release build first
- Change a bad array length, such as `[i64; n]`, to report one error, `Array length must be a whole number, as in '[i64; 3]'.`, in place of a syntax error that was often followed by a second one
- Change type mismatch errors to say "type parameter" before a type that is one. `Expected string, got string.` is now `Expected type parameter string, got string.`
- **Breaking**: a struct can't be named like a built-in type: `unit`, `bool`, `string` or a number type such as `i64`. It was accepted, but the name in a type annotation still meant the built-in type
- Fix a syntax error hiding the errors in a `type` declaration that follows it
- Change `@listSlice`, `@arrSlice` and the `slice` method to return an empty list when `start` equals `end`, as `@strSlice` does. It was an error
- Fix `<=` and `>=` being `true` when a float is `NaN`. Every direct comparison with `NaN` is now `false`. Generic code ordering values through `Ord` is unchanged. New opcodes `OP_LESS_EQUAL` (45) and `OP_GREATER_EQUAL` (46)
- Fix a number literal not taking the number type of an argument, as it does in `5 == x`. With a `u8` named `x`, `Eq.equals(5, x)`, `5.equals(x)` and `5.max(x)` were type errors
- `krb --help` lists the `docs` command, and no longer shows `krb docs 'search terms'`, which `krb docs` ignored
- **Breaking**: a type parameter now hides any type of the same name inside its declaration. A struct or alias was already hidden, and now a type such as `string` or `i64` is too, as in `fun id[string](x: string): string`. A parameter given type arguments, such as `Box[i64]` where `Box` is a parameter, is an error even when a struct or alias has that name
- **Breaking**: `Self` can't be the name of a type parameter. It was accepted and ignored, and is reported as `'Self' can't be the name of a type parameter.`
- **Breaking**: `const` is a reserved word. In the standard library it declares an array length as a parameter, as in `native fun @arrConcat[T, const N, const M](a: [T; N], b: [T; M]): [T];`, in place of the fixed names `N` and `M`
- **Breaking**: a list or array impl declares its parameters after `impl`, as in `impl[T] [T]` and `impl[T, const N] [T; N]`. Only the standard library writes these
- **Breaking**: a method can't declare a type parameter with the same name as one of its impl's, such as `fun with[T]` in `impl Box[T]`. It is reported as `Generic parameter 'T' is already declared.`
- Fix a standard library method being able to return one array length as another, such as a `[T; M]` as a `[T; N]`
- Fix a false type error in an array method with four or more type parameters of its own
- A type alias of a struct, including a generic alias, can be used in a struct literal and to call a static method
  - `type Alias = Struct;`: `Alias { n: 1 }` and `Alias.make()`.
  - An alias of a primitive type can call its static methods, such as `type Meters = i64;`: `Meters.default()`
- **Breaking**: a struct literal can't name a type parameter, such as `T { n: 1 }` inside `fun f[T]`, even when a struct is also called `T`
- VS Code extension: highlight `const` and trait names in list and array impls, and update the impl snippets and array native hovers
- VS Code extension: highlight `impl` blocks for primitives, lists and arrays, `native` declarations, built-in type names, every number suffix and method calls on literals, and add snippets for them, a `native` hover and an `@strToBytes` snippet
- **Breaking**: `native` is a reserved word. The standard library declares the types of natives with it, such as `native fun @listPop[T](list: [T]): T;`
- Fix a false `Can't tell what 'T' is from the arguments.` error when a generic function calls an array method such as `concat` on its own `[T; n]`
- Fix the compiler crashing on a function with many more than 255 parameters or generic parameters, or a call with many more than 255 arguments. It now reports the existing limit error
- Add natives for fixed size arrays: `@arrLen`, `@arrContains`, `@arrEqual`, `@arrCopy`, `@arrReverse`, `@arrJoin`, `@arrSlice`, `@arrConcat` and `@arrToList`. Their signatures name the array's length with the placeholders `N` and `M`, such as `fun ([T; N]) => [T; N]` for `@arrCopy`, which native signatures can use but user code can't. `@arrSlice`, `@arrConcat` and `@arrToList` return lists. There is no `@arrIsEmpty`, `@arrPush` or other function that changes an array's length, and a list native given an array (or the other way round) is a compile error
- A literal such as `[1, 2, 3]` now makes a fixed size array `[i64; 3]` unless a list type is expected. An empty literal `[]` is an error unless a type is expected (`Can't tell the item type of an empty literal.`), so write `let xs: [i64] = [];`, and a literal given directly to a list parameter of a native, such as `@listLen([])`, is still a list. Arrays have their own heap object `ObjArray` and their own bytecode `OP_ARRAY` (43), and `OP_ARRAY` (38) is renamed `OP_LIST`. Indexing works on arrays, an array literal with the wrong number of items for its type is an error, and a number literal index past the end of an array is a compile error (`Index 2 is out of range for [i64; 2].`). **Breaking**: a list native such as `@listPush` no longer accepts an unannotated literal held in a variable, so write `let xs: [i64] = [1, 2, 3];`, and `var xs = [];` needs a type too
- Parse and type check the fixed size array type `[T; n]`, such as `[i64; 3]`. The length is a whole number literal and is part of the type, so `[i64; 3]`, `[i64; 4]` and `[i64]` are all different types. New AST node `NODE_TYPE_ARRAY` (`TypeArrayNode`), and the message for an unclosed list type is now `Expect ']' after list element type.`
- Rename the growable `[T]` type from array to list. Its natives start with `@list` instead of `@arr` (`@arrPush` is now `@listPush`, and so on), and the old names are no longer defined. `@typeof` of a `[T]` value is now `"list"`. The compiler's `TYPE_ARRAY`, `NODE_TYPE_ARRAY` and `typeArray` are now `TYPE_LIST`, `NODE_TYPE_LIST` and `typeList`, and the heap object `ObjArray` is now `ObjList`
- CMake finds readline with `find_library` and uses `-DCMAKE_PREFIX_PATH`. On
  macOS it prefers Homebrew's readline to the system `libedit`, which shows no
  prompt or echo for piped input
- Fix a trait impl replacing a struct's own method of the same name, including a private one, when the trait impl was loaded after it, such as on a later REPL line. The own method now keeps the name, and `Trait.method(value)` calls the trait's
- Add `krb init` to initialize a new Kirby project
- Add `krb config` to view the project's `kirby.project.toml` configuration
  - `krb config bin` Prints the `bin` value
  - `krb config examples` Prints the `examples` value
- Add `krb build <path> -o <output>` to build a file into a single executable
  - The executable is a copy of `krb-runtime`, which has the VM but no
    compiler, with the compiled program attached. `krb-runtime` is installed
    next to `krb`. `--runtime <path>` names a different one
  - Compiled code has a byte form for the first time: it starts with `KRBC`
    and records the Kirby version, and a different version refuses to load it
- Change `@argc` and `@argv` so a script sees the same arguments however it is
  started
  - `@argv(0)` is the program and `@argv(1)` onward are the script's own
    arguments, so `krb run file.krb a b` and a built `./app a b` both have an
    `@argc()` of 3
  - `@argv(0)` is the path given to `krb run`, the path of the example for
    `krb example`, `exec` for `krb exec` and `repl` for `krb repl`
  - Before, a script also saw `krb`, the command and the path, so its first
    argument was `@argv(3)`. The `cat`, `env` and `fib` examples are updated
  - `krb exec <source>` now passes the arguments after the source to the script
- Fix `if`/`else` and lambdas failing as an implicit return, and a nested
  block's last expression wrongly returning from the whole function. Blocks
  and `if` now have one parser each, and only a function body returns its value
- Fix `@randBetween` almost never returning `min`, and losing precision with very large numbers
- Add immutable struct fields `struct Box { let value: f64 }`
- Add new native function `@now()` that returns the current unix timestamp
- Add `Date` struct to `stdlib.krb`
  - Has `new(timestamp)`, `now()`, `from_parts(...)` and `from(text)` static
    methods, and a `get_` method for the timestamp and each part
- Allow number literals to have `f64` or `F64` suffix e.g. `123f64`, `456F64`
- New CLI subcommand `krb docs` which opens `https://kirbylang.github.io/kirbylang/` in the default browser
- VS Code extension: fix wrong or missing details in several native function hovers
- Add `u8` number type
- Add the number types `i8`, `i16`, `i32`, `u16`, `u32`, `u64` and `f32`, and
  suffixes for every number type, such as `7i32` and `1.5f32`
- Add `as` keyword for casting between number types (`u8` <-> `f64`)
  - New opcode: `OP_CAST`
- An array index that is not a whole number, such as `array[1.5]` or
  `array[-0.5]`, is now a runtime error instead of being truncated to a whole
  number
- Make `i64` the default number type. A whole number literal such as `42` is
  `i64`, and a literal with a decimal point such as `1.5` is `f64`
  - A whole number literal still takes the expected type, so
    `let x: f64 = 42;` works
  - Dividing two integers drops the remainder: `7 / 2` is `3`
  - An `f64` array index is a type error. Cast it with `as i64`
  - `@argc`, `@now` and `@randBetween` return an `i64`, and `@exit`,
    `@strRepeat` and `@randBetween` take an `i64`
  - `Date` in `stdlib.krb` uses `i64` for its timestamp and parts
- Replace `@len` with `@strLen` and `@arrLen` to improve builtin function typechecking
- `@strIndexOf` returns `-1` instead of `nil` when the string is not found
- Replace `@parseNumber` with `@parseInt` and `@parseFloat`
- `Ord`'s `cmp` returns an `i64`
- Whole number literals become `f64` where an `f64` is expected, including inside arithmetic, `if`/`else` branches and arrays. `let x: f64 = 7 / 2;` and `[1, 2.5]` no longer need `.0` on every number
- Dividing whole number literals where an `f64` is expected now divides as floats: `let x: f64 = 7 / 2;` is `3.5`, while `let x = 7 / 2;` is still `3`. Integer variables still divide as integers
- Clearer error when a number is too large for its type: `Integer literal 300 doesn't fit in u8, which holds 0 to 255.`
- Array types are written `[T]`, such as `let names: [string] = [];` or `fun total(numbers: [i64]): i64`. The `Array` type is removed
- Array natives are type checked by their element type: `@arrContains([1, 2], 2.0)` and `@arrPush(numbers, 2.5)` are type errors instead of quietly giving a wrong answer, and `@arrPop` gives the element type
- `@strSplit` returns an array of `string` and `@strToBytes` an array of `u8`
- `@strSlice`, `@strReplace` and `@strReplaceAll` are type checked
- Integer math that overflows its type is a runtime error instead of wrapping around: `255u8 + 1u8` stops with `Integer overflow: 255 + 1 doesn't fit in u8, which holds 0 to 255.`
- Fix a crash when a file's last instruction ends in an operand equal to `OP_RETURN`'s number
  - The compiler mistook it for a return and left out the script's final `OP_NIL` and `OP_RETURN`, so the VM ran past the end of the bytecode
- Implement function and struct generics, without trait bounds, in the typechecker. This was only in the parser before.
- The `let` keyword on immutable struct fields is now optional `struct Box[T] { value: T; }`
- Fix `@rand`, `@rand01` and `@randBetween` returning nearly the same value on every run on systems whose `rand()` changes little between close seeds, such as macOS. Random numbers now come from `arc4random` on macOS, and from `random` seeded with the time (with nanoseconds) and the process ID elsewhere
- Change to require that the top level of a Kirby file must only be declarations. Any assigned values must be comptime values (literal values or references to already declared items e.g. function or let)
- Trait Bounds
  - Functions, structs, and type aliases can define traits a type parameter must implement
    - `fun sum[T: Add](a: T, b: T): T = a + b;`
    - `struct Addable[T: Add] { value: T }`
    - `type Summable[T: Display] = [T];`
  - Multiple bounds can be defined on a type parameter: `[T: Add + Eq]`
  - New builtin traits: `Add`, `Sub`, `Mul`, `Div`, `Mod` and `Neg`
  - Super trait methods are supported on bounded type parameters e.g. `[T: Ord]` can call `t.toString()`
  - Calling a static trait method through a bound, such as `T.default()`, is now a compile error: `Can't call a static method on type parameter 'T'.` It was a runtime error, `Undefined variable 'T'.`
  - **Breaking:** `==` and `!=` now require struct to implement the `Eq` trait. Comparing against identity is no longer available
- Primitive Impls
  - Add methods on primitive types: `"abc".len()`, `2.5.floor()`, `xs.push(1)`
    - Works on `string`, `bool`, `unit`, every number type, lists and arrays
    - You can't write your own: `impl f64 { ... }` is an error
  - Add `Display`, `Eq`, `Ord`, `Default` and the operator traits to primitive types, so `7.toString()` and `a.cmp(b)` work
  - Allow calling a static trait method on a primitive's type name: `f64.default()`
  - Add `Trait.method(value)` to call a trait's method by name: `Display.toString(point)`
    - Runs the trait's method even when the type has its own method with the same name
  - Fix a type's own method running instead of the trait's method of the same name, through a type parameter's bound, an operator, or printing a struct
  - Add a compile error when two traits give a struct the same method name, or a method is both an instance and a static method
  - Change the error for a primitive passed to a bounded type parameter to say why: `string implements Display, but a primitive can't be used with a bound yet, required by T.` It used to say `string doesn't implement Display`
  - Fix `@listSlice`, `@arrSlice` and the `slice` method rejecting a `start` of `0`. Their errors for a negative position now name the right argument

## 0.4.0

- Decouple compiler, GC, and VM
- Build `stdlib/stdlib.krb` into the binary, so `krb` runs from any directory
- Structs
  - [Accessibility modifiers `pub`](https://github.com/kirbylang/kirbylang/issues/9)
    - New token: `TOKEN_PUB`
    - New operand for `OP_FIELD`: `flag` where `0` is false, `1` is true
  - Hoist structs during compilation
  - Deprecate the call syntax e.g. `Point()`
- [`continue` keyword](https://github.com/kirbylang/kirbylang/issues/13)
- [String interpolation](https://github.com/kirbylang/kirbylang/issues/15): `$"Hello {name}!"`
  - New tokens: `TOKEN_INTERP_STRING`, `TOKEN_INTERP_START`, `TOKEN_INTERP_MIDDLE`, `TOKEN_INTERP_END`
  - New AST node: `NODE_INTERP_STRING`
  - Placeholders can be a `string`, `f64`, `bool`, or a type that implements
    `Display`, which converts with its `toString()`
  - `{{` and `}}` write literal braces: `$"{{{n}}}" // {n}`
    - A single `}` is an error: `$"Hello }" // Error at '$"Hello }"': Single '}' in interpolated string. Write '}}' for a literal '}'.`
  - Compiles to `@strConcat([...])`. No new opcode.
- Three or more strings joined with `+` compile to one `@strConcat([...])`
  instead of an `OP_ADD` per `+`
- Unit literal expression: `()`.
- Numbers print with as many digits as they need and no more: `10`, not
  `10.000000`, and `0.1 + 0.2` as `0.30000000000000004`. Numbers from `1e-6` up
  to `1e21` print in plain decimal form, others in exponent form (`1e-7`,
  `1e+21`). `print`, `@numberToString`, and printed arrays all use this format.
- New native functions:
  - `@assert(condition, message)`
  - `@panic(message)`
  - `@boolToString(b)`
  - Output: `@print(value)`, `@println(value)`, `@eprint(value)`,
    `@eprintln(value)`. They write a value the way `print` does, using a
    struct's `Display` impl when it has one
  - Math: `@floor(n)`, `@round(n)`, `@trunc(n)`, `@abs(n)`, `@sqrt(n)`, `@pow(base, exponent)`, `@min(a, b)`, `@max(a, b)`
  - Arrays: `@arrJoin(array, separator)`
    - Every element must already be a string. There is no implicit conversion
      to string, so joining numbers means mapping `@numberToString` first
  - Strings: `@strConcat(strings)`, `@strContains(s, sub)`, `@strIndexOf(s, sub)`, `@strSlice(s, start, end)`, `@strSplit(s, sep)`, `@strTrim(s)`, `@strToUpper(s)`, `@strToLower(s)`, `@strStartsWith(s, prefix)`, `@strEndsWith(s, suffix)`, `@strRepeat(s, count)`, `@strReplace(s, old, new)`, `@strReplaceAll(s, old, new)`
- Native functions are now prefixed with `@` (e.g. `len(x)` becomes `@len(x)`)
  - `@` is now a reserved character in declarations and assignment expressions
    - This prevents native functions from being reassigned
  - `__version__()` is now `@version()`
- Initial implementation of types and typechecker
  - Limitations
    - Generic types are ignored
  - Native function signatures
    - Calls to a native with a signature are checked like any other call
    - `@clock`, `@version`, `@exit`, `@rand`, `@rand01`, `@randBetween`,
      `@ceil`, `@readFileToString`, `@writeStringToFile`, `@numberToString`,
      `@boolToString`,
      `@fileExists`, `@getenv`, `@setenv`, `@argc`, `@parseNumber`, `@strIsEmpty`,
      `@assert`, `@panic`, `@floor`, `@round`, `@trunc`, `@abs`, `@sqrt`,
      `@pow`, `@min`, `@max`, `@strContains`, `@strStartsWith`,
      `@strEndsWith`, `@strTrim`, `@strToUpper`, `@strToLower`, `@strRepeat`,
      `@strSplit`
    - Still unchecked, pending generics: `@len`, `@typeof`, `@instanceOf`,
      `@is`, `@isNumber`, `@isFunction`, `@isBool`, `@isString`, `@isNil`,
      `@strConcat`, `@print`, `@println`, `@eprint`, `@eprintln`, and the `@arr*`
      family
    - Still unchecked, pending `Option[T]`: `@argv`, `@prompt`, `@stdin`,
      `@strIndexOf`
    - Still unchecked, pending more than two parameters: `@strSlice`,
      `@strReplace`, `@strReplaceAll`
  - Primitive types
    - `string`
    - `bool`
    - `unit`, written `()` as a value
    - `f64`
  - Complex Types
    - `fun (f64) => f64`
    - `Struct`
    - `Array`
  - Generic types
    - Parses but the generic types are ignored
    - `fun [T, U] (T) => U`
    - `Struct[T]`
    - `type MapFn[T, U] = fun [T, U] (T) => U;`
  - Type aliases
    - `type number = f64`;
  - Bindings
    - `var age: f64 = 100;`
    - `let name = "Kirby";`
  - Functions
    - `fun map(value: f64): f64 { ... }`
  - Lambdas
    - `let sum: fun (f64, f64) => f64 = fun (a: f64, b: f64): f64 { a + b };`
  - Struct fields
    - `struct Box { pub let value: f64; }`
  - Traits
    - `trait Display { fun toString(self): string; }`
    - Supertraits: `trait Ord: Eq { ... }`
    - `Self` type inside `impl` blocks
    - Traits can only be implemented once on a struct
    - A trait name can only be declared once, and can't reuse a builtin
      trait's name (`Display`/`Eq`/`Ord`/`Default`)
    - Builtin traits, always in scope: `Display`, `Eq`, `Ord`, `Default`
    - A circular supertrait chain (`trait A: A {}`, or `trait A: B {} trait B: A {}`) is a compile error
    - Require structs to implement `Eq` trait for `==`/`!=`
    - `print` uses a struct's `Display` impl: it prints what `toString()`
      returns
    - Limitations
      - `impl Trait for` a primitive type (`f64`, `string`, `bool`, `unit`) isn't supported yet -- needs the same static call-resolution work operator overloading does
      - No real operator overloading yet. `Eq` is only a typecheck. `==` still runs identity equality
- [Definite Assignment Analysis](https://en.wikipedia.org/wiki/Definite_assignment_analysis)
- Projects
  - Directories with `kirby.project.krb` in the root
  - `bin = "bin/main.krb"` for `krb run`
  - `examples = "examples"` for `krb example NAME`
- `krb` revamping
  - Subcommands
    - `krb run [path] [args...]`
    - `krb example <name> [args...]`
    - `krb exec <source>`
    - `krb repl`
    - `krb compile <path>`
    - `krb parse <path>`
    - `krb lex <path>`
- Removal of the `print` statement. Replaced by the builtin `@print`, `@println`, `@eprint`, `@eprintln` functions

- Refine shadow binding rules

  | Binding  | Shadowed By | Result                                              |
  | -------- | ----------- | --------------------------------------------------- |
  | `var`    | `fun`       | Invalid.<br>This is an effect of function hoisting. |
  | `var`    | `let`       | Valid                                               |
  | `var`    | `struct`    | Invalid.<br>This is an effect of struct hoisting.   |
  | `var`    | `var`       | Valid                                               |
  | `fun`    | `fun`       | Invalid                                             |
  | `fun`    | `let`       | Invalid                                             |
  | `fun`    | `struct`    | Invalid                                             |
  | `fun`    | `var`       | Invalid                                             |
  | `let`    | `let`       | Invalid                                             |
  | `let`    | `var`       | Invalid                                             |
  | `let`    | `fun`       | Invalid                                             |
  | `let`    | `struct`    | Invalid                                             |
  | `struct` | `let`       | Invalid                                             |
  | `struct` | `var`       | Invalid                                             |
  | `struct` | `fun`       | Invalid                                             |
  | `struct` | `struct`    | Invalid                                             |

## 0.3.0

- Support for escape characters in strings: `\n`, `\r`, `\t`, `\"`, `\\`
- Function body expressions: `fun sum(a, b) = a + b;`
- Function implicit returns: `fun sum(a, b) { a + b }`
- Move all functions in stdlib.krb to native functions
  - `is`
  - `isBool`
  - `isNumber`
  - `isFunction`
  - `isString`
  - `isNil`
  - `strIsEmpty`
- Block expressions: `var sum = { 5 + 10 };`
  - New bytecode op: `OP_CLOSE_BLOCK_EXPR n`
- If expressions
  - `@println(if (true) "Hello" else "World"); // Hello`
  - `@println(if (value > 9000) "Over 9000!" else if (value == 42) "Life, Universe, Everything" else "Error"); // Error`
  - `@println(if (false) 123); // nil`
- Lambdas
  - `var sum = fun (a, b) { a + b };`
- A shared library is now generated on build: `libkirby.{so,dylib,dll}`
- Transition from a single-pass to a multi-pass compiler
  - Introduce `lex` function which produces `TokenStream`
  - Introduce `parse` function which consumes `TokenStream` and produces `AstNode**`
  - Update `compile` function to consume `AstNode**` to produce bytecode
  - Forward referencing functions (use before declaring)
  - Add `break` statement in `for` and `while` loops
- Replace `class` with `struct` + `impl`
  - Struct initializers e.g. `Point { x: 1, y: 0 }`
  - All methods for structs are defined in the `impl` block
  - Instance methods accept `self` as the first argument. Otherwise it's a static method.
  - Constructors are declared as static methods. No more `init` constructor.
  - New function type: `TYPE_STATIC_METHOD` (Removes old `TYPE_INITIALIZER`)
  - New tokens: `TOKEN_IMPL`, `TOKEN_COLON`
  - Renamed token: `TOKEN_STRUCT` -> `TOKEN_CLASS`
  - Renamed token: `TOKEN_THIS` -> `TOKEN_SELF`
  - Renamed bytecode OP: `OP_CLASS` -> `OP_STRUCT`
  - New bytecode OP: `OP_STRUCT_INIT`
- Add support for shebangs `#!/usr/bin/env krb run`
- Add immutable bindings `let`

## 0.2.0

- Modulo operator: `%`
  - New token: `TOKEN_MODULO`
  - New bytecode op: `OP_MODULO`
- Short forms of `and`, `or`: `&&` and `||`
- Nullish coalescing operator (`??`)
  - New tokens: `TOKEN_QUESTION_QUESTION`
  - New bytecode OPs: `OP_JUMP_IF_NOT_NIL`
- Class field declarations
  - Class fields are no longer dynamically defined. They must be declared before use.
  - New bytecode OP: `OP_FIELD`
- Native function: `numberToString(number)`
- Arrays
  - New tokens: `TOKEN_LEFT_BRACKET`, `TOKEN_RIGHT_BRACKET`
  - New bytecode OPs: `OP_ARRAY`, `OP_INDEX_GET`, `OP_INDEX_SET`
  - New native functions:
    - `arrPush(array, value)`
    - `arrPop(array)`
    - `arrInsert(array, index, value)`
    - `arrRemove(array, index)`
    - `arrClear(array)`
    - `arrContains(array, value)`
    - `arrCopy(array)`
    - `arrIsEmpty(array)`
    - `arrEqual(a, b)`
    - `arrSlice(array, start, end)`
    - `arrConcat(a, b)`
    - `arrReverse(array)`
  - Update `len` native function to return array lengths as well

## 0.1.0

These are the documented changes to the language/syntax from the original Lox language.

- No class inheritance
- Native functions added
  - `exit`
  - `version`
  - `rand`
  - `rand01`
  - `randBetween`
  - `ceil`
  - `readFileToString`
  - `writeStringToFile`
  - `fileExists`
  - `getenv`
  - `setenv`
  - `len`
  - `typeof`
  - `argc`
  - `argv`
  - `prompt`
  - `stdin`
  - `instanceOf`
  - `parseNumber`
