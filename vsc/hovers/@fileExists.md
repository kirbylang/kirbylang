```kirby
native fun @fileExists(path: string): bool;
```

Return if a file or path exists at a given path.

## Usage

```kirby
let path = @prompt("File Path: ");

if (!@fileExists(path)) {
    @println("File doesn't exist '" + path + "'");
    @exit(1)
}

@println(path);
```

## Reference

https://kirbylang.github.io/kirbylang/reference/builtin-functions.html#fileexists
