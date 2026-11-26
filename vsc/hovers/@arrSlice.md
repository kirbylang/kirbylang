```kirby
native fun @arrSlice[T, const N](array: [T; N], start: integer, end: integer): [T];
```

Copies the items from a start index up to an end index of an array into a new list. The result is a list because its length isn't known when the program is compiled. `start` must be before `end`.

## Usage

```kirby
let a = [10, 20, 30, 40, 50];
let slice = @arrSlice(a, 1, 4);

@println(slice); // [20, 30, 40]
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#arrslice
