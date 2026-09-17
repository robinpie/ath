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

/* fuzz_cake.c -- libFuzzer harness for the !^CAKE schema engine.
 *
 * Each input is a .^CAKE source text. The harness:
 *   1. parses/lays it out with ath_cake_load_source inside an error frame
 *      (a catchable schema error is a normal outcome, not a finding);
 *   2. checks layout invariants on every exported recipe (abort() on violation):
 *      power-of-two alignment, size % align == 0, every field inside the recipe,
 *      struct offsets non-decreasing, union arms sorted strictly by code,
 *      a well-formed 8-char code, and 00000000 only for the empty recipe;
 *   3. bakes each (reasonably sized) recipe and SPRINKLEs a distinct value
 *      into every reachable scalar leaf, then SCOOPs them all back: any
 *      mismatch means two paths alias the same bytes. Every enumerated path
 *      must resolve (a RAW error on one is a finding), one-past-the-end array
 *      indices must be rejected, and PLATE/UNPLATE must round-trip.
 *
 * Build/run: see fuzz/README.md (make -C .. fuzz-cake).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ath_runtime.h"
#include "ath_cake.h"
#include "ath_buffer.h"

#define MAX_BAKE      (1 << 20)   /* skip the builtin pass for recipes bigger than this */
#define MAX_LEAVES    512
#define PATH_MAX_LEN  512

static void fail(const char *what, const AthRecipe *r) {
    fprintf(stderr, "INVARIANT VIOLATED: %s (recipe %s code %.8s size %d align %d)\n",
            what, r && r->bind_name ? r->bind_name : "?", r ? r->code : "?",
            r ? r->size : -1, r ? r->align : -1);
    abort();
}

static int is_pow2(int v) { return v > 0 && (v & (v - 1)) == 0; }

/* index of a code character in the captchalogue alphabet, or -1 */
static int code_idx(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return 10 + (c - 'A');
    if (c >= 'a' && c <= 'z') return 36 + (c - 'a');
    if (c == '?') return 62;
    if (c == '!') return 63;
    return -1;
}

static int code_cmp(const char *a, const char *b) {
    int i;
    for (i = 0; i < 8; i++) {
        int d = code_idx(a[i]) - code_idx(b[i]);
        if (d) return d;
    }
    return 0;
}

static void check_type(const CkType *t, const AthRecipe *owner) {
    if (!is_pow2(t->align)) fail("type alignment not a power of two", owner);
    if (t->size < 0) fail("negative type size", owner);
    if (t->tag == CK_T_ARRAY) {
        long stride;
        if (!t->elem) fail("array without element type", owner);
        check_type(t->elem, owner);
        stride = ((long)t->elem->size + t->elem->align - 1) / t->elem->align * t->elem->align;
        if (t->array_count < 0) fail("negative array count", owner);
        if ((long)t->size != t->array_count * stride) fail("array size != count * stride", owner);
        if (t->align != t->elem->align) fail("array align != element align", owner);
    } else if (t->tag == CK_T_NESTED) {
        if (!t->nested) fail("nested type without recipe", owner);
        if (t->size != t->nested->size) fail("nested size != recipe size", owner);
    }
}

static void check_recipe(const AthRecipe *r) {
    int i;
    for (i = 0; i < 8; i++)
        if (code_idx(r->code[i]) < 0) fail("code has a character outside the alphabet", r);
    if (r->code[8] != '\0') fail("code not NUL-terminated", r);
    if (!is_pow2(r->align)) fail("recipe alignment not a power of two", r);
    if (r->size < 0 || r->size % r->align != 0) fail("size not a multiple of alignment", r);
    if (r->rise_to && r->align < r->rise_to) fail("alignment below RISE TO", r);

    if (r->kind == CK_KIND_STRUCT) {
        int prev_end = 0;
        int is_empty = (r->n_ingredients == 0);
        if (is_empty != (memcmp(r->code, "00000000", 8) == 0))
            fail("00000000 must be exactly the empty recipe", r);
        for (i = 0; i < r->n_ingredients; i++) {
            const CkIngredient *ing = &r->ingredients[i];
            check_type(ing->type, r);
            if (ing->offset < prev_end) fail("ingredient overlaps the previous one", r);
            if ((long)ing->offset + ing->type->size > r->size) fail("ingredient extends past recipe size", r);
            if (!r->dense && ing->offset % ing->type->align != 0) fail("non-dense ingredient misaligned", r);
            prev_end = ing->offset + ing->type->size;
        }
    } else {
        if (r->n_arms < 1 || r->n_arms > 256) fail("union arm count out of range", r);
        if (r->size < 1) fail("union smaller than its FLAVOR tag", r);
        if (memcmp(r->code, "00000000", 8) == 0) fail("union has the generic code", r);
        for (i = 0; i < r->n_arms; i++) {
            const CkArm *arm = &r->arms[i];
            if (arm->payload_offset < 1) fail("union payload overlaps FLAVOR tag", r);
            if ((long)arm->payload_offset + arm->recipe->size > r->size) fail("union arm extends past size", r);
            if (memcmp(arm->code, arm->recipe->code, 8) != 0) fail("arm code differs from arm recipe code", r);
            if (i > 0 && code_cmp(r->arms[i - 1].code, arm->code) >= 0) fail("union arms not strictly sorted by code", r);
            check_recipe(arm->recipe);
        }
    }
}

/* ---------------- builtin pass ---------------- */

typedef struct { char path[PATH_MAX_LEN]; CkTypeTag tag; int is_signed; } Leaf;
typedef struct { Leaf leaves[MAX_LEAVES]; int n; } LeafSet;

static void add_type_leaves(LeafSet *ls, const char *prefix, const CkType *t, int depth);

static void add_recipe_leaves(LeafSet *ls, const char *prefix, const AthRecipe *r, int depth) {
    int i;
    char buf[PATH_MAX_LEN];
    if (depth > 16) return;
    if (r->kind == CK_KIND_UNION) {
        /* only the first arm: arms alias by design, so filling several would be a false positive */
        if (r->n_arms > 0 && r->arms[0].name) {
            if (snprintf(buf, sizeof buf, "%s%s", prefix, r->arms[0].name) >= (int)sizeof buf) return;
            if (!strchr(r->arms[0].name, '.')) {
                char sub[PATH_MAX_LEN];
                if (snprintf(sub, sizeof sub, "%s.", buf) >= (int)sizeof sub) return;
                add_recipe_leaves(ls, sub, r->arms[0].recipe, depth + 1);
            }
        }
        return;
    }
    for (i = 0; i < r->n_ingredients; i++) {
        const CkIngredient *ing = &r->ingredients[i];
        if (ing->is_reserved) continue;
        if (snprintf(buf, sizeof buf, "%s%s", prefix, ing->name) >= (int)sizeof buf) continue;
        add_type_leaves(ls, buf, ing->type, depth + 1);
    }
}

static void add_type_leaves(LeafSet *ls, const char *path, const CkType *t, int depth) {
    char buf[PATH_MAX_LEN];
    if (ls->n >= MAX_LEAVES || depth > 16 || t->size == 0) return;
    if (t->tag == CK_T_ARRAY) {
        long idx[2]; int k, nidx;
        idx[0] = 0; idx[1] = t->array_count - 1;
        nidx = (t->array_count > 1) ? 2 : 1;
        for (k = 0; k < nidx; k++) {
            if (snprintf(buf, sizeof buf, "%s.%ld", path, idx[k]) >= (int)sizeof buf) return;
            add_type_leaves(ls, buf, t->elem, depth + 1);
        }
    } else if (t->tag == CK_T_NESTED) {
        if (snprintf(buf, sizeof buf, "%s.", path) >= (int)sizeof buf) return;
        add_recipe_leaves(ls, buf, t->nested, depth + 1);
    } else {
        Leaf *l = &ls->leaves[ls->n++];
        strcpy(l->path, path);
        l->tag = t->tag;
        l->is_signed = t->is_signed;
    }
}

static int is_ptr(CkTypeTag t) { return t == CK_T_STRING || t == CK_T_RELIC || t == CK_T_CRUST; }
static int is_float(CkTypeTag t) { return t == CK_T_DROP || t == CK_T_DOLLOP; }

/* A distinct in-range value for leaf k (every integer field holds 1..100; BOOLEAN only 0/1). */
static long leaf_int(const Leaf *l, int k) {
    if (l->tag == CK_T_BOOLEAN) return k & 1;
    return (long)(k % 100) + 1;
}

/* Call a builtin inside an error frame. Returns 1 on success (result in *out, +1 ref), 0 if it raised. */
static int call(AthValue (*fn)(AthScope *, int, AthValue *), int argc, AthValue *argv, AthValue *out) {
    AthErrorFrame ef;
    AthValue v;
    ATH_ATTEMPT_BEGIN(ef) {
        v = fn(NULL, argc, argv);
        ATH_ATTEMPT_END(ef);
        *out = v;
        return 1;
    } ATH_SALVAGE_BEGIN(ef) {
        free(ef.error_msg);
        ATH_SALVAGE_END(ef);
    }
    *out = ath_void();
    return 0;
}

static void exercise_recipe(AthValue rv) {
    AthRecipe *r = rv.as.recipe;
    static LeafSet ls;
    AthValue buf, res, args[4];
    int i;
    if (r->size > MAX_BAKE) return;

    if (!call(ath_builtin_BAKE, 1, &rv, &buf)) fail("BAKE raised", r);
    if (buf.type != ATH_BUFFER || buf.as.buffer->length != r->size) fail("BAKE size mismatch", r);

    ls.n = 0;
    add_recipe_leaves(&ls, "", r, 0);

    if (r->kind == CK_KIND_UNION) {
        args[0] = buf; args[1] = rv; args[2] = ath_str_val(ath_string_from_cstr("FLAVOR"));
        args[3] = ath_int(0);
        if (!call(ath_builtin_SPRINKLE, 4, args, &res)) fail("SPRINKLE FLAVOR raised", r);
        ath_value_decref(res); ath_value_decref(args[2]);
    }

    /* write every leaf */
    for (i = 0; i < ls.n; i++) {
        const Leaf *l = &ls.leaves[i];
        args[0] = buf; args[1] = rv;
        args[2] = ath_str_val(ath_string_from_cstr(l->path));
        if (is_ptr(l->tag)) {
            args[3] = ath_void(); /* no loose-relic constructor here: SCOOP-only below */
            ath_value_decref(args[2]);
            continue;
        }
        args[3] = is_float(l->tag) ? ath_float((double)leaf_int(l, i)) : ath_int(leaf_int(l, i));
        if (!call(ath_builtin_SPRINKLE, 4, args, &res)) {
            fprintf(stderr, "path: %s\n", l->path);
            fail("SPRINKLE raised on an enumerated in-range path", r);
        }
        ath_value_decref(res);
        ath_value_decref(args[2]);
    }
    /* read every leaf back: a mismatch means two distinct paths share bytes */
    for (i = 0; i < ls.n; i++) {
        const Leaf *l = &ls.leaves[i];
        args[0] = buf; args[1] = rv;
        args[2] = ath_str_val(ath_string_from_cstr(l->path));
        if (!call(ath_builtin_SCOOP, 3, args, &res)) {
            fprintf(stderr, "path: %s\n", l->path);
            fail("SCOOP raised on an enumerated path", r);
        }
        if (!is_ptr(l->tag)) {
            int ok = is_float(l->tag)
                ? (res.type == ATH_FLOAT && res.as.float_ == (double)leaf_int(l, i))
                : (res.type == ATH_INTEGER && res.as.integer == leaf_int(l, i));
            if (!ok) { fprintf(stderr, "path: %s\n", l->path); fail("SCOOP did not return the SPRINKLEd value (aliasing fields?)", r); }
        }
        ath_value_decref(res);
        ath_value_decref(args[2]);
    }

    /* PLATE / UNPLATE round trip */
    args[0] = buf; args[1] = rv;
    if (!call(ath_builtin_PLATE, 2, args, &res)) fail("PLATE raised", r);
    {
        AthValue un, a2[2];
        a2[0] = res; a2[1] = rv;
        if (!call(ath_builtin_UNPLATE, 2, a2, &un)) fail("UNPLATE of own PLATE raised", r);
        if (un.type != ATH_BUFFER || un.as.buffer->length != r->size ||
            (r->size && memcmp(un.as.buffer->bytes, buf.as.buffer->bytes, (size_t)r->size) != 0))
            fail("UNPLATE(PLATE(b)) != b", r);
        ath_value_decref(un);
    }
    ath_value_decref(res);

    /* bad paths must be rejected, not crash */
    {
        static const char *bad[] = { "", ".", "..", "FLAVOR.x", "_", "0", "-1", "x.-1",
                                     "x.99999999999999999999", "x.", ".x" };
        size_t k;
        for (k = 0; k < sizeof bad / sizeof bad[0]; k++) {
            args[0] = buf; args[1] = rv;
            args[2] = ath_str_val(ath_string_from_cstr(bad[k]));
            if (call(ath_builtin_SCOOP, 3, args, &res)) ath_value_decref(res);
            ath_value_decref(args[2]);
        }
    }
    ath_value_decref(buf);
}

int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size) {
    char *src;
    AthErrorFrame ef;
    AthValue mod = ath_void();
    int loaded = 0;
    int i;

    if (size > 64 * 1024) return 0;
    src = (char *)malloc(size + 1);
    if (!src) return 0;
    memcpy(src, data, size);
    src[size] = '\0';

    ATH_ATTEMPT_BEGIN(ef) {
        mod = ath_cake_load_source(src, "fuzz.^CAKE");
        loaded = 1;
        ATH_ATTEMPT_END(ef);
    } ATH_SALVAGE_BEGIN(ef) {
        free(ef.error_msg);
        ATH_SALVAGE_END(ef);
    }
    free(src);
    if (!loaded) return 0;

    for (i = 0; i < mod.as.map->capacity; i++) {
        AthMapEntry *e = &mod.as.map->entries[i];
        if (!e->used || e->value.type != ATH_RECIPE) continue;
        check_recipe(e->value.as.recipe);
        exercise_recipe(e->value);
    }
    ath_value_decref(mod);
    return 0;
}
