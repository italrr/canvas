# canvas [~]

canvas is dynamic, high level, general-purpose, programming language, designed and developed by me. canvas is heavily inspired by JavaScript, LISP and Python. It features a form of JIT compiler to speed up execution. It comes with a couple of fundamental Standard Libraries such as `fs` File System and `core/std` signal handling.

canvas was created to be used as the extension language for my own projects. It still is in heavy development.

canvas is:
- Prefix Notation. `[STATEMENT ARGUMENT0...ARGUMENTN]`
- Brackets used to separate statements. 
- Lists are very important. `[1 2 3]`
- `Store` entities that work similar to JSON objects. `store a:5 b:10`
- No OOP. Stores cannot contain functions, only data. The language actively discourages any type of OOP convention.
- Namespaces used to organize hierarchies. `[namespace a:100 b:[fn [n][+ n n]]]`
- Import/Library support. Compiled/Binary functions/libraries also possible with some limitations.
- Mostly immutable. There are SOME exceptions. In theory only NUMBERs and STRINGs can be fully mutated (changing memory's content without altering IDs).
- Recursion highly encouraged.
- JIT compiled.
- Intended for scripting, but capable of general software development.
- Very basic garbage collection using C++'s reference counting through std::memory (expect some leaks until v1.0.0).
- Errors are handled by checking return types (nil, 1, 0 etc).
- Standard Library!
- Parallelism and concurrency are first class citizens (TBD/WIP)

This is what a _hello world_ looks in canvas:
```
io:out 'Hello World!\n'
```

Simple arithmetic operations using any number of real numbers:
```
+ 2 2
[* 2 [+ 2 2]]
/ 100 2 2 2
```

# Why?
It started as an experiment, or toy project. The original idea began when I was in colleged. I worked on it through the years on and off, constantly starting over. Recently finally managed to get something to show for. My current intention with it is to use it on my game engine (Caribbean Raster), but will eventually mature it enough for other people to use.

# Standard Library
- `math`: Includes general math function such as `math:sin`, `math:PI`.
- `time/tm`: It includes time related function such as `tm:epoch` (milliseconds since epoch).
- `bm`: It's a very basic Bitmap manipulation library.
- `core`: It includes a couple of generic, core and standard functions such as `std::exit`.
- `fs`: It includes File System functions.
- `io`: Related to terminal functions such as `out`, `err` and `in`. It's embedded in the binary (no need to import).

The Standard Library is in still in heavy development.

# How to Build?
canvas is very simple and thus easy to build. It only really needs a C++11 compiler, pthreads and CMake. It however was built around GCC, and depends on Unix/Linux and related `std`. You should be able to build it on Windows by using MinGW easily, VC++ would require some work.

### Building on LINUX:
Simply:
```
cmake .
make -j
```

### Building on WINDOWS:
Using MinGW & Window's CMAKE:
```
cmake . -G "MinGW Makefiles"
make -j
```

# TODO
- Further stability testing.
- Improve GC. canvas is probably wasting *a lot* of memory right now.
- Further complete Standard Library.
- Add innate concurrency/parralelism interface (Including thread safetiness tools such as atomics, sempahores, etc).
- Improve error handling and checking.

# Examples

Checkout `./examples/`

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