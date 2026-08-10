# LUCA Language Support

Syntax highlighting and bracket matching for the [LUCA](https://github.com/tian/luca) programming language — a minimal functional language with lambda calculus at its core.

## Features

- **Syntax highlighting** — keywords, types, string literals, numbers, operators, and identifiers with embedded hyphens
- **Bracket matching** — auto-closing and surrounding pairs for `()`
- **Language configuration** — associates `.luca` files with the LUCA language mode

## Language Overview

LUCA is a statically-typed functional language with:

- Lambda abstractions: `\x : int . x` or `lambda x : int . x`
- Arrow types: `\f : int -> int . f 5`
- Let bindings: `let x = 1 in x + x`
- Conditionals: `if x < 0 then 0 - x else x`
- Implicit application: `f x y`
- De Bruijn index resolution

```luca
let inc = \x : int . x + 1 in
let add = \x : int . \y : int . x + y in
let max = \a : int . \b : int .
  if a > b then a else b in
max (inc 5) (add 3 4)
```

## Requirements

No dependencies — the extension works in any VS Code `^1.125.0` or newer.

## Release Notes

See [CHANGELOG.md](./CHANGELOG.md).
