```kirby
native fun @writeStringToFile(path: string, contents: string): unit;
```

Write a string to a file at a given path.

## Usage

```kirby
var path = @prompt("File: ");
var text = @prompt("Text: ");

@writeStringToFile(path, text);
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#writestringtofile
