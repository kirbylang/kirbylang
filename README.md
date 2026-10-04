![kirby programming language](./docs/kirby-logo.png)

<div>
<img src="https://github.com/kirbylang/kirbylang/actions/workflows/ci.yml/badge.svg" />
<img src="https://github.com/kirbylang/kirbylang/actions/workflows/release.yml/badge.svg" />
</div>

---

An aspiring embeddable scripting language.

```kirby
#!/usr/bin/env krb run

struct StringBuilder {
    var value: Array;
}

impl StringBuilder {
    pub fun add(self, add: string): Self {
        @arrPush(self.value, add);

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

- [Documentation](./wiki/README.md)
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

## AI

### What Did I Use It For

Yes, AI (LLMs) haved been used to discuss how to implement the features/behaviors I want in the language. Especially implementing them in C. The "how" included introductions to memory arenas or how to approach converting the language from a single pass compiler (ala clox) to a multipass compiler. I could make such a refactor in Rust or Typescript but manual memory management is still a struggle.

Along the lines of "how", the [proposals](https://github.com/kirbylang/proposals) are drafted by AI but reviewed by me. As proposals are added or changed, other related proposals also get updated. The proposals (plus the actual implementation code) serve as context of where the language is going.

That doesn't mean there isn't generated code in the repo. There is code generated while refactoring code. Another example would be when I hit a wall trying to implement something and the generated guide provides code examples, that code has made it in directly or indirectly.

Any code that is generated is reviewed by me and rejected by me. I work hard to keep "hands on the wheel" at all times.

### What I Didn't Use It For

What didn't I use AI for? The documentation is written by me. Including this. All of the language's syntax, outside of the original clox framework, was designed by me. I knew/know what I want the language to look and feel like. It draws heavily from Rust and Typescript.

I also don't use agentic coding. All AI usage was conversation driven then producing a high level document when attempting to implement different concepts. I also try to implementing things myself as I'm doing this to learn C. When I've run in to roadblocks I've used AI to work through issues and why they are happening.

I've written several other languages ([reqlang-expr](https://github.com/testingrequired/reqlang-expr), [locks](https://github.com/kyleect/locks), [egon](https://github.com/egonlang/egonlang)) or DSLs ([reqlang](https://github.com/testingrequired/reqlang)) in multiple languages without the use of AI. I even have a [template](https://github.com/kyleect/language-project-template) for working on parsing/language projects in Rust. Without the use of AI.

I understand many folks thoughts on any language model use. I'm not advocating anyone use this language, I'm just open sourcing my exploration in this space.
