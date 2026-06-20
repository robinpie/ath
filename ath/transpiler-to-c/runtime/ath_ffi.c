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

#if !defined(_WIN32)
/* sigjmp_buf / sigsetjmp are POSIX/XSI extensions; _GNU_SOURCE pulls them in. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif

#include "ath_platform.h"
#include "ath_ffi.h"
#include "ath_ffi_signal.h"
#include "ath_session.h"
#include "ath_entity.h"
#include "ath_relic.h"
#include "ath_buffer.h"
#include "ath_error.h"
#include "ath_cake.h"      /* AthRecipe, recipe-typed by-value structs */
#include "ath_scope.h"     /* ath_scope_get for dotted recipe resolution */
#include "ath_builtins.h"  /* ath_call_sync (for callback trampoline) */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if !defined(ATH_WASM)
/* === Real FFI implementation (POSIX + Windows). Under WASM, libffi is unavailable in the wasi sysroot, so this whole TU collapses to the stub entry points at the bottom of the file. === */

#include <setjmp.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

/* Returns 0 on success, non-zero signal number on fault. On Windows there are no POSIX signals, so all sessions behave as UNSAFE. */
static int _ath_ffi_call_protected(AthFfiSig *sig, void *retbuf, void **arg_ptrs) {
#ifdef _WIN32
    /* No POSIX signal handling on Windows -- always call directly. */
    ffi_call(&sig->cif, FFI_FN(sig->symbol), retbuf, arg_ptrs);
    return 0;
#else
    sigjmp_buf saved_jmp;
    struct AthSession *prev_session;
    int prev_in_call;
    int signo;

    if (sig->session->unsafe) {
        ffi_call(&sig->cif, FFI_FN(sig->symbol), retbuf, arg_ptrs);
        return 0;
    }
    ath_ffi_signal_init();
    prev_session = ath_ffi_active_session;
    prev_in_call = ath_ffi_in_call;
    memcpy(&saved_jmp, &ath_ffi_jmp, sizeof(saved_jmp));
    signo = sigsetjmp(ath_ffi_jmp, 1);
    if (signo == 0) {
        ath_ffi_active_session = sig->session;
        ath_ffi_in_call = 1;
        ffi_call(&sig->cif, FFI_FN(sig->symbol), retbuf, arg_ptrs);
    }
    ath_ffi_in_call = prev_in_call;
    ath_ffi_active_session = prev_session;
    memcpy(&ath_ffi_jmp, &saved_jmp, sizeof(saved_jmp));
    return signo;
#endif
}

#define ATH_FFI_MAX_ARGS 16

int ath_ffi_tag_from_name(const char *name) {
    if (!name) return -1;
    if (strcmp(name, "INTEGER")  == 0) return ATH_FFI_INTEGER;
    if (strcmp(name, "FLOAT")    == 0) return ATH_FFI_FLOAT;
    if (strcmp(name, "BOOLEAN")  == 0) return ATH_FFI_BOOLEAN;
    if (strcmp(name, "STRING")   == 0) return ATH_FFI_STRING;
    if (strcmp(name, "VOID")     == 0) return ATH_FFI_VOID;
    if (strcmp(name, "RELIC")    == 0) return ATH_FFI_RELIC;
    if (strcmp(name, "BUFFER")   == 0) return ATH_FFI_BUFFER;
    if (strcmp(name, "CALLBACK") == 0) return ATH_FFI_CALLBACK;
    return -1;
}

static ffi_type *libffi_type_for(AthFfiTypeTag tag) {
    switch (tag) {
    case ATH_FFI_INTEGER:  return &ffi_type_slong;
    case ATH_FFI_FLOAT:    return &ffi_type_double;
    case ATH_FFI_BOOLEAN:  return &ffi_type_sint;
    case ATH_FFI_STRING:   return &ffi_type_pointer;
    case ATH_FFI_VOID:     return &ffi_type_void;
    case ATH_FFI_RELIC:    return &ffi_type_pointer;
    case ATH_FFI_BUFFER:   return &ffi_type_pointer;
    case ATH_FFI_CALLBACK: return &ffi_type_pointer;
    case ATH_FFI_RECIPE:   return &ffi_type_void; /* replaced by struct_type */
    }
    return &ffi_type_void;
}

/* ===== Recipe (by-value struct) -> libffi aggregate ffi_type =====

   A recipe lays out like a C aggregate. libffi has no array or union type, so
   we expand arrays into repeated elements and represent nested recipes as
   nested FFI_TYPE_STRUCT ffi_types (preserving their tail padding). The host
   ABI must reproduce the recipe's mandated layout; ath_ffi_sig_create asserts
   the libffi-computed size equals recipe->size and rejects mismatches. */

static ffi_type *ck_leaf_ffi(CkType *t) {
    switch (t->tag) {
    case CK_T_PINCH:   return t->is_signed ? &ffi_type_sint8  : &ffi_type_uint8;
    case CK_T_DASH:    return t->is_signed ? &ffi_type_sint16 : &ffi_type_uint16;
    case CK_T_SPOON:   return t->is_signed ? &ffi_type_sint32 : &ffi_type_uint32;
    case CK_T_CUP:     return t->is_signed ? &ffi_type_sint64 : &ffi_type_uint64;
    case CK_T_DROP:    return &ffi_type_float;
    case CK_T_DOLLOP:  return &ffi_type_double;
    case CK_T_INTEGER: return &ffi_type_slong;
    case CK_T_BOOLEAN: return &ffi_type_uint8;
    case CK_T_STRING:
    case CK_T_RELIC:
    case CK_T_CRUST:   return &ffi_type_pointer;
    default:           return NULL;
    }
}

static ffi_type *ck_build_ffi_type(AthRecipe *r, int *ok);

/* Number of libffi element slots contributed by one ingredient type. */
static int ck_type_elem_count(CkType *t) {
    if (t->tag == CK_T_ARRAY) return (int)t->array_count * ck_type_elem_count(t->elem);
    return 1; /* scalar, crust, or nested (a single nested struct ffi_type) */
}

/* Append the ffi_type element(s) for one type into elems[*idx]. Returns 0 ok. */
static int ck_fill_type(CkType *t, ffi_type **elems, int *idx) {
    if (t->tag == CK_T_ARRAY) {
        int k;
        for (k = 0; k < t->array_count; k++)
            if (ck_fill_type(t->elem, elems, idx) != 0) return -1;
        return 0;
    }
    if (t->tag == CK_T_NESTED) {
        int ok = 1;
        ffi_type *st = ck_build_ffi_type(t->nested, &ok);
        if (!ok) return -1;
        elems[(*idx)++] = st;
        return 0;
    }
    {
        ffi_type *p = ck_leaf_ffi(t);
        if (!p) return -1;
        elems[(*idx)++] = p;
        return 0;
    }
}

/* Recursively build a heap FFI_TYPE_STRUCT for a struct recipe. *ok is set to 0
   on any unrepresentable feature (union, IMPERIAL, empty, bad leaf). */
static ffi_type *ck_build_ffi_type(AthRecipe *r, int *ok) {
    int total = 0, i, idx = 0;
    ffi_type *t;
    ffi_type **elems;
    if (r->kind != CK_KIND_STRUCT || r->imperial) { *ok = 0; return NULL; }
    for (i = 0; i < r->n_ingredients; i++)
        total += ck_type_elem_count(r->ingredients[i].type);
    if (total <= 0) { *ok = 0; return NULL; }
    elems = (ffi_type **)calloc((size_t)total + 1, sizeof(ffi_type *));
    if (!elems) ath_fatal("out of memory");
    for (i = 0; i < r->n_ingredients; i++) {
        if (ck_fill_type(r->ingredients[i].type, elems, &idx) != 0) {
            free(elems); *ok = 0; return NULL;
        }
    }
    elems[total] = NULL;
    t = (ffi_type *)calloc(1, sizeof(ffi_type));
    if (!t) ath_fatal("out of memory");
    t->type = FFI_TYPE_STRUCT;
    t->elements = elems;
    return t;
}

/* Free a heap struct ffi_type (and nested ones); leaves static primitives. */
static void ck_free_ffi_type(ffi_type *t) {
    int i;
    if (!t || t->type != FFI_TYPE_STRUCT || !t->elements) return;
    for (i = 0; t->elements[i]; i++) ck_free_ffi_type(t->elements[i]);
    free(t->elements);
    free(t);
}

/* Resolve a transcription type name to a recipe via the scope.
   Accepts "Recipe" or "Module.Recipe". Returns NULL if not a recipe. */
static AthRecipe *ck_resolve_recipe_name(AthScope *scope, const char *name) {
    const char *dot;
    AthValue v;
    if (!scope || !name) return NULL;
    dot = strchr(name, '.');
    if (dot) {
        char mod[128];
        size_t n = (size_t)(dot - name);
        if (n == 0 || n >= sizeof(mod)) return NULL;
        memcpy(mod, name, n); mod[n] = '\0';
        v = ath_scope_get(scope, mod);
        if (v.type != ATH_MODULE && v.type != ATH_MAP) return NULL;
        v = ath_map_get(v.as.map, dot + 1);
    } else {
        v = ath_scope_get(scope, name);
    }
    return (v.type == ATH_RECIPE) ? v.as.recipe : NULL;
}

/* ===== Nested type-spec parser =====

   Grammar:
     type    := simple | callback
     simple  := "INTEGER" | "FLOAT" | "BOOLEAN" | "STRING" | "VOID"
              | "RELIC"   | "BUFFER"
     callback:= "CALLBACK" "(" [ type { "," type } ] ")" "->" type

   Strings are emitted by the codegen, so no whitespace ever appears inside
   them; the parser doesn't bother skipping any. */

static AthFfiSig *_sig_alloc_callback(int nparams);
static void _sig_finalize_cif(AthFfiSig *sig);
static int  _parse_type(const char **cur, AthFfiParam *out);

static int _match_keyword(const char **cur, const char *kw) {
    size_t n = strlen(kw);
    if (strncmp(*cur, kw, n) != 0) return 0;
    *cur += n;
    return 1;
}

static int _parse_callback(const char **cur, AthFfiParam *out) {
    AthFfiSig *cb;
    AthFfiParam params[ATH_FFI_MAX_ARGS];
    AthFfiParam ret;
    int nparams = 0;
    int i;
    if (**cur != '(') return -1;
    (*cur)++;
    if (**cur != ')') {
        for (;;) {
            if (nparams >= ATH_FFI_MAX_ARGS) return -1;
            if (_parse_type(cur, &params[nparams]) < 0) return -1;
            nparams++;
            if (**cur == ',') { (*cur)++; continue; }
            break;
        }
    }
    if (**cur != ')') return -1;
    (*cur)++;
    if (!_match_keyword(cur, "->")) return -1;
    if (_parse_type(cur, &ret) < 0) return -1;
    cb = _sig_alloc_callback(nparams);
    for (i = 0; i < nparams; i++) {
        cb->params[i] = params[i];
        cb->arg_types[i] = libffi_type_for(params[i].tag);
    }
    cb->ret = ret;
    cb->ret_type = libffi_type_for(ret.tag);
    _sig_finalize_cif(cb);
    out->tag = ATH_FFI_CALLBACK;
    out->callback_sig = cb;
    return 0;
}

static int _parse_type(const char **cur, AthFfiParam *out) {
    out->callback_sig = NULL;
    out->recipe = NULL;
    out->struct_type = NULL;
    if (_match_keyword(cur, "INTEGER")) { out->tag = ATH_FFI_INTEGER; return 0; }
    if (_match_keyword(cur, "FLOAT"))   { out->tag = ATH_FFI_FLOAT;   return 0; }
    if (_match_keyword(cur, "BOOLEAN")) { out->tag = ATH_FFI_BOOLEAN; return 0; }
    if (_match_keyword(cur, "STRING"))  { out->tag = ATH_FFI_STRING;  return 0; }
    if (_match_keyword(cur, "VOID"))    { out->tag = ATH_FFI_VOID;    return 0; }
    if (_match_keyword(cur, "RELIC"))   { out->tag = ATH_FFI_RELIC;   return 0; }
    if (_match_keyword(cur, "BUFFER"))  { out->tag = ATH_FFI_BUFFER;  return 0; }
    if (_match_keyword(cur, "CALLBACK")) return _parse_callback(cur, out);
    return -1;
}

static AthFfiSig *_sig_alloc_callback(int nparams) {
    AthFfiSig *sig = (AthFfiSig *)calloc(1, sizeof(AthFfiSig));
    if (!sig) ath_fatal("out of memory");
    sig->refcount = 1;
    sig->nparams = nparams;
    if (nparams > 0) {
        sig->params = (AthFfiParam *)calloc((size_t)nparams, sizeof(AthFfiParam));
        sig->arg_types = (ffi_type **)calloc((size_t)nparams, sizeof(ffi_type *));
        if (!sig->params || !sig->arg_types) ath_fatal("out of memory");
    }
    return sig;
}

static void _sig_finalize_cif(AthFfiSig *sig) {
    if (ffi_prep_cif(&sig->cif, FFI_DEFAULT_ABI,
                     (unsigned int)sig->nparams,
                     sig->ret_type, sig->arg_types) != FFI_OK) {
        ath_runtime_error("FFI: ffi_prep_cif failed", 0, 0);
    }
}

static void _free_param_contents(AthFfiParam *p) {
    if (p->callback_sig) ath_ffi_sig_free(p->callback_sig);
    if (p->struct_type) ck_free_ffi_type(p->struct_type);
    if (p->recipe) ath_recipe_decref(p->recipe);
}

static void _free_params(AthFfiParam *params, int n) {
    int i;
    if (!params) return;
    for (i = 0; i < n; i++) _free_param_contents(&params[i]);
    free(params);
}

/* Resolve one type name into a param: try the FFI keyword grammar first, then
   fall back to a !^CAKE recipe lookup (by-value struct). Returns 0 ok, -1 on
   unparseable, -2 on recipe that cannot be a host-ABI FFI type. */
static int _resolve_param(AthScope *scope, const char *type_name,
                          AthFfiParam *out, int is_return) {
    const char *cur = type_name ? type_name : "";
    AthRecipe *rec;
    int ok = 1;
    if (_parse_type(&cur, out) == 0 && *cur == '\0') return 0;
    /* not a keyword type -- try a recipe */
    rec = ck_resolve_recipe_name(scope, type_name);
    if (!rec) return -1;
    out->callback_sig = NULL;
    out->tag = ATH_FFI_RECIPE;
    out->recipe = rec;
    ath_recipe_incref(rec);
    out->struct_type = ck_build_ffi_type(rec, &ok);
    if (!ok) { ath_recipe_decref(rec); out->recipe = NULL; return -2; }
    (void)is_return;
    return 0;
}

AthFfiSig *ath_ffi_sig_create(AthSession *session,
                              AthScope *scope,
                              const char *symbol_name,
                              const char *ret_type_name,
                              int nparams,
                              const char **param_type_names,
                              const char *drops_name_or_null) {
    AthFfiSig *sig;
    void *sym;
    int i;
    AthFfiParam ret_param;
    int rr;
#ifndef _WIN32
    char *err;
#endif

    if (!session || !session->dlhandle) {
        ath_runtime_error("session: cannot transcribe; library not loaded", 0, 0);
        return NULL;
    }
    if (nparams > ATH_FFI_MAX_ARGS) {
        ath_runtime_error_fmt("session: %s has %d params (max %d)",
                              symbol_name, nparams, ATH_FFI_MAX_ARGS);
        return NULL;
    }
#ifdef _WIN32
    sym = (void *)GetProcAddress((HMODULE)session->dlhandle, symbol_name);
    if (!sym) {
        ath_runtime_error_fmt("session: GetProcAddress(%s) failed (error %lu)",
                              symbol_name, (unsigned long)GetLastError());
        return NULL;
    }
#else
    dlerror();
    sym = dlsym(session->dlhandle, symbol_name);
    {
        char *err = dlerror();
        if (err) {
            ath_runtime_error_fmt("session: dlsym(%s): %s", symbol_name, err);
            return NULL;
        }
    }
#endif

    rr = _resolve_param(scope, ret_type_name, &ret_param, 1);
    if (rr == -2) {
        ath_runtime_error_fmt("session: recipe return type '%s' is not a valid FFI type (%s)",
                              ret_type_name, symbol_name);
        return NULL;
    }
    if (rr < 0) {
        ath_runtime_error_fmt("session: unparseable return type '%s' for %s",
                              ret_type_name, symbol_name);
        return NULL;
    }
    if (ret_param.tag == ATH_FFI_CALLBACK) {
        if (ret_param.callback_sig) ath_ffi_sig_free(ret_param.callback_sig);
        ath_runtime_error_fmt("session: CALLBACK return type not supported (%s)",
                              symbol_name);
        return NULL;
    }

    sig = (AthFfiSig *)calloc(1, sizeof(AthFfiSig));
    if (!sig) ath_fatal("out of memory");
    sig->refcount = 1;
    sig->nparams = nparams;
    sig->ret = ret_param;
    sig->symbol = sym;
    sig->session = session;
    if (drops_name_or_null && drops_name_or_null[0]) {
        sig->drops_name = (char *)malloc(strlen(drops_name_or_null) + 1);
        if (!sig->drops_name) ath_fatal("out of memory");
        strcpy(sig->drops_name, drops_name_or_null);
    }
    if (nparams > 0) {
        sig->params = (AthFfiParam *)calloc((size_t)nparams, sizeof(AthFfiParam));
        sig->arg_types = (ffi_type **)calloc((size_t)nparams, sizeof(ffi_type *));
        if (!sig->params || !sig->arg_types) ath_fatal("out of memory");
        for (i = 0; i < nparams; i++) {
            AthFfiParam p;
            int pr = _resolve_param(scope, param_type_names[i], &p, 0);
            if (pr == -2) {
                ath_ffi_sig_free(sig);
                ath_runtime_error_fmt("session: recipe param type '%s' is not a valid FFI type (%s)",
                                      param_type_names[i], symbol_name);
                return NULL;
            }
            if (pr < 0) {
                ath_ffi_sig_free(sig);
                ath_runtime_error_fmt("session: unparseable param type '%s' for %s",
                                      param_type_names[i], symbol_name);
                return NULL;
            }
            if (p.tag == ATH_FFI_VOID) {
                _free_param_contents(&p);
                ath_ffi_sig_free(sig);
                ath_runtime_error_fmt("session: VOID is not a valid parameter type (%s)",
                                      symbol_name);
                return NULL;
            }
            sig->params[i] = p;
            sig->arg_types[i] = (p.tag == ATH_FFI_RECIPE)
                                ? p.struct_type : libffi_type_for(p.tag);
        }
    }
    sig->ret_type = (sig->ret.tag == ATH_FFI_RECIPE)
                    ? sig->ret.struct_type : libffi_type_for(sig->ret.tag);
    if (ffi_prep_cif(&sig->cif, FFI_DEFAULT_ABI,
                     (unsigned int)nparams,
                     sig->ret_type, sig->arg_types) != FFI_OK) {
        ath_ffi_sig_free(sig);
        ath_runtime_error_fmt("session: ffi_prep_cif failed for %s", symbol_name);
        return NULL;
    }
    /* ffi_prep_cif has now filled in each struct ffi_type's size. The host ABI
       must reproduce the recipe's mandated layout, or the struct cannot cross
       the boundary intact. */
    if (sig->ret.tag == ATH_FFI_RECIPE &&
        (int)sig->ret.struct_type->size != sig->ret.recipe->size) {
        ath_ffi_sig_free(sig);
        ath_runtime_error_fmt("session: recipe return layout does not match host ABI (%s)",
                              symbol_name);
        return NULL;
    }
    for (i = 0; i < nparams; i++) {
        if (sig->params[i].tag == ATH_FFI_RECIPE &&
            (int)sig->params[i].struct_type->size != sig->params[i].recipe->size) {
            ath_ffi_sig_free(sig);
            ath_runtime_error_fmt("session: recipe param layout does not match host ABI (%s)",
                                  symbol_name);
            return NULL;
        }
    }
    return sig;
}

void ath_ffi_sig_free(AthFfiSig *sig) {
    if (!sig) return;
    if (--sig->refcount > 0) return;
    _free_params(sig->params, sig->nparams);
    if (sig->arg_types) free(sig->arg_types);
    _free_param_contents(&sig->ret);
    if (sig->drops_name) free(sig->drops_name);
    free(sig);
}

/* Each raw arg slot holds one C primitive. We pass &raw[i].<field> as the arg pointer to ffi_call. */
typedef union {
    long          i;
    double        f;
    int           b;
    const char   *s;
    void         *p;
} AthFfiRaw;

/* Per-call callback closures: one slot per CALLBACK argument. */
typedef struct {
    ffi_closure *closure;
    void        *code;
    AthRite     *rite;
    AthFfiSig   *cb_sig;
} AthFfiClosureSlot;

static void _ath_ffi_trampoline(ffi_cif *cif, void *ret, void **args, void *user_data) {
    AthFfiClosureSlot *slot = (AthFfiClosureSlot *)user_data;
    AthFfiSig         *cs   = slot->cb_sig;
    AthValue           argv[ATH_FFI_MAX_ARGS];
    AthValue           rv;
    AthValue           rite_val;
    int                i;
    (void)cif;
    for (i = 0; i < cs->nparams; i++) {
        switch (cs->params[i].tag) {
        case ATH_FFI_INTEGER: argv[i] = ath_int(*(long *)args[i]); break;
        case ATH_FFI_FLOAT:   argv[i] = ath_float(*(double *)args[i]); break;
        case ATH_FFI_BOOLEAN: argv[i] = ath_bool(*(int *)args[i] != 0); break;
        case ATH_FFI_STRING: {
            const char *s = *(const char **)args[i];
            argv[i] = ath_str_cstr(s ? s : "");
            break;
        }
        case ATH_FFI_RELIC: {
            /* Hand the foreign pointer back as a "loose" relic (no owner, no destructor). It survives session death and BANISH on it is a no-op. */
            void     *p = *(void **)args[i];
            AthRelic *r = ath_relic_new(p, NULL, NULL);
            argv[i] = ath_relic_val(r);
            break;
        }
        case ATH_FFI_BUFFER:
        case ATH_FFI_CALLBACK:
        case ATH_FFI_VOID:
        default:
            argv[i] = ath_void();
            break;
        }
    }
    rite_val = ath_rite_val(slot->rite);
    ath_value_incref(rite_val); /* match the decref pattern below */
    rv = ath_call_sync(slot->rite->closure, rite_val, cs->nparams, argv);
    ath_value_decref(rite_val);
    for (i = 0; i < cs->nparams; i++) {
        if (argv[i].type == ATH_STRING || argv[i].type == ATH_RELIC)
            ath_value_decref(argv[i]);
    }
    switch (cs->ret.tag) {
    case ATH_FFI_INTEGER:
        *(long *)ret = (rv.type == ATH_INTEGER) ? rv.as.integer :
                       (rv.type == ATH_BOOLEAN) ? rv.as.integer : 0;
        break;
    case ATH_FFI_FLOAT:
        *(double *)ret = (rv.type == ATH_FLOAT)   ? rv.as.float_ :
                         (rv.type == ATH_INTEGER) ? (double)rv.as.integer : 0;
        break;
    case ATH_FFI_BOOLEAN:
        *(int *)ret = ath_is_truthy(rv) ? 1 : 0;
        break;
    case ATH_FFI_VOID:
        break;
    case ATH_FFI_STRING: {
        /* C-side gets a malloc'd copy; it's the C caller's job to free (or accept the leak). */
        char *out = NULL;
        if (rv.type == ATH_STRING && rv.as.string && rv.as.string->length > 0) {
            out = (char *)malloc((size_t)rv.as.string->length + 1);
            if (out) {
                memcpy(out, rv.as.string->data, (size_t)rv.as.string->length);
                out[rv.as.string->length] = '\0';
            }
        }
        *(char **)ret = out;
        break;
    }
    case ATH_FFI_RELIC: {
        void *p = NULL;
        if (rv.type == ATH_RELIC && rv.as.relic && !rv.as.relic->cursed)
            p = rv.as.relic->ptr;
        *(void **)ret = p;
        break;
    }
    default:
        memset(ret, 0, sizeof(void *));
        break;
    }
    ath_value_decref(rv);
}

static int _ath_ffi_make_closure(AthRite *rite, AthFfiSig *cb_sig,
                                 AthFfiClosureSlot *slot) {
    slot->closure = (ffi_closure *)ffi_closure_alloc(sizeof(ffi_closure), &slot->code);
    if (!slot->closure) return -1;
    slot->rite   = rite;
    slot->cb_sig = cb_sig;
    ath_rite_incref(rite);
    if (ffi_prep_closure_loc(slot->closure, &cb_sig->cif,
                             _ath_ffi_trampoline, slot, slot->code) != FFI_OK) {
        ath_rite_decref(rite);
        ffi_closure_free(slot->closure);
        slot->closure = NULL;
        return -1;
    }
    return 0;
}

static void _ath_ffi_free_closure(AthFfiClosureSlot *slot) {
    if (!slot->closure) return;
    ffi_closure_free(slot->closure);
    if (slot->rite) ath_rite_decref(slot->rite);
    slot->closure = NULL;
    slot->rite = NULL;
}

AthValue ath_ffi_invoke(struct AthScope *scope, int argc, AthValue *argv) {
    AthRite *self = ath_current_rite();
    AthFfiSig *sig;
    int i;
    AthFfiRaw raw[ATH_FFI_MAX_ARGS];
    void *arg_ptrs[ATH_FFI_MAX_ARGS];
    AthFfiClosureSlot closures[ATH_FFI_MAX_ARGS];
    int nclosures = 0;
    AthFfiRaw retbuf;
    AthValue result;
    (void)scope;

    for (i = 0; i < ATH_FFI_MAX_ARGS; i++) {
        closures[i].closure = NULL;
        closures[i].rite = NULL;
    }

    if (!self || !self->data) {
        ath_runtime_error("FFI invocation has no signature", 0, 0);
        return ath_void();
    }
    sig = (AthFfiSig *)self->data;
    if (!sig->session) {
        ath_runtime_error("FFI: session is gone", 0, 0);
        return ath_void();
    }
    if (sig->session->dying) {
        ath_runtime_error("FFI: session is dying", 0, 0);
        return ath_void();
    }
    if (argc != sig->nparams) {
        ath_runtime_error_fmt("FFI: expected %d arguments, got %d",
                              sig->nparams, argc);
        return ath_void();
    }

    for (i = 0; i < argc; i++) {
        AthValue v = argv[i];
        switch (sig->params[i].tag) {
        case ATH_FFI_INTEGER:
            if (v.type != ATH_INTEGER) {
                int j; for (j = 0; j < nclosures; j++) _ath_ffi_free_closure(&closures[j]);
                ath_runtime_error_fmt("FFI arg %d: expected INTEGER, got %s",
                                      i, ath_typeof_str(v));
                return ath_void();
            }
            raw[i].i = v.as.integer;
            arg_ptrs[i] = &raw[i].i;
            break;
        case ATH_FFI_FLOAT:
            if (v.type == ATH_FLOAT)        raw[i].f = v.as.float_;
            else if (v.type == ATH_INTEGER) raw[i].f = (double)v.as.integer;
            else {
                int j; for (j = 0; j < nclosures; j++) _ath_ffi_free_closure(&closures[j]);
                ath_runtime_error_fmt("FFI arg %d: expected FLOAT, got %s",
                                      i, ath_typeof_str(v));
                return ath_void();
            }
            arg_ptrs[i] = &raw[i].f;
            break;
        case ATH_FFI_BOOLEAN:
            if (v.type != ATH_BOOLEAN) {
                int j; for (j = 0; j < nclosures; j++) _ath_ffi_free_closure(&closures[j]);
                ath_runtime_error_fmt("FFI arg %d: expected BOOLEAN, got %s",
                                      i, ath_typeof_str(v));
                return ath_void();
            }
            raw[i].b = (int)v.as.integer;
            arg_ptrs[i] = &raw[i].b;
            break;
        case ATH_FFI_STRING:
            if (v.type != ATH_STRING) {
                int j; for (j = 0; j < nclosures; j++) _ath_ffi_free_closure(&closures[j]);
                ath_runtime_error_fmt("FFI arg %d: expected STRING, got %s",
                                      i, ath_typeof_str(v));
                return ath_void();
            }
            raw[i].s = v.as.string ? v.as.string->data : "";
            arg_ptrs[i] = &raw[i].s;
            break;
        case ATH_FFI_RELIC:
            if (v.type != ATH_RELIC) {
                int j; for (j = 0; j < nclosures; j++) _ath_ffi_free_closure(&closures[j]);
                ath_runtime_error_fmt("FFI arg %d: expected RELIC, got %s",
                                      i, ath_typeof_str(v));
                return ath_void();
            }
            if (!v.as.relic || v.as.relic->cursed) {
                int j; for (j = 0; j < nclosures; j++) _ath_ffi_free_closure(&closures[j]);
                ath_runtime_error_fmt("FFI arg %d: relic is cursed", i);
                return ath_void();
            }
            /* Loose relics (owner == NULL) are accepted; they come from foreign callbacks where we don't know the originating session. */
            if (v.as.relic->owner != NULL &&
                v.as.relic->owner != sig->session) {
                int j; for (j = 0; j < nclosures; j++) _ath_ffi_free_closure(&closures[j]);
                ath_runtime_error_fmt(
                    "FFI arg %d: relic belongs to a different session", i);
                return ath_void();
            }
            raw[i].p = v.as.relic->ptr;
            arg_ptrs[i] = &raw[i].p;
            break;
        case ATH_FFI_BUFFER:
            if (v.type != ATH_BUFFER) {
                int j; for (j = 0; j < nclosures; j++) _ath_ffi_free_closure(&closures[j]);
                ath_runtime_error_fmt("FFI arg %d: expected BUFFER, got %s",
                                      i, ath_typeof_str(v));
                return ath_void();
            }
            raw[i].p = (v.as.buffer && v.as.buffer->bytes)
                       ? (void *)v.as.buffer->bytes : NULL;
            arg_ptrs[i] = &raw[i].p;
            break;
        case ATH_FFI_CALLBACK:
            if (v.type != ATH_RITE) {
                int j; for (j = 0; j < nclosures; j++) _ath_ffi_free_closure(&closures[j]);
                ath_runtime_error_fmt("FFI arg %d: expected RITE for CALLBACK, got %s",
                                      i, ath_typeof_str(v));
                return ath_void();
            }
            if (_ath_ffi_make_closure(v.as.rite,
                                      sig->params[i].callback_sig,
                                      &closures[nclosures]) != 0) {
                int j; for (j = 0; j < nclosures; j++) _ath_ffi_free_closure(&closures[j]);
                ath_runtime_error_fmt("FFI arg %d: failed to allocate callback closure", i);
                return ath_void();
            }
            raw[i].p = closures[nclosures].code;
            arg_ptrs[i] = &raw[i].p;
            nclosures++;
            break;
        case ATH_FFI_RECIPE:
            if (v.type != ATH_BUFFER) {
                int j; for (j = 0; j < nclosures; j++) _ath_ffi_free_closure(&closures[j]);
                ath_runtime_error_fmt("FFI arg %d: expected a baked BUFFER, got %s",
                                      i, ath_typeof_str(v));
                return ath_void();
            }
            if (!v.as.buffer || v.as.buffer->length != sig->params[i].recipe->size) {
                int j; for (j = 0; j < nclosures; j++) _ath_ffi_free_closure(&closures[j]);
                ath_runtime_error_fmt("RAW: FFI arg %d: buffer size %d does not match recipe size %d",
                                      i, v.as.buffer ? v.as.buffer->length : 0,
                                      sig->params[i].recipe->size);
                return ath_void();
            }
            /* libffi reads the struct's bytes through the arg pointer. */
            arg_ptrs[i] = (void *)v.as.buffer->bytes;
            break;
        case ATH_FFI_VOID:
            { int j; for (j = 0; j < nclosures; j++) _ath_ffi_free_closure(&closures[j]); }
            ath_runtime_error_fmt("FFI arg %d: VOID is not a valid argument", i);
            return ath_void();
        }
    }

    /* A by-value struct return needs storage at least the struct's size; the
       small AthFfiRaw union cannot hold it. Use a scratch allocation. */
    {
        void *retptr;
        void *ret_scratch = NULL;
        int fault;
        if (sig->ret.tag == ATH_FFI_RECIPE) {
            int rsz = sig->ret.recipe->size;
            size_t alloc = (size_t)(rsz > (int)sizeof(AthFfiRaw) ? rsz : (int)sizeof(AthFfiRaw));
            ret_scratch = malloc(alloc);
            if (!ret_scratch) ath_fatal("out of memory");
            retptr = ret_scratch;
        } else {
            retptr = &retbuf;
        }
        fault = _ath_ffi_call_protected(sig, retptr, arg_ptrs);
        if (fault != 0) {
            /* The signal handler set session->faulted. Trigger the death path so the teardown continuation runs the fault branch (curse relics, skip dlclose, leak the mapping). Closures leak here too -- the universe is gone. */
            if (ret_scratch) free(ret_scratch);
            if (sig->session->entity) ath_entity_die(sig->session->entity);
            ath_runtime_error_fmt(
                "foreign universe collapsed: signal %d", fault);
            return ath_void();
        }
        /* Free callback closures now that the call has returned. */
        for (i = 0; i < nclosures; i++) _ath_ffi_free_closure(&closures[i]);
        if (sig->ret.tag == ATH_FFI_RECIPE) {
            AthBuffer *out = ath_buffer_new(sig->ret.recipe->size);
            if (sig->ret.recipe->size > 0)
                memcpy(out->bytes, ret_scratch, (size_t)sig->ret.recipe->size);
            free(ret_scratch);
            return ath_buffer_val(out);
        }
    }

    switch (sig->ret.tag) {
    case ATH_FFI_INTEGER: result = ath_int(retbuf.i); break;
    case ATH_FFI_FLOAT:   result = ath_float(retbuf.f); break;
    case ATH_FFI_BOOLEAN: result = ath_bool(retbuf.b != 0); break;
    case ATH_FFI_STRING: {
        const char *s = (const char *)retbuf.p;
        result = ath_str_cstr(s ? s : "");
        break;
    }
    case ATH_FFI_VOID:    result = ath_void(); break;
    case ATH_FFI_RELIC: {
        AthRite *destructor = NULL;
        AthRelic *r;
        if (sig->drops_name && sig->session->rites) {
            AthValue dv = ath_map_get(sig->session->rites, sig->drops_name);
            if (dv.type == ATH_RITE) destructor = dv.as.rite;
        }
        r = ath_relic_new(retbuf.p, sig->session, destructor);
        result = ath_relic_val(r); /* result owns the single ref from ath_relic_new */
        break;
    }
    case ATH_FFI_BUFFER:
        ath_runtime_error("FFI: BUFFER return type not supported", 0, 0);
        return ath_void();
    case ATH_FFI_CALLBACK:
        ath_runtime_error("FFI: CALLBACK return type not supported", 0, 0);
        return ath_void();
    case ATH_FFI_RECIPE:   /* handled before this switch via early return */
    default:
        result = ath_void();
    }
    return result;
}

#else /* ATH_WASM -- stub TU: no libffi, no foreign calls reachable. */

int ath_ffi_tag_from_name(const char *name) {
    if (!name) return -1;
    if (strcmp(name, "INTEGER")  == 0) return ATH_FFI_INTEGER;
    if (strcmp(name, "FLOAT")    == 0) return ATH_FFI_FLOAT;
    if (strcmp(name, "BOOLEAN")  == 0) return ATH_FFI_BOOLEAN;
    if (strcmp(name, "STRING")   == 0) return ATH_FFI_STRING;
    if (strcmp(name, "VOID")     == 0) return ATH_FFI_VOID;
    if (strcmp(name, "RELIC")    == 0) return ATH_FFI_RELIC;
    if (strcmp(name, "BUFFER")   == 0) return ATH_FFI_BUFFER;
    if (strcmp(name, "CALLBACK") == 0) return ATH_FFI_CALLBACK;
    return -1;
}

AthFfiSig *ath_ffi_sig_create(struct AthSession *session,
                              AthScope *scope,
                              const char *symbol_name,
                              const char *ret_type_name,
                              int nparams,
                              const char **param_type_names,
                              const char *drops_name_or_null) {
    (void)session; (void)scope; (void)symbol_name; (void)ret_type_name;
    (void)nparams; (void)param_type_names; (void)drops_name_or_null;
    ath_runtime_error("FFI: foreign sessions are not supported in WASM", 0, 0);
    return NULL;
}

void ath_ffi_sig_free(AthFfiSig *sig) {
    (void)sig; /* never created under WASM */
}

AthValue ath_ffi_invoke(struct AthScope *scope, int argc, AthValue *argv) {
    (void)scope; (void)argc; (void)argv;
    ath_runtime_error("FFI: foreign sessions are not supported in WASM", 0, 0);
    return ath_void();
}

#endif /* ATH_WASM */
