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

typedef enum {VNIL, VNAT, VREA, VSYMOP, VSYMF, VSYM, VLST, VSEQ} vtype;

typedef union Val_ Val;

typedef struct List_v_ {
	size_t n;
	Val **v;
} List_v;

/* Special environment symbols: */
#define IT "it"
#define ENDF "end"
#define ENDIF "endif"

typedef enum {
	UNSET,
	OK,
	SKIP,	/* for 'if, skip until 'else or 'endif */
	DEF,	/* we're processing a (multiline) function definition */
	RETURN,	/* returning from function (short circuit) */
	BACKTRACK,
	FATAL
} inxs;

typedef struct {
	char name[WSZ];
	Val *v;
} Symval;

typedef struct Env_ {
	inxs state;
	size_t n;
	Symval **s;
	struct Env_ *parent;
} Env;

typedef struct {
	inxs state;
	Val *v;
} Ir;		/* return type for reduce */

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
		Ir (*v)(Env *e, Val *s, size_t p);
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

static void
print_v(Val *a) {
	if (a == NULL) {
		printf("? %s: NULL\n", __FUNCTION__);
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
		case VSYMOP:
			printf("`%s ", a->symop.name);
			break;
		case VSYMF:
			printf("`(%s ", a->symf.name);
			for (size_t i=0; i<a->symf.param.n; ++i) {
				print_v(a->symf.param.v[i]);
			}
			printf("[%lu] ", a->symf.body.n);
			for (size_t i=0; i<a->symf.body.n; ++i) {
				size_t N = 2;
				if (i > N && i < a->symf.body.n - N) {
					printf(".");
				} else {
					print_v(a->symf.body.v[i]);
				}
			}
			printf(") ");
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
print_inxs(inxs s) {
	switch (s) {
		case UNSET:
			printf("Unset ");
			break;
		case OK:
			printf("Ok ");
			break;
		case SKIP:
			printf("Skip ");
			break;
		case DEF:
			printf("Fun ");
			break;
		case RETURN:
			printf("Return ");
			break;
		case BACKTRACK:
			printf("Error ");
			break;
		case FATAL:
			printf("Fatal ");
			break;
	}
}
static void
print_rc(Ir a) {
	printf("[");
	print_inxs(a.state);
	printf(", ");
	if (a.v != NULL) {
		print_v(a.v);
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
		case VSYMOP: /* symop holds postate to val in Syms */
			break;
		case VSYMF:
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
	if (a->hdr.t == VSYMOP) {
		return a->symop.v != NULL;
	}
	if (a->hdr.t == VSYMF) {
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
	if (a->hdr.t == VSYMOP) {
		return (a->symop.v == b->symop.v);
	}
	if (a->hdr.t == VSYMF) {
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
	if (a->hdr.t == VSYMOP) {
		return (a->symop.v == b->symop.v);
	}
	if (a->hdr.t == VSYMF) {
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
	} else if (a->hdr.t == VSYMF) {
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
	c[a.n] = copy_v(b);
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
print_symval(Symval *a) {
	assert(a != NULL);
	printf("%s = ", a->name);
	print_v(a->v);
}
static void
print_env(Env *a, const char *col1) {
	assert(a != NULL);
	printf("%s env:\n", col1);
	printf("%s\tstate: ", col1); print_inxs(a->state); printf("\n");
	for (size_t i=0; i<a->n; ++i) {
		printf("%s\t", col1);
		print_symval(a->s[i]);
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
	for (size_t i=0; i<a->n; ++i) {
		Symval *c = a->s[i];
		if (strncmp(c->name, b, WSZ*sizeof(char)) == 0) {
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
lookup(Env *a, char *b, bool global) {
	assert(a != NULL && "env null");
	assert((b != NULL && strlen(b) != 0) && "symbol name null");
	Symval *sv = lookup_id(a, b, global, NULL);
	if (sv == NULL) {
		return NULL;
	}
	return sv->v;
}

static bool
added_sym(Env *a, Symval *b, bool err) {
	assert(a != NULL && "environment null");
	assert(b != NULL && "symbol null");
	if (lookup(a, b->name, false) != NULL) {
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

static Ir interp_now(Env *e, Val *a, bool look);
static Ir interp(Env *e, Val *a);

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
reduce_infix_arg(Env *e, Val *s, size_t p, Val **pa, bool looka, Val **pb, bool lookb) {
	Val *a, *b;
	if (!infixed(p, s->seq.v.n)) {
		printf("? %s: symbol not infixed\n", 
				__FUNCTION__);
		return false;
	}
	Ir rc = interp_now(e, s->seq.v.v[p-1], looka); 
	if (rc.state != OK) {
		printf("? %s: 1st argument null\n", 
				__FUNCTION__);
		return false;
	}
	a = rc.v;
	rc = interp_now(e, s->seq.v.v[p+1], lookb);
	if (rc.state != OK) {
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
reduce_prefix1_arg(Env *e, Val *s, size_t p, Val **pa, bool looka) {
	Val *a;
	if (!prefixed1(p, s->seq.v.n)) {
		printf("? %s: symbol not prefixed to one argument\n", 
				__FUNCTION__);
		return false;
	}
	Ir rc = interp_now(e, s->seq.v.v[p+1], looka);
	if (rc.state != OK) {
		return false;
	}
	a = rc.v;
	*pa = a;
	return true;
}
static bool
reduce_prefix2_arg(Env *e, Val *s, size_t p, Val **pa, bool looka, Val **pb, bool lookb) {
	Val *a, *b;
	if (!prefixed2(p, s->seq.v.n)) {
		printf("? %s: symbol not prefixed to 2 arguments\n", 
				__FUNCTION__);
		return false;
	}
	Ir rc = interp_now(e, s->seq.v.v[p+1], looka);
	if (rc.state != OK) {
		printf("? %s: 1st argument is null\n", 
				__FUNCTION__);
		return false;
	}
	a = rc.v;
	rc = interp_now(e, s->seq.v.v[p+2], lookb);
	if (rc.state != OK) {
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
reduce_prefixn_arg(Env *e, Val *s, size_t p, Val **pa, bool looka) {
	Val *a;
	a = malloc(sizeof(*a));
	a->hdr.t = VSEQ;  /* because we copy arguments, we keep the seq type */
	a->seq.v.n = s->seq.v.n -p-1; 
	a->seq.v.v = malloc((a->seq.v.n) * sizeof(Val*));
	Ir rc;
	for (size_t i=p+1; i < s->seq.v.n; ++i) {
		rc = interp_now(e, s->seq.v.v[i], looka);
		if (rc.state != OK) {
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

static Ir 
reduce_mul(Env *e, Val *s, size_t p) {
	Ir rc = (Ir) {FATAL, NULL};
	Val *a, *b;
	if (!reduce_infix_arg(e, s, p, &a, true, &b, true)) {
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
	rc = (Ir) {OK, s};
	return rc;
}
static Ir 
reduce_div(Env *e, Val *s, size_t p) {
	Ir rc = (Ir) {FATAL, NULL};
	Val *a, *b;
	if (!reduce_infix_arg(e, s, p, &a, true, &b, true)) {
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
	rc = (Ir) {OK, s};
	return rc;
}
static Ir
reduce_plu(Env *e, Val *s, size_t p) {
	Ir rc = (Ir) {FATAL, NULL};
	Val *a, *b;
	if (!reduce_infix_arg(e, s, p, &a, true, &b, true)) {
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
	rc = (Ir) {OK, s};
	return rc;
}
static Ir 
reduce_min(Env *e, Val *s, size_t p) {
	Ir rc = (Ir) {FATAL, NULL};
	Val *a, *b;
	if (!reduce_infix_arg(e, s, p, &a, true, &b, true)) {
		return rc;
	}
	if ((a->hdr.t != VNAT && a->hdr.t != VREA)
			|| (b->hdr.t != VNAT && b->hdr.t != VREA)) {
		printf("? %s: arguments not numbers in (", 
				__FUNCTION__);
		print_v(s);
		printf(")\n");
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
	rc = (Ir) {OK, s};
	return rc;
}
static Ir
reduce_les(Env *e, Val *s, size_t p) {
	Ir rc = (Ir) {FATAL, NULL};
	Val *a, *b;
	if (!reduce_infix_arg(e, s, p, &a, true, &b, true)) {
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
	rc = (Ir) {OK, s};
	return rc;
}
static Ir
reduce_leq(Env *e, Val *s, size_t p) {
	Ir rc = (Ir) {FATAL, NULL};
	Val *a, *b;
	if (!reduce_infix_arg(e, s, p, &a, true, &b, true)) {
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
	rc = (Ir) {OK, s};
	return rc;
}
static Ir 
reduce_gre(Env *e, Val *s, size_t p) {
	Ir rc = (Ir) {FATAL, NULL};
	Val *a, *b;
	if (!reduce_infix_arg(e, s, p, &a, true, &b, true)) {
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
	rc = (Ir) {OK, s};
	return rc;
}
static Ir 
reduce_geq(Env *e, Val *s, size_t p) {
	Ir rc = (Ir) {FATAL, NULL};
	Val *a, *b;
	if (!reduce_infix_arg(e, s, p, &a, true, &b, true)) {
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
	rc = (Ir) {OK, s};
	return rc;
}
static Ir 
reduce_eq(Env *e, Val *s, size_t p) {
	Ir rc = (Ir) {FATAL, NULL};
	Val *a, *b;
	if (!reduce_infix_arg(e, s, p, &a, true, &b, true)) {
		return rc;
	}
	bool c = isequal_v(a, b);
	free_v(a);
	free_v(b);
	Val *d = malloc(sizeof(Val));
	d->hdr.t = VNAT;
	d->nat.v = c ? 1 : 0;
	upd_infix(s, p, d);
	rc = (Ir) {OK, s};
	return rc;
}
static Ir 
reduce_neq(Env *e, Val *s, size_t p) {
	Ir rc = (Ir) {FATAL, NULL};
	Val *a, *b;
	if (!reduce_infix_arg(e, s, p, &a, true, &b, true)) {
		return rc;
	}
	bool c = isequal_v(a, b);
	free_v(a);
	free_v(b);
	Val *d = malloc(sizeof(Val));
	d->hdr.t = VNAT;
	d->nat.v = c ? 0 : 1;
	upd_infix(s, p, d);
	rc = (Ir) {OK, s};
	return rc;
}
static Ir 
reduce_eqv(Env *e, Val *s, size_t p) {
	Ir rc = (Ir) {FATAL, NULL};
	Val *a, *b;
	if (!reduce_infix_arg(e, s, p, &a, true, &b, true)) {
		return rc;
	}
	bool c = isequiv_v(a, b);
	free_v(a);
	free_v(b);
	Val *d = malloc(sizeof(Val));
	d->hdr.t = VNAT;
	d->nat.v = c ? 1 : 0;
	upd_infix(s, p, d);
	rc = (Ir) {OK, s};
	return rc;
}
static Ir 
reduce_and(Env *e, Val *s, size_t p) {
	Ir rc = (Ir) {FATAL, NULL};
	Val *a, *b;
	if (!reduce_infix_arg(e, s, p, &a, true, &b, true)) {
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
	rc = (Ir) {OK, s};
	return rc;
}
static Ir 
reduce_or(Env *e, Val *s, size_t p) {
	Ir rc = (Ir) {FATAL, NULL};
	Val *a, *b;
	if (!reduce_infix_arg(e, s, p, &a, true, &b, true)) {
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
	rc = (Ir) {OK, s};
	return rc;
}
static Ir 
reduce_not(Env *e, Val *s, size_t p) {
	Ir rc = (Ir) {FATAL, NULL};
	Val *a;
	if (!reduce_prefix1_arg(e, s, p, &a, true)) {
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
	rc = (Ir) {OK, s};
	return rc;
}

static Ir 
reduce_print(Env *e, Val *s, size_t p) {
	Ir rc = (Ir) {FATAL, NULL};
	Val *a;
	if (!reduce_prefix1_arg(e, s, p, &a, true)) {
		return rc;
	}
	print_v(a);
	printf("\n");
	upd_prefix1(s, p, a);
	rc = (Ir) {OK, s};
	return rc;
}
static Ir 
reduce_solve(Env *e, Val *s, size_t p) {
	Ir rc = (Ir) {FATAL, NULL};
	Val *a;
	if (!reduce_prefix1_arg(e, s, p, &a, true)) {
		return rc;
	}
	upd_prefix1(s, p, a);
	rc = (Ir) {OK, s};
	return rc;
}
static Ir 
reduce_do(Env *e, Val *s, size_t p) {
	Val *a;
	if (!reduce_prefix1_arg(e, s, p, &a, false)) {
		return (Ir) {FATAL, NULL};
	}
	if (a->hdr.t == VSYM) {
		Val *b = lookup(e, a->sym.v, false);
		if (b == NULL) {
			printf("? %s: argument symbol undefined (%s)\n", 
					__FUNCTION__, a->sym.v);
			free_v(a);
			return (Ir) {FATAL, NULL};
		}
		free_v(a);
		a = copy_v(b);
	}
	if (a->hdr.t != VLST) {
		printf("? %s: argument not a list\n", 
				__FUNCTION__);
		free_v(a);
		return (Ir) {FATAL, NULL};
	}
	a->hdr.t = VSEQ;
	Ir rc = interp_now(e, a, false);
	free_v(a);
	if (rc.state == FATAL) {
		return rc;
	}
	upd_prefix1(s, p, rc.v);
	return (Ir) {rc.state, s};
}
static Ir 
reduce_list(Env *e, Val *s, size_t p) {
	Val *a;
	if (!reduce_prefixn_arg(e, s, p, &a, false)) {
		return (Ir) {FATAL, NULL};
	}
	a->hdr.t = VLST;
	upd_prefixall(s, p, a);
	return (Ir) {OK, s};
}
static Ir 
reduce_call(Env *e, Val *s, size_t p) {
	Val *a, *b;
	if (!reduce_prefix2_arg(e, s, p, &a, true, &b, false)) {
		return (Ir) {FATAL, NULL};
	}
	if (b->hdr.t != VSYM) {
		printf("? %s: 2nd argument is not a symbol\n", 
				__FUNCTION__);
		free_v(a);
		free_v(b);
		return (Ir) {FATAL, NULL};
	}
	Symval *sv = symval(b->sym.v, a);
	free_v(b);
	if (sv == NULL) {
		free_v(a);
		return (Ir) {FATAL, NULL};
	}
	if (!stored_sym(e, sv)) {
		free_symval(sv);
		free_v(a);
		return (Ir) {FATAL, NULL};
	}
	upd_prefix2(s, p, a);
	return (Ir) {OK, s};
}
static Ir 
reduce_true(Env *e, Val *s, size_t p) {
	Val *a;
	a = lookup(e, IT, false);
	if (a == NULL) {
		printf("? %s: 'it' undefined\n", 
				__FUNCTION__);
		return (Ir) {FATAL, NULL};
	}
	Val *b = malloc(sizeof(*b));
	assert(b != NULL);
	b->hdr.t = VNAT;
	b->nat.v = istrue_v(a);
	upd_prefix0(s, p, b);
	return (Ir) {OK, s};
}
static Ir 
reduce_false(Env *e, Val *s, size_t p) {
	Val *a;
	a = lookup(e, IT, false);
	if (a == NULL) {
		printf("? %s: 'it' undefined\n", 
				__FUNCTION__);
		return (Ir) {FATAL, NULL};
	}
	Val *b = malloc(sizeof(*b));
	assert(b != NULL);
	b->hdr.t = VNAT;
	b->nat.v = !istrue_v(a);
	upd_prefix0(s, p, b);
	return (Ir) {OK, s};
}
static Ir 
reduce_if(Env *e, Val *s, size_t p) {
	if (p != 0 || p != s->seq.v.n - 2) {
		printf("? %s: 'if' sequence invalid\n",
				__FUNCTION__);
		return (Ir) {FATAL, NULL};
	}
	Val *a;
	if (!reduce_prefix1_arg(e, s, p, &a, true)) {
		return (Ir) {FATAL, NULL};
	}
	Val *b = malloc(sizeof(*b));
	assert(b != NULL);
	b->hdr.t = VNAT;
	Ir rc;
	if (istrue_v(a)) {
		b->nat.v = 1;
		rc.state = OK;
	} else {
		b->nat.v = 0;
		rc.state = SKIP;
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

static Ir 
reduce_else(Env *e, Val *s, size_t p) {
	if (!(p == 0 && s->seq.v.n == 1)) {
		printf("? %s: `else syntax incorrect\n", __FUNCTION__);
		return (Ir) {FATAL, NULL};
	}
	if (e->state == SKIP) {
		Val *a = malloc(sizeof(*a));
		a->hdr.t = VNAT;
		a->nat.v = 1;
		upd_prefix0(s, p, a);
		return (Ir) {OK, s};
	}
	if (e->state == OK) {
		Val *it = lookup(e, IT, false);
		if (it == NULL) {
			printf("? %s: `else before `if\n", __FUNCTION__);
			return (Ir) {FATAL, NULL};
		} 
		Val *b = copy_v(it);
		upd_prefix0(s, p, b);
		return (Ir) {SKIP, s};
	}
	printf("? %s: `else in invalid state (%u)\n", __FUNCTION__, e->state);
	return (Ir) {FATAL, NULL};
}
static Ir 
reduce_rem(Env *e, Val *s, size_t p) {
	Ir rc;
	Val *b;
	Val *a = lookup(e, IT, false);
	if (a == NULL) {
		b = malloc(sizeof(*b));
		assert(b != NULL);
		b->hdr.t = VNIL;
	} else {
		b = copy_v(a);
	}
	upd_prefixall(s, p, b);
	rc.state = OK;
	rc.v = s;
	return rc;
}

/* --- reduce function definition --- */
static Ir
reduce_def(Env *e, Val *s, size_t p) {
	/* rem: define foo (a, b) or define foo () ; */
	if (s->seq.v.n != 3 || p != 0) {
		printf("? %s: incorrect number of arguments to 'define'\n",
				__FUNCTION__);
		return (Ir) {FATAL, NULL};
	}
	Val *fname, *fparam;
	if (!reduce_prefix2_arg(e, s, p, &fname, false, &fparam, false)) {
		return (Ir) {FATAL, NULL};
	}
	if (fname->hdr.t != VSYM) {
		printf("? %s: expecting symbol for function name\n",
				__FUNCTION__);
		free_v(fname);
		free_v(fparam);
		return (Ir) {FATAL, NULL};
	}
	if (!(fparam->hdr.t == VLST || fparam->hdr.t == VNIL)) {
		printf("? %s: expecting list or '()' for function parameters\n",
				__FUNCTION__);
		free_v(fname);
		free_v(fparam);
		return (Ir) {FATAL, NULL};
	}
	if (fparam->hdr.t == VLST) {
		for (size_t i=0; i < fparam->lst.v.n; ++i) {
			if (fparam->lst.v.v[i]->hdr.t != VSYM) {
				printf("? %s: expecting symbol for function parameter\n",
						__FUNCTION__);
				free_v(fname);
				free_v(fparam);
				return (Ir) {FATAL, NULL};
			}
		}
	}
	Val *f = malloc(sizeof(*f));
	assert(f != NULL && "function val NULL");
	f->hdr.t = VSYMF;
	strncpy(f->symf.name, fname->sym.v, sizeof(f->symf.name));
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
	free_v(fparam);
	upd_prefix2(s, p, f);
	return (Ir) {DEF, s};
}
static Ir 
reduce_end(Env *e, Val *s, size_t p) {
	/* rem: end somefun or end if ; */
	if (p != 0 || s->seq.v.n != 2) {
		printf("? %s: invalid 'end', expecting argument\n",
				__FUNCTION__);
		return (Ir) {FATAL, NULL};
	}
	Val *a;
	if (!reduce_prefix1_arg(e, s, p, &a, false)) {
		return (Ir) {FATAL, NULL};
	}
	Val *b = NULL;
	if (a->hdr.t == VSYMOP) {
		/* rem: end if */
		if (a->symop.v != reduce_if) {
			printf("? %s: 'end' with wrong operator argument (expecting 'if')\n",
					__FUNCTION__);
			free_v(a);
			return (Ir) {FATAL, NULL};
		}
		Val *c = lookup(e, IT, false);
		if (c == NULL) {
			printf("? %s: 'it' required (the 'if condition), but undefined\n",
					__FUNCTION__);
			free_v(a);
			return (Ir) {FATAL, NULL};
		} else {
			b = copy_v(c);
		}
		free_v(a);
	} else if (a->hdr.t == VSYM) {
		/* rem: end function */
		if (e->state != DEF) {
			printf("? %s: 'end' outside function definition\n", 
					__FUNCTION__);
			free_v(a);
			return (Ir) {FATAL, NULL};
		}
		Val *c = lookup(e, IT, false);
		if (c == NULL) {
			printf("? %s: 'it' required, yet undefined\n", 
					__FUNCTION__);
			free_v(a);
			return (Ir) {FATAL, NULL};
		}
		if (c->hdr.t != VSYMF) {
			printf("? %s: 'end' argument is not a function name\n", 
					__FUNCTION__);
			free_v(a);
			return (Ir) {FATAL, NULL};
		}
		if (strncmp(a->sym.v, c->symf.name, sizeof(a->sym.v)) != 0) {
			/* rem: not the function being defined, then part of the body */
			free_v(a);
			return (Ir) {BACKTRACK, NULL};
		}
		/* rem: end 'fun ; add the fun symbol */
		b = copy_v(c);
		Symval *sv = symval(b->symf.name, b);
		if (!stored_sym(e, sv)) {
			free_symval(sv);
			free_v(a);
			return (Ir) {FATAL, NULL};
		}
		free_v(a);
	} else {
		printf("? %s: 'end' with wrong argument type\n",
				__FUNCTION__);
		free_v(a);
		return (Ir) {FATAL, NULL};
	}
	upd_prefix1(s, p, b);
	return (Ir) {OK, s};
}
/* --- reduce (user) function application --- */
static Ir 
reduce_fun(Env *e, Val *s, size_t p) {
	/* rem: ... foo (1, 2) or foo () ... */
	Val *al;
	Val *f = s->seq.v.v[p];
	if (!reduce_prefix1_arg(e, s, p, &al, true)) {
		printf("? %s: invalid argument to `%s\n", 
				__FUNCTION__, f->symf.name);
		return (Ir) {FATAL, NULL};
	}
	if (!(al->hdr.t == VLST || al->hdr.t == VNIL)) {
		printf("? %s: argument to `%s not a list or '()'\n", 
				__FUNCTION__, f->symf.name);
		free_v(al);
		return (Ir) {FATAL, NULL};
	}
	if (al->hdr.t == VNIL && f->symf.param.n != 0) {
		printf("? %s: expected %lu argument(s) to `%s\n", 
				__FUNCTION__, f->symf.param.n, f->symf.name);
		free_v(al);
		return (Ir) {FATAL, NULL};
	}
	if (al->hdr.t == VLST && al->lst.v.n != f->symf.param.n) {
		printf("? %s: number of arguments to `%s mismatch (got %lu, expected %lu)\n", 
				__FUNCTION__, f->symf.name,
				al->lst.v.n, f->symf.param.n);
		free_v(al);
		return (Ir) {FATAL, NULL};
	}
	/* setup local env */
	Env *le = malloc(sizeof(*le));
	assert(le != NULL);
	le->state = OK;
	le->n = 0;
	le->s = NULL;
	le->parent = e;
	if (al->hdr.t == VLST) {
		/* add symval for each param, value is al's */
		for (size_t i=0; i<f->symf.param.n; ++i) {
			Symval *sv = symval(f->symf.param.v[i]->sym.v, al->lst.v.v[i]);
			if (!stored_sym(le, sv)) {
				free_symval(sv);
				free_v(al);
				free_env(le, false);
				return (Ir) {FATAL, NULL};
			}
		}
	}
	free_v(al);
	/* reduce each expression in body, like interp_ph: */
	Val *v;
	for (size_t i=0; i<f->symf.body.n; ++i) {
		v = copy_v(f->symf.body.v[i]);
		if (Dbg) { printf("##  %s %5s: ", __FUNCTION__, "value"); print_v(v); printf("\n"); }
		Ir xs = interp(le, v);
		free_v(v);
		if (Dbg) { printf("##  %s %5s: ", __FUNCTION__, "reduce"); print_rc(xs); printf("\n"); } 
		le->state = xs.state;
		if (le->state == FATAL) {
			free_env(le, false);
			return (Ir) {FATAL, NULL};
		}
		Symval *it = symval(IT, xs.v);
		free_v(xs.v);
		if (it == NULL) {
			free_env(le, false);
			return (Ir) {FATAL, NULL};
		}
		xs.v = NULL;
		if (!stored_sym(le, it)) {
			free_symval(it);
			free_env(le, false);
			return (Ir) {FATAL, NULL};
		}
		if (le->state == RETURN) {
			break;
		}
	}
	if (Dbg) { printf("##  %s %5s:\n", __FUNCTION__, "local"); print_env(le, "##"); }
	/* return local (function's) 'it to caller */
	char *sym = IT;
	Val *lit = NULL;
	while ((lit = lookup(le, sym, false)) != NULL) {
		if (lit->hdr.t == VSYM) {
			sym = lit->sym.v;
			continue;
		}
		break;
	}
	if (lit == NULL) {
		printf("? %s: 'it' from '%s' undefined (function without body?)\n",
				__FUNCTION__, f->symf.name);
		free_env(le, false);
		return (Ir) {FATAL, NULL};
	}
	upd_prefix1(s, p, copy_v(lit));
	free_env(le, false);
	return (Ir) {OK, s};
}
static Ir 
reduce_return(Env *e, Val *s, size_t p) {
	if (!(p == 0 && s->seq.v.n == 1)) {
		printf("? %s: 'return' syntax incorrect\n", __FUNCTION__);
		return (Ir) {FATAL, NULL};
	}
	/* return only from a function call */
	if (e->parent == NULL) {
		printf("? %s: 'return' outside function\n", __FUNCTION__);
		return (Ir) {FATAL, NULL};
	}
	Val *it = lookup(e, IT, false);
	if (it == NULL) {
		printf("? %s: 'it' undefined\n", __FUNCTION__);
		return (Ir) {FATAL, NULL};
	}
	upd_prefix0(s, p, copy_v(it));
	return (Ir) {RETURN, s};
}
static Ir 
reduce_env(Env *e, Val *s, size_t p) {
	if (!(p == 0 && s->seq.v.n == 1)) {
		printf("? %s: `env syntax incorrect\n", __FUNCTION__);
		return (Ir) {FATAL, NULL};
	}
	print_env(e, ">");
	Val *it = lookup(e, IT, false);
	if (it == NULL) {
		printf("? %s: 'it undefined\n", __FUNCTION__);
		return (Ir) {FATAL, NULL};
	}
	upd_prefix0(s, p, copy_v(it));
	return (Ir) {OK, s};
}

/* --------------- builtin or base function symbols -------------------- */

/* user defined fun priority */
#define DEFPRIO 0

typedef struct Symop_ {
	char name[WSZ];
	int prio;
	Ir (*f)(Env *e, Val *s, size_t p);
	int arity;
} Symop;

#define NSYMS 29
Symop Syms[] = {
	(Symop) {"rem:",   -20, reduce_rem,  -1}, /* -1 arity: remainder of seq val */
	(Symop) {"true?",  -20, reduce_true,  0},
	(Symop) {"false?", -20, reduce_false, 0},
	(Symop) {"list",   -20, reduce_list, -1},
	(Symop) {"solve",  -20, reduce_solve,  1},
	(Symop) {"do",     -20, reduce_do,    1},
	(Symop) {"call",   -20, reduce_call,  2},
	(Symop) {"define", -20, reduce_def,  2},
	(Symop) {"def",    -20, reduce_def,  2},
	(Symop) {"return", -20, reduce_return, 0},
	(Symop) {"end",    -20, reduce_end,  1}, /* needs to be prior to user def */
	(Symop) {"else",   -20, reduce_else, 0},
	(Symop) {"print",  -20, reduce_print, 1},
	(Symop) {"env",    -20, reduce_env, 0},
	(Symop) {"=",      -10, reduce_eq,  2},
	(Symop) {"/=",     -10, reduce_neq, 2},
	/* priority DEFPRIO (0) is for function (user defined) */
	(Symop) {"*",    20, reduce_mul, 2},
	(Symop) {"/",    20, reduce_div, 2},
	(Symop) {"+",    30, reduce_plu, 2},
	(Symop) {"-",    30, reduce_min, 2},
	(Symop) {"<",    40, reduce_les, 2},
	(Symop) {"<=",   40, reduce_leq, 2},
	(Symop) {">",    40, reduce_gre, 2},
	(Symop) {">=",   40, reduce_geq, 2},
	(Symop) {"~=",   40, reduce_eqv, 2},
	(Symop) {"not",  50, reduce_not, 1},
	(Symop) {"and",  60, reduce_and, 2},
	(Symop) {"or",   60, reduce_or,  2},
	(Symop) {"if",  100, reduce_if,  1},
};
static int
minprio() {
	int m = 0;
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
			|| a->hdr.t == VSYMF
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
			free_v(d);
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
			free_v(d);
		}
		return a;
	}
	printf("? %s: unknown seme\n",
			__FUNCTION__);
	return NULL;
}

/* ----- evaluation, pass 2, symbolic computation ----- */

static Ir 
solve_if_fun(Env *e, Val *a) {
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
				c->hdr.t = VSYMOP;
				c->symop.prio = so->prio;
				c->symop.v = so->f;
				c->symop.arity = so->arity;
				strncpy(c->symop.name, so->name, 1+strlen(so->name));
				free_v(b);
				a->seq.v.v[i] = c;
				continue;
			}
			if (strncmp(b->sym.v, IT, 3) == 0) {
				/* 'it is resolved at exec */
				continue;
			}
			c = lookup(e, b->sym.v, false);
			if (c != NULL && (c->hdr.t == VSYMF
					|| c->hdr.t == VSYMOP)) {
				free_v(b);
				a->seq.v.v[i] = copy_v(c);
				continue;
			}
		}
	}
	return (Ir) {OK, a};
}

static Ir 
exec_seq(Env *e, Val *b, bool look) {
	/* symbol application: consumes the seq, until 1 item left */
	assert(b != NULL);
	if (Dbg) { printf("##  %s (%d) entry: ", __FUNCTION__, look); print_v(b); printf("\n"); }
	/* resolve symbols to operators, functions */
	Ir rc = solve_if_fun(e, b);
	if (rc.state == FATAL) {
		return rc;
	}
	if (Dbg) { printf("##  %s resolved: ", __FUNCTION__); print_v(b); printf("\n"); }
	rc = (Ir) {UNSET, NULL};
	Val *c;
	while (b->seq.v.n > 0) {
		/* stop condition: seq reduces to single element */
		if (b->seq.v.n == 1) {
			c = b->seq.v.v[0];
			/* loop end, return the reduced-to value, 
			 * unless it's an operator of 0 arity,
			 * then, fall through & execute it below */
			/* (functions all have one parameter) */
			if (!(c->hdr.t == VSYMOP && c->symop.arity == 0)) {
				if (rc.state == UNSET) {
					rc.state = e->state;
				}
				rc.v = copy_v(c);
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
			if (c->hdr.t == VSYMF) {
				if (DEFPRIO < hiprio) {
					hiprio = DEFPRIO;
					symat = i;
					symfound = true;
					symtype = VSYMF;
				}
			} else if (c->hdr.t == VSYMOP) {
				if (c->symop.prio < hiprio) {
					hiprio = c->symop.prio;
					symat = i;
					symfound = true;
					symtype = VSYMOP;
				}
			} 
		}
		if (!symfound) { 
			printf("? %s: sequence without function ",__FUNCTION__);
			print_v(b);
			printf("\n");
			return (Ir) {FATAL, NULL};
		}
		assert(symtype == VSYMF || symtype == VSYMOP);
		/* apply the symbol, returns the reduced seq */
		if (symtype == VSYMF) {
			/* user defined function */
			rc = reduce_fun(e, b, symat);
		} else if (symtype == VSYMOP) {
			/* builtin operator */
			rc = b->seq.v.v[symat]->symop.v(e, b, symat);
		}
		if (rc.state == FATAL || rc.state == BACKTRACK) {
			assert(rc.v == NULL);
			return rc;
		}
		if (Dbg) { printf("##  %s reduced: ", __FUNCTION__); print_v(b); printf("\n"); }
		b = rc.v;
		/* rc.state set by the reduce_*() */
	}
	/* empty seq after reduction? */
	printf("? %s: sequence empty\n",__FUNCTION__);
	return (Ir) {FATAL, NULL};
}
static Ir 
interp_seq(Env *e, Val *a, bool look) {
	Ir rc;
	Val *b = NULL;
	for (size_t i=0; i < a->seq.v.n; ++i) {
		rc = interp_now(e, a->seq.v.v[i], look);
		if (rc.state != OK) {
			/* other states have no meaning inside a seq */
			free_v(b);
			free_v(rc.v);
			return (Ir) {FATAL, NULL};
		}
		b = push_v(VSEQ, b, rc.v);
		free_v(rc.v);
	}
	if (b == NULL) {
		b = malloc(sizeof(*b));
		assert( b != NULL);
		b->hdr.t = VNIL;
		return (Ir) {OK, b};
	}
	rc = exec_seq(e, b, look);
	if (Dbg) { printf("##  %s exit: ", __FUNCTION__); print_v(rc.v); printf("\n"); }
	free_v(b);
	return rc;
}
static Ir 
interp_atom(Env *e, Val *a, bool look) {
	if (a->hdr.t == VSYM) {
		/* resolve operators */
		Symop *so = lookup_op(a->sym.v);
		if (so != NULL) {
			Val *b = malloc(sizeof(*b));
			b->hdr.t = VSYMOP;
			b->symop.prio = so->prio;
			b->symop.v = so->f;
			b->symop.arity = so->arity;
			strncpy(b->symop.name, so->name, 1+strlen(so->name));
			return (Ir) {OK, b};
		}
		/* resolve 'it */
		if (strncmp(a->sym.v, IT, 3) == 0) {
			Val *b = lookup(e, IT, false);
			if (b == NULL) {
				printf("? %s: 'it undefined\n",
					__FUNCTION__);
				return (Ir) {FATAL, NULL};
			} 
			return (Ir) {OK, copy_v(b)};
		}
		/* resolve according to the look directive */
		if (look) {
			Val *b = lookup(e, a->sym.v, true);
			if (b == NULL) {
				printf("? %s: unknown symbol '%s'\n",
					__FUNCTION__, a->sym.v);
				return (Ir) {FATAL, NULL};
			} 
			return (Ir) {OK, copy_v(b)};
		}
	}
	return (Ir) {OK, copy_v(a)};
}
static Ir 
interp_lst(Env *e, Val *a, bool look) {
	Ir rc;
	Val *b = NULL;
	for (size_t i=0; i < a->lst.v.n; ++i) {
		rc = interp_now(e, a->lst.v.v[i], look);
		if (rc.state != OK) {
			/* other states have no meaning inside a list */
			free_v(b);
			free_v(rc.v);
			return (Ir) {FATAL, NULL};
		}
		b = push_v(VLST, b, rc.v);
		free_v(rc.v);
	}
	if (Dbg) { printf("##  %s exit: ", __FUNCTION__); print_v(b); printf("\n"); }
	return (Ir) {OK, b};
}

static Ir 
interp_now(Env *e, Val *a, bool look) {
	/* returns a fresh value, 'a untouched */
	if (isatom_v(a)) {
		return interp_atom(e, a, look);
	}
	if (a->hdr.t == VLST) {
		return interp_lst(e, a, look);
	}
	if (a->hdr.t == VSEQ) {
		return interp_seq(e, a, look);
	}
	printf("? %s: unknown value\n", __FUNCTION__);
	return (Ir) {FATAL, NULL};
}

static Ir
interp_body(Env *e, Val *s, size_t p) {
	Val *fun = lookup(e, IT, false);
	if (fun == NULL) {
		printf("? %s: 'it' required, yet undefined\n", __FUNCTION__);
		return (Ir) {FATAL, NULL};
	}
	if (fun->hdr.t != VSYMF) {
		printf("? %s: 'it' is not a function\n", __FUNCTION__);
		return (Ir) {FATAL, NULL};
	}
	/* closure: replace free symbols with env value */
	for (size_t i=0; i<s->seq.v.n; ++i) {
		Val *c = s->seq.v.v[i];
		/* we're only concerned with symbols */
		if (c->hdr.t != VSYM) {
			continue;
		}
		/* 'it resolved later */
		if (strncmp(c->sym.v, IT, 3) == 0) {
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
		Val *fv = lookup(e, c->sym.v, true);
		/* unknown sym, can be normal, determined in function or operator */
		if (fv == NULL) {
			continue;
		}
		/* closure: replace sym with value from env */
		free_v(c);
		s->seq.v.v[i] = copy_v(fv);
	}
	fun->symf.body = push_l(fun->symf.body, s);
	return (Ir) {DEF, copy_v(fun)};
}

static Ir
interp_maybe_later(Env *e, Val *a) {
	/* returns a fresh value, 'a untouched */
	if (Dbg) { printf("##  %s entry: ", __FUNCTION__); print_v(a); printf("\n"); }
	Val *b = copy_v(a);
	/* resolve symbols (not 'it) to operators and functions */
	Ir rc = solve_if_fun(e, b);
	if (rc.state == FATAL) {
		free_v(b);
		return rc;
	}
	if (Dbg) { printf("##  %s resolved: ", __FUNCTION__); print_v(b); printf("\n"); }
	/* rem: expecting end 'foo */
	if (b->seq.v.v[0]->hdr.t == VSYMOP 
			&& b->seq.v.v[0]->symop.v == reduce_end
			&& b->seq.v.v[0]->symop.arity == b->seq.v.n -1
			&& b->seq.v.v[1]->hdr.t == VSYM) {
		Ir rc = interp_now(e, b, false);
		/* not a valid end expression after all */
		if (rc.state == BACKTRACK) {
			rc = interp_body(e, b, 0);
			free_v(b);
			return rc;
		}
		free_v(b);
		return rc;
	}
	rc = interp_body(e, b, 0);
	free_v(b);
	return rc;
}

static Ir
interp_maybe_skip(Env *e, Val *a) {
	/* returns a fresh value, 'a untouched */
	if (Dbg) { printf("##  %s entry: ", __FUNCTION__); print_v(a); printf("\n"); }
	Val *b = copy_v(a);
	/* resolve symbols to operators, functions */
	Ir rc = solve_if_fun(e, b);
	if (rc.state == FATAL) {
		free_v(b);
		return rc;
	}
	if (Dbg) { printf("##  %s resolved: ", __FUNCTION__); print_v(b); printf("\n"); }
	bool an_endif = b->seq.v.v[0]->hdr.t == VSYMOP 
			&& b->seq.v.v[0]->symop.v == reduce_end
			&& b->seq.v.v[0]->symop.arity == b->seq.v.n -1
			&& b->seq.v.v[1]->hdr.t == VSYMOP 
			&& b->seq.v.v[1]->symop.v == reduce_if ;
	bool an_else = b->seq.v.v[0]->hdr.t == VSYMOP 
			&& b->seq.v.v[0]->symop.v == reduce_else ;
	if (an_endif || an_else) {
		Ir rc = interp_now(e, b, false);
		free_v(b);
		return rc;
	}
	/* default: skip val */
	free_v(b);
	Val *it = lookup(e, IT, false);
	if (it == NULL) {
		printf("? %s: 'it' undefined\n", __FUNCTION__);
		return (Ir) {FATAL, NULL};
	}
	return (Ir) {SKIP, copy_v(it)};
}

static Ir 
interp(Env *e, Val *a) {
	/* returns a fresh value, 'a untouched */
	assert(e != NULL && "env is null");
	assert(a != NULL && "value is null");
	switch (e->state) {
		case DEF:
			return interp_maybe_later(e, a);
		case SKIP:
			return interp_maybe_skip(e, a);
		case OK:
			return interp_now(e, a, false);
		default:
			printf("? %s: unexpected state\n", __FUNCTION__);
			return (Ir) {FATAL, NULL};
	}
	return (Ir) {FATAL, NULL};
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
interp_ph(Env *env, Phrase *a) {
	assert(env != NULL && "environment null");
	assert(a != NULL && "phrase null");
	for (size_t i=0; i<a->n; ++i) {
		if (Dbg) { printf("# %3lu %6s: %s\n", i, "expr", a->x[i]); }
		Expr *ex = exp_of_words(a->x[i]);
		if (ex == NULL) {
			return false;
		}
		Sem *sm = seme_of_exp(ex);
		free_x(ex);
		if (sm == NULL) {
			return false;
		}
		if (Dbg) { printf("# %3lu %6s: ", i, "seme"); print_s(sm); printf("\n"); }
		Val *v = val_of_seme(env, sm);
		free_s(sm);
		free(sm);
		if (v == NULL) {
			return false;
		}
		if (Dbg) { printf("# %3lu %6s: ", i, "value"); print_v(v); printf("\n"); }
		Ir xs = interp(env, v);
		free_v(v);
		if (Dbg) { printf("# %3lu %6s: ", i, "reduce"); print_rc(xs); printf("\n"); }
		if (xs.state == FATAL) {
			return false;
		}
		env->state = xs.state;
		Symval *it = symval(IT, xs.v);
		free_v(xs.v);
		if (it == NULL) {
			return false;
		}
		xs.v = NULL;
		if (!stored_sym(env, it)) {
			free_symval(it);
			return false;
		}
		if (Dbg) { printf("# %3lu env:\n", i); print_env(env, "#"); }
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
	Env *e = malloc(sizeof(*e));
	assert(e != NULL);
	e->state = OK;
	e->n = 0;
	e->s = NULL;
	e->parent = NULL;
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
				if (e->state != OK) {
					printf("? %s: unexpected end of program\n",
							__FUNCTION__);
				}
				printf("> exit\n"); print_env(e, ">");
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
		bool r = interp_ph(e, ph);
		free_ph(ph);
		if (!r) {
			free_env(e, true);
			return EXIT_FAILURE;
		}
	}
	free_env(e, true);
	return EXIT_SUCCESS;
}
