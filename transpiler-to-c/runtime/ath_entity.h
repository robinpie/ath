// SPDX-License-Identifier: GPL-2.0-only
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

/* ath_entity.h -- entity lifecycle (timer, process, connection, watcher, branch, composite) */
#ifndef ATH_ENTITY_H
#define ATH_ENTITY_H

#include "ath_value.h"
#include <stdint.h>

/* forward */
struct AthCont;

typedef enum {
    ATH_ENTITY_THIS       = 0,
    ATH_ENTITY_TIMER      = 1,
    ATH_ENTITY_BRANCH     = 2,
    ATH_ENTITY_PROCESS    = 3,
    ATH_ENTITY_CONNECTION = 4,
    ATH_ENTITY_WATCHER    = 5,
    ATH_ENTITY_AND        = 6,
    ATH_ENTITY_OR         = 7,
    ATH_ENTITY_NOT        = 8,
    ATH_ENTITY_SESSION    = 9
} AthEntityKind;

typedef struct AthWaiter {
    struct AthCont   *cont;
    struct AthWaiter *next;
} AthWaiter;

typedef struct AthEntity {
    int            refcount;
    AthEntityKind  kind;
    const char    *name;    /* owned by scope or static string */
    int            is_dead;
    AthWaiter     *waiters;
    /* timer */
    unsigned long  deadline_ms;
    /* process -- pid_t on POSIX, HANDLE (cast to intptr_t) on Windows */
    intptr_t       pid;
    /* connection -- fd on POSIX, SOCKET (cast to intptr_t) on Windows */
    intptr_t       sockfd;
    /* watcher */
    char          *filepath;
    unsigned long  next_poll_ms;
    /* composite */
    struct AthEntity *left;
    struct AthEntity *right;
    int             and_left_dead;  /* for AND: track which side died */
    int             and_right_dead;
    /* session (weak back-ref; session owns the entity strongly) */
    struct AthSession *session;
} AthEntity;

AthEntity *ath_entity_this_new(void);
AthEntity *ath_entity_timer_new(const char *name, unsigned long ms);
AthEntity *ath_entity_branch_new(const char *name);
AthEntity *ath_entity_process_new(const char *name, const char *cmd, char *const argv[]);
AthEntity *ath_entity_connection_new(const char *name, const char *host, int port);
AthEntity *ath_entity_watcher_new(const char *name, const char *filepath);
AthEntity *ath_entity_and_new(AthEntity *a, AthEntity *b);
AthEntity *ath_entity_or_new(AthEntity *a, AthEntity *b);
AthEntity *ath_entity_not_new(AthEntity *inner);
AthEntity *ath_entity_session_new(const char *name);

void ath_entity_die(AthEntity *e);
void ath_entity_on_death(AthEntity *e, struct AthCont *k);
void ath_entity_incref(AthEntity *e);
void ath_entity_decref(AthEntity *e);

/* Universal DIE on an AthValue. Accepts ATH_ENTITY or ATH_SESSION (which
   forwards to its underlying entity). Any other type is a runtime error. */
void ath_die_value(AthValue v);

/* Pull the underlying AthEntity from an AthValue. Used by entity-expression
   codegen so identifiers bound to either ATH_ENTITY or ATH_SESSION resolve
   to the same entity object. Raises a runtime error for other types. */
struct AthEntity *ath_extract_entity(AthValue v);

/* BANISH on a value. Handles ATH_RELIC (run destructor, clear pointer) and
   ATH_BUFFER (release bytes). Any other type is a runtime error. */
void ath_banish_value(AthValue v);

/* called by the event loop each tick to check process/connection/watcher */
void ath_entity_poll(AthEntity *e, unsigned long now_ms);

/* global list of entities needing polling (process/connection/watcher) */
void ath_entity_register_pollable(AthEntity *e);
void ath_entity_unregister_pollable(AthEntity *e);
void ath_entity_poll_all(unsigned long now_ms);

/* number of live (non-dead) pollable entities -- keeps the event loop alive */
int ath_entity_pending_count(void);

#endif /* ATH_ENTITY_H */
