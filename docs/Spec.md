# canvas [~]

### Natural Types

- Lists: `[]`
- Proto (Also known as Objects): `proto`
- Number (`double` type): `0123456789.-`
- String: `'<STRING>'`
- Function: `fn [ARGUMENTS][BODY]`

### Conditionals and loops
- `if`: If the provided statement is true, it executes the first block, if not(and it provided), it executes the second block.
- `do`: It will return the second block as long as the first block returns 1 or more, or not `nil`.
- `iter`: It goes through an iterable, which are Lists and Protos. With the usage of the `~` Namer modifier, you may refer and utilize every item of the iteration. Example `iter [.. 1 5]~i [io:out i]` returns `12345`.
- `for`: Provided an "start" block, a "conditional" block, and a "modifier" block, it will return the 4th and subsequent blocks until the "conditional" block is no longer true. Example: `for [set i 0][lt i 10][++ i][io:out i '\n']` returns `0123456789`.
- `or`: It returns 1 if one of the provided items is 1 or more, or not `nil`.
- `and`: It returns 1 if all the provided items are 1 or more, or not `nil`.
- `nand`: The opposite of and.
- `nor`: The opposite of or.
- `eq`: It returns true if all items are 1 or not `nil`.
- `neq`: It returns true if all items are 0 or `nil`.
- `gt`: It returns true if all the items value are in descending order (15 5 1).
- `gte`: It returns true if all the items value are in descending order OR are equal to the first (15 15 1).
- `lt`: It returns true if all the items value are in ascending order (1 5 15).
- `lte`: It returns true if all the items value are in ascending order OR are equal to the first (1 5 5).

### Standard Operators

- `set`: Allows to define an object with a name into a context.
- `rset`: Allows to redefine an existing object with a name. Unlike `set`, `rset` can redefine objects within PROTOs.
- `splice`: It allows to combine at least two PROTOs. `splice [proto a:2] [proto b:3]` would return `proto a:2 b:3`.
- `..`: Provided two different whole numbers numbers (and optionally, the third argument, a step) it creates a range or list. `.. 1 5`: would return `[1 2 3 4 5]`. `.. 1 10 2` would return `[1 3 5 7 9]`.
- `l-flatten`: It takes all sublists provided and _flattens_ them into a single item. Example input `l-flatten [1 2 3 [4 5 [6 7]]]` would return `[1 2 3 4 5 6 7]`.
- `++`: It works similarly to many C-like languages. It increases the provided item by 1 given it's a number.
- `--`: Same as `++` but decreasing it by 1.
- `s-join`: joins strings into a single one. However `+` also does that.

### Arithmetic

- `+`: Sums all items in order. It can behave differently depending of the type. It can concat strings, but also sum lists of the same size.
- `-`: Substracts all items in order. Similar behavior as above except by the string manipulation.
- `/`: Divides all items in order. Similar behavior as above.
- `*`: Multiplies all numbers in order. Similar behavior as above.

### Interrupts

- `ret`: Stops current execution of loop or function and hauls the item provided upwards in the code.
- `stop`: Same as `ret`, except there's no item hauled.
- `skip`: Skips current step of the execution inside loops. In Functions, it does the same as `ret`. Similarly as `ret`, it can haul an item upwards.