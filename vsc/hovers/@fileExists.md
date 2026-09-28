Return if a file or path exists at a given path.

```kirby
let path = @prompt("File Path: ");

if (!@fileExists(path)) {
    @println("File doesn't exist '" + path + "'");
    @exit(1)
}

@println(path);
```
