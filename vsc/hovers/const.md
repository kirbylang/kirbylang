Declare an array length as a parameter. Only the standard library can use `const`.

`const N` stands for a length in the same way `T` stands for a type.

```kirby
impl[T, const N] [T; N] {
    pub fun copy(self): [T; N] = @arrCopy(self);
}
```
