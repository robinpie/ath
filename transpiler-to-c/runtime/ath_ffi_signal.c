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

#include "ath_platform.h"

#if !defined(_WIN32) && !defined(ATH_WASM)
/* Need POSIX 2008 XSI extensions for sigaltstack, stack_t, SA_NODEFER,
   SA_ONSTACK. _GNU_SOURCE is the simplest umbrella on Linux. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif

#include "ath_ffi_signal.h"
#include "ath_session.h"
#include <string.h>

#if !defined(_WIN32) && !defined(ATH_WASM)
#include <signal.h>

struct AthSession *ath_ffi_active_session = NULL;
sigjmp_buf         ath_ffi_jmp;
volatile int       ath_ffi_in_call = 0;

static int _installed = 0;

/* Fixed-size alternate stack. SIGSTKSZ is non-constant on recent glibc, and
   we'd rather pick a known-good size than fight feature-test macros. */
#define ATH_FFI_ALTSTACK_SIZE 65536
static char _altstack[ATH_FFI_ALTSTACK_SIZE];

static void _ath_ffi_handler(int signo, siginfo_t *si, void *uctx) {
    (void)si; (void)uctx;
    if (ath_ffi_in_call && ath_ffi_active_session) {
        ath_ffi_active_session->faulted = 1;
        ath_ffi_in_call = 0;
        siglongjmp(ath_ffi_jmp, signo);
    }
    /* Not our fault: restore default and re-raise to crash normally. */
    signal(signo, SIG_DFL);
    raise(signo);
}

void ath_ffi_signal_init(void) {
    struct sigaction sa;
    stack_t           ss;

    if (_installed) return;
    _installed = 1;

    ss.ss_sp    = _altstack;
    ss.ss_size  = sizeof(_altstack);
    ss.ss_flags = 0;
    sigaltstack(&ss, NULL);

    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = _ath_ffi_handler;
    sa.sa_flags     = SA_SIGINFO | SA_NODEFER | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS,  &sa, NULL);
    sigaction(SIGFPE,  &sa, NULL);
    sigaction(SIGILL,  &sa, NULL);
}

#else  /* _WIN32 or ATH_WASM: no POSIX signals */

struct AthSession *ath_ffi_active_session = NULL;
jmp_buf            ath_ffi_jmp;   /* not used; kept to satisfy the extern */
volatile int       ath_ffi_in_call = 0;

void ath_ffi_signal_init(void) {
    /* No SEH/signal integration. Sessions behave as UNSAFE. */
}

#endif
