## Kirby

[Kirby](https://github.com/kirbylang/kirbylang) is a statically typed interpreted language. It's design was inspired by many programming languages I've used. Both the good (parts I wanted to include) and the bad (things I wanted to avoid).

- Rust
- Typescript
- Swift

## Special Thanks

A special thanks to [Robert Nystrom](https://stuffwithstuff.com/) for his [Crafting Interpreters](https://craftinginterpreters.com/) book. I can't recommend it enough. It's fully illustrated and how it lays out concepts is very intuitive.

Kirby started as the `clox` implementation in that book. It served as my introduction to the concept of bytecode and a stack based VM, as well as my introduction to C.

If you can, I highly recommend buying a hard copy as the illustrations are gorgerous.

## Learning Project

As mentioned above, this is a personal learning project.

On the language implementation side of things, this is my project to explore topics I haven't on my [previous](https://github.com/testingrequired/reqlang-expr) [language](https://github.com/kyleect/locks) [projects](https://github.com/testingrequired/reqlang).

Things like type systems (bounded parametric polymorphism, monomorphization, trade offs in those decisions), modules/namespaces, tooling like a linter and formatter.

## AI Usage

This is an up front disclosure that AI was used in the development of this project. This was not slop coded project. I fully implemented `clox` by following the book. My online handles and website is "Testing Required". Software quality is a core to me as a developer.

A vast majority of AI usage in this project has been conversational. A tool to explore what I know I want from the language. Agentic coding has gradulally increased over time (80:20 me/agent to 25:75 me/agent now) but it's hands on the wheel at all times. It's not an exageration to say my rejection rate in conversation is 5:1 and my rejection rate of AI written code (considering my edits of it's outputed code) is 30:1.

AI isn't writing code I wouldn't be writing myself. I don't auto approve any writing or editing files. Every edit is reviewed when I click approve or reject. This project has 3000+ E2E tests (and growing) + unit tests written or reviewed by me on every single commit.

This is also a learning project, not just interpreters, compilers, type systems but agentic coding. As a developer with 30+ years experience coding (personally and professionally), I am learning for myself what this technology means.

Finally, this language is not designed for production use. It's a learning project. I'm not advocating anyone using it outside of me. It's open sourced and MIT licensed because that's how I release all of my projects.

## First Steps

1. Download the [latest release](https://github.com/kirbylang/kirbylang/releases).
2. Install the [CLI](./cli.md).
3. Use the [REPL](./cli.md#repl) and explore the [basics](./basics.md).
4. Create a [project](./projects.md#new-project).

## Other Links

- [CHANGELOG.md](https://github.com/kirbylang/kirbylang/blob/main/docs/CHANGELOG.md)
