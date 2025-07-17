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
- _Mostly_ immutability
- _Mostly_ Functional
- Recursion highly encouraged
- Standard Library (io, fs, net, bitmap, time, math)
- Error handling _a la_ C (checking return types). No try/catch
- Parallelism and concurrency as native features (WIP)

## Dependencies
canvas does not require any special dependency. It requires a C++11 capable compiler, preferably GCC (or MINGW), but other Unix-like compilers can work.

## How to build
- `cmake .`
- `make -j`
- `sudo make install`