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

/* fuzz_ops.c -- libFuzzer harness for the !~ATH runtime value API.
 *
 * The input is decoded as a little bytecode program over NREG value registers:
 * load constants/strings/integers, call built-in rites with register arguments,
 * apply the arithmetic/comparison/bitwise operators, index and index-assign,
 * build arrays and maps, create sylladices (with C-implemented hash/predicate
 * rites that can misbehave: wrong return types, LONG_MIN, raising, re-entering
 * the sylladex), CAPTCHALOGUE/EJECT with any modifier, BANISH buffers, and run
 * the !^CAKE builtins with fuzzer-chosen recipes, paths and buffer lengths.
 *
 * This is the same call surface generated C uses, with the same ownership
 * convention (every producer returns +1 except borrowed scope reads). Every
 * operation runs inside an error frame: a catchable runtime error is a normal
 * outcome. Findings are sanitizer reports, crashes, and abort()s from the cheap
 * oracles below (structural value invariants and a few round-trip identities).
 *
 * Reference cycles (arr[0] = arr) are possible through index-assign
 * exactly as in the language; stringifying them recurses forever, so the harness
 * refuses to store a container into a container (see no_cycles()).
 *
 * Build/run: make fuzz-ops-run (see fuzz/README.md).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "ath_runtime.h"
#include "ath_cake.h"
#include "ath_buffer.h"
#include "ath_sylladex.h"
#include "ath_builtins.h"

#define NREG       16
#define MAX_OPS    256
#define MAX_STR    256

/* ------------------------------------------------------------------ */
/* input reader                                                       */
/* ------------------------------------------------------------------ */

static const unsigned char *in_p;
static size_t in_n;

static unsigned u8(void) {
    if (!in_n) return 0;
    in_n--;
    return *in_p++;
}
static int reg(void) { return (int)(u8() % NREG); }

static const long interesting[] = {
    0, 1, -1, 2, 3, 7, 8, 15, 16, 31, 32, 63, 64, 127, 128, 255, 256, 65535, 65536,
    0x10FFFF, 0x110000, 0xD800, INT_MAX, INT_MIN, (long)INT_MAX + 1, LONG_MAX, LONG_MIN,
    4294967295L, 4294967296L, 4294967297L, -4294967296L, 1L << 30, (1L << 30) + 1
};

static long read_int(void) {
    unsigned k = u8();
    if (k < 128) return interesting[k % (sizeof interesting / sizeof interesting[0])];
    if (k < 192) return (long)(signed char)u8();
    {
        unsigned long v = 0;
        int i;
        for (i = 0; i < (int)sizeof(long); i++) v = (v << 8) | u8();
        return v <= (unsigned long)LONG_MAX ? (long)v : -(long)(~v) - 1;
    }
}

/* ------------------------------------------------------------------ */
/* registers + error frames                                           */
/* ------------------------------------------------------------------ */

static AthValue R[NREG];

/* Rough byte weight of a value, bounded in depth. Values that repeatedly
   stringify themselves into themselves grow exponentially; that is the program's
   fault, not the runtime's, so the harness drops anything past a budget. */
static long weight(AthValue v, int depth) {
    long w = 1;
    int i;
    if (depth > 3) return 1 << 20;
    switch (v.type) {
    case ATH_STRING: return v.as.string->length + 1;
    case ATH_BUFFER: return v.as.buffer->length + 1;
    case ATH_ARRAY:
        for (i = 0; i < v.as.array->length && w < (1L << 24); i++) w += weight(v.as.array->data[i], depth + 1);
        return w;
    case ATH_MAP: case ATH_MODULE:
        for (i = 0; i < v.as.map->capacity && w < (1L << 24); i++)
            if (v.as.map->entries[i].used)
                w += v.as.map->entries[i].key->length + weight(v.as.map->entries[i].value, depth + 1);
        return w;
    case ATH_SYLLADEX: return 16 * (ath_syl_count(v.as.sylladex) + 1);
    default: return 1;
    }
}
#define REG_BUDGET    (1L << 16)   /* a register may hold this much */
#define STORE_BUDGET  (1L << 10)   /* a value stored inside a container */

static void put(int dst, AthValue v) {
    ath_value_decref(R[dst]);
    if (weight(v, 0) > REG_BUDGET) {
        ath_value_decref(v);
        v = ath_void();
    }
    R[dst] = v;
}
static int storable(AthValue v) { return weight(v, 0) <= STORE_BUDGET; }

/* Run body(ctx) inside an error frame; returns 1 if it completed. */
typedef void (*Body)(void *ctx);
static int guarded(Body body, void *ctx) {
    AthErrorFrame ef;
    ATH_ATTEMPT_BEGIN(ef) {
        body(ctx);
        ATH_ATTEMPT_END(ef);
        return 1;
    } ATH_SALVAGE_BEGIN(ef) {
        if (!ef.error_msg) { fprintf(stderr, "error frame with no message\n"); abort(); }
        free(ef.error_msg);
        ATH_SALVAGE_END(ef);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* oracles                                                            */
/* ------------------------------------------------------------------ */

static void die(const char *what) {
    fprintf(stderr, "INVARIANT VIOLATED: %s\n", what);
    abort();
}

static void dump_str(const char *label, AthValue v) {
    int i;
    if (v.type != ATH_STRING) { fprintf(stderr, "%s: <%s>\n", label, ath_typeof_str(v)); return; }
    fprintf(stderr, "%s (%d bytes): \"", label, v.as.string->length);
    for (i = 0; i < v.as.string->length; i++) {
        unsigned char ch = (unsigned char)v.as.string->data[i];
        if (ch >= 0x20 && ch < 0x7f && ch != '\\' && ch != '"') fputc(ch, stderr);
        else fprintf(stderr, "\\x%02x", ch);
    }
    fprintf(stderr, "\"\n");
}

static void check_value(AthValue v, int depth) {
    int i;
    if (depth > 4) return;
    switch (v.type) {
    case ATH_STRING:
        if (v.as.string->refcount <= 0) die("string refcount <= 0");
        if (v.as.string->length < 0) die("negative string length");
        if (v.as.string->data[v.as.string->length] != '\0') die("string not NUL-terminated");
        break;
    case ATH_ARRAY:
        if (v.as.array->refcount <= 0) die("array refcount <= 0");
        if (v.as.array->length < 0 || v.as.array->length > v.as.array->capacity)
            die("array length outside [0, capacity]");
        for (i = 0; i < v.as.array->length && i < 8; i++) check_value(v.as.array->data[i], depth + 1);
        break;
    case ATH_MAP:
        if (v.as.map->refcount <= 0) die("map refcount <= 0");
        if (v.as.map->count < 0 || v.as.map->count > v.as.map->capacity) die("map count outside [0, capacity]");
        break;
    case ATH_BUFFER:
        if (v.as.buffer->refcount <= 0) die("buffer refcount <= 0");
        if (v.as.buffer->length < 0) die("negative buffer length");
        if ((v.as.buffer->bytes == NULL) != (v.as.buffer->length == 0)) die("buffer bytes/length disagree");
        break;
    case ATH_SYLLADEX:
        if (v.as.sylladex->refcount <= 0) die("sylladex refcount <= 0");
        if (ath_syl_count(v.as.sylladex) < 0) die("negative sylladex count");
        break;
    default:
        break;
    }
}

/* Does container `c` (transitively, bounded) hold a container at all? Storing a
   container inside another is allowed, but the harness only permits it when the
   stored value holds no containers itself, which rules out every cycle. */
static int holds_container(AthValue v) {
    return v.type == ATH_ARRAY || v.type == ATH_MAP || v.type == ATH_MODULE || v.type == ATH_SYLLADEX;
}
static int no_cycles(AthValue container, AthValue stored) {
    int i;
    if (!holds_container(container) || !holds_container(stored)) return 1;
    if (stored.type == ATH_ARRAY) {
        for (i = 0; i < stored.as.array->length; i++)
            if (holds_container(stored.as.array->data[i])) return 0;
        return stored.as.array != container.as.array;
    }
    if (stored.type == ATH_MAP || stored.type == ATH_MODULE) {
        for (i = 0; i < stored.as.map->capacity; i++)
            if (stored.as.map->entries[i].used && holds_container(stored.as.map->entries[i].value)) return 0;
        return stored.as.map != container.as.map;
    }
    return 0; /* a sylladex inside a container: its contents are not inspectable here */
}

/* ------------------------------------------------------------------ */
/* rites for HASHMAP / TECHHOP                                         */
/* ------------------------------------------------------------------ */

static unsigned rite_mode;      /* chosen by the input */
static int      reentry_depth;
static int      reentry_reg;    /* register a re-entrant predicate touches */

typedef struct { AthSylladex *s; unsigned mode; AthValue value; } Reentry;

static void reenter(void *vp) {
    Reentry *re = (Reentry *)vp;
    AthValue a = ath_int(0), b = ath_int(0), got;
    if (re->mode == 9) {
        got = ath_syl_eject(re->s, EJ_GROOVE_SHADE, &a, &b);
        ath_value_decref(got);
    } else if (re->mode == 10) {
        got = ath_syl_eject(re->s, EJ_SLOT, &a, NULL);
        ath_value_decref(got);
    } else {
        ath_syl_captchalogue(re->s, re->value, &a, NULL);
    }
}

static AthValue rite_body(int argc, AthValue *argv, int is_pred) {
    unsigned m = rite_mode % 12;
    long idx = (argc > 1 && argv[1].type == ATH_INTEGER) ? argv[1].as.integer : 0;
    switch (m) {
    case 0: return is_pred ? ath_bool(1) : ath_int(0);
    case 1: return is_pred ? ath_bool((idx & 1) == 0) : ath_int(LONG_MIN);
    case 2: return is_pred ? ath_bool(idx == 0) : ath_int(LONG_MAX);
    case 3: return ath_str_cstr("not a number");
    case 4: return ath_void();
    case 5: ath_runtime_error("rite raised", 0, 0); return ath_void();
    case 6: return ath_int(-1);
    case 7: return ath_int(INT_MIN);
    case 8: return is_pred ? ath_bool(0) : ath_int((long)INT_MAX + 7);
    default:
        /* re-enter: mutate a sylladex (possibly the one being operated on) */
        if (reentry_depth < 2 && R[reentry_reg].type == ATH_SYLLADEX) {
            Reentry re;
            re.s = R[reentry_reg].as.sylladex;
            re.mode = m;
            re.value = (argc > 0 && storable(argv[0]) && !holds_container(argv[0])) ? argv[0] : ath_void();
            ath_syl_incref(re.s);
            reentry_depth++;
            guarded(reenter, &re);   /* its errors stay inside: the ref above must be dropped */
            reentry_depth--;
            ath_syl_decref(re.s);
        }
        return is_pred ? ath_bool(1) : ath_int(3);
    }
}
static AthValue rite_hash(AthScope *s, int argc, AthValue *argv) { (void)s; return rite_body(argc, argv, 0); }
static AthValue rite_pred(AthScope *s, int argc, AthValue *argv) { (void)s; return rite_body(argc, argv, 1); }

static AthRite *hash_rite, *pred_rite;

/* ------------------------------------------------------------------ */
/* !^CAKE recipes (loaded once)                                        */
/* ------------------------------------------------------------------ */

static const char *SCHEMA =
    "RECIPE Point { INGREDIENT x: SIGNED SPOON; INGREDIENT y: SIGNED SPOON; }\n"
    "RECIPE Segment { INGREDIENT start: Point; INGREDIENT end: Point; }\n"
    "DENSE IMPERIAL RECIPE Packet { INGREDIENT magic: SPOON; INGREDIENT version: PINCH;"
    "  INGREDIENT _: PINCH; INGREDIENT seq: DASH; }\n"
    "RECIPE Named  { INGREDIENT id: SPOON; INGREDIENT name: 16 OF PINCH; }\n"
    "RECIPE Placed { INGREDIENT id: SPOON; INGREDIENT at: 2 OF SIGNED SPOON; }\n"
    "RECIPE Entity = Named && Placed;\n"
    "RECIPE Circle { INGREDIENT r: SPOON; }\n"
    "RECIPE Rect   { INGREDIENT w: SPOON; INGREDIENT h: SPOON; }\n"
    "RECIPE Shape = Circle || Rect;\n"
    "RECIPE Generic {}\n"
    "RECIPE Scalars { INGREDIENT a: PINCH; INGREDIENT b: DASH; INGREDIENT c: SPOON;"
    "  INGREDIENT d: CUP; INGREDIENT e: DROP; INGREDIENT f: DOLLOP; INGREDIENT g: BOOLEAN; }\n"
    "DENSE RECIPE Packed { INGREDIENT tag: PINCH; INGREDIENT data: CUP; }\n"
    "RECIPE Risen { INGREDIENT x: SPOON; RISE TO 16; }\n"
    "RECIPE Node { INGREDIENT value: SIGNED CUP; INGREDIENT next: CRUST OF Node; }\n"
    "RECIPE Grid { INGREDIENT cells: 3 OF (4 OF Point); INGREDIENT s: Shape; INGREDIENT tail: SIGNED PINCH; }\n"
    "IMPERIAL RECIPE Wire { INGREDIENT len: DASH; INGREDIENT body: 5 OF SIGNED SPOON; INGREDIENT q: DOLLOP; }\n"
    "RECIPE BigShape = Grid || Scalars || Packed || Wire;\n";

#define MAX_RECIPES 32
static AthValue recipes[MAX_RECIPES];
static int      n_recipes;
static AthValue cake_module;

int LLVMFuzzerInitialize(int *argc, char ***argv) {
    AthErrorFrame ef;
    int i;
    (void)argc; (void)argv;
    ATH_ATTEMPT_BEGIN(ef) {
        cake_module = ath_cake_load_source(SCHEMA, "fuzz_ops.^CAKE");
        ATH_ATTEMPT_END(ef);
    } ATH_SALVAGE_BEGIN(ef) {
        fprintf(stderr, "fuzz_ops: embedded schema failed to load: %s\n", ef.error_msg);
        abort();
        ATH_SALVAGE_END(ef);
    }
    for (i = 0; i < cake_module.as.map->capacity && n_recipes < MAX_RECIPES; i++) {
        AthMapEntry *e = &cake_module.as.map->entries[i];
        if (e->used && e->value.type == ATH_RECIPE) recipes[n_recipes++] = e->value;
    }
    if (n_recipes == 0) { fprintf(stderr, "fuzz_ops: no recipes\n"); abort(); }
    hash_rite = ath_rite_new_sync(NULL, rite_hash, -1);
    pred_rite = ath_rite_new_sync(NULL, rite_pred, -1);
    return 0;
}

/* path strings the dictionary alone would rarely assemble */
static const char *PATHS[] = {
    "x", "y", "start.x", "end.y", "magic", "version", "_", "seq", "name", "name.0",
    "name.15", "name.16", "at.1", "at.2", "FLAVOR", "Circle", "Circle.r", "Rect.h",
    "cells.2.3.y", "cells.3.0.x", "s.Rect.w", "s.FLAVOR", "tail", "Grid.cells.0.0.x",
    "Scalars.f", "Packed.data", "a", "d", "e", "f", "g", "data", "value", "next",
    "id", "Named", "", ".", "x.", ".x", "name.-1", "name.+1", "name. 1", "name.0x1",
    "name.99999999999999999999999", "s.Circle.r.x", "cells.0", "cells.0.0"
};

/* ------------------------------------------------------------------ */
/* operations                                                          */
/* ------------------------------------------------------------------ */

typedef AthValue (*Builtin)(AthScope *, int, AthValue *);
static const Builtin BUILTINS[] = {
    ath_builtin_TYPEOF, ath_builtin_LENGTH, ath_builtin_COUNT, ath_builtin_PARSE_INT,
    ath_builtin_PARSE_FLOAT, ath_builtin_STRING, ath_builtin_INT, ath_builtin_FLOAT,
    ath_builtin_CHAR, ath_builtin_CODE, ath_builtin_BIN, ath_builtin_HEX,
    ath_builtin_APPEND, ath_builtin_PREPEND, ath_builtin_SLICE, ath_builtin_FIRST,
    ath_builtin_LAST, ath_builtin_CONCAT, ath_builtin_KEYS, ath_builtin_VALUES,
    ath_builtin_HAS, ath_builtin_SET, ath_builtin_DELETE, ath_builtin_SPLIT,
    ath_builtin_JOIN, ath_builtin_SUBSTRING, ath_builtin_UPPERCASE, ath_builtin_LOWERCASE,
    ath_builtin_TRIM, ath_builtin_REPLACE, ath_builtin_RANDOM, ath_builtin_RANDOM_INT,
    ath_builtin_BUFFER, ath_builtin_BYTE_AT, ath_builtin_SET_BYTE, ath_builtin_BUFFER_TO_STRING,
    ath_builtin_STRING_TO_BUFFER, ath_builtin_RECKON,
    ath_builtin_CAPTCHA, ath_builtin_SIZEOF, ath_builtin_BAKE, ath_builtin_SPRINKLE,
    ath_builtin_SCOOP, ath_builtin_FLAVOR, ath_builtin_PLATE, ath_builtin_TASTE, ath_builtin_UNPLATE
};
#define N_BUILTINS ((int)(sizeof BUILTINS / sizeof BUILTINS[0]))

typedef AthValue (*Binop)(AthValue, AthValue);
static const Binop BINOPS[] = {
    ath_add, ath_sub, ath_mul, ath_div, ath_mod, ath_band, ath_bor, ath_bxor,
    ath_lshift, ath_rshift, ath_eq, ath_ne, ath_lt, ath_gt, ath_le, ath_ge
};
#define N_BINOPS ((int)(sizeof BINOPS / sizeof BINOPS[0]))

typedef struct {
    int op, dst, a, b, c, k, argc;
    int args[4];
    AthValue tmp;         /* owned scratch value, released by the caller */
    char str[MAX_STR + 1];
    int  str_len;
} Ctx;

static void do_call(void *vp) {
    Ctx *x = (Ctx *)vp;
    AthValue argv[4];
    int i;
    for (i = 0; i < x->argc; i++) argv[i] = R[x->args[i]];
    /* no giant buffers: gigabyte allocations are slow under ASan and prove nothing */
    if (BUILTINS[x->k % N_BUILTINS] == ath_builtin_BUFFER && x->argc == 1 &&
        argv[0].type == ATH_INTEGER && argv[0].as.integer > REG_BUDGET && argv[0].as.integer <= (long)INT_MAX)
        return;
    put(x->dst, BUILTINS[x->k % N_BUILTINS](NULL, x->argc, argv));
}

static void do_binop(void *vp) {
    Ctx *x = (Ctx *)vp;
    put(x->dst, BINOPS[x->k % N_BINOPS](R[x->a], R[x->b]));
}

static void do_unop(void *vp) {
    Ctx *x = (Ctx *)vp;
    switch (x->k % 6) {
    case 0: put(x->dst, ath_neg(R[x->a])); break;
    case 1: put(x->dst, ath_bnot(R[x->a])); break;
    case 2: put(x->dst, ath_bool(ath_is_truthy(R[x->a]))); break;
    case 3: {
        char *s = ath_stringify(R[x->a]);
        if (!s) die("ath_stringify returned NULL");
        put(x->dst, ath_str_cstr(s));
        free(s);
        break;
    }
    case 4: put(x->dst, ath_str_cstr(ath_typeof_str(R[x->a]))); break;
    default: put(x->dst, ath_value_copy(R[x->a])); break;
    }
}

static void do_index(void *vp) {
    Ctx *x = (Ctx *)vp;
    if (!storable(R[x->b])) return;   /* a map lookup stringifies the key */
    put(x->dst, ath_index(R[x->a], R[x->b]));
}

static void do_index_set(void *vp) {
    Ctx *x = (Ctx *)vp;
    /* the key is stored too (maps stringify it), and the container grows in place, past put()'s budget check */
    if (!no_cycles(R[x->a], R[x->c % NREG]) || !storable(R[x->c % NREG]) || !storable(R[x->b])) return;
    if (weight(R[x->a], 0) > REG_BUDGET) return;
    ath_index_set(R[x->a], R[x->b], R[x->c % NREG]);
}

static void do_member(void *vp) {
    Ctx *x = (Ctx *)vp;
    if (x->k & 1) {
        if (!no_cycles(R[x->a], R[x->c % NREG]) || !storable(R[x->c % NREG])) return;
        if (weight(R[x->a], 0) > REG_BUDGET) return;
        ath_member_set(R[x->a], x->str, R[x->c % NREG]);
    } else {
        put(x->dst, ath_member(R[x->a], x->str));
    }
}

static void do_array(void *vp) {
    Ctx *x = (Ctx *)vp;
    AthArray *arr = ath_array_new(x->argc);
    int i;
    for (i = 0; i < x->argc; i++) {
        AthValue v = R[x->args[i]];
        if (holds_container(v) || !storable(v)) v = ath_int(i);  /* keep containers one level deep and small */
        ath_value_incref(v);
        arr->data[i] = v;
    }
    arr->length = x->argc;
    put(x->dst, ath_array_val(arr));
}

static void do_syl_new(void *vp) {
    Ctx *x = (Ctx *)vp;
    int n = ath_clamp_int(R[x->a].type == ATH_INTEGER ? R[x->a].as.integer : (long)(x->k % 9));
    int m = ath_clamp_int(R[x->b].type == ATH_INTEGER ? R[x->b].as.integer : (long)(x->c % 5) + 1);
    /* no multi-gigabyte slot arrays: those are an allocation, not a bug */
    if (n > 4096) n = (n & 0xFFF);
    if (m > 4096) m = (m & 0xFFF);
    switch (x->op % 8) {
    case 0: put(x->dst, ath_sylladex_val(ath_syl_stack_new(n))); break;
    case 1: put(x->dst, ath_sylladex_val(ath_syl_queue_new(n))); break;
    case 2: put(x->dst, ath_sylladex_val(ath_syl_tree_new(n & 1))); break;
    case 3: put(x->dst, ath_sylladex_val(ath_syl_hashmap_new(n, (x->k & 16) ? hash_rite : NULL))); break;
    case 4: put(x->dst, ath_sylladex_val(ath_syl_ouija_new(n))); break;
    case 5: put(x->dst, ath_sylladex_val(ath_syl_bottle_new(n))); break;
    case 6:
        if ((long)n * (long)m > 4096) { n = (n % 64) + 1; m = (m % 64) + 1; }  /* keep the grid small */
        put(x->dst, ath_sylladex_val(ath_syl_techhop_new(n, m, pred_rite, pred_rite)));
        break;
    default: put(x->dst, ath_sylladex_val(ath_syl_juju_new(n))); break;
    }
}

static void do_captcha(void *vp) {
    Ctx *x = (Ctx *)vp;
    AthSylladex *s;
    if (R[x->dst].type != ATH_SYLLADEX) ath_runtime_error("not a sylladex", 0, 0);
    s = R[x->dst].as.sylladex;
    if (!no_cycles(R[x->dst], R[x->a]) || !storable(R[x->a]) || !storable(R[x->b])) return;
    ath_syl_incref(s);   /* the operation may drop the register's reference via a re-entrant rite */
    x->tmp = ath_sylladex_val(s);
    ath_syl_captchalogue(s, R[x->a],
                         (x->k & 1) ? &R[x->b] : NULL,
                         (x->k & 2) ? &R[x->c % NREG] : NULL);
}

static void do_eject(void *vp) {
    Ctx *x = (Ctx *)vp;
    AthSylladex *s;
    AthValue got;
    if (R[x->a].type != ATH_SYLLADEX) ath_runtime_error("not a sylladex", 0, 0);
    s = R[x->a].as.sylladex;
    ath_syl_incref(s);
    x->tmp = ath_sylladex_val(s);
    got = ath_syl_eject(s, (AthEjectMod)(x->k % 6),
                        (x->k & 8) ? &R[x->b] : NULL,
                        (x->k & 16) ? &R[x->c % NREG] : NULL);
    put(x->dst, got);
}

static void do_cake(void *vp) {
    Ctx *x = (Ctx *)vp;
    AthValue argv[4];
    AthValue rec = recipes[x->k % n_recipes];
    AthValue path = ath_str_val(ath_string_new(x->str, x->str_len));
    x->tmp = path;
    argv[0] = R[x->a]; argv[1] = rec; argv[2] = path; argv[3] = R[x->b];
    switch (x->op % 5) {
    case 0: put(x->dst, ath_builtin_SCOOP(NULL, 3, argv)); break;
    case 1: put(x->dst, ath_builtin_SPRINKLE(NULL, 4, argv)); break;
    case 2: put(x->dst, ath_buffer_val(ath_buffer_new(x->c % 256))); break;  /* a buffer of any length */
    case 3: argv[1] = rec; put(x->dst, ath_builtin_BAKE(NULL, 1, &rec)); break;
    default: {
        AthValue pv[2];
        pv[0] = R[x->a]; pv[1] = rec;
        put(x->dst, ((x->c & 1) ? ath_builtin_UNPLATE : ath_builtin_FLAVOR)(NULL, 2, pv));
        break;
    }
    }
}

/* identities that must hold whenever both halves succeed */
static void do_roundtrip(void *vp) {
    Ctx *x = (Ctx *)vp;
    AthValue v = R[x->a], r1, r2, argv[2];
    switch (x->k % 3) {
    case 0: /* CODE(CHAR(cp)) == cp for 0 < cp <= 0x10FFFF */
        if (v.type != ATH_INTEGER || v.as.integer <= 0 || v.as.integer > 0x10FFFF) return;
        r1 = ath_builtin_CHAR(NULL, 1, &v);
        x->tmp = r1;
        r2 = ath_builtin_CODE(NULL, 1, &r1);
        if (r2.type != ATH_INTEGER || r2.as.integer != v.as.integer) die("CODE(CHAR(cp)) != cp");
        break;
    case 1: /* BUFFER_TO_STRING(STRING_TO_BUFFER(s)) == s */
        if (v.type != ATH_STRING) return;
        r1 = ath_builtin_STRING_TO_BUFFER(NULL, 1, &v);
        x->tmp = r1;
        r2 = ath_builtin_BUFFER_TO_STRING(NULL, 1, &r1);
        if (r2.type != ATH_STRING || r2.as.string->length != v.as.string->length ||
            memcmp(r2.as.string->data, v.as.string->data, (size_t)v.as.string->length) != 0) {
            dump_str("s", v); dump_str("round trip", r2);
            die("BUFFER_TO_STRING(STRING_TO_BUFFER(s)) != s");
        }
        ath_value_decref(r2);
        break;
    default: /* JOIN(SPLIT(s, d), d) == s for a non-empty delimiter */
        if (v.type != ATH_STRING || R[x->b].type != ATH_STRING || R[x->b].as.string->length == 0) return;
        argv[0] = v; argv[1] = R[x->b];
        r1 = ath_builtin_SPLIT(NULL, 2, argv);
        x->tmp = r1;
        argv[0] = r1;
        r2 = ath_builtin_JOIN(NULL, 2, argv);
        if (r2.type != ATH_STRING || r2.as.string->length != v.as.string->length ||
            memcmp(r2.as.string->data, v.as.string->data, (size_t)v.as.string->length) != 0) {
            dump_str("s", v); dump_str("d", R[x->b]); dump_str("JOIN(SPLIT(s, d), d)", r2);
            die("JOIN(SPLIT(s, d), d) != s");
        }
        ath_value_decref(r2);
        break;
    }
}

int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size) {
    int i, ops = 0;
    int mark;

    in_p = data; in_n = size;
    for (i = 0; i < NREG; i++) R[i] = ath_int(i);
    srand(1);
    rite_mode = 0;
    reentry_depth = 0;
    mark = ath_sink_mark();

    while (in_n && ops++ < MAX_OPS) {
        Ctx x;
        Body body = NULL;
        unsigned opc = u8();
        memset(&x, 0, sizeof x);
        x.tmp = ath_void();
        x.op = (int)u8(); x.dst = reg(); x.a = reg(); x.b = reg(); x.c = (int)u8(); x.k = (int)u8();
        switch (opc % 16) {
        case 0: put(x.dst, ath_int(read_int())); break;
        case 1: {
            double d;
            unsigned char raw[sizeof(double)];
            int j;
            for (j = 0; j < (int)sizeof raw; j++) raw[j] = (unsigned char)u8();
            memcpy(&d, raw, sizeof d);
            put(x.dst, ath_float(d));
            break;
        }
        case 2: {
            int len = (int)(u8() % (MAX_STR + 1)), j;
            for (j = 0; j < len && in_n; j++) x.str[j] = (char)u8();
            put(x.dst, ath_str_val(ath_string_new(x.str, j)));
            break;
        }
        case 3:
            switch (x.k % 7) {
            case 0: put(x.dst, ath_void()); break;
            case 1: put(x.dst, ath_bool(x.c & 1)); break;
            case 2: put(x.dst, ath_map_val(ath_map_new(x.c % 8))); break;
            case 3: ath_recipe_incref(recipes[x.c % n_recipes].as.recipe);
                    put(x.dst, recipes[x.c % n_recipes]); break;
            case 4: ath_rite_incref(hash_rite); put(x.dst, ath_rite_val(hash_rite)); break;
            case 5: rite_mode = (unsigned)x.c; reentry_reg = x.a; break;
            default: ath_value_incref(R[x.a]); put(x.dst, R[x.a]); break;  /* alias */
            }
            break;
        case 4:
            x.k = (int)(u8() % N_BUILTINS);
            x.argc = (int)(u8() % 5);
            for (i = 0; i < x.argc; i++) x.args[i] = reg();
            body = do_call;
            break;
        case 5: body = do_binop; break;
        case 6: body = do_unop; break;
        case 7: body = do_index; break;
        case 8: body = do_index_set; break;
        case 9: {
            int len = (int)(u8() % 16), j;
            for (j = 0; j < len && in_n; j++) x.str[j] = (char)u8();
            x.str[j] = '\0';
            body = do_member;
            break;
        }
        case 10:
            x.argc = (int)(u8() % 5);
            for (i = 0; i < x.argc; i++) x.args[i] = reg();
            body = do_array;
            break;
        case 11: body = do_syl_new; break;
        case 12: x.dst = reg(); body = do_captcha; break;
        case 13: body = do_eject; break;
        case 14: {
            unsigned pick = u8();
            if (pick < 200) {
                const char *p = PATHS[pick % (sizeof PATHS / sizeof PATHS[0])];
                x.str_len = (int)strlen(p);
                memcpy(x.str, p, (size_t)x.str_len);
            } else {
                int len = (int)(u8() % 64), j;
                for (j = 0; j < len && in_n; j++) x.str[j] = (char)u8();
                x.str_len = j;
            }
            body = do_cake;
            break;
        }
        default:
            if (x.k & 64) {
                if (R[x.a].type == ATH_BUFFER) ath_buffer_release(R[x.a].as.buffer);  /* BANISH */
            } else {
                body = do_roundtrip;
            }
            break;
        }
        if (body) guarded(body, &x);
        ath_value_decref(x.tmp);
        ath_sink_flush(mark);
        if (_ath_error_top != NULL) die("error frame stack not unwound");
        check_value(R[x.dst], 0);
        check_value(R[x.a], 0);
    }

    for (i = 0; i < NREG; i++) { ath_value_decref(R[i]); R[i] = ath_void(); }
    ath_sink_flush(mark);
    return 0;
}
