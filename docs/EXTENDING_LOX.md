# Extending Lox

These are the documented changes to the language/syntax from the original Lox language.

## Next: 0.3.0

- Support to escape characters in strings: `\n`, `\r`, `\t`, `\"`, `\\`
- Function body expressions: `fun sum(a, b) = a + b;`
- Function implicit returns: `fun sum(a, b) { a + b }`
- Move all functions in stdlib.lox to native functions
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
- Native Functions
  - [ ] `arrSort(array, fn)`
  - [ ] `arrMap(array, fn)`
  - [ ] `arrJoinToString(array, separator)`

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
