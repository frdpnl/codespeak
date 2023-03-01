# codespeak
A personal programming language.

This is a prototype, meant to explore language design ideas, interpreters and maybe compilers.

## Structure

Lines are made of sequences of expressions, each separated by `;`

For example:

`2 * 3.; print it` displays 6

`it` is a builtin symbol that stores the previous expression's value

`2, +, n ; print it` displays the list 2, +, n 

## Symbolic reduction

Note: `()` expresses symbol application (or its single value).
The parenthesized expression ends up reduced to a single value.
After evaluation, no `()` are left.

Whitespace separates words.

`2 = 2.` 0 (false), strict equality

`2 ~= 2.0` 1, more or less equal

`2 /= 2.` 1, different types

`(2,) ~= (2.,)` 1 (true)

`(2,3,+) = (2, (2 + 1), +)` 1 (true)

### Special symbols

`3 ; rem: rest of this expression is ignored ; print it` displays 3

`3 ; call it p` defines the symbol p, worth 3, same as `call 3 p`

`call + plus; 1 plus 3; print it` displays 4

`look` evaluates its argument

`call 3 p ; print p` displays p, but `call 3 p ; look p; print it` displays 3

`true?` and `false?` test the special symbol `it`

### List and Do

`,` defines a list

`2, (3,4),4` list includes a sub-list

`list` changes the rest of the expression into a list

`list 1 + 3` the list 1, +, 3

`do` means evaluate following list as an expression

`2, +, 3; do it` 5

`1 + 2 ; it, *, 2 ; do it; print it` 6

`list 1 + 2; do it` 3, or `do (list 1 + 2)` but sequence preferred

`1, +, 3; list it; do it; do it` 4, or `do (do (list (list 1 + 3)))`

### Conditional

`if 2 > 1 ; print correct ; end if` displays correct

`if -3 > 3 ; print weird ; end if` doesn't display anything, rest of line is ignored, `it` is false

`if false? ; print better ; end if` displays better, because `it` is false (from previous `if`)

`2 > 1 ; if true? ; print true ; end if` displays 'true

### Functions

```
define f (a,) ; a * 2 ; end f
f (3,) ; print it
```

```
define newf ()
	define g () ; 2 ; end g
end f
newf () ; call it two
two ()
```

### Invalid expressions

`2 3 4` symbolic application (implicit ()), but no symbols present

`2, 3 + 4` symbol + is unexpected, needs a () or ,

`2, = 2,` 2 is unexpected, needs () or ,

`2 + 3., 4` cannot add a natural number to a list


## Known bugs


## Background

This small language project tries to provide a more natural way to express programs, 
according to my findings from the study of natural languages 
(such as the Classic style, Chomsky's Minimal Program. More details to come),
and to my taste and limitations :)

*Language does not need grammar, but grammar needs language.* (Sylvain Auroux)

A line (aka phrase) is a sequence of expressions.

An expression can include symbols, that trigger computation (totally symbol dependent, see Minimal Program).

Single dependency is suckless.org's libgrapheme, for unicode support in source code.
(like other suckless.org programs - `st`, `dwm` - it is excellent).
Also, relies on one of FreeBSD's specific libc function (so, probably does not compile on GNU/Linux).
Inspired by nanopass; several small steps to evaluation (inefficient most of the time).

