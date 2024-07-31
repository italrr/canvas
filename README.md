# canvas [~]

canvas is dynamic, high level, general-purpose, programming language designed and developed by me. canvas is heavily inspired by JavaScript, LISP and Python. It features a form of JIT compiler to speed up execution.

canvas is:
- Prefix Notation.
- Lists are very important.
- No classes, no objects. OOP nada. Purely function and procedurally defined.
- Namespaces.
- Import/Library support.
- Mostly immutable. There are SOME exceptions. In theory only NUMBERs and STRINGs can be fully mutated (changing memory's content without altering IDs).
- Recursion highly encouraged.
- JIT compiled.
- Intended for scripting, but capable of general software development.
- Very basic garbage collection using C++'s reference counting through std::memory.
- Errors are handled by checking return types (nil, 1, 0 etc).
- Every statement must be within their brackets to avoid unintentional behavior.
- Parallelism and concurrency are first class citizens (WIP)

This is what a _hello world_ looks in canvas:
```
io:out 'Hello World!\n'
```

# Why?
It started as an experiment, or toy project. The original idea began when I was om colleged. I worked on it through the years on and off, constantly starting over. Recently finally managed to get something to show for. My current intention with it is to use it on my game engine (Caribbean Raster), but will eventually mature it enough for other people to use.

# Examples

### FizzBizz Example
```
[for i from 1 to 50 [
    [if [eq [math:mod i 3] 0][
        [io:out 'fizz ' i '\n']
    ]
        [if [eq [math:mod i 5] 0][
            [io:out 'buzz ' i '\n']
        ]]        
    ]
    [if [eq [math:mod i 15] 0][
        [io:out 'fizzbuzz ' i '\n']
    ]]     
]]
```