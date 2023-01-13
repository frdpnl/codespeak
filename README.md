# codespeak
Personal programming language

## Expression

Presented by examples.

### Symbolic application

Note: `()` means sub-expression.
It also expresses symbol application, or a single value.

Whitespace expected to separate words.

`2` 2

`(2)` 2

`2 * 3` 6, valid because of an implicit top-level ()

`2 * (3 + 2)` 10

`2 * 3 + 2.0` 8.0 (real) 

`2 * 2 > 3.` 1 (true), precedence applies

`2 = 2.` 0 (false), strict equality

`2 ~= 2.` 1 (true), roughly equal

`2 /= 2.` 1 (true), different

`2 * 0.5 = 1 + 0 = 1` 1 (true)

`2 = 2 = 2.` 0 (false), true is 1, so different from 2

`(2,3,+) = (2, (2 + 1), +)` 1 (true)

`2 and -2` 1 (true)

`2 = 3 or 3 = 2. * 3` 0 (false), and/or have lower precedence

### List

`,` means list

`2, 3` list of 2 natural numbers

`2, (3 * 4), 4` list of 3 numbers

`2, (3,4),4` list includes a sub-list

`2, +, 3` also a list (you can guess where this is going)

### Invalid expressions

`2 3 4` symbolic application (implicit ()), but no symbols present

`2, 3 + 4` symbol + is unexpected, needs () or ,

`2, = 2,` 2 is unexpected, needs () or ,

`2 + 3., 4` cannot add a natural number to a list

## Known bugs


## Background

This small language project tries to provide a more natural way to express programs, 
according to my findings from the study of natural languages 
(such as the Classic style, Chomsky's Minimal Program. More details to come),
and to my taste and limitations :)

*Language does not need grammar, but grammar needs language.* (citation missing)

A program is a sequence of paragraphs (not here yet).

A paragraph is a sequence of sentences (not here yet).

A sentence is a sequence of expressions (expressions in progress).

An expression can include symbols, that are loosely evaluated (totally symbol dependent, Minimal Program).

Envisioning something like:

```
2, 3, 4; call it times. 
3th time; take it; call it p. 
2 * p + 1; print it.
```

`take p` removes p from list.

For the learning experience, all is in C (gnu99), and theory is introduced when needed (a learning exercise).

Single dependency is suckless.org's libgrapheme, for unicode support in source code.
(like other suckless.org programs - `st`, `dwm` - it is excellent).
Also, relies on one of FreeBSD's specific libc function (so, probably does not compile on GNU/Linux).
Inspired by nanopass; several small steps to evaluation (inefficient most of the time).

Long-term objective is to compile to WASM (yes, not here yet).

Beyond long-term project is to provide WASM runtime to GPU.


