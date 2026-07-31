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
