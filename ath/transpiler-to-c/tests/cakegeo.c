/* SPDX-License-Identifier: GPL-2.0-only */
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
