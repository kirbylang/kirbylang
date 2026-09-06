## self

A reference to the instance an instance method was called on.

`self` is only available inside a method that declares it as its first
parameter -- a method without it is a static method and has no instance.

```kirby
struct Counter {
    var count = 0;
}

impl Counter {
    fun increment(self) {
        self.count = self.count + 1;
    }
}
```

## Self

The `Self` type and value is only avaible in struct implementation blocks. It represents the target struct.

```kirby
struct Counter {
    var count = 0;
}

impl Default for Counter {
    fun default(): Self = Self { count: 0 };
}
```
