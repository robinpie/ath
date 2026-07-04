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

/* Fixture library for the ffi__TestRecipe by-value struct round-trip test.
   Built into tests/work/libcakegeo.so by the test Makefile; the case opens it
   as "../work/libcakegeo.so" (cases run with cwd = tests/cases). */

struct Point { int x; int y; };

struct Point translate(struct Point p, long dx, long dy) {
    struct Point r;
    r.x = p.x + (int)dx;
    r.y = p.y + (int)dy;
    return r;
}

long dist2(struct Point p) {
    return (long)p.x * p.x + (long)p.y * p.y;
}
