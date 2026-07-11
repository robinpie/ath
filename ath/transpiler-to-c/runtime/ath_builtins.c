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

/* ath_builtins.c -- all !~ATH built-in rites */
#include "ath_builtins.h"
#include "ath_error.h"
#include "ath_eventloop.h"
#include "ath_sylladex.h"
#include "ath_buffer.h"
#include "ath_entity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* ===== Helper macros ===== */

#define REQUIRE_ARGC(n, name) \
    if (argc != (n)) ath_runtime_error_fmt(name " requires %d argument(s), got %d", (n), argc)

#define REQUIRE_TYPE(v, t, name, argname) \
    if ((v).type != (t)) \
        ath_runtime_error_fmt(name ": " argname " must be %s, got %s", \
                              ath_typeof_str(ath_int(0)+(t)-ATH_INTEGER), \
                              ath_typeof_str(v))

#define REQUIRE_INT(v, name, argname) \
    if ((v).type != ATH_INTEGER) \
        ath_runtime_error_fmt(name ": " argname " must be INTEGER, got %s", ath_typeof_str(v))

#define REQUIRE_FLOAT_OR_INT(v, name, argname) \
    if ((v).type != ATH_INTEGER && (v).type != ATH_FLOAT) \
        ath_runtime_error_fmt(name ": " argname " must be numeric, got %s", ath_typeof_str(v))

#define REQUIRE_STRING(v, name, argname) \
    if ((v).type != ATH_STRING) \
        ath_runtime_error_fmt(name ": " argname " must be STRING, got %s", ath_typeof_str(v))

#define REQUIRE_ARRAY(v, name, argname) \
    if ((v).type != ATH_ARRAY) \
        ath_runtime_error_fmt(name ": " argname " must be ARRAY, got %s", ath_typeof_str(v))

#define REQUIRE_MAP(v, name, argname) \
    if ((v).type != ATH_MAP) \
        ath_runtime_error_fmt(name ": " argname " must be MAP, got %s", ath_typeof_str(v))

/* ===== Lookup table ===== */

static struct { const char *name; AthBuiltinFn fn; } _builtins[] = {
    {"UTTER",       ath_builtin_UTTER},
    {"HEED",        ath_builtin_HEED},
    {"SCRY",        ath_builtin_SCRY},
    {"INSCRIBE",    ath_builtin_INSCRIBE},
    {"TYPEOF",      ath_builtin_TYPEOF},
    {"LENGTH",      ath_builtin_LENGTH},
    {"COUNT",       ath_builtin_COUNT},
    {"PARSE_INT",   ath_builtin_PARSE_INT},
    {"PARSE_FLOAT", ath_builtin_PARSE_FLOAT},
    {"STRING",      ath_builtin_STRING},
    {"INT",         ath_builtin_INT},
    {"FLOAT",       ath_builtin_FLOAT},
    {"CHAR",        ath_builtin_CHAR},
    {"CODE",        ath_builtin_CODE},
    {"BIN",         ath_builtin_BIN},
    {"HEX",         ath_builtin_HEX},
    {"APPEND",      ath_builtin_APPEND},
    {"PREPEND",     ath_builtin_PREPEND},
    {"SLICE",       ath_builtin_SLICE},
    {"FIRST",       ath_builtin_FIRST},
    {"LAST",        ath_builtin_LAST},
    {"CONCAT",      ath_builtin_CONCAT},
    {"KEYS",        ath_builtin_KEYS},
    {"VALUES",      ath_builtin_VALUES},
    {"HAS",         ath_builtin_HAS},
    {"SET",         ath_builtin_SET},
    {"DELETE",      ath_builtin_DELETE},
    {"SPLIT",       ath_builtin_SPLIT},
    {"JOIN",        ath_builtin_JOIN},
    {"SUBSTRING",   ath_builtin_SUBSTRING},
    {"UPPERCASE",   ath_builtin_UPPERCASE},
    {"LOWERCASE",   ath_builtin_LOWERCASE},
    {"TRIM",        ath_builtin_TRIM},
    {"REPLACE",     ath_builtin_REPLACE},
    {"RANDOM",      ath_builtin_RANDOM},
    {"RANDOM_INT",  ath_builtin_RANDOM_INT},
    {"TIME",        ath_builtin_TIME},
    {"BUFFER",            ath_builtin_BUFFER},
    {"BYTE_AT",           ath_builtin_BYTE_AT},
    {"SET_BYTE",          ath_builtin_SET_BYTE},
    {"BUFFER_TO_STRING",  ath_builtin_BUFFER_TO_STRING},
    {"STRING_TO_BUFFER",  ath_builtin_STRING_TO_BUFFER},
    {"RECKON",            ath_builtin_RECKON},
    {"SENDIFICATE",       ath_builtin_SENDIFICATE},
    {"APPEARIFY",         ath_builtin_APPEARIFY},
    {"CAPTCHA",           ath_builtin_CAPTCHA},
    {"SIZEOF",            ath_builtin_SIZEOF},
    {"BAKE",              ath_builtin_BAKE},
    {"SPRINKLE",          ath_builtin_SPRINKLE},
    {"SCOOP",             ath_builtin_SCOOP},
    {"FLAVOR",            ath_builtin_FLAVOR},
    {"PLATE",             ath_builtin_PLATE},
    {"TASTE",             ath_builtin_TASTE},
    {"UNPLATE",           ath_builtin_UNPLATE},
    {NULL, NULL}
};

AthBuiltinFn ath_builtin_lookup(const char *name) {
    int i;
    for (i = 0; _builtins[i].name; i++)
        if (strcmp(_builtins[i].name, name) == 0) return _builtins[i].fn;
    return NULL;
}

void ath_scope_define_builtins(AthScope *s) {
    int i;
    AthScope *target = s;
    /* Put the ~55 builtins in a scope of their own above the program root,
     * so ordinary global-variable lookups don't linearly scan past them.
     * Shadowing still works: a user global with a builtin's name lands in
     * `s` and is found first. */
    if (s->parent == NULL) {
        target = ath_scope_new(NULL);   /* refcount 1, owned by s below */
        target->is_builtins = 1;
        s->parent = target;
    }
    for (i = 0; _builtins[i].name; i++) {
        AthRite  *r = ath_rite_new_sync(target, _builtins[i].fn, -1);
        AthValue  v = ath_rite_val(r);
        ath_scope_define(target, _builtins[i].name, v, 1);
        ath_rite_decref(r);
    }
}

/* Stack of currently-calling rites. ath_current_rite() returns the top; the stack lets FFI rites that re-enter !~ATH (e.g., via destructors) recover the correct caller on each frame. Single-threaded runtime. */
static AthRite *_ath_calling_rite = NULL;

AthRite *ath_current_rite(void) { return _ath_calling_rite; }

AthValue ath_call_sync(AthScope *scope, AthValue callee, int argc, AthValue *argv) {
    if (callee.type == ATH_RITE) {
        AthRite *r = callee.as.rite;
        AthRite *prev;
        AthValue result;
        if (r->is_async)
            ath_runtime_error("cannot call async rite synchronously", 0, 0);
        prev = _ath_calling_rite;
        _ath_calling_rite = r;
        result = r->fn.sync(r->closure ? r->closure : scope, argc, argv);
        _ath_calling_rite = prev;
        return result;
    }
    ath_runtime_error("value is not callable", 0, 0);
    return ath_void();
}

/* ===== I/O ===== */

AthValue ath_builtin_UTTER(AthScope *s, int argc, AthValue *argv) {
    int i;
    (void)s;
    for (i = 0; i < argc; i++) {
        char *str = ath_stringify(argv[i]);
        if (i > 0) fputs(" ", stdout);
        fputs(str, stdout);
        free(str);
    }
    fputc('\n', stdout);
    fflush(stdout);
    return ath_void();
}

AthValue ath_builtin_HEED(AthScope *s, int argc, AthValue *argv) {
    char buf[4096];
    int len;
    (void)s; (void)argc; (void)argv;
    if (!fgets(buf, sizeof(buf), stdin)) return ath_str_cstr("");
    len = (int)strlen(buf);
    if (len > 0 && buf[len-1] == '\n') buf[--len] = '\0';
    return ath_str_cstr(buf);
}

AthValue ath_builtin_SCRY(AthScope *s, int argc, AthValue *argv) {
    (void)s;
    REQUIRE_ARGC(1, "SCRY");
    if (argv[0].type == ATH_VOID) {
        /* read all stdin */
        char *buf = NULL;
        size_t total = 0, cap = 4096;
        buf = (char*)malloc(cap);
        if (!buf) ath_fatal("out of memory");
        while (!feof(stdin)) {
            size_t n;
            if (total + 1024 > cap) {
                cap *= 2;
                buf = (char*)realloc(buf, cap);
                if (!buf) ath_fatal("out of memory");
            }
            n = fread(buf + total, 1, 1024, stdin);
            total += n;
        }
        buf[total] = '\0';
        {
            AthString *str = ath_string_new(buf, (int)total);
            AthValue v = ath_str_val(str);
            free(buf);
            return v;
        }
    } else {
        FILE *f;
        char *buf;
        long size;
        AthValue result;
        REQUIRE_STRING(argv[0], "SCRY", "path");
        f = fopen(argv[0].as.string->data, "rb");
        if (!f) ath_runtime_error_fmt("SCRY: cannot open file '%s'",
                                       argv[0].as.string->data);
        fseek(f, 0, SEEK_END);
        size = ftell(f);
        rewind(f);
        buf = (char*)malloc(size + 1);
        if (!buf) { fclose(f); ath_fatal("out of memory"); }
        fread(buf, 1, size, f);
        fclose(f);
        buf[size] = '\0';
        {
            AthString *str = ath_string_new(buf, (int)size);
            result = ath_str_val(str);
            free(buf);
            return result;
        }
    }
}

AthValue ath_builtin_INSCRIBE(AthScope *s, int argc, AthValue *argv) {
    FILE *f;
    char *content;
    (void)s;
    REQUIRE_ARGC(2, "INSCRIBE");
    REQUIRE_STRING(argv[0], "INSCRIBE", "path");
    f = fopen(argv[0].as.string->data, "wb");
    if (!f) ath_runtime_error_fmt("INSCRIBE: cannot open file '%s'",
                                   argv[0].as.string->data);
    content = ath_stringify(argv[1]);
    fwrite(content, 1, strlen(content), f);
    fclose(f);
    free(content);
    return ath_void();
}

/* ===== Type operations ===== */

AthValue ath_builtin_TYPEOF(AthScope *s, int argc, AthValue *argv) {
    (void)s; REQUIRE_ARGC(1, "TYPEOF");
    return ath_str_cstr(ath_typeof_str(argv[0]));
}

AthValue ath_builtin_LENGTH(AthScope *s, int argc, AthValue *argv) {
    (void)s; REQUIRE_ARGC(1, "LENGTH");
    if (argv[0].type == ATH_STRING)
        return ath_int(argv[0].as.string->length);
    if (argv[0].type == ATH_ARRAY)
        return ath_int(argv[0].as.array->length);
    if (argv[0].type == ATH_BUFFER)
        return ath_int(argv[0].as.buffer ? argv[0].as.buffer->length : 0);
    ath_runtime_error_fmt("LENGTH: expected STRING, ARRAY, or BUFFER, got %s",
                           ath_typeof_str(argv[0]));
    return ath_void();
}

AthValue ath_builtin_COUNT(AthScope *s, int argc, AthValue *argv) {
    (void)s; REQUIRE_ARGC(1, "COUNT");
    if (argv[0].type != ATH_SYLLADEX)
        ath_runtime_error_fmt("COUNT: expected a sylladex, got %s",
                              ath_typeof_str(argv[0]));
    return ath_int(ath_syl_count(argv[0].as.sylladex));
}

AthValue ath_builtin_PARSE_INT(AthScope *s, int argc, AthValue *argv) {
    char *end;
    long v;
    (void)s; REQUIRE_ARGC(1, "PARSE_INT");
    REQUIRE_STRING(argv[0], "PARSE_INT", "value");
    /* reject floats with decimal point */
    if (strchr(argv[0].as.string->data, '.'))
        ath_runtime_error("PARSE_INT: cannot parse float string as integer", 0, 0);
    v = strtol(argv[0].as.string->data, &end, 10);
    if (end == argv[0].as.string->data || *end != '\0')
        ath_runtime_error_fmt("PARSE_INT: cannot parse '%s'",
                               argv[0].as.string->data);
    return ath_int(v);
}

AthValue ath_builtin_PARSE_FLOAT(AthScope *s, int argc, AthValue *argv) {
    char *end;
    double v;
    (void)s; REQUIRE_ARGC(1, "PARSE_FLOAT");
    REQUIRE_STRING(argv[0], "PARSE_FLOAT", "value");
    v = strtod(argv[0].as.string->data, &end);
    if (end == argv[0].as.string->data || *end != '\0')
        ath_runtime_error_fmt("PARSE_FLOAT: cannot parse '%s'",
                               argv[0].as.string->data);
    return ath_float(v);
}

AthValue ath_builtin_STRING(AthScope *s, int argc, AthValue *argv) {
    char *str;
    AthValue result;
    (void)s; REQUIRE_ARGC(1, "STRING");
    str = ath_stringify(argv[0]);
    result = ath_str_cstr(str);
    free(str);
    return result;
}

AthValue ath_builtin_INT(AthScope *s, int argc, AthValue *argv) {
    (void)s; REQUIRE_ARGC(1, "INT");
    if (argv[0].type == ATH_INTEGER) return argv[0];
    if (argv[0].type == ATH_FLOAT)   return ath_int((long)argv[0].as.float_);
    ath_runtime_error_fmt("INT: expected numeric, got %s", ath_typeof_str(argv[0]));
    return ath_void();
}

AthValue ath_builtin_FLOAT(AthScope *s, int argc, AthValue *argv) {
    (void)s; REQUIRE_ARGC(1, "FLOAT");
    if (argv[0].type == ATH_FLOAT)   return argv[0];
    if (argv[0].type == ATH_INTEGER) return ath_float((double)argv[0].as.integer);
    ath_runtime_error_fmt("FLOAT: expected numeric, got %s", ath_typeof_str(argv[0]));
    return ath_void();
}

AthValue ath_builtin_CHAR(AthScope *s, int argc, AthValue *argv) {
    char buf[5];
    int cp, len = 0;
    (void)s; REQUIRE_ARGC(1, "CHAR");
    REQUIRE_INT(argv[0], "CHAR", "codepoint");
    cp = (int)argv[0].as.integer;
    /* Encode as UTF-8 */
    if (cp < 0x80) {
        buf[len++] = (char)cp;
    } else if (cp < 0x800) {
        buf[len++] = (char)(0xC0 | (cp >> 6));
        buf[len++] = (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        buf[len++] = (char)(0xE0 | (cp >> 12));
        buf[len++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[len++] = (char)(0x80 | (cp & 0x3F));
    } else {
        buf[len++] = (char)(0xF0 | (cp >> 18));
        buf[len++] = (char)(0x80 | ((cp >> 12) & 0x3F));
        buf[len++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[len++] = (char)(0x80 | (cp & 0x3F));
    }
    buf[len] = '\0';
    return ath_str_cstr(buf);
}

AthValue ath_builtin_CODE(AthScope *s, int argc, AthValue *argv) {
    const unsigned char *p;
    int cp;
    (void)s; REQUIRE_ARGC(1, "CODE");
    REQUIRE_STRING(argv[0], "CODE", "string");
    if (argv[0].as.string->length == 0)
        ath_runtime_error("CODE: string must not be empty", 0, 0);
    p = (const unsigned char *)argv[0].as.string->data;
    /* Decode first UTF-8 codepoint */
    if (*p < 0x80) {
        cp = *p;
    } else if (*p < 0xE0) {
        cp = ((*p & 0x1F) << 6) | (p[1] & 0x3F);
    } else if (*p < 0xF0) {
        cp = ((*p & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
    } else {
        cp = ((*p & 0x07) << 18) | ((p[1] & 0x3F) << 12) |
             ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
    }
    return ath_int((long)cp);
}

AthValue ath_builtin_BIN(AthScope *s, int argc, AthValue *argv) {
    char buf[70];
    unsigned long v;
    int pos = 0, bit;
    (void)s; REQUIRE_ARGC(1, "BIN");
    REQUIRE_INT(argv[0], "BIN", "value");
    v = (unsigned long)argv[0].as.integer;
    if (v == 0) return ath_str_cstr("0");
    /* Start from the top bit of unsigned long's actual width: 63 on LP64, 31 on Windows (LLP64). Shifting by >= the width is undefined, which previously produced garbage leading bits on Windows. */
    for (bit = (int)(sizeof(unsigned long) * 8) - 1; bit >= 0; bit--)
        if (v & (1UL << bit)) break;
    for (; bit >= 0; bit--)
        buf[pos++] = (v & (1UL << bit)) ? '1' : '0';
    buf[pos] = '\0';
    return ath_str_cstr(buf);
}

AthValue ath_builtin_HEX(AthScope *s, int argc, AthValue *argv) {
    char buf[32];
    (void)s; REQUIRE_ARGC(1, "HEX");
    REQUIRE_INT(argv[0], "HEX", "value");
    sprintf(buf, "%lX", (unsigned long)argv[0].as.integer);
    return ath_str_cstr(buf);
}

/* ===== Array operations ===== */

AthValue ath_builtin_APPEND(AthScope *s, int argc, AthValue *argv) {
    AthArray *dst;
    int i;
    (void)s; REQUIRE_ARGC(2, "APPEND");
    REQUIRE_ARRAY(argv[0], "APPEND", "array");
    dst = ath_array_new(argv[0].as.array->length + 1);
    dst->length = argv[0].as.array->length;
    for (i = 0; i < dst->length; i++) {
        dst->data[i] = argv[0].as.array->data[i];
        ath_value_incref(dst->data[i]);
    }
    dst->data[dst->length] = argv[1];
    ath_value_incref(argv[1]);
    dst->length++;
    return ath_array_val(dst);
}

AthValue ath_builtin_PREPEND(AthScope *s, int argc, AthValue *argv) {
    AthArray *dst;
    int i;
    (void)s; REQUIRE_ARGC(2, "PREPEND");
    REQUIRE_ARRAY(argv[0], "PREPEND", "array");
    dst = ath_array_new(argv[0].as.array->length + 1);
    dst->length = argv[0].as.array->length + 1;
    dst->data[0] = argv[1];
    ath_value_incref(argv[1]);
    for (i = 0; i < argv[0].as.array->length; i++) {
        dst->data[i+1] = argv[0].as.array->data[i];
        ath_value_incref(dst->data[i+1]);
    }
    return ath_array_val(dst);
}

AthValue ath_builtin_SLICE(AthScope *s, int argc, AthValue *argv) {
    AthArray *src, *dst;
    int start, end, i;
    (void)s; REQUIRE_ARGC(3, "SLICE");
    REQUIRE_ARRAY(argv[0], "SLICE", "array");
    REQUIRE_INT(argv[1], "SLICE", "start");
    REQUIRE_INT(argv[2], "SLICE", "end");
    src = argv[0].as.array;
    start = (int)argv[1].as.integer;
    end   = (int)argv[2].as.integer;
    if (start < 0) start = 0;
    if (end > src->length) end = src->length;
    if (start > end) start = end;
    dst = ath_array_new(end - start);
    dst->length = end - start;
    for (i = 0; i < dst->length; i++) {
        dst->data[i] = src->data[start + i];
        ath_value_incref(dst->data[i]);
    }
    return ath_array_val(dst);
}

AthValue ath_builtin_FIRST(AthScope *s, int argc, AthValue *argv) {
    (void)s; REQUIRE_ARGC(1, "FIRST");
    REQUIRE_ARRAY(argv[0], "FIRST", "array");
    if (argv[0].as.array->length == 0)
        ath_runtime_error("FIRST: array is empty", 0, 0);
    /* +1: builtins always return an owned reference (sunk by codegen) */
    ath_value_incref(argv[0].as.array->data[0]);
    return argv[0].as.array->data[0];
}

AthValue ath_builtin_LAST(AthScope *s, int argc, AthValue *argv) {
    (void)s; REQUIRE_ARGC(1, "LAST");
    REQUIRE_ARRAY(argv[0], "LAST", "array");
    if (argv[0].as.array->length == 0)
        ath_runtime_error("LAST: array is empty", 0, 0);
    /* +1: builtins always return an owned reference (sunk by codegen) */
    ath_value_incref(argv[0].as.array->data[argv[0].as.array->length - 1]);
    return argv[0].as.array->data[argv[0].as.array->length - 1];
}

AthValue ath_builtin_CONCAT(AthScope *s, int argc, AthValue *argv) {
    AthArray *a, *b, *dst;
    int i;
    (void)s; REQUIRE_ARGC(2, "CONCAT");
    REQUIRE_ARRAY(argv[0], "CONCAT", "array1");
    REQUIRE_ARRAY(argv[1], "CONCAT", "array2");
    a = argv[0].as.array; b = argv[1].as.array;
    dst = ath_array_new(a->length + b->length);
    dst->length = a->length + b->length;
    for (i = 0; i < a->length; i++) { dst->data[i] = a->data[i]; ath_value_incref(a->data[i]); }
    for (i = 0; i < b->length; i++) { dst->data[a->length+i] = b->data[i]; ath_value_incref(b->data[i]); }
    return ath_array_val(dst);
}

/* ===== Map operations ===== */

AthValue ath_builtin_KEYS(AthScope *s, int argc, AthValue *argv) {
    (void)s; REQUIRE_ARGC(1, "KEYS");
    REQUIRE_MAP(argv[0], "KEYS", "map");
    return ath_array_val(ath_map_keys(argv[0].as.map));
}

AthValue ath_builtin_VALUES(AthScope *s, int argc, AthValue *argv) {
    (void)s; REQUIRE_ARGC(1, "VALUES");
    REQUIRE_MAP(argv[0], "VALUES", "map");
    return ath_array_val(ath_map_values(argv[0].as.map));
}

AthValue ath_builtin_HAS(AthScope *s, int argc, AthValue *argv) {
    (void)s; REQUIRE_ARGC(2, "HAS");
    REQUIRE_MAP(argv[0], "HAS", "map");
    REQUIRE_STRING(argv[1], "HAS", "key");
    return ath_bool(ath_map_has(argv[0].as.map, argv[1].as.string->data));
}

AthValue ath_builtin_SET(AthScope *s, int argc, AthValue *argv) {
    AthMap *dst;
    char *key;
    (void)s; REQUIRE_ARGC(3, "SET");
    REQUIRE_MAP(argv[0], "SET", "map");
    dst = ath_map_copy(argv[0].as.map);
    key = ath_stringify(argv[1]);
    ath_map_set(dst, key, argv[2]);
    free(key);
    return ath_map_val(dst);
}

AthValue ath_builtin_DELETE(AthScope *s, int argc, AthValue *argv) {
    AthMap *dst;
    (void)s; REQUIRE_ARGC(2, "DELETE");
    REQUIRE_MAP(argv[0], "DELETE", "map");
    REQUIRE_STRING(argv[1], "DELETE", "key");
    dst = ath_map_copy(argv[0].as.map);
    ath_map_delete(dst, argv[1].as.string->data);
    return ath_map_val(dst);
}

/* ===== String operations ===== */

AthValue ath_builtin_SPLIT(AthScope *s, int argc, AthValue *argv) {
    AthArray *result;
    const char *str, *delim;
    int str_len, delim_len;
    (void)s; REQUIRE_ARGC(2, "SPLIT");
    REQUIRE_STRING(argv[0], "SPLIT", "string");
    REQUIRE_STRING(argv[1], "SPLIT", "delimiter");
    str       = argv[0].as.string->data;
    str_len   = argv[0].as.string->length;
    delim     = argv[1].as.string->data;
    delim_len = argv[1].as.string->length;
    result = ath_array_new(16);
    if (delim_len == 0) {
        /* split into individual bytes */
        int i;
        for (i = 0; i < str_len; i++) {
            AthString *cs = ath_string_new(&str[i], 1);
            AthValue v = ath_str_val(cs);
            if (result->length >= result->capacity) {
                result->capacity *= 2;
                result->data = (AthValue*)realloc(result->data,
                                                   sizeof(AthValue)*result->capacity);
            }
            result->data[result->length++] = v;
        }
    } else {
        int start = 0, i;
        for (i = 0; i <= str_len - delim_len; ) {
            if (memcmp(str + i, delim, delim_len) == 0) {
                AthString *part = ath_string_new(str + start, i - start);
                AthValue v = ath_str_val(part);
                if (result->length >= result->capacity) {
                    result->capacity *= 2;
                    result->data = (AthValue*)realloc(result->data,
                                                       sizeof(AthValue)*result->capacity);
                }
                result->data[result->length++] = v;
                start = i + delim_len;
                i = start;
            } else {
                i++;
            }
        }
        /* tail */
        {
            AthString *part = ath_string_new(str + start, str_len - start);
            AthValue v = ath_str_val(part);
            if (result->length >= result->capacity) {
                result->capacity *= 2;
                result->data = (AthValue*)realloc(result->data,
                                                   sizeof(AthValue)*result->capacity);
            }
            result->data[result->length++] = v;
        }
    }
    return ath_array_val(result);
}

AthValue ath_builtin_JOIN(AthScope *s, int argc, AthValue *argv) {
    AthArray *arr;
    const char *delim;
    int delim_len, total = 0, i;
    char *buf, *p;
    AthValue result;
    (void)s; REQUIRE_ARGC(2, "JOIN");
    REQUIRE_ARRAY(argv[0], "JOIN", "array");
    REQUIRE_STRING(argv[1], "JOIN", "delimiter");
    arr       = argv[0].as.array;
    delim     = argv[1].as.string->data;
    delim_len = argv[1].as.string->length;
    /* compute total size */
    for (i = 0; i < arr->length; i++) {
        char *s2 = ath_stringify(arr->data[i]);
        total += (int)strlen(s2);
        free(s2);
        if (i < arr->length-1) total += delim_len;
    }
    buf = (char*)malloc(total + 1);
    p = buf;
    for (i = 0; i < arr->length; i++) {
        char *s2 = ath_stringify(arr->data[i]);
        int l = (int)strlen(s2);
        memcpy(p, s2, l); p += l;
        free(s2);
        if (i < arr->length-1) { memcpy(p, delim, delim_len); p += delim_len; }
    }
    *p = '\0';
    {
        AthString *str = ath_string_new(buf, total);
        result = ath_str_val(str);
        free(buf);
        return result;
    }
}

AthValue ath_builtin_SUBSTRING(AthScope *s, int argc, AthValue *argv) {
    int start, end, len;
    (void)s; REQUIRE_ARGC(3, "SUBSTRING");
    REQUIRE_STRING(argv[0], "SUBSTRING", "string");
    REQUIRE_INT(argv[1], "SUBSTRING", "start");
    REQUIRE_INT(argv[2], "SUBSTRING", "end");
    len   = argv[0].as.string->length;
    start = (int)argv[1].as.integer;
    end   = (int)argv[2].as.integer;
    if (start < 0) start = 0;
    if (end > len) end = len;
    if (start > end) start = end;
    {
        AthString *str = ath_string_new(argv[0].as.string->data + start, end - start);
        return ath_str_val(str);
    }
}

AthValue ath_builtin_UPPERCASE(AthScope *s, int argc, AthValue *argv) {
    char *buf;
    int i, len;
    AthString *str;
    (void)s; REQUIRE_ARGC(1, "UPPERCASE");
    REQUIRE_STRING(argv[0], "UPPERCASE", "string");
    len = argv[0].as.string->length;
    buf = (char*)malloc(len + 1);
    for (i = 0; i < len; i++) buf[i] = (char)toupper((unsigned char)argv[0].as.string->data[i]);
    buf[len] = '\0';
    str = ath_string_new(buf, len);
    free(buf);
    return ath_str_val(str);
}

AthValue ath_builtin_LOWERCASE(AthScope *s, int argc, AthValue *argv) {
    char *buf;
    int i, len;
    AthString *str;
    (void)s; REQUIRE_ARGC(1, "LOWERCASE");
    REQUIRE_STRING(argv[0], "LOWERCASE", "string");
    len = argv[0].as.string->length;
    buf = (char*)malloc(len + 1);
    for (i = 0; i < len; i++) buf[i] = (char)tolower((unsigned char)argv[0].as.string->data[i]);
    buf[len] = '\0';
    str = ath_string_new(buf, len);
    free(buf);
    return ath_str_val(str);
}

AthValue ath_builtin_TRIM(AthScope *s, int argc, AthValue *argv) {
    const char *data;
    int start, end;
    (void)s; REQUIRE_ARGC(1, "TRIM");
    REQUIRE_STRING(argv[0], "TRIM", "string");
    data  = argv[0].as.string->data;
    start = 0;
    end   = argv[0].as.string->length;
    while (start < end && isspace((unsigned char)data[start])) start++;
    while (end > start && isspace((unsigned char)data[end-1])) end--;
    {
        AthString *str = ath_string_new(data + start, end - start);
        return ath_str_val(str);
    }
}

AthValue ath_builtin_REPLACE(AthScope *s, int argc, AthValue *argv) {
    const char *src, *old, *new_;
    int src_len, old_len, new_len;
    char *result;
    int result_len = 0, result_cap;
    int i;
    (void)s; REQUIRE_ARGC(3, "REPLACE");
    REQUIRE_STRING(argv[0], "REPLACE", "string");
    REQUIRE_STRING(argv[1], "REPLACE", "old");
    REQUIRE_STRING(argv[2], "REPLACE", "new");
    src     = argv[0].as.string->data;
    src_len = argv[0].as.string->length;
    old     = argv[1].as.string->data;
    old_len = argv[1].as.string->length;
    new_    = argv[2].as.string->data;
    new_len = argv[2].as.string->length;
    if (old_len == 0) { /* no-op for empty old; +1 like every builtin return */
        ath_value_incref(argv[0]);
        return argv[0];
    }
    result_cap = src_len + 64;
    result = (char*)malloc(result_cap);
    for (i = 0; i <= src_len - old_len; ) {
        if (memcmp(src + i, old, old_len) == 0) {
            while (result_len + new_len + 1 > result_cap) {
                result_cap *= 2;
                result = (char*)realloc(result, result_cap);
            }
            memcpy(result + result_len, new_, new_len);
            result_len += new_len;
            i += old_len;
        } else {
            if (result_len + 2 > result_cap) {
                result_cap *= 2;
                result = (char*)realloc(result, result_cap);
            }
            result[result_len++] = src[i++];
        }
    }
    /* append remaining */
    while (i < src_len) {
        if (result_len + 2 > result_cap) {
            result_cap *= 2;
            result = (char*)realloc(result, result_cap);
        }
        result[result_len++] = src[i++];
    }
    result[result_len] = '\0';
    {
        AthString *str = ath_string_new(result, result_len);
        AthValue v = ath_str_val(str);
        free(result);
        return v;
    }
}

/* ===== Utility ===== */

AthValue ath_builtin_RANDOM(AthScope *s, int argc, AthValue *argv) {
    (void)s; (void)argc; (void)argv;
    return ath_float((double)rand() / ((double)RAND_MAX + 1.0));
}

AthValue ath_builtin_RANDOM_INT(AthScope *s, int argc, AthValue *argv) {
    long min_v, max_v, range;
    (void)s; REQUIRE_ARGC(2, "RANDOM_INT");
    REQUIRE_INT(argv[0], "RANDOM_INT", "min");
    REQUIRE_INT(argv[1], "RANDOM_INT", "max");
    min_v = argv[0].as.integer;
    max_v = argv[1].as.integer;
    if (min_v > max_v) ath_runtime_error("RANDOM_INT: min > max", 0, 0);
    range = max_v - min_v + 1;
    return ath_int(min_v + (long)(rand() % (int)range));
}

AthValue ath_builtin_TIME(AthScope *s, int argc, AthValue *argv) {
    (void)s; (void)argc; (void)argv;
    return ath_int((long)ath_eventloop_now_ms());
}

/* ===== BUFFER (FFI) ===== */

AthValue ath_builtin_BUFFER(AthScope *s, int argc, AthValue *argv) {
    long n;
    AthBuffer *b;
    (void)s; REQUIRE_ARGC(1, "BUFFER");
    REQUIRE_INT(argv[0], "BUFFER", "size");
    n = argv[0].as.integer;
    if (n < 0) ath_runtime_error("BUFFER: size must be non-negative", 0, 0);
    b = ath_buffer_new((int)n);
    return ath_buffer_val(b);
}

AthValue ath_builtin_BYTE_AT(AthScope *s, int argc, AthValue *argv) {
    long idx;
    AthBuffer *b;
    (void)s; REQUIRE_ARGC(2, "BYTE_AT");
    if (argv[0].type != ATH_BUFFER)
        ath_runtime_error_fmt("BYTE_AT: first argument must be BUFFER, got %s",
                              ath_typeof_str(argv[0]));
    REQUIRE_INT(argv[1], "BYTE_AT", "index");
    b = argv[0].as.buffer;
    idx = argv[1].as.integer;
    if (!b || !b->bytes || idx < 0 || idx >= b->length)
        ath_runtime_error_fmt("BYTE_AT: index %ld out of range [0, %d)",
                              idx, b ? b->length : 0);
    return ath_int((long)b->bytes[idx]);
}

AthValue ath_builtin_SET_BYTE(AthScope *s, int argc, AthValue *argv) {
    long idx, val;
    AthBuffer *b;
    (void)s; REQUIRE_ARGC(3, "SET_BYTE");
    if (argv[0].type != ATH_BUFFER)
        ath_runtime_error_fmt("SET_BYTE: first argument must be BUFFER, got %s",
                              ath_typeof_str(argv[0]));
    REQUIRE_INT(argv[1], "SET_BYTE", "index");
    REQUIRE_INT(argv[2], "SET_BYTE", "value");
    b = argv[0].as.buffer;
    idx = argv[1].as.integer;
    val = argv[2].as.integer;
    if (!b || !b->bytes || idx < 0 || idx >= b->length)
        ath_runtime_error_fmt("SET_BYTE: index %ld out of range [0, %d)",
                              idx, b ? b->length : 0);
    if (val < 0 || val > 255)
        ath_runtime_error_fmt("SET_BYTE: value %ld out of byte range [0, 255]",
                              val);
    b->bytes[idx] = (unsigned char)val;
    return ath_void();
}

AthValue ath_builtin_BUFFER_TO_STRING(AthScope *s, int argc, AthValue *argv) {
    long n;
    AthBuffer *b;
    AthString *str;
    (void)s;
    if (argc < 1 || argc > 2)
        ath_runtime_error_fmt("BUFFER_TO_STRING: expected 1 or 2 args, got %d", argc);
    if (argv[0].type != ATH_BUFFER)
        ath_runtime_error_fmt("BUFFER_TO_STRING: first argument must be BUFFER, got %s",
                              ath_typeof_str(argv[0]));
    b = argv[0].as.buffer;
    if (argc == 2) {
        REQUIRE_INT(argv[1], "BUFFER_TO_STRING", "length");
        n = argv[1].as.integer;
    } else {
        n = b ? b->length : 0;
    }
    if (n < 0 || (b && n > b->length))
        ath_runtime_error_fmt("BUFFER_TO_STRING: length %ld out of range", n);
    if (n == 0 || !b || !b->bytes) return ath_str_cstr("");
    str = ath_string_new((const char *)b->bytes, (int)n);
    return ath_str_val(str);
}

AthValue ath_builtin_STRING_TO_BUFFER(AthScope *s, int argc, AthValue *argv) {
    AthBuffer *b;
    AthString *str;
    (void)s; REQUIRE_ARGC(1, "STRING_TO_BUFFER");
    REQUIRE_STRING(argv[0], "STRING_TO_BUFFER", "value");
    str = argv[0].as.string;
    b = ath_buffer_new(str ? str->length : 0);
    if (str && str->length > 0)
        memcpy(b->bytes, str->data, (size_t)str->length);
    return ath_buffer_val(b);
}

/* ===== Portals (raw / datagram sockets) ===== */

/* RFC 1071 internet checksum over a BUFFER (or a [off, off+len) slice of it).
   Returns the folded one's-complement 16-bit value to store in the checksum
   field. Protocol-agnostic: for a TCP/UDP transport checksum the caller stages
   the 12-byte pseudo-header + segment (checksum field zeroed) in one BUFFER and
   passes the whole thing. Note the UDP-only rule that a computed 0x0000 is
   transmitted as 0xFFFF is the caller's responsibility (it must NOT be applied
   to the IPv4 header checksum, where 0 is a legal value). */
AthValue ath_builtin_RECKON(AthScope *s, int argc, AthValue *argv) {
    AthBuffer *b;
    long off, len, i, end;
    unsigned long sum = 0;
    (void)s;
    if (argc != 1 && argc != 3)
        ath_runtime_error_fmt("RECKON: expected 1 or 3 args, got %d", argc);
    if (argv[0].type != ATH_BUFFER)
        ath_runtime_error_fmt("RECKON: first argument must be BUFFER, got %s",
                              ath_typeof_str(argv[0]));
    b = argv[0].as.buffer;
    if (argc == 3) {
        REQUIRE_INT(argv[1], "RECKON", "offset");
        REQUIRE_INT(argv[2], "RECKON", "length");
        off = argv[1].as.integer;
        len = argv[2].as.integer;
    } else {
        off = 0;
        len = b ? b->length : 0;
    }
    if (off < 0 || len < 0 || (b && off + len > b->length) || (!b && (off || len)))
        ath_runtime_error_fmt("RECKON: range [%ld, %ld) out of buffer bounds", off, off + len);
    end = off + len;
    for (i = off; i + 1 < end; i += 2)
        sum += ((unsigned long)b->bytes[i] << 8) | (unsigned long)b->bytes[i + 1];
    if ((end - off) & 1)                       /* trailing odd byte, zero-padded */
        sum += (unsigned long)b->bytes[end - 1] << 8;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return ath_int((long)((~sum) & 0xFFFF));
}

AthValue ath_builtin_SENDIFICATE(AthScope *s, int argc, AthValue *argv) {
    AthEntity *e;
    AthBuffer *b;
    char *host;
    long port;
    int sent, len;
    (void)s; REQUIRE_ARGC(4, "SENDIFICATE");
    e = ath_extract_entity(argv[0]);
    if (!e || e->kind != ATH_ENTITY_PORTAL)
        ath_runtime_error("SENDIFICATE: first argument must be a portal", 0, 0);
    if (argv[1].type != ATH_BUFFER)
        ath_runtime_error_fmt("SENDIFICATE: second argument must be BUFFER, got %s",
                              ath_typeof_str(argv[1]));
    REQUIRE_STRING(argv[2], "SENDIFICATE", "host");
    REQUIRE_INT(argv[3], "SENDIFICATE", "port");
    b = argv[1].as.buffer;
    port = argv[3].as.integer;
    host = ath_stringify(argv[2]);
    len = b ? b->length : 0;
    sent = ath_portal_send(e, (b && b->bytes) ? b->bytes : (const unsigned char *)"",
                           len, host, (int)port);
    free(host);
    return ath_int((long)sent);
}

AthValue ath_builtin_APPEARIFY(AthScope *s, int argc, AthValue *argv) {
    AthEntity *e;
    AthBuffer *b;
    int got;
    (void)s; REQUIRE_ARGC(2, "APPEARIFY");
    e = ath_extract_entity(argv[0]);
    if (!e || e->kind != ATH_ENTITY_PORTAL)
        ath_runtime_error("APPEARIFY: first argument must be a portal", 0, 0);
    if (argv[1].type != ATH_BUFFER)
        ath_runtime_error_fmt("APPEARIFY: second argument must be BUFFER, got %s",
                              ath_typeof_str(argv[1]));
    b = argv[1].as.buffer;
    got = ath_portal_recv(e, (b && b->bytes) ? b->bytes : (unsigned char *)"",
                          b ? b->length : 0);
    return ath_int((long)got);
}
