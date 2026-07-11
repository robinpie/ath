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

/* ath_scope.c */
#include "ath_scope.h"
#include "ath_error.h"
#include <stdlib.h>
#include <string.h>

/* Names are a few characters; an inline compare beats a libc strcmp call. */
static int name_eq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

/* djb2 over the name; used to bucket the intern table */
static unsigned int scope_hash(const char *name) {
    unsigned int h = 5381;
    while (*name) h = ((h << 5) + h) + (unsigned char)*name++;
    return h;
}

/* Binding names are interned: each distinct name is malloc'd once and kept
 * for the life of the program, so ath_scope_define never copies a name and
 * ath_scope_decref never frees one. The set of distinct names is bounded by
 * the program's identifiers, so this "leak" is a fixed cost. */
typedef struct InternEntry {
    const char         *str;
    unsigned int        hash;
    struct InternEntry *next;
} InternEntry;

#define INTERN_BUCKETS 256
static InternEntry *_intern[INTERN_BUCKETS];

static const char *intern_name(const char *name, unsigned int h) {
    InternEntry **bucket = &_intern[h & (INTERN_BUCKETS - 1)];
    InternEntry *e;
    char *copy;
    for (e = *bucket; e; e = e->next)
        if (e->hash == h && name_eq(e->str, name)) return e->str;
    e = (InternEntry *)malloc(sizeof(InternEntry));
    copy = (char *)malloc(strlen(name) + 1);
    if (!e || !copy) ath_fatal("out of memory");
    strcpy(copy, name);
    e->str  = copy;
    e->hash = h;
    e->next = *bucket;
    *bucket = e;
    return e->str;
}

/* Query cache: generated code passes variable names as C string literals,
 * so the same pointer recurs at every call site. Map query pointer ->
 * interned pointer so hot lookups skip hashing and the intern table. The
 * strcmp on a hit guards the rare dynamic-string caller whose freed buffer
 * got reused at the same address. */
#define QCACHE_SIZE 256
static struct { const char *q; const char *interned; } _qcache[QCACHE_SIZE];

static const char *intern_query(const char *name) {
    /* No shift: adjacent short literals sit only a few bytes apart, so the
     * low address bits are exactly what distinguishes them. */
    unsigned int idx =
        (unsigned int)((unsigned long)(size_t)name) & (QCACHE_SIZE - 1);
    unsigned int h;
    const char *iname;
    if (_qcache[idx].q == name && name_eq(name, _qcache[idx].interned))
        return _qcache[idx].interned;
    h = scope_hash(name);
    iname = intern_name(name, h);
    _qcache[idx].q = name;
    _qcache[idx].interned = iname;
    return iname;
}

/* Rite frames create and destroy one scope per call; recycle them. */
static AthScope *_scope_free_list  = NULL;
static int       _scope_free_count = 0;
#define SCOPE_FREE_MAX 128

AthScope *ath_scope_new(AthScope *parent) {
    AthScope *s;
    if (_scope_free_list) {
        s = _scope_free_list;
        _scope_free_list = (AthScope *)s->parent;
        _scope_free_count--;
    } else {
        s = (AthScope *)malloc(sizeof(AthScope));
        if (!s) ath_fatal("out of memory");
    }
    s->refcount = 1;
    s->parent   = parent;
    if (parent) parent->refcount++;
    s->count    = 0;
    s->capacity = ATH_SCOPE_INLINE;
    s->bindings = s->inline_bindings;
    s->is_builtins = 0;
    return s;
}

void ath_scope_incref(AthScope *s) { if (s) s->refcount++; }

void ath_scope_decref(AthScope *s) {
    int i;
    if (!s) return;
    if (--s->refcount > 0) return;
    for (i = 0; i < s->count; i++)
        ath_value_decref(s->bindings[i].value);   /* names are interned */
    if (s->bindings != s->inline_bindings) free(s->bindings);
    if (s->parent) ath_scope_decref(s->parent);
    if (_scope_free_count < SCOPE_FREE_MAX) {
        s->parent = (AthScope *)_scope_free_list;
        _scope_free_list = s;
        _scope_free_count++;
    } else {
        free(s);
    }
}

void ath_scope_define(AthScope *s, const char *name, AthValue v, int is_const) {
    int i;
    const char *iname = intern_query(name);
    /* Check if already defined in this scope (allow re-define for ~ATH re-imports).
       All binding names are interned, so pointer equality suffices. */
    for (i = 0; i < s->count; i++) {
        if (s->bindings[i].name == iname) {
            if (s->bindings[i].is_const)
                ath_runtime_error_fmt("cannot reassign constant '%s'", name);
            ath_value_decref(s->bindings[i].value);
            s->bindings[i].value = v;
            ath_value_incref(v);
            s->bindings[i].is_const = is_const;
            return;
        }
    }
    /* Builtins live one scope above the program root; a global define of a
     * builtin's name must still fail loudly, as it did when builtins shared
     * the root scope. */
    if (s->parent && s->parent->is_builtins) {
        AthScope *b = s->parent;
        for (i = 0; i < b->count; i++) {
            if (b->bindings[i].name == iname && b->bindings[i].is_const)
                ath_runtime_error_fmt("cannot reassign constant '%s'", name);
        }
    }
    if (s->count >= s->capacity) {
        s->capacity *= 2;
        if (s->bindings == s->inline_bindings) {
            s->bindings = (AthBinding *)malloc(sizeof(AthBinding) * s->capacity);
            if (!s->bindings) ath_fatal("out of memory");
            memcpy(s->bindings, s->inline_bindings,
                   sizeof(AthBinding) * s->count);
        } else {
            s->bindings = (AthBinding *)realloc(s->bindings,
                                                 sizeof(AthBinding) * s->capacity);
            if (!s->bindings) ath_fatal("out of memory");
        }
    }
    s->bindings[s->count].name     = iname;
    s->bindings[s->count].value    = v;
    ath_value_incref(v);
    s->bindings[s->count].is_const = is_const;
    s->count++;
}

AthValue ath_scope_get(AthScope *s, const char *name) {
    int i;
    const char *iname = intern_query(name);
    AthScope *cur = s;
    while (cur) {
        for (i = 0; i < cur->count; i++) {
            if (cur->bindings[i].name == iname)
                return cur->bindings[i].value;
        }
        cur = cur->parent;
    }
    ath_runtime_error_fmt("undefined variable '%s'", name);
    return ath_void();
}

void ath_scope_set(AthScope *s, const char *name, AthValue v) {
    int i;
    const char *iname = intern_query(name);
    AthScope *cur = s;
    while (cur) {
        for (i = 0; i < cur->count; i++) {
            if (cur->bindings[i].name == iname) {
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
    const char *iname = intern_query(name);
    for (i = 0; i < s->count; i++)
        if (s->bindings[i].name == iname) return 1;
    return 0;
}
