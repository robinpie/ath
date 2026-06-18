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

/* ath_value.c -- tagged-union value implementation */
#include "ath_value.h"
#include "ath_entity.h"
#include "ath_scope.h"
#include "ath_error.h"
#include "ath_sylladex.h"
#include "ath_relic.h"
#include "ath_buffer.h"
#include "ath_session.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ===== String ===== */

AthString *ath_string_new(const char *data, int len) {
    AthString *s = (AthString *)malloc(sizeof(AthString) + len);
    if (!s) ath_fatal("out of memory");
    s->refcount = 1;
    s->length = len;
    memcpy(s->data, data, len);
    s->data[len] = '\0';
    return s;
}

AthString *ath_string_from_cstr(const char *cstr) {
    return ath_string_new(cstr, (int)strlen(cstr));
}

AthString *ath_string_concat(AthString *a, AthString *b) {
    int total = a->length + b->length;
    AthString *s = (AthString *)malloc(sizeof(AthString) + total);
    if (!s) ath_fatal("out of memory");
    s->refcount = 1;
    s->length = total;
    memcpy(s->data, a->data, a->length);
    memcpy(s->data + a->length, b->data, b->length);
    s->data[total] = '\0';
    return s;
}

AthString *ath_string_from_long(long v) {
    char buf[32];
    int n = sprintf(buf, "%ld", v);
    return ath_string_new(buf, n);
}

AthString *ath_string_from_double(double v) {
    char buf[64];
    int n;
    /* Match Python's float repr: no trailing zeros if not needed */
    if (v == (long)v && v >= -1e15 && v <= 1e15) {
        n = sprintf(buf, "%.1f", v);
    } else {
        n = sprintf(buf, "%g", v);
    }
    return ath_string_new(buf, n);
}

void ath_string_incref(AthString *s) {
    if (s) s->refcount++;
}

void ath_string_decref(AthString *s) {
    if (!s) return;
    if (--s->refcount <= 0) free(s);
}

const char *ath_string_cstr(AthString *s) {
    return s ? s->data : "";
}

/* ===== Array ===== */

AthArray *ath_array_new(int capacity) {
    AthArray *a = (AthArray *)malloc(sizeof(AthArray));
    if (!a) ath_fatal("out of memory");
    a->refcount = 1;
    a->length = 0;
    a->capacity = capacity < 4 ? 4 : capacity;
    a->data = (AthValue *)malloc(sizeof(AthValue) * a->capacity);
    if (!a->data) ath_fatal("out of memory");
    return a;
}

AthArray *ath_array_copy(AthArray *src) {
    int i;
    AthArray *dst = ath_array_new(src->length);
    dst->length = src->length;
    for (i = 0; i < src->length; i++) {
        dst->data[i] = src->data[i];
        ath_value_incref(dst->data[i]);
    }
    return dst;
}

void ath_array_free(AthArray *a) {
    int i;
    if (!a) return;
    for (i = 0; i < a->length; i++) ath_value_decref(a->data[i]);
    free(a->data);
    free(a);
}

void ath_array_incref(AthArray *a) { if (a) a->refcount++; }

void ath_array_decref(AthArray *a) {
    if (!a) return;
    if (--a->refcount <= 0) ath_array_free(a);
}

static void ath_array_push(AthArray *a, AthValue v) {
    if (a->length >= a->capacity) {
        a->capacity *= 2;
        a->data = (AthValue *)realloc(a->data, sizeof(AthValue) * a->capacity);
        if (!a->data) ath_fatal("out of memory");
    }
    a->data[a->length++] = v;
}

/* ===== Map ===== */

#define MAP_LOAD_FACTOR 0.7

static unsigned int ath_map_hash(const char *key, int len) {
    unsigned int h = 5381;
    int i;
    for (i = 0; i < len; i++) h = ((h << 5) + h) + (unsigned char)key[i];
    return h;
}

AthMap *ath_map_new(int capacity) {
    AthMap *m = (AthMap *)malloc(sizeof(AthMap));
    if (!m) ath_fatal("out of memory");
    m->refcount = 1;
    m->count = 0;
    m->capacity = capacity < 8 ? 8 : capacity;
    m->entries = (AthMapEntry *)calloc(m->capacity, sizeof(AthMapEntry));
    if (!m->entries) ath_fatal("out of memory");
    return m;
}

static void ath_map_rehash(AthMap *m) {
    int old_cap = m->capacity;
    AthMapEntry *old = m->entries;
    int i;
    m->capacity = old_cap * 2;
    m->entries = (AthMapEntry *)calloc(m->capacity, sizeof(AthMapEntry));
    if (!m->entries) ath_fatal("out of memory");
    m->count = 0;
    for (i = 0; i < old_cap; i++) {
        if (old[i].used) {
            ath_map_set_str(m, old[i].key, old[i].value);
            /* key was already incref'd; don't double-free */
        }
    }
    free(old);
}

void ath_map_set_str(AthMap *m, AthString *key, AthValue val) {
    unsigned int h;
    int idx, i;
    if ((double)m->count / m->capacity > MAP_LOAD_FACTOR) ath_map_rehash(m);
    h = ath_map_hash(key->data, key->length);
    idx = (int)(h % (unsigned int)m->capacity);
    for (i = 0; i < m->capacity; i++) {
        int slot = (idx + i) % m->capacity;
        AthMapEntry *e = &m->entries[slot];
        if (!e->used) {
            e->key = key;
            ath_string_incref(key);
            e->value = val;
            ath_value_incref(val);
            e->used = 1;
            m->count++;
            return;
        }
        if (e->used && e->key->length == key->length &&
            memcmp(e->key->data, key->data, key->length) == 0) {
            ath_value_decref(e->value);
            e->value = val;
            ath_value_incref(val);
            return;
        }
    }
    ath_fatal("map full after rehash (bug)");
}

void ath_map_set(AthMap *m, const char *key, AthValue val) {
    AthString *k = ath_string_from_cstr(key);
    ath_map_set_str(m, k, val);
    ath_string_decref(k);
}

AthValue ath_map_get(AthMap *m, const char *key) {
    unsigned int h;
    int idx, i, klen;
    if (!m) return ath_void();
    klen = (int)strlen(key);
    h = ath_map_hash(key, klen);
    idx = (int)(h % (unsigned int)m->capacity);
    for (i = 0; i < m->capacity; i++) {
        int slot = (idx + i) % m->capacity;
        AthMapEntry *e = &m->entries[slot];
        if (!e->used) break;
        if (e->key->length == klen && memcmp(e->key->data, key, klen) == 0)
            return e->value;
    }
    return ath_void();
}

int ath_map_has(AthMap *m, const char *key) {
    unsigned int h;
    int idx, i, klen;
    if (!m) return 0;
    klen = (int)strlen(key);
    h = ath_map_hash(key, klen);
    idx = (int)(h % (unsigned int)m->capacity);
    for (i = 0; i < m->capacity; i++) {
        int slot = (idx + i) % m->capacity;
        AthMapEntry *e = &m->entries[slot];
        if (!e->used) break;
        if (e->key->length == klen && memcmp(e->key->data, key, klen) == 0)
            return 1;
    }
    return 0;
}

void ath_map_delete(AthMap *m, const char *key) {
    unsigned int h;
    int idx, i, klen;
    klen = (int)strlen(key);
    h = ath_map_hash(key, klen);
    idx = (int)(h % (unsigned int)m->capacity);
    for (i = 0; i < m->capacity; i++) {
        int slot = (idx + i) % m->capacity;
        AthMapEntry *e = &m->entries[slot];
        if (!e->used) return;
        if (e->key->length == klen && memcmp(e->key->data, key, klen) == 0) {
            ath_string_decref(e->key);
            ath_value_decref(e->value);
            e->used = 0;
            m->count--;
            return;
        }
    }
}

AthArray *ath_map_keys(AthMap *m) {
    int i;
    AthArray *a = ath_array_new(m->count);
    for (i = 0; i < m->capacity; i++) {
        if (m->entries[i].used) {
            AthValue v;
            ath_string_incref(m->entries[i].key);
            v = ath_str_val(m->entries[i].key);
            ath_array_push(a, v);
        }
    }
    return a;
}

AthArray *ath_map_values(AthMap *m) {
    int i;
    AthArray *a = ath_array_new(m->count);
    for (i = 0; i < m->capacity; i++) {
        if (m->entries[i].used) {
            AthValue v = m->entries[i].value;
            ath_value_incref(v);
            ath_array_push(a, v);
        }
    }
    return a;
}

AthMap *ath_map_copy(AthMap *src) {
    int i;
    AthMap *dst = ath_map_new(src->capacity);
    for (i = 0; i < src->capacity; i++) {
        if (src->entries[i].used)
            ath_map_set_str(dst, src->entries[i].key, src->entries[i].value);
    }
    return dst;
}

void ath_map_free(AthMap *m) {
    int i;
    if (!m) return;
    for (i = 0; i < m->capacity; i++) {
        if (m->entries[i].used) {
            ath_string_decref(m->entries[i].key);
            ath_value_decref(m->entries[i].value);
        }
    }
    free(m->entries);
    free(m);
}

void ath_map_incref(AthMap *m) { if (m) m->refcount++; }
void ath_map_decref(AthMap *m) {
    if (!m) return;
    if (--m->refcount <= 0) ath_map_free(m);
}

/* ===== Rite ===== */

static AthRite *ath_rite_alloc(struct AthScope *closure, int is_async, int arity) {
    AthRite *r = (AthRite *)malloc(sizeof(AthRite));
    if (!r) ath_fatal("out of memory");
    r->refcount = 1;
    r->closure = closure;
    if (closure) ath_scope_incref(closure);
    r->is_async = is_async;
    r->arity = arity;
    r->data = NULL;
    r->data_free = NULL;
    return r;
}

AthRite *ath_rite_new_sync(struct AthScope *closure, AthRiteSyncFn fn, int arity) {
    AthRite *r = ath_rite_alloc(closure, 0, arity);
    r->fn.sync = fn;
    return r;
}

AthRite *ath_rite_new_async(struct AthScope *closure, AthRiteAsyncFn fn, int arity) {
    AthRite *r = ath_rite_alloc(closure, 1, arity);
    r->fn.async = fn;
    return r;
}

AthRite *ath_rite_new_ffi(struct AthScope *closure, AthRiteSyncFn fn, int arity,
                          void *data, AthRiteDataFree data_free) {
    AthRite *r = ath_rite_alloc(closure, 0, arity);
    r->fn.sync = fn;
    r->data = data;
    r->data_free = data_free;
    return r;
}

void ath_rite_incref(AthRite *r) { if (r) r->refcount++; }
void ath_rite_decref(AthRite *r) {
    if (!r) return;
    if (--r->refcount <= 0) {
        if (r->closure) ath_scope_decref(r->closure);
        if (r->data && r->data_free) r->data_free(r->data);
        free(r);
    }
}

/* ===== Value constructors ===== */

AthValue ath_void(void) {
    AthValue v;
    v.type = ATH_VOID;
    v.as.integer = 0;
    return v;
}

AthValue ath_bool(int b) {
    AthValue v;
    v.type = ATH_BOOLEAN;
    v.as.integer = b ? 1 : 0;
    return v;
}

AthValue ath_int(long n) {
    AthValue v;
    v.type = ATH_INTEGER;
    v.as.integer = n;
    return v;
}

AthValue ath_float(double f) {
    AthValue v;
    v.type = ATH_FLOAT;
    v.as.float_ = f;
    return v;
}

AthValue ath_str_val(AthString *s) {
    AthValue v;
    v.type = ATH_STRING;
    v.as.string = s;
    return v;
}

AthValue ath_str_cstr(const char *s) {
    AthValue v;
    v.type = ATH_STRING;
    v.as.string = ath_string_from_cstr(s);
    return v;
}

AthValue ath_array_val(AthArray *a) {
    AthValue v;
    v.type = ATH_ARRAY;
    v.as.array = a;
    return v;
}

AthValue ath_map_val(AthMap *m) {
    AthValue v;
    v.type = ATH_MAP;
    v.as.map = m;
    return v;
}

AthValue ath_module_val(AthMap *m) {
    AthValue v;
    v.type = ATH_MODULE;
    v.as.map = m;
    return v;
}

AthValue ath_entity_val(struct AthEntity *e) {
    AthValue v;
    v.type = ATH_ENTITY;
    v.as.entity = e;
    return v;
}

AthValue ath_rite_val(AthRite *r) {
    AthValue v;
    v.type = ATH_RITE;
    v.as.rite = r;
    return v;
}

AthValue ath_sylladex_val(struct AthSylladex *s) {
    AthValue v;
    v.type = ATH_SYLLADEX;
    v.as.sylladex = s;
    return v;
}

AthValue ath_relic_val(struct AthRelic *r) {
    AthValue v;
    v.type = ATH_RELIC;
    v.as.relic = r;
    return v;
}

AthValue ath_buffer_val(struct AthBuffer *b) {
    AthValue v;
    v.type = ATH_BUFFER;
    v.as.buffer = b;
    return v;
}

AthValue ath_session_val(struct AthSession *s) {
    AthValue v;
    v.type = ATH_SESSION;
    v.as.session = s;
    return v;
}

/* ===== Refcount ===== */

void ath_value_incref(AthValue v) {
    switch (v.type) {
    case ATH_STRING:   ath_string_incref(v.as.string); break;
    case ATH_ARRAY:    ath_array_incref(v.as.array);   break;
    case ATH_MAP:      ath_map_incref(v.as.map);        break;
    case ATH_MODULE:   ath_map_incref(v.as.map);        break;
    case ATH_RITE:     ath_rite_incref(v.as.rite);      break;
    case ATH_SYLLADEX: ath_syl_incref(v.as.sylladex);   break;
    case ATH_RELIC:    ath_relic_incref(v.as.relic);    break;
    case ATH_BUFFER:   ath_buffer_incref(v.as.buffer);  break;
    case ATH_SESSION:  ath_session_incref(v.as.session); break;
    default: break;
    }
}

void ath_value_decref(AthValue v) {
    switch (v.type) {
    case ATH_STRING:   ath_string_decref(v.as.string); break;
    case ATH_ARRAY:    ath_array_decref(v.as.array);   break;
    case ATH_MAP:      ath_map_decref(v.as.map);        break;
    case ATH_MODULE:   ath_map_decref(v.as.map);        break;
    case ATH_RITE:     ath_rite_decref(v.as.rite);      break;
    case ATH_SYLLADEX: ath_syl_decref(v.as.sylladex);   break;
    case ATH_RELIC:    ath_relic_decref(v.as.relic);    break;
    case ATH_BUFFER:   ath_buffer_decref(v.as.buffer);  break;
    case ATH_SESSION:  ath_session_decref(v.as.session); break;
    default: break;
    }
}

AthValue ath_value_copy(AthValue v) {
    /* For mutable types, make a deep copy; primitives are already value types */
    switch (v.type) {
    case ATH_STRING: ath_string_incref(v.as.string); return v;
    case ATH_ARRAY: {
        AthArray *a = ath_array_copy(v.as.array);
        return ath_array_val(a);
    }
    case ATH_MAP: {
        AthMap *m = ath_map_copy(v.as.map);
        return ath_map_val(m);
    }
    case ATH_MODULE: {
        AthMap *m = ath_map_copy(v.as.map);
        return ath_module_val(m);
    }
    case ATH_RITE: ath_rite_incref(v.as.rite); return v;
    case ATH_SYLLADEX: ath_syl_incref(v.as.sylladex); return v;
    case ATH_RELIC: ath_relic_incref(v.as.relic); return v;
    case ATH_BUFFER: ath_buffer_incref(v.as.buffer); return v;
    case ATH_SESSION: ath_session_incref(v.as.session); return v;
    default: return v;
    }
}

/* ===== Type checks ===== */

int ath_is_truthy(AthValue v) {
    switch (v.type) {
    case ATH_VOID:    return 0;
    case ATH_BOOLEAN: return v.as.integer != 0;
    case ATH_INTEGER: return v.as.integer != 0;
    case ATH_FLOAT:   return v.as.float_ != 0.0;
    case ATH_STRING:  return v.as.string && v.as.string->length > 0;
    case ATH_ARRAY:   return v.as.array  && v.as.array->length > 0;
    case ATH_MAP:     return v.as.map    && v.as.map->count > 0;
    case ATH_MODULE:  return 1;
    case ATH_SYLLADEX: return ath_syl_is_truthy(v.as.sylladex);
    case ATH_RELIC:   return v.as.relic && !v.as.relic->cursed && v.as.relic->ptr != NULL;
    case ATH_BUFFER: {
        int i;
        if (!v.as.buffer || !v.as.buffer->bytes) return 0;
        for (i = 0; i < v.as.buffer->length; i++)
            if (v.as.buffer->bytes[i] != 0) return 1;
        return 0;
    }
    case ATH_SESSION: return v.as.session && v.as.session->entity &&
                             !v.as.session->entity->is_dead;
    default:          return 1;
    }
}

const char *ath_typeof_str(AthValue v) {
    switch (v.type) {
    case ATH_VOID:    return "VOID";
    case ATH_BOOLEAN: return "BOOLEAN";
    case ATH_INTEGER: return "INTEGER";
    case ATH_FLOAT:   return "FLOAT";
    case ATH_STRING:  return "STRING";
    case ATH_ARRAY:   return "ARRAY";
    case ATH_MAP:     return "MAP";
    case ATH_ENTITY:  return "ENTITY";
    case ATH_RITE:    return "RITE";
    case ATH_MODULE:  return "MODULE";
    case ATH_SYLLADEX: return ath_syl_typeof_str(v.as.sylladex);
    case ATH_RELIC:   return "RELIC";
    case ATH_BUFFER:  return "BUFFER";
    case ATH_SESSION: return "SESSION";
    default:          return "UNKNOWN";
    }
}

/* Forward: implemented below after stringify helpers */
static char *stringify_array(AthArray *a);
static char *stringify_map(AthMap *m);

char *ath_stringify(AthValue v) {
    char *buf;
    switch (v.type) {
    case ATH_VOID:    buf = (char*)malloc(5); strcpy(buf,"VOID"); return buf;
    case ATH_BOOLEAN: buf = (char*)malloc(6);
        strcpy(buf, v.as.integer ? "ALIVE" : "DEAD"); return buf;
    case ATH_INTEGER: {
        char tmp[32];
        sprintf(tmp, "%ld", v.as.integer);
        buf = (char*)malloc(strlen(tmp)+1); strcpy(buf,tmp); return buf;
    }
    case ATH_FLOAT: {
        char tmp[64];
        if (v.as.float_ == (long)v.as.float_ && v.as.float_ >= -1e15 && v.as.float_ <= 1e15)
            sprintf(tmp, "%.1f", v.as.float_);
        else
            sprintf(tmp, "%g", v.as.float_);
        buf = (char*)malloc(strlen(tmp)+1); strcpy(buf,tmp); return buf;
    }
    case ATH_STRING:
        buf = (char*)malloc(v.as.string->length + 1);
        memcpy(buf, v.as.string->data, v.as.string->length);
        buf[v.as.string->length] = '\0';
        return buf;
    case ATH_ARRAY:  return stringify_array(v.as.array);
    case ATH_MAP:    return stringify_map(v.as.map);
    case ATH_MODULE: return stringify_map(v.as.map);
    case ATH_ENTITY: {
        const char *name = v.as.entity ? v.as.entity->name : "?";
        buf = (char*)malloc(strlen(name)+1); strcpy(buf,name); return buf;
    }
    case ATH_RITE:
        buf = (char*)malloc(8); strcpy(buf,"<rite>"); return buf;
    case ATH_SYLLADEX:
        return ath_syl_stringify(v.as.sylladex);
    case ATH_RELIC:
        if (!v.as.relic || v.as.relic->cursed) {
            buf = (char*)malloc(16); strcpy(buf,"<cursed relic>"); return buf;
        }
        buf = (char*)malloc(8); strcpy(buf,"<relic>"); return buf;
    case ATH_BUFFER: {
        char tmp[32];
        sprintf(tmp, "<buffer:%d>", v.as.buffer ? v.as.buffer->length : 0);
        buf = (char*)malloc(strlen(tmp)+1); strcpy(buf,tmp); return buf;
    }
    case ATH_SESSION: {
        const char *name = (v.as.session && v.as.session->entity)
                           ? v.as.session->entity->name : "?";
        buf = (char*)malloc(strlen(name) + 12);
        sprintf(buf, "<session:%s>", name);
        return buf;
    }
    default:
        buf = (char*)malloc(8); strcpy(buf,"<?>"); return buf;
    }
}

static char *stringify_array(AthArray *a) {
    /* "[e1, e2, ...]" */
    char **parts;
    int i, total = 3; /* "[]" + null */
    char *result, *p;
    if (!a || a->length == 0) {
        result = (char*)malloc(3); strcpy(result, "[]"); return result;
    }
    parts = (char**)malloc(sizeof(char*) * a->length);
    for (i = 0; i < a->length; i++) {
        parts[i] = ath_stringify(a->data[i]);
        total += (int)strlen(parts[i]) + 2; /* ", " */
    }
    result = (char*)malloc(total + 4);
    p = result;
    *p++ = '[';
    for (i = 0; i < a->length; i++) {
        if (i > 0) { *p++ = ','; *p++ = ' '; }
        strcpy(p, parts[i]); p += strlen(parts[i]);
        free(parts[i]);
    }
    free(parts);
    *p++ = ']'; *p = '\0';
    return result;
}

static char *stringify_map(AthMap *m) {
    /* "{k: v, ...}" */
    char **parts;
    int i, j, total = 3, count = 0;
    char *result, *p;
    if (!m || m->count == 0) {
        result = (char*)malloc(3); strcpy(result, "{}"); return result;
    }
    parts = (char**)malloc(sizeof(char*) * m->count * 2);
    for (i = 0; i < m->capacity; i++) {
        if (m->entries[i].used) {
            parts[count*2]   = (char*)malloc(m->entries[i].key->length + 1);
            memcpy(parts[count*2], m->entries[i].key->data, m->entries[i].key->length);
            parts[count*2][m->entries[i].key->length] = '\0';
            parts[count*2+1] = ath_stringify(m->entries[i].value);
            total += (int)strlen(parts[count*2]) + (int)strlen(parts[count*2+1]) + 4;
            count++;
        }
    }
    result = (char*)malloc(total + 4);
    p = result;
    *p++ = '{';
    for (j = 0; j < count; j++) {
        if (j > 0) { *p++ = ','; *p++ = ' '; }
        strcpy(p, parts[j*2]); p += strlen(parts[j*2]);
        *p++ = ':'; *p++ = ' ';
        strcpy(p, parts[j*2+1]); p += strlen(parts[j*2+1]);
        free(parts[j*2]); free(parts[j*2+1]);
    }
    free(parts);
    *p++ = '}'; *p = '\0';
    return result;
}

/* ===== Operators ===== */

AthValue ath_add(AthValue a, AthValue b) {
    /* string coercion: if either is string, convert both */
    if (a.type == ATH_STRING || b.type == ATH_STRING) {
        char *sa = ath_stringify(a), *sb = ath_stringify(b);
        AthString *s = ath_string_new(sa, (int)strlen(sa));
        AthString *t = ath_string_from_cstr(sb);
        AthString *result = ath_string_concat(s, t);
        free(sa); free(sb);
        ath_string_decref(s); ath_string_decref(t);
        return ath_str_val(result);
    }
    if (a.type == ATH_INTEGER && b.type == ATH_INTEGER)
        return ath_int(a.as.integer + b.as.integer);
    if (a.type == ATH_FLOAT || b.type == ATH_FLOAT) {
        double fa = (a.type==ATH_FLOAT) ? a.as.float_ : (double)a.as.integer;
        double fb = (b.type==ATH_FLOAT) ? b.as.float_ : (double)b.as.integer;
        return ath_float(fa + fb);
    }
    ath_runtime_error("'+' requires numeric or string operands", 0, 0);
    return ath_void();
}

AthValue ath_sub(AthValue a, AthValue b) {
    if (a.type == ATH_INTEGER && b.type == ATH_INTEGER)
        return ath_int(a.as.integer - b.as.integer);
    if ((a.type==ATH_FLOAT||a.type==ATH_INTEGER) && (b.type==ATH_FLOAT||b.type==ATH_INTEGER)) {
        double fa = (a.type==ATH_FLOAT) ? a.as.float_ : (double)a.as.integer;
        double fb = (b.type==ATH_FLOAT) ? b.as.float_ : (double)b.as.integer;
        return ath_float(fa - fb);
    }
    ath_runtime_error("'-' requires numeric operands", 0, 0);
    return ath_void();
}

AthValue ath_mul(AthValue a, AthValue b) {
    if (a.type == ATH_INTEGER && b.type == ATH_INTEGER)
        return ath_int(a.as.integer * b.as.integer);
    if ((a.type==ATH_FLOAT||a.type==ATH_INTEGER) && (b.type==ATH_FLOAT||b.type==ATH_INTEGER)) {
        double fa = (a.type==ATH_FLOAT) ? a.as.float_ : (double)a.as.integer;
        double fb = (b.type==ATH_FLOAT) ? b.as.float_ : (double)b.as.integer;
        return ath_float(fa * fb);
    }
    ath_runtime_error("'*' requires numeric operands", 0, 0);
    return ath_void();
}

AthValue ath_div(AthValue a, AthValue b) {
    if (a.type == ATH_INTEGER && b.type == ATH_INTEGER) {
        if (b.as.integer == 0) ath_runtime_error("division by zero", 0, 0);
        return ath_int(a.as.integer / b.as.integer);
    }
    if ((a.type==ATH_FLOAT||a.type==ATH_INTEGER) && (b.type==ATH_FLOAT||b.type==ATH_INTEGER)) {
        double fa = (a.type==ATH_FLOAT) ? a.as.float_ : (double)a.as.integer;
        double fb = (b.type==ATH_FLOAT) ? b.as.float_ : (double)b.as.integer;
        if (fb == 0.0) ath_runtime_error("division by zero", 0, 0);
        return ath_float(fa / fb);
    }
    ath_runtime_error("'/' requires numeric operands", 0, 0);
    return ath_void();
}

AthValue ath_mod(AthValue a, AthValue b) {
    if (a.type == ATH_INTEGER && b.type == ATH_INTEGER) {
        if (b.as.integer == 0) ath_runtime_error("modulo by zero", 0, 0);
        return ath_int(a.as.integer % b.as.integer);
    }
    ath_runtime_error("'%%' requires integer operands", 0, 0);
    return ath_void();
}

AthValue ath_neg(AthValue a) {
    if (a.type == ATH_INTEGER) return ath_int(-a.as.integer);
    if (a.type == ATH_FLOAT)   return ath_float(-a.as.float_);
    ath_runtime_error("unary '-' requires numeric operand", 0, 0);
    return ath_void();
}

AthValue ath_band(AthValue a, AthValue b) {
    if (a.type != ATH_INTEGER || b.type != ATH_INTEGER)
        ath_runtime_error("'&' requires integer operands", 0, 0);
    return ath_int(a.as.integer & b.as.integer);
}

AthValue ath_bor(AthValue a, AthValue b) {
    if (a.type != ATH_INTEGER || b.type != ATH_INTEGER)
        ath_runtime_error("'|' requires integer operands", 0, 0);
    return ath_int(a.as.integer | b.as.integer);
}

AthValue ath_bxor(AthValue a, AthValue b) {
    if (a.type != ATH_INTEGER || b.type != ATH_INTEGER)
        ath_runtime_error("'^' requires integer operands", 0, 0);
    return ath_int(a.as.integer ^ b.as.integer);
}

AthValue ath_bnot(AthValue a) {
    if (a.type != ATH_INTEGER)
        ath_runtime_error("'~' requires integer operand", 0, 0);
    return ath_int(~a.as.integer);
}

AthValue ath_lshift(AthValue a, AthValue b) {
    if (a.type != ATH_INTEGER || b.type != ATH_INTEGER)
        ath_runtime_error("'<<' requires integer operands", 0, 0);
    return ath_int(a.as.integer << (int)b.as.integer);
}

AthValue ath_rshift(AthValue a, AthValue b) {
    if (a.type != ATH_INTEGER || b.type != ATH_INTEGER)
        ath_runtime_error("'>>' requires integer operands", 0, 0);
    return ath_int((long)((unsigned long)a.as.integer >> (int)b.as.integer));
}

static int ath_values_equal(AthValue a, AthValue b) {
    if (a.type != b.type) {
        /* int/float cross comparison */
        if ((a.type==ATH_INTEGER||a.type==ATH_FLOAT) &&
            (b.type==ATH_INTEGER||b.type==ATH_FLOAT)) {
            double fa = (a.type==ATH_FLOAT)?a.as.float_:(double)a.as.integer;
            double fb = (b.type==ATH_FLOAT)?b.as.float_:(double)b.as.integer;
            return fa == fb;
        }
        return 0;
    }
    switch (a.type) {
    case ATH_VOID:    return 1;
    case ATH_BOOLEAN: return a.as.integer == b.as.integer;
    case ATH_INTEGER: return a.as.integer == b.as.integer;
    case ATH_FLOAT:   return a.as.float_  == b.as.float_;
    case ATH_STRING:
        return a.as.string->length == b.as.string->length &&
               memcmp(a.as.string->data, b.as.string->data, a.as.string->length) == 0;
    case ATH_ENTITY:  return a.as.entity == b.as.entity;
    case ATH_SYLLADEX: return ath_syl_eq(a.as.sylladex, b.as.sylladex);
    case ATH_RELIC:   return a.as.relic == b.as.relic;
    case ATH_SESSION: return a.as.session == b.as.session;
    case ATH_BUFFER:
        if (a.as.buffer == b.as.buffer) return 1;
        if (!a.as.buffer || !b.as.buffer) return 0;
        if (a.as.buffer->length != b.as.buffer->length) return 0;
        if (a.as.buffer->length == 0) return 1;
        return memcmp(a.as.buffer->bytes, b.as.buffer->bytes,
                      a.as.buffer->length) == 0;
    default: return 0;
    }
}

AthValue ath_eq(AthValue a, AthValue b) { return ath_bool(ath_values_equal(a,b)); }
AthValue ath_ne(AthValue a, AthValue b) { return ath_bool(!ath_values_equal(a,b)); }

static double ath_to_num(AthValue v, const char *op) {
    if (v.type == ATH_INTEGER) return (double)v.as.integer;
    if (v.type == ATH_FLOAT)   return v.as.float_;
    ath_runtime_error_fmt("'%s' requires numeric operands", op);
    return 0;
}

AthValue ath_lt(AthValue a, AthValue b) { return ath_bool(ath_to_num(a,"<") < ath_to_num(b,"<")); }
AthValue ath_gt(AthValue a, AthValue b) { return ath_bool(ath_to_num(a,">") > ath_to_num(b,">")); }
AthValue ath_le(AthValue a, AthValue b) { return ath_bool(ath_to_num(a,"<=") <= ath_to_num(b,"<=")); }
AthValue ath_ge(AthValue a, AthValue b) { return ath_bool(ath_to_num(a,">=") >= ath_to_num(b,">=")); }

AthValue ath_index(AthValue obj, AthValue idx) {
    switch (obj.type) {
    case ATH_ARRAY: {
        int i;
        if (idx.type != ATH_INTEGER)
            ath_runtime_error("array index must be INTEGER", 0, 0);
        i = (int)idx.as.integer;
        if (i < 0 || i >= obj.as.array->length)
            ath_runtime_error("array index out of bounds", 0, 0);
        return obj.as.array->data[i];
    }
    case ATH_STRING: {
        int i;
        AthString *s;
        if (idx.type != ATH_INTEGER)
            ath_runtime_error("string index must be INTEGER", 0, 0);
        i = (int)idx.as.integer;
        if (i < 0 || i >= obj.as.string->length)
            ath_runtime_error("string index out of bounds", 0, 0);
        s = ath_string_new(&obj.as.string->data[i], 1);
        return ath_str_val(s);
    }
    case ATH_MAP:
    case ATH_MODULE: {
        char *key = ath_stringify(idx);
        AthValue v = ath_map_get(obj.as.map, key);
        free(key);
        return v;
    }
    case ATH_SYLLADEX:
        ath_runtime_error("sylladex is not indexable; use EJECT", 0, 0);
        return ath_void();
    default:
        ath_runtime_error("cannot index this type", 0, 0);
        return ath_void();
    }
}

AthValue ath_member(AthValue obj, const char *member) {
    switch (obj.type) {
    case ATH_MAP:
    case ATH_MODULE:
        return ath_map_get(obj.as.map, member);
    case ATH_SESSION:
        if (!obj.as.session) {
            ath_runtime_error_fmt("session has no member '%s'", member);
            return ath_void();
        }
        return ath_map_get(obj.as.session->rites, member);
    case ATH_SYLLADEX:
        ath_runtime_error_fmt("sylladex has no member '%s'", member);
        return ath_void();
    default:
        ath_runtime_error_fmt("cannot access member '%s' on non-map value", member);
        return ath_void();
    }
}

void ath_index_set(AthValue obj, AthValue idx, AthValue val) {
    if (obj.type == ATH_ARRAY) {
        int i;
        if (idx.type != ATH_INTEGER)
            ath_runtime_error("array index must be INTEGER", 0, 0);
        i = (int)idx.as.integer;
        if (i < 0 || i >= obj.as.array->length)
            ath_runtime_error("array index out of bounds for assignment", 0, 0);
        ath_value_decref(obj.as.array->data[i]);
        obj.as.array->data[i] = val;
        ath_value_incref(val);
    } else if (obj.type == ATH_MAP || obj.type == ATH_MODULE) {
        char *key = ath_stringify(idx);
        ath_map_set(obj.as.map, key, val);
        free(key);
    } else {
        ath_runtime_error("cannot index-assign this type", 0, 0);
    }
}

void ath_member_set(AthValue obj, const char *member, AthValue val) {
    if (obj.type != ATH_MAP && obj.type != ATH_MODULE)
        ath_runtime_error("cannot set member on non-map value", 0, 0);
    ath_map_set(obj.as.map, member, val);
}
