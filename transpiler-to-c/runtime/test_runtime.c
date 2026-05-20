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

/* test_runtime.c — hand-written CPS test of the runtime.
   Equivalent to:
     import timer T(5ms);
     ~ATH(T) {} EXECUTE(UTTER("Hello from C runtime!"));
     THIS.DIE();
*/
#include "ath_runtime.h"
#include <stdio.h>
#include <stdlib.h>

/* --- Forward declarations --- */
typedef struct MainFrame MainFrame;
static void main_seg1(AthCont *self, AthValue result);
static void main_seg0(AthCont *self, AthValue result);

/* --- Frame struct --- */
struct MainFrame {
    AthCont   base;
    AthScope *scope;
    AthValue  T_entity;
};

static MainFrame *main_frame_new(AthCont *next, AthScope *scope) {
    MainFrame *f = (MainFrame *)malloc(sizeof(MainFrame));
    f->base.resume   = main_seg0;
    f->base.next     = next;
    f->base.refcount = 1;
    f->scope         = scope;
    ath_scope_incref(scope);
    return f;
}

static void main_frame_free(MainFrame *f) {
    ath_scope_decref(f->scope);
    free(f);
}

/* seg1: EXECUTE body — UTTER("Hello from C runtime!") + THIS.DIE() */
static void main_seg1(AthCont *self, AthValue result) {
    MainFrame *f = (MainFrame *)self;
    AthValue args[1];
    AthValue this_val;
    (void)result;

    /* UTTER("Hello from C runtime!") */
    args[0] = ath_str_cstr("Hello from C runtime!");
    ath_builtin_UTTER(f->scope, 1, args);
    ath_value_decref(args[0]);

    /* THIS.DIE() */
    this_val = ath_scope_get(f->scope, "THIS");
    ath_entity_die(this_val.as.entity);

    main_frame_free(f);
}

/* seg0: import timer T(5ms), ~ATH(T){} EXECUTE(seg1) */
static void main_seg0(AthCont *self, AthValue result) {
    MainFrame *f = (MainFrame *)self;
    AthEntity *T;
    (void)result;

    /* import timer T(5ms) */
    T = ath_entity_timer_new("T", 5);
    f->T_entity = ath_entity_val(T);
    ath_scope_define(f->scope, "T", f->T_entity, 0);

    /* ~ATH(T) {} EXECUTE(seg1) — register seg1 as waiter on T */
    f->base.resume = main_seg1;
    ath_entity_on_death(T, (AthCont*)f);

    /* Do NOT free frame here; it will be called by the event loop */
}

int main(void) {
    AthScope  *global;
    AthEntity *this_ent;
    MainFrame *frame;

    ath_eventloop_init();
    srand(42);

    global   = ath_scope_new(NULL);
    this_ent = ath_entity_this_new();
    ath_scope_define(global, "THIS", ath_entity_val(this_ent), 0);

    frame = main_frame_new(NULL, global);
    ath_eventloop_schedule((AthCont*)frame, ath_void());
    ath_eventloop_run();

    ath_scope_decref(global);
    return 0;
}
