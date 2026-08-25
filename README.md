# canvas [~]

`canvas` is a dynamic, high-level scripting language designed mainly as an extension interface and rapid prototyping tool for other projects.

The idea behind `canvas` is simple: keep the language small, predictable, and easy to embed while still giving it enough expressive power to be useful.

It uses compact prefix syntax, bracket-structured code, first-class aggregate values, lexical closures, and a runtime built around contexts and values instead of classes or large frameworks.

`canvas` has gone through several rewrites and experiments. Version **1.1.0** continues the current direct-interpreter design and expands its function/runtime behavior.

## Features

- Direct interpreter over token trees
- Prefix notation
- Bracket-structured syntax
- First-class **LIST** and **STORE** values
- Basic types:
  - **NIL**
  - **NUMBER**
  - **STRING**
  - **LIST**
  - **STORE**
  - **FUNCTION**
- First-class functions
- Lexical closures
- Named arguments
- Variadic functions
- Mostly immutable data, with explicit mutation
- Dynamic native library system
- Relaxed parser for informal input
- Simple explicit error handling
- Native modules:
  - `json`
  - `io`
  - `math`
  - `time`
  - `file`

## Design

### No OOP

`canvas` is not object-oriented.

Stores can contain functions, but they are just stores. There are no classes, inheritance, constructors, or object hierarchies.

```canvas
[[let test [b:store [~n 10] [~add [fn [a b] [+ a b]]]]]
 [let f [test ~add]]
 [f 1 2]]
```

## Syntax

Code is organized with brackets and uses prefix notation.

```text
[operation argument argument ...]
```

Statements can contain other statements:

```canvas
[+ 1 [* 2 3]]
```

The parser is intentionally relaxed.

```canvas
[~]> + 1 1
2

[~]> [+ 2 2]
4

[~]> [[+ 3 4]]
7

[~]> [[[+ 2 2 2 24]]]
30

[~]> [+ 2 2][+ 10 11]
21

[~]> [[[+ 2 3][+ 4 5] 2]]
[5 9 2]
```

Explicit brackets are still recommended when code could be ambiguous.

## Values

### Numbers

```canvas
42
3.14
-9
```

### Strings

```canvas
'hello'
'canvas'
```

### Lists

```canvas
[1 2 3]
```

### Stores

```canvas
[[~name 'Alex'] [~role 'Engineer']]
```

## Variables

### `let`

```canvas
[[let a 5] a]
```

### `mut`

```canvas
[[let a 5] [mut a 9] a]
```

## Functions

### `fn`

```canvas
[[let add [fn [a b] [+ a b]]]
 [add 2 3]]
```

Functions are first-class values and can be returned from other functions.

### Closures

Functions capture the lexical values available where they are created.

```canvas
[let make [fn [a b] [[let c 5] [return [fn [] [+ a b c]]]]]] [let closure [make 10 20]] [closure]
```

Result:

```canvas
35
```

### Variadic functions

`@` can be used as the argument list for a variadic function.

```canvas
[fn [@] @]
```

## Stores

### Explicit store

```canvas
[b:store [~name 'Alex'] [~role 'builder']]
```

### Implicit store

If every member of an aggregate is named, `canvas` treats it as a store.

```canvas
[[~name 'Alex'] [~role 'builder']]
```

### Access

```canvas
[[let user [b:store [~name 'Alex'] [~role 'builder']]]
 [user ~name]]
```

## Prefixers

Prefixers are one of the main expressive features in `canvas`.

### `~` Namer

Associates a name with a value.

It is used by stores, named arguments, selectors, iterators, and other named runtime structures.

```canvas
[[~name 'Alex'] [~role 'builder']]
```

Named arguments can also be passed out of order:

```canvas
[[let pair [fn [a b] [b:list a b]]]
 [pair [~b 9] [~a 3]]]
```

### `^` Expander

Expands a list into the surrounding collector.

```canvas
[1 ^[2 3] 4]
```

```canvas
[+ 1 ^[2 3]]
```

It can be used during list construction and function argument collection.

### `%` Template formatter

Evaluates its children and returns a string.

Named values define temporary names and `{name}` placeholders resolve from the template scope.

```canvas
[% [~name 'Alex'] 'Hello {name}']
```

Result:

```canvas
'Hello Alex'
```

### `?` Safe execution

Executes code while swallowing errors.

If the expression fails, it returns `nil` without changing the outer error state.

```canvas
?[nth [1 2 3] 999]
```

## Control flow

### `if`

```canvas
[if [> 5 3] 'yes' 'no']
```

### `while`

```canvas
[[let n 0]
 [while [< n 5] [++ n]]
 n]
```

### `for`

```canvas
[for [~x [0 5]]
    [print x]]
```

### `foreach`

```canvas
[foreach [~item [1 2 3]]
    [print item]]
```

### `return`

```canvas
[fn [a] [return [+ a 1]]]
```

### `yield`

`yield` propagates a value through the current control flow.

### `skip`

`skip` skips the current loop iteration.

## Strings

Strings use single quotes.

```canvas
'hello world'
```

`s-join` joins values into a string.

```canvas
[s-join 'Hello' ' ' 'world']
```

Result:

```canvas
'Hello world'
```

## Imports

### Script libraries

Use `import` to load another `.cv` file.

```canvas
[import 'helpers']
```

### Native libraries

Use `import:dynamic-library` to load a native module.

```canvas
[import:dynamic-library 'json']
```

The module registers its functions directly into the current context.

## Native modules

### `json`

```canvas
[[import:dynamic-library 'json']
 [json:dump [[~a [1 2 3]] [~b [[~x 1] [~y nil]]]]]]
```

```canvas
[[import:dynamic-library 'json']
 [json:parse '{"a":1,"b":[2,3]}']]
```

### `io`

```canvas
[[import:dynamic-library 'io']
 [io:out 'hello']]
```

```canvas
[[import:dynamic-library 'io']
 [io:in]]
```

### `math`

```canvas
[[import:dynamic-library 'math']
 [math:sin 0]]
```

```canvas
[[import:dynamic-library 'math']
 math:pi]
```

### `time`

```canvas
[[import:dynamic-library 'time']
 [tm:epoch]]
```

```canvas
[[import:dynamic-library 'time']
 [tm:format [tm:date [tm:epoch] 'GMT-4'] '%d/%m/%year %h:%M:%s %tz']]
```

### `file`

```canvas
[[import:dynamic-library 'file']
 [file:exists './test.txt']]
```

```canvas
[[import:dynamic-library 'file']
 [file:get-extension './test.txt']]
```

## Error handling

`canvas` keeps error handling simple.

Functions can return `nil`, callers can inspect returned values, and `?` can intentionally ignore failures.

There is no exception-based language-level control flow.

## Building

### Requirements

- CMake
- C++17 compiler
- C11 compiler

Unix-like environments and MinGW-style Windows builds are supported.

### Debug

```bash
cmake -S . -B build -DCV_ENABLE_SANITIZERS=ON
cmake --build build -j
```

### Release

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DCV_ENABLE_SANITIZERS=OFF
cmake --build build-release -j
```

### Install

```bash
cmake --install build-release
```

## Goals

`canvas` is not trying to compete with large general-purpose language ecosystems.

It is meant to stay:

- easy to embed
- easy to extend
- easy to understand
- useful for automation and project-specific scripting
- small enough to change without fighting the language itself

## Status

**Version: 1.1.0**

Canvas 1.1.0 continues the current direct-interpreter design, with improved function semantics, lexical closures, native API compatibility, and general runtime fixes.
