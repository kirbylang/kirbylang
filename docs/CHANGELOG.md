# Change Log

## Next: 0.4.0

- Decouple compiler, GC, and VM
- Structs
  - [Accessibility modifiers `pub`](https://github.com/kirbylang/kirbylang/issues/9)
    - New token: `TOKEN_PUB`
    - New operand for `OP_FIELD`: `flag` where `0` is false, `1` is true
  - Hoist structs during compilation
  - Deprecate the call syntax e.g. `Point()`
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

- [ ] [String interpolation](https://github.com/kirbylang/kirbylang/issues/15)
- [ ] [`continue` keyword](https://github.com/kirbylang/kirbylang/issues/13)
- Lambdas
  - [ ] Lambda body expressions `var sum = fun (a, b) a + b;`
- Native Functions
  - [ ] `print`, `println`, `eprint`, `eprintln`
  - [ ] `arrSort(array, fn)`
  - [ ] `arrMap(array, fn)`
  - [ ] `arrJoinToString(array, separator)`

## 0.3.0

- Support to escape characters in strings: `\n`, `\r`, `\t`, `\"`, `\\`
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
  - `print if (true) "Hello" else "World"; // Hello`
  - `print if (value > 9000) "Over 9000!" else if (value == 42) "Life, Universe, Everything" else "Error"; // Error`
  - `print if (false) 123; // nil`
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
- Add support for shebangs `#!/usr/bin/env krb -f`
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
  - `__version__`
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
