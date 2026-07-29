# The Anvil DSL

Anvil is a small C++-like language for writing stateless decision functions.

## Types

Locals and parameters may only have these types:

- `bool`
- `int`
- `int64_t`
- `double`

There is no `float`, no `struct`/`class`, no `auto` (except at the top-level lambda binding, see below), no `const` on locals, no `static`, no arrays, no pointers, no references. Every local declaration has exactly one declarator:

```cpp
double x = 1.0;    // ok
int64_t k = 0;     // ok
double y;          // ok (declared, initialized later)
int a, b;          // NOT allowed (multi-declarator)
```

Integer division of `int64_t` operands truncates as in C++.

## Literals

Integer (`3`, `-1`), floating (`1.5`, `1e-9`), and boolean (`true`, `false`).

## Operators

| Category   | Operators                            |
|------------|--------------------------------------|
| Arithmetic | `+  -  *  /  %`                      |
| Comparison | `<  <=  >  >=  ==  !=`               |
| Boolean    | `&&  ||  !` (only in conditions)     |
| Ternary    | `cond ? a : b`                       |
| Cast       | `static_cast<T>(x)` or C-style `(T)x` for the scalar types above |

Shifts (`>>`, `<<`) and bitwise ops (`&`, `|`, `^`) are NOT accepted. Use `x / 4` and `x * 4` (or `std::floor(x / 4.0)` for a truncating right shift on a `double`).

## Statements

- assignment `x = expr;`
- compound assignment `+=`, `-=`, `*=`, `/=`
- postfix as a full statement: `x++;` `x--;` (NEVER prefix `++x` / `--x`, NEVER inside another expression)
- `if (cond) { ... } else { ... }`
- `while (cond) { ... }`
- `for (int i = 0; i < LIT; i++) { ... }`
- `break;`, `continue;`
- `return expr;`

## Loop bounds must be integer literals

The bound in a `for` condition must be a bare integer literal — not a macro, not a local, not a conjunction.

```cpp
for (int i = 0; i < 16; i++) { ... }              // ok
for (int i = 0; i < N; i++) { ... }               // NOT ok (non-literal)
for (int i = 0; i < 16 && cond; i++) { ... }      // NOT ok (conjunction)
```

For an early exit, use a guarded `break;` at the top of the body:

```cpp
for (int k = 0; k < 8; k++) {
    if (k >= LIMIT) break;
    ...
}
```

## Lambda form

Bind the decision function to a top-level `auto` local. The lambda captures by reference; its parameters and return type are dictated by the surrounding scaffolding (see the task description for the exact signature).

```cpp
auto some_name = [&](FS_REF fs, ...) -> double {
    ...
    return ...;
};
```

## Math builtins

Available as free functions inside the body: `std::max(a, b)`, `std::min(a, b)`, `std::floor(x)`, `std::ceil(x)`, `std::pow(x, y)`, `std::log(x)`, `std::abs(x)`. Do NOT use `std::max<T>(a, b)` (explicit template arg is rejected) — cast the args first: `std::max(static_cast<double>(a), static_cast<double>(b))`.

## No cross-invocation state

The function is stateless — it may not carry information from one call to the next. No `static` locals, no globals, no writes to features from inside the body. If you need memory that persists across invocations (a countdown, cooldown, budget, EWMA, dwell time, …), express it as a **listener** on a declared feature and query it via `fs.get_*`. The scaffolding provides listener types (`RollingWindow`, `RollingCount`, `EWMA`, `MinMax`, `RollingPercentile`, `Average`, …); attach them once in the listener block, then read them inside the function.

## No preprocessor

Do NOT emit `#include` lines and do not define your own macros. The scaffolding pulls in every header it needs.
