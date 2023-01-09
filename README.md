# codespeak
Personal programming language

## Expression

Examples first:

### Symbolic application

`()` means symbolic application (includes sub-expression), or single value.
Whitespace needed to separate words.

`2` 2

`(2)` 2

`(2 * 3)`

`2 * 3` implicit top-level ()

`2 * (3 + 2)` 14

`2 * 3 + 2.0` 8.0 (real) 

### List

`,` means list

`2, 3` list of 2 natural numbers

`2, (3 * 4), 4` list of 3 numbers

`2, (3,4),4` list includes a sub-list

`2, +, 3` also a list (you can guess where this is going)

### Invalid expressions

`2 3 4` symbolic application, but no symbols present

`2, 3 + 4` symbol + is unexpected, need ()

`2 + 3., 4` cannot add a natural number to a list

## Bugs

Heap deallocation is not always correctly handled in case of misformed programs.

## General principles

This small language project tries to provide a more natural way to express programs, 
according to my findings from the study of natural languages 
(such as the Classic style, Chomsky's Minimal Program. More details to come).

A program is a sequence of paragraphs (not here yet).

A paragraph is a sequence of sentences (not here yet).

A sentence is a sequence of expressions (expressions in progress).

An expression can include symbols, that are loosely evaluated (totally symbol dependent).

Envisioning something like:

```
2, 3, 4; call it times. 
3th time; take it; call it p. 
2 * p + 1; print it.
```

`take p` removes p from list.

For the learning experience, all is in C (gnu99), and theory is introduced when absolutely needed (a learning exercise).

Single dependency is suckless.org's libgrapheme, for unicode support in source code.
Also, relies on one of FreeBSD's specific libc function (so, probably does not compile on GNU/Linux).
Inspired by nanopass, several small steps to evaluation (inefficient most of the time).

Long-term objective is to compile to WebAssembly (yes, not here yet).


