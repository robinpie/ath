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
