# codespeak
A personal programming language.

This is a prototype, meant to explore language design ideas, interpreters and later compilers.

I intentionally ignore some common C practices. 

## Phrase

A phrase is a sequence of expressions.

For example:

` 23 ; 3 + 4 ; 2,3,4` 

evaluates to the list 2, 3, 4.

`it` is a builtin symbol that stores the previous expression's value.

`23; it * 2` 46

`2, +, n ; call it A` gives the list 2, +, n (n is undefined at this point, still valid)

## Expression

Presented by examples.

### Symbolic reduction

Note: `()` expresses symbol application or a single value.
The parenthesized expression ends up reduced to a single value.
After evaluation, no `()` are left.

Whitespace expected to separate words.

`(+)` the symbol + (builtin)

`2 * 2 > 3.` 1 (true), precedence applies

`2 = 2.` 0 (false), strict equality

`2 ~= 2.0` 1, more or less equal

`2 /= 2.` 1, different types

`(2,) ~= (2.,)` 1 (true)

`(2,3,+) = (2, (2 + 1), +)` 1

`2 and (not -2)` 0

`x` the not yet defined symbol x (valid expression)

### List

`,` defines a list

`2, (3 * 4), 4` list of 3 numbers

`2, (3,4),4` list includes a sub-list

`2, +, 3` also a list (you can guess where this is going)

`list` changes the rest of the expression into a list

`list 1 2 3` list 1, 2, 3

### Builtin operators

`3 ; call it p` defines the symbol p, worth 3

### Higher order: do, list, id

`do` means evaluate following list as an expression
`do` is the inverse of `list`.

`do (2, +, 3)` 5

`4 ~= 2. * do (1, +, 1)` 1 (true)

`do (list 1 + 2)` 3

`do (do (list (list +)))` + (the symbol)

`1 + 2 ; it, *, 2 ; do it` 6

`2, *, n ; call it a ; call 2 n ; do a` 4 

`2, -, n ; call it a ; call 2 n ; id a` the list 2, -, 2

`id` evaluates to its argument (strictly evaluated)

### Invalid expressions

`2 3 4` symbolic application (implicit ()), but no symbols present

`2, 3 + 4` symbol + is unexpected, needs a () or ,

`2, = 2,` 2 is unexpected, needs () or ,

`2 + 3., 4` cannot add a natural number to a list

## Known bugs

After reading the book *Fluent C* (C design patterns), some refactoring is necessary.


## Background

This small language project tries to provide a more natural way to express programs, 
according to my findings from the study of natural languages 
(such as the Classic style, Chomsky's Minimal Program. More details to come),
and to my taste and limitations :)

*Language does not need grammar, but grammar needs language.* (Sylvain Auroux interview)

A program is a sequence of paragraphs (not here yet).

A paragraph is a sequence of phrases (not here yet).

A phrase is a sequence of expressions.

An expression can include symbols, that trigger computation (totally symbol dependent, see Minimal Program).

Envisioning something like:

```
2, 3, 4; call it times 
3th time; take it; call it p 
2 * p + 1; print it
```

`take p` removes p from list.

For the learning experience, all is in C (gnu99), and theory is introduced when needed.

Single dependency is suckless.org's libgrapheme, for unicode support in source code.
(like other suckless.org programs - `st`, `dwm` - it is excellent).
Also, relies on one of FreeBSD's specific libc function (so, probably does not compile on GNU/Linux).
Inspired by nanopass; several small steps to evaluation (inefficient most of the time).

Long-term objective is to compile to WASM (yes, not here yet).


