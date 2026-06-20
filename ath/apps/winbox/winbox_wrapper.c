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

/*
 * winbox_wrapper.c -- thin DLL shim around user32 MessageBoxA for the
 * winBoxDemo.~ATH demo.
 *
 * The shim exists because MessageBoxA takes an HWND (pointer-sized) as its
 * first argument, but !~ATH's FFI INTEGER type maps to C `long` which is only
 * 32 bits on Windows x86_64 (LLP64 model).  Passing NULL through a 32-bit
 * slot and into a 64-bit HWND parameter is undefined behaviour.  This wrapper
 * accepts only two strings and the button flags and supplies the NULL HWND
 * internally, giving the !~ATH side a clean all-STRING/INTEGER interface.
 *
 * Build (cross-compile from Linux):
 *   x86_64-w64-mingw32-gcc -shared -O2 winbox_wrapper.c \
 *       -o winbox_wrapper.dll -luser32
 *
 * The demo expects winbox_wrapper.dll to live alongside the compiled .exe.
 */
#include <windows.h>

__declspec(dllexport)
int show_messagebox(const char *text, const char *caption, int buttons) {
    return MessageBoxA(NULL, text ? text : "", caption ? caption : "", (UINT)buttons);
}
