# canvas

### Standard Operators

- `set NAME ITEM`: Allows to define an object with a name into a context.
- `rset NAME[:] ITEM`: Allows to rename an existing object with a name.
- `splice`: It allows to combine at least two PROTO. `splice [proto a:2] [proto b:3]` would return `proto a:2 b:3`.
- `eq`: It returns true if all items are 1 or not `nil`.
- `neq`: It returns true if all items are 0 or `nil`.
- `gt`: It returns true if all the items value are in descending order (15 5 1).
- `gte`: It returns true if all the items value are in descending order OR are equal to the first (15 5 1).
- `lt`: It returns true if all the items value are in ascending order (1 5 15).
- `lte`: It returns true if all the items value are in ascending order OR are equal to the first (1 5 15).
- `..`: Provided by two different while numbers numbers (and optionally, the third argument, a step) it creates at least. `.. 1 5`: would return `[1 2 3 4 5]`. `.. 1 10 2` would return `[1 3 5 7 9]`.
`l-flatten`: It takes all sublists provided and _flattens_ them into a single item. Example input `l-flatten [1 2 3 [4 5 [6 7]]]` would return `[1 2 3 4 5 6 7]`.
- `++`: It works similarly to many C-like languages. It increased the provided item by 1 given it's a number.
- `--`: Same as `++` but decreasing it by 1.
- `s-join`: joins strings into a single one. However `+` also does that.

- `+`: Typical sum (all items). It can behave differently depending of the type.
- `-`: Typical substract (all items). It can behave differently depending of the type.
- `/`: Typical divison (all items). It can behave differently depending of the type.
- `*`: Typical multiplication (all items). It can behave differently depending of the type.
- `or`: Typical `OR` operator
- `and`: Typical `AND` operator
- `nand`: Typical `NAND` operator
- `nor`: Typical `NOR` operator