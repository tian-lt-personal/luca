## LUCA — Language Reference

### Syntax (EBNF)

```ebnf
program       ::= { type-decl } term

type-decl     ::= "type" id "=" constructor { "|" constructor }
constructor   ::= id                                   (* nullary: Zero *)
                | id "of" type                         (* payload: Num of int, Add of (expr, expr) *)

term          ::= let-term
                | if-term
                | match-term
                | lambda
                | bin-term

let-term      ::= "let" id (":" type)? "=" term "in" term
                                                  (* desugars to (\id : type . term) term;
                                                     absent annotation: the bound expression's type *)
                | "let" "{" id { "," id } "}" "=" term "in" term
                                                  (* structured binding, by element position *)

if-term       ::= "if" term "then" term "else" term

match-term    ::= "match" term "with" match-arm { "|" match-arm }
match-arm     ::= pattern "." term
pattern       ::= id                                   (* nullary constructor *)
                | id id                                (* single payload: Num n *)
                | id "(" id { "," id } ")"             (* product payload: Add (l, r) *)

lambda        ::= ("\" | "lambda") id ":" type "." term

bin-term      ::= apply { ("+" | "-" | "*" | "/" | "=" | "!=" | ">" | "<") apply }    (* left-associative *)

apply         ::= unary { unary }                     (* left-associative implicit application *)

unary         ::= "-" unary                           (* desugared to "0 - operand" *)
                | "fix" lambda
                | atomic

atomic        ::= id
                | integer
                | "true"
                | "false"
                | "(" term ")"                         (* parenthesized expression *)
                | tuple-lit

tuple-lit     ::= "(" ")"                              (* unit *)
                | "(" term "," term { "," term } ")"   (* product literal *)

type          ::= "int"
                | "bool"
                | "string"
                | "(" ")"                              (* unit type *)
                | "(" type ")"
                | product-type
                | id                                   (* name declared by "type" *)
                | type "->" type                       (* right-associative *)

product-type  ::= "(" type "," type { "," type } ")"
```

#### Lexical Tokens

```ebnf
id            ::= [a-zA-Z_] [a-zA-Z0-9_-]*
integer       ::= [0-9]+
```

#### Operator Precedence

| Level | Construct              | Prec |
|-------|------------------------|------|
| 5     | Application            | 50   |
| 4     | Unary `-`              | 40   |
| 3     | `*` `/`                | 30   |
| 2     | `+` `-` (binary)       | 20   |
| 1     | `=` `!=` `>` `<`       | 10   |

#### Notes

- The lambda body extends as far right as possible: `\x:int . x y` parses as `\x:int . (x y)`.
- `then`/`else` keywords delimit `if` branches; each branch is a full expression.
- Unary minus `-x` desugars to `0 - x`.
- `let x : T = E1 in E2` desugars to `(\x : T . E2) E1`; the type annotation is optional — absent, `x` gets E1's inferred type (monomorphic: no generalization). When written, E1 is checked against it.
- `let` expressions right-associate: `let x : int = 1 in let y : int = 2 in x + y`.
- Identifiers support hyphens: `foo-bar` is a single identifier.
- String literals (`"..."`) are lexed but not yet represented in the AST.
- `fix (\f : τ . \x : σ . body)` is the fixed-point operator: the generator must have type `τ -> σ` with `τ` equal to `σ`, and the whole expression has type `σ`. The recursion variable is the generator's outermost parameter (`f`).
- The `fix` operand must be a lambda whose body is a lambda (a "function generator").
- `fix`, `type`, `of`, `match` and `with` are reserved words.
- Application requires a function in function position and an argument of the parameter type; both are rejected when known to be wrong.
- The type system is STLC + `fix` (the Y combinator): no polymorphism, no type variables. Lambda parameters must always be annotated. One position infers instead: `let x = E` takes E's type. Everything else is validated against explicit annotations.

### Product types (tuples)

- `(1, 2)` is a tuple literal with the product type `(int, int)`; `(1, true)` is a mixed tuple. Element types are inferred from the elements; a whole tuple can be annotated: `let p : (int, bool) = (1, true) in ...`.
- `()` is the unit literal with the unit type `()`. A single parenthesized value is just that value: `(1)` and `(int)` are `1` and `int`.
- Product types are structural and positional: `(int, bool)` matches any other `(int, bool)` element by element, in order.
- `let {a, b} = E in body` destructures a product by position: `a` is the first element, `b` the second. The arity must match exactly.
- There is no field access and no named record types — names are given where a tuple is used, via structured binding.

### Variant types (recursive sums)

- `type shape = Circle of int | Square | Rect of (int, int)` declares a **variant type**: a named sum of constructors, each with an optional payload type (`of`). A constructor with no payload is *nullary*.
- Constructors build values: `Circle 5`, `Rect (3, 4)`, `Square`. A payload is a single value of the payload type (a product payload is one tuple: `Add (e1, e2)`).
- A type can reference **itself** in its own declaration: `type expr = Num of int | Add of (expr, expr)` — recursive variants. There are no forward references or mutual recursion in v1.
- `match e with C1 x . e1 | C2 . e2 | C3 (a, b) . e3` consumes a variant: the scrutinee must have a declared variant type, every constructor must appear **exactly once** (in any order), and the arms must produce the same result type. A single pattern name binds the whole payload; `(a, b)` binds the elements of a product payload.
- Variant types are **nominal**: `shape` equals `shape` (by name) and nothing else — a `shape` value is not interchangeable with an anonymous `(int, bool)` or a different declared type.
- Constructor names are globally unique and may not be shadowed by `let`/lambda/binding names.
- A nested `match` inside a non-final arm body needs parens (the `|` separators would otherwise bind to the inner match).

### Compilation pipeline (two passes)

Parsing runs in two passes:

1. **Pass 1 — syntax + semantics**: builds the full AST with every check (unbound names, types, `match` exhaustiveness, ...) and resolves names to de Bruijn indices. `let`, structured-binding, and `match` desugarings happen here. All diagnostics come from this pass.
2. **Pass 2 — AST optimization** (semantics-preserving):
   - **Unused-binding elimination (tree-shaking)**: `(\x : T . body) e` — what `let x : T = e in body` desugars to — is replaced by `body` when `x` is never referenced, and the surviving free-variable de Bruijn indices are renumbered. Unused `let` bindings and lambda parameters are dropped; a partially used structured binding keeps only the projections it needs (`let {a, b} = p in b` keeps `b` and drops `a`).
   - **Constant folding**: `1 + 2` → `3`, `8 / 2` → `4`, `1 = 2` → `false` (the result literal has the operator's result type, `int` vs `bool`). Division by zero is never folded.
   - **Dead-branch elimination**: `if true then A else B` → `A` (a literal condition).

Pass 2 never changes behavior — the language is pure, so a dropped expression is unobservable — it only removes work. `lucacli -d` dumps the *optimized* tree. Match-arm payload closures and `fix` bodies keep their shapes (the machine applies arm bodies to the payload and expects `fix`'s body to be a lambda).

### Diagnostics

Compilation errors are reported fail-fast, one at a time, in a clang-style format. The position is computed from byte offsets (1-based line and column):

```
bad.luca:2:3: error: cannot pass value of type 'bool' to parameter of type 'int'
  f true
    ^~~~
hint: pass an argument of the parameter type
```

Error codes: `A*` for lexing errors, `B*` for parsing errors, `C*` for sema (type) errors.

| Code | Meaning |
|------|---------|
| A001 | unexpected character |
| A002 | unterminated string literal |
| A003 | identifier starting with a digit |
| B001 | unexpected token in expression position |
| B002 | expected a type |
| B003 | expected an identifier after `\` |
| B004 | expected an identifier after `let` |
| B005 | expected a delimiter/punctuation (e.g. `)`, `:`, `.`, `=`, `in`, `with`) or a binding/constructor name |
| B006 | unexpected end of input |
| B007 | `fix` operand is not a lambda whose body is a lambda |
| B009 | malformed `type` declaration |
| C001 | unbound identifier |
| C002 | applying a value that is not a function |
| C003 | argument/parameter type mismatch in an application |
| C004 | `if` condition is not `bool` |
| C005 | `if` branches have different types |
| C007 | `fix` generator does not satisfy the fixpoint condition `τ = σ` |
| C008 | operator applied to a non-`int` operand |
| C009 | top-level expression has a function type |
| C010 | duplicate type declaration |
| C011 | unknown type name in a type annotation |
| C015 | annotated `let` value does not match its annotation |
| C016 | structured binding target is not a product type |
| C017 | structured binding arity mismatch |
| C018 | duplicate name in a binding pattern |
| C019 | duplicate constructor name |
| C020 | unknown constructor in a `match` pattern |
| C021 | `match` scrutinee is not a variant value |
| C022 | `match` is not exhaustive, repeats a constructor, or has a mismatched payload pattern |
| C023 | `match` arms have different result types |
| C024 | constructor argument does not match its payload type |
| C025 | binding a constructor name |
