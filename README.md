canvas
------

`canvas` is a multi-paradigm scripting language, inspired by JS and Lisp.

This is rather a toy project.

Short summary:
- There are no classes. Only objects akin to JS-like objects
- Designed to be fully functional (unmutability), although mutations are possible
- Just like Lisp, everything is a list one way or the other
- Strings are formed using single quotes (') for now

Examples: 
```

(def a (+ 2 2))
(print a)


(func b (x y)(
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
(obj b (
    (def a)

))

# defines c as b and a dynamic object joined #
(splice c b (obj (
    (let d 5)
    (let e 5)
)))


(print "hi %s" with ("lol"))

```
