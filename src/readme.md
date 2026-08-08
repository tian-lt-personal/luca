## LUCA — Language Reference

### Syntax (EBNF)

```ebnf
program       ::= expr

expr          ::= if-expr
                | lambda
                | bin-expr

if-expr       ::= "if" expr "then" expr "else" expr

lambda        ::= ("\" | "lambda") id ":" type "." expr

bin-expr      ::= apply { ("+" | "-" | "*" | "/") apply }    (* left-associative *)

apply         ::= unary { unary }               (* left-associative implicit application *)

unary         ::= "-" unary                     (* desugared to "0 - operand" *)
                | atomic

atomic        ::= id
                | integer
                | "true"
                | "false"
                | "(" expr ")"

type          ::= "int"
                | "bool"
                | "string"
                | "(" ")"                        (* unit type *)
```

#### Lexical Tokens

```ebnf
id            ::= [a-zA-Z_] [a-zA-Z0-9_-]*
integer       ::= [0-9]+
```

#### Operator Precedence

| Level | Construct              | Prec |
|-------|------------------------|------|
| 4     | Application            | 40   |
| 3     | Unary `-`              | 30   |
| 2     | `*` `/`                | 20   |
| 1     | `+` `-` (binary)       | 10   |

#### Notes

- The lambda body extends as far right as possible: `\x:int . x y` parses as `\x:int . (x y)`.
- `then`/`else` keywords delimit `if` branches; each branch is a full expression.
- Unary minus `-x` desugars to `0 - x`.
- Identifiers support hyphens: `foo-bar` is a single identifier.
- String literals (`"..."`) are lexed but not yet represented in the AST.
