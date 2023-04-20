/*
 * This file is part of Codespeak.
 *
 * Codespeak is free software: you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, 
 * or (at your option) any later version.
 *
 * Codespeak is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; 
 * without even the implied warranty of MERCHANTABILITY 
 * or FITNESS FOR A PARTICULAR PURPOSE. 
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with Codespeak. 
 * If not, see <https://www.gnu.org/licenses/>.
 * 
 *
 * 13.10.2022 created.
 *
 * gcc -std=gnu99 -Wall -g -I./libgrapheme/include -L./libgrapheme/lib this_file -lgrapheme 
 *
 */

#include <grapheme.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>

bool Dbg = false;

/* ----- words to evaluate --------- */

typedef enum { SEP, LEFT, RIGHT, STR } wtype;  

/* max size for a recognized word */
#define WSZ 128

typedef struct { 
	wtype t;
	char v[WSZ]; /* wasteful, but limited */
} Word;

typedef struct {
	size_t n;
	Word *w;
} Expr;

static Expr *
expr() {
	Expr *a = malloc(sizeof(*a));
	assert(a != NULL);
	a->n = 0;
	a->w = NULL;
	return a;
}

static void
free_x(Expr *a) {
	assert(a != NULL);
	free(a->w);
	free(a);
}

static Word *
word(wtype a) {
	Word *b = malloc(sizeof(*b));
	assert(b != NULL);
	b->t = a;
	b->v[0] = '\0';
	return b;
}

static Word *
word_str(char *a, size_t n) {
	if (n >= WSZ) {
		printf("? %s: word to big\n", 
				__FUNCTION__);
		return NULL;
	}
	Word *b = word(STR);
	memmove(b->v, a, n*sizeof(char));
	b->v[n] = '\0';
	return b;
}

static Expr *
push_xz(Expr *a, Word *b) {
	assert(a != NULL);
	Word *c = malloc((a->n+1)*sizeof(Word));
	assert(c != NULL);
	memmove(c+a->n, b, sizeof(Word));
	if (a->n) {
		memmove(c, a->w, a->n*sizeof(Word));
		free(a->w);
	}
	a->w = c;
	a->n++;
	free(b);
	return a;
}

/* ----- extract words from raw text ----- */

static Expr *
exp_of_words(char *a) {
	size_t boff = 0;
	char buf[WSZ];
	buf[0] = '\0';
	size_t read;
	Expr *b = expr();
	Word *w = NULL;
	for (size_t off = 0; a[off] != '\0'; off += read) {
		read = grapheme_next_character_break_utf8(
				a+off, SIZE_MAX);
		if (strncmp("(", a+off, read) == 0) {
			if (boff) {
				w = word_str(buf, boff);
				b = push_xz(b, w);
				boff = 0;
			}
			w = word(LEFT);
			b = push_xz(b, w);
			continue;
		} 
		if (strncmp(")", a+off, read) == 0) {
			if (boff) {
				w = word_str(buf, boff);
				b = push_xz(b, w);
				boff = 0;
			}
			w = word(RIGHT);
			b = push_xz(b, w);
			continue;
		} 
		if (strncmp(",", a+off, read) == 0) {
			if (boff) {
				w = word_str(buf, boff);
				b = push_xz(b, w);
				boff = 0;
			}
			w = word(SEP);
			b = push_xz(b, w);
			continue;
		} 
		if (isspace((int)*(a+off))) {
			if (boff) {
				w = word_str(buf, boff);
				b = push_xz(b, w);
				boff = 0;
			}
			continue;
		} 
		if (boff+read >= WSZ) {
			printf("\n? %s: word too big (%luB)!\n", 
					__FUNCTION__,
					boff+read);
			free_x(b);
			return NULL;
		}
		/* default case: add character to current word */
		memmove(buf+boff, a+off, read*sizeof(char));
		boff += read;
		buf[boff] = '\0';
	}
	if (boff) {
		w = word_str(buf, boff);
		b = push_xz(b, w);
	}
	return b;
}

/* ----- From words to semes (increased semantics) ----- */

typedef enum {SNIL, SNAT, SREA, SSYM, SLST, SSEQ} stype;

typedef union Sem_ Sem;

typedef struct List_ {
	size_t n;
	Sem *s;
} List;

typedef union Sem_ {
	struct {
		stype t;
	} hdr;
	struct {
		stype t;
		long long v;
	} nat;
	struct {
		stype t;
		double v;
	} rea;
	struct {
		stype t;
		char v[WSZ];
	} sym;
	struct {
		stype t;
		List v;
	} lst;
	struct {
		stype t;
		List v;
	} seq;
} Sem;

static void
print_s(Sem *a) {
	assert(a != NULL);
	switch (a->hdr.t) {
		case SNIL:
			printf("Nil ");
			break;
		case SNAT:
			printf("%lldN ", a->nat.v);
			break;
		case SREA:
			printf("%.2lfR ", a->rea.v);
			break;
		case SSYM:
			printf("%s ", a->sym.v);
			break;
		case SLST:
			printf("{ ");
			for (size_t i=0; i<a->lst.v.n; ++i) {
				print_s(a->lst.v.s+i);
			}
			printf("} ");
			break;
		case SSEQ:
			printf("( ");
			for (size_t i=0; i<a->seq.v.n; ++i) {
				print_s(a->seq.v.s+i);
			}
			printf(") ");
			break;
		default:
			printf("? %s: unknown seme\n",
					__FUNCTION__);
	}
}

static Sem *
sem_nil() {
	Sem *b = malloc(sizeof(*b));
	assert(b != NULL);
	b->hdr.t = SNIL;
	return b;
}

static Sem *
sem_nat(long long a) {
	Sem *b = malloc(sizeof(*b));
	assert(b != NULL);
	b->nat.t = SNAT;
	b->nat.v = a;
	return b;
}

static Sem *
sem_rea(double a) {
	Sem *b = malloc(sizeof(*b));
	assert(b != NULL);
	b->rea.t = SREA;
	b->rea.v = a;
	return b;
}

static Sem *
sem_sym(char *a) {
	Sem *b = malloc(sizeof(*b));
	assert(b != NULL);
	b->sym.t = SSYM;
	strncpy(b->sym.v, a, WSZ);
	b->sym.v[WSZ-1] = '\0';
	return b;
}

static Sem *
sem_seq() {
	Sem *b = malloc(sizeof(*b));
	assert(b != NULL);
	b->lst.t = SSEQ;
	b->lst.v.n = 0;
	b->lst.v.s = NULL;
	return b;
}

static void 
free_s(Sem *a) {
	assert(a != NULL);
	switch (a->hdr.t) {
		case SNIL:
		case SNAT:
		case SREA:
		case SSYM:
			break;
		case SSEQ:
			for (size_t i=0; i<a->seq.v.n; ++i) {
				free_s(a->seq.v.s +i);
			}
			free(a->seq.v.s);
			break;
		case SLST:
			for (size_t i=0; i<a->lst.v.n; ++i) {
				free_s(a->lst.v.s +i);
			}
			free(a->lst.v.s);
			break;
		default:
			printf("? %s: unknown seme\n",
					__FUNCTION__);
	}
}

static Sem* 
isnat(Word *a) {
	if (a->t != STR) {
		printf("? %s word is not a string\n",
				__FUNCTION__);
		return NULL;
	}
	errno = 0;
	char *end = NULL;
	long long n = strtoll(a->v, &end, 10);
	if (errno == EINVAL) {
		printf("? %s: natural number invalid %s\n", 
				__FUNCTION__, a->v);
		return NULL;
	}
	if (errno == ERANGE) {
		printf("? %s: natural number out of range %s\n", 
				__FUNCTION__, a->v);
		return NULL;
	} 
	if (*end != '\0') {
		return NULL;
	} 
	return sem_nat(n);
}

static Sem *
isrea(Word *a) {
	if (a->t != STR) {
		printf("? %s word is not a string\n",
				__FUNCTION__);
		return NULL;
	}
	char *err = NULL;
	errno = 0;
	double f = strtod(a->v, &err);
	if (errno == 0 && *err == '\0') {
		return sem_rea(f);
	}
	if (errno == ERANGE) {
		if (f == 0) {
			printf("? %s: real underflow %s\n", 
					__FUNCTION__, a->v);
		} else {
			printf("? %s: real overflow %s\n", 
					__FUNCTION__, a->v);
		}
	}
	return NULL;
}

static Sem *
issym(Word *a) {
	if (a->t != STR) {
		printf("? %s: word is not a string\n",
				__FUNCTION__);
		return NULL;
	}
	return sem_sym(a->v);
}

static Sem *
push_s(Sem *a, Sem *b) {
	assert(a != NULL && "seq or lst is null");
	assert(b != NULL && "pushed seme is null");
	if (a->hdr.t != SSEQ && a->hdr.t != SLST) {
		printf("? %s: not a seq or lst seme\n",
				__FUNCTION__);
		return NULL;
	}
	Sem *c = malloc((a->seq.v.n +1)*sizeof(Sem));
	assert(c != NULL);
	memmove(c, a->seq.v.s, a->seq.v.n * sizeof(Sem));
	memmove(c + a->seq.v.n, b, sizeof(Sem));
	++(a->seq.v.n);
	free(a->seq.v.s);
	a->seq.v.s = c;
	return a;
}

static Sem *
lst_of(Sem *a) {
	assert(a != NULL);
	if (a->hdr.t != SSEQ && a->hdr.t != SLST) {
		printf("? %s: not a seq or lst seme\n",
				__FUNCTION__);
		free_s(a);
		free(a);
		return NULL;
	}
	if (a->hdr.t == SSEQ && a->seq.v.n > 1) {
		printf("? %s: cannot add a list element to a seq-seme\n",
				__FUNCTION__);
		free_s(a);
		free(a);
		return NULL;
	}
	a->hdr.t = SLST;
	return a;
}

static Sem *
seme_of_exp_part(Expr *a, size_t from, size_t tox) {
	if (from == tox) {
		return sem_nil();
	}
	size_t iw = from;
	Sem *b = sem_seq();
	Sem *c = NULL;
	size_t inpar = 0;
	bool lst_expect1 = false;
	bool pushed = true; /* for clarity */
	while (iw < tox) {
		pushed = false;
		switch (a->w[iw].t) {
			case SEP:
				b = lst_of(b);
				if (b == NULL) {
					return NULL;
				}
				if (b->lst.v.n == 0 || lst_expect1) {
					c = sem_nil();
					b = push_s(b, c);
					free(c);
				}
				lst_expect1 = true;
				break;
			case LEFT:
				if (b->hdr.t == SLST && !lst_expect1) {
					printf("? %s: unexpected list element\n",
							__FUNCTION__);
					free_s(b);
					free(b);
					return NULL;
				}
				inpar = 1;
				for (size_t ip = iw+1; ip < tox; ++ip) {
					if (a->w[ip].t == LEFT) {
						++inpar;
					} else if (a->w[ip].t == RIGHT) {
						--inpar;
						if (inpar == 0) {
							c = seme_of_exp_part(a, iw+1, ip);
							if (c == NULL) {
								free_s(b);
								return NULL;
							}
							b = push_s(b, c);
							free(c);
							if (b == NULL) {
								return NULL;
							}
							iw = ip;
							pushed = true;
							break;
						}
					}
				}
				if (inpar != 0) {
					printf("? %s: unmatched %s\n",
							__FUNCTION__,
							inpar < 0 ? ")" : "(");
					free_s(b);
					free(b);
					return NULL;
				}
				break;
			case RIGHT:
				printf("? %s: unmatched )\n",
						__FUNCTION__);
				free_s(b);
				free(b);
				return NULL;
			case STR:
				if (b->hdr.t == SLST && !lst_expect1) {
					printf("? %s: unexpected list element\n",
							__FUNCTION__);
					free_s(b);
					free(b);
					return NULL;
				}
				c = isnat(a->w+iw);
				if (c == NULL) {
					c = isrea(a->w+iw);
				}
				if (c == NULL) {
					c = issym(a->w+iw);
				}
				if (c == NULL) {
					printf("? %s: unknown word\n",
							__FUNCTION__);
					free_s(b);
					free(b);
					return NULL;
				}
				b = push_s(b, c);
				free(c);
				if (b == NULL) {
					return NULL;
				}
				pushed = true;
				break;
			default:
				printf("? %s: unexpected word\n",
						__FUNCTION__);
				free_s(b);
				free(b);
				return NULL;
		}
		if (pushed && b->hdr.t == SLST) {
			lst_expect1 = false;
			pushed = false;
		}
		iw++;
	}
	return b;
}

static Sem *
seme_of_exp(Expr *a) {
	assert(a != NULL);
	if (a->n == 0) {
		return sem_nil();
	} 
	return seme_of_exp_part(a, 0, a->n);
}

/* ----- Evaluation, pass 1 ----- */

typedef enum {VNIL, VNAT, VREA, VOPE, VFUN, VSYM, VLST, VSEQ} vtype;

typedef union Val_ Val;

typedef struct List_v_ {
	size_t n;
	Val **v;
} List_v;

/* Special environment symbols: */
#define IT "it"
#define ITNAME "__it__"
#define ENDF "end"
#define ENDIF "endif"
#define LOOPNAME "__loop__"
#define LOOPNEST "__nested_loops__"

typedef enum {
	FATAL,
	NORM,
	IFSKIP,	/* for 'if, skip until 'else or 'endif */
	FUNDEF,	/* we're processing a (multiline) function definition */
	BACKTRACK,
	RETURN,	/* returning from function (short circuit) */
	LOOPDEF,
	STOP	/* interrupt loop */
} istate;

typedef enum {
	FAIL,
	OK,
	NOP,
	SKIP,
	DEF,
	BACK,
	RET,
	LOOP,
	INT
} rc;

typedef struct {
	char name[WSZ];
	Val *v;
} Symval;

typedef struct Env_ {
	istate state;
	size_t n;
	Symval **s;
	struct Env_ *parent;
} Env;

typedef struct {
	rc code;
	Val *v;
} Ires;		/* return type for reduce */

typedef union Val_ {
	struct {
		vtype t;
	} hdr;
	struct {
		vtype t;
		long long v;
	} nat;
	struct {
		vtype t;
		double v;
	} rea;
	struct {
		vtype t;
		int prio;
		Ires (*v)(Env *e, Val *s, size_t p);
		int arity;
		char name[WSZ];
	} symop;
	struct {
		vtype t;
		List_v param;
		List_v body;
		char name[WSZ];
	} symf;
	struct {
		stype t;
		char v[WSZ];
	} sym;
	struct {
		vtype t;
		List_v v;
	} lst;
	struct {
		vtype t;
		List_v v;
	} seq;
} Val;

static void print_v(Val *a, bool abr);

static void
printx_v(Val *a, bool abr, const char *pfx) {
	if (a == NULL) {
		printf("\n? %s: NULL\n", __FUNCTION__);
		return;
	}
	switch (a->hdr.t) {
		case VNIL:
			printf("Nil ");
			break;
		case VNAT:
			printf("%lld ", a->nat.v);
			break;
		case VREA:
			printf("%.2lf ", a->rea.v);
			break;
		case VOPE:
			printf("`%s ", a->symop.name);
			break;
		case VFUN:
			if (abr) {
				printf("%s`%s ", pfx, a->symf.name);
				break;
			}
			printf("\n%s `%s (", pfx, a->symf.name);
			for (size_t i=0; i<a->symf.param.n; ++i) {
				print_v(a->symf.param.v[i], abr);
			}
			printf(") [%lu]:", a->symf.body.n);
			for (size_t i=0; i<a->symf.body.n; ++i) {
				size_t N = 2;
				if (i > N && i < a->symf.body.n - N) {
					if (i == N+1) {
						printf("\n%s    ", pfx);
					}
					printf(".");
				} else {
					printf("\n%s    ", pfx);
					print_v(a->symf.body.v[i], abr);
				}
			}
			break;
		case VSYM:
			printf("'%s ", a->sym.v);
			break;
		case VLST:
			if (abr) {
				printf("%s{ x%lu } ", pfx, a->lst.v.n);
				break;
			}
			printf("%s{ ", pfx);
			for (size_t i=0; i<a->lst.v.n; ++i) {
				printx_v(a->lst.v.v[i], abr, pfx);
			}
			printf("} ");
			break;
		case VSEQ:
			if (abr) {
				printf("%s( x%lu ) ", pfx, a->seq.v.n);
				break;
			}
			printf("%s( ", pfx);
			for (size_t i=0; i<a->seq.v.n; ++i) {
				printx_v(a->seq.v.v[i], abr, pfx);
			}
			printf(") ");
			break;
		default:
			printf("? %s: unknown value\n",
					__FUNCTION__);
	}
}
static void 
print_v(Val *a, bool abr) {
	printx_v(a, abr, "");
}
static void
print_istate(istate s) {
	switch (s) {
		case NORM:
			printf("Ok ");
			break;
		case IFSKIP:
			printf("Skip ");
			break;
		case FUNDEF:
			printf("Fun ");
			break;
		case RETURN:
			printf("Return ");
			break;
		case LOOPDEF:
			printf("Loop ");
			break;
		case STOP:
			printf("Stop ");
			break;
		case BACKTRACK:
			printf("Backtrack ");
			break;
		case FATAL:
			printf("Fatal ");
			break;
	}
}
static void
print_code(rc s) {
	switch (s) {
		case FAIL:
			printf("Fail ");
			break;
		case OK:
			printf("Ok ");
			break;
		case NOP:
			printf("Nop ");
			break;
		case SKIP:
			printf("Skip ");
			break;
		case DEF:
			printf("Def ");
			break;
		case BACK:
			printf("Back ");
			break;
		case RET:
			printf("Ret ");
			break;
		case LOOP:
			printf("Loop ");
			break;
		case INT:
			printf("Int ");
			break;
	}
}
static void
print_rc(Ires a) {
	printf("[");
	print_code(a.code);
	printf(", ");
	if (a.v != NULL) {
		print_v(a.v, false);
	} else {
		printf("null");
	}
	printf(" ]");
}

static void 
free_v(Val *a) {
	if (a == NULL) {
		return;
	}
	switch (a->hdr.t) {
		case VNIL:
		case VNAT:
		case VREA:
		case VSYM:
		case VOPE: /* symop holds postate to val in Syms */
			break;
		case VFUN:
			for (size_t i=0; i<a->symf.param.n; ++i) {
				free_v(a->symf.param.v[i]);
			}
			free(a->symf.param.v);
			for (size_t i=0; i<a->symf.body.n; ++i) {
				free_v(a->symf.body.v[i]);
			}
			free(a->symf.body.v);
			break;
		case VSEQ:
			for (size_t i=0; i<a->seq.v.n; ++i) {
				free_v(a->seq.v.v[i]);
			}
			free(a->seq.v.v);
			break;
		case VLST:
			for (size_t i=0; i<a->lst.v.n; ++i) {
				free_v(a->lst.v.v[i]);
			}
			free(a->lst.v.v);
			break;
		default:
			printf("? %s: unknown value\n",
					__FUNCTION__);
	}
	free(a);
}

static bool
istrue_v(Val *a) {
	assert(a != NULL);
	if (a->hdr.t == VNIL) {
		return false;
	}
	if (a->hdr.t == VNAT) {
		return a->nat.v != 0;
	}
	if (a->hdr.t == VREA) {
		return a->rea.v != 0.;
	}
	if (a->hdr.t == VOPE) {
		return a->symop.v != NULL;
	}
	if (a->hdr.t == VFUN) {
		return (a->symf.param.n != 0  
				&& a->symf.body.n != 0);
	}
	if (a->hdr.t == VSYM) {
		return true;
	}
	if (a->hdr.t == VLST) {
		return a->lst.v.n > 0;
	}
	printf("? %s: unsupported value\n",
			__FUNCTION__);
	return false;
}
static bool
isequal_v(Val *a, Val *b) {
	assert(a != NULL && b != NULL);
	if (a->hdr.t != b->hdr.t) {
		return false;
	}
	if (a->hdr.t == VNIL) {
		return true;
	}
	if (a->hdr.t == VNAT) {
		return (a->nat.v == b->nat.v);
	}
	if (a->hdr.t == VREA) {
		return (a->rea.v == b->rea.v);
	}
	if (a->hdr.t == VOPE) {
		return (a->symop.v == b->symop.v);
	}
	if (a->hdr.t == VFUN) {
		if (strncmp(a->symf.name, b->symf.name, WSZ*sizeof(char)) != 0) {
			return false;
		}
		if (a->symf.param.n != b->symf.param.n) {
			return false;
		}
		if (a->symf.body.n != b->symf.body.n) {
			return false;
		}
		for (size_t i=0; i<a->symf.body.n; ++i) { 
			if (!isequal_v(a->symf.body.v[i], b->symf.body.v[i])) {
				return false;
			}
		}
		return true;
	}
	if (a->hdr.t == VSYM) {
		return (strncmp(a->sym.v, b->sym.v, WSZ*sizeof(char)) == 0);
	}
	if (a->hdr.t == VLST) {
		for (size_t i=0; i<a->lst.v.n; ++i) { 
			if (!isequal_v(a->lst.v.v[i], b->lst.v.v[i])) {
				return false;
			}
		}
		return true;
	}
	if (a->hdr.t == VSEQ) {
		for (size_t i=0; i<a->seq.v.n; ++i) { 
			if (!isequal_v(a->seq.v.v[i], b->seq.v.v[i])) {
				return false;
			}
		}
		return true;
	}
	printf("? %s: unsupported value\n",
			__FUNCTION__);
	return false;
}

static bool
isequiv_v(Val *a, Val *b) {
	assert(a != NULL && b != NULL);
	if (a->hdr.t == VNAT && b->hdr.t == VREA) {
		return ((double)a->nat.v == b->rea.v);
	}
	if (a->hdr.t == VREA && b->hdr.t == VNAT) {
		return (a->rea.v == (double)b->nat.v);
	}
	if (a->hdr.t != b->hdr.t) {
		return false;
	}
	if (a->hdr.t == VNIL) {
		return true;
	}
	if (a->hdr.t == VNAT) {
		return (a->nat.v == b->nat.v);
	}
	if (a->hdr.t == VREA) {
		return (a->rea.v == b->rea.v);
	}
	if (a->hdr.t == VOPE) {
		return (a->symop.v == b->symop.v);
	}
	if (a->hdr.t == VFUN) {
		return (strncmp(a->symf.name, b->symf.name, WSZ*sizeof(char)) == 0);
	}
	if (a->hdr.t == VSYM) {
		if (strncmp(a->sym.v, b->sym.v, WSZ*sizeof(char)) != 0) {
			return false;
		}
		if (a->symf.param.n != b->symf.param.n) {
			return false;
		}
		if (a->symf.body.n != b->symf.body.n) {
			return false;
		}
		for (size_t i=0; i<a->symf.body.n; ++i) { 
			if (!isequiv_v(a->symf.body.v[i], b->symf.body.v[i])) {
				return false;
			}
		}
		return true;
	}
	if (a->hdr.t == VLST) {
		for (size_t i=0; i<a->lst.v.n; ++i) { 
			if (!isequiv_v(a->lst.v.v[i], b->lst.v.v[i])) {
				return false;
			}
		}
		return true;
	}
	printf("? %s: unsupported value\n",
			__FUNCTION__);
	return false;
}
static Val *
copy_v(Val *a) {
	assert(a != NULL);
	Val *b = malloc(sizeof(*b));
	memcpy(b, a, sizeof(Val));
	if (a->hdr.t == VSEQ || a->hdr.t == VLST) {
		if (a->seq.v.n > 0) {
			b->seq.v.v = malloc(a->seq.v.n * sizeof(Val*));
			for (size_t i=0; i<a->seq.v.n; ++i) {
				b->seq.v.v[i] = copy_v(a->seq.v.v[i]);
			}
		} else {
			b->seq.v.v = NULL;
		}
	} else if (a->hdr.t == VFUN) {
		if (a->symf.param.n > 0) {
			b->symf.param.v = malloc(a->symf.param.n * sizeof(Val*));
			for (size_t i=0; i<a->symf.param.n; ++i) {
				b->symf.param.v[i] = copy_v(a->symf.param.v[i]);
			}
		} else {
			b->symf.param.v = NULL;
		}
		if (a->symf.body.n > 0) {
			b->symf.body.v = malloc(a->symf.body.n * sizeof(Val*));
			for (size_t i=0; i<a->symf.body.n; ++i) {
				b->symf.body.v[i] = copy_v(a->symf.body.v[i]);
			}
		} else {
			b->symf.body.v = NULL;
		}
	}
	return b;
}

static List_v 
push_l(List_v a, Val *b) {
	assert(b != NULL && "val is null");
	Val **c = malloc((a.n +1)*sizeof(Val*));
	assert(c != NULL && "list realloc failed");
	if (a.n > 0) {
		memmove(c, a.v, a.n * sizeof(Val*));
	}
	c[a.n] = b;
	if (a.n > 0) {
		free(a.v);
	}
	++(a.n);
	a.v = c;
	return a;
}
static Val *
push_v(vtype t, Val *a, Val *b) {
	assert(b != NULL && "val is null");
	if (t != VSEQ && t != VLST) {
		printf("? %s: not a seq or list\n",
				__FUNCTION__);
		return NULL;
	}
	if (a == NULL) {
		a = malloc(sizeof(*a));
		assert(a != NULL);
		a->hdr.t = t;
		a->seq.v.n = 0;
		a->seq.v.v = NULL;
	}
	a->seq.v = push_l(a->seq.v, b);
	return a;
}
static void
print_symval(Symval *a, const char *pfx) {
	assert(a != NULL);
	printf("%s = ", a->name);
	printx_v(a->v, false, pfx);
}
static void
print_env(Env *a, const char *col1) {
	assert(a != NULL);
	printf("%s env: ", col1);
	printf("state = "); print_istate(a->state); printf("\n");
	for (size_t i=0; i<a->n; ++i) {
		printf("%s ", col1);
		print_symval(a->s[i], col1);
		printf("\n");
	}
	if (a->parent) {
		printf("%s parent ", col1);
		print_env(a->parent, col1);
	}
}
static Symval *
symval(char *a, Val *b) {
	assert(a != NULL && "name is null");
	assert(b != NULL && "val is null");
	if (strlen(a) == 0) {
		printf("? %s: empty name\n",
				__FUNCTION__);
		return NULL;
	};
	if (strlen(a) >= WSZ) {
		printf("? %s: symbol name too long (%s)\n",
				__FUNCTION__, a);
		return NULL;
	};
	Symval *c = malloc(sizeof(*c));
	strncpy(c->name, a, WSZ*sizeof(char));
	c->v = copy_v(b);
	return c;
}
static void
free_symval(Symval *a) {
	assert(a != NULL);
	free_v(a->v);
	free(a);
}
static void 
free_env(Env *a, bool global) {
	if (a == NULL) {
		return;
	}
	for (size_t i=0; i<a->n; ++i) {
		free_symval(a->s[i]);
	}
	free(a->s);
	if (global && a->parent) {
		free_env(a->parent, global);
	}
	free(a);
}
static Symval *
lookup_id(Env *a, char *b, bool global, size_t *id) {
	assert(a != NULL && "env is null");
	assert(b != NULL && "name is null");
	if (strlen(b) == 0) {
		printf("? %s: symbol name null\n",
				__FUNCTION__);
		return NULL;
	}
	/* resolve in local env first */
	for (size_t i=0; i<a->n; ++i) {
		Symval *c = a->s[i];
		if (strncmp(c->name, b, sizeof(c->name)) == 0) {
			if (id != NULL) {
				*id = i;
			}
			return c;
		}
	}
	if (global && a->parent) {
		return lookup_id(a->parent, b, global, id);
	}
	return NULL;
}
static Val *
lookup(Env *a, char *b, bool global, bool iterate) {
	assert(a != NULL && "env null");
	assert((b != NULL && strlen(b) != 0) && "symbol name null");
	Symval *sv = lookup_id(a, b, global, NULL);
	if (sv == NULL) {
		return NULL;
	}
	if (iterate) {
		Val *c = sv->v; 
		while (c->hdr.t == VSYM) {
			sv = lookup_id(a, c->sym.v, global, NULL);
			if (sv == NULL) {
				return NULL;
			}
			c = sv->v;
		}
	}
	return sv->v;
}

static bool
added_sym(Env *a, Symval *b, bool err) {
	assert(a != NULL && "environment null");
	assert(b != NULL && "symbol null");
	if (lookup(a, b->name, false, false) != NULL) {
		if (err) {
			printf("? %s: symbol already defined (%s)\n",
					__FUNCTION__, b->name);
		}
		return false;
	}
	Symval **c = malloc((a->n +1)*sizeof(Symval*));
	if (a->n > 0) {
		memmove(c, a->s, a->n * sizeof(Symval*));
	}
	c[a->n] = b;
	if (a->n > 0) {
		free(a->s);
	}
	++(a->n);
	a->s = c;
	return true;
}
static bool
upded_sym(Env *a, Symval *b, bool err) {
	assert(a != NULL && "environment null");
	assert(b != NULL && "symbol null");
	assert(strlen(b->name) != 0);
	size_t id;
	Symval *c = lookup_id(a, b->name, false, &id);
	if (c == NULL) {
		if (err) {
			printf("? %s: symbol not found (%s)\n",
					__FUNCTION__, b->name);
		}
		return false;
	}
	free_symval(c);
	a->s[id] = b;
	return true;
}
static bool
stored_sym(Env *a, Symval *b) {
	if (upded_sym(a, b, false)) {
		return true;
	}
	if (added_sym(a, b, true)) {
		return true;
	}
	return false;
}

/* -------- operation symbol definitions ------------
 * Reduce interface:
 * reduce or modify the input sequence, but do not free seq
 * 1. work off copies of arguments
 * 2. produce a fresh value (can be one of the copies)
 * 3. replace operation application with that fresh value
 * (delete previous values in the expression, add new one)
 * 4. cleanup working copies
 *
 * Interpreter returns a single val, not a seq.
 */

static Ires eval_norm(Env *e, Val *a, bool look);
static Ires solve_sym(Env *e, Val *a, bool look);
static Ires solve_lst(Env *e, Val *a, bool look);
static istate eval(Env *e, Val *a);

static bool 
infixed(size_t p, size_t n) {
	return (p > 0 && p < n-1);
}
static bool 
prefixed1(size_t p, size_t n) {
	return (p >= 0 && p < n-1);
}
static bool 
prefixed2(size_t p, size_t n) {
	return (p >= 0 && p < n-2);
}

static bool
set_infix_arg(Env *e, Val *s, size_t p, Val **pa, bool looka, Val **pb, bool lookb) {
	*pa = *pb = NULL;
	if (!infixed(p, s->seq.v.n)) {
		printf("? %s: operator not infixed\n", 
				__FUNCTION__);
		return false;
	}
	Ires rc = eval_norm(e, s->seq.v.v[p-1], looka);
	if (rc.state != NORM) {
		return false;
	}
	*pa = rc.v;
	rc = eval_norm(e, s->seq.v.v[p+1], lookb);
	if (rc.state != NORM) {
		free_v(*pa);
		*pa = NULL;
		return false;
	}
	*pb = rc.v;
	return true;
}
static bool
set_prefix1_arg(Env *e, Val *s, size_t p, Val **pa, bool looka) {
	*pa = NULL;
	if (!prefixed1(p, s->seq.v.n)) {
		printf("? %s: symbol not prefixed to one argument\n", 
				__FUNCTION__);
		return false;
	}
	Ires rc = solve_sym(e, s->seq.v.v[p+1], looka);
	if (rc.code != OK && rc.code != NOP) {
		return false;
	}
	*pa = rc.v;
	return true;
}
static bool
set_prefix2_arg(Env *e, Val *s, size_t p, Val **pa, bool looka, Val **pb, bool lookb) {
	*pa = *pb = NULL;
	if (!prefixed2(p, s->seq.v.n)) {
		printf("? %s: symbol not prefixed to 2 arguments\n", 
				__FUNCTION__);
		return false;
	}
	Ires rc = eval_norm(e, s->seq.v.v[p+1], looka);
	if (rc.state != NORM) {
		return false;
	}
	*pa = rc.v;
	rc = eval_norm(e, s->seq.v.v[p+2], lookb);
	if (rc.state != NORM) {
		free_v(*pa);
		*pa = NULL;
		return false;
	}
	*pb = rc.v;
	return true;
}
static bool
set_prefixn_arg(Env *e, Val *s, size_t p, Val **pa, bool looka) {
	*pa = NULL;
	if (p == s->seq.v.n -1) {
		printf("? %s: arguments expected for ", __FUNCTION__);
		print_v(s->seq.v.v[p], true);
		printf("\n");
		return false;
	}
	Val *a = malloc(sizeof(*a));
	a->hdr.t = VSEQ;
	a->seq.v.n = s->seq.v.n -p-1; 
	a->seq.v.v = malloc((a->seq.v.n) * sizeof(Val*));
	Ires rc;
	for (size_t i=p+1; i < s->seq.v.n; ++i) {
		Val *si = s->seq.v.v[i];
		rc = eval_norm(e, si, looka);
		if (rc.state != NORM) {
			free_v(a);
			printf("? %s: %luth argument unknown\n", 
					__FUNCTION__, i);
			return false;
		}
		a->seq.v.v[i-p-1] = rc.v;
	}
	*pa = a;
	return true;
}
static void
upd_infix(Val *s, size_t p, Val *a) {
	/* consumed 2 seq items */
	for (size_t i=p-1; i < s->seq.v.n && i < p+2; ++i) {
		free_v(s->seq.v.v[i]);
	}
	s->seq.v.v[p-1] = a;
	for (size_t i=p+2; i < s->seq.v.n; ++i) {
		s->seq.v.v[i-2] = s->seq.v.v[i];
	}
	s->seq.v.n -= 2;
}
static void
upd_prefixk(Val *s, size_t p, Val *a, size_t k) {
	/* consume k+1 seq item */
	for (size_t i=p; i < s->seq.v.n && i < p+k+1; ++i) {
		free_v(s->seq.v.v[i]);
	}
	s->seq.v.v[p] = a;
	for (size_t i=p+k+1; i < s->seq.v.n; ++i) {
		s->seq.v.v[i-k] = s->seq.v.v[i];
	}
	s->seq.v.n -= k;
}
static void
upd_prefix0(Val *s, size_t p, Val *a) {
	/* consume 0 seq item */
	upd_prefixk(s, p, a, 0);
}
static void
upd_prefix1(Val *s, size_t p, Val *a) {
	/* consume 1 seq item */
	upd_prefixk(s, p, a, 1);
}
static void
upd_prefix2(Val *s, size_t p, Val *a) {
	/* consume 2 seq item */
	upd_prefixk(s, p, a, 2);
}
static void
upd_prefixall(Val *s, size_t p, Val *a) {
	/* consume n-p-1 (all remaining items) seq item */
	upd_prefixk(s, p, a, s->seq.v.n - p - 1);
}

static Ires 
op_mul(Env *e, Val *s, size_t p) {
	Ires rc = (Ires) {FATAL, NULL};
	Val *a, *b;
	if (!set_infix_arg(e, s, p, &a, true, &b, true)) {
		return rc;
	}
	if ((a->hdr.t != VNAT && a->hdr.t != VREA)
			|| (b->hdr.t != VNAT && b->hdr.t != VREA)) {
		printf("? %s: arguments not numbers\n", 
				__FUNCTION__);
		free_v(a);
		free_v(b);
		return rc;
	}
	if (a->hdr.t == VNAT && b->hdr.t == VNAT) {
		a->nat.v *= b->nat.v;
	} else if (a->hdr.t == VNAT && b->hdr.t == VREA) {
		a->hdr.t = VREA;
		a->rea.v = (double)a->nat.v * b->rea.v;
	} else if (a->hdr.t == VREA && b->hdr.t == VNAT) {
		a->rea.v *= (double)b->nat.v;
	} else if (a->hdr.t == VREA && b->hdr.t == VREA) {
		a->rea.v *= b->rea.v;
	}
	free_v(b);
	upd_infix(s, p, a);
	rc = (Ires) {NORM, s};
	return rc;
}
static Ires 
op_div(Env *e, Val *s, size_t p) {
	Ires rc = (Ires) {FATAL, NULL};
	Val *a, *b;
	if (!set_infix_arg(e, s, p, &a, true, &b, true)) {
		return rc;
	}
	if ((a->hdr.t != VNAT && a->hdr.t != VREA)
			|| (b->hdr.t != VNAT && b->hdr.t != VREA)) {
		printf("? %s: arguments not numbers\n", 
				__FUNCTION__);
		free_v(a);
		free_v(b);
		return rc;
	}
	if ((b->hdr.t == VNAT && b->nat.v == 0) 
			|| (b->hdr.t == VREA && b->rea.v == 0.)) {
		printf("? %s: division by 0\n", 
				__FUNCTION__);
		free_v(a);
		free_v(b);
		return rc;
	}
	if (a->hdr.t == VNAT && b->hdr.t == VNAT) {
		a->nat.v /= b->nat.v;
	} else if (a->hdr.t == VNAT && b->hdr.t == VREA) {
		a->hdr.t = VREA;
		a->rea.v = (double)a->nat.v / b->rea.v;
	} else if (a->hdr.t == VREA && b->hdr.t == VNAT) {
		a->rea.v /= (double)b->nat.v;
	} else if (a->hdr.t == VREA && b->hdr.t == VREA) {
		a->rea.v /= b->rea.v;
	}
	free_v(b);
	upd_infix(s, p, a);
	rc = (Ires) {NORM, s};
	return rc;
}
static Ires
op_plu(Env *e, Val *s, size_t p) {
	Ires rc = (Ires) {FATAL, NULL};
	Val *a, *b;
	if (!set_infix_arg(e, s, p, &a, true, &b, true)) {
		return rc;
	}
	if ((a->hdr.t != VNAT && a->hdr.t != VREA)
			|| (b->hdr.t != VNAT && b->hdr.t != VREA)) {
		printf("? %s: arguments not numbers\n", 
				__FUNCTION__);
		free_v(a);
		free_v(b);
		return rc;
	}
	if (a->hdr.t == VNAT && b->hdr.t == VNAT) {
		a->nat.v += b->nat.v;
	} else if (a->hdr.t == VNAT && b->hdr.t == VREA) {
		a->hdr.t = VREA;
		a->rea.v = (double)a->nat.v + b->rea.v;
	} else if (a->hdr.t == VREA && b->hdr.t == VNAT) {
		a->rea.v += (double)b->nat.v;
	} else if (a->hdr.t == VREA && b->hdr.t == VREA) {
		a->rea.v += b->rea.v;
	}
	free_v(b);
	upd_infix(s, p, a);
	rc = (Ires) {NORM, s};
	return rc;
}
static Ires 
op_min(Env *e, Val *s, size_t p) {
	Ires rc = (Ires) {FATAL, NULL};
	Val *a, *b;
	if (!set_infix_arg(e, s, p, &a, true, &b, true)) {
		return rc;
	}
	if ((a->hdr.t != VNAT && a->hdr.t != VREA)
			|| (b->hdr.t != VNAT && b->hdr.t != VREA)) {
		printf("? %s: arguments not numbers in \"", 
				__FUNCTION__);
		print_v(s, false);
		printf("\"\n");
		free_v(a);
		free_v(b);
		return rc;
	}
	if (a->hdr.t == VNAT && b->hdr.t == VNAT) {
		a->nat.v -= b->nat.v;
	} else if (a->hdr.t == VNAT && b->hdr.t == VREA) {
		a->hdr.t = VREA;
		a->rea.v = (double)a->nat.v - b->rea.v;
	} else if (a->hdr.t == VREA && b->hdr.t == VNAT) {
		a->rea.v -= (double)b->nat.v;
	} else if (a->hdr.t == VREA && b->hdr.t == VREA) {
		a->rea.v -= b->rea.v;
	}
	free_v(b);
	upd_infix(s, p, a);
	rc = (Ires) {NORM, s};
	return rc;
}
static Ires
op_les(Env *e, Val *s, size_t p) {
	Ires rc = (Ires) {FATAL, NULL};
	Val *a, *b;
	if (!set_infix_arg(e, s, p, &a, true, &b, true)) {
		return rc;
	}
	/* to be expanded to other types */
	if ((a->hdr.t != VNAT && a->hdr.t != VREA)
			|| (b->hdr.t != VNAT && b->hdr.t != VREA)) {
		printf("? %s: arguments not numbers\n", 
				__FUNCTION__);
		free_v(a);
		free_v(b);
		return rc;
	}
	if (a->hdr.t == VNAT && b->hdr.t == VNAT) {
		a->nat.v = a->nat.v < b->nat.v;
	} else if (a->hdr.t == VNAT && b->hdr.t == VREA) {
		a->nat.v = a->nat.v < b->rea.v;
	} else if (a->hdr.t == VREA && b->hdr.t == VNAT) {
		a->hdr.t = VNAT;
		a->nat.v = a->nat.v < b->nat.v;
	} else if (a->hdr.t == VREA && b->hdr.t == VREA) {
		a->hdr.t = VNAT;
		a->nat.v = a->rea.v < b->rea.v;
	}
	free_v(b);
	upd_infix(s, p, a);
	rc = (Ires) {NORM, s};
	return rc;
}
static Ires
op_leq(Env *e, Val *s, size_t p) {
	Ires rc = (Ires) {FATAL, NULL};
	Val *a, *b;
	if (!set_infix_arg(e, s, p, &a, true, &b, true)) {
		return rc;
	}
	/* to be expanded to other types */
	if ((a->hdr.t != VNAT && a->hdr.t != VREA)
			|| (b->hdr.t != VNAT && b->hdr.t != VREA)) {
		printf("? %s: arguments not numbers\n", 
				__FUNCTION__);
		free_v(a);
		free_v(b);
		return rc;
	}
	if (a->hdr.t == VNAT && b->hdr.t == VNAT) {
		a->nat.v = a->nat.v <= b->nat.v;
	} else if (a->hdr.t == VNAT && b->hdr.t == VREA) {
		a->nat.v = a->nat.v <= b->rea.v;
	} else if (a->hdr.t == VREA && b->hdr.t == VNAT) {
		a->hdr.t = VNAT;
		a->nat.v = a->nat.v <= b->nat.v;
	} else if (a->hdr.t == VREA && b->hdr.t == VREA) {
		a->hdr.t = VNAT;
		a->nat.v = a->rea.v <= b->rea.v;
	}
	free_v(b);
	upd_infix(s, p, a);
	rc = (Ires) {NORM, s};
	return rc;
}
static Ires 
op_gre(Env *e, Val *s, size_t p) {
	Ires rc = (Ires) {FATAL, NULL};
	Val *a, *b;
	if (!set_infix_arg(e, s, p, &a, true, &b, true)) {
		return rc;
	}
	/* to be expanded to other types */
	if ((a->hdr.t != VNAT && a->hdr.t != VREA)
			|| (b->hdr.t != VNAT && b->hdr.t != VREA)) {
		printf("? %s: arguments not numbers\n", 
				__FUNCTION__);
		free_v(a);
		free_v(b);
		return rc;
	}
	if (a->hdr.t == VNAT && b->hdr.t == VNAT) {
		a->nat.v = a->nat.v > b->nat.v;
	} else if (a->hdr.t == VNAT && b->hdr.t == VREA) {
		a->nat.v = a->nat.v > b->rea.v;
	} else if (a->hdr.t == VREA && b->hdr.t == VNAT) {
		a->hdr.t = VNAT;
		a->nat.v = a->nat.v > b->nat.v;
	} else if (a->hdr.t == VREA && b->hdr.t == VREA) {
		a->hdr.t = VNAT;
		a->nat.v = a->rea.v > b->rea.v;
	}
	free_v(b);
	upd_infix(s, p, a);
	rc = (Ires) {NORM, s};
	return rc;
}
static Ires 
op_geq(Env *e, Val *s, size_t p) {
	Ires rc = (Ires) {FATAL, NULL};
	Val *a, *b;
	if (!set_infix_arg(e, s, p, &a, true, &b, true)) {
		return rc;
	}
	/* to be expanded to other types */
	if ((a->hdr.t != VNAT && a->hdr.t != VREA)
			|| (b->hdr.t != VNAT && b->hdr.t != VREA)) {
		printf("? %s: arguments not numbers\n", 
				__FUNCTION__);
		free_v(a);
		free_v(b);
		return rc;
	}
	if (a->hdr.t == VNAT && b->hdr.t == VNAT) {
		a->nat.v = a->nat.v >= b->nat.v;
	} else if (a->hdr.t == VNAT && b->hdr.t == VREA) {
		a->nat.v = a->nat.v >= b->rea.v;
	} else if (a->hdr.t == VREA && b->hdr.t == VNAT) {
		a->hdr.t = VNAT;
		a->nat.v = a->nat.v >= b->nat.v;
	} else if (a->hdr.t == VREA && b->hdr.t == VREA) {
		a->hdr.t = VNAT;
		a->nat.v = a->rea.v >= b->rea.v;
	}
	free_v(b);
	upd_infix(s, p, a);
	rc = (Ires) {NORM, s};
	return rc;
}
static Ires 
op_eq(Env *e, Val *s, size_t p) {
	Ires rc = (Ires) {FATAL, NULL};
	Val *a, *b;
	if (!set_infix_arg(e, s, p, &a, true, &b, true)) {
		return rc;
	}
	bool c = isequal_v(a, b);
	free_v(a);
	free_v(b);
	Val *d = malloc(sizeof(Val));
	d->hdr.t = VNAT;
	d->nat.v = c ? 1 : 0;
	upd_infix(s, p, d);
	rc = (Ires) {NORM, s};
	return rc;
}
static Ires 
op_neq(Env *e, Val *s, size_t p) {
	Ires rc = (Ires) {FATAL, NULL};
	Val *a, *b;
	if (!set_infix_arg(e, s, p, &a, true, &b, true)) {
		return rc;
	}
	bool c = isequal_v(a, b);
	free_v(a);
	free_v(b);
	Val *d = malloc(sizeof(Val));
	d->hdr.t = VNAT;
	d->nat.v = c ? 0 : 1;
	upd_infix(s, p, d);
	rc = (Ires) {NORM, s};
	return rc;
}
static Ires 
op_eqv(Env *e, Val *s, size_t p) {
	Ires rc = (Ires) {FATAL, NULL};
	Val *a, *b;
	if (!set_infix_arg(e, s, p, &a, true, &b, true)) {
		return rc;
	}
	bool c = isequiv_v(a, b);
	free_v(a);
	free_v(b);
	Val *d = malloc(sizeof(Val));
	d->hdr.t = VNAT;
	d->nat.v = c ? 1 : 0;
	upd_infix(s, p, d);
	rc = (Ires) {NORM, s};
	return rc;
}
static Ires 
op_and(Env *e, Val *s, size_t p) {
	Ires rc = (Ires) {FATAL, NULL};
	Val *a, *b;
	if (!set_infix_arg(e, s, p, &a, true, &b, true)) {
		return rc;
	}
	if ((a->hdr.t != VNAT) || (b->hdr.t != VNAT)) {
		printf("? %s: arguments not natural numbers\n", 
				__FUNCTION__);
		free_v(a);
		free_v(b);
		return rc;
	}
	a->nat.v = (a->nat.v == 0 ? 0 : 1);
	b->nat.v = (b->nat.v == 0 ? 0 : 1);
	a->nat.v = a->nat.v && b->nat.v;
	free_v(b);
	upd_infix(s, p, a);
	rc = (Ires) {NORM, s};
	return rc;
}
static Ires 
op_or(Env *e, Val *s, size_t p) {
	Ires rc = (Ires) {FATAL, NULL};
	Val *a, *b;
	if (!set_infix_arg(e, s, p, &a, true, &b, true)) {
		return rc;
	}
	if ((a->hdr.t != VNAT) || (b->hdr.t != VNAT)) {
		printf("? %s: arguments not natural numbers\n", 
				__FUNCTION__);
		free_v(a);
		free_v(b);
		return rc;
	}
	a->nat.v = (a->nat.v == 0 ? 0 : 1);
	b->nat.v = (b->nat.v == 0 ? 0 : 1);
	a->nat.v = a->nat.v || b->nat.v;
	free_v(b);
	upd_infix(s, p, a);
	rc = (Ires) {NORM, s};
	return rc;
}
static Ires 
op_not(Env *e, Val *s, size_t p) {
	Ires rc = (Ires) {FATAL, NULL};
	Val *a;
	if (!set_prefix1_arg(e, s, p, &a, true)) {
		return rc;
	}
	if ((a->hdr.t != VNAT)) {
		printf("? %s: argument not natural number (boolean)\n", 
				__FUNCTION__);
		free_v(a);
		return rc;
	}
	a->nat.v = (a->nat.v == 0 ? 1 : 0);
	upd_prefix1(s, p, a);
	rc = (Ires) {NORM, s};
	return rc;
}

static Ires 
op_print(Env *e, Val *s, size_t p) {
	Ires rc = (Ires) {FATAL, NULL};
	Val *a;
	if (!set_prefix1_arg(e, s, p, &a, true)) {
		return rc;
	}
	print_v(a, false);
	printf("\n");
	upd_prefix1(s, p, a);
	rc = (Ires) {NORM, s};
	return rc;
}
static Ires 
op_solve(Env *e, Val *s, size_t p) {
	Ires rc = (Ires) {FATAL, NULL};
	Val *a;
	if (!set_prefix1_arg(e, s, p, &a, true)) {
		return rc;
	}
	upd_prefix1(s, p, a);
	rc = (Ires) {NORM, s};
	return rc;
}
static Ires 
op_do(Env *e, Val *s, size_t p) {
	Val *a;
	if (!set_prefix1_arg(e, s, p, &a, false)) {
		return (Ires) {FATAL, NULL};
	}
	if (a->hdr.t == VSYM) {
		Val *b = lookup(e, a->sym.v, false, true);
		if (b == NULL) {
			printf("? %s: argument symbol undefined (%s)\n", 
					__FUNCTION__, a->sym.v);
			free_v(a);
			return (Ires) {FATAL, NULL};
		}
		free_v(a);
		a = copy_v(b);
	}
	if (a->hdr.t != VLST) {
		printf("? %s: argument not a list\n", 
				__FUNCTION__);
		free_v(a);
		return (Ires) {FATAL, NULL};
	}
	a->hdr.t = VSEQ;
	Ires rc = eval_norm(e, a, false);
	free_v(a);
	if (rc.state == FATAL) {
		return rc;
	}
	upd_prefix1(s, p, rc.v);
	return (Ires) {rc.state, s};
}
static Ires 
op_list(Env *e, Val *s, size_t p) {
	Val *a;
	if (!set_prefixn_arg(e, s, p, &a, false)) {
		return (Ires) {FATAL, NULL};
	}
	a->hdr.t = VLST;
	upd_prefixall(s, p, a);
	return (Ires) {NORM, s};
}
static Ires 
op_call(Env *e, Val *s, size_t p) {
	Val *a, *b;
	if (!set_prefix2_arg(e, s, p, &a, true, &b, false)) {
		return (Ires) {FATAL, NULL};
	}
	if (b->hdr.t != VSYM) {
		printf("? %s: 2nd argument is not a symbol, got ", 
				__FUNCTION__);
		print_v(b, true); printf("\n");
		free_v(a);
		free_v(b);
		return (Ires) {FATAL, NULL};
	}
	Symval *sv = symval(b->sym.v, a);
	free_v(b);
	if (sv == NULL) {
		free_v(a);
		return (Ires) {FATAL, NULL};
	}
	if (!stored_sym(e, sv)) {
		free_symval(sv);
		free_v(a);
		return (Ires) {FATAL, NULL};
	}
	upd_prefix2(s, p, a);
	return (Ires) {NORM, s};
}
static Ires 
op_true(Env *e, Val *s, size_t p) {
	Val *a;
	a = lookup(e, ITNAME, false, false);
	if (a == NULL) {
		printf("? %s: 'it undefined\n", 
				__FUNCTION__);
		return (Ires) {FATAL, NULL};
	}
	Val *b = malloc(sizeof(*b));
	assert(b != NULL);
	b->hdr.t = VNAT;
	b->nat.v = istrue_v(a);
	upd_prefix0(s, p, b);
	return (Ires) {NORM, s};
}
static Ires 
op_false(Env *e, Val *s, size_t p) {
	Val *a;
	a = lookup(e, ITNAME, false, false);
	if (a == NULL) {
		printf("? %s: 'it undefined\n", 
				__FUNCTION__);
		return (Ires) {FATAL, NULL};
	}
	Val *b = malloc(sizeof(*b));
	assert(b != NULL);
	b->hdr.t = VNAT;
	b->nat.v = !istrue_v(a);
	upd_prefix0(s, p, b);
	return (Ires) {NORM, s};
}
static Ires 
op_if(Env *e, Val *s, size_t p) {
	if (p != 0 || p != s->seq.v.n - 2) {
		printf("? %s: 'if' sequence invalid\n",
				__FUNCTION__);
		return (Ires) {FATAL, NULL};
	}
	Val *a;
	if (!set_prefix1_arg(e, s, p, &a, true)) {
		return (Ires) {FATAL, NULL};
	}
	Val *b = malloc(sizeof(*b));
	assert(b != NULL);
	b->hdr.t = VNAT;
	Ires rc;
	if (istrue_v(a)) {
		b->nat.v = 1;
		rc.state = NORM;
	} else {
		b->nat.v = 0;
		rc.state = IFSKIP;
	} 
	free_v(a);
	upd_prefix1(s, p, b);
	rc.v = s;
	return rc;
}

static int  
position(Val *a, List_v lv) {
	for (size_t i=0; i<lv.n; ++i) {
		if (isequal_v(a, lv.v[i])) {
			return i;
		}
	}
	return -1;
}

static Ires 
op_else(Env *e, Val *s, size_t p) {
	if (!(p == 0 && s->seq.v.n == 1)) {
		printf("? %s: `else syntax incorrect\n", __FUNCTION__);
		return (Ires) {FATAL, NULL};
	}
	if (e->state == IFSKIP) {
		Val *a = malloc(sizeof(*a));
		a->hdr.t = VNAT;
		a->nat.v = 1;
		upd_prefix0(s, p, a);
		return (Ires) {NORM, s};
	}
	if (e->state == NORM) {
		Val *it = lookup(e, ITNAME, false, false);
		if (it == NULL) {
			printf("? %s: `else before `if\n", __FUNCTION__);
			return (Ires) {FATAL, NULL};
		} 
		Val *b = copy_v(it);
		upd_prefix0(s, p, b);
		return (Ires) {IFSKIP, s};
	}
	printf("? %s: `else in invalid state (%u)\n", __FUNCTION__, e->state);
	return (Ires) {FATAL, NULL};
}
static Ires 
op_rem(Env *e, Val *s, size_t p) {
	if (p != 0) {
		printf("? %s: `rem syntax invalid, needs to be 1st\n",
				__FUNCTION__);
		return (Ires) {FATAL, NULL};
	}
	Ires rc;
	Val *b;
	Val *a = lookup(e, ITNAME, false, false);
	if (a == NULL) {
		b = malloc(sizeof(*b));
		assert(b != NULL);
		b->hdr.t = VNIL;
	} else {
		b = copy_v(a);
	}
	upd_prefixall(s, p, b);
	rc.state = NORM;
	rc.v = s;
	return rc;
}

/* --- reduce function definition --- */
static Ires
op_def(Env *e, Val *s, size_t p) {
	/* rem: define foo (a, b) or define foo () ; */
	if (s->seq.v.n != 3 || p != 0) {
		printf("? %s: incorrect number of arguments to 'define'\n",
				__FUNCTION__);
		return (Ires) {FATAL, NULL};
	}
	Val *fname, *fparam;
	if (!set_prefix2_arg(e, s, p, &fname, false, &fparam, false)) {
		return (Ires) {FATAL, NULL};
	}
	if (fname->hdr.t != VSYM) {
		printf("? %s: expecting symbol for function name, got ",
				__FUNCTION__);
		print_v(fname, true); printf("\n");
		free_v(fname);
		free_v(fparam);
		return (Ires) {FATAL, NULL};
	}
	if (!(fparam->hdr.t == VLST || fparam->hdr.t == VNIL)) {
		printf("? %s: expecting list or '()' for function parameters\n",
				__FUNCTION__);
		free_v(fname);
		free_v(fparam);
		return (Ires) {FATAL, NULL};
	}
	if (fparam->hdr.t == VLST) {
		for (size_t i=0; i < fparam->lst.v.n; ++i) {
			if (fparam->lst.v.v[i]->hdr.t != VSYM) {
				printf("? %s: expecting symbol for function parameter\n",
						__FUNCTION__);
				free_v(fname);
				free_v(fparam);
				return (Ires) {FATAL, NULL};
			}
		}
	}
	Val *f = malloc(sizeof(*f));
	assert(f != NULL && "function val NULL");
	f->hdr.t = VFUN;
	strncpy(f->symf.name, fname->sym.v, sizeof(f->symf.name));
	/* TODO replace with simple memcpy */
	f->symf.param.n = 0;
	f->symf.param.v = NULL;
	if (fparam->hdr.t == VLST) {
		for (size_t i=0; i<fparam->lst.v.n; ++i) {
			f->symf.param = push_l(f->symf.param, fparam->lst.v.v[i]);
		}
	}
	f->symf.body.n = 0;
	f->symf.body.v = NULL;
	free_v(fname);
	/* we used all items in fparam for f, so do not free them in free_v */
	fparam->lst.v.n = 0;
	free_v(fparam);
	upd_prefix2(s, p, f);
	return (Ires) {FUNDEF, s};
}
static Ires
op_loop(Env *e, Val *s, size_t p) {
	if (Dbg) { printf("#\t  %s %5s: ", __FUNCTION__, "entry"); printx_v(s,false,"#\t"); printf("\n"); }
	/* rem: loop */
	if (s->seq.v.n != 1) {
		printf("? %s: `loop does not take arguments\n",
				__FUNCTION__);
		return (Ires) {FATAL, NULL};
	}
	Val *f = malloc(sizeof(*f));
	assert(f != NULL && "loop val NULL");
	f->hdr.t = VFUN;
	strncpy(f->symf.name, LOOPNAME, sizeof(f->symf.name));
	f->symf.param.n = 0;
	f->symf.param.v = NULL;
	f->symf.body.n = 0;
	f->symf.body.v = NULL;
	upd_prefix0(s, p, f);
	return (Ires) {LOOPDEF, s};
}
static Ires 
op_end(Env *e, Val *s, size_t p) {
	if (Dbg) { printf("#\t  %s %5s: ", __FUNCTION__, "entry"); printx_v(s, false, "#\t"); printf("\n"); }
	/* rem: end somefun or end if or end loop ; */
	if (p != 0 || s->seq.v.n != 2) {
		printf("? %s: invalid 'end', expecting argument\n",
				__FUNCTION__);
		return (Ires) {FATAL, NULL};
	}
	Val *a;
	if (!set_prefix1_arg(e, s, p, &a, false)) {
		return (Ires) {FATAL, NULL};
	}
	Val *b = NULL;
	if (a->hdr.t == VOPE) {
		/* rem: end if */
		if (a->symop.v == op_if || a->symop.v == op_loop) {
			Val *c = lookup(e, ITNAME, false, false);
			if (c == NULL) {
				printf("? %s: 'it undefined, missing `if or `loop?\n",
						__FUNCTION__);
				free_v(a);
				return (Ires) {FATAL, NULL};
			} else {
				b = copy_v(c);
			}
			free_v(a);
		} else {
			printf("? %s: 'end' with wrong operator argument (expecting `if or `loop)\n",
					__FUNCTION__);
			free_v(a);
			return (Ires) {FATAL, NULL};
		}
	} else if (a->hdr.t == VSYM) {
		/* rem: end function */
		if (e->state != FUNDEF) {
			printf("? %s: 'end' outside function definition\n", 
					__FUNCTION__);
			free_v(a);
			return (Ires) {FATAL, NULL};
		}
		Val *c = lookup(e, ITNAME, false, false);
		if (c == NULL) {
			printf("? %s: 'it' required, yet undefined\n", 
					__FUNCTION__);
			free_v(a);
			return (Ires) {FATAL, NULL};
		}
		if (c->hdr.t != VFUN) {
			printf("? %s: 'end' argument is not a function name\n", 
					__FUNCTION__);
			free_v(a);
			return (Ires) {FATAL, NULL};
		}
		if (strncmp(a->sym.v, c->symf.name, sizeof(a->sym.v)) != 0) {
			/* rem: not the function being defined, then part of the body */
			free_v(a);
			return (Ires) {BACKTRACK, NULL};
		}
		/* rem: end 'fun ; add the fun symbol */
		b = copy_v(c);
		Symval *sv = symval(b->symf.name, b);
		if (!stored_sym(e, sv)) {
			free_symval(sv);
			free_v(a);
			return (Ires) {FATAL, NULL};
		}
		free_v(a);
	} else {
		printf("? %s: 'end' with wrong argument type\n",
				__FUNCTION__);
		free_v(a);
		return (Ires) {FATAL, NULL};
	}
	upd_prefix1(s, p, b);
	return (Ires) {NORM, s};
}
/* --- reduce (user) function application --- */
Env *
new_env(Env *parent) {
	Env *e = malloc(sizeof(*e));
	assert(e != NULL);
	e->state = NORM;
	e->n = 0;
	e->s = NULL;
	e->parent = parent;
	Val *it = malloc(sizeof(*it));
	it->hdr.t = VNIL;
	Symval *svit = symval(ITNAME, it);
	free_v(it);
	if (svit == NULL) {
		free_env(e, false);
		return NULL;
	}
	if (!stored_sym(e, svit)) {
		free_symval(svit);
		free_env(e, false);
		return NULL;
	}
	Val *lnst = malloc(sizeof(*lnst));
	lnst->hdr.t = VNAT;
	lnst->nat.v = 0;
	Symval *lv = symval(LOOPNEST, lnst);
	free_v(lnst);
	if (lv == NULL) {
		free_env(e, false);
		return NULL;
	}
	if (!stored_sym(e, lv)) {
		free_symval(lv);
		free_env(e, false);
		return NULL;
	}
	return e;
}

static Ires 
apply_fun(Env *e, Val *s, size_t p) {
	/* rem: ... foo (1, 2) or foo () ... */
	Val *al;
	Val *f = s->seq.v.v[p];
	if (!set_prefix1_arg(e, s, p, &al, true)) {
		printf("? %s: invalid argument to `%s\n", 
				__FUNCTION__, f->symf.name);
		return (Ires) {FAIL, s};
	}
	if (!(al->hdr.t == VLST || al->hdr.t == VNIL)) {
		printf("? %s: argument to `%s not a list or '()'\n", 
				__FUNCTION__, f->symf.name);
		return (Ires) {FAIL, s};
	}
	if (al->hdr.t == VNIL && f->symf.param.n != 0) {
		printf("? %s: expected %lu argument(s) to `%s\n", 
				__FUNCTION__, f->symf.param.n, f->symf.name);
		return (Ires) {FAIL, s};
	}
	if (al->hdr.t == VLST && al->lst.v.n != f->symf.param.n) {
		printf("? %s: number of arguments to `%s mismatch (got %lu, expected %lu)\n", 
				__FUNCTION__, f->symf.name,
				al->lst.v.n, f->symf.param.n);
		return (Ires) {FAIL, s};
	}
	/* setup local env */
	Env *le = new_env(e);
	if (le == NULL) {
		printf("? %s: local env creation failed\n",
				__FUNCTION__);
		return (Ires) {FAIL, s};
	}
	if (al->hdr.t == VLST) {
		Ires rc = solve_lst(le, al, true); /* true: solve all sym. macros? */
		if (rc.code != OK) {
			return (Ires) {FAIL, s};
		}
		/* add symval for each param, value is al's */
		for (size_t i=0; i<f->symf.param.n; ++i) {
			Symval *sv = symval(f->symf.param.v[i]->sym.v, al->lst.v.v[i]);
			if (!stored_sym(le, sv)) {
				free_symval(sv);
				free_env(le, false);
				return (Ires) {FAIL, s};
			}
		}
	}
	/* reduce each expression in body, like eval_ph: */
	Val *v;
	for (size_t i=0; i<f->symf.body.n; ++i) {
		v = copy_v(f->symf.body.v[i]);
		if (Dbg) { printf("#\t  %s %5s: ", __FUNCTION__, "value"); printx_v(v,false,"#\t"); printf("\n"); }
		istate xs = eval(le, v);
		if (Dbg) { printf("#\t  %s %5s: ", __FUNCTION__, "reduce"); print_istate(xs); printf("\n"); } 
		if (xs == FATAL) {
			free_env(le, false);
			return (Ires) {FAIL, s};
		}
		if (xs == RETURN) {
			break;
		}
	}
	if (Dbg) { printf("#\t  %s %5s:\n", __FUNCTION__, "done"); print_env(le, "#\t"); }
	/* return local (function's) 'it to caller */
	char *sym = ITNAME;
	Val *lit = lookup(le, sym, false, true);
	if (lit == NULL) {
		printf("? %s: 'it from `%s undefined\n",
				__FUNCTION__, f->symf.name);
		free_env(le, false);
		return (Ires) {FAIL, s};
	}
	upd_prefix1(s, p, copy_v(lit));
	free_env(le, false);
	return (Ires) {OK, s};
}
static Ires 
op_return(Env *e, Val *s, size_t p) {
	if (!(p == 0 && s->seq.v.n == 1)) {
		printf("? %s: `return syntax incorrect\n", __FUNCTION__);
		return (Ires) {FATAL, NULL};
	}
	/* return only from a function call */
	if (e->parent == NULL) {
		printf("? %s: `return outside function\n", __FUNCTION__);
		return (Ires) {FATAL, NULL};
	}
	Val *it = lookup(e, ITNAME, false, true);
	if (it == NULL) {
		printf("? %s: 'it undefined\n", __FUNCTION__);
		return (Ires) {FATAL, NULL};
	}
	upd_prefix0(s, p, copy_v(it));
	return (Ires) {RETURN, s};
}
static Ires 
op_stop(Env *e, Val *s, size_t p) {
	if (!(p == 0 && s->seq.v.n == 1)) {
		printf("? %s: `stop syntax incorrect\n", __FUNCTION__);
		return (Ires) {FATAL, NULL};
	}
	Val *it = lookup(e, ITNAME, false, false);
	if (it == NULL) {
		printf("? %s: 'it undefined (`stop return value)\n", __FUNCTION__);
		return (Ires) {FATAL, NULL};
	}
	upd_prefix0(s, p, copy_v(it));
	return (Ires) {STOP, s};
}
static Ires 
op_env(Env *e, Val *s, size_t p) {
	if (!(p == 0 && s->seq.v.n == 1)) {
		printf("? %s: `env syntax incorrect\n", __FUNCTION__);
		return (Ires) {FATAL, NULL};
	}
	print_env(e, ">");
	Val *it = lookup(e, ITNAME, false, false);
	if (it == NULL) {
		printf("? %s: 'it undefined\n", __FUNCTION__);
		return (Ires) {FATAL, NULL};
	}
	upd_prefix0(s, p, copy_v(it));
	return (Ires) {NORM, s};
}

/* --------------- builtin or base function symbols -------------------- */

/* user defined fun priority */
#define FUNDEFPRIO 0

typedef struct Symop_ {
	char name[WSZ];
	int prio;
	Ires (*f)(Env *e, Val *s, size_t p);
	int arity;
} Symop;

Symop Syms[] = {
	(Symop) {"call",   -20, op_call,  2},
	(Symop) {"define", -20, op_def,  2},
	(Symop) {"def",    -20, op_def,  2},
	(Symop) {"do",     -20, op_do,    1},
	(Symop) {"else",   -20, op_else, 0},
	(Symop) {"end",    -20, op_end,  1}, /* needs to be prior to loop, if, ufun */
	(Symop) {"env",    -20, op_env, 0},
	(Symop) {"list",   -20, op_list, -1},
	(Symop) {"loop",   -20, op_loop, 0},
	(Symop) {"print",  -20, op_print, 1}, 
	(Symop) {"rem:",   -20, op_rem,  -1}, /* -1 arity: remainder of seq val */
	(Symop) {"return", -20, op_return, 0},
	(Symop) {"stop",   -20, op_stop, 0},
	(Symop) {"solve",  -20, op_solve,  1},
	(Symop) {"true?",  -20, op_true,  0},
	(Symop) {"false?", -20, op_false, 0},
	/* priority FUNDEFPRIO (0) is for function (user defined) */
	(Symop) {"*",    20, op_mul, 2},
	(Symop) {"/",    20, op_div, 2},
	(Symop) {"+",    30, op_plu, 2},
	(Symop) {"-",    30, op_min, 2},
	(Symop) {"=",    40, op_eq,  2},
	(Symop) {"/=",   40, op_neq, 2},
	(Symop) {"<",    40, op_les, 2},
	(Symop) {"<=",   40, op_leq, 2},
	(Symop) {">",    40, op_gre, 2},
	(Symop) {">=",   40, op_geq, 2},
	(Symop) {"~=",   40, op_eqv, 2},
	(Symop) {"not",  50, op_not, 1},
	(Symop) {"and",  60, op_and, 2},
	(Symop) {"or",   70, op_or,  2},
	(Symop) {"if",   80, op_if,  1},
	(Symop) {"", 0, op_false, -1},
};
static int
minprio() {
	int m = 0;
	for (int i=0; Syms[i].name[0] != '\0'; ++i) {
		if (Syms[i].prio > m) {
			m = Syms[i].prio;
		}
	}
	return m;
}
static Symop *
lookup_op(char *a) {
	for (size_t i=0; Syms[i].name[0] != '\0'; ++i) {
		Symop *b = Syms+i;
		if (strncmp(b->name, a, sizeof(b->name)) == 0) {
			return b;
		}
	}
	return NULL;
}

/* --------------- user defined value symbols -------------------- */


static bool
isatom_v(Val *a) {
	assert(a != NULL);
	return (a->hdr.t == VNIL 
			|| a->hdr.t == VNAT 
			|| a->hdr.t == VREA
			|| a->hdr.t == VSYM
			|| a->hdr.t == VFUN
			|| a->hdr.t == VOPE);
}

static Val *
val_of_seme(Env *e, Sem *s) {
	/* returns fresh value, 's untouched */
	assert(s != NULL && "seme null");
	Val *a = NULL;
	if (s->hdr.t == SNIL ) {
		a = malloc(sizeof(*a));
		a->hdr.t = VNIL;
		return a;
	}
	if (s->hdr.t == SNAT) {
		a = malloc(sizeof(*a));
		a->hdr.t = VNAT;
		a->nat.v = s->nat.v;
		return a;
	}
	if (s->hdr.t == SREA) {
		a = malloc(sizeof(*a));
		a->hdr.t = VREA;
		a->rea.v = s->rea.v;
		return a;
	}
	if (s->hdr.t == SSYM) {
		a = malloc(sizeof(*a));
		assert(a != NULL);
		a->hdr.t = VSYM;
		strncpy(a->sym.v, s->sym.v, 1+strlen(s->sym.v));
		return a;
	}
	if (s->hdr.t == SLST) {
		size_t n = s->lst.v.n;
		Sem *l = s->lst.v.s;
		Val *d;
		for (size_t i=0; i<n; ++i) {
			d = val_of_seme(e, l+i);
			if (d == NULL) {
				free_v(a);
				return NULL;
			}
			a = push_v(VLST, a, d);
		}
		return a;
	}
	if (s->hdr.t == SSEQ) {
		size_t n = s->seq.v.n;
		Sem *l = s->seq.v.s;
		Val *d;
		for (size_t i=0; i<n; ++i) {
			d = val_of_seme(e, l+i);
			if (d == NULL) {
				free_v(a);
				return NULL;
			}
			a = push_v(VSEQ, a, d);
		}
		return a;
	}
	printf("? %s: unknown seme\n",
			__FUNCTION__);
	return NULL;
}

/* ----- evaluation, pass 2, symbolic computation ----- */

static Ires 
solve_fun(Env *e, Val *a) {
	/* works on SEQ and LST */
	assert(a->hdr.t == VSEQ || a->hdr.t == VLST);
	Val *b = NULL;
	for (size_t i=0; i < a->seq.v.n; ++i) {
		b = a->seq.v.v[i];
		if (b->hdr.t == VSYM) {
			Val *c = NULL;
			Symop *so = lookup_op(b->sym.v);
			if (so != NULL) {
				c = malloc(sizeof(*c));
				c->hdr.t = VOPE;
				c->symop.prio = so->prio;
				c->symop.v = so->f;
				c->symop.arity = so->arity;
				strncpy(c->symop.name, so->name, 1+strlen(so->name));
				free_v(b);
				a->seq.v.v[i] = c;
				continue;
			}
			if (strncmp(b->sym.v, IT, sizeof(b->sym.v)) == 0) {
				/* 'it is resolved at exec */
				continue;
			}
			c = lookup(e, b->sym.v, true, true);
			if (c != NULL && (c->hdr.t == VFUN
					|| c->hdr.t == VOPE)) {
				free_v(b);
				a->seq.v.v[i] = copy_v(c);
				continue;
			}
		}
	}
	return (Ires) {NORM, a};
}

static Ires 
Reduce_seq(Env *e, Val *b, bool look) {
	/* symbol application: consumes the seq, until 1 item left */
	assert(b != NULL);
	if (Dbg) { printf("#\t  %s (%d) entry: ", __FUNCTION__, look); printx_v(b,false,"#\t"); printf("\n"); }
	Ires rc = (Ires) {NOP, b};
	Val *c;
	while (b->seq.v.n > 0) {
		/* stop condition: seq reduced to single element */
		if (b->seq.v.n == 1) {
			/* reduction loop ends, return the reduced seq,
			 * unless it's an operator of 0 arity,
			 * then, fall through & execute it below */
			/* (user functions all have one parameter) */
			c = b->seq.v.v[0]; /* shorthand */
			if (!(c->hdr.t == VOPE && c->symop.arity == 0)) {
				rc.v = b;
				assert(rc.code == OK || rc.code == NOP);
				return rc;
			}
		}
		int hiprio = minprio()+1;
		size_t symat = 0;
		bool symfound = false;
		vtype symtype = 0;
		/* apply symops from left to right (for same priority symbols) */
		for (size_t i=0; i < b->seq.v.n; ++i) {
			c = b->seq.v.v[i];
			if (c->hdr.t == VFUN) {
				if (FUNDEFPRIO < hiprio) {
					hiprio = FUNDEFPRIO;
					symat = i;
					symfound = true;
					symtype = VFUN;
				}
			} else if (c->hdr.t == VOPE) {
				if (c->symop.prio < hiprio) {
					hiprio = c->symop.prio;
					symat = i;
					symfound = true;
					symtype = VOPE;
				}
			} 
		}
		if (!symfound) { 
			printf("? %s: sequence without function ",__FUNCTION__);
			print_v(b, true);
			printf("\n");
			return (Ires) {FAIL, b};
		}
		assert(symtype == VFUN || symtype == VOPE);
		/* apply the symbol, returns the reduced seq */
		if (symtype == VFUN) {
			/* user defined function */
			rc = apply_fun(e, b, symat);
		} else if (symtype == VOPE) {
			/* builtin operator */
			rc = b->seq.v.v[symat]->symop.v(e, b, symat);
		}
		if (!(rc.code == OK && rc.code == NOP)) {
			return rc;
		}
		b = rc.v; // TODO needed?
		/* rc.code set by the op_*() */
		if (Dbg) { printf("#\t  %s reduced: ", __FUNCTION__); printx_v(b,false,"#\t"); printf("\n"); }
	}
	/* empty seq after reduction? */
	printf("? %s: sequence empty\n",__FUNCTION__);
	return (Ires) {FAIL, b};
}
static Ires 
eval_loop(Env *e, Val *s) {
	/* rem: loop execution */
	Val *v;
	istate xs = NORM;
	if (Dbg) { printf("#\t  %s entry:\n", __FUNCTION__); }
	while (!(xs == STOP || xs == RETURN)) {
		for (size_t i=0; i<s->symf.body.n; ++i) {
			v = copy_v(s->symf.body.v[i]);
			xs = eval(e, v);
			free_v(v);
			if (xs == FATAL) {
				return (Ires) {FATAL, NULL};
			}
			if (xs == RETURN) {
				break;
			}
			if (xs == STOP) {
				break;
			}
		}
	}
	if (Dbg) { printf("#\t  %s end:\n", __FUNCTION__); print_env(e, "#\t"); }
	Val *it = lookup(e, ITNAME, false, false);
	if (it == NULL) {
		printf("? %s: 'it from loop undefined\n",
				__FUNCTION__);
		return (Ires) {FATAL, NULL};
	}
	if (xs == STOP) {
		xs = NORM;
	}
	return (Ires) {xs, copy_v(it)};
}
static Ires 
solve_seq(Env *e, Val *a, bool look) {
	if (a->seq.v.n == 0) {
		Val *b = malloc(sizeof(*b));
		b->hdr.t = VNIL;
		free_v(a);
		return (Ires) {OK, b};
	}
	Ires rc;
	/* resolve all syms to functions to prepare for reduction: */
	for (size_t i=0; i < a->seq.v.n; ++i) {
		rc = eval_norm(e, a->seq.v.v[i], look);
		if (!(rc.code == OK || rc.code == NOP)) {
			return rc;
		}
		a->seq.v.v[i] = rc.v;
	}
	rc = Reduce_seq(e, a, look);
	if (rc.code == OK || rc.code == NOP) {
		rc.v = a->seq.v.v[0]; /* steal single seq item */
		a->seq.v.n = 0;
		free_v(a);
		rc.code = OK;
	}
	return rc;
}
static Ires 
solve_sym(Env *e, Val *a, bool look) {
	assert(a->hdr.t == VSYM);
	/* resolve operators */
	Symop *so = lookup_op(a->sym.v);
	if (so != NULL) {
		Val *b = malloc(sizeof(*b));
		b->hdr.t = VOPE;
		b->symop.prio = so->prio;
		b->symop.v = so->f;
		b->symop.arity = so->arity;
		strncpy(b->symop.name, so->name, 1+strlen(so->name));
		return (Ires) {OK, b};
	}
	/* resolve all symbols if requested */
	if (look) {
		Val *b = lookup(e, a->sym.v, true, look);
		if (b == NULL) {
			printf("? %s: unknown symbol '%s\n",
				__FUNCTION__, a->sym.v);
			return (Ires) {FAIL, a};
		} 
		return (Ires) {OK, copy_v(b)};
	}
	/* resolve 'it */
	if (strncmp(a->sym.v, IT, sizeof(a->sym.v)) == 0) {
		Val *b = lookup(e, ITNAME, false, false);
		if (b == NULL) {
			printf("? %s: 'it undefined\n",
				__FUNCTION__);
			return (Ires) {FAIL, a};
		} 
		return (Ires) {OK, copy_v(b)};
	}
	/* resolve symbol if it points to function: */
	Val *b = lookup(e, a->sym.v, true, true);
	if (b != NULL && (b->hdr.t == VFUN
			|| b->hdr.t == VOPE)) {
		return (Ires) {OK, copy_v(b)};
	}
	return (Ires) {NOP, a};
}
static Ires 
solve_lst(Env *e, Val *a, bool look) {
	Ires rc;
	for (size_t i=0; i < a->lst.v.n; ++i) {
		rc = eval_norm(e, a->seq.v.v[i], look);
		if (!(rc.code == OK || rc.code == NOP)) {
			return (Ires) {FAIL, a};
		}
		a->seq.v.v[i] = rc.v;
	}
	if (Dbg) { printf("#\t  %s exit: ", __FUNCTION__); printx_v(a, false,"#\t"); printf("\n"); }
	return (Ires) {OK, a};
}

static Ires 
eval_norm(Env *e, Val *a, bool look) {
	/* returns 'a, or a new val and frees 'a */
	Ires r = {OK, a};
	if (a->hdr.t == VSYM) {
		r = solve_sym(e, a, look);
		if (r.code == OK) {
			/* r.v holds a new val */
			free_v(a);
		} else if (r.code == NOP) {
			r.code = OK;
		}
		return r;
	}
	if (a->hdr.t == VSEQ) {
		return solve_seq(e, a, look);
	}
	return r;
}

static void 
update_freesym(Env *e, Val *fun, Val *s) {
	Val *c, *fv = NULL;
	for (size_t i=0; i<s->seq.v.n; ++i) {
		c = s->seq.v.v[i];
		if (c->hdr.t == VSYM) {
			/* 'it resolved later, at fun execution */
			if (strncmp(c->sym.v, IT, sizeof(c->sym.v)) == 0) {
				continue;
			}
			/* skip recursive call */
			if (strncmp(c->sym.v, fun->symf.name, sizeof(c->sym.v)) == 0) {
				continue;
			}
			/* ignore function parameters */
			int id = position(c, fun->symf.param);
			if (id != -1) {
				continue;
			}
			Val *l = lookup(e, c->sym.v, true, false);
			/* unknown sym, can be normal, determined in function or operator */
			if (l == NULL) {
				continue;
			}
			/* closure: replace sym with value from env */
			fv = copy_v(l);
		} else if (c->hdr.t == VSEQ || c->hdr.t == VLST) {
			update_freesym(e, fun, c);
			fv = copy_v(c);
		} else {
			continue;
		}
		free_v(c);
		s->seq.v.v[i] = fv;
	}
}

static Ires
eval_body(Env *e, Val *s, size_t p) {
	if (Dbg) { printf("#\t  %s entry: ", __FUNCTION__); printx_v(s, false,"#\t"); printf("\n"); }
	Val *fun = lookup(e, ITNAME, false, false);
	if (fun == NULL) {
		printf("? %s: 'it undefined\n", __FUNCTION__);
		return (Ires) {FATAL, NULL};
	}
	if (fun->hdr.t != VFUN) {
		printf("? %s: no function under definition\n", __FUNCTION__);
		return (Ires) {FATAL, NULL};
	}
	Val *fret = copy_v(fun);
	/* replace free symbols with env value: */
	update_freesym(e, fun, s);
	fret->symf.body = push_l(fret->symf.body, copy_v(s));
	if (Dbg) { printf("#\t  %s exit: ", __FUNCTION__); printx_v(s,false,"#\t"); printf("\n"); }
	return (Ires) {FUNDEF, fret};
}

static Ires
eval_loop_body(Env *e, Val *s, size_t p) {
	Val *loop = lookup(e, ITNAME, false, false);
	if (loop == NULL) {
		printf("? %s: 'it required, yet undefined\n", __FUNCTION__);
		return (Ires) {FATAL, NULL};
	}
	if (loop->hdr.t != VFUN) {
		printf("? %s: 'it is not a loop function\n", __FUNCTION__);
		return (Ires) {FATAL, NULL};
	}
	Val *lret = copy_v(loop);
	lret->symf.body = push_l(lret->symf.body, copy_v(s));
	return (Ires) {LOOPDEF, lret};
}
static Ires
eval_maybe_def(Env *e, Val *a) {
	/* returns a fresh value, 'a untouched */
	if (Dbg) { printf("#\t  %s entry: ", __FUNCTION__); printx_v(a,false,"#\t"); printf("\n"); }
	/* resolve symbols (not 'it) to operators and functions */
	Ires rc = solve_fun(e, a);
	if (rc.state == FATAL) {
		return rc;
	}
	if (Dbg) { printf("#\t  %s resolved: ", __FUNCTION__); printx_v(a,false,"#\t"); printf("\n"); }
	/* rem: expecting end 'foo */
	if (a->seq.v.v[0]->hdr.t == VOPE 
			&& a->seq.v.v[0]->symop.v == op_end
			&& a->seq.v.v[0]->symop.arity == a->seq.v.n -1
			&& a->seq.v.v[1]->hdr.t == VSYM) {
		Ires rc = eval_norm(e, a, false);
		/* not a valid end 'fun expression after all, add to body instead */
		if (rc.state == BACKTRACK) {
			rc = eval_body(e, a, 0);
			return rc;
		}
		return rc;
	}
	rc = eval_body(e, a, 0);
	return rc;
}

static Ires
eval_maybe_loop(Env *e, Val *a) {
	/* returns a fresh value, 'a untouched */
	if (Dbg) { printf("#\t  %s entry: ", __FUNCTION__); printx_v(a,false,"#\t"); printf("\n"); }
	/* resolve symbols (not 'it) to operators and functions */
	Ires rc = solve_fun(e, a);
	if (rc.state == FATAL) {
		return rc;
	}
	if (Dbg) { printf("#\t  %s resolved: ", __FUNCTION__); printx_v(a,false,"#\t"); printf("\n"); }
	Val *lnst = lookup(e, LOOPNEST, false, false);
	if (lnst == NULL) {
		printf("? %s: nested loop count missing\n", __FUNCTION__);
		return (Ires) {FATAL, NULL};
	}
	if (lnst->hdr.t != VNAT) {
		printf("? %s: nested loop count invalid\n", __FUNCTION__);
		return (Ires) {FATAL, NULL};
	}
	/* rem: handle nested loop, another `loop val */
	if (a->seq.v.v[0]->hdr.t == VOPE 
			&& a->seq.v.v[0]->symop.v == op_loop) {
		++(lnst->nat.v);
	} /* rem: `end `loop ? */
	else if (a->seq.v.v[0]->hdr.t == VOPE 
			&& a->seq.v.v[0]->symop.v == op_end
			&& a->seq.v.v[0]->symop.arity == a->seq.v.n -1
			&& a->seq.v.v[1]->hdr.t == VOPE 
			&& a->seq.v.v[1]->symop.v == op_loop) {
		/* `end `loop while in topmost loop, execute this */
		if (lnst->nat.v == 0) {
			Ires rc = eval_norm(e, a, false);
			return rc;
		}
		--(lnst->nat.v);
		/* then proceed to add to body */
	}
	rc = eval_loop_body(e, a, 0);
	return rc;
}
static Ires
eval_maybe_skip(Env *e, Val *a) {
	/* returns a fresh value, 'a untouched */
	if (Dbg) { printf("#\t  %s entry: ", __FUNCTION__); printx_v(a,false,"#\t"); printf("\n"); }
	/* resolve symbols to functions */
	Ires rc = solve_fun(e, a);
	if (rc.state == FATAL) {
		return rc;
	}
	if (Dbg) { printf("#\t  %s resolved: ", __FUNCTION__); printx_v(a,false,"#\t"); printf("\n"); }
	bool an_endif = a->seq.v.v[0]->hdr.t == VOPE 
			&& a->seq.v.v[0]->symop.v == op_end
			&& a->seq.v.v[0]->symop.arity == a->seq.v.n -1
			&& a->seq.v.v[1]->hdr.t == VOPE 
			&& a->seq.v.v[1]->symop.v == op_if ;
	bool an_else = a->seq.v.v[0]->hdr.t == VOPE 
			&& a->seq.v.v[0]->symop.v == op_else ;
	if (an_endif || an_else) {
		Ires rc = eval_norm(e, a, false);
		return rc;
	}
	/* default: skip val */
	Val *it = lookup(e, ITNAME, false, false);
	if (it == NULL) {
		printf("? %s: 'it undefined\n", __FUNCTION__);
		return (Ires) {FATAL, NULL};
	}
	return (Ires) {IFSKIP, copy_v(it)};
}

static istate 
eval(Env *e, Val *a) {
	/* frees 'a, updates env's state, sets 'it, returns state */
	assert(e != NULL && "env is null");
	assert(a != NULL && "value is null");
	if (Dbg) { printf("#\t %s: entry\n", __FUNCTION__); print_env(e, "#\t"); }
	Ires rc;
	switch (e->state) {
		case NORM:
			rc = eval_norm(e, a, false);
			break;
		case LOOPDEF:
			rc = eval_maybe_loop(e, a);
			/* if ended a loop (NORM state), execute it now */
			if (rc.code == OK) {
				e->state = NORM;
				Ires lrc = eval_loop(e, rc.v);
				free_v(rc.v);
				rc = lrc;
			}
			break;
		case FUNDEF:
			rc = eval_maybe_def(e, a);
			break;
		case IFSKIP:
			rc = eval_maybe_skip(e, a);
			break;
		default:
			printf("? %s: invalid state\n", __FUNCTION__);
			rc = (Ires) {FAIL, a};
	}
	if (Dbg) { printf("#\t %s: eval'd code ", __FUNCTION__); print_code(rc.code); printf("\n");}
	switch (rc.code) {
		case FAIL:
			e->state = FATAL;
			free_v(rc.v);
			return e->state;
		case OK:
		case NOP:
			e->state = NORM;
			break;
		case SKIP:
			e->state = IFSKIP;
			break;
		case DEF:
			e->state = FUNDEF;
			break;
		case RET:
			e->state = RETURN;
			break;
		case BACK:
			e->state = BACKTRACK;
			break;
		case LOOP:
			e->state = LOOPDEF;
			break;
		case INT:
			e->state = STOP;
			break;
		default:
			printf("? %s: invalid code\n", __FUNCTION__);
			e->state = FATAL;
			free_v(rc.v);
			return e->state;
	}
	Symval *it = symval(ITNAME, rc.v);
	free_v(rc.v);
	if (it == NULL) {
		e->state = FATAL;
		return e->state;
	}
	if (!stored_sym(e, it)) {
		free_symval(it);
		e->state = FATAL;
		return e->state;
	}
	if (Dbg) { printf("#\t %s: exit\n", __FUNCTION__); print_env(e, "#\t"); }
	return e->state;
}

/* ------------ Phrase, a line, a list of expressions --------- */

typedef struct {
	size_t n;
	char **x;
} Phrase;

static Phrase *
phrase() {
	Phrase *a = malloc(sizeof(*a));
	assert(a != NULL);
	a->n = 0;
	a->x = NULL;
	return a;
}

static void
free_ph(Phrase *a) {
	assert(a != NULL);
	for (size_t i=0; i<a->n; ++i) {
		free(a->x[i]);
	}
	free(a->x);
	free(a);
}

static void
print_ph(Phrase *a) {
	assert(a != NULL);
	for (size_t i=0; i<a->n; ++i) {
		printf("%s ; ", a->x[i]);
	}
	printf("\n");
}

static Phrase *
push_ph(Phrase *A, char *b) {
	assert(b != NULL);
	if (A == NULL) {
		A = phrase();
	}
	char **c = malloc((A->n +1)*sizeof(c));
	if (A->n > 0) {
		memmove(c, A->x, A->n * sizeof(char*));
	}
	c[A->n] = b;
	if (A->n > 0) {
		free(A->x);
	}
	++(A->n);
	A->x = c;
	return A;
}

/* expression is 64 words large */
#define XSZ 64*WSZ

static Phrase *
phrase_of_str(char *a) {
	size_t boff = 0;
	char buf[XSZ];
	buf[0] = '\0';
	size_t read;
	Phrase *b = NULL;
	char *x = NULL;
	bool inspace = false;
	for (size_t off = 0; a[off] != '\0'; off += read) {
		read = grapheme_next_character_break_utf8(
				a+off, SIZE_MAX);
		if (strncmp(";", a+off, read) == 0) {
			if (boff) {
				x = malloc(1 + boff*sizeof(*x));
				strncpy(x, buf, 1+boff);
				b = push_ph(b, x);
				boff = 0;
			}
			inspace = false;
			continue;
		} 
		if (boff+read >= XSZ) {
			printf("\n? %s: expression too big (%luB)!\n", 
					__FUNCTION__,
					boff+read);
			free_ph(b);
			return NULL;
		}
		if (isspace((int)*(a+off))) {
			if (inspace) { /* remove repeated space */
				continue;
			}
			if (boff == 0) { /* left trim */
				continue;
			}
			inspace = true;
		} else {
			inspace = false;
		}
		/* default case: add character to current word */
		memmove(buf+boff, a+off, read*sizeof(char));
		boff += read;
		buf[boff] = '\0';
	}
	if (boff) {
		x = malloc(1 + boff*sizeof(*x));
		strncpy(x, buf, 1+boff);
		b = push_ph(b, x);
	}
	return b;
}

/* -------------- Phrase -------------- */

static bool
eval_ph(Env *env, Phrase *a) {
	/* evalret a line, for later line-specific behaviour */
	assert(env != NULL && "environment null");
	assert(a != NULL && "phrase null");
	for (size_t i=0; i<a->n; ++i) {
		Expr *ex = exp_of_words(a->x[i]);
		if (ex == NULL) {
			return false;
		}
		Sem *sm = seme_of_exp(ex);
		free_x(ex);
		if (sm == NULL) {
			return false;
		}
		Val *v = val_of_seme(env, sm);
		free_s(sm);
		free(sm);
		if (v == NULL) {
			return false;
		}
		if (Dbg) { printf("# %3lu %6s: ", i, "value"); printx_v(v,false,"#\t"); printf("\n"); }
		istate xs = eval(env, v);
		if (Dbg) { printf("# %3lu %6s: ", i, "eval"); print_istate(xs); printf("\n"); }
		if (xs == FATAL) {
			return false;
		}
	}
	return true;
}

/* ----- main ----- */

typedef enum {LINE, EMPTYL, ENDL, ERRL} Lrc;

static Lrc 
readline(char **S) {
	char *line = NULL;
	size_t linesz = 0;
	ssize_t rd = 0;
	errno = 0;
	rd = getline(&line, &linesz, stdin);
	if (rd == -1) {
		if (ferror(stdin) != 0) {
			printf("? %s: %s\n",
					__FUNCTION__,
					strerror(errno));
			return ERRL;
		}
		free(line);
		*S = NULL;
		return ENDL;
	}
	if (isspace((int)line[rd-1])) {
		line[rd-1] = '\0';
	}
	if (strlen(line) == 0) {
		free(line);
		*S = NULL;
		return EMPTYL;
	}
	*S = line;
	return LINE;
}

int
main(int argc, char **argv) {
	if (argc == 2) {
		Dbg = true;
	}
	/* initialize root env */
	Env *e = new_env(NULL);
	char *line;
	while (1) {
		Lrc rc = readline(&line);
		switch (rc) {
			case ERRL:
				printf("? %s: error\n", __FUNCTION__); 
				print_env(e, "?");
				free_env(e, true);
				return EXIT_FAILURE;
			case ENDL:
				if (e->state != NORM) {
					printf("? %s: unexpected end of program\n",
							__FUNCTION__);
				}
				print_env(e, ">");
				printf("> bye!\n");
				free_env(e, true);
				return EXIT_SUCCESS;
			case EMPTYL:
				continue;
			case LINE:
				printf("> input: \"%s\"\n", line);
				break;
		}
		Phrase *ph = phrase_of_str(line);
		free(line);
		if (ph == NULL) {
			free_env(e, true);
			return EXIT_FAILURE;
		}
		if (Dbg) { printf("# phrase: "); print_ph(ph); }
		bool r = eval_ph(e, ph);
		free_ph(ph);
		if (!r) {
			free_env(e, true);
			return EXIT_FAILURE;
		}
	}
	free_env(e, true);
	return EXIT_SUCCESS;
}
