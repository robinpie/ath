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

#include "ath_buffer.h"
#include "ath_error.h"
#include <stdlib.h>

AthBuffer *ath_buffer_new(int length) {
    AthBuffer *b;
    if (length < 0) ath_runtime_error("BUFFER size must be non-negative", 0, 0);
    b = (AthBuffer *)malloc(sizeof(AthBuffer));
    if (!b) ath_fatal("out of memory");
    b->refcount = 1;
    b->length = length;
    if (length > 0) {
        b->bytes = (unsigned char *)calloc((size_t)length, 1);
        if (!b->bytes) ath_fatal("out of memory");
    } else {
        b->bytes = NULL;
    }
    return b;
}

void ath_buffer_incref(AthBuffer *b) {
    if (b) b->refcount++;
}

void ath_buffer_decref(AthBuffer *b) {
    if (!b) return;
    if (--b->refcount <= 0) {
        if (b->bytes) free(b->bytes);
        free(b);
    }
}

void ath_buffer_release(AthBuffer *b) {
    if (!b) return;
    if (b->bytes) {
        free(b->bytes);
        b->bytes = NULL;
    }
    b->length = 0;
}
