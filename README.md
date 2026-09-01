# kirby

[![Verify](https://github.com/kirbylang/kirbylang/actions/workflows/ci.yml/badge.svg)](https://github.com/kirbylang/kirbylang/actions/workflows/ci.yml)

[![Release](https://github.com/kirbylang/kirbylang/actions/workflows/release.yml/badge.svg)](https://github.com/kirbylang/kirbylang/actions/workflows/release.yml)

An aspiring embeddable scripting language.

```kirby
#!/usr/bin/env krb -f

// Function with an expression body
fun fizzbuzz(n: f64): string =
    if (n % 5 == 0 and n % 3 == 0) "FizzBuzz"
    else if (n % 3 == 0) "Fizz"
    else if (n % 5 == 0) "Buzz"
    else numberToString(n);

// Read from stdin
var limit: f64 = parseNumber(prompt("limit: "));

for (var i = 1; i <= limit; i = i + 1) {
    print fizzbuzz(i);
}
```

## Learning Project

This is a highly modified implementation of clox, from the [Crafting Interpreters](https://craftinginterpreters.com/) book. I highly recommend the free online version then purchasing a physical copy.

This is also a learning project for me. Specifically to learn C (coming from a Rust/Typescript/Java background).

## Documentation

- [Types](./docs/TYPES.md)
- [CLI](./docs/CLI.md)
- [Development](./docs/DEVELOPMENT.md)
- [Change Log](./docs/CHANGELOG.md)
- [Tests](./tests/README.md)
- [Scripts](./scripts/README.md)
- [Important Files](./docs/DEVELOPMENT.md#important-files)
- [Writing A Test](./tests/README.md#writing-a-test)
- [Create A Native Function](./docs/DEVELOPMENT.md#create-a-native-function)

## AI

Yes, AI (LLMs) haved been used to discuss how to implement the features/behaviors I want in the language. Especially in implementing them in C. I've written several other languages ([reqlang-expr](https://github.com/testingrequired/reqlang-expr), [locks](https://github.com/kyleect/locks), [egon](https://github.com/egonlang/egonlang)) or DSLs ([reqlang](https://github.com/testingrequired/reqlang)) in multiple languages without the use of AI. I even have a [template](https://github.com/kyleect/language-project-template) for working on parsing/language projects in Rust. Without the use of AI.

The "how" included introductions to memory arenas or how to approach converting the language from a single pass compiler (ala clox) to a multipass compiler. I could make such a refactor in Rust or Typescript but manual memory management is still a struggle.

### What I Didn't Use It For

What didn't I use AI for? The documentation is written by me. Including this. All of the language's syntax, outside of the original clox framework, was designed by me. I knew/know what I want the language to look and feel like. It draws heavily from Rust and Typescript.

I also didn't use agentic coding. All AI usage was conversation driven then producing a high level document when attempting to implement different concepts. I also try to implementing things myself as I'm doing this to learn C. When I've run in to roadblocks I've used AI to work through issues and why they are happening.
