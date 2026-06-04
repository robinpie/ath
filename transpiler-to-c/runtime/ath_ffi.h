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

/* ath_ffi.h -- libffi marshalling layer for foreign-session calls.
   Each transcribed C function gets an AthFfiSig describing its libffi CIF
   and a wrapping AthRite whose sync function is ath_ffi_invoke. The rite's
   `data` slot points at the sig. */
#ifndef ATH_FFI_H
#define ATH_FFI_H

#include "ath_platform.h"
#if defined(ATH_WASM)
/* libffi is unavailable in the wasi sysroot. Provide stub types so the
   AthFfiSig struct still compiles for the entity/death plumbing; ath_ffi.c
   compiles as a stub TU whose entry points raise a catchable runtime error.
   No FFI call is ever reached (foreign sessions cannot open under WASM). */
typedef struct { int _unused; } ffi_cif;
typedef struct ath_ffi_type_stub ffi_type;
#else
#include <ffi.h>
#endif
#include "ath_value.h"

struct AthSession;

typedef enum {
    ATH_FFI_INTEGER  = 0,
    ATH_FFI_FLOAT    = 1,
    ATH_FFI_BOOLEAN  = 2,
    ATH_FFI_STRING   = 3,
    ATH_FFI_VOID     = 4,
    ATH_FFI_RELIC    = 5,
    ATH_FFI_BUFFER   = 6,
    ATH_FFI_CALLBACK = 7   /* callable parameter -- !~ATH rite wrapped as libffi closure */
} AthFfiTypeTag;

/* A single parameter (or the return) of a transcription. For simple types
   `callback_sig` is NULL; for CALLBACK it points at a sub-sig that
   describes the C signature of the callback. */
typedef struct AthFfiParam {
    AthFfiTypeTag     tag;
    struct AthFfiSig *callback_sig; /* non-NULL iff tag == ATH_FFI_CALLBACK */
} AthFfiParam;

typedef struct AthFfiSig {
    int                refcount;
    int                nparams;
    AthFfiParam       *params;        /* nparams items */
    AthFfiParam        ret;
    ffi_cif            cif;
    ffi_type         **arg_types;     /* nparams items */
    ffi_type          *ret_type;
    void              *symbol;        /* dlsym result; NULL for nested-callback sigs */
    char              *drops_name;    /* NULL if no DROPS clause */
    struct AthSession *session;       /* weak */
} AthFfiSig;

/* Returns one of the ATH_FFI_* tags, or -1 if name is unknown. */
int ath_ffi_tag_from_name(const char *name);

/* Build a sig from string type names. dlsyms the symbol, runs ffi_prep_cif.
   On failure raises a runtime error and returns NULL. */
AthFfiSig *ath_ffi_sig_create(struct AthSession *session,
                              const char *symbol_name,
                              const char *ret_type_name,
                              int nparams,
                              const char **param_type_names,
                              const char *drops_name_or_null);

void ath_ffi_sig_free(AthFfiSig *sig);

/* AthRiteSyncFn shared by all FFI rites. Reads the sig from the current
   rite's `data` slot, marshals args, calls ffi_call, marshals the return. */
AthValue ath_ffi_invoke(struct AthScope *scope, int argc, AthValue *argv);

#endif /* ATH_FFI_H */
