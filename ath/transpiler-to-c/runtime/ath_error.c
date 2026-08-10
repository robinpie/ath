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

/* Bounded message formatting.
 *
 * vsnprintf is C99, so under -std=c89 we roll our own tiny bounded formatter
 * instead of vsprintf (which has no bound and happily runs off the stack when
 * a message interpolates a user-controlled string). Only the conversions that
 * call sites actually use are supported: %s, %c, %d, %ld, %lu and %%. Anything
 * else is copied through verbatim and stops the scan, so an unknown conversion
 * can never make us pull the wrong type out of the va_list.
 *
 * Messages longer than the buffer are truncated and marked with a trailing
 * "...", never written past the end.
 */
#define ATH_ERR_MSG_CAP  512      /* buffer size, including the NUL */
#define ATH_ERR_NUM_CAP   32      /* room for any long, sign included */

/* Append n bytes of s, clipping at the buffer's capacity. *len is the current
 * message length (always < cap, and buf is always NUL-terminated). *clipped is
 * raised if anything had to be dropped. */
static void _ath_msg_append(char *buf, size_t cap, size_t *len,
                            const char *s, size_t n, int *clipped) {
    size_t room = cap - 1 - *len;
    if (n > room) { n = room; *clipped = 1; }
    if (n) {
        memcpy(buf + *len, s, n);
        *len += n;
        buf[*len] = '\0';
    }
}

/* Render v in decimal into out (no NUL); returns the digit count. */
static size_t _ath_msg_ulong(char *out, unsigned long v) {
    char tmp[ATH_ERR_NUM_CAP];
    size_t n = 0, i;
    if (v == 0UL) tmp[n++] = '0';
    while (v != 0UL) { tmp[n++] = (char)('0' + (int)(v % 10UL)); v /= 10UL; }
    for (i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    return n;
}

/* Same, signed. Negation goes through unsigned so LONG_MIN is well defined. */
static size_t _ath_msg_long(char *out, long v) {
    unsigned long u;
    size_t n = 0;
    if (v < 0) { out[n++] = '-'; u = (unsigned long)(-(v + 1)) + 1UL; }
    else       { u = (unsigned long)v; }
    return n + _ath_msg_ulong(out + n, u);
}

void ath_runtime_error_fmt(const char *fmt, ...) {
    char buf[ATH_ERR_MSG_CAP];
    char num[ATH_ERR_NUM_CAP];
    va_list ap;
    const char *p = fmt;
    size_t len = 0;
    int clipped = 0;

    buf[0] = '\0';
    va_start(ap, fmt);
    while (*p) {
        if (*p != '%') {
            const char *run = p;
            while (*p && *p != '%') p++;
            _ath_msg_append(buf, sizeof buf, &len, run, (size_t)(p - run), &clipped);
        } else if (p[1] == '%') {
            _ath_msg_append(buf, sizeof buf, &len, "%", (size_t)1, &clipped);
            p += 2;
        } else if (p[1] == 's') {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            _ath_msg_append(buf, sizeof buf, &len, s, strlen(s), &clipped);
            p += 2;
        } else if (p[1] == 'c') {
            char c = (char)va_arg(ap, int);
            _ath_msg_append(buf, sizeof buf, &len, &c, (size_t)1, &clipped);
            p += 2;
        } else if (p[1] == 'd') {
            size_t n = _ath_msg_long(num, (long)va_arg(ap, int));
            _ath_msg_append(buf, sizeof buf, &len, num, n, &clipped);
            p += 2;
        } else if (p[1] == 'l' && p[2] == 'd') {
            size_t n = _ath_msg_long(num, va_arg(ap, long));
            _ath_msg_append(buf, sizeof buf, &len, num, n, &clipped);
            p += 3;
        } else if (p[1] == 'l' && p[2] == 'u') {
            size_t n = _ath_msg_ulong(num, va_arg(ap, unsigned long));
            _ath_msg_append(buf, sizeof buf, &len, num, n, &clipped);
            p += 3;
        } else {
            /* Unsupported conversion: emit the rest literally and stop, rather
             * than guess at the argument type and desync the va_list. */
            _ath_msg_append(buf, sizeof buf, &len, p, strlen(p), &clipped);
            break;
        }
    }
    va_end(ap);

    if (clipped) {
        buf[len - 3] = '.';
        buf[len - 2] = '.';
        buf[len - 1] = '.';
    }
    ath_condemn(buf, 0, 0);
}

void ath_fatal(const char *msg) {
    fprintf(stderr, "Fatal: %s\n", msg);
    exit(1);
}
