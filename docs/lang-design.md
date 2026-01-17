# Lexical conventions

## Tokens

$token = keyword~|~operator~|~specifier~|~identifier~|~literal$

## Keywords

$keyword$ can be one of the following items:
- `if`
- `then`
- `else`
- `curexpr`

## Operators

$operator$ can be one of the following items:
- `(` : the opening parenthesis.
- `)` : the closing parenthesis.
- `->` : property resolver.
- `+`
- `-`
- `*`
- `/`
- `%`
- `:`

## Specifiers

$specifier$ can be one of the following items:
- `.` : forms a lambda expression, such as `x.x`, `x.x + 1`, `x.y. x + y`.

## Operators

# Expressions

$epxr = term~|~ifstatement~|~arithmetic~|~application$  
$abstraction = var. expr$  
$application = expr~expr$  
$term = id~|~value~|~abstraction~|~\text{(}expr\text{)}$  
$arithmetic = expr~binary\_operator~expr~|~unary\_operator~expr $

Examples:

- `x.x 1` evaluates to `1`.
- `x.x + 1 1` is equivalent to `(x.x + 1) 1`, which evaluates to `2` eventually.
- `x.y.x + y 2 3` evaluates to `5` eventually.
  - step 1: `y.2 + y 3` (non-terminal expression).
  - step 2: `2 + 3` (non-terminal expression).
  - step 3: `5`

---
