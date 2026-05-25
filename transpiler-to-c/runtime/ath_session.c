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

#include "ath_session.h"
#include "ath_relic.h"
#include "ath_entity.h"
#include "ath_value.h"
#include "ath_error.h"
#include "ath_eventloop.h"
#include "ath_builtins.h"  /* ath_call_sync */
#include "ath_ffi.h"
#include <stdlib.h>
#include <stdio.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static void _ffi_sig_release(void *data) {
    ath_ffi_sig_free((AthFfiSig *)data);
}

AthSession *ath_session_new(AthEntity *entity, void *dlhandle, int unsafe) {
    AthSession *s = (AthSession *)malloc(sizeof(AthSession));
    if (!s) ath_fatal("out of memory");
    s->refcount = 1;
    s->entity = entity;
    if (entity) ath_entity_incref(entity);
    s->rites = ath_map_new(8);
    s->dlhandle = dlhandle;
    s->relics = NULL;
    s->relic_count = 0;
    s->relic_cap = 0;
    s->next_creation_index = 0;
    s->dying = 0;
    s->faulted = 0;
    s->unsafe = unsafe;
    return s;
}

void ath_session_incref(AthSession *s) {
    if (s) s->refcount++;
}

void ath_session_decref(AthSession *s) {
    int i;
    if (!s) return;
    if (--s->refcount > 0) return;
    /* Curse any still-registered relics so dangling weak pointers go inert. */
    for (i = 0; i < s->relic_count; i++) {
        if (s->relics[i]) {
            s->relics[i]->cursed = 1;
            s->relics[i]->ptr = NULL;
            s->relics[i]->owner = NULL;
        }
    }
    if (s->relics) free(s->relics);
    if (s->rites) ath_map_decref(s->rites);
    if (s->entity) ath_entity_decref(s->entity);
    /* Note: dlhandle is not closed here. dlclose happens in
       ath_session_schedule_teardown, which runs while refcount is still > 0. */
    free(s);
}

void ath_session_register_relic(AthSession *s, AthRelic *r) {
    if (!s || !r) return;
    if (s->relic_count >= s->relic_cap) {
        int newcap = s->relic_cap == 0 ? 4 : s->relic_cap * 2;
        AthRelic **nr = (AthRelic **)realloc(s->relics,
                                             sizeof(AthRelic *) * newcap);
        if (!nr) ath_fatal("out of memory");
        s->relics = nr;
        s->relic_cap = newcap;
    }
    r->creation_index = s->next_creation_index++;
    s->relics[s->relic_count++] = r;
}

void ath_session_unregister_relic(AthSession *s, AthRelic *r) {
    int i, j;
    if (!s || !r) return;
    for (i = 0; i < s->relic_count; i++) {
        if (s->relics[i] == r) {
            for (j = i; j < s->relic_count - 1; j++) {
                s->relics[j] = s->relics[j + 1];
            }
            s->relic_count--;
            return;
        }
    }
}

/* ===== Two-phase death =====
   ath_entity_die marks the session dying (blocking new FFI calls) and
   schedules _session_teardown via the event loop. The continuation runs
   destructors and dlcloses at the loop top so it can't trample a foreign
   call still on the C stack. */

typedef struct {
    AthCont     base;
    AthSession *session;
} _SessionTeardownCont;

static void _session_teardown(AthCont *self, AthValue unused) {
    _SessionTeardownCont *c = (_SessionTeardownCont *)self;
    AthSession *s = c->session;
    int i;
    AthEntity *e;
    AthWaiter *w;
    (void)unused;

    if (!s->faulted) {
        /* Orderly path: run destructors LIFO. The relic registry is append-
           only with shift-down on unregister, so reverse-array order is
           reverse creation order. Each destructor is allowed to call back
           into the session, so we clear `dying` for the duration. If a
           destructor itself raises, escalate to fault death and stop. */
        for (i = s->relic_count - 1; i >= 0; i--) {
            AthRelic *r = s->relics[i];
            if (!r || r->cursed) continue;
            if (r->destructor) {
                /* volatile: these are read after the longjmp from ATTEMPT,
                   so the compiler must not assume the values are still in
                   registers. */
                volatile AthValue       dv = ath_rite_val(r->destructor);
                volatile AthValue       arg = ath_relic_val(r);
                AthErrorFrame  ef;
                int            old_dying = s->dying;
                s->dying = 0;
                {
                    AthValue dv_local  = dv;
                    AthValue arg_local = arg;
                    ath_value_incref(dv_local);
                    ath_value_incref(arg_local);
                }
                ATH_ATTEMPT_BEGIN(ef) {
                    AthValue dv_local  = dv;
                    AthValue arg_local = arg;
                    ath_call_sync(NULL, dv_local, 1, &arg_local);
                    ATH_ATTEMPT_END(ef);
                } ATH_SALVAGE_BEGIN(ef) {
                    if (ef.error_msg) free(ef.error_msg);
                    s->faulted = 1;
                    ATH_SALVAGE_END(ef);
                }
                {
                    AthValue dv_local  = dv;
                    AthValue arg_local = arg;
                    ath_value_decref(arg_local);
                    ath_value_decref(dv_local);
                }
                s->dying = old_dying;
                if (s->faulted) {
                    /* Escalation: curse this relic and everything below it
                       (we walked relics[i..count-1] already in earlier
                       iterations, or about to). */
                    int j;
                    for (j = 0; j <= i; j++) {
                        if (s->relics[j]) ath_relic_curse(s->relics[j]);
                    }
                    break;
                }
            }
            ath_relic_curse(r);
        }
    }
    if (s->faulted) {
        /* Fault path: curse everything still uncursed; leak the mapping. */
        for (i = 0; i < s->relic_count; i++) {
            if (s->relics[i]) ath_relic_curse(s->relics[i]);
        }
    } else if (s->dlhandle) {
#ifdef _WIN32
        FreeLibrary((HMODULE)s->dlhandle);
#else
        dlclose(s->dlhandle);
#endif
        s->dlhandle = NULL;
    }

    /* Mark entity dead and fire waiters last, so ~ATH(M) observers only
       see the session after every destructor has run and the library is
       gone. */
    e = s->entity;
    if (e && !e->is_dead) {
        e->is_dead = 1;
        w = e->waiters;
        e->waiters = NULL;
        while (w) {
            AthWaiter *next = w->next;
            ath_eventloop_schedule(w->cont, ath_void());
            free(w);
            w = next;
        }
    }

    ath_session_decref(s); /* drop the schedule-time ref */
    free(c);
}

void ath_session_schedule_teardown(AthSession *s) {
    _SessionTeardownCont *c;
    if (!s) return;
    c = (_SessionTeardownCont *)malloc(sizeof(*c));
    if (!c) ath_fatal("out of memory");
    c->base.resume   = _session_teardown;
    c->base.next     = NULL;
    c->base.refcount = 1;
    c->session       = s;
    ath_session_incref(s); /* keep the session alive until teardown runs */
    ath_eventloop_schedule((AthCont *)c, ath_void());
}

void ath_session_transcribe(AthSession *s,
                            const char *symbol_name,
                            const char *ret_type_name,
                            int nparams,
                            const char **param_type_names,
                            const char *drops_name_or_null) {
    AthFfiSig *sig;
    AthRite   *rite;
    AthValue   v;
    if (!s) {
        ath_runtime_error("session: transcribe on NULL session", 0, 0);
        return;
    }
    sig = ath_ffi_sig_create(s, symbol_name, ret_type_name,
                             nparams, param_type_names, drops_name_or_null);
    if (!sig) return; /* runtime error already raised */
    rite = ath_rite_new_ffi(NULL, ath_ffi_invoke, nparams,
                            sig, _ffi_sig_release);
    v = ath_rite_val(rite);
    ath_map_set(s->rites, symbol_name, v);
    ath_rite_decref(rite); /* map now holds the ref */
}

AthSession *ath_session_create(const char *name, const char *libpath, int unsafe) {
    void *handle;
    AthEntity *entity;
    AthSession *session;
#ifdef _WIN32
    handle = (void *)LoadLibraryA(libpath);
    if (!handle) {
        ath_runtime_error_fmt("session: LoadLibraryA failed for '%s' (error %lu)",
                              libpath, (unsigned long)GetLastError());
        return NULL;
    }
#else
    handle = dlopen(libpath, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        ath_runtime_error_fmt("session: dlopen failed: %s", dlerror());
        return NULL;
    }
#endif
    entity = ath_entity_session_new(name);
    session = ath_session_new(entity, handle, unsafe);
    entity->session = session;
    ath_entity_decref(entity);
    return session;
}
