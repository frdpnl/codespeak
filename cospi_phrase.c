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
} Listw;

static Listw *
listw() {
	Listw *a = malloc(sizeof(*a));
	a->n = 0;
	a->w = NULL;
	return a;
}

static void
free_lw(Listw *a) {
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
		printf("%s:%d word to big\n", 
				__FUNCTION__, __LINE__);
		return NULL;
	}
	Word *b = word(STR);
	memcpy(b->v, a, n*sizeof(char));
	b->v[n] = '\0';
	return b;
}

static Listw *
push_wz(Listw *a, Word *b) {
	if (a == NULL) {
		printf("%s:%d list is null\n", 
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
			printf("%s:%d unknown word type ", 
					__FUNCTION__, __LINE__);
	}
}

static void
print_lw(Listw *a) {
	if (a == NULL) {
		printf("list of words is empty\n");
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

static Listw *
read_words(char *a) {
	size_t boff = 0;
	char buf[WSZ];
	buf[0] = '\0';
	size_t read;
	Listw *b = listw();
	Word *w = NULL;
	for (size_t off = 0; a[off] != '\0'; off += read) {
		read = grapheme_next_character_break_utf8(
				a+off, SIZE_MAX);
		if (strncmp("(", a+off, read) == 0) {
			if (boff) {
				w = word_str(buf, boff);
				b = push_wz(b, w);
				boff = 0;
			}
			w = word(LEFT);
			b = push_wz(b, w);
			continue;
		} 
		if (strncmp(")", a+off, read) == 0) {
			if (boff) {
				w = word_str(buf, boff);
				b = push_wz(b, w);
				boff = 0;
			}
			w = word(RIGHT);
			b = push_wz(b, w);
			continue;
		} 
		if (strncmp(",", a+off, read) == 0) {
			if (boff) {
				w = word_str(buf, boff);
				b = push_wz(b, w);
				boff = 0;
			}
			w = word(SEP);
			b = push_wz(b, w);
			continue;
		} 
		if (isspace((int)*(a+off))) {
			if (boff) {
				w = word_str(buf, boff);
				b = push_wz(b, w);
				boff = 0;
			}
			continue;
		} 
		if (boff+read >= WSZ) {
			printf("\n%s:%d word too big (%luB)!\n", 
					__FUNCTION__,
					__LINE__,
					boff+read);
			free_lw(b);
			return NULL;
		}
		/* default case: add to current word */
		memcpy(buf+boff, a+off, read*sizeof(char));
		boff += read;
		buf[boff] = '\0';
	}
	printf("\n");
	if (boff) {
		w = word_str(buf, boff);
		b = push_wz(b, w);
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
		printf("%s:%d null seme\n",
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
			printf("%s:%d unknown seme\n",
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
		printf("%s:%d null seme\n",
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
			printf("%s:%d unknown seme\n",
					__FUNCTION__,__LINE__);
	}
}

static Sem* 
isnat(Word *a) {
	if (a->t != STR) {
		printf("%s:%d word is not a string\n",
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
		printf("%s:%d natural number out of range %s\n", 
				__FUNCTION__, __LINE__, a->v);
	} 
	return NULL;
}

static Sem *
isrea(Word *a) {
	if (a->t != STR) {
		printf("%s:%d word is not a string\n",
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
			printf("%s:%d real underflow %s\n", 
					__FUNCTION__, __LINE__, a->v);
		} else {
			printf("%s:%d real overflow %s\n", 
					__FUNCTION__, __LINE__, a->v);
		}
	}
	return NULL;
}

static Sem *
issym(Word *a) {
	if (a->t != STR) {
		printf("%s:%d word is not a string\n",
				__FUNCTION__, __LINE__);
		return NULL;
	}
	return sem_sym(a->v);
}

static Sem *
push_s(Sem *a, Sem *b) {
	if (a == NULL) {
		printf("%s:%d seq or lst seme is null\n",
				__FUNCTION__,__LINE__);
		return NULL;
	}
	if (b == NULL) {
		printf("%s:%d pushed seme is null\n",
				__FUNCTION__,__LINE__);
		return NULL;
	}
	if (a->hdr.t != SSEQ && a->hdr.t != SLST) {
		printf("%s:%d not a seq or lst seme\n",
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
		printf("%s:%d seme is null\n",
				__FUNCTION__,__LINE__);
		return NULL;
	}
	if (a->hdr.t != SSEQ && a->hdr.t != SLST) {
		printf("%s:%d not a seq or lst seme\n",
				__FUNCTION__,__LINE__);
		free_s(a);
		return NULL;
	}
	if (a->hdr.t == SSEQ && a->seq.v.n > 1) {
		printf("%s:%d cannot add a list element to a seq-seme\n",
				__FUNCTION__,__LINE__);
		free_s(a);
		return NULL;
	}
	a->hdr.t = SLST;
	return a;
}

static Sem *
read_seme(Listw *a, size_t from, size_t tox) {
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
					if (b == NULL) {
						return NULL;
					}
				}
				lst_expect1 = true;
				break;
			case LEFT:
				if (b->hdr.t == SLST && !lst_expect1) {
					printf("%s:%d unexpected list element\n",
							__FUNCTION__, __LINE__);
					free_s(b);
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
					printf("%s:%d unmatched %s\n",
							__FUNCTION__,__LINE__,
							inpar < 0 ? ")" : "(");
					free_s(b);
					return NULL;
				}
				break;
			case RIGHT:
				printf("%s:%d unmatched )\n",
						__FUNCTION__,__LINE__);
				free_s(b);
				return NULL;
			case STR:
				if (b->hdr.t == SLST && !lst_expect1) {
					printf("%s:%d unexpected list element\n",
							__FUNCTION__, __LINE__);
					free_s(b);
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
					printf("%s:%d unknown word\n",
							__FUNCTION__,__LINE__);
					free_s(b);
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
				printf("%s:%d unexpected word\n",
						__FUNCTION__,__LINE__);
				free_s(b);
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
read_semes(Listw *a) {
	if (a == NULL) {
		printf("%s:%d null list of words\n",
				__FUNCTION__,__LINE__);
		return NULL;
	}
	if (a->n == 0) {
		return sem_nil();
	} 
	return read_seme(a, 0, a->n);
}

/* ----- evaluation, pass 1 ----- */

typedef enum {VNIL, VNAT, VREA, VSYM, VLST, VSEQ} vtype;

typedef union Val_ Val;

typedef struct vList_ {
	size_t n;
	Val **v;
} vList;

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
	} sym;
	struct {
		vtype t;
		vList v;
	} lst;
	struct {
		vtype t;
		vList v;
	} seq;
} Val;

static void
print_v(Val *a) {
	if (a == NULL) {
		printf("%s:%d null value\n",
				__FUNCTION__,__LINE__);
		return;
	}
	switch (a->hdr.t) {
		case VNIL:
			printf("Nil ");
			break;
		case VNAT:
			printf("n%lld ", a->nat.v);
			break;
		case VREA:
			printf("r%.2lf ", a->rea.v);
			break;
		case VSYM:
			printf("'%p(%d) ", a->sym.v, a->sym.prio);
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
			printf("%s:%d unknown value\n",
					__FUNCTION__, __LINE__);
	}
}

static void 
free_v(Val *a) {
	if (a == NULL) {
		printf("%s:%d null value\n",
				__FUNCTION__, __LINE__);
		return;
	}
	switch (a->hdr.t) {
		case VNIL:
		case VSYM:
		case VNAT:
		case VREA:
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
			printf("%s:%d unknown value\n",
					__FUNCTION__,__LINE__);
	}
	free(a);
}

static Val *
copy_v(Val *a) {
	if (a == NULL) {
		printf("%s:%d value is null\n",
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
		printf("%s:%d seq or lst val is null\n",
				__FUNCTION__,__LINE__);
		return NULL;
	}
	if (b == NULL) {
		printf("%s:%d pushed val is null\n",
				__FUNCTION__,__LINE__);
		return NULL;
	}
	if (a->hdr.t != VSEQ && a->hdr.t != VLST) {
		printf("%s:%d not a seq or lst val\n",
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

static Val * eval(Val *a);

static bool 
infixed(size_t p, size_t n) {
	return (p > 0 && p < n-1);
}

Val *
eval_mul(Val *s, size_t p) {
	if (!infixed(p, s->seq.v.n)) {
		printf("%s:%d symbol not infixed\n", 
				__FUNCTION__,__LINE__);
		free_v(s);
		return NULL;
	}
	Val *a = eval(s->seq.v.v[p-1]);
	if (a == NULL) {
		printf("%s:%d 1st argument null\n", 
				__FUNCTION__,__LINE__);
		free_v(s);
		return NULL;
	}
	Val *b = eval(s->seq.v.v[p+1]);
	if (b == NULL) {
		printf("%s:%d 2nd argument null\n", 
				__FUNCTION__,__LINE__);
		free_v(a);
		free_v(s);
		return NULL;
	}
	if ((a->hdr.t != VNAT && a->hdr.t != VREA)
			|| (b->hdr.t != VNAT && b->hdr.t != VREA)) {
		printf("%s:%d arguments not numbers\n", 
				__FUNCTION__,__LINE__);
		free_v(a);
		free_v(b);
		free_v(s);
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
	/* consumed 2 seq items */
	for (size_t i=p-1; i < s->seq.v.n && i < p+2; ++i) {
		free_v(s->seq.v.v[i]);
	}
	s->seq.v.v[p-1] = a;
	for (size_t i=p+2; i < s->seq.v.n; ++i) {
		s->seq.v.v[i-2] = s->seq.v.v[i];
	}
	s->seq.v.n -= 2;
	return s;
}
Val *
eval_div(Val *s, size_t p) {
	printf("%s:%d\n", __FUNCTION__,__LINE__);
	free_v(s);
	return NULL;
}
Val *
eval_plu(Val *s, size_t p) {
	if (!infixed(p, s->seq.v.n)) {
		printf("%s:%d symbol not infixed\n", 
				__FUNCTION__,__LINE__);
		free_v(s);
		return NULL;
	}
	Val *a = eval(s->seq.v.v[p-1]);
	if (a == NULL) {
		printf("%s:%d 1st argument null\n", 
				__FUNCTION__,__LINE__);
		free_v(s);
		return NULL;
	}
	Val *b = eval(s->seq.v.v[p+1]);
	if (b == NULL) {
		printf("%s:%d 2nd argument null\n", 
				__FUNCTION__,__LINE__);
		free_v(a);
		free_v(s);
		return NULL;
	}
	if ((a->hdr.t != VNAT && a->hdr.t != VREA)
			|| (b->hdr.t != VNAT && b->hdr.t != VREA)) {
		printf("%s:%d arguments not numbers\n", 
				__FUNCTION__,__LINE__);
		free_v(a);
		free_v(b);
		free_v(s);
		return NULL;
	}
	if (a->hdr.t == VNAT && b->hdr.t == VNAT) {
		a->nat.v += b->nat.v;
	} else if (a->hdr.t == VNAT && b->hdr.t == VREA) {
		a->hdr.t = VREA;
		a->rea.v = (double)a->nat.v * b->rea.v;
	} else if (a->hdr.t == VREA && b->hdr.t == VNAT) {
		a->rea.v += (double)b->nat.v;
	} else if (a->hdr.t == VREA && b->hdr.t == VREA) {
		a->rea.v += b->rea.v;
	}
	free_v(b);
	/* consumed 2 seq items */
	for (size_t i=p-1; i < s->seq.v.n && i < p+2; ++i) {
		free_v(s->seq.v.v[i]);
	}
	s->seq.v.v[p-1] = a;
	for (size_t i=p+2; i < s->seq.v.n; ++i) {
		s->seq.v.v[i-2] = s->seq.v.v[i];
	}
	s->seq.v.n -= 2;
	return s;
}
Val *
eval_min(Val *s, size_t p) {
	printf("%s:%d\n", __FUNCTION__,__LINE__);
	free_v(s);
	return NULL;
}
Val *
eval_les(Val *s, size_t p) {
	printf("%s:%d\n", __FUNCTION__,__LINE__);
	free_v(s);
	return NULL;
}
Val *
eval_leq(Val *s, size_t p) {
	printf("%s:%d\n", __FUNCTION__,__LINE__);
	free_v(s);
	return NULL;
}
Val *
eval_gre(Val *s, size_t p) {
	printf("%s:%d\n", __FUNCTION__,__LINE__);
	free_v(s);
	return NULL;
}
Val *
eval_grq(Val *s, size_t p) {
	printf("%s:%d\n", __FUNCTION__,__LINE__);
	free_v(s);
	return NULL;
}

typedef struct symtof_ {
	char str[WSZ];
	unsigned int prio;
	Val* (*f)(Val *s, size_t p);
} symtof;

#define NSYMS 8
symtof Syms[] = {
	(symtof) {"*",  1, eval_mul},
	(symtof) {"/",  1, eval_div},
	(symtof) {"+",  2, eval_plu},
	(symtof) {"-",  2, eval_min},
	(symtof) {"<",  3, eval_les},
	(symtof) {"<=", 3, eval_leq},
	(symtof) {">",  3, eval_gre},
	(symtof) {">=", 3, eval_grq},
};

static symtof *
find_sym(char *a) {
	for (size_t i=0; i<NSYMS; ++i) {
		symtof *b = Syms+i;
		if (strncmp(b->str, a, WSZ) == 0) {
			return b;
		}
	}
	return NULL;
}

static bool
isatom_v(Val *a) {
	if (a==NULL) {
		printf("%s:%d null value\n",
				__FUNCTION__,__LINE__);
		return false;
	}
	return (a->hdr.t == VNIL 
			|| a->hdr.t == VNAT 
			|| a->hdr.t == VREA 
			|| a->hdr.t == VSYM);
}

static Val *
read_val(Sem *a) {
	if (a == NULL) {
		printf("%s:%d seme is null\n",
				__FUNCTION__,__LINE__);
		return NULL;
	}
	Val *b = malloc(sizeof(Val));
	if (a->hdr.t == SNIL ) {
		b->hdr.t = VNIL;
		return b;
	}
	if (a->hdr.t == SNAT) {
		b->hdr.t = VNAT;
		b->nat.v = a->nat.v;
		return b;
	}
	if (a->hdr.t == SREA) {
		b->hdr.t = VREA;
		b->rea.v = a->rea.v;
		return b;
	}
	if (a->hdr.t == SSYM) {
		b->hdr.t = VSYM;
		symtof *s = find_sym(a->sym.v);
		if (s == NULL) {
			printf("%s:%d unknown symbol \"%s\"\n",
					__FUNCTION__,__LINE__,a->sym.v);
			return NULL;
		}
		b->sym.prio = s->prio;
		b->sym.v = s->f;
		return b;
	}
	if (a->hdr.t == SLST) {
		b->hdr.t = VLST;
		b->lst.v.n = 0;
		b->lst.v.v = NULL;
		size_t n = a->lst.v.n;
		Sem *l = a->lst.v.s;
		Val *c;
		for (size_t i=0; i<n; ++i) {
			c = read_val(l+i);
			b = push_v(b, c);
		}
		return b;
	}
	if (a->hdr.t == SSEQ) {
		b->hdr.t = VSEQ;
		b->seq.v.n = 0;
		b->seq.v.v = NULL;
		size_t n = a->seq.v.n;
		Sem *l = a->seq.v.s;
		Val *c;
		for (size_t i=0; i<n; ++i) {
			c = read_val(l+i);
			b = push_v(b, c);
		}
		return b;
	}
	printf("%s:%d unknown seme\n",
			__FUNCTION__,__LINE__);
	return NULL;
}

/* ----- evaluation, pass 2 ----- */

static Val *
eval(Val *a) {
	if (a == NULL) {
		printf("%s:%d value is null\n",
				__FUNCTION__,__LINE__);
		return NULL;
	}
	Val *b = NULL;
	if (isatom_v(a)) {
		b = copy_v(a);
		return b;
	}
	Val *c;
	if (a->hdr.t == VLST) {
		b = malloc(sizeof(Val));
		b->hdr.t = VLST;
		b->lst.v.n = 0;
		b->lst.v.v = NULL;
		for (size_t i=0; i < a->lst.v.n; ++i) {
			c = eval(a->lst.v.v[i]);
			b = push_v(b, c);
		}
		return b;
	}
	if (a->hdr.t == VSEQ) {
		b = copy_v(a);
		while (b->seq.v.n > 0) {
			unsigned int minprio = 100; /* fix this later (fun that computes it) */
			size_t symat = 0;
			bool symfound = false;
			for (size_t i=0; i < b->seq.v.n; ++i) {
				c = b->seq.v.v[i];
				if (c->hdr.t == VSYM) {
					if (c->sym.prio < minprio) {
						minprio = c->sym.prio;
						symat = i;
						symfound = true;
					}
				}
			}
			if (!symfound) { 
				if (b->seq.v.n == 1) {
					c = eval(b->seq.v.v[0]);
					free_v(b);
					return c;
				} else { /* multiple seq items without any symbol */
					printf("%s:%d seq-value without symbol\n",
							__FUNCTION__,__LINE__);
					free_v(b);
					return NULL;
				}
			}
			/* execute the symbol */
			b = b->seq.v.v[symat]->sym.v(b, symat);
			if (b == NULL) {
				return NULL;
			}
		}
		/* empty seq, return nil */
		c = malloc(sizeof(Val));
		c->hdr.t = VNIL;
		free_v(b);
		return c;
	}
	printf("%s:%d unknown value\n",
			__FUNCTION__,__LINE__);
	return NULL;
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
		return 1;
	}
	char *s = argv[1];
	printf("%s\n", s);
	Listw *lw = read_words(s);
	if (lw == NULL) {
		return 1;
	}
	Sem *ls = read_semes(lw);
	free_lw(lw);
	if (ls == NULL) {
		return 1;
	}
	printf("semes: "); print_s(ls); printf("\n");
	Val *v = read_val(ls);
	free_s(ls);
	free(ls);
	if (v == NULL) {
		return 1;
	}
	printf("values: "); print_v(v); printf("\n");
	Val *ev = eval(v);
	free_v(v);
	if (ev == NULL) {
		return 1;
	}
	printf("evaluated: "); print_v(ev); printf("\n\n");
	free_v(ev);
	return 0;
}
