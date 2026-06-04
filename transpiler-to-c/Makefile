# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2026 robinpie
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

CC      = gcc
CFLAGS  = -std=c89 -pedantic -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable

RUNTIME_DIR = runtime
SRCS    = $(filter-out $(RUNTIME_DIR)/test_runtime.c, $(wildcard $(RUNTIME_DIR)/*.c))
OBJS    = $(SRCS:.c=.o)
LIB     = libath_runtime.a
BIN     = athtoc-bin
BIN_I686 = athtoc-bin-i686
BIN_WIN64 = athtoc-bin-win64.exe
BOOTSTRAP_C = /tmp/_athtoc_bootstrap.c

# Win64 runtime static library: the runtime cross-compiled with MinGW-w64 and
# vendored libffi, so the wine-based test target can link cases without
# recompiling runtime/*.c for every case.
LIB_WIN64  = libath_runtime_win64.a
WIN64_OBJS = $(SRCS:.c=.win64.o)

# ===== WebAssembly / WASI target =====
# WASM is a first-class fourth target. The transpiler self-hosts to athtoc.wasm
# (a WASI module that reads .~ATH on stdin and emits C89 on stdout), and
# transpiled !~ATH programs compile to standalone .wasm via libath_runtime_wasm.a.
#
# Toolchain: a wasi-sdk clang. On a monolithic wasi-sdk install, set
# WASI_SDK=/opt/wasi-sdk (the bin/ and share/wasi-sysroot subdirs are derived
# from it). On Arch (wasi-libc + system clang) the defaults below work out of
# the box: system clang and the sysroot under /usr/share/wasi-sysroot.
WASI_SDK       ?=
WASI_SYSROOT   ?= $(if $(WASI_SDK),$(WASI_SDK)/share/wasi-sysroot,/usr/share/wasi-sysroot)
WASI_CLANG     ?= $(if $(WASI_SDK),$(WASI_SDK)/bin/clang,clang)
WASI_CC         = $(WASI_CLANG) --target=wasm32-wasi --sysroot=$(WASI_SYSROOT)
WASMTIME       ?= wasmtime
# wasm runs on the host OS stack, and self-hosting transpilation recurses
# deeply, so raise the engine's wasm-stack limit well above the 512 KB default
# (the recipes also `ulimit -s unlimited` so the host stack can actually grow
# that far). Override WASM_STACK if your environment caps the stack lower.
WASM_STACK     ?= 1073741824
# wasmtime needs the exceptions proposal enabled to run our sjlj lowering.
WASMTIME_FLAGS ?= -W exceptions=y -W max-wasm-stack=$(WASM_STACK)
# wasm32-wasi is LP32: `long` is 4 bytes, identical to the i686 build, so
# integer semantics need no special handling. setjmp/longjmp (ath_error) lower
# to the wasm exception-handling proposal: -wasm-enable-sjlj turns on sjlj and
# -wasm-use-legacy-eh=false emits the modern exnref form that wasmtime runs
# (with -W exceptions=y); -lsetjmp supplies the sjlj helper routines.
WASM_CFLAGS     = -std=c89 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter \
                  -Wno-unused-variable -Wno-deprecated \
                  -mllvm -wasm-enable-sjlj -mllvm -wasm-use-legacy-eh=false
# 256 MB linear-memory stack so deeply-recursive self-hosting transpilation
# does not overflow the default ~64 KB wasm stack.
WASM_LDFLAGS    = -lsetjmp -Wl,-z,stack-size=268435456 -Wl,--stack-first

BIN_WASM   = athtoc.wasm
LIB_WASM   = libath_runtime_wasm.a
WASM_OBJS  = $(SRCS:.c=.wasm.o)

# Source files for the self-hosted transpiler.
TRANSPILER_SRCS = transpiler/main.~ATH transpiler/lexer.~ATH transpiler/parser.~ATH transpiler/cps.~ATH transpiler/codegen.~ATH

# MinGW-w64 cross-compiler and vendored Windows libffi.
# libffi is from the MSYS2 mingw-w64-x86_64-libffi package (vendored in vendor/win64/).
CROSS_CC     = x86_64-w64-mingw32-gcc
WIN64_LIBFFI = vendor/win64/libffi
# Silence some MinGW-specific pedantic warnings that don't affect correctness.
WIN64_CFLAGS = -std=c89 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter \
               -Wno-unused-variable -Wno-pedantic-ms-format

.PHONY: all bin bin-i686 bin-win64 bin-wasm lib lib-win64 lib-wasm \
        test test-linux test-win64 test-wasm verify-wasm-selfhost smoke clean

# Default target: rebuild athtoc-bin from the latest .~ATH source files,
# using the existing athtoc-bin to transpile itself.
all: $(BIN)

bin: $(BIN)

# athtoc-bin re-bootstraps itself: feed transpiler/main.~ATH through the
# existing binary to produce fresh C source, then compile.
# The shipped athtoc-bin in the repo is the bootstrap seed.
$(BIN): $(TRANSPILER_SRCS) $(SRCS)
	@if [ ! -x "./$(BIN)" ]; then \
	  echo "ERROR: ./$(BIN) not found. The repo ships a pre-built x86_64 Linux binary;"; \
	  echo "       restore it from git or supply your own bootstrap binary."; \
	  exit 1; \
	fi
	cd transpiler && ../$(BIN) < main.~ATH > $(BOOTSTRAP_C)
	$(CC) $(CFLAGS) -O2 $(BOOTSTRAP_C) $(SRCS) -I$(RUNTIME_DIR) -ldl -lffi -o $(BIN)

# Build the i686 (32-bit) binary using -m32 and the lib32 libffi.
# Requires: gcc-multilib (or a multilib-enabled gcc) and lib32-libffi.
# On Arch: pacman -S lib32-libffi
# On Debian/Ubuntu: apt install gcc-multilib libffi-dev:i386
bin-i686: $(BIN_I686)

$(BIN_I686): $(TRANSPILER_SRCS) $(SRCS)
	@if [ ! -x "./$(BIN)" ]; then \
	  echo "ERROR: ./$(BIN) not found -- needed to transpile the source."; \
	  exit 1; \
	fi
	cd transpiler && ../$(BIN) < main.~ATH > $(BOOTSTRAP_C)
	$(CC) $(CFLAGS) -m32 -O2 $(BOOTSTRAP_C) $(SRCS) -I$(RUNTIME_DIR) -L/usr/lib32 -ldl -lffi -o $(BIN_I686)

# Build the x86_64 Windows binary using MinGW-w64 cross-compiler.
# Requires: mingw-w64-gcc (pacman -S mingw-w64-gcc)
# Vendored Windows libffi lives in vendor/win64/libffi/ (from MSYS2
# mingw-w64-x86_64-libffi package -- see vendor/win64/README.md).
# The resulting exe can be tested with Wine: wine ./athtoc-bin-win64.exe
bin-win64: $(BIN_WIN64)

$(BIN_WIN64): $(TRANSPILER_SRCS) $(SRCS)
	@if [ ! -x "./$(BIN)" ]; then \
	  echo "ERROR: ./$(BIN) not found -- needed to transpile the source."; \
	  exit 1; \
	fi
	cd transpiler && ../$(BIN) < main.~ATH > $(BOOTSTRAP_C)
	$(CROSS_CC) $(WIN64_CFLAGS) -O2 $(BOOTSTRAP_C) $(SRCS) \
	    -I$(RUNTIME_DIR) -I$(WIN64_LIBFFI)/include \
	    -Wl,-Bstatic $(WIN64_LIBFFI)/lib/libffi.a \
	    -Wl,-Bdynamic -lws2_32 -static-libgcc \
	    -o $(BIN_WIN64)

# Build the runtime as a static library (for users who want to link against it).
lib: $(LIB)

$(LIB): $(OBJS)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build the runtime as a Windows static library (for the wine-based test target).
# Requires: mingw-w64-gcc. Uses the vendored Windows libffi headers.
lib-win64: $(LIB_WIN64)

$(LIB_WIN64): $(WIN64_OBJS)
	x86_64-w64-mingw32-ar rcs $@ $^

%.win64.o: %.c
	$(CROSS_CC) $(WIN64_CFLAGS) -I$(WIN64_LIBFFI)/include -c $< -o $@

# Build the runtime as a WASM/WASI static library (linked by transpiled programs
# and by the wasm test target). Requires a wasi-sdk clang (see WASI_* above).
lib-wasm: $(LIB_WASM)

# llvm-ar emits wasm-compatible archives; override AR_WASM for a custom path.
AR_WASM ?= llvm-ar
$(LIB_WASM): $(WASM_OBJS)
	$(AR_WASM) rcs $@ $^

%.wasm.o: %.c
	$(WASI_CC) $(WASM_CFLAGS) -I$(RUNTIME_DIR) -O2 -c $< -o $@

# Build the self-hosting WASM transpiler, athtoc.wasm. Same bootstrap shape as
# bin-win64: transpile transpiler/main.~ATH with the native athtoc-bin to fresh
# C89, then compile that to a WASI module against the runtime sources. The
# shipped athtoc.wasm in the repo is a committed bootstrap artifact.
bin-wasm: $(BIN_WASM)

$(BIN_WASM): $(TRANSPILER_SRCS) $(SRCS)
	@if [ ! -x "./$(BIN)" ]; then \
	  echo "ERROR: ./$(BIN) not found -- needed to transpile the source."; \
	  exit 1; \
	fi
	cd transpiler && ../$(BIN) < main.~ATH > $(BOOTSTRAP_C)
	$(WASI_CC) $(WASM_CFLAGS) -O2 $(BOOTSTRAP_C) $(SRCS) \
	    -I$(RUNTIME_DIR) $(WASM_LDFLAGS) -o $(BIN_WASM)

# Run the test harness against the WASM target. The harness itself runs natively
# (it forks runcase.sh via libruncase); ATH_TARGET=wasm tells libruncase to
# transpile/compile/run each case through the wasm toolchain (wasmtime + wasi
# clang). FFI/session, process, and connection cases report 'skip'.
test-wasm: $(LIB) $(LIB_WASM) $(BIN_WASM)
	ATH_TARGET=wasm WASMTIME="$(WASMTIME)" WASI_CLANG="$(WASI_CLANG)" \
	  WASI_SYSROOT="$(WASI_SYSROOT)" WASM_STACK="$(WASM_STACK)" \
	  $(MAKE) -C tests run

# Prove athtoc.wasm is a faithful self-host: it must reproduce its own C output,
# and that output must byte-match the native athtoc-bin for the same input.
verify-wasm-selfhost: $(BIN_WASM) $(BIN)
	@set -e; \
	ulimit -s unlimited 2>/dev/null || true; \
	echo "[1/4] native athtoc-bin transpiles main.~ATH -> native.c"; \
	cd transpiler && ../$(BIN) < main.~ATH > /tmp/_ath_native.c && cd ..; \
	echo "[2/4] athtoc.wasm transpiles main.~ATH -> out1.c"; \
	$(WASMTIME) run $(WASMTIME_FLAGS) --dir transpiler::. $(BIN_WASM) \
	    < transpiler/main.~ATH > /tmp/_ath_out1.c; \
	echo "[3/4] compile out1.c -> athtoc2.wasm, transpile main.~ATH -> out2.c"; \
	$(WASI_CC) $(WASM_CFLAGS) -O2 /tmp/_ath_out1.c $(SRCS) \
	    -I$(RUNTIME_DIR) $(WASM_LDFLAGS) -o /tmp/_athtoc2.wasm; \
	$(WASMTIME) run $(WASMTIME_FLAGS) --dir transpiler::. /tmp/_athtoc2.wasm \
	    < transpiler/main.~ATH > /tmp/_ath_out2.c; \
	echo "[4/4] assert out1.c == out2.c == native.c"; \
	if ! cmp -s /tmp/_ath_out1.c /tmp/_ath_out2.c; then \
	  echo "FAIL: wasm transpiler is not a fixpoint (out1.c != out2.c)"; \
	  diff /tmp/_ath_out1.c /tmp/_ath_out2.c | head; exit 1; fi; \
	if ! cmp -s /tmp/_ath_out1.c /tmp/_ath_native.c; then \
	  echo "FAIL: wasm output differs from native athtoc-bin"; \
	  diff /tmp/_ath_native.c /tmp/_ath_out1.c | head; exit 1; fi; \
	echo "OK: athtoc.wasm is a faithful self-host (byte-identical to native)"

# Run the !~ATH-based test harness (tests/harness.~ATH). The harness itself
# is built with the frozen tests/athtoc-stable, so a broken mainline athtoc
# still lets the suite run; each test case is transpiled by $(BIN) via the
# libruncase FFI session. The harness binary is always native; the target
# (native vs. win64) is selected by the ATH_TARGET env var, read by libruncase.
#
# 'test' runs all targets; 'test-linux', 'test-win64', and 'test-wasm' run one
# each. Cases are filtered per target by the optional platform field in
# manifest.txt (WASM skips FFI/session, process, and connection cases).
test: test-linux test-win64 test-wasm

test-linux: $(LIB) $(BIN)
	ATH_TARGET=native $(MAKE) -C tests run

# Requires: wine + mingw-w64-gcc. Transpiles each case with athtoc-bin-win64.exe
# under wine, cross-compiles, and runs under wine.
test-win64: $(LIB) $(LIB_WIN64) $(BIN_WIN64)
	ATH_TARGET=win64 $(MAKE) -C tests run

# Quick smoke test using the self-hosted binary.
smoke: $(BIN)
	echo 'import timer T(1ms); ~ATH(T){} EXECUTE(UTTER("Hello")); THIS.DIE();' \
	    | ./$(BIN) > /tmp/_ath_smoke.c
	$(CC) $(CFLAGS) /tmp/_ath_smoke.c $(SRCS) -I$(RUNTIME_DIR) -ldl -lffi -o /tmp/_ath_smoke
	/tmp/_ath_smoke

# Note: clean does NOT remove the pre-built binaries (athtoc-bin, athtoc-bin-i686).
# Remove them manually only if you have another way to rebuild them.
clean:
	rm -f $(OBJS) $(LIB) $(WIN64_OBJS) $(LIB_WIN64) $(WASM_OBJS) $(LIB_WASM) $(BOOTSTRAP_C)
	$(MAKE) -C tests clean
