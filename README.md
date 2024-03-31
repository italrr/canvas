# canvas [~]

**canvas** is an esoteric, dynamic, pseudo-functional, psudo-procedural, programming/scripting language designed with expressiveness and hackability in mind. It's heavily inspired by JavaScript, and LISP, a tad little bit of Python, but it's mostly my own creation. This project is nowhere near close to completion but it's come close enough for some usability.

## Documentation
- [Standard Operators](./docs/Spec.md).

Short summary:

- Interpreter is made in C++11.
- Prefix Notation.
- The interpreter only accepts ASCII encoding for sources.
- Garbage collected through reference counting (Using C++'s memory)
- Selective mutability
- No classes. And Objects are not exactly _objects_. They're called Contexts and they can be used to store variables, but one should use a different philosophy from OOP when using them.
- Similarly to LISP, it tries to treat everything as a list.
- Strings can only be formed with single quotes `'EXAMPLE'`
- Unusual control flow driven by abstract concepts such as context "linking", context switching, etc.
- Recursion is encouraged
- Parallelism and concurrency are first class citizens
- Honestly? Downright different (Hopefully within reason). This project still needs to be proven, so don't expect anything serious for now.

**canvas** expression structure
```
[OPERATOR(OPTIONAL MODIFIER) ARGUMENTS]
```
`cv` will most of the time treat something as a list, or will try to **eval** it to a list. Therefore, there are two possible scenarios in which `cv` won't treat a block as a list:

- Imperative Blocks: `[OPERATOR v[0] ... var[n]]` or `[FUNCTION(MOD) v[0] ... var[n]]`
- Inline-Context Switching: `[CONTEXT-NAME: CODE[0]...CODE[n]]`

## Why?

This project is an experiment. I came up with the basic concept when I was college and since then have worked my way up to where it is right now. What makes it special enough to justify it being so different? Well, **canvas** was _actually_ designed to be used because of its parallelism and expressiveness. Hopefully I'll be able to prove my vision by creating POCs.

## Modifiers
Modifiers are (sometimes complex) abstractions that can be used to drive the control flow canvas has, modify a procedure of sorts, switch a context, or invoke a specific routine. The standard modifiers are:
- `~`: **The Namer**. It basically allows to rename a variable on the fly (on the current context). It's also used to define variables within Contexts. Examples:
    - `5~n`
    - `[ct 2.5~n 15~r]`
    - `[l-range 1 10]~range`
- `:`: **The Scope**. It allows switching contexts. Either directly accessing a specific variable within that context or complety switch the currently context in the flow. Examples:
    - `set test [ct 10~n]` -> `test:n`: Should print 10.
    - `set test [ct 10~n]` -> `test: + 1 1`: Should increase `n` within `test` to 11.
- `|`: **The Trait**. It allows accessing/executing traits inate to specific types. See below.
- `^`: **The Expander** or simply Expand. It's a handy modifier that basically takes a list and expands it to make it so everything item it contains will take a space within the current execution. For example, when operator `+` will sum all arguments you give it. What if you want to sum all numbers within a List? That's where Expand comes in. See:
    ```
        set a [1 2 3]
        set b [+ a^]
    ```
    `b` will be 6.
- `<`: **The Linker**. It allows linking context. WIP.

## Traits
Treaits are essentially fancy inline functions or behaviors that are embedded in types. Each type possess specific traits.

For example `[1 2 3]|n`. The List type has a trait called `n` that will return the number of items.

Another example `'Test'|reverse`. The String type has a `reverse` trait will essentially reverse the string. It makes a copy of the original.


# Introspection or Metaprogramming
WIP

## Libraries (WIP)
Libraries can be imported by using the statement `[import "library-name"]`. The interpreter will look for a file named `library-name` locally. If not found, then it will check the libraries installed in `/usr/lib/cv/`.