canvas
------

`canvas` is a simple multi-paradigm scripting language, heavily
inspired by Lisp, JS and C++.

This is rather a toy project of my own, mainly for learning.

Short summary:
- There are no classes. Only objects akin to JS-like objects
- Designed to be fully functional (unmutability), although mutations are possible
- Just like Lisp, everything is a list one way or the other
- Strings are formed using single quotes (') for now

Examples: 
```

(def a as (+ 2 2))
(print a)


(func b as (x y) -> (
    (let b 5)
    (return (+ x y b))
))

# loop from 0 to 10 #
(for (.. 0 10) with i (
    (if (= i 5)
        (print i)
        
    )
))

# creates structure #
(struct b as (
    (let a as 5)
    (let b as 5)
    (let c as 8)
))

# defines c as b and a dynamic object joined #
(splice c as b (object (
    (let d as 5)
    (let e as 5)
)))


(print "hi %s" with ("lol"))

```
