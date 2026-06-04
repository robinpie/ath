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

/* ath_ffi_signal.h -- best-effort signal handling for foreign calls.

   When a transcribed C call faults (SIGSEGV/SIGBUS/SIGFPE/SIGILL) the
   handler marks the session faulted and longjmps out of ffi_call. The FFI
   layer then turns the fault into a catchable runtime error and triggers
   session death. UNSAFE sessions skip this protection entirely.

   This is best-effort, not isolation: the foreign code may have corrupted
   the runtime before faulting. The conceit "another universe collapsed"
   is a recovery convenience, not a safety boundary. */
#ifndef ATH_FFI_SIGNAL_H
#define ATH_FFI_SIGNAL_H

#include "ath_platform.h"
#include <setjmp.h>

struct AthSession;

extern struct AthSession *ath_ffi_active_session;  /* set around ffi_call */
#if defined(_WIN32) || defined(ATH_WASM)
extern jmp_buf             ath_ffi_jmp;            /* not used (no POSIX signals) */
#else
extern sigjmp_buf          ath_ffi_jmp;            /* current longjmp target */
#endif
extern volatile int       ath_ffi_in_call;         /* 1 while ffi_call runs */

/* Install handlers (idempotent). Called lazily from the FFI layer. */
void ath_ffi_signal_init(void);

#endif /* ATH_FFI_SIGNAL_H */
