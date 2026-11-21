<div>
<img src="https://github.com/kirbylang/kirbylang/actions/workflows/ci.yml/badge.svg" />
<img src="https://github.com/kirbylang/kirbylang/actions/workflows/release.yml/badge.svg" />
<img src="https://github.com/kirbylang/kirbylang/actions/workflows/docs.yml/badge.svg" />
</div>

![kirby programming language](./docs/kirby-logo.png)

🐈‍⬛ An aspiring embeddable scripting language.

```kirby
#!/usr/bin/env krb run

struct StringBuilder {
    value: [string];
}

impl StringBuilder {
    pub fun add(self, add: string): Self {
        @listPush(self.value, add);

        self
    }
}

impl Default for StringBuilder {
    fun default(): Self = Self { value: [] };
}

impl Display for StringBuilder {
    fun toString(self): string = @strConcat(self.value);
}

let builder = StringBuilder.default()
    .add("Hello")
    .add(" ")
    .add("World");

@println(builder);
```

## What Is It?

This is an interpreted language (bytecode VM) heavily inspired from my favorite parts of different languages.

## Documentation

- [Documentation](https://kirbylang.github.io/kirbylang/)
  - [Development](./docs/DEVELOPMENT.md)
  - [Change Log](./docs/CHANGELOG.md)
  - [Proposals](./docs/PROPOSALS.md)
  - [Types](./docs/TYPES.md)
  - [CLI](./docs/CLI.md)
  - [Example Kirby Project](./example_project/README.md)
- [Tests](./tests/README.md)
- [Scripts](./scripts/README.md)
- [Important Files](./docs/DEVELOPMENT.md#important-files)
- [Writing A Test](./tests/README.md#writing-a-test)
- [Create A Native Function](./docs/DEVELOPMENT.md#create-a-native-function)

## Learning Project

This is a learning project for me. It started as my introduction to C. I've used many other languages (Rust, Javascript/Typescript, C#, Java, Python, Ruby, PHP) but never having to deal with manual memory management.

This is a heavily modified version of the clox implementation, from the [Crafting Interpreters](https://craftinginterpreters.com/) book. I highly recommend checking out the (free) online version before purchasing a physical copy. It has beautiful illustrations.

On the language implementation side of things, this is my project for learning how to explore things like a type system, modules/namespaces, tooling like a linter and formatter.

My long term plan is to get the language in a good spot with types and modules before rewriting (by hand, no LLMs) in Rust. It's a langauge I'm very comfortable implementing langauges in at this point. Plus [E2E tests](./tests/README.md) can be used to validate the second implementation.

## AI Usage

Yes, AI was used in this project but it's more complicated than that.

https://kirbylang.github.io/kirbylang/#ai-usage
