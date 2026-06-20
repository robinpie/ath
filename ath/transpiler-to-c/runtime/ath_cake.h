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

/* ath_cake.h -- the !^CAKE schema engine.
   A !^CAKE recipe describes a C-compatible memory layout. A `.^CAKE` file is
   parsed at runtime (ath_cake_load) into recipe values (ATH_RECIPE) and integer
   measures, returned as an ATH_MODULE. Recipes feed the BAKE/SCOOP/SPRINKLE...
   builtins and the recipe-typed by-value FFI. The two parts that every
   implementation must reproduce bit-for-bit are the layout algorithm and the
   captchalogue-code canonicalization; both live in ath_cake.c. */
#ifndef ATH_CAKE_H
#define ATH_CAKE_H

#include "ath_value.h"

typedef enum {
    CK_KIND_STRUCT = 0,   /* ordered ingredients */
    CK_KIND_UNION  = 1    /* FLAVOR tag + arms (a marble cake) */
} CkRecipeKind;

/* Resolved ingredient element type. Arrays wrap an element type. */
typedef enum {
    CK_T_PINCH = 0, CK_T_DASH, CK_T_SPOON, CK_T_CUP,  /* fixed-width ints */
    CK_T_DROP, CK_T_DOLLOP,                           /* fixed-width floats */
    CK_T_INTEGER, CK_T_BOOLEAN, CK_T_STRING, CK_T_RELIC, /* native scalars */
    CK_T_NESTED,   /* by-value recipe: ->nested holds the layout */
    CK_T_CRUST,    /* pointer to a recipe: ptr-sized, pointee not embedded */
    CK_T_ARRAY     /* ->array_count of ->elem */
} CkTypeTag;

typedef struct CkType {
    CkTypeTag        tag;
    int              is_signed;     /* fixed-width ints only */
    struct AthRecipe *nested;       /* CK_T_NESTED: strong ref to the embedded recipe */
    long             array_count;   /* CK_T_ARRAY */
    struct CkType   *elem;          /* CK_T_ARRAY element type (owned) */
    int              size;          /* resolved bytes (array: count * stride) */
    int              align;
} CkType;

typedef struct CkIngredient {
    char    *name;          /* "_" allowed and repeatable */
    CkType  *type;          /* owned */
    int      offset;        /* byte offset within the recipe (or union payload) */
    int      is_reserved;   /* name == "_": inaccessible to SCOOP/SPRINKLE */
} CkIngredient;

typedef struct CkArm {
    char             *name;   /* the arm recipe's binding name; used for paths */
    struct AthRecipe *recipe; /* strong; the arm's own struct layout */
    char              code[9];/* arm captchalogue code; arms are sorted by this */
    int               payload_offset;
} CkArm;

typedef struct AthRecipe {
    int            refcount;
    CkRecipeKind   kind;
    char          *bind_name;     /* the RECIPE name; NOT hashed (may be NULL) */

    CkIngredient  *ingredients;   /* struct recipes */
    int            n_ingredients;

    CkArm         *arms;          /* union recipes; sorted by code ascending */
    int            n_arms;

    int            dense;         /* packed: every alignment forced to 1 */
    int            imperial;      /* big-endian fixed-width scalars */
    int            rise_to;       /* 0 = none; else min alignment (power of two) */

    int            size;          /* total byte size of a baked instance */
    int            align;         /* alignment */
    char           code[9];       /* 8 captchalogue chars + NUL */
} AthRecipe;

void ath_recipe_incref(AthRecipe *r);
void ath_recipe_decref(AthRecipe *r);

/* Parse + analyze a `.^CAKE` file. Returns an ATH_MODULE mapping each recipe
   name to an ATH_RECIPE value and each measure name to an ATH_INTEGER. Raises a
   catchable runtime error (CURDLED / COLLAPSED SOUFFLE / STALE / parse error)
   on failure. */
AthValue ath_cake_load(const char *path);

/* Build a libffi struct ffi_type from a recipe layout, for by-value FFI. The
   returned ffi_type (and its elements array) is heap-allocated; free it with
   ath_cake_ffi_type_free. Returns NULL and raises a runtime error if the recipe
   cannot be a host-ABI FFI type (IMPERIAL byte order, or a mandated alignment
   the host ABI does not reproduce). Declared here but defined in ath_ffi.c so
   the WASM stub TU does not pull in libffi. */
struct ath_cake_ffi_type;

#endif /* ATH_CAKE_H */
