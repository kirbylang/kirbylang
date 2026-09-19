Split a string into an array of strings at each place the separator appears. An empty separator splits the string into single character strings.

```kirby
print @strSplit("a,b,c", ","); // [a, b, c]
print @strSplit("abc", ""); // [a, b, c]
```
