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

/* ath_error.h -- error handling (setjmp/longjmp) for ATTEMPT/SALVAGE/CONDEMN */
#ifndef ATH_ERROR_H
#define ATH_ERROR_H

#include <setjmp.h>

typedef struct AthErrorFrame {
    jmp_buf               env;
    char                 *error_msg;  /* malloc'd; NULL if no error yet */
    int                   error_line;
    int                   error_col;
    struct AthErrorFrame *prev;
} AthErrorFrame;

extern AthErrorFrame *_ath_error_top;

/* Install a new error frame. If setjmp returns 0, body runs. If it returns 1, a CONDEMN was thrown and error_msg is set. */
#define ATH_ATTEMPT_BEGIN(frame) \
    do { \
        (frame).error_msg = NULL; \
        (frame).prev = _ath_error_top; \
        _ath_error_top = &(frame); \
    } while(0); \
    if (setjmp((frame).env) == 0)

#define ATH_ATTEMPT_END(frame) \
    _ath_error_top = (frame).prev

#define ATH_SALVAGE_BEGIN(frame) \
    else { \
        _ath_error_top = (frame).prev;

#define ATH_SALVAGE_END(frame) \
    }

/* Throw an error to the nearest SALVAGE */
void ath_condemn(const char *msg, int line, int col);

/* Internal: raise a runtime error (goes to nearest SALVAGE or terminates) */
void ath_runtime_error(const char *msg, int line, int col);
void ath_runtime_error_fmt(const char *fmt, ...);
void ath_fatal(const char *msg); /* unrecoverable; prints and exits */

#endif /* ATH_ERROR_H */
