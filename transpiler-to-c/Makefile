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

# Source files for the self-hosted transpiler.
TRANSPILER_SRCS = transpiler/main.~ATH transpiler/lexer.~ATH transpiler/parser.~ATH transpiler/cps.~ATH transpiler/codegen.~ATH

# MinGW-w64 cross-compiler and vendored Windows libffi.
# libffi is from the MSYS2 mingw-w64-x86_64-libffi package (vendored in vendor/win64/).
CROSS_CC     = x86_64-w64-mingw32-gcc
WIN64_LIBFFI = vendor/win64/libffi
# Silence some MinGW-specific pedantic warnings that don't affect correctness.
WIN64_CFLAGS = -std=c89 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter \
               -Wno-unused-variable -Wno-pedantic-ms-format

.PHONY: all bin bin-i686 bin-win64 lib test smoke clean

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

# Run the !~ATH-based test harness (tests/harness.~ATH). The harness itself
# is built with the frozen tests/athtoc-stable, so a broken mainline athtoc
# still lets the suite run; each test case is transpiled by $(BIN) via the
# libruncase FFI session.
test: $(LIB) $(BIN)
	$(MAKE) -C tests run

# Quick smoke test using the self-hosted binary.
smoke: $(BIN)
	echo 'import timer T(1ms); ~ATH(T){} EXECUTE(UTTER("Hello")); THIS.DIE();' \
	    | ./$(BIN) > /tmp/_ath_smoke.c
	$(CC) $(CFLAGS) /tmp/_ath_smoke.c $(SRCS) -I$(RUNTIME_DIR) -ldl -lffi -o /tmp/_ath_smoke
	/tmp/_ath_smoke

# Note: clean does NOT remove the pre-built binaries (athtoc-bin, athtoc-bin-i686).
# Remove them manually only if you have another way to rebuild them.
clean:
	rm -f $(OBJS) $(LIB) $(BOOTSTRAP_C)
	$(MAKE) -C tests clean
