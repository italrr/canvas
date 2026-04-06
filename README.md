canvas [~]
----------

`canvas` is a dynamic, high-level, general-purpose scripting language designed as an extension interface and rapid prototyping tool for other projects.

The philosophy of `canvas` is to stay as simple as possible while still giving the user a lot of expressive power. It favors a compact prefix syntax, first-class aggregate types, and a runtime model based on contexts and values instead of classes, inheritance, or large frameworks.

`canvas` has gone through several rewrites and experiments. Version **1.0.0** is the first stable release of the current design.

## Features

- Direct interpreter over token trees
- Prefix notation
- Bracket-structured syntax
- First-class **LIST** and **STORE** values
- Basic types are **NIL**, **NUMBER**, **STRING**, **LIST**, **STORE**, and **FUNCTION**
- Mostly immutable data model, with explicit mutation where needed
- Dynamic library system for native modules
- Relaxed parser for informal input
- Error handling through explicit return values and control-flow, not exceptions
- Standard/native modules such as:
  - `json`
  - `io`
  - `math`
  - `time`
  - `file`

## Design notes

### No OOP
`canvas` does not try to be object-oriented. You can store functions inside stores, but stores are not classes and there is no inheritance model.

```canvas
[[let test [b:store [~n 10] [~k [fn [a b] [+ a b]]]]]]
[[let f [test ~k]]]
[f 1 2]
```

## Syntax

`canvas` organizes code using brackets. Statements are prefix-notated and can themselves contain nested statements.

At a high level:

```text
"[ []...[] -> STATEMENT(S) ]" -> PROGRAM
```

In practice, the interpreter is intentionally relaxed and will try to repair incomplete or informal input when possible.

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

The takeaway is simple: `canvas` is flexible, but explicit bracketing is always safer when ambiguity matters.

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
[[~name 'Italo'] [~role 'builder']]
```

## Variables and functions

### `let`
```canvas
[[let a 5] a]
```

### `mut`
```canvas
[[let a 5] [mut a 9] a]
```

### `fn`
```canvas
[[let add [fn [a b] [+ a b]]]
[add 2 3]]
```

## Stores

### Explicit store construction
```canvas
[b:store [~name 'Italo'] [~role 'builder']]
```

### Implicit store construction
If every member of an aggregate is named, `canvas` treats it as a store.

```canvas
[[~name 'Italo'] [~role 'builder']]
```

### Store access
```canvas
[[let user [b:store [~name 'Italo'] [~role 'builder']]]
[user ~name]]
```

## Prefixers

Prefixers are one of the main expressive tools in `canvas`.

### `~` Namer
The namer associates a name with a value. It is used in store construction, named arguments, selectors, iterators, and other named runtime structures.

```canvas
[[~name 'Italo'] [~role 'builder']]
```

```canvas
[[let pair [fn [a b] [b:list a b]]]
 [pair [~b 9] [~a 3]]]
```

### `^` Expander
The expander spreads the contents of a list into the surrounding collector.

```canvas
[[1 ^[2 3] 4]]
```

```canvas
[[+ 1 ^[2 3]]]
```

This works in places that collect multiple values, such as list construction and function argument collection.

### `%` Template formatter
The template formatter evaluates its children and always returns a string.

- Named values define temporary names
- String values are appended to the output
- `{name}` placeholders are resolved from the current template scope

```canvas
[% [~name 'Italo'] 'Hello {name}']
```

Result:

```canvas
'Hello Italo'
```

### `?` Safe execution
The `?` prefix evaluates code and ignores errors. If the code fails, it simply returns `nil` and does not pollute the outer cursor/error state.

```canvas
?[nth [1 2 3] 999]
```

## Control flow

### `if`
```canvas
[if [> 5 3]
    'yes'
    'no']
```

### `while`
```canvas
[[let n 0]
 [while [< n 5]
    [++ n]]
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

## Imports

### Script imports
Use `import` to load another `.cv` file into the current context.

```canvas
[import 'math_helpers']
```

### Native/dynamic library imports
Use `import:dynamic-library` to load a native module.

```canvas
[import:dynamic-library 'json']
```

After loading, the module registers its functions into the current context.

## Standard and native modules

### `json`
Examples:

```canvas
[[import:dynamic-library 'json']
 [json:dump [[~a [1 2 3]] [~b [[~x 1] [~y nil]]]]]]
```

```canvas
[[import:dynamic-library 'json']
 [json:parse '{"a":1,"b":[2,3]}']]
```

### `io`
Examples:

```canvas
[[import:dynamic-library 'io']
 [io:out 'hello']]
```

```canvas
[[import:dynamic-library 'io']
 [io:in]]
```

### `math`
Examples:

```canvas
[[import:dynamic-library 'math']
 [[math ~sin] 0]]
```

```canvas
[[import:dynamic-library 'math']
 [math ~pi]]
```

### `time`
Examples:

```canvas
[[import:dynamic-library 'time']
 [tm:epoch]]
```

```canvas
[[import:dynamic-library 'time']
 [tm:format [tm:date [tm:epoch] 'GMT-4'] '%d/%m/%year %h:%M:%s %tz']]
```

### `file`
Examples:

```canvas
[[import:dynamic-library 'file']
 [file:exists './test.txt']]
```

```canvas
[[import:dynamic-library 'file']
 [file:get-extension './test.txt']]
```

### `bmp`
Bitmap/image helper functionality is available through the native `bmp` module.

## Error handling

`canvas` favors explicit error handling instead of exception-style flow.

Examples:

- functions may return `nil`
- callers may inspect returned values
- `?` can be used to swallow failures intentionally

This keeps the runtime small and predictable.

## Building

### Requirements

- CMake
- A C++17-capable compiler
- A C11-capable C compiler

Primary target is Unix-like environments. MinGW-style setups are also supported by the build system.

### Build

Debug/development style build:

```bash
cmake -S . -B build -DCV_ENABLE_SANITIZERS=ON
cmake --build build -j
```

Release build:

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DCV_ENABLE_SANITIZERS=OFF
cmake --build build-release -j
```

### Install

```bash
cmake --install build-release
```

## Goals

The goal of `canvas` is not to compete with giant general-purpose ecosystems. It is meant to be:

- easy to embed
- easy to extend
- expressive for configuration, automation, and project-specific scripting
- small enough to understand and evolve

## Status

**Version: 1.0.0**

Canvas 1.0.0 is the first stable release of the current direct-interpreter design.
