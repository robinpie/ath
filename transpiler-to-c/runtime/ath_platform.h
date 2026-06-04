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

/* ath_platform.h -- platform detection for the !~ATH C89 runtime.
 *
 * The runtime targets three platforms: POSIX (the default `#else` branch),
 * Win32 (`_WIN32`), and WebAssembly/WASI (`ATH_WASM`). WASM is a 32-bit (LP32)
 * target -- `long` is 4 bytes, identical to the i686 build, so integer
 * semantics need no special handling. What WASM lacks is dlopen/libffi (no
 * foreign sessions), POSIX signals (sessions behave as UNSAFE), and
 * process/socket support (no process/connection entities). Those non-portable
 * areas are gated on ATH_WASM; everything else compiles as-is. */
#ifndef ATH_PLATFORM_H
#define ATH_PLATFORM_H

#if defined(__wasm__) || defined(__wasi__)
#  define ATH_WASM 1
#endif

#endif /* ATH_PLATFORM_H */
