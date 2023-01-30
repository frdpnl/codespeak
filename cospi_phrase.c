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
 * warning, valgrind prefers dynamic libs.
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
	a->n = 0;
	a->w = NULL;
	return a;
}

static void
free_x(Expr *a) {
	if (a == NULL) {
		return;
	}
	free(a->w);
	free(a);
}

static Word *
word(wtype a) {
	Word *b = malloc(sizeof(*b));
	b->t = a;
	b->v[0] = '\0';
	return b;
}

static Word *
word_str(char *a, size_t n) {
	if (n >= WSZ) {
		printf("? %s:%d word to big\n", 
				__FUNCTION__, __LINE__);
		return NULL;
	}
	Word *b = word(STR);
	memcpy(b->v, a, n*sizeof(char));
	b->v[n] = '\0';
	return b;
}

static Expr *
push_xz(Expr *a, Word *b) {
	if (a == NULL) {
		printf("? %s:%d list is null\n", 
				__FUNCTION__, __LINE__);
		return NULL;
	}
	Word *c = malloc((a->n+1)*sizeof(Word));
	memcpy(c+a->n, b, sizeof(Word));
	if (a->n) {
		memcpy(c, a->w, a->n*sizeof(Word));
		free(a->w);
	}
	a->w = c;
	a->n++;
	free(b);
	return a;
}

static void
print_w(Word *a) {
	if (a == NULL) {
		printf("Null");
		return;
	}
	char *typtxt[] = { "Sep", "Left", "Right", "Str"};
	switch (a->t) {
		case STR:
			printf("%s[%s]", typtxt[a->t], a->v);
			break;
		case SEP:
		case LEFT: 
		case RIGHT:
			printf("%s", typtxt[a->t]);
			break;
		default:
			printf("? %s:%d unknown word type ", 
					__FUNCTION__, __LINE__);
	}
}

static void
print_x(Expr *a) {
	if (a == NULL) {
		printf("? %s:%d list of words is empty\n",
				__FUNCTION__,__LINE__);
		return;
	}
	printf("(");
	for (size_t i=0; i<a->n; ++i) {
		print_w(a->w+i);
		printf(" ");
	}
	printf(")");
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
			printf("\n? %s:%d word too big (%luB)!\n", 
					__FUNCTION__,
					__LINE__,
					boff+read);
			free_x(b);
			return NULL;
		}
		/* default case: add character to current word */
		memcpy(buf+boff, a+off, read*sizeof(char));
		boff += read;
		buf[boff] = '\0';
	}
	printf("\n");
	if (boff) {
		w = word_str(buf, boff);
		b = push_xz(b, w);
	}
	return b;
}

/* ----- identify words, as semes, 1st level semes ----- */

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
	if (a == NULL) {
		printf("? %s:%d null seme\n",
				__FUNCTION__,__LINE__);
		return;
	}
	switch (a->hdr.t) {
		case SNIL:
			printf("Nil ");
			break;
		case SNAT:
			printf("n%lld ", a->nat.v);
			break;
		case SREA:
			printf("r%.2lf ", a->rea.v);
			break;
		case SSYM:
			printf("'%s ", a->sym.v);
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
			printf("? %s:%d unknown seme\n",
					__FUNCTION__, __LINE__);
	}
}

static Sem *
sem_nil() {
	Sem *b = malloc(sizeof(*b));
	b->hdr.t = SNIL;
	return b;
}

static Sem *
sem_nat(long long a) {
	Sem *b = malloc(sizeof(*b));
	b->nat.t = SNAT;
	b->nat.v = a;
	return b;
}

static Sem *
sem_rea(double a) {
	Sem *b = malloc(sizeof(*b));
	b->rea.t = SREA;
	b->rea.v = a;
	return b;
}

static Sem *
sem_sym(char *a) {
	Sem *b = malloc(sizeof(*b));
	b->sym.t = SSYM;
	strncpy(b->sym.v, a, WSZ);
	b->sym.v[WSZ-1] = '\0';
	return b;
}

static Sem *
sem_lst() {
	Sem *b = malloc(sizeof(*b));
	b->lst.t = SLST;
	b->lst.v.n = 0;
	b->lst.v.s = NULL;
	return b;
}

static Sem *
sem_seq() {
	Sem *b = malloc(sizeof(*b));
	b->lst.t = SSEQ;
	b->lst.v.n = 0;
	b->lst.v.s = NULL;
	return b;
}

static void 
free_s(Sem *a) {
	if (a == NULL) {
		printf("? %s:%d null seme\n",
				__FUNCTION__, __LINE__);
		return;
	}
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
			printf("? %s:%d unknown seme\n",
					__FUNCTION__,__LINE__);
	}
}

static Sem* 
isnat(Word *a) {
	if (a->t != STR) {
		printf("? %s:%d word is not a string\n",
				__FUNCTION__, __LINE__);
		return NULL;
	}
	const char *err;
	errno = 0;
	long long n = strtonum(a->v, LLONG_MIN, LLONG_MAX, &err);
	if (err == NULL) {
		return sem_nat(n);
	}
	if (errno == ERANGE) {
		printf("? %s:%d natural number out of range %s\n", 
				__FUNCTION__, __LINE__, a->v);
	} 
	return NULL;
}

static Sem *
isrea(Word *a) {
	if (a->t != STR) {
		printf("? %s:%d word is not a string\n",
				__FUNCTION__, __LINE__);
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
			printf("? %s:%d real underflow %s\n", 
					__FUNCTION__, __LINE__, a->v);
		} else {
			printf("? %s:%d real overflow %s\n", 
					__FUNCTION__, __LINE__, a->v);
		}
	}
	return NULL;
}

static Sem *
issym(Word *a) {
	if (a->t != STR) {
		printf("? %s:%d word is not a string\n",
				__FUNCTION__, __LINE__);
		return NULL;
	}
	return sem_sym(a->v);
}

static Sem *
push_s(Sem *a, Sem *b) {
	if (a == NULL) {
		printf("? %s:%d seq or lst seme is null\n",
				__FUNCTION__,__LINE__);
		return NULL;
	}
	if (b == NULL) {
		printf("? %s:%d pushed seme is null\n",
				__FUNCTION__,__LINE__);
		return NULL;
	}
	if (a->hdr.t != SSEQ && a->hdr.t != SLST) {
		printf("? %s:%d not a seq or lst seme\n",
				__FUNCTION__,__LINE__);
		return NULL;
	}
	Sem *c = malloc((a->seq.v.n +1)*sizeof(Sem));
	memcpy(c, a->seq.v.s, a->seq.v.n * sizeof(Sem));
	memcpy(c + a->seq.v.n, b, sizeof(Sem));
	++(a->seq.v.n);
	free(a->seq.v.s);
	a->seq.v.s = c;
	return a;
}

static Sem *
lst_of(Sem *a) {
	if (a == NULL) {
		printf("? %s:%d seme is null\n",
				__FUNCTION__,__LINE__);
		return NULL;
	}
	if (a->hdr.t != SSEQ && a->hdr.t != SLST) {
		printf("? %s:%d not a seq or lst seme\n",
				__FUNCTION__,__LINE__);
		free_s(a);
		free(a);
		return NULL;
	}
	if (a->hdr.t == SSEQ && a->seq.v.n > 1) {
		printf("? %s:%d cannot add a list element to a seq-seme\n",
				__FUNCTION__,__LINE__);
		free_s(a);
		free(a);
		return NULL;
	}
	a->hdr.t = SLST;
	return a;
}

static Sem *
read_seme(Expr *a, size_t from, size_t tox) {
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
					printf("? %s:%d unexpected list element\n",
							__FUNCTION__, __LINE__);
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
							c = read_seme(a, iw+1, ip);
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
					printf("? %s:%d unmatched %s\n",
							__FUNCTION__,__LINE__,
							inpar < 0 ? ")" : "(");
					free_s(b);
					free(b);
					return NULL;
				}
				break;
			case RIGHT:
				printf("? %s:%d unmatched )\n",
						__FUNCTION__,__LINE__);
				free_s(b);
				free(b);
				return NULL;
			case STR:
				if (b->hdr.t == SLST && !lst_expect1) {
					printf("? %s:%d unexpected list element\n",
							__FUNCTION__, __LINE__);
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
					printf("? %s:%d unknown word\n",
							__FUNCTION__,__LINE__);
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
				printf("? %s:%d unexpected word\n",
						__FUNCTION__,__LINE__);
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
	if (a == NULL) {
		printf("? %s:%d null list of words\n",
				__FUNCTION__,__LINE__);
		return NULL;
	}
	if (a->n == 0) {
		return sem_nil();
	} 
	return read_seme(a, 0, a->n);
}

/* ----- evaluation, pass 1 ----- */

typedef enum {VNIL, VNAT, VREA, VSYMOP, VLST, VSEQ} vtype;

typedef union Val_ Val;

typedef struct List_v_ {
	size_t n;
	Val **v;
} List_v;

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
		Val* (*v)(Val *s, size_t p);
	} symop;
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
		printf("? %s:%d null value\n",
				__FUNCTION__,__LINE__);
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
			printf("'%p(%d) ", a->symop.v, a->symop.prio);
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
			printf("? %s:%d unknown value\n",
					__FUNCTION__, __LINE__);
	}
}

static void 
free_v(Val *a) {
	if (a == NULL) {
		printf("? %s:%d null value\n",
				__FUNCTION__, __LINE__);
		return;
	}
	switch (a->hdr.t) {
		case VNIL:
		case VNAT:
		case VREA:
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
			printf("? %s:%d unknown value\n",
					__FUNCTION__,__LINE__);
	}
	free(a);
}

static bool
isequal_v(Val *a, Val *b) {
	if (a == NULL || b == NULL) {
		printf("? %s:%d value is null\n",
				__FUNCTION__,__LINE__);
		return false;
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
	if (a->hdr.t == VLST) {
		for (size_t i=0; i<a->lst.v.n; ++i) { 
			bool c = isequal_v(a->lst.v.v[i], b->lst.v.v[i]);
			if (!c) {
				return c;
			}
		}
		return true;
	}
	printf("? %s:%d unsupported value\n",
			__FUNCTION__,__LINE__);
	return false;
}

static bool
isequiv_v(Val *a, Val *b) {
	if (a == NULL || b == NULL) {
		printf("? %s:%d value is null\n",
				__FUNCTION__,__LINE__);
		return false;
	}
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
	if (a->hdr.t == VLST) {
		for (size_t i=0; i<a->lst.v.n; ++i) { 
			bool c = isequiv_v(a->lst.v.v[i], b->lst.v.v[i]);
			if (!c) {
				return c;
			}
		}
		return true;
	}
	printf("? %s:%d unsupported value\n",
			__FUNCTION__,__LINE__);
	return false;
}
static Val *
copy_v(Val *a) {
	if (a == NULL) {
		printf("? %s:%d value is null\n",
				__FUNCTION__,__LINE__);
		return NULL;
	}
	Val *b = malloc(sizeof(Val));
	memcpy(b, a, sizeof(Val));
	if (a->hdr.t == VSEQ || a->hdr.t == VLST) {
		b->seq.v.v = malloc(a->seq.v.n * sizeof(Val*));
		for (size_t i=0; i<a->seq.v.n; ++i) {
			b->seq.v.v[i] = copy_v(a->seq.v.v[i]);
		}
	}
	return b;
}
static Val *
push_v(Val *a, Val *b) {
	if (a == NULL) {
		printf("? %s:%d seq or list is null\n",
				__FUNCTION__,__LINE__);
		return NULL;
	}
	if (b == NULL) {
		printf("? %s:%d pushed value is null\n",
				__FUNCTION__,__LINE__);
		return NULL;
	}
	if (a->hdr.t != VSEQ && a->hdr.t != VLST) {
		printf("? %s:%d not a seq or list val\n",
				__FUNCTION__,__LINE__);
		return NULL;
	}
	Val **c = malloc((a->seq.v.n +1)*sizeof(Val*));
	if (a->seq.v.n > 0) {
		memcpy(c, a->seq.v.v, a->seq.v.n * sizeof(Val*));
	}
	c[a->seq.v.n] = b;
	if (a->seq.v.n > 0) {
		free(a->seq.v.v);
	}
	++(a->seq.v.n);
	a->seq.v.v = c;
	return a;
}

/* -------- operation symbol definitions ------------
 * recommended interface (resource management):
 * 1. work off copies of arguments
 * 2. produce a fresh value (can be one of the copies)
 * 3. replace symbol operation expression with that fresh value
 * (delete previous values in the expression, add new one)
 * 4. cleanup behind you (working copies)
 * So, an optimization could be to reuse one existing argument for result
 * and avoid alloc/free of original argument. 
 * But, loss of flexibility isn't worth it (for compiler).
 * */

static Val * eval(Val *a);

static bool 
infixed(size_t p, size_t n) {
	return (p > 0 && p < n-1);
}
static bool 
prefixed1(size_t p, size_t n) {
	return (p >= 0 && p < n-1);
}

static bool
get_eval_infix_arg(Val *s, size_t p, Val **pa, Val **pb) {
	Val *a, *b;
	if (!infixed(p, s->seq.v.n)) {
		printf("? %s:%d symbol not infixed\n", 
				__FUNCTION__,__LINE__);
		return false;
	}
	a = eval(s->seq.v.v[p-1]); 
	/* this is already evaluated by eval(), so will just copy_v().
	 * right now, keep eval() instead of a direct copy_v(), 
	 * in order to support later flexibility */
	if (a == NULL) {
		printf("? %s:%d 1st argument null\n", 
				__FUNCTION__,__LINE__);
		return false;
	}
	b = eval(s->seq.v.v[p+1]);
	if (b == NULL) {
		printf("? %s:%d 2nd argument null\n", 
				__FUNCTION__,__LINE__);
		free_v(a);
		return false;
	}
	*pa = a;
	*pb = b;
	return true;
}
static bool
copy_infix_arg(Val *s, size_t p, Val **pa, Val **pb) {
	Val *a, *b;
	if (!infixed(p, s->seq.v.n)) {
		printf("? %s:%d symbol not infixed\n", 
				__FUNCTION__,__LINE__);
		return false;
	}
	a = copy_v(s->seq.v.v[p-1]); 
	if (a == NULL) {
		printf("? %s:%d 1st argument null\n", 
				__FUNCTION__,__LINE__);
		return false;
	}
	b = copy_v(s->seq.v.v[p+1]);
	if (b == NULL) {
		printf("? %s:%d 2nd argument null\n", 
				__FUNCTION__,__LINE__);
		free_v(a);
		return false;
	}
	*pa = a;
	*pb = b;
	return true;
}
static bool
get_eval_prefix1_arg(Val *s, size_t p, Val **pa) {
	Val *a;
	if (!prefixed1(p, s->seq.v.n)) {
		printf("? %s:%d symbol not prefixed to one argument\n", 
				__FUNCTION__,__LINE__);
		return false;
	}
	a = eval(s->seq.v.v[p+1]);
	if (a == NULL) {
		printf("? %s:%d argument null\n", 
				__FUNCTION__,__LINE__);
		return false;
	}
	*pa = a;
	return true;
}
static bool
copy_prefix1_arg(Val *s, size_t p, Val **pa) {
	Val *a;
	if (!prefixed1(p, s->seq.v.n)) {
		printf("? %s:%d symbol not prefixed to one argument\n", 
				__FUNCTION__,__LINE__);
		return false;
	}
	a = copy_v(s->seq.v.v[p+1]);
	if (a == NULL) {
		printf("? %s:%d argument null\n", 
				__FUNCTION__,__LINE__);
		return false;
	}
	*pa = a;
	return true;
}
static bool
copy_prefixn_arg(Val *s, size_t p, Val **pa) {
	Val *a;
	a = malloc(sizeof(*a));
	a->hdr.t = VSEQ;  /* because we copy arguments, we keep the seq type */
	a->seq.v.n = s->seq.v.n -p-1; 
	a->seq.v.v = malloc((a->seq.v.n) * sizeof(Val*));
	for (size_t i=p+1; i < s->seq.v.n; ++i) {
		a->seq.v.v[i-p-1] = copy_v(s->seq.v.v[i]);
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
upd_prefix1(Val *s, size_t p, Val *a) {
	/* consumed 1 seq item */
	for (size_t i=p; i < s->seq.v.n && i < p+2; ++i) {
		free_v(s->seq.v.v[i]);
	}
	s->seq.v.v[p] = a;
	for (size_t i=p+2; i < s->seq.v.n; ++i) {
		s->seq.v.v[i-1] = s->seq.v.v[i];
	}
	s->seq.v.n -= 1;
}
static void
upd_prefixn(Val *s, size_t p, Val *a) {
	/* consumed n-p seq item */
	for (size_t i=p; i < s->seq.v.n; ++i) {
		free_v(s->seq.v.v[i]);
	}
	s->seq.v.v[p] = a;
	s->seq.v.n = p+1;
}

static Val *
eval_mul(Val *s, size_t p) {
	Val *a, *b;
	if (!copy_infix_arg(s, p, &a, &b)) {
		printf("? %s:%d infix expression invalid\n", 
				__FUNCTION__,__LINE__);
		return NULL;
	}
	if ((a->hdr.t != VNAT && a->hdr.t != VREA)
			|| (b->hdr.t != VNAT && b->hdr.t != VREA)) {
		printf("? %s:%d arguments not numbers\n", 
				__FUNCTION__,__LINE__);
		free_v(a);
		free_v(b);
		return NULL;
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
	return s;
}
static Val *
eval_div(Val *s, size_t p) {
	Val *a, *b;
	if (!copy_infix_arg(s, p, &a, &b)) {
		printf("? %s:%d infix expression invalid\n", 
				__FUNCTION__,__LINE__);
		return NULL;
	}
	if ((a->hdr.t != VNAT && a->hdr.t != VREA)
			|| (b->hdr.t != VNAT && b->hdr.t != VREA)) {
		printf("? %s:%d arguments not numbers\n", 
				__FUNCTION__,__LINE__);
		free_v(a);
		free_v(b);
		return NULL;
	}
	if ((b->hdr.t == VNAT && b->nat.v == 0) 
			|| (b->hdr.t == VREA && b->rea.v == 0.)) {
		printf("? %s:%d division by 0\n", 
				__FUNCTION__,__LINE__);
		free_v(a);
		free_v(b);
		return NULL;
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
	return s;
}
static Val *
eval_plu(Val *s, size_t p) {
	Val *a, *b;
	if (!copy_infix_arg(s, p, &a, &b)) {
		printf("? %s:%d infix expression invalid\n", 
				__FUNCTION__,__LINE__);
		return NULL;
	}
	if ((a->hdr.t != VNAT && a->hdr.t != VREA)
			|| (b->hdr.t != VNAT && b->hdr.t != VREA)) {
		printf("? %s:%d arguments not numbers\n", 
				__FUNCTION__,__LINE__);
		free_v(a);
		free_v(b);
		return NULL;
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
	return s;
}
static Val *
eval_min(Val *s, size_t p) {
	Val *a, *b;
	if (!copy_infix_arg(s, p, &a, &b)) {
		printf("? %s:%d infix expression invalid\n", 
				__FUNCTION__,__LINE__);
		return NULL;
	}
	if ((a->hdr.t != VNAT && a->hdr.t != VREA)
			|| (b->hdr.t != VNAT && b->hdr.t != VREA)) {
		printf("? %s:%d arguments not numbers\n", 
				__FUNCTION__,__LINE__);
		free_v(a);
		free_v(b);
		return NULL;
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
	return s;
}
static Val *
eval_les(Val *s, size_t p) {
	Val *a, *b;
	if (!copy_infix_arg(s, p, &a, &b)) {
		printf("? %s:%d infix expression invalid\n", 
				__FUNCTION__,__LINE__);
		return NULL;
	}
	/* to be expanded to other types */
	if ((a->hdr.t != VNAT && a->hdr.t != VREA)
			|| (b->hdr.t != VNAT && b->hdr.t != VREA)) {
		printf("? %s:%d arguments not numbers\n", 
				__FUNCTION__,__LINE__);
		free_v(a);
		free_v(b);
		return NULL;
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
	return s;
}
static Val *
eval_leq(Val *s, size_t p) {
	Val *a, *b;
	if (!copy_infix_arg(s, p, &a, &b)) {
		printf("? %s:%d infix expression invalid\n", 
				__FUNCTION__,__LINE__);
		return NULL;
	}
	/* to be expanded to other types */
	if ((a->hdr.t != VNAT && a->hdr.t != VREA)
			|| (b->hdr.t != VNAT && b->hdr.t != VREA)) {
		printf("? %s:%d arguments not numbers\n", 
				__FUNCTION__,__LINE__);
		free_v(a);
		free_v(b);
		return NULL;
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
	return s;
}
static Val *
eval_gre(Val *s, size_t p) {
	Val *a, *b;
	if (!copy_infix_arg(s, p, &a, &b)) {
		printf("? %s:%d infix expression invalid\n", 
				__FUNCTION__,__LINE__);
		return NULL;
	}
	/* to be expanded to other types */
	if ((a->hdr.t != VNAT && a->hdr.t != VREA)
			|| (b->hdr.t != VNAT && b->hdr.t != VREA)) {
		printf("? %s:%d arguments not numbers\n", 
				__FUNCTION__,__LINE__);
		free_v(a);
		free_v(b);
		return NULL;
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
	return s;
}
static Val *
eval_geq(Val *s, size_t p) {
	Val *a, *b;
	if (!copy_infix_arg(s, p, &a, &b)) {
		printf("? %s:%d infix expression invalid\n", 
				__FUNCTION__,__LINE__);
		return NULL;
	}
	/* to be expanded to other types */
	if ((a->hdr.t != VNAT && a->hdr.t != VREA)
			|| (b->hdr.t != VNAT && b->hdr.t != VREA)) {
		printf("? %s:%d arguments not numbers\n", 
				__FUNCTION__,__LINE__);
		free_v(a);
		free_v(b);
		return NULL;
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
	return s;
}
static Val *
eval_eq(Val *s, size_t p) {
	Val *a, *b;
	if (!copy_infix_arg(s, p, &a, &b)) {
		printf("? %s:%d infix expression invalid\n", 
				__FUNCTION__,__LINE__);
		return NULL;
	}
	bool c = isequal_v(a, b);
	free_v(a);
	free_v(b);
	Val *d = malloc(sizeof(Val));
	d->hdr.t = VNAT;
	d->nat.v = c ? 1 : 0;
	upd_infix(s, p, d);
	return s;
}
static Val *
eval_neq(Val *s, size_t p) {
	Val *a, *b;
	if (!copy_infix_arg(s, p, &a, &b)) {
		printf("? %s:%d infix expression invalid\n", 
				__FUNCTION__,__LINE__);
		return NULL;
	}
	bool c = isequal_v(a, b);
	free_v(a);
	free_v(b);
	Val *d = malloc(sizeof(Val));
	d->hdr.t = VNAT;
	d->nat.v = c ? 0 : 1;
	upd_infix(s, p, d);
	return s;
}
static Val *
eval_eqv(Val *s, size_t p) {
	Val *a, *b;
	if (!copy_infix_arg(s, p, &a, &b)) {
		printf("? %s:%d infix expression invalid\n", 
				__FUNCTION__,__LINE__);
		return NULL;
	}
	bool c = isequiv_v(a, b);
	free_v(a);
	free_v(b);
	Val *d = malloc(sizeof(Val));
	d->hdr.t = VNAT;
	d->nat.v = c ? 1 : 0;
	upd_infix(s, p, d);
	return s;
}
static Val *
eval_and(Val *s, size_t p) {
	Val *a, *b;
	if (!copy_infix_arg(s, p, &a, &b)) {
		printf("? %s:%d infix expression invalid\n", 
				__FUNCTION__,__LINE__);
		return NULL;
	}
	if ((a->hdr.t != VNAT) || (b->hdr.t != VNAT)) {
		printf("? %s:%d arguments not natural numbers\n", 
				__FUNCTION__,__LINE__);
		free_v(a);
		free_v(b);
		return NULL;
	}
	a->nat.v = (a->nat.v == 0 ? 0 : 1);
	b->nat.v = (b->nat.v == 0 ? 0 : 1);
	a->nat.v = a->nat.v && b->nat.v;
	free_v(b);
	upd_infix(s, p, a);
	return s;
}
static Val *
eval_or(Val *s, size_t p) {
	Val *a, *b;
	if (!copy_infix_arg(s, p, &a, &b)) {
		printf("? %s:%d infix expression invalid\n", 
				__FUNCTION__,__LINE__);
		return NULL;
	}
	if ((a->hdr.t != VNAT) || (b->hdr.t != VNAT)) {
		printf("? %s:%d arguments not natural numbers\n", 
				__FUNCTION__,__LINE__);
		free_v(a);
		free_v(b);
		return NULL;
	}
	a->nat.v = (a->nat.v == 0 ? 0 : 1);
	b->nat.v = (b->nat.v == 0 ? 0 : 1);
	a->nat.v = a->nat.v || b->nat.v;
	free_v(b);
	upd_infix(s, p, a);
	return s;
}
static Val *
eval_not(Val *s, size_t p) {
	Val *a;
	if (!copy_prefix1_arg(s, p, &a)) {
		printf("? %s:%d prefix expression invalid\n", 
				__FUNCTION__,__LINE__);
		return NULL;
	}
	if ((a->hdr.t != VNAT)) {
		printf("? %s:%d argument not natural number (boolean)\n", 
				__FUNCTION__,__LINE__);
		free_v(a);
		return NULL;
	}
	a->nat.v = (a->nat.v == 0 ? 1 : 0);
	upd_prefix1(s, p, a);
	return s;
}
static Val *
eval_id(Val *s, size_t p) {
	Val *a;
	if (!copy_prefix1_arg(s, p, &a)) {
		printf("? %s:%d prefix expression invalid\n", 
				__FUNCTION__,__LINE__);
		return NULL;
	}
	upd_prefix1(s, p, a);
	return s;
}

static Val * eval(Val *);

static Val *
eval_do(Val *s, size_t p) {
	Val *a;
	if (!copy_prefix1_arg(s, p, &a)) {
		printf("? %s:%d prefix expression invalid\n", 
				__FUNCTION__,__LINE__);
		return NULL;
	}
	if (a->hdr.t != VLST) {
		printf("? %s:%d argument not a list\n", 
				__FUNCTION__,__LINE__);
		free_v(a);
		return NULL;
	}
	a->hdr.t = VSEQ;
	Val *b = eval(a);
	free_v(a);
	if (b == NULL) {
		return NULL;
	}
	upd_prefix1(s, p, b);
	return s;
}

static Val *
eval_list(Val *s, size_t p) {
	Val *a;
	if (!copy_prefixn_arg(s, p, &a)) {
		printf("? %s:%d prefix expression invalid\n", 
				__FUNCTION__,__LINE__);
		return NULL;
	}
	a->hdr.t = VLST;
	upd_prefixn(s, p, a);
	return s;
}

/* --------------- builtin or base function symbols -------------------- */

typedef struct Symop_ {
	char name[WSZ];
	unsigned int prio;
	Val* (*f)(Val *s, size_t p);
} Symop;

#define NSYMS 17
Symop Syms[] = {
	(Symop) {"id",   10, eval_id}, /* technical symbol */
	(Symop) {"do",   10, eval_do},
	(Symop) {"list", 10, eval_list},
	(Symop) {"*",    20, eval_mul},
	(Symop) {"/",    20, eval_div},
	(Symop) {"+",    30, eval_plu},
	(Symop) {"-",    30, eval_min},
	(Symop) {"<",    40, eval_les},
	(Symop) {"<=",   40, eval_leq},
	(Symop) {">",    40, eval_gre},
	(Symop) {">=",   40, eval_geq},
	(Symop) {"=",    40, eval_eq},
	(Symop) {"/=",   40, eval_neq},
	(Symop) {"~=",   40, eval_eqv},
	(Symop) {"not",  50, eval_not},
	(Symop) {"and",  60, eval_and},
	(Symop) {"or",   60, eval_or},
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
get_symop(char *a) {
	for (size_t i=0; i<NSYMS; ++i) {
		Symop *b = Syms+i;
		if (strncmp(b->name, a, WSZ) == 0) {
			return b;
		}
	}
	return NULL;
}

/* --------------- user defined value symbols -------------------- */

typedef struct {
	char name[WSZ];
	Val *v;
} Symval;

typedef struct {
	size_t n;
	Symval **s;
} Env;

static void
print_symval(Symval *a) {
	if (a == NULL) {
		printf("? %s:%d symbol null\n",
				__FUNCTION__,__LINE__);
		return;
	}
	printf("'%s = ", a->name);
	print_v(a->v);
}
static void
print_env(Env *a) {
	if (a == NULL) {
		printf("? %s:%d environment null\n",
				__FUNCTION__,__LINE__);
		return;
	}
	for (size_t i=0; i<a->n; ++i) {
		print_symval(a->s[i]);
		printf(", ");
	}
	printf("\n");
}
static Symval *
symval(char *a, Val *b) {
	if (a == NULL || b == NULL) {
		printf("? %s:%d null values for symbol\n",
				__FUNCTION__,__LINE__);
		return NULL;
	}
	if (strlen(a) == 0) {
		printf("? %s:%d empty name\n",
				__FUNCTION__,__LINE__);
		return NULL;
	};
	if (strlen(a) >= WSZ) {
		printf("? %s:%d symbol name too long (%s)\n",
				__FUNCTION__,__LINE__, a);
		return NULL;
	};
	Symval *c = malloc(sizeof(*c));
	strncpy(c->name, a, WSZ*sizeof(char));
	c->v = copy_v(b);
	return c;
}
static void
free_symval(Symval *a) {
	if (a == NULL) {
		printf("? %s:%d null symbol\n",
				__FUNCTION__, __LINE__);
		return;
	}
	free_v(a->v);
	free(a);
}
static void 
free_env(Env *a) {
	if (a == NULL) {
		printf("? %s:%d null environment\n",
				__FUNCTION__, __LINE__);
		return;
	}
	for (size_t i=0; i<a->n; ++i) {
		free_symval(a->s[i]);
	}
	free(a->s);
	free(a);
}
static Symval *
get_symval_id(Env *a, char *b, size_t *id) {
	if (a == NULL) {
		printf("? %s:%d environment null\n",
				__FUNCTION__,__LINE__);
		return NULL;
	}
	if (b == NULL || strlen(b) == 0) {
		printf("? %s:%d symbol name null\n",
				__FUNCTION__,__LINE__);
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

static Symval *
get_symval(Env *a, char *b) {
	if (a == NULL) {
		printf("? %s:%d environment null\n",
				__FUNCTION__,__LINE__);
		return NULL;
	}
	if (b == NULL || strlen(b) == 0) {
		printf("? %s:%d symbol name null\n",
				__FUNCTION__,__LINE__);
		return NULL;
	}
	return get_symval_id(a, b, NULL);
}

static bool
isatom_v(Val *a) {
	if (a==NULL) {
		printf("? %s:%d null value\n",
				__FUNCTION__,__LINE__);
		return false;
	}
	return (a->hdr.t == VNIL 
			|| a->hdr.t == VNAT 
			|| a->hdr.t == VREA
			|| a->hdr.t == VSYMOP);
}

static Val *
val_of_seme(Env *a, Sem *b) {
	/* returns fresh value, a, b untouched */
	if (b == NULL) {
		printf("? %s:%d seme is null\n",
				__FUNCTION__,__LINE__);
		return NULL;
	}
	Val *c = NULL;
	if (b->hdr.t == SNIL ) {
		c = malloc(sizeof(*c));
		c->hdr.t = VNIL;
		return c;
	}
	if (b->hdr.t == SNAT) {
		c = malloc(sizeof(*c));
		c->hdr.t = VNAT;
		c->nat.v = b->nat.v;
		return c;
	}
	if (b->hdr.t == SREA) {
		c = malloc(sizeof(*c));
		c->hdr.t = VREA;
		c->rea.v = b->rea.v;
		return c;
	}
	if (b->hdr.t == SSYM) {
		/* is it a operation symbol? */
		Symop *sb = get_symop(b->sym.v);
		if (sb != NULL) {
			c = malloc(sizeof(*c));
			c->hdr.t = VSYMOP;
			c->symop.prio = sb->prio;
			c->symop.v = sb->f;
			return c;
		}
		/* is it a user-defined value symbol? */
		Symval *sv = get_symval(a, b->sym.v);
		if (sv != NULL) {
			c = copy_v(sv->v);
			return c;
		}
		/* later: user defined function symbol... */
		printf("? %s:%d unknown symbol seme\n",
				__FUNCTION__,__LINE__);
		return NULL;
	}
	if (b->hdr.t == SLST) {
		c = malloc(sizeof(*c));
		c->hdr.t = VLST;
		c->lst.v.n = 0;
		c->lst.v.v = NULL;
		size_t n = b->lst.v.n;
		Sem *l = b->lst.v.s;
		Val *d;
		for (size_t i=0; i<n; ++i) {
			d = val_of_seme(a, l+i);
			if (d == NULL) {
				free_v(c);
				return NULL;
			}
			c = push_v(c, d);
		}
		return c;
	}
	if (b->hdr.t == SSEQ) {
		c = malloc(sizeof(*c));
		c->hdr.t = VSEQ;
		c->seq.v.n = 0;
		c->seq.v.v = NULL;
		size_t n = b->seq.v.n;
		Sem *l = b->seq.v.s;
		Val *d;
		for (size_t i=0; i<n; ++i) {
			d = val_of_seme(a, l+i);
			if (d == NULL) {
				free_v(c);
				return NULL;
			}
			c = push_v(c, d);
		}
		return c;
	}
	printf("? %s:%d unknown seme\n",
			__FUNCTION__,__LINE__);
	return NULL;
}

/* ----- evaluation, pass 2 ----- */

static Val *
eval_seq(Val *a) {
	/* work on copy of seq, which gets progressively reduced, and destroyed */
	Val *b = malloc(sizeof(*b));
	assert(b != NULL);
	b->hdr.t = VSEQ;
	b->seq.v.n = 0;
	b->seq.v.v = NULL;
	/* eval seq items, in case items generate symbols */
	for (size_t i=0; i < a->seq.v.n; ++i) {
		Val *c = eval(a->seq.v.v[i]);
		if (c == NULL) {
			free_v(b);
			return NULL;
		}
		b = push_v(b, c);
	}
	/* symbol application: consumes the seq, until 1 item left */
	Val *c = NULL;  /* shorthand */
	while (b->seq.v.n > 0) {
		/* stop: seq of 1 element reduced to this element */
		if (b->seq.v.n == 1) {
			c = b->seq.v.v[0];
			Val *d = copy_v(c);
			free_v(b);
			return d;
		}
		unsigned int hiprio = minprio()+1;
		size_t symat = 0;
		bool symfound = false;
		/* apply symbols from left to right (for same priority symbols) */
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
			printf("? %s:%d sequence without function symbol\n",
					__FUNCTION__,__LINE__);
			free_v(b);
			return NULL;
		}
		/* apply the symbol, returns the reduced seq */
		c = b->seq.v.v[symat]->symop.v(b, symat);
		if (c == NULL) {
			printf("? %s:%d symbol application failed\n",
					__FUNCTION__,__LINE__);
			free_v(b);
			return NULL;
		}
		b = c;
	}
	/* empty seq, means nil value */
	free_v(b);
	c = malloc(sizeof(*c));
	c->hdr.t = VNIL;
	return c;
}

static Val *
eval(Val *a) {
	/* eval() returns a fresh value, leaves a untouched */
	assert(a != NULL && "value is null");
	if (isatom_v(a)) {
		Val *b = copy_v(a);
		return b;
	}
	if (a->hdr.t == VLST) {
		Val *b = malloc(sizeof(*b));
		b->hdr.t = VLST;
		b->lst.v.n = 0;
		b->lst.v.v = NULL;
		for (size_t i=0; i < a->lst.v.n; ++i) {
			Val *c = eval(a->lst.v.v[i]);
			if (c == NULL) {
				free_v(b);
				return NULL;
			}
			b = push_v(b, c);
		}
		return b;
	}
	if (a->hdr.t == VSEQ) {
		return eval_seq(a);
	}
	printf("? %s:%d unknown value\n",
			__FUNCTION__,__LINE__);
	return NULL;
}

/* ------------ Phrases, list of expressions --------- */

typedef struct {
	size_t n;
	char **x;
} Phrase;

static Phrase *
phrase() {
	Phrase *a = malloc(sizeof(*a));
	a->n = 0;
	a->x = NULL;
	return a;
}

static void
free_ph(Phrase *a) {
	if (a == NULL) {
		return;
	}
	for (size_t i=0; i<a->n; ++i) {
		free(a->x[i]);
	}
	free(a->x);
	free(a);
}

static void
print_ph(Phrase *a) {
	if (a == NULL) {
		printf("? %s:%d phrase is empty\n",
				__FUNCTION__,__LINE__);
		return;
	}
	for (size_t i=0; i<a->n; ++i) {
		printf("%s; ", a->x[i]);
	}
	printf("\n");
}

static Phrase *
push_ph(Phrase *a, char *b) {
	if (a == NULL) {
		printf("? %s:%d phrase is null\n",
				__FUNCTION__,__LINE__);
		return NULL;
	}
	if (b == NULL) {
		printf("? %s:%d pushed expression is null\n",
				__FUNCTION__,__LINE__);
		return NULL;
	}
	char **c = malloc((a->n +1)*sizeof(c));
	if (a->n > 0) {
		memcpy(c, a->x, a->n * sizeof(char*));
	}
	c[a->n] = b;
	if (a->n > 0) {
		free(a->x);
	}
	++(a->n);
	a->x = c;
	return a;
}

/* expression is 64 words max */
#define XSZ 64*WSZ

static Phrase *
phrase_of_str(char *a) {
	size_t boff = 0;
	char buf[XSZ];
	buf[0] = '\0';
	size_t read;
	Phrase *b = phrase();
	assert(b != NULL && "Phrase allocation error");
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
			printf("\n? %s:%d expression too big (%luB)!\n", 
					__FUNCTION__,
					__LINE__,
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
		memcpy(buf+boff, a+off, read*sizeof(char));
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

/* Environment: defined symbols across a phrase */

static bool
added_sym(Env *a, Symval *b, bool err) {
	assert(a != NULL && "environment null");
	if (b == NULL) {
		printf("? %s:%d pushed symbol null\n",
				__FUNCTION__,__LINE__);
		return false;
	}
	if (get_symval(a, b->name) != NULL) {
		if (err) {
			printf("? %s:%d symbol already defined (%s)\n",
					__FUNCTION__,__LINE__, b->name);
		}
		return false;
	}
	Symval **c = malloc((a->n +1)*sizeof(Symval*));
	if (a->n > 0) {
		memcpy(c, a->s, a->n * sizeof(Symval*));
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
	if (b == NULL) { 
		printf("? %s:%d pushed symbol null\n",
				__FUNCTION__,__LINE__);
		return false;
	}
	if (strlen(b->name) == 0) {
		printf("? %s:%d symbol name null\n",
				__FUNCTION__,__LINE__);
		return false;
	}
	size_t id;
	Symval *c = get_symval_id(a, b->name, &id);
	if (c == NULL) {
		if (err) {
			printf("? %s:%d symbol not found (%s)\n",
					__FUNCTION__,__LINE__, b->name);
		}
		return false;
	}
	free_symval(c);
	a->s[id] = b;
	return true;
}
static bool
store_sym(Env *a, Symval *b) {
	if (upded_sym(a, b, false)) {
		return true;
	}
	if (added_sym(a, b, true)) {
		return true;
	}
	return false;
}

static Env *
eval_ph(Phrase *a) {
	assert(a != NULL && "phrase null");
	Env *b = malloc(sizeof(*b));
	assert(b != NULL);
	b->n = 0;
	b->s = NULL;
	for (size_t i=0; i<a->n; ++i) {
		printf("# expression %lu:\t %s", i, a->x[i]);
		Expr *ex = exp_of_words(a->x[i]);
		/* a is freed where allocated, not here but by caller */
		if (ex == NULL) {
			free_env(b);
			return NULL;
		}
		Sem *sm = seme_of_exp(ex);
		free_x(ex);
		if (sm == NULL) {
			free_env(b);
			return NULL;
		}
		printf("# seme %lu:\t ", i); print_s(sm); printf("\n");
		Val *v = val_of_seme(b, sm);
		free_s(sm);
		free(sm);
		if (v == NULL) {
			free_env(b);
			return NULL;
		}
		printf("# value %lu:\t ", i); print_v(v); printf("\n");
		Val *ev = eval(v);
		free_v(v);
		if (ev == NULL) {
			free_env(b);
			return NULL;
		}
		printf("# = "); print_v(ev); printf("\n");
		Symval *it = symval("it", ev);
		free_v(ev);
		if (it == NULL) {
			free_env(b);
			return NULL;
		}
		if (!store_sym(b, it)) {
			free_symval(it);
			free_env(b);
			return NULL;
		}
		printf("# env: "); print_env(b); printf("\n");
	}
	return b;
}

/* ----- main ----- */

static void
usage(const char *exe) {
	printf("usage: %s expression\n", exe);
}

int
main(int argc, char **argv) {
	if (argc < 2) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}
	char *s = argv[1];
	printf("# input:\t %s\n", s);
	Phrase *ph = phrase_of_str(s);
	if (ph == NULL) {
		return EXIT_FAILURE;
	}
	printf("# phrase:\t "); print_ph(ph); printf("\n");
	Env *e = eval_ph(ph);
	free_ph(ph);
	if (e == NULL) {
		return EXIT_FAILURE;
	}
	print_env(e);
	free_env(e);
	return EXIT_SUCCESS;
}
