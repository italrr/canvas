canvas [~]
----------
`canvas` is a dynamic, high-level, JIT compiled, general purpose, scripting/programming language designed and developed as a extension interface or propotyping tool for scrappy solutions. It is a piece of software entirely of my own creation. The philosophy of `canvas` aims to be as simple as it can possibly be while also being as powerful as the user can make it out.

It's a language that was concieved when I was in college, and had had many different iterations, variations and rewrites. It's heavily inspired by other languages of my choise such as Python and JavaScript. There's also an small touch of Lisp into it.

## Features
- JIT Compiled
- No VM, but it's garbage collected by C++'s std::memory through smart use of contexts
- Statements are structured through the usage of brackets.
- Syntax is prefix notated
- Lists are first class citizens
- **No OOP**
- _Mostly_ immutable
- _Mostly_ Functional
- Recursion highly encouraged
- Standard Library (io, fs, net, bitmap, time, math)
- Error handling _a la_ C (checking return types). No try/catch
- Expessive yet simple Syntax
- Parallelism and concurrency as native features (WIP)

## Syntax

`canvas` syntax follows the next pattern pattern 100% of the time [STATEMENT0 ... STATEMENTN]. 

### Example of an addition
`+ 2 2` -> `4`

### Example of a list definition
`[1 2 3]` -> `[1 2 3]`

### Prefixers

Prefixers are special syntatic tools that pretend to increment expressiveness. One of the prefixers is the Positional Prefixer. It simplifies passing parameters to functions by name ignoring the order.

For example, let's say I have a "func" function that takes 2 arguments: param1 and param2. Boths of these arguments are defined as Positional. This means I can execute this function the following way

```func [:param2 VALUE2] [:param1 VALUE1]```

## Dependencies
canvas does not require any special dependency. It requires a C++11 capable compiler, preferably GCC (or MINGW), but other Unix-like compilers can work.

## How to build
- `cmake .`
- `make -j`
- `sudo make install`