canvas
------

*ATTENTION: This is more of a learning/toy project. This is my attempt to make my own type of LISP*

`canvas` is very simple language inspired by LISP and JS. It's not meant be performant in casey capacity.

```

(let a (+ 2 2))
(print a)


(func a (x y)(
    (let b 5)
    (return (+ x y b))
))

# loop from 0 to 10 #
(for (.. 0 10) as i (
    (print i)
))

# creates object #
(object b (
    (let a 5)
    (let b 5)
    (let c 8)
))

# defines c as b and a dynamic object joined #
(splice c with b (
    (let d 5)
    (let e 5)
))


(print "hi %s" with ("lol"))

```
