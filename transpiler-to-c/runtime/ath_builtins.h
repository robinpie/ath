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

/* ath_builtins.h -- all !~ATH built-in rites */
#ifndef ATH_BUILTINS_H
#define ATH_BUILTINS_H

#include "ath_value.h"
#include "ath_scope.h"

/* All builtins have signature: AthValue fn(AthScope*, int argc, AthValue* argv) */
typedef AthValue (*AthBuiltinFn)(AthScope *scope, int argc, AthValue *argv);

/* Lookup a builtin by name; returns NULL if not found */
AthBuiltinFn ath_builtin_lookup(const char *name);

/* Register all builtins as const AthRite values in scope s */
void ath_scope_define_builtins(AthScope *s);

/* Call a value (rite or builtin) synchronously */
AthValue ath_call_sync(AthScope *scope, AthValue callee, int argc, AthValue *argv);

/* Individual builtins (also callable directly) */
AthValue ath_builtin_UTTER(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_HEED(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_SCRY(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_INSCRIBE(AthScope *s, int argc, AthValue *argv);

AthValue ath_builtin_TYPEOF(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_LENGTH(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_COUNT(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_PARSE_INT(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_PARSE_FLOAT(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_STRING(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_INT(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_FLOAT(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_CHAR(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_CODE(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_BIN(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_HEX(AthScope *s, int argc, AthValue *argv);

AthValue ath_builtin_APPEND(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_PREPEND(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_SLICE(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_FIRST(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_LAST(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_CONCAT(AthScope *s, int argc, AthValue *argv);

AthValue ath_builtin_KEYS(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_VALUES(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_HAS(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_SET(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_DELETE(AthScope *s, int argc, AthValue *argv);

AthValue ath_builtin_SPLIT(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_JOIN(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_SUBSTRING(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_UPPERCASE(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_LOWERCASE(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_TRIM(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_REPLACE(AthScope *s, int argc, AthValue *argv);

AthValue ath_builtin_RANDOM(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_RANDOM_INT(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_TIME(AthScope *s, int argc, AthValue *argv);

/* BUFFER (FFI) */
AthValue ath_builtin_BUFFER(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_BYTE_AT(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_SET_BYTE(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_BUFFER_TO_STRING(AthScope *s, int argc, AthValue *argv);
AthValue ath_builtin_STRING_TO_BUFFER(AthScope *s, int argc, AthValue *argv);

#endif /* ATH_BUILTINS_H */
