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

/* ath_error.c */
#include "ath_error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

AthErrorFrame *_ath_error_top = NULL;

void ath_condemn(const char *msg, int line, int col) {
    if (_ath_error_top) {
        free(_ath_error_top->error_msg);
        _ath_error_top->error_msg = (char*)malloc(strlen(msg) + 1);
        strcpy(_ath_error_top->error_msg, msg);
        _ath_error_top->error_line = line;
        _ath_error_top->error_col  = col;
        longjmp(_ath_error_top->env, 1);
    }
    fprintf(stderr, "Unhandled CONDEMN");
    if (line > 0) fprintf(stderr, " [line %d, col %d]", line, col);
    fprintf(stderr, ": %s\n", msg);
    exit(1);
}

void ath_runtime_error(const char *msg, int line, int col) {
    ath_condemn(msg, line, col);
}

void ath_runtime_error_fmt(const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    ath_condemn(buf, 0, 0);
}

void ath_fatal(const char *msg) {
    fprintf(stderr, "Fatal: %s\n", msg);
    exit(1);
}
