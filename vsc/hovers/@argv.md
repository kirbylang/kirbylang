Access the arguments passed to the program by index. `@argv(0)` is the program
and `@argv(1)` is the first argument. It is `nil` past the last one.

```kirby
@println(@argv(1));
```
