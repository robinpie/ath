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

/* ath_scope.h -- lexical scope chain */
#ifndef ATH_SCOPE_H
#define ATH_SCOPE_H

#include "ath_value.h"

typedef struct AthBinding {
    const char *name;  /* interned -- owned by the intern table, never freed */
    AthValue    value;
    int         is_const;
} AthBinding;

#define ATH_SCOPE_INLINE 8

typedef struct AthScope {
    int           refcount;
    struct AthScope *parent;
    int           count;
    int           capacity;
    AthBinding   *bindings;
    /* small scopes (rite frames) live here, avoiding a malloc per frame */
    AthBinding    inline_bindings[ATH_SCOPE_INLINE];
    /* 1 on the builtins scope spliced above the program root; a define in
     * its direct child must not silently shadow a builtin */
    int           is_builtins;
} AthScope;

AthScope *ath_scope_new(AthScope *parent);
void      ath_scope_incref(AthScope *s);
void      ath_scope_decref(AthScope *s);
void      ath_scope_define(AthScope *s, const char *name, AthValue v, int is_const);
AthValue  ath_scope_get(AthScope *s, const char *name);
void      ath_scope_set(AthScope *s, const char *name, AthValue v);
int       ath_scope_has_local(AthScope *s, const char *name);

#endif /* ATH_SCOPE_H */
