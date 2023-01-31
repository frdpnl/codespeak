# codespeak
A personal programming language.

This is a prototype, meant to explore language design ideas, interpreters and one day compilers.

## Phrase

A phrase is a sequence of expressions, separated by `;`.

For example:

`2 * 3.; print it` displays 6. (stdout)

`it` is a builtin symbol that stores the previous expression's value

`2, +, n ; call it A` defines symbol A, the list 2, +, n (n is undefined at this point, still a valid expression)

### Symbolic reduction

Note: `()` expresses symbol application or a single value.
The parenthesized expression ends up reduced to a single value.
After evaluation, no `()` are left.

Whitespace expected to separate words.

`2 = 2.` 0 (false), strict equality

`2 ~= 2.0` 1, more or less equal

`2 /= 2.` 1, different types

`(2,) ~= (2.,)` 1 (true)

`(2,3,+) = (2, (2 + 1), +)` 1

### Builtin symbols

`3 ; call it p` defines the symbol p, worth 3

`3; print it` displays (stdout) 3

`,` defines a list

`2, (3,4),4` list includes a sub-list

`2, +, 3` also a list 

`list` changes the rest of the expression into a list

`list 1 + 3` the list 1, +, 3

`do` means evaluate following list as an expression
`do` is the inverse of `list`.

`1 + 2 ; it, *, 2 ; do it` 6

`2, +, 3; do it` 5

`list 1 + 2; do it` 3, or `do (list 1 + 2)` but sequence preferred

`1, +, 3; list it; do it; do it` 4, or `do (do (list (list 1 + 3)))`

`2, *, n ; call it a ; call 2 n ; do a` 4 

`2, -, n ; call it a ; call 2 n ; id a` the list 2, -, 2

`eval` evaluates its argument (strictly evaluated)

`2, n, 3; call it A; call * n; do A; print it` 

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

*Language does not need grammar, but grammar needs language.* (Sylvain Auroux interview)

A program is a sequence of paragraphs (not here yet).

A paragraph is a sequence of phrases (not here yet).

A phrase is a sequence of expressions.

An expression can include symbols, that trigger computation (totally symbol dependent, see Minimal Program).

Single dependency is suckless.org's libgrapheme, for unicode support in source code.
(like other suckless.org programs - `st`, `dwm` - it is excellent).
Also, relies on one of FreeBSD's specific libc function (so, probably does not compile on GNU/Linux).
Inspired by nanopass; several small steps to evaluation (inefficient most of the time).

Long-term objective is to compile to WASM (yes, not here yet).

