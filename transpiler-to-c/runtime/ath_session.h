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

/* ath_session.h -- loaded shared library + transcribed rites.
   Sessions are the !~ATH FFI mechanism: a session value carries a dlopen
   handle, a map of transcribed rites (callable as M.foo(...)), and a
   registry of relics minted by those rites. Sessions are paired with an
   AthEntity so they participate in death waiting via ~ATH(M). */
#ifndef ATH_SESSION_H
#define ATH_SESSION_H

struct AthEntity;
struct AthMap;
struct AthRelic;

typedef struct AthSession {
    int                refcount;
    struct AthEntity  *entity;       /* strong */
    struct AthMap     *rites;        /* strong; transcribed-name -> ATH_RITE */
    void              *dlhandle;     /* dlopen handle, owned; closed at teardown */
    struct AthRelic  **relics;       /* weak refs */
    int                relic_count;
    int                relic_cap;
    int                next_creation_index;
    int                dying;        /* set by ath_entity_die; blocks new FFI calls */
    int                faulted;      /* signal-handler escalation flag */
    int                unsafe;       /* skip signal-handler protection */
} AthSession;

AthSession *ath_session_new(struct AthEntity *entity, void *dlhandle, int unsafe);
void        ath_session_incref(AthSession *s);
void        ath_session_decref(AthSession *s);

/* High-level constructor: dlopen the library, build the entity + session,
   and link them. Returns NULL on dlopen failure (entity is not created).
   The returned AthSession has refcount 1; the caller is the sole owner. */
AthSession *ath_session_create(const char *name, const char *libpath, int unsafe);

/* Resolve `symbol_name` in the session's library, build a libffi sig, wrap
   it as an AthRite, and store under `symbol_name` in session->rites. All
   type names are bare strings ("INTEGER", "STRING", "RELIC", ...). On
   failure raises a runtime error. */
void ath_session_transcribe(AthSession *s,
                            const char *symbol_name,
                            const char *ret_type_name,
                            int nparams,
                            const char **param_type_names,
                            const char *drops_name_or_null);

/* Relic registry. Relics call register on construction and unregister on
   their own destruction. The session does not own the relics; it just keeps
   weak pointers so it can curse them on death. */
void ath_session_register_relic(AthSession *s, struct AthRelic *r);
void ath_session_unregister_relic(AthSession *s, struct AthRelic *r);

/* Queue the session's teardown continuation. ath_entity_die calls this
   after marking the session dying; the continuation runs at the next
   event-loop tick. Orderly teardown runs DROPS destructors in reverse
   creation order then dlcloses; fault teardown (session->faulted set by
   the signal handler) curses every relic, skips destructors, and leaks
   the mapping. Either way, the session's entity is marked dead and
   ~ATH(M) waiters fire only after teardown completes. */
void ath_session_schedule_teardown(AthSession *s);

#endif /* ATH_SESSION_H */
