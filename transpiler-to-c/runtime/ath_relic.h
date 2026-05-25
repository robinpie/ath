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

/* ath_relic.h -- opaque foreign pointer minted by a session.
   A relic carries a raw C pointer plus a weak back-reference to the session
   that produced it. When the session dies, all its still-live relics are
   cursed (ptr cleared, cursed flag set) so dangling pointers go inert. */
#ifndef ATH_RELIC_H
#define ATH_RELIC_H

struct AthSession;
struct AthRite;

typedef struct AthRelic {
    int                refcount;
    void              *ptr;            /* raw foreign pointer; NULL if cursed */
    struct AthSession *owner;          /* weak; NULL once owner is gone */
    struct AthRite    *destructor;     /* strong, may be NULL */
    int                cursed;
    int                creation_index; /* assigned by the session on register */
} AthRelic;

AthRelic *ath_relic_new(void *ptr, struct AthSession *owner,
                        struct AthRite *destructor);
void      ath_relic_incref(AthRelic *r);
void      ath_relic_decref(AthRelic *r);
void      ath_relic_curse(AthRelic *r); /* mark cursed, clear ptr/owner */

#endif /* ATH_RELIC_H */
