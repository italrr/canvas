# canvas [~]

**canvas** is an esoteric, dynamic, pseudo-functional, psudo-procedural, programming/scripting language designed with expressiveness and hackability in mind. It's heavily inspired by JavaScript, and LISP, a tad little bit of Python, but it's mostly my own creation. This project is nowhere near close to completion but it's come close enough for some usability.

## Documentation
- [Standard Operators](./docs/Spec.md).

Short summary:

- Prefix Notation
- Code can only be ASCII. Anything else won't be interpreted correctly
- Garbage collected through reference counting (Currently using a custom design. There probably will be a lot of leaks for a bit)
- Only specific types of items can mutate
- No classes. And Objects are not exactly _objects_. They're called contexts and they can be used to store variables, but one should use a different philosophy from OOP when using them.
- Similarly to LISP, it tries to treat everything as a list.
- Strings can only be formed with single quotes `'EXAMPLE'`
- Unusual control flow driven by abstract concepts such as context "linking".
- Recursion is encouraged
- Parallelism and concurrency are first class citizens
- Honestly? Downright different (Hopefully within reason). This project is still to be proven, so don't expect anything serious for now.

**canvas** expression structure
```
[OPERATOR(OPTIONAL MODIFIER) ARGUMENTS]
```
Depending on the context operator is an operator name is not mandatory. For example, `cv` will interpret the following as a List: `[1 2 3]`. So this means that when the first item is not a function, it will be interpreted as a natural type.

## Why?

This project is an experiment. I came up with the basic concept when I was college and, since then have worked my way up to where it is right now. What makes it special enough to justify it being so different? Well, **canvas** was _actually_ designed to be used because of its parallelism and expressiveness. Hopefully I'll be able to prove my vision using examples in the future.

## Modifiers
Modifiers are (sometimes complex) abstractions that can be used to drive the unsual control flow canvas has, modify a behavior of sorts, switch a context, or invoke a specific routine.
- `:`: It's the access modifier. It allows accessing contexts' members directly.
- `^`: The Expand modifier has two main functions
    - Allows to take a list (or members of a context) and expand them to the current execution, essentially transforming each item into an argument.
- `~`: It's called the namer modifier. It allows renaming an object on the fly on the current context. It's also used for naming variables within contexts.
- `|`: The Parralel linker allows linking contexts. Useful for multi-threading, concurrency and/or just complex control flow. It also opens the door for potentially messy code, so it should be used intelligently.


## Libraries (WIP)
Libraries can be imported by using the statement `[import "library-name"]`. The interpreter will look for a file named `library-name` locally. If not found, then it will check the libraries installed in `/usr/lib/cv/`.