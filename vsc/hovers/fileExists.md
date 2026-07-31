Return if a file or path exists at a given path

```kirby
var path = prompt("File Path: ");

if (!fileExists(path)) {
    print "File doesn't exist '" + path + "'";
    exit(1)
}

print path;
```
