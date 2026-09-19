Get the part of a string from `start` up to, but not including, `end`. Both are whole positions from 0 to the length of the string. It is an error if either is outside the string, is not a whole number, or if `start` is after `end`.

```kirby
print @strSlice("hello world", 0, 5); // hello
print @strSlice("hello world", 6, 11); // world
```
