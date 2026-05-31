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

#include "ath_relic.h"
#include "ath_session.h"
#include "ath_value.h"
#include "ath_error.h"
#include <stdlib.h>

AthRelic *ath_relic_new(void *ptr, AthSession *owner, AthRite *destructor) {
    AthRelic *r = (AthRelic *)malloc(sizeof(AthRelic));
    if (!r) ath_fatal("out of memory");
    r->refcount = 1;
    r->ptr = ptr;
    r->owner = owner;
    r->destructor = destructor;
    if (destructor) ath_rite_incref(destructor);
    r->cursed = 0;
    r->creation_index = 0;
    if (owner) ath_session_register_relic(owner, r);
    return r;
}

void ath_relic_incref(AthRelic *r) {
    if (r) r->refcount++;
}

void ath_relic_decref(AthRelic *r) {
    if (!r) return;
    if (--r->refcount <= 0) {
        if (r->owner) ath_session_unregister_relic(r->owner, r);
        if (r->destructor) ath_rite_decref(r->destructor);
        free(r);
    }
}

void ath_relic_curse(AthRelic *r) {
    if (!r) return;
    r->cursed = 1;
    r->ptr = NULL;
    r->owner = NULL;
}
