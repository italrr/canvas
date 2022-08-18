# canvas

# What is it?

`cv` is an esoteric dynamic/pseudo-functional programming language designed with simplicity and hackability in mind. It's heavily inspired by C++ and Javascript, with a very small touch of LISP.

The interpreter is intended to be a single file, easy to include and integrate with other projects. The intepreter is written in C++.

Short summary:

- Code only supports ASCII (no unicode or emojis)
- Garbage collected
- Recursion highly encouraged
- Prefix Notation
- No classes
- Objects are defined by the `proto` procedure. Objects bodies are JSON objects basically, but with the ability to define `cv` expressions
- Designed to imitate the functional paradigm but only superficially.  
- Similarly to LISP, it tries to treat everything as a list.
- Strings are only formed with double quotes `"EXAMPLE"`
- It's primarily intepreted, but later on it will feature a way to compile into binary (cv code -> C code -> [COMPILER] -> Binary)
- Most of the `std` is defined in the language itself.
- Parallelism and asynchronism are very fundamental features of the language (no need for libraries).

`cv` expression structure
```
[(MODIFIER/CONTEXT-SWITCH:)IMPERATIVE(:MEMBER/TRAIT) ARGUMENTS]
```

Every statement must start a IMPERATIVE except in case of lists.

# Why?

Because I want to. I don't care if people ever learn it or adopt it, it's not my intention. However, I will be using this language to add scripting/interactibility to my other projects.

# Examples

```

// C style comments

// Simple sum
> [+ 2 2]
> 10

// Sum of two variables
[
    [def a 5]
    [def b 5]
    [std:out + a b]
]
>10

// Getting the average of a list
[
    [def l [2 5 6]]
    [def avg [/ [+ l:pop] l:length]]
]
>4.333...
/*
    `:pop` is a behavioral trait of lists that basically expands its contents to turn each item in the list to an argument. Then `+` sums each argument. 
    `:length` is a static property of lists that stores the number of items in a list.
*/

```

## Some explanation
Colons before an imperative are either modifiers or request a context. A context is pretty much any block code. For example, an expression defined inside of a `proto`, the context is everything inside the `proto` itself.
Colons after the imperative are either behavioral traits or members of the imperative. Imperatives can be `proto`, constants or functions.

## Behavioral Traits vs Functions
Behavioral traits are essentially functions, but only act depending on the context (operator/imperative context, not code block "context").

## How to define a library or includable file

```
[
    std:module "library-name" [
        proto [
            "print": [ fn [input] [std::out input "\n"] ],
            "uint8": [ trait integer [[traits:numerical_operation self:value] [traits:literal_operation [fn [][return string self:value]]]] [traits:any [integer:min 0] [integer:max 255] integer:signed] ] 
        ] 
    ]
]
EOF
```

This library then can be imported by using the statement `[std:import "library-name"]`. The interpreter will look for a file named `library-name` locally. If not found, then it will check the libraries installed in `/usr/lib/canvas/`. if not found, the
interpreter will either throw a warning and continue or stop the program's execution (depending if the interpreter was executed with the `--relaxed` argument).

The items defined inside this library are a function called print which basically takes any string and then calls a function called `out` from `std`, and then adds "\n".
The other item is a trait that tells any other defined with this trait will be an `integer`, whose only working actionable contexts are numerical or literal.