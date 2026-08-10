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

/* ath_cake_builtins.c -- the !~ATH-side built-in rites that operate on !^CAKE
   recipes: CAPTCHA, SIZEOF, BAKE, SPRINKLE, SCOOP, FLAVOR, PLATE, TASTE,
   UNPLATE. A baked recipe instance is a BUFFER (an exactly-sized C struct).
   Byte assembly assumes a little-endian host for host-order (non-IMPERIAL)
   fields -- true of every documented target (x86-64, i686, wasm32, win64). */

#include "ath_builtins.h"
#include "ath_cake.h"
#include "ath_buffer.h"
#include "ath_relic.h"
#include "ath_error.h"
#include <stdlib.h>
#include <string.h>

/* ===================================================================== */
/* Path resolution                                                       */
/* ===================================================================== */

typedef struct {
    int        valid;
    int        reserved;     /* path hit a reserved '_' ingredient (RAW) */
    int        is_flavor;    /* leaf is the union FLAVOR tag (1-byte @ offset) */
    int        offset;       /* byte offset into the buffer */
    CkType    *leaf;         /* leaf scalar/array/nested/crust type, or NULL */
    AthRecipe *leaf_recipe;  /* non-NULL: leaf is a whole recipe region (a union arm) */
    int        imperial;     /* effective endianness for the leaf */
} CkPath;

static int ck_stride(CkType *elem) {
    int a = elem->align;
    if (a <= 1) return elem->size;
    return ((elem->size + a - 1) / a) * a;
}

static CkPath ck_resolve_path(AthRecipe *top, const char *path) {
    CkPath out;
    AthRecipe *rec = top;
    CkType *type = NULL;
    int offset = 0;
    int imperial = top->imperial;
    const char *p = path;
    char seg[128];

    out.valid = 0; out.reserved = 0; out.is_flavor = 0;
    out.offset = 0; out.leaf = NULL; out.leaf_recipe = NULL; out.imperial = 0;

    if (!path || !*path) return out;

    for (;;) {
        int i = 0, a, found;
        while (*p && *p != '.') { if (i < 127) seg[i++] = *p; p++; }
        seg[i] = '\0';
        if (i == 0) return out;

        /* A nested-recipe field transparently becomes its recipe when another
           segment follows. */
        if (type && type->tag == CK_T_NESTED) {
            rec = type->nested; type = NULL; imperial = rec->imperial;
        }

        if (type == NULL) {
            if (rec->kind == CK_KIND_UNION) {
                if (strcmp(seg, "FLAVOR") == 0) {
                    out.is_flavor = 1;
                    out.offset = offset;
                    out.valid = (*p == '\0');
                    return out;
                }
                found = 0;
                for (a = 0; a < rec->n_arms; a++) {
                    if (strcmp(rec->arms[a].name, seg) == 0) {
                        offset += rec->arms[a].payload_offset;
                        rec = rec->arms[a].recipe;
                        type = NULL;
                        imperial = rec->imperial;
                        found = 1;
                        break;
                    }
                }
                if (!found) return out;
            } else {
                found = 0;
                for (a = 0; a < rec->n_ingredients; a++) {
                    if (strcmp(rec->ingredients[a].name, seg) == 0) {
                        if (rec->ingredients[a].is_reserved) {
                            out.reserved = 1;
                            return out;
                        }
                        offset += rec->ingredients[a].offset;
                        type = rec->ingredients[a].type;
                        rec = NULL;
                        found = 1;
                        break;
                    }
                }
                if (!found) return out;
            }
        } else if (type->tag == CK_T_ARRAY) {
            char *end;
            long k = strtol(seg, &end, 10);
            if (*end != '\0') return out;
            if (k < 0 || k >= type->array_count) return out;
            offset += (int)k * ck_stride(type->elem);
            type = type->elem;
        } else {
            /* scalar/crust but more path remains */
            return out;
        }

        if (*p == '\0') break;
        p++; /* consume '.' */
    }

    if (type == NULL && rec != NULL) {
        /* landed on a union arm name directly: whole-arm region */
        out.leaf_recipe = rec;
        out.offset = offset;
        out.valid = 1;
        return out;
    }
    out.offset = offset;
    out.leaf = type;
    out.imperial = imperial;
    out.valid = 1;
    return out;
}

/* ===================================================================== */
/* Scalar load/store (LE host for non-IMPERIAL; BE for IMPERIAL)         */
/* ===================================================================== */

/* A field may be wider than the host's long -- CUP is 8 bytes on every target,
   but long is 4 on i686/Windows/wasm. Only the low sizeof(long) bytes are ever
   shifted (a shift of >= the type's width is undefined), and the bytes above
   them carry the value's sign extension, so a narrow-long host reads and writes
   exactly the bytes a wide-long host would. */

static long ck_load_int(const unsigned char *base, int width, int is_signed, int imperial) {
    int low = width < (int)sizeof(long) ? width : (int)sizeof(long);
    unsigned long v = 0;
    int i;
    for (i = 0; i < low; i++) {
        int b = imperial ? base[width - 1 - i] : base[i];
        v |= ((unsigned long)b) << (8 * i);
    }
    if (is_signed && width < (int)sizeof(long)) {
        unsigned long sign = 1UL << (width * 8 - 1);
        if (v & sign) v |= ~((sign << 1) - 1);
    }
    if (width > (int)sizeof(long)) {
        /* The discarded high bytes must be exactly the sign extension of the
           long we are about to return; anything else does not fit an !~ATH
           INTEGER here, and truncating it silently would corrupt the value. */
        unsigned char fill =
            (v & (1UL << (sizeof(long) * 8 - 1))) ? (unsigned char)0xFF : (unsigned char)0x00;
        for (i = low; i < width; i++) {
            int b = imperial ? base[width - 1 - i] : base[i];
            if ((unsigned char)b != fill)
                ath_runtime_error_fmt(
                    "RAW: %d-byte field holds a value too wide for this platform's INTEGER",
                    width);
        }
    }
    return (long)v;
}

static void ck_store_int(unsigned char *base, int width, long v, int imperial) {
    int low = width < (int)sizeof(long) ? width : (int)sizeof(long);
    unsigned long uv = (unsigned long)v;
    unsigned char fill = v < 0 ? (unsigned char)0xFF : (unsigned char)0x00;
    int i;
    for (i = 0; i < width; i++) {
        unsigned char b = i < low
            ? (unsigned char)((uv >> (8 * i)) & 0xFFUL)
            : fill;
        if (imperial) base[width - 1 - i] = b;
        else base[i] = b;
    }
}

static double ck_load_float(const unsigned char *base, int width, int imperial) {
    unsigned char tmp[8];
    int i;
    for (i = 0; i < width; i++)
        tmp[i] = imperial ? base[width - 1 - i] : base[i];
    if (width == 4) { float f; memcpy(&f, tmp, 4); return (double)f; }
    { double d; memcpy(&d, tmp, 8); return d; }
}

static void ck_store_float(unsigned char *base, int width, double val, int imperial) {
    unsigned char tmp[8];
    int i;
    if (width == 4) { float f = (float)val; memcpy(tmp, &f, 4); }
    else { memcpy(tmp, &val, 8); }
    for (i = 0; i < width; i++) {
        if (imperial) base[width - 1 - i] = tmp[i];
        else base[i] = tmp[i];
    }
}

static int ck_is_int_tag(CkTypeTag t) {
    return t == CK_T_PINCH || t == CK_T_DASH || t == CK_T_SPOON || t == CK_T_CUP ||
           t == CK_T_INTEGER || t == CK_T_BOOLEAN;
}
static int ck_is_float_tag(CkTypeTag t) { return t == CK_T_DROP || t == CK_T_DOLLOP; }
static int ck_is_ptr_tag(CkTypeTag t) {
    return t == CK_T_STRING || t == CK_T_RELIC || t == CK_T_CRUST;
}
static int ck_tag_signed(CkType *t) {
    if (t->tag == CK_T_INTEGER) return 1;
    if (t->tag == CK_T_BOOLEAN) return 0;
    return t->is_signed;
}

/* ===================================================================== */
/* Argument helpers                                                      */
/* ===================================================================== */

static AthRecipe *ck_arg_recipe(AthValue v, const char *name) {
    if (v.type != ATH_RECIPE)
        ath_runtime_error_fmt("%s: expected a RECIPE, got %s", name, ath_typeof_str(v));
    return v.as.recipe;
}

static AthBuffer *ck_arg_buffer(AthValue v, const char *name) {
    if (v.type != ATH_BUFFER)
        ath_runtime_error_fmt("%s: expected a BUFFER, got %s", name, ath_typeof_str(v));
    return v.as.buffer;
}

/* ===================================================================== */
/* CAPTCHA / SIZEOF / BAKE                                               */
/* ===================================================================== */

AthValue ath_builtin_CAPTCHA(AthScope *s, int argc, AthValue *argv) {
    (void)s;
    if (argc != 1) ath_runtime_error_fmt("CAPTCHA requires 1 argument, got %d", argc);
    return ath_str_cstr(ck_arg_recipe(argv[0], "CAPTCHA")->code);
}

AthValue ath_builtin_SIZEOF(AthScope *s, int argc, AthValue *argv) {
    (void)s;
    if (argc != 1) ath_runtime_error_fmt("SIZEOF requires 1 argument, got %d", argc);
    return ath_int(ck_arg_recipe(argv[0], "SIZEOF")->size);
}

AthValue ath_builtin_BAKE(AthScope *s, int argc, AthValue *argv) {
    AthRecipe *r;
    (void)s;
    if (argc != 1) ath_runtime_error_fmt("BAKE requires 1 argument, got %d", argc);
    r = ck_arg_recipe(argv[0], "BAKE");
    return ath_buffer_val(ath_buffer_new(r->size));
}

/* ===================================================================== */
/* FLAVOR / PLATE / TASTE / UNPLATE                                      */
/* ===================================================================== */

AthValue ath_builtin_FLAVOR(AthScope *s, int argc, AthValue *argv) {
    AthBuffer *b;
    AthRecipe *r;
    (void)s;
    if (argc != 2) ath_runtime_error_fmt("FLAVOR requires 2 arguments, got %d", argc);
    b = ck_arg_buffer(argv[0], "FLAVOR");
    r = ck_arg_recipe(argv[1], "FLAVOR");
    if (r->kind != CK_KIND_UNION)
        ath_runtime_error("RAW: FLAVOR requires a union recipe", 0, 0);
    if (!b->bytes || b->length < 1)
        ath_runtime_error("RAW: buffer too short for FLAVOR tag", 0, 0);
    return ath_int((long)b->bytes[0]);
}

AthValue ath_builtin_PLATE(AthScope *s, int argc, AthValue *argv) {
    AthBuffer *b, *out;
    AthRecipe *r;
    (void)s;
    if (argc != 2) ath_runtime_error_fmt("PLATE requires 2 arguments, got %d", argc);
    b = ck_arg_buffer(argv[0], "PLATE");
    r = ck_arg_recipe(argv[1], "PLATE");
    out = ath_buffer_new(8 + b->length);
    memcpy(out->bytes, r->code, 8);
    if (b->length > 0) memcpy(out->bytes + 8, b->bytes, (size_t)b->length);
    return ath_buffer_val(out);
}

AthValue ath_builtin_TASTE(AthScope *s, int argc, AthValue *argv) {
    AthBuffer *b;
    (void)s;
    if (argc != 1) ath_runtime_error_fmt("TASTE requires 1 argument, got %d", argc);
    b = ck_arg_buffer(argv[0], "TASTE");
    if (!b->bytes || b->length < 8)
        ath_runtime_error("RAW: plated buffer too short to TASTE", 0, 0);
    return ath_str_val(ath_string_new((const char *)b->bytes, 8));
}

AthValue ath_builtin_UNPLATE(AthScope *s, int argc, AthValue *argv) {
    AthBuffer *b, *out;
    AthRecipe *r;
    int body_len;
    (void)s;
    if (argc != 2) ath_runtime_error_fmt("UNPLATE requires 2 arguments, got %d", argc);
    b = ck_arg_buffer(argv[0], "UNPLATE");
    r = ck_arg_recipe(argv[1], "UNPLATE");
    if (!b->bytes || b->length < 8)
        ath_runtime_error("RAW: plated buffer too short to UNPLATE", 0, 0);
    if (memcmp(b->bytes, r->code, 8) != 0)
        ath_runtime_error_fmt("STALE: plated code does not match recipe %s", r->code);
    body_len = b->length - 8;
    if (body_len > r->size)
        ath_runtime_error("OVERBAKED: plated body longer than recipe", 0, 0);
    if (body_len < r->size)
        ath_runtime_error("RAW: plated body shorter than recipe", 0, 0);
    out = ath_buffer_new(body_len);
    if (body_len > 0) memcpy(out->bytes, b->bytes + 8, (size_t)body_len);
    return ath_buffer_val(out);
}

/* ===================================================================== */
/* SCOOP / SPRINKLE                                                      */
/* ===================================================================== */

static void ck_check_bounds(AthBuffer *b, int offset, int width) {
    if (offset < 0 || offset + width > b->length || !b->bytes)
        ath_runtime_error("RAW: field access out of buffer bounds", 0, 0);
}

AthValue ath_builtin_SCOOP(AthScope *s, int argc, AthValue *argv) {
    AthBuffer *b;
    AthRecipe *r;
    const char *path;
    CkPath rp;
    (void)s;
    if (argc != 3) ath_runtime_error_fmt("SCOOP requires 3 arguments, got %d", argc);
    b = ck_arg_buffer(argv[0], "SCOOP");
    r = ck_arg_recipe(argv[1], "SCOOP");
    if (argv[2].type != ATH_STRING)
        ath_runtime_error("SCOOP: path must be a STRING", 0, 0);
    path = argv[2].as.string->data;

    rp = ck_resolve_path(r, path);
    if (rp.reserved)
        ath_runtime_error("RAW: reserved ingredient is inaccessible", 0, 0);
    if (!rp.valid)
        ath_runtime_error_fmt("RAW: unknown or ill-typed path '%s'", path);

    if (rp.is_flavor) {
        ck_check_bounds(b, rp.offset, 1);
        return ath_int((long)b->bytes[rp.offset]);
    }
    if (rp.leaf_recipe) {
        AthBuffer *out = ath_buffer_new(rp.leaf_recipe->size);
        ck_check_bounds(b, rp.offset, rp.leaf_recipe->size);
        if (rp.leaf_recipe->size > 0)
            memcpy(out->bytes, b->bytes + rp.offset, (size_t)rp.leaf_recipe->size);
        return ath_buffer_val(out);
    }
    {
        CkType *t = rp.leaf;
        ck_check_bounds(b, rp.offset, t->size);
        if (t->tag == CK_T_ARRAY || t->tag == CK_T_NESTED) {
            AthBuffer *out = ath_buffer_new(t->size);
            if (t->size > 0) memcpy(out->bytes, b->bytes + rp.offset, (size_t)t->size);
            return ath_buffer_val(out);
        }
        if (ck_is_int_tag(t->tag))
            return ath_int(ck_load_int(b->bytes + rp.offset, t->size,
                                       ck_tag_signed(t), rp.imperial));
        if (ck_is_float_tag(t->tag))
            return ath_float(ck_load_float(b->bytes + rp.offset, t->size, rp.imperial));
        if (ck_is_ptr_tag(t->tag)) {
            void *ptr = NULL;
            memcpy(&ptr, b->bytes + rp.offset, sizeof(void *));
            return ath_relic_val(ath_relic_new(ptr, NULL, NULL)); /* loose relic */
        }
    }
    ath_runtime_error("RAW: unscoopable field", 0, 0);
    return ath_void();
}

AthValue ath_builtin_SPRINKLE(AthScope *s, int argc, AthValue *argv) {
    AthBuffer *b;
    AthRecipe *r;
    const char *path;
    AthValue val;
    CkPath rp;
    (void)s;
    if (argc != 4) ath_runtime_error_fmt("SPRINKLE requires 4 arguments, got %d", argc);
    b = ck_arg_buffer(argv[0], "SPRINKLE");
    r = ck_arg_recipe(argv[1], "SPRINKLE");
    if (argv[2].type != ATH_STRING)
        ath_runtime_error("SPRINKLE: path must be a STRING", 0, 0);
    path = argv[2].as.string->data;
    val = argv[3];

    rp = ck_resolve_path(r, path);
    if (rp.reserved)
        ath_runtime_error("RAW: reserved ingredient is inaccessible", 0, 0);
    if (!rp.valid)
        ath_runtime_error_fmt("RAW: unknown or ill-typed path '%s'", path);

    if (rp.is_flavor) {
        ck_check_bounds(b, rp.offset, 1);
        if (val.type != ATH_INTEGER)
            ath_runtime_error("RAW: FLAVOR must be set to an INTEGER", 0, 0);
        b->bytes[rp.offset] = (unsigned char)(val.as.integer & 0xFF);
        return ath_void();
    }
    if (rp.leaf_recipe) {
        if (val.type != ATH_BUFFER || val.as.buffer->length != rp.leaf_recipe->size)
            ath_runtime_error("RAW: nested write needs a matching BUFFER", 0, 0);
        ck_check_bounds(b, rp.offset, rp.leaf_recipe->size);
        if (rp.leaf_recipe->size > 0)
            memcpy(b->bytes + rp.offset, val.as.buffer->bytes, (size_t)rp.leaf_recipe->size);
        return ath_void();
    }
    {
        CkType *t = rp.leaf;
        ck_check_bounds(b, rp.offset, t->size);
        if (t->tag == CK_T_ARRAY || t->tag == CK_T_NESTED) {
            if (val.type != ATH_BUFFER || val.as.buffer->length != t->size)
                ath_runtime_error("RAW: nested/array write needs a matching BUFFER", 0, 0);
            if (t->size > 0)
                memcpy(b->bytes + rp.offset, val.as.buffer->bytes, (size_t)t->size);
            return ath_void();
        }
        if (ck_is_int_tag(t->tag)) {
            long n;
            if (val.type != ATH_INTEGER && val.type != ATH_BOOLEAN)
                ath_runtime_error("RAW: integer field needs an INTEGER", 0, 0);
            n = val.as.integer;
            /* range check -- only fields narrower than long can overflow; a
               field at least that wide holds every long (the store sign-extends
               across any bytes above sizeof(long)), so it rejects nothing. */
            if (t->size >= (int)sizeof(long)) {
                /* always in range */
            } else if (ck_tag_signed(t)) {
                long lo = -(1L << (t->size * 8 - 1));
                long hi = (1L << (t->size * 8 - 1)) - 1;
                if (n < lo || n > hi)
                    ath_runtime_error("RAW: value does not fit signed field", 0, 0);
            } else {
                unsigned long umax = (1UL << (t->size * 8)) - 1;
                if (n < 0 || (unsigned long)n > umax)
                    ath_runtime_error("RAW: value does not fit unsigned field", 0, 0);
            }
            ck_store_int(b->bytes + rp.offset, t->size, n, rp.imperial);
            return ath_void();
        }
        if (ck_is_float_tag(t->tag)) {
            double d;
            if (val.type == ATH_FLOAT) d = val.as.float_;
            else if (val.type == ATH_INTEGER) d = (double)val.as.integer;
            else { ath_runtime_error("RAW: float field needs a FLOAT", 0, 0); return ath_void(); }
            ck_store_float(b->bytes + rp.offset, t->size, d, rp.imperial);
            return ath_void();
        }
        if (ck_is_ptr_tag(t->tag)) {
            void *ptr = NULL;
            if (val.type != ATH_RELIC)
                ath_runtime_error("RAW: pointer field needs a RELIC", 0, 0);
            if (val.as.relic && !val.as.relic->cursed) ptr = val.as.relic->ptr;
            memcpy(b->bytes + rp.offset, &ptr, sizeof(void *));
            return ath_void();
        }
    }
    ath_runtime_error("RAW: unsprinklable field", 0, 0);
    return ath_void();
}
