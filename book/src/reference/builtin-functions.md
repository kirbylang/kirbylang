# Builtin Functions

Kirby has several builtin functions (native functions) available.

Builtin functions are prefixed, which makes them easy to tell apart from user functions. e.g. `@strLen`, `@getenv`

This also makes `@` a reserved character in identifiers.

## Lists

### `@listClear`

Clears all values from a list.

```kirby
fun main(): unit {
    var list: [i64] = [1, 2, 3];

    @listClear(list);

    @println(list); // []
}
```

### `@listConcat`

Concatenates two lists in to a new list.

```kirby
fun main(): unit {
    var a: [i64] = [1, 2, 3];
    var b: [i64] = [4, 5, 6];
    var c = @listConcat(a, b);

    @println(a); // [1, 2, 3]
    @println(b); // [4, 5, 6]
    @println(c); // [1, 2, 3, 4, 5, 6]
}
```

### `@listContains`

Returns if a list contains a value.

```kirby
fun main(): unit {
    var list: [i64] = [1, 2, 3];

    @println(@listContains(list, 2)); // true
    @println(@listContains(list, 99)); // false
}
```

### `@listCopy`

Copy a list to a new list.

```kirby
fun main(): unit {
    var a: [i64] = [1, 2, 3];
    var b = a;
    var c = @listCopy(a);

    a[0] = 100;

    @println(a); // [100, 2, 3]
    @println(b); // [100, 2, 3]
    @println(c); // [1, 2, 3]
}
```

### `@listEqual`

Shallow compares two lists' values for equality.

```kirby
fun main(): unit {
    var a: [i64] = [1, 2, 3];
    var b = a;
    var c = @listCopy(a);

    @println(@listEqual(a, b)); // true
    @println(@listEqual(b, c)); // true
    @println(@listEqual(a, c)); // true

    c[0] = 100;

    @println(@listEqual(a, c)); // false
}
```

### `@listInsert`

Insert a value into a list at a specific index.

```kirby
fun main(): unit {
    var list: [string] = [];

    @listInsert(list, 0, "Hello");
    @listInsert(list, 1, "World");

    @println(list); // [Hello, World]
}
```

### `@listIsEmpty`

Returns if a list is empty or not.

```kirby
fun main(): unit {
    @println(@listIsEmpty([])); // true
    @println(@listIsEmpty([1, 2, 3])); // false
}
```

### `@listLen`

Get the number of elements in a list, as an `i64`.

```kirby
fun main(): unit {
    @println(@listLen([1, 2, 3])); // 3
    @println(@listLen([])); // 0
}
```

### `@listJoin`

Concatenates every element of a list into one string, separated by a separator string. Every element must already be a string.

```kirby
fun main(): unit {
    var parts: [string] = ["kirby", "is", "a", "language"];

    @println(@listJoin(parts, " ")); // kirby is a language
    @println(@listJoin(parts, ", ")); // kirby, is, a, language
    @println(@listJoin(parts, "")); // kirbyisalanguage
}
```

### `@listPop`

Remove and return the last value in a list. It is an error if the list is empty.

```kirby
fun main(): unit {
    var list: [string] = ["Hello", "World"];
    var poppedValue = @listPop(list);

    @println(list); // [Hello]
    @println(poppedValue); // World
}
```

### `@listPush`

Push a value onto the end of a list.

```kirby
fun main(): unit {
    var list: [string] = [];

    @listPush(list, "Hello");
    @listPush(list, "World");

    @println(list); // [Hello, World]
}
```

### `@listRemove`

Remove and return the value at a specific index. It is an error if the index is outside the list.

```kirby
fun main(): unit {
    var list: [i64] = [1, 2, 3];
    var removed = @listRemove(list, 0);

    @println(list); // [2, 3]
    @println(removed); // 1
}
```

### `@listReverse`

Get a new list with the items in the opposite order. The original list isn't changed.

```kirby
fun main(): unit {
    var list: [i64] = [1, 2, 3];
    let reversed = @listReverse(list);

    @println(reversed); // [3, 2, 1]
    @println(list); // [1, 2, 3]
}
```

### `@listSlice`

Copy the items from `start` up to, but not including, `end` into a new list. Positions count from `0`, and `start` must be before `end`.

```kirby
fun main(): unit {
    var list: [i64] = [10, 20, 30, 40, 50];
    var slice = @listSlice(list, 1, 4);

    @println(slice); // [20, 30, 40]
}
```

## Arrays

These work on fixed size arrays such as `[i64; 3]`. An array can't change length, so there are no push or pop functions. Use the `@list` functions on a list `[i64]`, and `@arrToList` to turn an array into a list.

### `@arrLen`

Get the number of elements in an array, as an `i64`. The length is part of the array's type, so it is known when the program is compiled.

```kirby
fun main(): unit {
    let a = [1, 2, 3];

    @println(@arrLen(a)); // 3
}
```

### `@arrContains`

Returns if an array contains a value.

```kirby
fun main(): unit {
    let a = [1, 2, 3];

    @println(@arrContains(a, 2)); // true
    @println(@arrContains(a, 99)); // false
}
```

### `@arrEqual`

Shallow compares two arrays' values for equality. Both arrays must have the same length and item type.

```kirby
fun main(): unit {
    let a = [1, 2, 3];
    let b = [1, 2, 3];

    @println(@arrEqual(a, b)); // true
}
```

### `@arrCopy`

Copy an array to a new array of the same length.

```kirby
fun main(): unit {
    let a = [1, 2, 3];
    let b = @arrCopy(a);

    b[0] = 100;

    @println(a); // [1, 2, 3]
    @println(b); // [100, 2, 3]
}
```

### `@arrReverse`

Returns a new array with the items of an array in reverse order, with the same length.

```kirby
fun main(): unit {
    let a = [1, 2, 3];

    @println(@arrReverse(a)); // [3, 2, 1]
}
```

### `@arrJoin`

Concatenates every element of an array of strings into one string, separated by a separator string.

```kirby
fun main(): unit {
    let parts = ["kirby", "is", "a", "language"];

    @println(@arrJoin(parts, " ")); // kirby is a language
}
```

### `@arrSlice`

Copies the items from a start index up to an end index of an array into a new list. The result is a list because its length isn't known when the program is compiled. Positions count from `0`, and `start` must be before `end`.

```kirby
fun main(): unit {
    let a = [10, 20, 30, 40, 50];
    let slice = @arrSlice(a, 1, 4);

    @println(slice); // [20, 30, 40]
}
```

### `@arrConcat`

Concatenates two arrays in to a new list. The result is a list because its length isn't known when the program is compiled.

```kirby
fun main(): unit {
    let a = [1, 2, 3];
    let b = [4, 5, 6];

    @println(@arrConcat(a, b)); // [1, 2, 3, 4, 5, 6]
}
```

### `@arrToList`

Copies an array to a new list, which can grow and shrink.

```kirby
fun main(): unit {
    let a = [1, 2, 3];
    let list = @arrToList(a);

    @listPush(list, 4);

    @println(list); // [1, 2, 3, 4]
}
```

## Date/Time

### `@clock`

Get the processor time the program has used, in seconds. Time spent waiting, such as for input, isn't counted.

```kirby
fun main(): unit {
    @println(@clock());
}
```

### `@now`

Get the current number of whole seconds since 1970-01-01.

```kirby
fun main(): unit {
    let now: i64 = @now();

    @println(now);
}
```

## Environment

### `@getenv`

Get an environment variable's value. It is an error if the variable isn't set.

```kirby
fun main(): unit {
    @println(@getenv("PATH"));
}
```

### `@setenv`

Set an environment variable to a string value.

```kirby
fun main(): unit {
    @setenv("NAME", "WORLD");

    @println("Hello " + @getenv("NAME"));
}
```

### `@argc`

The number of arguments passed to the program, counting the program itself.

```kirby
fun main(): unit {
    @println(@argc()); // 2, for: krb run file.krb one
}
```

### `@argv`

Access the arguments passed to the program by index. `@argv(0)` is the program
and `@argv(1)` is the first argument. It is `nil` past the last one.

```kirby
fun main(): unit {
    @println(@argv(1));
}
```

## Errors

### `@assert`

Exit (code 70) with an error message if the asserted condition is `false`.

```kirby
fun main(): unit {
    @assert(1 + 1 == 2, "math is broken"); // passes

    @assert(false, "expected a positive number"); // assertion failed: expected a positive number
}
```

### `@panic`

Exit (code 70) with an error message.

```kirby
fun main(): unit {
    @panic("unreachable state"); // panic: unreachable state
}
```

## IO

### stdout

#### `@print`

Write a value to stdout without a newline. A struct that implements `Display` is written with its `toString()`.

```kirby
fun main(): unit {
    @print("Loading");
    @print("...");
}
```

#### `@println`

Write a value to stdout, followed by a newline. A struct that implements `Display` is written with its `toString()`.

```kirby
fun main(): unit {
    @println("Hello, World");
    @println(0.1 + 0.2); // 0.30000000000000004
}
```

### stderr

#### `@eprint`

Write a value to stderr without a newline. A struct that implements `Display` is written with its `toString()`.

```kirby
fun main(): unit {
    @eprint("error: ");
}
```

#### `@eprintln`

Write a value to stderr, followed by a newline. A struct that implements `Display` is written with its `toString()`.

```kirby
fun main(): unit {
    @eprintln("something failed");
}
```

### stdin

#### `@prompt`

Read in text from `stdin` until newline (NL) is encountered. An optional message can be passed as well.

```kirby
fun main(): unit {
    var name = @prompt("Name: ");

    @println("Hello " + name);
}
```

#### `@stdin`

Read in text from `stdin` until end of file (EOF) is encountered.

```kirby
fun main(): unit {
    var text = @stdin();
}
```

### Files

#### `@fileExists`

Return if a file or path exists at a given path.

```kirby
fun main(): unit {
    let path = @prompt("File Path: ");

    if (!@fileExists(path)) {
      @println("File doesn't exist '" + path + "'");

      @exit(1)
    }

    @println(path);
}
```

#### `@readFileToString`

Read a file at path to string.

```kirby
fun main(): unit {
    let text = @readFileToString("./path/to/file.txt");

    @println(text);
}
```

#### `@writeStringToFile`

Write a string to a file at a given path.

```kirby
fun main(): unit {
    var path = @prompt("File: ");
    var text = @prompt("Text: ");

    @writeStringToFile(path, text);
}
```

## Math

### `@abs`

Get a number without its sign.

```kirby
fun main(): unit {
    @println(@abs(-3.5)); // 3.5

    @println(@abs(3.5)); // 3.5
}
```

### `@ceil`

Round a number up.

```kirby
fun main(): unit {
    @println(@ceil(1.6)); // 2
}
```

### `@floor`

Round a number down to the nearest whole number.

```kirby
fun main(): unit {
    @println(@floor(2.7)); // 2

    @println(@floor(-2.5)); // -3
}
```

### `@max`

Get the larger of two numbers.

```kirby
fun main(): unit {
    @println(@max(3, 7)); // 7

    @println(@max(-1, -5)); // -1
}
```

### `@min`

Get the smaller of two numbers.

```kirby
fun main(): unit {
    @println(@min(3, 7)); // 3

    @println(@min(-1, -5)); // -5
}
```

### `@pow`

Raise a number to a power. It is an error to raise `0` to a negative power, or a negative number to a fractional power.

```kirby
fun main(): unit {
    @println(@pow(2, 10)); // 1024

    @println(@pow(9, 0.5)); // 3
}
```

### `@round`

Round a number to the nearest whole number. Halves round away from zero.

```kirby
fun main(): unit {
    @println(@round(2.4)); // 2

    @println(@round(2.5)); // 3

    @println(@round(-2.5)); // -3
}
```

### `@sqrt`

Get the square root of a number. A negative number is an error. When the argument is a constant, such as `@sqrt(-1)`, the error is reported before the program runs.

```kirby
fun main(): unit {
    @println(@sqrt(16)); // 4

    @println(@sqrt(2.25)); // 1.5
}
```

### `@trunc`

Remove a number's fractional part, moving toward zero.

```kirby
fun main(): unit {
    @println(@trunc(2.7)); // 2

    @println(@trunc(-2.7)); // -2
}
```

## Numbers

### `@numberToString`

Convert a number of any type to a string.

```kirby
fun main(): unit {
    @println(@numberToString(123)); // 123
    @println(@numberToString(255u8)); // 255
}
```

### `@parseFloat`

Parse a number such as `1.5` or `-2` into an `f64`.

```kirby
fun main(): unit {
    @println(@parseFloat("1.5")); // 1.5
    @println(@parseFloat("42")); // 42
    @println(@parseFloat("2.5e-4")); // 0.00025
}
```

### `@parseInt`

Parse a string of digits (with an optional sign `+` or `-`) into an `i64`.

```kirby
fun main(): unit {
    @println(@parseInt("123")); // 123
    @println(@parseInt("-45")); // -45
}
```

## Random

### `@rand`

Get a random number.

```kirby
fun main(): unit {
    @println(@rand()); // for example, 1145892349
}
```

### `@rand01`

Get a random number from 0 to 1.

```kirby
fun main(): unit {
    @println(@rand01()); // for example, 0.22301528985752506
}
```

### `@randBetween`

Get a random whole number from min to max, as an `i64`. `min` itself is currently almost never returned.

```kirby
fun main(): unit {
    @println(@randBetween(1, 10)); // for example, 3
}
```

## Strings

### `@boolToString`

Convert a boolean to a string.

```kirby
fun main(): unit {
    @println(@boolToString(true)); // true
}
```

### `@strContains`

Returns if a string contains another string.

```kirby
fun main(): unit {
    @println(@strContains("hello world", "lo wo")); // true
    @println(@strContains("hello", "xyz")); // false
}
```

### `@strEndsWith`

Returns if a string ends with another string.

```kirby
fun main(): unit {
    @println(@strEndsWith("hello", "lo")); // true
    @println(@strEndsWith("hello", "he")); // false
}
```

### `@strIndexOf`

Get the position where a string first appears in another string, counting from 0, as an `i64`. Returns `-1` if it does not appear.

```kirby
fun main(): unit {
    @println(@strIndexOf("hello world", "world")); // 6
    @println(@strIndexOf("hello", "xyz")); // -1
}
```

### `@strToBytes`

Convert a string to a list of `u8` bytes, one element per byte, in order. The elements are `u8`, so bytes 128..255 are preserved.

```kirby
fun main(): unit {
    let bytes: [u8] = @strToBytes("AB");
    @println(bytes[0]); // 65
    @println(bytes[1]); // 66
}
```

### `@strIsEmpty`

Returns if a string's length is zero or not.

```kirby
fun main(): unit {
    @println(@strIsEmpty("")); // true
    @println(@strIsEmpty("Hello World")); // false
}
```

### `@strLen`

Get the number of bytes in a string, as an `i64`.

```kirby
fun main(): unit {
    @println(@strLen("Hello, World!")); // 13
    @println(@strLen("")); // 0
}
```

### `@strRepeat`

Repeat a string a number of times. The count must be a whole number, zero or more.

```kirby
fun main(): unit {
    @println(@strRepeat("ab", 3)); // ababab
    @println(@strRepeat("ab", 0)); // (empty string)
}
```

### `@strReplace`

Replace the first place a string appears in another string. Returns the string unchanged if it does not appear, or if the string to replace is empty.

```kirby
fun main(): unit {
    @println(@strReplace("a-b-c", "-", "+")); // a+b-c
}
```

### `@strReplaceAll`

Replace every place a string appears in another string. Returns the string unchanged if it does not appear, or if the string to replace is empty.

```kirby
fun main(): unit {
    @println(@strReplaceAll("a-b-c", "-", "+")); // a+b+c
}
```

### `@strSlice`

Get the part of a string from `start` up to, but not including, `end`. Both are whole positions from 0 to the length of the string. It is an error if either is outside the string, is not a whole number, or if `start` is after `end`.

```kirby
fun main(): unit {
    @println(@strSlice("hello world", 0, 5)); // hello
    @println(@strSlice("hello world", 6, 11)); // world
}
```

### `@strConcat`

Concatenate a list of strings into one string. Every element must already be a string.

```kirby
fun main(): unit {
    @println(@strConcat(["Hello", ", ", "World"])); // Hello, World
    @println(@strConcat([])); // an empty string
}
```

### `@strSplit`

Split a string into a list of strings at each place the separator appears. An empty separator splits the string into single character strings.

```kirby
fun main(): unit {
    @println(@strSplit("a,b,c", ",")); // [a, b, c]
    @println(@strSplit("abc", "")); // [a, b, c]
}
```

### `@strStartsWith`

Returns if a string starts with another string.

```kirby
fun main(): unit {
    @println(@strStartsWith("hello", "he")); // true
    @println(@strStartsWith("hello", "lo")); // false
}
```

### `@strToLower`

Change the letters `A` to `Z` in a string to lower case. Other characters are not changed.

```kirby
fun main(): unit {
    @println(@strToLower("Hello, World!")); // hello, world!
}
```

### `@strToUpper`

Change the letters `a` to `z` in a string to upper case. Other characters are not changed.

```kirby
fun main(): unit {
    @println(@strToUpper("Hello, World!")); // HELLO, WORLD!
}
```

### `@strTrim`

Remove spaces, tabs, and newlines from both ends of a string.

```kirby
fun main(): unit {
    @println("[" + @strTrim("  hello  ") + "]"); // [hello]
}
```

## System

### `@exit`

Exit with an exit code.

```kirby
fun main(): unit {
    var exitCode = 1;

    @exit(exitCode);
}
```

#### Valid Exit Codes

Exit codes must be from `0` to `255`.

A number written directly in the call is checked before the program runs.

```kirby
fun main(): unit {
    @exit(-1); // Error: function @exit expects argument 1 to be a non-negative number but got -1.
}
```

A number held in a variable is checked while the program runs.

```kirby
fun main(): unit {
    let exitCode = -1;

    @exit(exitCode); // function @exit expects argument 1 to be greater than or equal to 0 but got -1.
}
```

### `@version`

Get the current version of the kirby language.

```kirby
fun main(): unit {
    @println(@version()); // for example, 0.4.0
}
```

## Types/Values

### `@instanceOf`

Returns if a value is an instance of a struct.

```kirby
struct Food {}

fun main(): unit {
    let food = Food {};

    @println(@instanceOf(food, Food)); // true
}
```

### `@typeof`

Get the name of a value's type, as a string. A number gives its number type, such as `i64`, `u8` or `f64`.

```kirby
struct Food {}

fun main(): unit {
    @println(@typeof(true)); // bool
    @println(@typeof(123)); // i64
    @println(@typeof(1.5)); // f64
    @println(@typeof("Hello World")); // string
    @println(@typeof(nil)); // nil

    let items: [i64] = [1, 2];
    @println(@typeof(items)); // list
    @println(@typeof([1, 2])); // array

    @println(@typeof(main)); // function
    @println(@typeof(Food)); // struct

    let food = Food {};

    @println(@typeof(food)); // instance
}
```

### `@is`

Returns whether a value is of the named type.

The second argument is a name that `@typeof` returns, such as `"bool"`, `"string"`, `"i64"`, `"function"` or `"nil"`. There is no name that matches every number type, so use `@isNumber` for that.

```kirby
fun main(): unit {
    @println(@is(true, "bool")); // true
    @println(@is(12345, "i64")); // true
    @println(@is(12345, "string")); // false
}
```

### `@isBool`

Returns if value is a bool or not.

```kirby
fun main(): unit {
    @println(@isBool(false)); // true

    @println(@isBool(123)); // false
}
```

### `@isFunction`

Returns if value is a function or not.

```kirby
fun sum(a: i64, b: i64): i64 = a + b;

fun main(): unit {
    @println(@isFunction(@ceil)); // true
    @println(@isFunction(sum)); // true
    @println(@isFunction(123)); // false
}
```

### `@isNil`

Returns if value is a nil or not.

```kirby
fun main(): unit {
    @println(@isNil(nil)); // true
    @println(@isNil(123)); // false
}
```

### `@isNumber`

Returns if value is a number or not.

```kirby
fun main(): unit {
    @println(@isNumber(123)); // true
    @println(@isNumber("Hello World")); // false
}
```

### `@isString`

Returns if value is a string or not.

```kirby
fun main(): unit {
    @println(@isString("Hello World")); // true
    @println(@isString(123)); // false
}
```
