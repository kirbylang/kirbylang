<h1>kirby <small>programming language</small></h1>

<div>
<img src="https://github.com/kirbylang/kirbylang/actions/workflows/ci.yml/badge.svg" />
<img src="https://github.com/kirbylang/kirbylang/actions/workflows/release.yml/badge.svg" />
</div>

---

An aspiring embeddable scripting language.

```kirby
#!/usr/bin/env krb -f

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

print builder.toString();
```

## Documentation

- [Documentation](./wiki/README.md)
  - [Development](./docs/DEVELOPMENT.md)
  - [Change Log](./docs/CHANGELOG.md)
  - [Proposals](./docs/PROPOSALS.md)
  - [Types](./docs/TYPES.md)
  - [CLI](./docs/CLI.md)
- [Tests](./tests/README.md)
- [Scripts](./scripts/README.md)
- [Important Files](./docs/DEVELOPMENT.md#important-files)
- [Writing A Test](./tests/README.md#writing-a-test)
- [Create A Native Function](./docs/DEVELOPMENT.md#create-a-native-function)

## Learning Project

This is a highly modified implementation of clox, from the [Crafting Interpreters](https://craftinginterpreters.com/) book. I highly recommend the free online version then purchasing a physical copy.

This is also a learning project for me. Specifically to learn C (coming from a Rust/Typescript/Java background) and to explore implementing a type system.

## AI

Yes, AI (LLMs) haved been used to discuss how to implement the features/behaviors I want in the language. Especially in implementing them in C. I've written several other languages ([reqlang-expr](https://github.com/testingrequired/reqlang-expr), [locks](https://github.com/kyleect/locks), [egon](https://github.com/egonlang/egonlang)) or DSLs ([reqlang](https://github.com/testingrequired/reqlang)) in multiple languages without the use of AI. I even have a [template](https://github.com/kyleect/language-project-template) for working on parsing/language projects in Rust. Without the use of AI.

The "how" included introductions to memory arenas or how to approach converting the language from a single pass compiler (ala clox) to a multipass compiler. I could make such a refactor in Rust or Typescript but manual memory management is still a struggle.

Along the lines of "how", the [proposals](https://github.com/kirbylang/proposals) are drafted by AI but reviewed by me. As proposals are added or changed, other related proposals also get updated. The proposals (plus the actual implementation code) serve as context of where the language is going.

### What I Didn't Use It For

What didn't I use AI for? The documentation is written by me. Including this. All of the language's syntax, outside of the original clox framework, was designed by me. I knew/know what I want the language to look and feel like. It draws heavily from Rust and Typescript.

I also don't use agentic coding. All AI usage was conversation driven then producing a high level document when attempting to implement different concepts. I also try to implementing things myself as I'm doing this to learn C. When I've run in to roadblocks I've used AI to work through issues and why they are happening. Any code that is generated is reviewed by me and often rejected by me. I work hard to keep "hands on the wheel" at all times.

I understand many folks thoughts on any language model use. I'm not advocating anyone use this language, I'm just open sourcing my exploration in this space.
