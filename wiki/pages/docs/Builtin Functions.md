---
aliases:
  - Native Functions
---

Kirby has several builtin functions (native functions) available.

Builtin functions are prefixed, making them easy to identifier vs user code functions. e.g. `@len`, `@getenv`

This also makes `@` a reserved character in identifiers.

## General

### `@len`

Get the length of a string or array.

```kirby
@println(@len("Hello, World!"));

@println(@len([1, 2 ,3]));
```

## Math

### `@abs`

Get a number without its sign.

```kirby
@println(@abs(-3.5)); // 3.5

@println(@abs(3.5)); // 3.5
```

### `@ceil`

Round a number up.

```
@println(@ceil(1.6)); // 2
```

### `@floor`

Round a number down to the nearest whole number.

```kirby
@println(@floor(2.7)); // 2

@println(@floor(-2.5)); // -3
```

### `@max`

Get the larger of two numbers.

```kirby
@println(@max(3, 7)); // 7

@println(@max(-1, -5)); // -1
```

### `@min`

Get the smaller of two numbers.

```kirby
@println(@min(3, 7)); // 3

@println(@min(-1, -5)); // -5
```

### `@pow`

Raise a number to a power. It is an error to raise `0` to a negative power, or a negative number to a fractional power.

```kirby
@println(@pow(2, 10)); // 1024

@println(@pow(9, 0.5)); // 3
```

### `@round`

Round a number to the nearest whole number. Halves round away from zero.

```kirby
@println(@round(2.4)); // 2

@println(@round(2.5)); // 3

@println(@round(-2.5)); // -3
```

### `@sqrt`

Get the square root of a number. A negative number is an error. When the argument is a constant, such as `@sqrt(-1)`, the error is reported before the program runs.

```kirby
@println(@sqrt(16)); // 4

@println(@sqrt(2.25)); // 1.5
```

### `@trunc`

Remove a number's fractional part, moving toward zero.

```kirby
@println(@trunc(2.7)); // 2

@println(@trunc(-2.7)); // -2
```

## Arrays

### `@arrClear`

Clears all values from an array.

```kirby
var array = [1, 2, 3];

@arrClear(array);

@println(array); // []
```

### `@arrConcat`

Concatenates two arrays in to a new array.

```kirby
var a = [1, 2, 3];
var b = [4, 5, 6];
var c = @arrConcat(a, b);

@println(a); // [1, 2, 3]
@println(b); // [4, 5, 6]
@println(c); // [1, 2, 3, 4, 5, 6]
```

### `@arrContains`

Returns if an array contains a value.

```kirby
var array = [1, 2, 3];

@println(@arrContains(array, 2)); // true
@println(@arrContains(array, 99)); // false
```

### `@arrCopy`

Copy an array to a new array.

```kirby
var a = [1, 2, 3];
var b = a;
var c = @arrCopy(a);

a[0] = 100;

@println(a); // [100, 2, 3]
@println(b); // [100, 2, 3]
@println(c); // [1, 2, 3]
```

### `@arrEqual`

Shallow compares two arrays' values for equality.

```kirby
var a = [1, 2, 3];
var b = a;
var c = @arrCopy(a);

@println(@arrEqual(a, b)); // true
@println(@arrEqual(b, c)); // false
@println(@arrEqual(a, c)); // false
```

### `@arrInsert`

Insert a value into an array at a specific index.

```kirby
var array = [];

@arrInsert(array, 0, "Hello");
@arrInsert(array, 1, "World");

@println(array); // [Hello, World]
```

### `@arrIsEmpty`

Returns if an array is empty or not.

```kirby
@println(@arrIsEmpty([]));
@println(@arrIsEmpty([1, 2, 3]));
```

### `@arrJoin`

Concatenates every element of an array into one string, separated by a separator string. Every element must already be a string.

```kirby
var parts = ["kirby", "is", "a", "language"];

@println(@arrJoin(parts, " ")); // kirby is a language
@println(@arrJoin(parts, ", ")); // kirby, is, a, language
@println(@arrJoin(parts, "")); // kirbyisalanguage
```

### `@arrPop`

Pop a value onto the end of an array and return it.

```kirby
var array = ["Hello", "World"];
var poppedValue = @arrPop(array);

@println(array); // [Hello]
@println(poppedValue); // World
```

### `@arrPush`

Push a value onto the end of an array.

```kirby
var array = [];

@arrPush(array, "Hello");
@arrPush(array, "World");

@println(array); // [Hello, World]
```

### `@arrRemove`

Insert a value into an array at a specific index.

```kirby
var array = [1, 2, 3];

@arrRemove(array, 0);

@println(array); // [2, 3]
```

### `@arrReverse`

Reverse an array in place.

```kirby
var array = [1, 2, 3];

@arrReverse(array);

@println(array); // [3, 2, 1]
```

### `@arrSlice`

Pop a value onto the end of an array and return it.

```kirby
var array = [10, 20, 30, 40, 50];
var slice = @arrSlice(array, 1, 4);

@println(@arrEqual([20, 30, 40], slice));
```

## Errors

### `@assert`

Exit (code 70) with an error message if the asserted condition is `false`.

```kirby
@assert(1 + 1 == 2, "math is broken"); // passes

@assert(false, "expected a positive number"); // assertion failed: expected a positive number
```

## `@panic`

Exit (code 70) with an error message.

```kirby
@panic("unreachable state"); // panic: unreachable state
```

## System

### `@version`

Get the current version of the kirby language.

```kirby
@println(@version()); // 0.0.0
```

### `@clock`

Get the number of seconds that have passed since the program started.

```kirby
@println(@clock());
```

### `@now`

Get the current number of seconds since 1970-01-01.

```kirby
let now: f64 = @now();

@println(now);
```

### `@exit`

Exit with an exit code.

```kirby
var exitCode = 1;

@exit(exitCode);
```

#### Valid Exit Codes

Exit codes must be `>= 0`.

```kirby
@exit(-1); // Compile time error

let exit_code = -1;

@exit(exit_code); // Runtime error
```

### `@getenv`

Get an environment variable's value.

```kirby
@println(@getenv("PATH"));
```

### `@setEnv`

Set an environment variable to a string value.

```kirby
@setenv("NAME", "WORLD");

@println("Hello " + @getenv("NAME"));
```

### `@prompt`

Read in text from `stdin` until newline (NL) is encountered. An optional message can be passed as well.

```kirby
var name = @prompt("Name: ");

@println("Hello " + name);
```

### `@stdin`

Read in text from \`stdin\` until end of file (EOF) is encountered.

```kirby
var text = @stdin();
```

### @argc

The number of arguments passed to the program, counting the program itself.

```kirby
@println(@argc()); // 2, for: krb run file.krb one
```

### @argv

Access the arguments passed to the program by index. `@argv(0)` is the program
and `@argv(1)` is the first argument. It is `nil` past the last one.

```kirby
@println(@argv(1));
```

## IO

### `@print`

Write a value to stdout, the way `print` does, without a newline. A struct that implements `Display` is written with its `toString()`.

```kirby
@print("Loading");
@print("...");
```

### `@println`

Write a value to stdout, the way `print` does, followed by a newline. A struct that implements `Display` is written with its `toString()`.

```kirby
@println("Hello, World");
@println(0.1 + 0.2); // 0.30000000000000004
```

### `@eprint`

Write a value to stderr without a newline. A struct that implements `Display` is written with its `toString()`.

```kirby
@eprint("error: ");
```

### `@eprintln`

Write a value to stderr, followed by a newline. A struct that implements `Display` is written with its `toString()`.

```kirby
@eprintln("something failed");
```

### `@fileExists`

Return if a file or path exists at a given path.

```kirby
let path = @prompt("File Path: ");

if (!@fileExists(path)) {
  @println("File doesn't exist '" + path + "'");

  @exit(1)
}

@println(path);
```

### `@readFileToString`

Read a file at path to string.

```kirby
let text = @readFileToString("./path/to/file.txt");

@println(text);
```

### `@writeStringToFile`

Write a string to a file at a given path.

```kirby
var path = @prompt("File: ");
var text = @prompt("Text: ");

@writeStringToFile(path, text);
```

## Types/Values

### `@instanceOf`

Returns if a value is an instance of a struct.

```kirby
struct Food {}

let food = Food();

@println(@instanceOf(food, Food)); // true
```

### `@typeof`

Get a value's type.

```kirby
@println(@typeof(true)); // "bool"
@println(@typeof(123)); // "number"
@println(@typeof("Hello World")); // "string"
@println(@typeof([])); // "array"

struct Food {}

@println(@typeof(Food)); // struct

let food = Food {};

@println(@typeof(food)); // instance
```

### `@is`

Returns whether a value is of the named type.

The second argument is one of `"bool"`, `"string"`, `"number"`, `"function"` or `"nil"`.

```kirby
@println(@is(true, "bool")); // true

@println(@is(12345, "string")); // false
```

### `@isBool`

Returns if value is a bool or not.

```kirby
@println(@isBool(false)); // true

@println(@isBool(123)); // false
```

### `@isFunction`

Returns if value is a function or not.

```kirby
fun sum(a, b) = a + b;

@println(@isFunction(@ceil)); // true
@println(@isFunction(sum)); // true
@println(@isFunction(123)); // false
```

### `@isNil`

Returns if value is a nil or not.

```kirby
@println(@isNil(nil)); // true
@println(@isNil(123)); // false
```

### `@isNumber`

Returns if value is a number or not.

```kirby
@println(@isNumber(123)); // true
@println(@isNumber("Hello World")); // false
```

### `@isString`

Returns if value is a string or not.

```kirby
@println(@isString("Hello World")); // true
@println(@isString(123)); // false
```

## Booleans

### `@boolToString`

Convert a boolean to a string.

```kirby
@println(@boolToString(true)); // "true"
```

## Numbers

### `@numberToString`

Convert a number to a string.

```kirby
@println(@numberToString(123)); // "123"
```

### `@parseNumber`

Parse a string into a number.

```kirby
@println(@parseNumber("123")); // 123
```

## Random

### `@rand`

Get a random number.

```kirby
@println(@rand()); // 1145892349
```

### `@rand01`

Randomly get 0 or 1.

```kirby
@println(@rand01()); // 0
```

### `@randBetween`

Get a random number between min and max.

```kirby
@println(@randBetween(1, 10)); // 3
```

## Strings

### `@strContains`

Returns if a string contains another string.

```kirby
@println(@strContains("hello world", "lo wo")); // true
@println(@strContains("hello", "xyz")); // false
```

### `@strEndsWith`

Returns if a string ends with another string.

```kirby
@println(@strEndsWith("hello", "lo")); // true
@println(@strEndsWith("hello", "he")); // false
```

### `@strIndexOf`

Get the position where a string first appears in another string, counting from 0. Returns `nil` if it does not appear.

```kirby
@println(@strIndexOf("hello world", "world")); // 6
@println(@strIndexOf("hello", "xyz")); // nil
```

### `@strIsEmpty`

Returns if a string's length is zero or not.

```kirby
@println(@strIsEmpty("")); // true
@println(@strIsEmpty("Hello World")); // false
```

### `@strRepeat`

Repeat a string a number of times. The count must be a whole number, zero or more.

```kirby
@println(@strRepeat("ab", 3)); // ababab
@println(@strRepeat("ab", 0)); // (empty string)
```

### `@strReplace`

Replace the first place a string appears in another string. Returns the string unchanged if it does not appear, or if the string to replace is empty.

```kirby
@println(@strReplace("a-b-c", "-", "+")); // a+b-c
```

### `@strReplaceAll`

Replace every place a string appears in another string. Returns the string unchanged if it does not appear, or if the string to replace is empty.

```kirby
@println(@strReplaceAll("a-b-c", "-", "+")); // a+b+c
```

### `@strSlice`

Get the part of a string from `start` up to, but not including, `end`. Both are whole positions from 0 to the length of the string. It is an error if either is outside the string, is not a whole number, or if `start` is after `end`.

```kirby
@println(@strSlice("hello world", 0, 5)); // hello
@println(@strSlice("hello world", 6, 11)); // world
```

### `@strConcat`

Concatenate an array of strings into one string. Every element must already be a string.

```kirby
@println(@strConcat(["Hello", ", ", "World"])); // Hello, World
@println(@strConcat([])); // an empty string
```

### `@strSplit`

Split a string into an array of strings at each place the separator appears. An empty separator splits the string into single character strings.

```kirby
@println(@strSplit("a,b,c", ",")); // [a, b, c]
@println(@strSplit("abc", "")); // [a, b, c]
```

### `@strStartsWith`

Returns if a string starts with another string.

```kirby
@println(@strStartsWith("hello", "he")); // true
@println(@strStartsWith("hello", "lo")); // false
```

### `@strToLower`

Change the letters `A` to `Z` in a string to lower case. Other characters are not changed.

```kirby
@println(@strToLower("Hello, World!")); // hello, world!
```

### `@strToUpper`

Change the letters `a` to `z` in a string to upper case. Other characters are not changed.

```kirby
@println(@strToUpper("Hello, World!")); // HELLO, WORLD!
```

### `@strTrim`

Remove spaces, tabs, and newlines from both ends of a string.

```kirby
@println("[" + @strTrim("  hello  ") + "]"); // [hello]
```
