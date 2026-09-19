Write a string to a file at a given path.

```kirby
var path = @prompt("File: ");
var text = @prompt("Text: ");

@writeStringToFile(path, text);
```
