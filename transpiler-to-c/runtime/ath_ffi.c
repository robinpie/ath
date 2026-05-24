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

#include "ath_ffi.h"
#include "ath_ffi_signal.h"
#include "ath_session.h"
#include "ath_entity.h"
#include "ath_relic.h"
#include "ath_buffer.h"
#include "ath_error.h"
#include "ath_builtins.h"  /* ath_call_sync (for callback trampoline) */
/* ath_current_rite is declared in ath_value.h */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#ifndef _WIN32
#include <dlfcn.h>
#endif

/* Returns 0 on success, non-zero signal number on fault. Save/restore the
   active-session and jmp_buf globals so nested calls (a callback that
   re-enters FFI) work — only the innermost protected call catches its own
   signal; outer call's jmp_buf is preserved. */
static int _ath_ffi_call_protected(AthFfiSig *sig, void *retbuf, void **arg_ptrs) {
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
    }
    return &ffi_type_void;
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

static void _free_params(AthFfiParam *params, int n) {
    int i;
    if (!params) return;
    for (i = 0; i < n; i++) {
        if (params[i].callback_sig) ath_ffi_sig_free(params[i].callback_sig);
    }
    free(params);
}

AthFfiSig *ath_ffi_sig_create(AthSession *session,
                              const char *symbol_name,
                              const char *ret_type_name,
                              int nparams,
                              const char **param_type_names,
                              const char *drops_name_or_null) {
    AthFfiSig *sig;
    void *sym;
    int i;
    AthFfiParam ret_param;
    const char *cur;
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
#ifndef _WIN32
    dlerror();
    sym = dlsym(session->dlhandle, symbol_name);
    err = dlerror();
    if (err) {
        ath_runtime_error_fmt("session: dlsym(%s): %s", symbol_name, err);
        return NULL;
    }
#else
    sym = NULL;
    ath_runtime_error("session: symbol resolution unsupported on Windows", 0, 0);
    return NULL;
#endif

    cur = ret_type_name ? ret_type_name : "";
    if (_parse_type(&cur, &ret_param) < 0 || *cur != '\0') {
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
            cur = param_type_names[i] ? param_type_names[i] : "";
            if (_parse_type(&cur, &p) < 0 || *cur != '\0') {
                ath_ffi_sig_free(sig);
                ath_runtime_error_fmt("session: unparseable param type '%s' for %s",
                                      param_type_names[i], symbol_name);
                return NULL;
            }
            if (p.tag == ATH_FFI_VOID) {
                if (p.callback_sig) ath_ffi_sig_free(p.callback_sig);
                ath_ffi_sig_free(sig);
                ath_runtime_error_fmt("session: VOID is not a valid parameter type (%s)",
                                      symbol_name);
                return NULL;
            }
            sig->params[i] = p;
            sig->arg_types[i] = libffi_type_for(p.tag);
        }
    }
    sig->ret_type = libffi_type_for(sig->ret.tag);
    if (ffi_prep_cif(&sig->cif, FFI_DEFAULT_ABI,
                     (unsigned int)nparams,
                     sig->ret_type, sig->arg_types) != FFI_OK) {
        ath_ffi_sig_free(sig);
        ath_runtime_error_fmt("session: ffi_prep_cif failed for %s", symbol_name);
        return NULL;
    }
    return sig;
}

void ath_ffi_sig_free(AthFfiSig *sig) {
    if (!sig) return;
    if (--sig->refcount > 0) return;
    _free_params(sig->params, sig->nparams);
    if (sig->arg_types) free(sig->arg_types);
    if (sig->ret.callback_sig) ath_ffi_sig_free(sig->ret.callback_sig);
    if (sig->drops_name) free(sig->drops_name);
    free(sig);
}

/* Each raw arg slot holds one C primitive. We pass &raw[i].<field> as the
   arg pointer to ffi_call. */
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
            /* Hand the foreign pointer back as a "loose" relic (no owner,
               no destructor). It survives session death and BANISH on it
               is a no-op. */
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
        /* C-side gets a malloc'd copy; it's the C caller's job to free
           (or accept the leak). */
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
            /* Loose relics (owner == NULL) are accepted; they come from
               foreign callbacks where we don't know the originating session. */
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
        case ATH_FFI_VOID:
            { int j; for (j = 0; j < nclosures; j++) _ath_ffi_free_closure(&closures[j]); }
            ath_runtime_error_fmt("FFI arg %d: VOID is not a valid argument", i);
            return ath_void();
        }
    }

    {
        int fault = _ath_ffi_call_protected(sig, &retbuf, arg_ptrs);
        if (fault != 0) {
            /* The signal handler set session->faulted. Trigger the death
               path so the teardown continuation runs the fault branch
               (curse relics, skip dlclose, leak the mapping). Closures
               leak here too — the universe is gone. */
            if (sig->session->entity) ath_entity_die(sig->session->entity);
            ath_runtime_error_fmt(
                "foreign universe collapsed: signal %d", fault);
            return ath_void();
        }
    }

    /* Free callback closures now that the call has returned. */
    for (i = 0; i < nclosures; i++) _ath_ffi_free_closure(&closures[i]);

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
    default:
        result = ath_void();
    }
    return result;
}
