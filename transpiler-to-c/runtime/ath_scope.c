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

/* ath_scope.c */
#include "ath_scope.h"
#include "ath_error.h"
#include <stdlib.h>
#include <string.h>

AthScope *ath_scope_new(AthScope *parent) {
    AthScope *s = (AthScope *)malloc(sizeof(AthScope));
    if (!s) ath_fatal("out of memory");
    s->refcount = 1;
    s->parent   = parent;
    if (parent) parent->refcount++;
    s->count    = 0;
    s->capacity = 8;
    s->bindings = (AthBinding *)malloc(sizeof(AthBinding) * s->capacity);
    if (!s->bindings) ath_fatal("out of memory");
    return s;
}

void ath_scope_incref(AthScope *s) { if (s) s->refcount++; }

void ath_scope_decref(AthScope *s) {
    int i;
    if (!s) return;
    if (--s->refcount > 0) return;
    for (i = 0; i < s->count; i++) {
        free((char*)s->bindings[i].name);
        ath_value_decref(s->bindings[i].value);
    }
    free(s->bindings);
    if (s->parent) ath_scope_decref(s->parent);
    free(s);
}

void ath_scope_define(AthScope *s, const char *name, AthValue v, int is_const) {
    int i;
    /* Check if already defined in this scope (allow re-define for ~ATH re-imports) */
    for (i = 0; i < s->count; i++) {
        if (strcmp(s->bindings[i].name, name) == 0) {
            if (s->bindings[i].is_const)
                ath_runtime_error_fmt("cannot reassign constant '%s'", name);
            ath_value_decref(s->bindings[i].value);
            s->bindings[i].value = v;
            ath_value_incref(v);
            s->bindings[i].is_const = is_const;
            return;
        }
    }
    if (s->count >= s->capacity) {
        s->capacity *= 2;
        s->bindings = (AthBinding *)realloc(s->bindings,
                                             sizeof(AthBinding) * s->capacity);
        if (!s->bindings) ath_fatal("out of memory");
    }
    s->bindings[s->count].name = (char *)malloc(strlen(name) + 1);
    strcpy((char*)s->bindings[s->count].name, name);
    s->bindings[s->count].value    = v;
    ath_value_incref(v);
    s->bindings[s->count].is_const = is_const;
    s->count++;
}

AthValue ath_scope_get(AthScope *s, const char *name) {
    int i;
    AthScope *cur = s;
    while (cur) {
        for (i = 0; i < cur->count; i++) {
            if (strcmp(cur->bindings[i].name, name) == 0)
                return cur->bindings[i].value;
        }
        cur = cur->parent;
    }
    ath_runtime_error_fmt("undefined variable '%s'", name);
    return ath_void();
}

void ath_scope_set(AthScope *s, const char *name, AthValue v) {
    int i;
    AthScope *cur = s;
    while (cur) {
        for (i = 0; i < cur->count; i++) {
            if (strcmp(cur->bindings[i].name, name) == 0) {
                if (cur->bindings[i].is_const)
                    ath_runtime_error_fmt("cannot reassign constant '%s'", name);
                ath_value_decref(cur->bindings[i].value);
                cur->bindings[i].value = v;
                ath_value_incref(v);
                return;
            }
        }
        cur = cur->parent;
    }
    ath_runtime_error_fmt("undefined variable '%s'", name);
}

int ath_scope_has_local(AthScope *s, const char *name) {
    int i;
    for (i = 0; i < s->count; i++)
        if (strcmp(s->bindings[i].name, name) == 0) return 1;
    return 0;
}
