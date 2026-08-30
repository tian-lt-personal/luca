## LUCA


### Prerequisites

- C++23 compiler
- CMake 3.20 or higher - A cross-platform build system generator.
- [vcpkg](https://github.com/microsoft/vcpkg) - A C++ package manager to install dependencies.  
  Set environment variable `VCPKG_ROOT` to the vcpkg installation path.
- re2c - Regular Expressions to Code.  
  (For Windows) You can install it via Chocolatey: `choco install re2c` or download it from [re2c.org](https://re2c.org/).

###  Building the Project

For msvc debugging, you can use the following commands:
```
cd src
cmake --preset vsdbg
cmake --build --preset vsdbg
```

### Language at a glance

A pure, statically typed functional language: lambda abstractions, `let` bindings, tuples, variant types with `match`, the fixed-point operator `fix`, and cross-file modules.

```luca
// functions and let bindings
let square = \x:int. x * x in
let sum-of-squares = \a:int. \b:int. square a + square b in
sum-of-squares 3 4
// => 25
```

```luca
// recursion via fix
let fact = fix (\f:int -> int. \n:int.
  if n < 2 then 1 else n * f (n - 1)) in
fact 10
// => 3628800
```

```luca
// tuples and structured binding
let p = (1, true) in
let {a, b} = p in
if b then a + 1 else a
// => 2
```

```luca
// recursive variant types and match
type expr = Num of int | Add of (expr, expr)

let eval = fix (\eval:expr -> int. \e:expr.
  match e with
    Num n . n
  | Add (l, r). eval l + eval r) in
eval (Add (Num 1, Add (Num 2, Num 3)))
// => 6
```

```luca
// modules: export bindings in one file, import them in another
// shapes.luca
type shape = Circle of int | Square | Rect of (int, int)
export let area = \s:shape. match s with
    Circle r . 3 * r * r
  | Square . 100
  | Rect (w, h). w * h
in ()

// main.luca
import "shapes.luca" in
area (Rect (3, 4))
// => 12
```

Run any of these with `luca file.luca`.

### Language Reference
For the full syntax reference, see [Language Reference](src/readme.md).

