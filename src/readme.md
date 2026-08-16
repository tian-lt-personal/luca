## LUCA — Language Reference

### Syntax (EBNF)

```ebnf
program       ::= term

term          ::= let-term
                | if-term
                | lambda
                | bin-term

let-term      ::= "let" id "=" term "in" term        (* desugars to (\id : typeof(term) . term) term *)

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
                | "(" term ")"

type          ::= "int"
                | "bool"
                | "string"
                | "(" ")"                              (* unit type *)
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
- `let x = E1 in E2` desugars to `(\x : typeof(E1) . E2) E1`; the type of E1 must be deducible by Sema.
- `let` expressions right-associate: `let x = 1 in let y = 2 in x + y`.
- Identifiers support hyphens: `foo-bar` is a single identifier.
- String literals (`"..."`) are lexed but not yet represented in the AST.
- `fix (\f : τ . \x : σ . body)` is the fixed-point operator: the generator must have type `τ -> σ` with `τ` equal to `σ`, and the whole expression has type `σ`. The recursion variable is the generator's outermost parameter (`f`).
- The `fix` operand must be a lambda whose body is a lambda (a "function generator").
- `fix` is a reserved word.
- Application requires a function in function position and an argument of the parameter type; both are rejected when known to be wrong.
