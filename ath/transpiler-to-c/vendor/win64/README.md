<!-- SPDX-License-Identifier: GPL-2.0-only -->
# vendor/win64 — Windows cross-compilation dependencies

This directory contains pre-built Windows (x86_64) libraries needed to
cross-compile the !~ATH runtime and transpiler for Windows using the
`mingw-w64` toolchain on Linux.

## Contents

```
vendor/win64/
└── libffi/
    ├── include/
    │   ├── ffi.h
    │   └── ffitarget.h
    └── lib/
        └── libffi.a      (static library, linked into athtoc-bin-win64.exe)
```

## Source

`libffi.a` and the headers were extracted from the MSYS2 package:

```
mingw-w64-x86_64-libffi-3.4.8-1-any.pkg.tar.zst
```

Downloaded from: https://repo.msys2.org/mingw/mingw64/

To update:

```bash
curl -LO https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-libffi-<ver>-any.pkg.tar.zst
mkdir -p /tmp/new-libffi && tar -I zstd -xf *.zst -C /tmp/new-libffi
cp /tmp/new-libffi/mingw64/include/ffi*.h      vendor/win64/libffi/include/
cp /tmp/new-libffi/mingw64/lib/libffi.a        vendor/win64/libffi/lib/
```

## Usage

The `bin-win64` Makefile target uses these files automatically:

```bash
make bin-win64   # builds athtoc-bin-win64.exe
```

## Runtime DLL

The runtime embeds `libffi` statically (`libffi.a`), so the resulting
`athtoc-bin-win64.exe` and user-compiled programs have no libffi DLL
dependency.  They do link dynamically against `ws2_32.dll` (Winsock),
which ships with every Windows installation and is available in Wine.
