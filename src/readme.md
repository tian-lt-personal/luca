## LUCA — Language Reference

### Syntax (EBNF)

```ebnf
program       ::= expr

expr          ::= let-expr
                | if-expr
                | lambda
                | bin-expr

let-expr      ::= "let" id "=" expr "in" expr        (* desugars to (\id : typeof(expr) . expr) expr *)

if-expr       ::= "if" expr "then" expr "else" expr

lambda        ::= ("\" | "lambda") id ":" type "." expr

bin-expr      ::= apply { ("+" | "-" | "*" | "/" | "=" | "!=" | ">" | "<") apply }    (* left-associative *)

apply         ::= unary { unary }                     (* left-associative implicit application *)

unary         ::= "-" unary                           (* desugared to "0 - operand" *)
                | atomic

atomic        ::= id
                | integer
                | "true"
                | "false"
                | "(" expr ")"

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
