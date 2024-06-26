# canvas [~]

**canvas** is an esoteric, dynamic, pseudo-functional, psudo-procedural, programming/scripting language designed with expressiveness and hackability in mind. It's heavily inspired by JavaScript, and LISP, a little bit of Python, but it's mostly my own creation. This project is nowhere near close to completion but it's come close enough for some usability.

## Documentation
- TODO

Short summary:

- Interpreter is made in C++11.
- Prefix Notation.
- The interpreter only accepts ASCII encoding for sources.
- Garbage collected through reference counting (Using C++'s memory)
- Selective mutability
- No classes. And Objects are not exactly _objects_. They're called Contexts and they can be used to store variables, but one should try to avoid the OOP mindset around them.
- Similarly to LISP, it tries to treat everything as a list.
- Strings can only be formed with single quotes `'EXAMPLE'`
- Atypical but powerful control flow known as inline context switching.
- Recursion is encouraged
- Parallelism and concurrency are first class citizens

Typical **canvas** _imperative_ expression structure
```
[OPERATOR(OPTIONAL MODIFIER) ARGUMENTS]
```
**cv** will most of the time treat something as a list, or will try to **eval** it to a list. Therefore, there are two possible scenarios in which **cv** won't treat a block as a list:

- Imperative Blocks: `[OPERATOR/CONSTRUCTOR v[0] ... var[n]]` or `[FUNCTION(MOD) v[0] ... var[n]]`
- Inline-Context Switching: `[CONTEXT-NAME: CODE[0]...CODE[n]]`

## Why?

This project is an experiment. I came up with the basic concept when I was in college and since then have worked my way up to where it is right now. What makes it special enough to justify it being so different? Well, **canvas** was _actually_ designed to be used because of its parallelism and expressiveness. Hopefully I'll be able to prove my vision by creating POCs.

## Modifiers
Modifiers are (sometimes complex) abstractions that can be used to drive the control flow canvas has, modify a procedure of sorts, switch a context, or invoke a specific routine. The standard modifiers are:
- `~`: **The Namer**. It basically allows to rename (or name) a variable on the fly (on the current context). It's also used to define variables within Contexts. Examples:
    - `5~n`
    - `[ct 2.5~n 15~r]`
    - `[l-gen 1 10]~list`
- `:`: **The Scope**. It allows switching contexts. Either directly accessing a specific variable within that context or complety switching the current context in the flow. Examples:
    - `set test [ct 10~n]` -> `test:n`: Should print 10.
    - `set test [ct 10~n]` -> `test: ++ n`: Should increase `n` within `test` to 11.
- `|`: **The Trait**. It allows accessing/executing traits inate to specific types. See next section.
- `^`: **The Expander** or simply Expand. It's a handy modifier that basically takes a list and expands it to make it so every item it contains will take a space within the arguments list. For example, the operator `+` will sum all arguments you give it (variadic). What if you want to sum all numbers within a List? That's where Expand comes in. See:
    ```
        CV> [+ [1 2 3 4 5]]
        nil
        '+': Provided (1) argument(s). Expected no less than (2) arguments.

        CV> [+ [1 2 3 4 5]^]
        15
    ```

## Traits
Traits are essentially fancy inline functions or behaviors that are embedded into types. Each type possess specific traits. All types possess a `|copy` and `|type` trait.

```
canvas[~] v1.2.0 X86_64 [WINDOWS]
>>>>> RELAXED MODE <<<<<

CV> [1 2 3 4]|copy
[1 2 3 4]

CV> [1 2 3 4]|type
'LIST'

CV> 'this is a test string'|type
'STRING'

CV> 'this is a test string'|length
21

CV> [1 2 3 4]|length
4
```

For example `[1 2 3]|length`. The List type has a trait called `length` that will return the number of items.

Another example `'Test'|reverse`. The String type has a `reverse` trait will essentially reverse the string. It makes a copy of the original.


# Introspection or Metaprogramming
WIP

## Libraries (WIP)
Libraries can be imported by using the statement `[import "library-name"]`. The interpreter will look for a file named `library-name` locally. If not found, then it will check the libraries installed in `/usr/lib/cv/`.