## LUCA — Language Reference

### Syntax (EBNF)

```ebnf
program       ::= term

term          ::= let-term
                | if-term
                | lambda
                | bin-term

let-term      ::= "let" id (":" type)? "=" term "in" term
                                                  (* desugars to (\id : type . term) term;
                                                     absent annotation: the bound expression's type *)
                | "let" "{" id { "," id } "}" "=" term "in" term
                                                  (* structured binding, by element position *)

if-term       ::= "if" term "then" term "else" term

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
- `fix` is a reserved word.
- Application requires a function in function position and an argument of the parameter type; both are rejected when known to be wrong.
- The type system is STLC + `fix` (the Y combinator): no polymorphism, no type variables. Lambda parameters must always be annotated. One position infers instead: `let x = E` takes E's type. Everything else is validated against explicit annotations.

### Product types (tuples)

- `(1, 2)` is a tuple literal with the product type `(int, int)`; `(1, true)` is a mixed tuple. Element types are inferred from the elements; a whole tuple can be annotated: `let p : (int, bool) = (1, true) in ...`.
- `()` is the unit literal with the unit type `()`. A single parenthesized value is just that value: `(1)` and `(int)` are `1` and `int`.
- Product types are structural and positional: `(int, bool)` matches any other `(int, bool)` element by element, in order.
- `let {a, b} = E in body` destructures a product by position: `a` is the first element, `b` the second. The arity must match exactly.
- There is no field access and no named record types — names are given where a tuple is used, via structured binding.

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
| B005 | expected a delimiter/punctuation (e.g. `)`, `:`, `.`, `=`, `in`) or a binding name |
| B006 | unexpected end of input |
| B007 | `fix` operand is not a lambda whose body is a lambda |
| C001 | unbound identifier |
| C002 | applying a value that is not a function |
| C003 | argument/parameter type mismatch in an application |
| C004 | `if` condition is not `bool` |
| C005 | `if` branches have different types |
| C007 | `fix` generator does not satisfy the fixpoint condition `τ = σ` |
| C008 | operator applied to a non-`int` operand |
| C009 | top-level expression has a function type |
| C015 | annotated `let` value does not match its annotation |
| C016 | structured binding target is not a product type |
| C017 | structured binding arity mismatch |
| C018 | duplicate name in a binding pattern |
