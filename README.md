canvas [~]
----------
`canvas` is a dynamic, high-level, JIT compiled, general purpose, scripting/programming language designed and developed as an extension interface or propotyping tool for scrapy solutions. The philosophy of `canvas` is to aim to be as simple as it can possibly be while also being as powerful as the user can make it out while remaining highly flexible.

It's a language that was concieved when I was in college, and had had many different iterations, variations and rewrites. It's heavily inspired by other languages of my choise such as Python and JavaScript. There's also an small touch of Lisp into it.

## Features
- JIT Compiled
- No VM, but it's garbage collected by C++'s std::memory through smart use of contexts
- Statements are structured through the usage of brackets.
- Syntax is prefix notated
- Lists and Stores (a type of dictionary) are first class citizens
- **No OOP**
- Basic types are NUMBER, STRING, LIST, STORE and FUNCTION
- _Mostly_ immutable
- _Mostly_ Functional
- Recursion highly encouraged
- Standard Library (io, fs, net, bitmap, time, math)
- Error handling _a la_ C (checking return types). No try/catch
- Expessive yet simple Syntax
- Native parallelism

Note regarding **No OOP**: While you experiment with canvas, you might find that you can in fact store Functions into Stores, however it won't let you execute them _from_ the store. You need to "take them out" of the Store for you use to them.
```
[~]> [[let test [b:store [~n 10] [~k [fn [a b][+ a b]]]]]]
[[~k [fn [a b] [+ a b]]] [~n 10]]
[~]> [[[test ~k] 1 2]]
[[fn [a b] [+ a b]] 1 2]
...
[~]> [[let f [test ~k]]]
[fn [a b] [+ a b]]
[~]> f 1 2
3
```

## Syntax

`canvas` follows a hierarchy of brackets to organize instructions/statements. The hierarchy goes like this:

```
"[ []...[] -> STATEMENT(S)/INSTRUCTION(S) ]" -> PROGRAM

```

In short, the "program" itself must be within brackets, and statements themselves, must be within brackets. The interpreter/compiler is very relaxed around this, as it would try to fix informal or incomplete `canvas` code to make it work. However, the user must be cautious since there can be the occasional case of ambiguity for the interprer/compiler and the user's expectations.

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
[~]> [+ 2 2][+ 10 11]       # Case of ambiguity. This is interpreted as a set of instruction, an addition and then another addition
21
[~]> [[+ 2 3][+ 4 5] 2]     # Same as before. An addition, then another addition, and a symbol thant returns itself (2)
2
[~]> [[[+ 2 3][+ 4 5] 2]]   # However, proper bracketing yields the intended result
[5 9 2]
```

The takeaway is that the user is encouraged to be explicit when needed to avoid unexpected behavior. The reason canvas is this way it's because I wanted to make it powerful, flexible, and not too cumbersome. JavaScript suffers from ambiguity causing unintended results as well, as a side-effect of its flexibility. I understand some people do not welcome this kind of quirk. But I personally find it not so much of a problem. If this is a deal breaker for you, canvas is definitely not the tool you want. 

## Prefixers

Prefixers are special syntactic tools that strive to increase expressiveness. One of the prefixers is the Namer (~). What the namer does is, it gives a name/symbol to a type within the current context. Prefixers could be really powerful tools to grant the user a lot of freedom and utility.

### Prefixer Namer

As previously mentioned, the namer simply defines a name for us to refer to a type. The namer is usually used in Store definitions/accesses and Positional Parameters.

#### Explicit Store Definition
```
[~]> [[b:store [~j 200]]]
[[~j 200]]
```

#### Implicit Store Definition
```
[~]> [[[~n 10][~k 25.5]]] # Similar to lists, if we define items in what could be an implicit array, but these are named,
                          # the interpreter considers it as a Store.
[[~k 25.5] [~n 10]]
```

#### Store Access
```
[~]> [[let s [b:store [~n 5][~l 3.14]]]]
[[~l 3.14] [~n 5]]
[~]> [s ~n]
5
```

#### `let` alternative / Positional Parameter
Another interesting way the Namer can be used is to essentially define variables, the same way as using `let`.

```
[~var 19200] # This defines a name/symbol called `var` that points the type number `19200`.
```

It doesn't work as well for `mut`, because canvas doesn't really deal with data in names, but references, so even though you might replace a variable's name with another type, the created references will still point to the original value. This is because Namer is mostly a runtime instruction.

In the case of Positional Parameters, let's say I have a "func" function that takes 2 paramters: param1 and param2, in that order. I can execute this function the following way

```func [~param2 VALUE2] [~param1 VALUE1]```

Function invocations create a context on the fly, so the interpreter allows me to fulfill a parameter requirenment by giving a name to a type during runtime. But one must have in mind that not all functions allow Prefixers in their param list.

### Prefixer Expander

The Expander Prefixer (^) allows the members of a list to be used as parameters in the current block. The syntax is `^CODE`. The code part is considered "appended body" and it's interpreted as its own code, irrespective of the current context. Meaining you could append an entire program to it, and it would interpret it as valid canvas.

```
[~]> [[1 ^[2 3]]]
[1 2 3]
...
[~]> [[+ 2 ^[2 2]]]
6
...
[~]> ^[1 2 3][4 5 6]
[4 5 6]                 # As mentioned above, since we used to statements as appended body, it would evaluate `[1 2 3]`, and
                        # then `[4 5 6]`, using the last statement as the subject.
```

### Prefixer Paralleller

The Paralleler Prefixer (|) is a body-appended Prefixer that invokes a thread on the given code.

```
[~]> [[await |[ [let n 50][while [-- n][print n]]]]]
...
3
2
1
nil
```

The thread starts as soon as the block evaluated. You may wait for a thread to finish synchronously using the imperative `await`. 

```
[~]> [[>> 3 [>> [await |[await |2]] 1]]]
[1 2 3]
[~]>
```
Nested parallel jobs
 

## Dependencies
`canvas` does not require any special dependency. It only requires a C++11 capable compiler, preferably GCC (or MINGW), but other Unix-like compilers can work. Visual Studio Compiler wouldn't compile without some work as `canvas` expects a Unix-like compiler/environment. For Windows it's recommended to use MINGW.

## How to build
- `cmake .`
- `make -j`
- `sudo make install`