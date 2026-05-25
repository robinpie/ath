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

/* ath_buffer.h -- mutable byte buffer for C FFI out-parameters.
   BUFFER is the only mutable value type in !~ATH; mutations are visible
   across all aliases. Memory is freed when refcount drops to zero, or
   explicitly via ath_buffer_release (used by BANISH). */
#ifndef ATH_BUFFER_H
#define ATH_BUFFER_H

typedef struct AthBuffer {
    int             refcount;
    int             length;     /* current allocated byte count */
    unsigned char  *bytes;      /* NULL iff length == 0 */
} AthBuffer;

AthBuffer *ath_buffer_new(int length);
void       ath_buffer_incref(AthBuffer *b);
void       ath_buffer_decref(AthBuffer *b);

/* BANISH semantics: free bytes immediately and zero the length, regardless
   of refcount. Subsequent reads see an empty buffer. */
void       ath_buffer_release(AthBuffer *b);

#endif /* ATH_BUFFER_H */
