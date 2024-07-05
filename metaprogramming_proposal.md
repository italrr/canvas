Canvas' main selling feature besides its concurrency is supposed to be its metaprogramming capabilities. As of right now (0.2.2) there's no metaprogramming. This document is meant to be propose a potential implementation

### Meta-imperatives

Meta-imperatives are contextual imperative objects/actions/conditions used when constructing a type. They help  modify the type's natural behavior or make it specific

- `policy`
- `member`
- `trait`
- `as`
- `invoker`


### Types

- 'STRING'
- 'NUMBER'
- 'CONTEXT'
- 'FUNCTION'
- 'INTERRUPT'
- 'LIST'
- 'JOB'

### Psudo Types

- 'PROTOTYPE'
- 'INVOKER'

```

fn [input] [
    |policy test input [std:is-type @ 'STRING'] [std:err @|name ' must be a STRING type']
    |policy test input [std:str-min @ 1]
    |policy test input [std:str-max @ 5]
    stop s-reverse input
]

[
    type Vector2
    |as 'PROTOTYPE' 'INVOKER'
    |invoker 'x' 'y'
    |policy test x [std:is-type @ 'NUMBER']
    |policy test y [std:is-type @ 'NUMBER']
    |member 'x' 'NUMBER'
    |member 'y' 'NUMBER'
    |trait 'x' 'stop x'
    |trait 'y' 'stop y'
    |trait STRING ''
    |trait NUMBER ''
]


```