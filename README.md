canvas [~]
----------
`canvas` is a dynamic, high-level, JIT compiled, general purpose, scripting/programming language designed and developed as an extension interface or propotyping tool for scrappy solutions. The philosophy of `canvas` is to aim to be as simple as it can possibly be while also being as powerful as the user can make it out.

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
- Recursion highly encouraged through 
- Standard Library (io, fs, net, bitmap, time, math)
- Error handling _a la_ C (checking return types). No try/catch
- Expessive yet simple Syntax
- Parallelism and concurrency as native features (WIP)

## Syntax

`canvas` follows a hierarchy of brackets to organize instructions/statements. The hierarchy goes like this:

```
[ []...[] -> STATEMENT(S)/INSTRUCTION(S) ] -> PROGRAM

```

In short, the "program" itself must be within brackets, and statements themselves, must be within brackets. The interpreter/compiler is very relaxed around this, as it would try to fix informal or incomplete `canvas` code to make it work. However, the user must be cautious since there can cases of ambiguity for the interprer/compiler and the user's expectations.

```
canvas[~] v1.0.0 X86_64 [WINDOWS] released in Oct. 1st 2025
>>>>> RELAXED <<<<<
[~]> + 1 1                  # Interpreted as [[+ 1 1]]
2
[~]> [+ 2 2]                # Interpreted as [[+ 2 2]]
4
[~]> [[+ 3 4]]              # Interpreted as it is
7
[~]> [[[+ 2 2 2 24]]]       # Anything above 2 brackets for a single instruction is automatically reducted to its formal form
30
[~]> [[[[+ 1 2 3 4 5]]]]    # In this case, the added double brackets tells the interpreter this can be treated as a list 
[15]
[~]> [+ 2 2][+ 10 11]       # Case of ambiguity. This is interpreted as a set of instruction
21
[~]> [[+ 2 3][+ 4 5] 2]     # Same as before
2
[~]> [[[+ 2 3][+ 4 5] 2]]   # However, proper bracketing yields the intended result
[5 9 2]
```

The takeaway is that the user is highly encouraged to be explicit when needed to avoid unexpected behavior.

### Prefixers

Prefixers are special syntactic tools that pretend to increase expressiveness. One of the prefixers is the Namer (~). What the namer does is, it gives a name/symbol to a type within the current context.

## Namer

As previously mentioned, the namer simply defines a name for us to refer to a type. The namer is usually used in Store definitions and Positional Parameters

### Explicit Store definition
```
[~]> [[b:store [~j 200]]]
[[~j 200]]
```

### Implicit Stores
```
[~]> [[[~n 10][~k 25.5]]] # Similar to lists, if we define items in what could be an implicit array, but these are named,
                          # it interprets it as a Store definition.
[[~k 25.5] [~n 10]]
```

### Positional Parameter / Let
For example, let's say I have a "func" function that takes 2 paramters: param1 and param2, in that order. I can execute this function the following way

```func [~param2 VALUE2] [~param1 VALUE1]```

Function invocations create a context on the fly, so the interpreter allows me to fulfill a parameter requirenment by giving a name to type during runtime.

The Namer can also be used as a replacement for `let`:

```
[~var 19200] # This defines a name/symbol called `var` that points the type number `19200`.
```

It doesn't work as well for `mut`, because canvas doesn't really deal with data in names, but references, so even though you might replace a variable's name with another type, the created references will still point to the original value. This is because Namer is mostly a runtime instruction.

## Dependencies
`canvas` does not require any special dependency. It only requires a C++11 capable compiler, preferably GCC (or MINGW), but other Unix-like compilers can work. Visual Studio Compiler wouldn't compile without some work as `canvas` expects a Unix-like compiler/environment. For Windows it's highly recommended using MINGW.

## How to build
- `cmake .`
- `make -j`
- `sudo make install`