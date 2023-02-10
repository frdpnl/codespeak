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
 * gcc -static -std=gnu99 -Wall -g -I../local/include -L../local/lib this_file.c -lgrapheme 
 * warning, valgrind prefers dynamic libs by default.
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
	const char *err;
	errno = 0;
	long long n = strtonum(a->v, LLONG_MIN, LLONG_MAX, &err);
	if (err == NULL) {
		return sem_nat(n);
	}
	if (errno == ERANGE) {
		printf("? %s: natural number out of range %s\n", 
				__FUNCTION__, a->v);
	} 
	return NULL;
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

typedef enum {VNIL, VNAT, VREA, VSYMOP, VSYM, VLST, VSEQ} vtype;

typedef union Val_ Val;

typedef struct List_v_ {
	size_t n;
	Val **v;
} List_v;

/* Special environment symbols: */
#define IT "it"
#define OOB "oob"

typedef struct {
	char name[WSZ];
	Val *v;
} Symval;

typedef struct {
	size_t n;
	Symval **s;
} Env;

typedef enum {
	OK,
	SKIP,	/* skip the rest of phrase (see `if), not an error */
	FATAL
} voob;  	/* OOB for out of band */

typedef struct {
	voob oob;
	Val *v;
} Rc;		/* return type for evaluation */

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
		unsigned int prio;
		Rc (*v)(Env *e, Val *s, size_t p);
		int arity;
		char name[WSZ];
	} symop;
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

static void
print_v(Val *a) {
	assert(a != NULL);
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
		case VSYMOP:
			printf("`%s ", a->symop.name);
			break;
		case VSYM:
			printf("'%s ", a->sym.v);
			break;
		case VLST:
			printf("{ ");
			for (size_t i=0; i<a->lst.v.n; ++i) {
				print_v(a->lst.v.v[i]);
			}
			printf("} ");
			break;
		case VSEQ:
			printf("( ");
			for (size_t i=0; i<a->seq.v.n; ++i) {
				print_v(a->seq.v.v[i]);
			}
			printf(") ");
			break;
		default:
			printf("? %s: unknown value\n",
					__FUNCTION__);
	}
}
static void
print_rc(Rc a) {
	switch (a.oob) {
		case OK:
			printf("OK ");
			break;
		case SKIP:
			printf("SKIP ");
			break;
		case FATAL:
			printf("FATAL ");
			break;
	}
	if (a.v != NULL) {
		print_v(a.v);
	} else {
		printf("null");
	}
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
		case VSYMOP: /* symop holds pointer to val in Syms */
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
	if (a->hdr.t == VSYMOP) {
		return a->symop.v != NULL;
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
	if (a->hdr.t == VSYMOP) {
		return (a->symop.v == b->symop.v);
	}
	if (a->hdr.t == VSYM) {
		return (strncmp(a->sym.v, b->sym.v, WSZ*sizeof(char)) == 0);
	}
	if (a->hdr.t == VLST) {
		for (size_t i=0; i<a->lst.v.n; ++i) { 
			bool c = isequal_v(a->lst.v.v[i], b->lst.v.v[i]);
			if (!c) {
				return c;
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
	if (a->hdr.t == VSYMOP) {
		return (a->symop.v == b->symop.v);
	}
	if (a->hdr.t == VSYM) {
		return (strncmp(a->sym.v, b->sym.v, WSZ*sizeof(char)) == 0);
	}
	if (a->hdr.t == VLST) {
		for (size_t i=0; i<a->lst.v.n; ++i) { 
			bool c = isequiv_v(a->lst.v.v[i], b->lst.v.v[i]);
			if (!c) {
				return c;
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
	Val *b = malloc(sizeof(Val));
	memmove(b, a, sizeof(Val));
	if (a->hdr.t == VSEQ || a->hdr.t == VLST) {
		b->seq.v.v = malloc(a->seq.v.n * sizeof(Val*));
		for (size_t i=0; i<a->seq.v.n; ++i) {
			b->seq.v.v[i] = copy_v(a->seq.v.v[i]);
		}
	}
	return b;
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
	Val **c = malloc((a->seq.v.n +1)*sizeof(Val*));
	if (a->seq.v.n > 0) {
		memmove(c, a->seq.v.v, a->seq.v.n * sizeof(Val*));
	}
	c[a->seq.v.n] = b;
	if (a->seq.v.n > 0) {
		free(a->seq.v.v);
	}
	++(a->seq.v.n);
	a->seq.v.v = c;
	return a;
}
static void
print_symval(Symval *a) {
	assert(a != NULL);
	printf("%s=", a->name);
	print_v(a->v);
}
static void
print_env(Env *a) {
	assert(a != NULL);
	for (size_t i=0; i<a->n; ++i) {
		print_symval(a->s[i]);
		printf(" ");
	}
	printf("\n");
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
free_env(Env *a) {
	if (a == NULL) {
		return;
	}
	for (size_t i=0; i<a->n; ++i) {
		free_symval(a->s[i]);
	}
	free(a->s);
	free(a);
}
static Symval *
lookup_id(Env *a, char *b, size_t *id) {
	assert(a != NULL && "env is null");
	assert(b != NULL && "name is null");
	if (strlen(b) == 0) {
		printf("? %s: symbol name null\n",
				__FUNCTION__);
		return NULL;
	}
	for (size_t i=0; i<a->n; ++i) {
		Symval *c = a->s[i];
		if (strncmp(c->name, b, WSZ*sizeof(char)) == 0) {
			if (id != NULL) {
				*id = i;
			}
			return c;
		}
	}
	return NULL;
}
static Val *
lookup(Env *a, char *b) {
	assert(a != NULL && "env null");
	assert((b != NULL && strlen(b) != 0) && "symbol name null");
	Symval *sv = lookup_id(a, b, NULL);
	if (sv == NULL) {
		return NULL;
	}
	return sv->v;
}

static bool
added_sym(Env *a, Symval *b, bool err) {
	assert(a != NULL && "environment null");
	assert(b != NULL && "symbol null");
	if (lookup(a, b->name) != NULL) {
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
	Symval *c = lookup_id(a, b->name, &id);
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
 * recommended interface (resource management):
 * 1. work off copies of arguments
 * 2. produce a fresh value (can be one of the copies)
 * 3. replace symbol operation expression with that fresh value
 * (delete previous values in the expression, add new one)
 * 4. cleanup behind you (working copies)
 * */

static Rc eval(Env *e, Val *a, bool look);

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
eval_infix_arg(Env *e, Val *s, size_t p, Val **pa, bool looka, Val **pb, bool lookb) {
	Val *a, *b;
	if (!infixed(p, s->seq.v.n)) {
		printf("? %s: symbol not infixed\n", 
				__FUNCTION__);
		return false;
	}
	Rc rc = eval(e, s->seq.v.v[p-1], looka); 
	if (rc.oob != OK) {
		printf("? %s: 1st argument null\n", 
				__FUNCTION__);
		return false;
	}
	a = rc.v;
	rc = eval(e, s->seq.v.v[p+1], lookb);
	if (rc.oob != OK) {
		printf("? %s: 2nd argument null\n", 
				__FUNCTION__);
		free_v(a);
		return false;
	}
	b = rc.v;
	*pa = a;
	*pb = b;
	return true;
}
static bool
eval_prefix1_arg(Env *e, Val *s, size_t p, Val **pa, bool looka) {
	Val *a;
	if (!prefixed1(p, s->seq.v.n)) {
		printf("? %s: symbol not prefixed to one argument\n", 
				__FUNCTION__);
		return false;
	}
	Rc rc = eval(e, s->seq.v.v[p+1], looka);
	if (rc.oob != OK) {
		printf("? %s: argument is null\n", 
				__FUNCTION__);
		return false;
	}
	a = rc.v;
	*pa = a;
	return true;
}
static bool
eval_prefix2_arg(Env *e, Val *s, size_t p, Val **pa, bool looka, Val **pb, bool lookb) {
	Val *a, *b;
	if (!prefixed2(p, s->seq.v.n)) {
		printf("? %s: symbol not prefixed to 2 arguments\n", 
				__FUNCTION__);
		return false;
	}
	Rc rc = eval(e, s->seq.v.v[p+1], looka);
	if (rc.oob != OK) {
		printf("? %s: 1st argument is null\n", 
				__FUNCTION__);
		return false;
	}
	a = rc.v;
	rc = eval(e, s->seq.v.v[p+2], lookb);
	if (rc.oob != OK) {
		printf("? %s: 2nd argument null\n", 
				__FUNCTION__);
		free_v(a);
		return false;
	}
	b = rc.v;
	*pa = a;
	*pb = b;
	return true;
}
static bool
eval_prefixn_arg(Env *e, Val *s, size_t p, Val **pa, bool looka) {
	Val *a;
	a = malloc(sizeof(*a));
	a->hdr.t = VSEQ;  /* because we copy arguments, we keep the seq type */
	a->seq.v.n = s->seq.v.n -p-1; 
	a->seq.v.v = malloc((a->seq.v.n) * sizeof(Val*));
	Rc rc;
	for (size_t i=p+1; i < s->seq.v.n; ++i) {
		//a->seq.v.v[i-p-1] = eval(e, s->seq.v.v[i], looka);
		rc = eval(e, s->seq.v.v[i], looka);
		if (rc.oob != OK) {
			free_v(a);
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
	/* consume k seq item */
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

static Rc 
eval_mul(Env *e, Val *s, size_t p) {
	Rc rc = (Rc) {FATAL, NULL};
	Val *a, *b;
	if (!eval_infix_arg(e, s, p, &a, true, &b, true)) {
		printf("? %s: infix expression invalid\n", 
				__FUNCTION__);
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
	rc = (Rc) {OK, s};
	return rc;
}
static Rc 
eval_div(Env *e, Val *s, size_t p) {
	Rc rc = (Rc) {FATAL, NULL};
	Val *a, *b;
	if (!eval_infix_arg(e, s, p, &a, true, &b, true)) {
		printf("? %s: infix expression invalid\n", 
				__FUNCTION__);
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
	rc = (Rc) {OK, s};
	return rc;
}
static Rc
eval_plu(Env *e, Val *s, size_t p) {
	Rc rc = (Rc) {FATAL, NULL};
	Val *a, *b;
	if (!eval_infix_arg(e, s, p, &a, true, &b, true)) {
		printf("? %s: infix expression invalid\n", 
				__FUNCTION__);
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
	rc = (Rc) {OK, s};
	return rc;
}
static Rc 
eval_min(Env *e, Val *s, size_t p) {
	Rc rc = (Rc) {FATAL, NULL};
	Val *a, *b;
	if (!eval_infix_arg(e, s, p, &a, true, &b, true)) {
		printf("? %s: infix expression invalid\n", 
				__FUNCTION__);
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
	rc = (Rc) {OK, s};
	return rc;
}
static Rc
eval_les(Env *e, Val *s, size_t p) {
	Rc rc = (Rc) {FATAL, NULL};
	Val *a, *b;
	if (!eval_infix_arg(e, s, p, &a, true, &b, true)) {
		printf("? %s: infix expression invalid\n", 
				__FUNCTION__);
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
	rc = (Rc) {OK, s};
	return rc;
}
static Rc
eval_leq(Env *e, Val *s, size_t p) {
	Rc rc = (Rc) {FATAL, NULL};
	Val *a, *b;
	if (!eval_infix_arg(e, s, p, &a, true, &b, true)) {
		printf("? %s: infix expression invalid\n", 
				__FUNCTION__);
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
	rc = (Rc) {OK, s};
	return rc;
}
static Rc 
eval_gre(Env *e, Val *s, size_t p) {
	Rc rc = (Rc) {FATAL, NULL};
	Val *a, *b;
	if (!eval_infix_arg(e, s, p, &a, true, &b, true)) {
		printf("? %s: infix expression invalid\n", 
				__FUNCTION__);
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
	rc = (Rc) {OK, s};
	return rc;
}
static Rc 
eval_geq(Env *e, Val *s, size_t p) {
	Rc rc = (Rc) {FATAL, NULL};
	Val *a, *b;
	if (!eval_infix_arg(e, s, p, &a, true, &b, true)) {
		printf("? %s: infix expression invalid\n", 
				__FUNCTION__);
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
	rc = (Rc) {OK, s};
	return rc;
}
static Rc 
eval_eq(Env *e, Val *s, size_t p) {
	Rc rc = (Rc) {FATAL, NULL};
	Val *a, *b;
	if (!eval_infix_arg(e, s, p, &a, true, &b, true)) {
		printf("? %s: infix expression invalid\n", 
				__FUNCTION__);
		return rc;
	}
	bool c = isequal_v(a, b);
	free_v(a);
	free_v(b);
	Val *d = malloc(sizeof(Val));
	d->hdr.t = VNAT;
	d->nat.v = c ? 1 : 0;
	upd_infix(s, p, d);
	rc = (Rc) {OK, s};
	return rc;
}
static Rc 
eval_neq(Env *e, Val *s, size_t p) {
	Rc rc = (Rc) {FATAL, NULL};
	Val *a, *b;
	if (!eval_infix_arg(e, s, p, &a, true, &b, true)) {
		printf("? %s: infix expression invalid\n", 
				__FUNCTION__);
		return rc;
	}
	bool c = isequal_v(a, b);
	free_v(a);
	free_v(b);
	Val *d = malloc(sizeof(Val));
	d->hdr.t = VNAT;
	d->nat.v = c ? 0 : 1;
	upd_infix(s, p, d);
	rc = (Rc) {OK, s};
	return rc;
}
static Rc 
eval_eqv(Env *e, Val *s, size_t p) {
	Rc rc = (Rc) {FATAL, NULL};
	Val *a, *b;
	if (!eval_infix_arg(e, s, p, &a, true, &b, true)) {
		printf("? %s: infix expression invalid\n", 
				__FUNCTION__);
		return rc;
	}
	bool c = isequiv_v(a, b);
	free_v(a);
	free_v(b);
	Val *d = malloc(sizeof(Val));
	d->hdr.t = VNAT;
	d->nat.v = c ? 1 : 0;
	upd_infix(s, p, d);
	rc = (Rc) {OK, s};
	return rc;
}
static Rc 
eval_and(Env *e, Val *s, size_t p) {
	Rc rc = (Rc) {FATAL, NULL};
	Val *a, *b;
	if (!eval_infix_arg(e, s, p, &a, true, &b, true)) {
		printf("? %s: infix expression invalid\n", 
				__FUNCTION__);
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
	rc = (Rc) {OK, s};
	return rc;
}
static Rc 
eval_or(Env *e, Val *s, size_t p) {
	Rc rc = (Rc) {FATAL, NULL};
	Val *a, *b;
	if (!eval_infix_arg(e, s, p, &a, true, &b, true)) {
		printf("? %s: infix expression invalid\n", 
				__FUNCTION__);
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
	rc = (Rc) {OK, s};
	return rc;
}
static Rc 
eval_not(Env *e, Val *s, size_t p) {
	Rc rc = (Rc) {FATAL, NULL};
	Val *a;
	if (!eval_prefix1_arg(e, s, p, &a, true)) {
		printf("? %s: prefix expression invalid\n", 
				__FUNCTION__);
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
	rc = (Rc) {OK, s};
	return rc;
}

static Rc 
eval_print(Env *e, Val *s, size_t p) {
	Rc rc = (Rc) {FATAL, NULL};
	Val *a;
	if (!eval_prefix1_arg(e, s, p, &a, false)) {
		printf("? %s: prefix expression invalid\n", 
				__FUNCTION__);
		return rc;
	}
	print_v(a);
	printf("\n");
	upd_prefix1(s, p, a);
	rc = (Rc) {OK, s};
	return rc;
}
static Rc 
eval_look(Env *e, Val *s, size_t p) {
	Rc rc = (Rc) {FATAL, NULL};
	Val *a;
	if (!eval_prefix1_arg(e, s, p, &a, true)) {
		printf("? %s: prefix expression invalid\n", 
				__FUNCTION__);
		return rc;
	}
	upd_prefix1(s, p, a);
	rc = (Rc) {OK, s};
	return rc;
}
static Rc 
eval_do(Env *e, Val *s, size_t p) {
	Val *a;
	if (!eval_prefix1_arg(e, s, p, &a, true)) {
		printf("? %s: prefix expression invalid\n", 
				__FUNCTION__);
		return (Rc) {FATAL, NULL};
	}
	if (a->hdr.t != VLST) {
		printf("? %s: argument not a list\n", 
				__FUNCTION__);
		free_v(a);
		return (Rc) {FATAL, NULL};
	}
	a->hdr.t = VSEQ;
	Rc rc = eval(e, a, false);
	free_v(a);
	if (rc.oob != OK) {
		return rc;
	}
	upd_prefix1(s, p, rc.v);
	return (Rc) {OK, s};
}
static Rc 
eval_list(Env *e, Val *s, size_t p) {
	Val *a;
	if (!eval_prefixn_arg(e, s, p, &a, false)) {
		printf("? %s: prefix expression invalid\n", 
				__FUNCTION__);
		return (Rc) {FATAL, NULL};
	}
	a->hdr.t = VLST;
	upd_prefixall(s, p, a);
	return (Rc) {OK, s};
}
static Rc 
eval_call(Env *e, Val *s, size_t p) {
	Val *a, *b;
	if (!eval_prefix2_arg(e, s, p, &a, true, &b, false)) {
		printf("? %s: prefix expression invalid\n", 
				__FUNCTION__);
		return (Rc) {FATAL, NULL};
	}
	if (b->hdr.t != VSYM) {
		printf("? %s: 2nd argument is not a symbol\n", 
				__FUNCTION__);
		free_v(a);
		free_v(b);
		return (Rc) {FATAL, NULL};
	}
	Symval *sv = symval(b->sym.v, a);
	free_v(b);
	if (sv == NULL) {
		free_v(a);
		return (Rc) {FATAL, NULL};
	}
	if (!stored_sym(e, sv)) {
		free_symval(sv);
		free_v(a);
		return (Rc) {FATAL, NULL};
	}
	upd_prefix2(s, p, a);
	return (Rc) {OK, s};
}
static Rc 
eval_true(Env *e, Val *s, size_t p) {
	Val *a;
	a = lookup(e, IT);
	if (a == NULL) {
		printf("? %s: 'it' undefined\n", 
				__FUNCTION__);
		return (Rc) {FATAL, NULL};
	}
	Val *b = malloc(sizeof(*b));
	assert(b != NULL);
	b->hdr.t = VNAT;
	b->nat.v = istrue_v(a);
	upd_prefix0(s, p, b);
	return (Rc) {OK, s};
}
static Rc 
eval_false(Env *e, Val *s, size_t p) {
	Val *a;
	a = lookup(e, IT);
	if (a == NULL) {
		printf("? %s: 'it' undefined\n", 
				__FUNCTION__);
		return (Rc) {FATAL, NULL};
	}
	Val *b = malloc(sizeof(*b));
	assert(b != NULL);
	b->hdr.t = VNAT;
	b->nat.v = !istrue_v(a);
	upd_prefix0(s, p, b);
	return (Rc) {OK, s};
}
static Rc 
eval_if(Env *e, Val *s, size_t p) {
	if (p != s->seq.v.n - 2) {
		printf("? %s: too many arguments to 'if'\n",
				__FUNCTION__);
		return (Rc) {FATAL, NULL};
	}
	Val *a;
	if (!eval_prefix1_arg(e, s, p, &a, true)) {
		printf("? %s: prefix expression invalid\n", 
				__FUNCTION__);
		return (Rc) {FATAL, NULL};
	}
	Val *b = malloc(sizeof(*b));
	assert(b != NULL);
	b->hdr.t = VNAT;
	Rc rc;
	if (istrue_v(a)) {
		b->nat.v = 1;
		rc.oob = OK;
	} else {
		b->nat.v = 0;
		rc.oob = SKIP;
	} 
	free_v(a);
	upd_prefix1(s, p, b);
	rc.v = s;
	return rc;
}


/* --------------- builtin or base function symbols -------------------- */

typedef struct Symop_ {
	char name[WSZ];
	unsigned int prio;
	Rc (*f)(Env *e, Val *s, size_t p);
	int arity;
} Symop;

#define NSYMS 22
Symop Syms[] = {
	(Symop) {"true?",  10, eval_true,  0},
	(Symop) {"false?", 10, eval_false, 0},
	(Symop) {"look",   10, eval_look,  1},
	(Symop) {"do",     10, eval_do,    1},
	(Symop) {"list",   10, eval_list, -1},
	(Symop) {"call",   10, eval_call,  2},
	(Symop) {"print",  10, eval_print, 1},
	(Symop) {"*",    20, eval_mul, 2},
	(Symop) {"/",    20, eval_div, 2},
	(Symop) {"+",    30, eval_plu, 2},
	(Symop) {"-",    30, eval_min, 2},
	(Symop) {"<",    40, eval_les, 2},
	(Symop) {"<=",   40, eval_leq, 2},
	(Symop) {">",    40, eval_gre, 2},
	(Symop) {">=",   40, eval_geq, 2},
	(Symop) {"=",    40, eval_eq,  2},
	(Symop) {"/=",   40, eval_neq, 2},
	(Symop) {"~=",   40, eval_eqv, 2},
	(Symop) {"not",  50, eval_not, 1},
	(Symop) {"and",  60, eval_and, 2},
	(Symop) {"or",   60, eval_or,  2},
	(Symop) {"if",   80, eval_if,  1},
};
static unsigned int
minprio() {
	unsigned int m = 0;
	for (int i=0; i<NSYMS; ++i) {
		if (Syms[i].prio > m) {
			m = Syms[i].prio;
		}
	}
	return m;
}
static Symop *
lookup_op(char *a) {
	for (size_t i=0; i<NSYMS; ++i) {
		Symop *b = Syms+i;
		if (strncmp(b->name, a, WSZ) == 0) {
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
			|| a->hdr.t == VSYMOP);
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
		/* builtin operators, and special symbol 'it are lookd now */
		Symop *so = lookup_op(s->sym.v);
		if (so != NULL) {
			a = malloc(sizeof(*a));
			a->hdr.t = VSYMOP;
			a->symop.prio = so->prio;
			a->symop.v = so->f;
			a->symop.arity = so->arity;
			strncpy(a->symop.name, so->name, 1+strlen(so->name));
			return a;
		}
		a = lookup(e, s->sym.v);
		if (strncmp(s->sym.v, IT, 3) == 0) {
			if (a == NULL) {
				printf("? %s: 'it' symbol undefined\n",
						__FUNCTION__);
				return NULL;
			}
			return copy_v(a);
		}
		if (a != NULL && a->hdr.t == VSYMOP) {
			return copy_v(a);
		}
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

static Rc 
eval_members1(Env *e, Val *a, bool look) {
	/* works on SEQ and LST */
	assert(a->hdr.t == VSEQ || a->hdr.t == VLST);
	Val *b = NULL;
	Rc rc = (Rc) {FATAL, NULL};
	for (size_t i=0; i < a->seq.v.n; ++i) {
		rc = eval(e, a->seq.v.v[i], look);
		if (rc.oob == FATAL) {
			printf("? %s: list or seq item unknown\n",
					__FUNCTION__);
			free_v(b);
			return rc;
		} else if (rc.oob == SKIP) {
			printf("? %s: list or seq item invalid\n",
					__FUNCTION__);
			free_v(b);
			free_v(rc.v);
			rc = (Rc) {FATAL, NULL};
			return rc;
		}
		b = push_v(a->hdr.t, b, rc.v);
	}
	rc = (Rc) {OK, b};
	return rc;
}

static Rc 
eval_seq(Env *e, Val *b, bool look) {
	/* symbol application: consumes the seq, until 1 item left */
	Rc rc = (Rc) {OK, NULL};
	Val *c;
	while (b->seq.v.n > 0) {
		/* stop condition: seq reduces to single element */
		if (b->seq.v.n == 1) {
			Val *d = b->seq.v.v[0];
			/* except, execute sequence of single unarity function */
			if (!(d->hdr.t == VSYMOP && d->symop.arity == 0)) {
				rc.v = copy_v(d);
				/* rc.oob comes from loop body */
				return rc;
			}
		}
		unsigned int hiprio = minprio()+1;
		size_t symat = 0;
		bool symfound = false;
		/* apply symops from left to right (for same priority symbols) */
		for (size_t i=0; i < b->seq.v.n; ++i) {
			c = b->seq.v.v[i];
			if (c->hdr.t == VSYMOP) {
				if (c->symop.prio < hiprio) {
					hiprio = c->symop.prio;
					symat = i;
					symfound = true;
				}
			} 
		}
		if (!symfound) { 
			printf("? %s: sequence without function symbol\n",
					__FUNCTION__);
			return (Rc) {FATAL, NULL};
		}
		/* apply the symbol, returns the reduced seq */
		printf("#  %s >> `%s of ", 
				__FUNCTION__, b->seq.v.v[symat]->symop.name);
		print_v(b); printf("\n");
		rc = b->seq.v.v[symat]->symop.v(e, b, symat);
		if (rc.oob == FATAL) {
			assert(rc.v == NULL);
			printf("? %s: symbol application failed\n",
					__FUNCTION__);
			return rc;
		}
		printf("#  %s << ",__FUNCTION__); print_rc(rc); printf("\n");
		b = rc.v;
	}
	/* empty seq, means nil value */
	c = malloc(sizeof(*c));
	c->hdr.t = VNIL;
	rc = (Rc) {OK, c};
	return rc;
}

static Rc 
eval(Env *e, Val *a, bool look) {
	/* returns a fresh value, 'a untouched */
	assert(e != NULL && "env is null");
	assert(a != NULL && "value is null");
	Rc rc = (Rc) {FATAL, NULL};
	if (isatom_v(a)) {
		if (look && a->hdr.t == VSYM) {
			Val *b = lookup(e, a->sym.v);
			if (b == NULL) {
				printf("? %s: unknown symbol '%s'\n",
					__FUNCTION__, a->sym.v);
				return (Rc) {FATAL, NULL};
			} 
			rc = (Rc) {OK, copy_v(b)};
			return rc;
		} 
		/* default: let the function handle it */
		rc = (Rc) {OK, copy_v(a)};
		return rc;
	}
	if (a->hdr.t == VLST) {
		return eval_members1(e, a, look);
	}
	if (a->hdr.t == VSEQ) {
		rc = eval_members1(e, a, look);
		if (rc.oob != OK) {
			return rc;
		}
		Val *b = rc.v;
		rc = eval_seq(e, b, look);
		free_v(b);
		return rc;
	}
	printf("? %s: unknown value type\n",
			__FUNCTION__);
	return (Rc) {FATAL, NULL};
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
push_ph(Phrase *a, char *b) {
	assert(b != NULL);
	if (a == NULL) {
		a = phrase();
	}
	char **c = malloc((a->n +1)*sizeof(c));
	if (a->n > 0) {
		memmove(c, a->x, a->n * sizeof(char*));
	}
	c[a->n] = b;
	if (a->n > 0) {
		free(a->x);
	}
	++(a->n);
	a->x = c;
	return a;
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
eval_ph(Env *e_o, Phrase *a) {
	assert(e_o != NULL && "environment null");
	assert(a != NULL && "phrase null");
	for (size_t i=0; i<a->n; ++i) {
		printf("# %3lu %5s: %s\n", i, "expr", a->x[i]);
		Expr *ex = exp_of_words(a->x[i]);
		/* a is freed where allocated, not here but by caller */
		if (ex == NULL) {
			return false;
		}
		Sem *sm = seme_of_exp(ex);
		free_x(ex);
		if (sm == NULL) {
			return false;
		}
		printf("# %3lu %5s: ", i, "seme"); print_s(sm); printf("\n");
		Val *v = val_of_seme(e_o, sm);
		free_s(sm);
		free(sm);
		if (v == NULL) {
			return false;
		}
		printf("# %3lu %5s: ", i, "value"); print_v(v); printf("\n");
		Rc rcv = eval(e_o, v, false);
		free_v(v);
		printf("# %3lu %5s: ", i, "eval"); print_rc(rcv); printf("\n"); 
		if (rcv.oob == FATAL) {
			return false;
		}
		Symval *it = symval(IT, rcv.v);
		free_v(rcv.v);
		if (it == NULL) {
			return false;
		}
		if (!stored_sym(e_o, it)) {
			free_symval(it);
			return false;
		}
		printf("# %3lu %5s: ", i, "env"); print_env(e_o); 
		if (rcv.oob == SKIP) {
			break;
		}
	}
	return true;
}

/* ----- main ----- */

static void
usage(const char *exe) {
	printf("usage: %s\n", exe);
}

enum Rstatus {LINE, EMPTY, END, ERR};

static enum Rstatus 
readline(char **s_o) {
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
			return ERR;
		}
		free(line);
		*s_o = NULL;
		return END;
	}
	if (isspace((int)line[rd-1])) {
		line[rd-1] = '\0';
	}
	if (strlen(line) == 0) {
		free(line);
		*s_o = NULL;
		return EMPTY;
	}
	*s_o = line;
	return LINE;
}

int
main(int argc, char **argv) {
	if (argc != 1) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}
	Env *e = malloc(sizeof(*e));
	assert(e != NULL);
	e->n = 0;
	e->s = NULL;
	char *line;
	while (1) {
		enum Rstatus lrc = readline(&line);
		switch (lrc) {
			case ERR:
				free_env(e);
				return EXIT_FAILURE;
			case END:
				free_env(e);
				return EXIT_SUCCESS;
			case EMPTY:
				continue;
			case LINE:
				printf("# ----line: \"%s\"\n", line);
				break;
		}
		Phrase *ph = phrase_of_str(line);
		free(line);
		if (ph == NULL) {
			free_env(e);
			return EXIT_FAILURE;
		}
		printf("# --phrase: "); print_ph(ph); 
		bool evrc = eval_ph(e, ph);
		free_ph(ph);
		if (!evrc) {
			free_env(e);
			return EXIT_FAILURE;
		}
	}
	free_env(e);
	return EXIT_SUCCESS;
}
