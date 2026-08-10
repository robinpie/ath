/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2026 robinpie
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* ath_cake.c -- the !^CAKE schema engine: a .^CAKE parser, the layout
   algorithm, the captchalogue-code canonicalization (FNV-1a-64 folded to 48
   bits, base-64 over Homestuck's alphabet), and the && / || alchemy operators.
   Pure C89; no libffi or dlopen, so it builds on every target. */

#include "ath_cake.h"
#include "ath_value.h"
#include "ath_error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ===================================================================== */
/* Small string buffer                                                   */
/* ===================================================================== */

typedef struct { char *buf; int len; int cap; } Sbuf;

static void sb_init(Sbuf *s) {
    s->cap = 64; s->len = 0;
    s->buf = (char *)malloc((size_t)s->cap);
    if (!s->buf) ath_fatal("out of memory");
    s->buf[0] = '\0';
}
static void sb_ensure(Sbuf *s, int extra) {
    if (s->len + extra + 1 > s->cap) {
        while (s->len + extra + 1 > s->cap) s->cap *= 2;
        s->buf = (char *)realloc(s->buf, (size_t)s->cap);
        if (!s->buf) ath_fatal("out of memory");
    }
}
static void sb_puts(Sbuf *s, const char *t) {
    int n = (int)strlen(t);
    sb_ensure(s, n);
    memcpy(s->buf + s->len, t, (size_t)n);
    s->len += n; s->buf[s->len] = '\0';
}
static void sb_putc_(Sbuf *s, char c) {
    sb_ensure(s, 1);
    s->buf[s->len++] = c; s->buf[s->len] = '\0';
}
static void sb_putint(Sbuf *s, long v) {
    char tmp[32]; sprintf(tmp, "%ld", v); sb_puts(s, tmp);
}

static char *ck_strdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *d = (char *)malloc(n);
    if (!d) ath_fatal("out of memory");
    memcpy(d, s, n);
    return d;
}

/* ===================================================================== */
/* Captchalogue code: FNV-1a-64 in two 32-bit limbs, folded to 48 bits,  */
/* base-64 over the 64-char alphabet (MSB-first).                        */
/* ===================================================================== */

static const char CK_ALPHABET[65] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz?!";

typedef struct { unsigned long hi, lo; } U64; /* each masked to 32 bits */

static void mul32(unsigned long a, unsigned long b,
                  unsigned long *hi, unsigned long *lo) {
    unsigned long a0 = a & 0xFFFFUL, a1 = (a >> 16) & 0xFFFFUL;
    unsigned long b0 = b & 0xFFFFUL, b1 = (b >> 16) & 0xFFFFUL;
    unsigned long p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
    unsigned long mid = (p00 >> 16) + (p01 & 0xFFFFUL) + (p10 & 0xFFFFUL);
    *lo = ((p00 & 0xFFFFUL) | ((mid & 0xFFFFUL) << 16)) & 0xFFFFFFFFUL;
    *hi = (p11 + (p01 >> 16) + (p10 >> 16) + (mid >> 16)) & 0xFFFFFFFFUL;
}

static U64 mul64(U64 a, U64 b) {
    unsigned long ll_hi, ll_lo;
    U64 r;
    mul32(a.lo, b.lo, &ll_hi, &ll_lo);
    r.lo = ll_lo;
    r.hi = (ll_hi + (a.lo * b.hi & 0xFFFFFFFFUL)
                  + (a.hi * b.lo & 0xFFFFFFFFUL)) & 0xFFFFFFFFUL;
    return r;
}

static void ck_code_from_desc(const char *desc, char out[9]) {
    U64 h, prime;
    unsigned long lo, hi;
    int sext[8];
    int k;
    const unsigned char *p = (const unsigned char *)desc;

    h.hi = 0xCBF29CE4UL; h.lo = 0x84222325UL;     /* FNV offset basis */
    prime.hi = 0x00000100UL; prime.lo = 0x000001B3UL; /* FNV prime */
    while (*p) {
        h.lo ^= (unsigned long)(*p);
        h = mul64(h, prime);
        p++;
    }
    /* fold: h48 = (h ^ (h >> 48)) & 0xFFFFFFFFFFFF */
    lo = (h.lo ^ ((h.hi >> 16) & 0xFFFFUL)) & 0xFFFFFFFFUL;
    hi = h.hi & 0xFFFFUL;
    if (lo == 0 && hi == 0) lo = 1; /* perturb all-zero (non-empty) hashes */
    for (k = 0; k < 8; k++) {
        sext[k] = (int)(lo & 0x3FUL);
        lo = ((lo >> 6) | ((hi & 0x3FUL) << 26)) & 0xFFFFFFFFUL;
        hi = hi >> 6;
    }
    for (k = 0; k < 8; k++) out[k] = CK_ALPHABET[sext[7 - k]];
    out[8] = '\0';
}

static int ck_alpha_index(char c) {
    int i;
    for (i = 0; i < 64; i++) if (CK_ALPHABET[i] == c) return i;
    return -1;
}

/* Compare 8-char codes by alphabet index (NOT ASCII). */
static int ck_code_cmp(const char *a, const char *b) {
    int i, ia, ib;
    for (i = 0; i < 8; i++) {
        ia = ck_alpha_index(a[i]); ib = ck_alpha_index(b[i]);
        if (ia != ib) return ia < ib ? -1 : 1;
    }
    return 0;
}

/* ===================================================================== */
/* Recipe lifecycle                                                      */
/* ===================================================================== */

static void ck_type_free(CkType *t);

static void ck_type_free(CkType *t) {
    if (!t) return;
    if (t->tag == CK_T_ARRAY) ck_type_free(t->elem);
    if (t->tag == CK_T_NESTED && t->nested) ath_recipe_decref(t->nested);
    free(t);
}

static void ath_recipe_free(AthRecipe *r) {
    int i;
    if (!r) return;
    for (i = 0; i < r->n_ingredients; i++) {
        if (r->ingredients[i].name) free(r->ingredients[i].name);
        ck_type_free(r->ingredients[i].type);
    }
    if (r->ingredients) free(r->ingredients);
    for (i = 0; i < r->n_arms; i++) {
        if (r->arms[i].name) free(r->arms[i].name);
        if (r->arms[i].recipe) ath_recipe_decref(r->arms[i].recipe);
    }
    if (r->arms) free(r->arms);
    if (r->bind_name) free(r->bind_name);
    free(r);
}

void ath_recipe_incref(AthRecipe *r) { if (r) r->refcount++; }
void ath_recipe_decref(AthRecipe *r) {
    if (!r) return;
    if (--r->refcount <= 0) ath_recipe_free(r);
}

static AthRecipe *ck_recipe_new(CkRecipeKind kind) {
    AthRecipe *r = (AthRecipe *)calloc(1, sizeof(AthRecipe));
    if (!r) ath_fatal("out of memory");
    r->refcount = 1;
    r->kind = kind;
    r->align = 1;
    strcpy(r->code, "00000000");
    return r;
}

/* ===================================================================== */
/* Scalar layout table (fixed widths MANDATED; native widths host-ABI)   */
/* ===================================================================== */

static int align_up(int off, int a) {
    if (a <= 1) return off;
    return ((off + a - 1) / a) * a;
}

static void ck_scalar_layout(CkTypeTag tag, int *size, int *align) {
    switch (tag) {
    case CK_T_PINCH:   *size = 1; *align = 1; break;
    case CK_T_DASH:    *size = 2; *align = 2; break;
    case CK_T_SPOON:   *size = 4; *align = 4; break;
    case CK_T_CUP:     *size = 8; *align = 8; break;  /* mandated 8 on ALL targets */
    case CK_T_DROP:    *size = 4; *align = 4; break;
    case CK_T_DOLLOP:  *size = 8; *align = 8; break;  /* mandated 8 on ALL targets */
    case CK_T_INTEGER: *size = (int)sizeof(long);  *align = (int)sizeof(long);  break;
    case CK_T_BOOLEAN: *size = 1; *align = 1; break;
    case CK_T_STRING:  *size = (int)sizeof(void *); *align = (int)sizeof(void *); break;
    case CK_T_RELIC:   *size = (int)sizeof(void *); *align = (int)sizeof(void *); break;
    default:           *size = 0; *align = 1; break;
    }
}

static const char *ck_typedesc_scalar(CkTypeTag tag, int is_signed) {
    switch (tag) {
    case CK_T_PINCH:   return is_signed ? "i8"  : "u8";
    case CK_T_DASH:    return is_signed ? "i16" : "u16";
    case CK_T_SPOON:   return is_signed ? "i32" : "u32";
    case CK_T_CUP:     return is_signed ? "i64" : "u64";
    case CK_T_DROP:    return "f32";
    case CK_T_DOLLOP:  return "f64";
    case CK_T_BOOLEAN: return "bool";
    case CK_T_INTEGER: return "long";
    case CK_T_STRING:  return "ptr";
    case CK_T_RELIC:   return "ptr";
    case CK_T_CRUST:   return "ptr";
    default:           return "?";
    }
}

static void ck_typedesc(Sbuf *sb, CkType *t) {
    if (t->tag == CK_T_ARRAY) {
        sb_putc_(sb, '[');
        sb_putint(sb, t->array_count);
        sb_putc_(sb, ']');
        ck_typedesc(sb, t->elem);
    } else if (t->tag == CK_T_NESTED) {
        sb_puts(sb, "rec(");
        sb_puts(sb, t->nested->code);
        sb_putc_(sb, ')');
    } else {
        sb_puts(sb, ck_typedesc_scalar(t->tag, t->is_signed));
    }
}

/* ===================================================================== */
/* Deep type equality (for && merge dedup)                               */
/* ===================================================================== */

static int ck_type_eq(CkType *a, CkType *b) {
    if (a->tag != b->tag) return 0;
    switch (a->tag) {
    case CK_T_PINCH: case CK_T_DASH: case CK_T_SPOON: case CK_T_CUP:
        return a->is_signed == b->is_signed;
    case CK_T_ARRAY:
        return a->array_count == b->array_count && ck_type_eq(a->elem, b->elem);
    case CK_T_NESTED:
        return strcmp(a->nested->code, b->nested->code) == 0;
    case CK_T_CRUST:
        return 1; /* two crusts are indistinguishable in the hash model */
    default:
        return 1; /* same tag, no further discriminator */
    }
}

static CkType *ck_type_copy(CkType *t) {
    CkType *c = (CkType *)calloc(1, sizeof(CkType));
    if (!c) ath_fatal("out of memory");
    *c = *t;
    if (t->tag == CK_T_ARRAY) c->elem = ck_type_copy(t->elem);
    if (t->tag == CK_T_NESTED && t->nested) ath_recipe_incref(t->nested);
    return c;
}

/* ===================================================================== */
/* Tokenizer                                                             */
/* ===================================================================== */

typedef enum { TK_EOF, TK_IDENT, TK_INT, TK_STR, TK_PUNCT } TkKind;

typedef struct { TkKind kind; char *text; long ival; int line; } Tok;

typedef struct CkMeasure { char *name; long value; } CkMeasure;

typedef struct AlchemyNode {
    int op;                 /* 0 = atom, 1 = &&, 2 = || */
    char *name;             /* atom */
    struct AlchemyNode *left, *right;
} AlchemyNode;

typedef enum { RT_SCALAR, RT_NAMED, RT_CRUST, RT_ARRAY } RawTypeKind;

typedef struct RawType {
    RawTypeKind kind;
    CkTypeTag scalar_tag;   /* RT_SCALAR */
    int is_signed;
    char *named;            /* RT_NAMED / RT_CRUST */
    long count;             /* RT_ARRAY */
    struct RawType *elem;
} RawType;

typedef struct RawMember {
    int is_rise;            /* 1 = RISE TO directive */
    long rise_val;
    char *name;             /* ingredient name */
    RawType *type;
    int line;
} RawMember;

typedef struct RawRecipe {
    char *name;
    int dense, imperial;
    int has_punch;
    char punch[16];
    int is_alchemy;
    RawMember *members; int n_members;
    AlchemyNode *alch;
    int resolved;           /* 0 none, 1 done, 2 in-progress */
    AthRecipe *result;      /* memoized strong ref */
    int line;
} RawRecipe;

typedef struct Ck {
    Tok *toks; int ntoks; int pos;
    CkMeasure *measures; int n_measures;
    RawRecipe *recipes; int n_recipes;
    const char *path;
} Ck;

static void ck_err(Ck *ck, const char *fmt, const char *a) {
    (void)ck;
    if (a) ath_runtime_error_fmt(fmt, a);
    else   ath_runtime_error_fmt("%s", fmt);
}

static int is_ident_start(int c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}
static int is_ident_char(int c) {
    return is_ident_start(c) || (c >= '0' && c <= '9');
}
static int is_digit(int c) { return c >= '0' && c <= '9'; }

static void tok_push(Tok **arr, int *n, int *cap, TkKind k, const char *text,
                     long ival, int line) {
    if (*n >= *cap) {
        *cap = *cap == 0 ? 64 : *cap * 2;
        *arr = (Tok *)realloc(*arr, sizeof(Tok) * (size_t)*cap);
        if (!*arr) ath_fatal("out of memory");
    }
    (*arr)[*n].kind = k;
    (*arr)[*n].text = text ? ck_strdup(text) : NULL;
    (*arr)[*n].ival = ival;
    (*arr)[*n].line = line;
    (*n)++;
}

static void ck_lex(Ck *ck, const char *src) {
    Tok *arr = NULL; int n = 0, cap = 0;
    int i = 0, line = 1;
    int len = (int)strlen(src);
    Sbuf tmp;
    while (i < len) {
        char c = src[i];
        if (c == '\n') { line++; i++; continue; }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v') { i++; continue; }
        if (c == '/' && i + 1 < len && src[i + 1] == '/') {
            while (i < len && src[i] != '\n') i++;
            continue;
        }
        if (is_ident_start((int)c)) {
            int start = i;
            char *s;
            while (i < len && is_ident_char((int)src[i])) i++;
            s = (char *)malloc((size_t)(i - start) + 1);
            if (!s) ath_fatal("out of memory");
            memcpy(s, src + start, (size_t)(i - start));
            s[i - start] = '\0';
            tok_push(&arr, &n, &cap, TK_IDENT, s, 0, line);
            free(s);
            continue;
        }
        if (is_digit((int)c) ||
            (c == '-' && i + 1 < len && is_digit((int)src[i + 1]))) {
            int start = i;
            long v;
            char *s;
            if (c == '-') i++;
            while (i < len && is_digit((int)src[i])) i++;
            s = (char *)malloc((size_t)(i - start) + 1);
            if (!s) ath_fatal("out of memory");
            memcpy(s, src + start, (size_t)(i - start));
            s[i - start] = '\0';
            v = atol(s);
            free(s);
            tok_push(&arr, &n, &cap, TK_INT, NULL, v, line);
            continue;
        }
        if (c == '"') {
            sb_init(&tmp);
            i++;
            while (i < len && src[i] != '"') {
                if (src[i] == '\\' && i + 1 < len) {
                    char e = src[i + 1];
                    if (e == 'n') sb_putc_(&tmp, '\n');
                    else if (e == 't') sb_putc_(&tmp, '\t');
                    else if (e == '\\') sb_putc_(&tmp, '\\');
                    else if (e == '"') sb_putc_(&tmp, '"');
                    else sb_putc_(&tmp, e);
                    i += 2;
                } else {
                    sb_putc_(&tmp, src[i]); i++;
                }
            }
            if (i >= len) { free(tmp.buf); ck_err(ck, "!^CAKE: unterminated string", NULL); }
            i++; /* closing quote */
            tok_push(&arr, &n, &cap, TK_STR, tmp.buf, 0, line);
            free(tmp.buf);
            continue;
        }
        /* punctuation */
        if ((c == '&' && i + 1 < len && src[i + 1] == '&') ||
            (c == '|' && i + 1 < len && src[i + 1] == '|')) {
            char p[3]; p[0] = c; p[1] = c; p[2] = '\0';
            tok_push(&arr, &n, &cap, TK_PUNCT, p, 0, line);
            i += 2;
            continue;
        }
        if (c == '{' || c == '}' || c == ':' || c == ';' ||
            c == '=' || c == '(' || c == ')') {
            char p[2]; p[0] = c; p[1] = '\0';
            tok_push(&arr, &n, &cap, TK_PUNCT, p, 0, line);
            i++;
            continue;
        }
        {
            char msg[64];
            sprintf(msg, "!^CAKE: unexpected character '%c'", c);
            ath_runtime_error_fmt("%s", msg);
        }
    }
    tok_push(&arr, &n, &cap, TK_EOF, NULL, 0, line);
    ck->toks = arr; ck->ntoks = n;
}

/* ===================================================================== */
/* Token cursor helpers                                                  */
/* ===================================================================== */

static Tok *cur(Ck *ck) { return &ck->toks[ck->pos]; }
static Tok *peekn(Ck *ck, int n) {
    int p = ck->pos + n;
    if (p >= ck->ntoks) p = ck->ntoks - 1;
    return &ck->toks[p];
}
static int is_ident(Tok *t, const char *s) {
    return t->kind == TK_IDENT && strcmp(t->text, s) == 0;
}
static int is_punct(Tok *t, const char *s) {
    return t->kind == TK_PUNCT && strcmp(t->text, s) == 0;
}
static void expect_punct(Ck *ck, const char *s) {
    if (!is_punct(cur(ck), s))
        ath_runtime_error_fmt("!^CAKE: expected '%s'", s);
    ck->pos++;
}
static void expect_ident(Ck *ck, const char *s) {
    if (!is_ident(cur(ck), s))
        ath_runtime_error_fmt("!^CAKE: expected '%s'", s);
    ck->pos++;
}
static char *take_ident(Ck *ck) {
    char *r;
    if (cur(ck)->kind != TK_IDENT)
        ath_runtime_error_fmt("!^CAKE: expected an identifier", NULL);
    r = ck_strdup(cur(ck)->text);
    ck->pos++;
    return r;
}

/* ===================================================================== */
/* Measures (pre-scan) and lookup                                        */
/* ===================================================================== */

static int ck_find_measure(Ck *ck, const char *name, long *out) {
    int i;
    for (i = 0; i < ck->n_measures; i++)
        if (strcmp(ck->measures[i].name, name) == 0) { *out = ck->measures[i].value; return 1; }
    return 0;
}

static void ck_prescan_measures(Ck *ck) {
    int i;
    int cap = 0;
    ck->measures = NULL; ck->n_measures = 0;
    for (i = 0; i + 3 < ck->ntoks; i++) {
        if (is_ident(&ck->toks[i], "MEASURE")) {
            Tok *nm = &ck->toks[i + 1];
            Tok *eq = &ck->toks[i + 2];
            Tok *vl = &ck->toks[i + 3];
            if (nm->kind == TK_IDENT && is_punct(eq, "=") && vl->kind == TK_INT) {
                if (ck->n_measures >= cap) {
                    cap = cap == 0 ? 8 : cap * 2;
                    ck->measures = (CkMeasure *)realloc(ck->measures,
                        sizeof(CkMeasure) * (size_t)cap);
                    if (!ck->measures) ath_fatal("out of memory");
                }
                ck->measures[ck->n_measures].name = ck_strdup(nm->text);
                ck->measures[ck->n_measures].value = vl->ival;
                ck->n_measures++;
            }
        }
    }
}

static long ck_take_intval(Ck *ck) {
    Tok *t = cur(ck);
    long v;
    if (t->kind == TK_INT) { ck->pos++; return t->ival; }
    if (t->kind == TK_IDENT && ck_find_measure(ck, t->text, &v)) { ck->pos++; return v; }
    ath_runtime_error_fmt("!^CAKE: expected an integer or measure", NULL);
    return 0;
}

/* ===================================================================== */
/* Raw type parsing                                                      */
/* ===================================================================== */

static int ck_scalar_kw(const char *s, CkTypeTag *tag) {
    if (strcmp(s, "PINCH") == 0)   { *tag = CK_T_PINCH;   return 1; }
    if (strcmp(s, "DASH") == 0)    { *tag = CK_T_DASH;    return 1; }
    if (strcmp(s, "SPOON") == 0)   { *tag = CK_T_SPOON;   return 1; }
    if (strcmp(s, "CUP") == 0)     { *tag = CK_T_CUP;     return 1; }
    if (strcmp(s, "DROP") == 0)    { *tag = CK_T_DROP;    return 1; }
    if (strcmp(s, "DOLLOP") == 0)  { *tag = CK_T_DOLLOP;  return 1; }
    if (strcmp(s, "INTEGER") == 0) { *tag = CK_T_INTEGER; return 1; }
    if (strcmp(s, "BOOLEAN") == 0) { *tag = CK_T_BOOLEAN; return 1; }
    if (strcmp(s, "STRING") == 0)  { *tag = CK_T_STRING;  return 1; }
    if (strcmp(s, "RELIC") == 0)   { *tag = CK_T_RELIC;   return 1; }
    return 0;
}

static int ck_tag_is_fixed_int(CkTypeTag t) {
    return t == CK_T_PINCH || t == CK_T_DASH || t == CK_T_SPOON || t == CK_T_CUP;
}

static RawType *ck_parse_type(Ck *ck);

static RawType *ck_parse_element(Ck *ck) {
    RawType *rt = (RawType *)calloc(1, sizeof(RawType));
    if (!rt) ath_fatal("out of memory");
    if (is_ident(cur(ck), "CRUST")) {
        ck->pos++;
        expect_ident(ck, "OF");
        rt->kind = RT_CRUST;
        rt->named = take_ident(ck);
        return rt;
    }
    if (is_ident(cur(ck), "SIGNED")) {
        CkTypeTag tag;
        char *base;
        ck->pos++;
        base = take_ident(ck);
        if (!ck_scalar_kw(base, &tag) || !ck_tag_is_fixed_int(tag)) {
            free(base); free(rt);
            ath_runtime_error_fmt("!^CAKE: SIGNED is valid only on PINCH/DASH/SPOON/CUP", NULL);
        }
        free(base);
        rt->kind = RT_SCALAR; rt->scalar_tag = tag; rt->is_signed = 1;
        return rt;
    }
    {
        CkTypeTag tag;
        char *name = take_ident(ck);
        if (ck_scalar_kw(name, &tag)) {
            free(name);
            rt->kind = RT_SCALAR; rt->scalar_tag = tag; rt->is_signed = 0;
            return rt;
        }
        rt->kind = RT_NAMED; rt->named = name;
        return rt;
    }
}

static RawType *ck_parse_type(Ck *ck) {
    /* array := intval "OF" (element | "(" array ")") */
    int is_array = 0;
    if (cur(ck)->kind == TK_INT) is_array = 1;
    else if (cur(ck)->kind == TK_IDENT && !is_ident(cur(ck), "CRUST") &&
             !is_ident(cur(ck), "SIGNED") && is_ident(peekn(ck, 1), "OF"))
        is_array = 1;
    if (is_array) {
        RawType *rt = (RawType *)calloc(1, sizeof(RawType));
        if (!rt) ath_fatal("out of memory");
        rt->kind = RT_ARRAY;
        rt->count = ck_take_intval(ck);
        expect_ident(ck, "OF");
        if (is_punct(cur(ck), "(")) {
            ck->pos++;
            rt->elem = ck_parse_type(ck);
            expect_punct(ck, ")");
        } else {
            rt->elem = ck_parse_element(ck);
        }
        return rt;
    }
    return ck_parse_element(ck);
}

static void ck_free_rawtype(RawType *rt) {
    if (!rt) return;
    if (rt->named) free(rt->named);
    if (rt->elem) ck_free_rawtype(rt->elem);
    free(rt);
}

/* ===================================================================== */
/* Alchemy expression parsing                                            */
/* ===================================================================== */

static AlchemyNode *ck_parse_union_expr(Ck *ck);

static AlchemyNode *ck_alch_new(int op) {
    AlchemyNode *n = (AlchemyNode *)calloc(1, sizeof(AlchemyNode));
    if (!n) ath_fatal("out of memory");
    n->op = op;
    return n;
}

static AlchemyNode *ck_parse_atom(Ck *ck) {
    if (is_punct(cur(ck), "(")) {
        AlchemyNode *e;
        ck->pos++;
        e = ck_parse_union_expr(ck);
        expect_punct(ck, ")");
        return e;
    }
    {
        AlchemyNode *a = ck_alch_new(0);
        a->name = take_ident(ck);
        return a;
    }
}

static AlchemyNode *ck_parse_merge_expr(Ck *ck) {
    AlchemyNode *left = ck_parse_atom(ck);
    while (is_punct(cur(ck), "&&")) {
        AlchemyNode *n;
        ck->pos++;
        n = ck_alch_new(1);
        n->left = left; n->right = ck_parse_atom(ck);
        left = n;
    }
    return left;
}

static AlchemyNode *ck_parse_union_expr(Ck *ck) {
    AlchemyNode *left = ck_parse_merge_expr(ck);
    while (is_punct(cur(ck), "||")) {
        AlchemyNode *n;
        ck->pos++;
        n = ck_alch_new(2);
        n->left = left; n->right = ck_parse_merge_expr(ck);
        left = n;
    }
    return left;
}

static void ck_free_alch(AlchemyNode *n) {
    if (!n) return;
    if (n->name) free(n->name);
    ck_free_alch(n->left);
    ck_free_alch(n->right);
    free(n);
}

/* ===================================================================== */
/* Declaration parsing                                                   */
/* ===================================================================== */

static void ck_parse_recipe(Ck *ck) {
    RawRecipe rr;
    int cap = 0;
    memset(&rr, 0, sizeof(rr));
    rr.line = cur(ck)->line;
    while (is_ident(cur(ck), "DENSE") || is_ident(cur(ck), "IMPERIAL")) {
        if (is_ident(cur(ck), "DENSE")) rr.dense = 1; else rr.imperial = 1;
        ck->pos++;
    }
    expect_ident(ck, "RECIPE");
    rr.name = take_ident(ck);
    if (is_ident(cur(ck), "PUNCHED")) {
        ck->pos++;
        if (cur(ck)->kind != TK_STR)
            ath_runtime_error_fmt("!^CAKE: PUNCHED expects a string code", NULL);
        strncpy(rr.punch, cur(ck)->text, sizeof(rr.punch) - 1);
        rr.punch[sizeof(rr.punch) - 1] = '\0';
        rr.has_punch = 1;
        ck->pos++;
    }
    if (is_punct(cur(ck), "=")) {
        /* The alchemy form has no modifier slot: an alchemized recipe inherits
           DENSE / IMPERIAL from its operands, so spelling them here would be a
           claim the declaration cannot honour. */
        if (rr.dense || rr.imperial)
            ath_runtime_error_fmt("!^CAKE: DENSE / IMPERIAL may not be written on an alchemized RECIPE ('= expr;'); they are inherited from the operands", NULL);
        ck->pos++;
        rr.is_alchemy = 1;
        rr.alch = ck_parse_union_expr(ck);
        expect_punct(ck, ";");
    } else if (is_punct(cur(ck), "{")) {
        ck->pos++;
        while (!is_punct(cur(ck), "}")) {
            RawMember m;
            memset(&m, 0, sizeof(m));
            m.line = cur(ck)->line;
            if (is_ident(cur(ck), "RISE")) {
                ck->pos++;
                expect_ident(ck, "TO");
                m.is_rise = 1;
                m.rise_val = ck_take_intval(ck);
                expect_punct(ck, ";");
            } else if (is_ident(cur(ck), "INGREDIENT")) {
                ck->pos++;
                m.name = take_ident(ck);
                expect_punct(ck, ":");
                m.type = ck_parse_type(ck);
                expect_punct(ck, ";");
            } else {
                ath_runtime_error_fmt("!^CAKE: expected INGREDIENT or RISE", NULL);
            }
            if (rr.n_members >= cap) {
                cap = cap == 0 ? 8 : cap * 2;
                rr.members = (RawMember *)realloc(rr.members,
                    sizeof(RawMember) * (size_t)cap);
                if (!rr.members) ath_fatal("out of memory");
            }
            rr.members[rr.n_members++] = m;
        }
        expect_punct(ck, "}");
        if (is_punct(cur(ck), ";")) ck->pos++;
    } else {
        ath_runtime_error_fmt("!^CAKE: expected '{' or '=' after RECIPE name", NULL);
    }
    /* append */
    {
        int n = ck->n_recipes;
        ck->recipes = (RawRecipe *)realloc(ck->recipes,
            sizeof(RawRecipe) * (size_t)(n + 1));
        if (!ck->recipes) ath_fatal("out of memory");
        ck->recipes[n] = rr;
        ck->n_recipes = n + 1;
    }
}

static void ck_skip_measure(Ck *ck) {
    /* MEASURE name = int ; (already captured in pre-scan) */
    expect_ident(ck, "MEASURE");
    (void)take_ident(ck);
    expect_punct(ck, "=");
    (void)ck_take_intval(ck);
    expect_punct(ck, ";");
}

static void ck_parse_file(Ck *ck) {
    while (cur(ck)->kind != TK_EOF) {
        if (is_ident(cur(ck), "MEASURE")) ck_skip_measure(ck);
        else ck_parse_recipe(ck);
    }
}

/* ===================================================================== */
/* Resolution: raw recipes -> AthRecipe with layout + code               */
/* ===================================================================== */

static RawRecipe *ck_find_recipe(Ck *ck, const char *name) {
    int i;
    for (i = 0; i < ck->n_recipes; i++)
        if (strcmp(ck->recipes[i].name, name) == 0) return &ck->recipes[i];
    return NULL;
}

static AthRecipe *ck_resolve_recipe(Ck *ck, RawRecipe *rr);

static CkType *ck_resolve_type(Ck *ck, RawType *rt, int imperial) {
    CkType *t = (CkType *)calloc(1, sizeof(CkType));
    if (!t) ath_fatal("out of memory");
    switch (rt->kind) {
    case RT_SCALAR:
        t->tag = rt->scalar_tag;
        t->is_signed = rt->is_signed;
        ck_scalar_layout(t->tag, &t->size, &t->align);
        if (imperial) {
            if (t->tag == CK_T_INTEGER || t->tag == CK_T_BOOLEAN ||
                t->tag == CK_T_STRING || t->tag == CK_T_RELIC) {
                free(t);
                ath_runtime_error_fmt("!^CAKE: IMPERIAL recipe may not contain native scalars", NULL);
            }
        }
        break;
    case RT_NAMED: {
        RawRecipe *target = ck_find_recipe(ck, rt->named);
        AthRecipe *nr;
        if (!target) {
            char *nm = ck_strdup(rt->named);
            free(t);
            ath_runtime_error_fmt("!^CAKE: unknown recipe '%s'", nm);
        }
        nr = ck_resolve_recipe(ck, target); /* +1 */
        if (imperial && !nr->imperial) {
            ath_recipe_decref(nr); free(t);
            ath_runtime_error_fmt("!^CAKE: IMPERIAL recipe may only embed IMPERIAL recipes", NULL);
        }
        t->tag = CK_T_NESTED;
        t->nested = nr; /* take the +1 */
        t->size = nr->size;
        t->align = nr->align;
        break;
    }
    case RT_CRUST: {
        RawRecipe *target = ck_find_recipe(ck, rt->named);
        if (!target) {
            char *nm = ck_strdup(rt->named);
            free(t);
            ath_runtime_error_fmt("!^CAKE: crust references unknown recipe '%s'", nm);
        }
        if (imperial) {
            free(t);
            ath_runtime_error_fmt("!^CAKE: IMPERIAL recipe may not contain crusts", NULL);
        }
        t->tag = CK_T_CRUST;
        t->nested = NULL; /* pointee not embedded; breaks the souffle cycle */
        t->size = (int)sizeof(void *);
        t->align = (int)sizeof(void *);
        break;
    }
    case RT_ARRAY: {
        CkType *elem = ck_resolve_type(ck, rt->elem, imperial);
        int stride;
        if (rt->count < 0) {
            ck_type_free(elem); free(t);
            ath_runtime_error_fmt("!^CAKE: array count must be non-negative", NULL);
        }
        stride = align_up(elem->size, elem->align);
        t->tag = CK_T_ARRAY;
        t->array_count = rt->count;
        t->elem = elem;
        t->size = (int)(rt->count * stride);
        t->align = elem->align;
        break;
    }
    default:
        free(t);
        ath_runtime_error_fmt("!^CAKE: internal: bad raw type", NULL);
    }
    return t;
}

static int ck_is_pow2(long v) { return v > 0 && (v & (v - 1)) == 0; }

static void ck_layout_struct(AthRecipe *r) {
    int off = 0, align = 1, i;
    for (i = 0; i < r->n_ingredients; i++) {
        CkType *t = r->ingredients[i].type;
        int a = r->dense ? 1 : t->align;
        off = align_up(off, a);
        r->ingredients[i].offset = off;
        off += t->size;
        if (a > align) align = a;
    }
    if (r->rise_to > align) align = r->rise_to;
    r->align = align;
    r->size = align_up(off, align);
}

static void ck_build_struct_descriptor(AthRecipe *r, char out[9]) {
    Sbuf sb;
    int i;
    if (r->n_ingredients == 0) { strcpy(out, "00000000"); return; }
    sb_init(&sb);
    sb_puts(&sb, "S(e=");
    sb_putc_(&sb, r->imperial ? 'B' : 'H');
    sb_puts(&sb, ",p=");
    sb_putint(&sb, r->dense ? 1 : 0);
    sb_puts(&sb, ",a=");
    sb_putint(&sb, r->align);
    sb_puts(&sb, ",z=");
    sb_putint(&sb, r->size);
    sb_puts(&sb, "){");
    for (i = 0; i < r->n_ingredients; i++) {
        if (i > 0) sb_putc_(&sb, ';');
        sb_puts(&sb, r->ingredients[i].name);
        sb_putc_(&sb, ':');
        ck_typedesc(&sb, r->ingredients[i].type);
        sb_putc_(&sb, '@');
        sb_putint(&sb, r->ingredients[i].offset);
        sb_putc_(&sb, '+');
        sb_putint(&sb, r->ingredients[i].type->size);
    }
    sb_putc_(&sb, '}');
    ck_code_from_desc(sb.buf, out);
    free(sb.buf);
}

static void ck_build_union_descriptor(AthRecipe *r, char out[9]) {
    Sbuf sb;
    int i;
    sb_init(&sb);
    sb_puts(&sb, "U(e=H,p=0,a=");
    sb_putint(&sb, r->align);
    sb_puts(&sb, ",z=");
    sb_putint(&sb, r->size);
    sb_puts(&sb, "){");
    for (i = 0; i < r->n_arms; i++) {
        if (i > 0) sb_putc_(&sb, ';');
        sb_puts(&sb, r->arms[i].name);
        sb_putc_(&sb, '=');
        sb_puts(&sb, r->arms[i].code);
        sb_putc_(&sb, '@');
        sb_putint(&sb, r->arms[i].payload_offset);
    }
    sb_putc_(&sb, '}');
    ck_code_from_desc(sb.buf, out);
    free(sb.buf);
}

static AthRecipe *ck_build_struct(Ck *ck, RawRecipe *rr) {
    AthRecipe *r = ck_recipe_new(CK_KIND_STRUCT);
    int i, ncount = 0, idx = 0;
    r->dense = rr->dense;
    r->imperial = rr->imperial;
    r->bind_name = ck_strdup(rr->name);
    for (i = 0; i < rr->n_members; i++) if (!rr->members[i].is_rise) ncount++;
    if (ncount > 0) {
        r->ingredients = (CkIngredient *)calloc((size_t)ncount, sizeof(CkIngredient));
        if (!r->ingredients) ath_fatal("out of memory");
    }
    for (i = 0; i < rr->n_members; i++) {
        RawMember *m = &rr->members[i];
        if (m->is_rise) {
            if (!ck_is_pow2(m->rise_val)) {
                ath_recipe_decref(r);
                ath_runtime_error_fmt("!^CAKE: RISE TO value must be a positive power of two", NULL);
            }
            if ((int)m->rise_val > r->rise_to) r->rise_to = (int)m->rise_val;
            continue;
        }
        r->ingredients[idx].name = ck_strdup(m->name);
        r->ingredients[idx].is_reserved = (strcmp(m->name, "_") == 0);
        r->ingredients[idx].type = ck_resolve_type(ck, m->type, rr->imperial);
        r->n_ingredients = idx + 1;
        idx++;
    }
    /* duplicate-name check (excluding "_") */
    for (i = 0; i < r->n_ingredients; i++) {
        int j;
        if (r->ingredients[i].is_reserved) continue;
        for (j = i + 1; j < r->n_ingredients; j++) {
            if (!r->ingredients[j].is_reserved &&
                strcmp(r->ingredients[i].name, r->ingredients[j].name) == 0) {
                ath_recipe_decref(r);
                ath_runtime_error_fmt("!^CAKE: duplicate ingredient name '%s'",
                                      r->ingredients[i].name);
            }
        }
    }
    ck_layout_struct(r);
    ck_build_struct_descriptor(r, r->code);
    return r;
}

/* ----- alchemy evaluation ----- */

static AthRecipe *ck_eval_alch(Ck *ck, AlchemyNode *n);

static AthRecipe *ck_merge(AthRecipe *a, AthRecipe *b) {
    AthRecipe *r;
    int total = a->n_ingredients + b->n_ingredients;
    int idx = 0, i, j;
    /* The layout modifiers are part of each operand's identity, so the two
       cakes must have been baked the same way; merging a packed cake into an
       unpacked one (or a big-endian one into a host-order one) would silently
       relocate and byte-swap fields that the source declared otherwise. */
    if ((a->dense != 0) != (b->dense != 0))
        ath_runtime_error_fmt("!^CAKE: CURDLED: && operands disagree on DENSE (one recipe is packed, the other is not)", NULL);
    if ((a->imperial != 0) != (b->imperial != 0))
        ath_runtime_error_fmt("!^CAKE: CURDLED: && operands disagree on IMPERIAL (one recipe is big-endian, the other is host order)", NULL);
    r = ck_recipe_new(CK_KIND_STRUCT);
    r->dense = a->dense;
    r->imperial = a->imperial;
    /* RISE TO is a floor on alignment, not a packing/byte-order commitment, so
       the merged cake honours both operands' floors: the larger one wins. */
    r->rise_to = a->rise_to > b->rise_to ? a->rise_to : b->rise_to;
    if (total > 0) {
        r->ingredients = (CkIngredient *)calloc((size_t)total, sizeof(CkIngredient));
        if (!r->ingredients) ath_fatal("out of memory");
    }
    for (i = 0; i < a->n_ingredients; i++) {
        r->ingredients[idx].name = ck_strdup(a->ingredients[i].name);
        r->ingredients[idx].is_reserved = a->ingredients[i].is_reserved;
        r->ingredients[idx].type = ck_type_copy(a->ingredients[i].type);
        idx++;
    }
    r->n_ingredients = idx;
    for (j = 0; j < b->n_ingredients; j++) {
        CkIngredient *bi = &b->ingredients[j];
        int dup = 0;
        if (!bi->is_reserved) {
            for (i = 0; i < a->n_ingredients; i++) {
                if (!a->ingredients[i].is_reserved &&
                    strcmp(a->ingredients[i].name, bi->name) == 0) {
                    if (!ck_type_eq(a->ingredients[i].type, bi->type)) {
                        ath_recipe_decref(r);
                        ath_runtime_error_fmt("!^CAKE: CURDLED: field '%s' has incompatible types in &&",
                                              bi->name);
                    }
                    dup = 1;
                    break;
                }
            }
        }
        if (dup) continue;
        r->ingredients[idx].name = ck_strdup(bi->name);
        r->ingredients[idx].is_reserved = bi->is_reserved;
        r->ingredients[idx].type = ck_type_copy(bi->type);
        idx++;
        r->n_ingredients = idx;
    }
    ck_layout_struct(r);
    ck_build_struct_descriptor(r, r->code);
    return r;
}

/* append a recipe (taking its strong ref) into an arm list, flattening unions */
static void ck_collect_arms(AthRecipe *rec, CkArm **arms, int *n, int *cap) {
    if (rec->kind == CK_KIND_UNION) {
        int i;
        for (i = 0; i < rec->n_arms; i++) {
            if (*n >= *cap) {
                *cap = *cap == 0 ? 4 : *cap * 2;
                *arms = (CkArm *)realloc(*arms, sizeof(CkArm) * (size_t)*cap);
                if (!*arms) ath_fatal("out of memory");
            }
            (*arms)[*n].name = ck_strdup(rec->arms[i].name);
            (*arms)[*n].recipe = rec->arms[i].recipe;
            ath_recipe_incref(rec->arms[i].recipe);
            strcpy((*arms)[*n].code, rec->arms[i].code);
            (*arms)[*n].payload_offset = 0;
            (*n)++;
        }
        ath_recipe_decref(rec);
    } else {
        if (*n >= *cap) {
            *cap = *cap == 0 ? 4 : *cap * 2;
            *arms = (CkArm *)realloc(*arms, sizeof(CkArm) * (size_t)*cap);
            if (!*arms) ath_fatal("out of memory");
        }
        (*arms)[*n].name = ck_strdup(rec->bind_name ? rec->bind_name : "?");
        (*arms)[*n].recipe = rec; /* take the ref */
        strcpy((*arms)[*n].code, rec->code);
        (*arms)[*n].payload_offset = 0;
        (*n)++;
    }
}

static AthRecipe *ck_build_union(CkArm *arms, int n) {
    AthRecipe *r = ck_recipe_new(CK_KIND_UNION);
    int i, j, P = 1, payload_off, payload_size = 0, m;
    /* sort by code (alphabet index) */
    for (i = 0; i < n; i++)
        for (j = i + 1; j < n; j++)
            if (ck_code_cmp(arms[i].code, arms[j].code) > 0) {
                CkArm tmp = arms[i]; arms[i] = arms[j]; arms[j] = tmp;
            }
    /* collapse equal codes (drop later duplicates) */
    m = 0;
    for (i = 0; i < n; i++) {
        if (m > 0 && ck_code_cmp(arms[m - 1].code, arms[i].code) == 0) {
            if (arms[i].name) free(arms[i].name);
            ath_recipe_decref(arms[i].recipe);
            continue;
        }
        arms[m++] = arms[i];
    }
    n = m;
    if (n > 256)
        ath_runtime_error_fmt("!^CAKE: a marble cake may have at most 256 arms", NULL);
    for (i = 0; i < n; i++) {
        if (arms[i].recipe->align > P) P = arms[i].recipe->align;
        if (arms[i].recipe->size > payload_size) payload_size = arms[i].recipe->size;
    }
    payload_off = align_up(1, P);
    r->align = P;
    r->size = align_up(payload_off + payload_size, P);
    for (i = 0; i < n; i++) arms[i].payload_offset = payload_off;
    r->arms = arms;
    r->n_arms = n;
    ck_build_union_descriptor(r, r->code);
    return r;
}

static AthRecipe *ck_eval_alch(Ck *ck, AlchemyNode *node) {
    if (node->op == 0) {
        RawRecipe *target = ck_find_recipe(ck, node->name);
        if (!target)
            ath_runtime_error_fmt("!^CAKE: unknown recipe '%s'", node->name);
        return ck_resolve_recipe(ck, target);
    }
    if (node->op == 1) {
        AthRecipe *a = ck_eval_alch(ck, node->left);
        AthRecipe *b = ck_eval_alch(ck, node->right);
        AthRecipe *m;
        if (a->kind != CK_KIND_STRUCT || b->kind != CK_KIND_STRUCT) {
            ath_recipe_decref(a); ath_recipe_decref(b);
            ath_runtime_error_fmt("!^CAKE: && requires struct recipes (a union is not a valid && operand)", NULL);
        }
        m = ck_merge(a, b);
        ath_recipe_decref(a); ath_recipe_decref(b);
        return m;
    }
    {
        CkArm *arms = NULL; int n = 0, cap = 0;
        AthRecipe *u;
        ck_collect_arms(ck_eval_alch(ck, node->left), &arms, &n, &cap);
        ck_collect_arms(ck_eval_alch(ck, node->right), &arms, &n, &cap);
        u = ck_build_union(arms, n);
        return u;
    }
}

static AthRecipe *ck_resolve_recipe(Ck *ck, RawRecipe *rr) {
    AthRecipe *result;
    if (rr->resolved == 1) { ath_recipe_incref(rr->result); return rr->result; }
    if (rr->resolved == 2)
        ath_runtime_error_fmt("!^CAKE: COLLAPSED SOUFFLE: '%s' embeds itself by value", rr->name);
    rr->resolved = 2;
    if (rr->is_alchemy) {
        result = ck_eval_alch(ck, rr->alch);
        if (result->bind_name) free(result->bind_name);
        result->bind_name = ck_strdup(rr->name);
    } else {
        result = ck_build_struct(ck, rr);
    }
    if (rr->has_punch && strcmp(result->code, rr->punch) != 0) {
        char msg[160];
        sprintf(msg, "!^CAKE: STALE: '%s' is punched \"%s\" but computed \"%s\"",
                rr->name, rr->punch, result->code);
        ath_runtime_error_fmt("%s", msg);
    }
    rr->result = result;        /* memo holds one ref */
    rr->resolved = 1;
    ath_recipe_incref(result);  /* ref for the caller */
    return result;
}

/* ===================================================================== */
/* Public entry point                                                    */
/* ===================================================================== */

static char *ck_read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    long sz;
    char *buf;
    size_t got;
    if (!f) {
        ath_runtime_error_fmt("!^CAKE: cannot open '%s'", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) sz = 0;
    buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); ath_fatal("out of memory"); }
    got = fread(buf, 1, (size_t)sz, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

static void ck_cleanup(Ck *ck) {
    int i, j;
    for (i = 0; i < ck->ntoks; i++)
        if (ck->toks[i].text) free(ck->toks[i].text);
    free(ck->toks);
    for (i = 0; i < ck->n_measures; i++) free(ck->measures[i].name);
    if (ck->measures) free(ck->measures);
    for (i = 0; i < ck->n_recipes; i++) {
        RawRecipe *rr = &ck->recipes[i];
        if (rr->name) free(rr->name);
        for (j = 0; j < rr->n_members; j++) {
            if (rr->members[j].name) free(rr->members[j].name);
            ck_free_rawtype(rr->members[j].type);
        }
        if (rr->members) free(rr->members);
        ck_free_alch(rr->alch);
        if (rr->result) ath_recipe_decref(rr->result);
    }
    if (ck->recipes) free(ck->recipes);
}

AthValue ath_cake_load(const char *path) {
    Ck ck;
    char *src;
    AthMap *m;
    int i;
    long mv;

    memset(&ck, 0, sizeof(ck));
    ck.path = path;
    src = ck_read_file(path);
    ck_lex(&ck, src);
    free(src);
    ck_prescan_measures(&ck);
    ck_parse_file(&ck);

    /* resolve every recipe */
    for (i = 0; i < ck.n_recipes; i++) {
        AthRecipe *r = ck_resolve_recipe(&ck, &ck.recipes[i]);
        ath_recipe_decref(r); /* memo already holds a ref; drop the caller ref */
    }

    m = ath_map_new(8);
    for (i = 0; i < ck.n_recipes; i++) {
        AthValue rv = ath_recipe_val(ck.recipes[i].result);
        ath_recipe_incref(ck.recipes[i].result);
        ath_map_set(m, ck.recipes[i].name, rv);
        ath_recipe_decref(ck.recipes[i].result); /* map holds its own ref */
    }
    for (i = 0; i < ck.n_measures; i++) {
        if (ck_find_measure(&ck, ck.measures[i].name, &mv))
            ath_map_set(m, ck.measures[i].name, ath_int(mv));
    }

    ck_cleanup(&ck);
    return ath_module_val(m);
}
