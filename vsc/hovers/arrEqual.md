Shallow compares two arrays' values for equality.

```kirby
var a = [1, 2, 3];
var b = a;
var c = arrCopy(a);

print arrEqual(a, b); // true
print arrEqual(b, c); // false
print arrEqual(a, c); // false
```
