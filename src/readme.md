## LUCA — Language Reference

### Syntax (EBNF)

```ebnf
program       ::= { type-decl } term

type-decl     ::= "type" id "=" record-type

term          ::= let-term
                | if-term
                | lambda
                | bin-term

let-term      ::= "let" id ":" type "=" term "in" term
                                                  (* desugars to (\id : type . term) term *)
                | "let" "{" id { "," id } "}" "=" term "in" term
                                                  (* structured binding, by field order *)

if-term       ::= "if" term "then" term "else" term

lambda        ::= ("\" | "lambda") id ":" type "." term

bin-term      ::= apply { ("+" | "-" | "*" | "/" | "=" | "!=" | ">" | "<") apply }    (* left-associative *)

apply         ::= unary { unary }                     (* left-associative implicit application *)

unary         ::= "-" unary                           (* desugared to "0 - operand" *)
                | "fix" lambda
                | postfix

postfix       ::= atomic { "." id }                   (* field access, binds tighter than application *)

atomic        ::= id
                | integer
                | "true"
                | "false"
                | "(" term ")"
                | tuple-lit

tuple-lit     ::= "{" field { "," field } "}"

field         ::= id ":" type "=" term

type          ::= "int"
                | "bool"
                | "string"
                | "(" ")"                              (* unit type *)
                | "(" type ")"
                | id                                   (* name declared by "type" *)
                | record-type
                | type "->" type                       (* right-associative *)

record-type   ::= "{" id ":" type { "," id ":" type } "}"
```

#### Lexical Tokens

```ebnf
id            ::= [a-zA-Z_] [a-zA-Z0-9_-]*
integer       ::= [0-9]+
```

#### Operator Precedence

| Level | Construct              | Prec |
|-------|------------------------|------|
| 6     | Field access `.`       | 60   |
| 5     | Application            | 50   |
| 4     | Unary `-`              | 40   |
| 3     | `*` `/`                | 30   |
| 2     | `+` `-` (binary)       | 20   |
| 1     | `=` `!=` `>` `<`       | 10   |

#### Notes

- The lambda body extends as far right as possible: `\x:int . x y` parses as `\x:int . (x y)`.
- `then`/`else` keywords delimit `if` branches; each branch is a full expression.
- Unary minus `-x` desugars to `0 - x`.
- `let x : T = E1 in E2` desugars to `(\x : T . E2) E1`; the type annotation is required and E1 is checked against it (a record literal with matching fields is accepted — see below).
- `let` expressions right-associate: `let x : int = 1 in let y : int = 2 in x + y`.
- Identifiers support hyphens: `foo-bar` is a single identifier.
- String literals (`"..."`) are lexed but not yet represented in the AST.
- `fix (\f : τ . \x : σ . body)` is the fixed-point operator: the generator must have type `τ -> σ` with `τ` equal to `σ`, and the whole expression has type `σ`. The recursion variable is the generator's outermost parameter (`f`).
- The `fix` operand must be a lambda whose body is a lambda (a "function generator").
- `fix` is a reserved word.
- Application requires a function in function position and an argument of the parameter type; both are rejected when known to be wrong.
- There is no type inference: every type is written explicitly (lambda parameters, `let` annotations, record fields, `fix`), and the type checker only validates annotations.

### Records and user-defined types

- `{x:int = 1, y:bool = false}` is a record literal with an *anonymous* record type `{x:int, y:bool}`; every field states its type explicitly. Fields are read with `p.x`.
- `type point = {x:int, y:bool}` (before the expression, referencing only earlier declarations) declares a **nominal** record type `point`, distinct from the anonymous `{x:int, y:bool}` — there are no implicit conversions between named and anonymous record types.
- A record *literal* can initialize a declared record type where that type is expected (an application argument, an annotated `let`, or a field with that annotation), as long as the fields match by name and type — in any order, reordered to the declaration order. Non-literal values must have the exact same type:
  - `(\p : point . p.x) {x:int = 1, y:bool = true}` ✓
  - `let p : point = {y:bool = false, x:int = 1} in ...` ✓
  - `let q : {x:int, y:bool} = {x:int = 1, y:bool = false} in (\p : point . p.x) q` ✗ (`q` has the anonymous type)
- `let {a, b} = E in body` destructures a record by field order: `a` is the first field, `b` the second. The arity must match.
- `p.x` binds tighter than application: `f p.x` is `f (p.x)`; write `(f p).x` for the other reading.
- `type` is a reserved word.

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
| B005 | expected a delimiter/punctuation (e.g. `)`, `:`, `.`, `=`, `in`) or a field/binding name |
| B006 | unexpected end of input |
| B007 | `fix` operand is not a lambda whose body is a lambda |
| B008 | empty tuple `{}` in a literal or type position |
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
| C012 | field access on a value that is not a record |
| C013 | record has no such field |
| C014 | duplicate field name in a literal or type declaration |
| C015 | value cannot initialize the expected type |
| C016 | structured binding target is not a record |
| C017 | structured binding arity mismatch |
| C018 | duplicate name in a binding pattern |
