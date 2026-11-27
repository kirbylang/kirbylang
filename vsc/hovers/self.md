## self

A reference to the value an instance method was called on.

`self` is only available inside a method that declares it as its first
parameter. A method without it is a static method and has no `self`.

```kirby
struct Counter {
    var count: i64;
}

impl Counter {
    pub fun increment(self): unit {
        self.count = self.count + 1;
    }
}
```

## Self

The `Self` type and value stands for the type an `impl` block is for. In a trait's
method signatures it is the type that implements the trait.

```kirby
struct Counter {
    var count: i64;
}

impl Default for Counter {
    fun default(): Self = Self { count: 0 };
}
```
