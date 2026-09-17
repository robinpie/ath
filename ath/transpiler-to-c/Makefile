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

# Win64 runtime static library: the runtime cross-compiled with MinGW-w64 and vendored libffi, so the wine-based test target can link cases without recompiling runtime/*.c for every case.
LIB_WIN64  = libath_runtime_win64.a
WIN64_OBJS = $(SRCS:.c=.win64.o)

# ===== WebAssembly / WASI target =====
# WASM is a first-class fourth target. The transpiler self-hosts to athtoc.wasm (a WASI module that reads .~ATH on stdin and emits C89 on stdout), and transpiled !~ATH programs compile to standalone .wasm via libath_runtime_wasm.a.
#
# Toolchain: a wasi-sdk clang. On a monolithic wasi-sdk install, set WASI_SDK=/opt/wasi-sdk (the bin/ and share/wasi-sysroot subdirs are derived from it). On Arch (wasi-libc + system clang) the defaults below work out of the box: system clang and the sysroot under /usr/share/wasi-sysroot.
WASI_SDK       ?=
WASI_SYSROOT   ?= $(if $(WASI_SDK),$(WASI_SDK)/share/wasi-sysroot,/usr/share/wasi-sysroot)
WASI_CLANG     ?= $(if $(WASI_SDK),$(WASI_SDK)/bin/clang,clang)
WASI_CC         = $(WASI_CLANG) --target=wasm32-wasi --sysroot=$(WASI_SYSROOT)
WASMTIME       ?= wasmtime
# wasm runs on the host OS stack, and self-hosting transpilation recurses deeply, so raise the engine's wasm-stack limit well above the 512 KB default (the recipes also `ulimit -s unlimited` so the host stack can actually grow that far). Override WASM_STACK if your environment caps the stack lower.
WASM_STACK     ?= 1073741824
# wasmtime needs the exceptions proposal enabled to run our sjlj lowering.
WASMTIME_FLAGS ?= -W exceptions=y -W max-wasm-stack=$(WASM_STACK)
# wasm32-wasi is LP32: `long` is 4 bytes, identical to the i686 build, so integer semantics need no special handling. setjmp/longjmp (ath_error) lower to the wasm exception-handling proposal: -wasm-enable-sjlj turns on sjlj and -wasm-use-legacy-eh=false emits the modern exnref form that wasmtime runs (with -W exceptions=y); -lsetjmp supplies the sjlj helper routines.
WASM_CFLAGS     = -std=c89 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter \
                  -Wno-unused-variable -Wno-deprecated \
                  -mllvm -wasm-enable-sjlj -mllvm -wasm-use-legacy-eh=false
# 256 MB linear-memory stack so deeply-recursive self-hosting transpilation does not overflow the default ~64 KB wasm stack.
WASM_LDFLAGS    = -lsetjmp -Wl,-z,stack-size=268435456 -Wl,--stack-first

BIN_WASM   = athtoc.wasm
LIB_WASM   = libath_runtime_wasm.a
WASM_OBJS  = $(SRCS:.c=.wasm.o)

# ===== Sanitizer (ASan + UBSan) build =====
# Debug-only variant of the runtime and transpiler for memory/UB checking. Objects are *.san.o so the normal library is never mixed with instrumented code. athtoc-bin-san is a build artifact, never a bootstrap seed.
SAN_FLAGS = -g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined
LIB_SAN   = libath_runtime_san.a
SAN_OBJS  = $(SRCS:.c=.san.o)
BIN_SAN   = athtoc-bin-san
# allow_user_segv_handler: the FFI fault handler installs its own SIGSEGV/SIGBUS handlers. detect_leaks=0: runtime errors longjmp past cleanup, so leak reports would swamp everything else (leak audit is a separate pass).
SAN_ASAN_OPTIONS  ?= allow_user_segv_handler=1:detect_leaks=0
SAN_UBSAN_OPTIONS ?= print_stacktrace=1

# Source files for the self-hosted transpiler.
TRANSPILER_SRCS = transpiler/main.~ATH transpiler/lexer.~ATH transpiler/parser.~ATH transpiler/cps.~ATH transpiler/codegen.~ATH

# MinGW-w64 cross-compiler and vendored Windows libffi.
# libffi is from the MSYS2 mingw-w64-x86_64-libffi package (vendored in vendor/win64/).
CROSS_CC     = x86_64-w64-mingw32-gcc
WIN64_LIBFFI = vendor/win64/libffi
# Silence some MinGW-specific pedantic warnings that don't affect correctness.
WIN64_CFLAGS = -std=c89 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter \
               -Wno-unused-variable -Wno-pedantic-ms-format

.PHONY: all bin bin-i686 bin-win64 bin-wasm lib lib-win64 lib-wasm lib-san bin-san test-san fuzz-cake fuzz-cake-run fuzz-ops fuzz-ops-run afl-athtoc afl-athtoc-run afl-check fuzz-clean \
        test test-linux test-win64 test-wasm verify-wasm-selfhost smoke \
        check-license clean

# Default target: rebuild athtoc-bin from the latest .~ATH source files, using the existing athtoc-bin to transpile itself.
all: $(BIN)

bin: $(BIN)

# athtoc-bin re-bootstraps itself: feed transpiler/main.~ATH through the existing binary to produce fresh C source, then compile. The shipped athtoc-bin in the repo is the bootstrap seed.
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
# Vendored Windows libffi lives in vendor/win64/libffi/ (from MSYS2 mingw-w64-x86_64-libffi package -- see vendor/win64/README.md).
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

# Build the runtime with AddressSanitizer + UndefinedBehaviorSanitizer.
lib-san: $(LIB_SAN)

$(LIB_SAN): $(SAN_OBJS)
	ar rcs $@ $^

%.san.o: %.c
	$(CC) $(CFLAGS) $(SAN_FLAGS) -c $< -o $@

# The transpiler built against the sanitized runtime (transpiled by the normal athtoc-bin).
bin-san: $(BIN_SAN)

$(BIN_SAN): $(TRANSPILER_SRCS) $(SRCS) $(BIN)
	cd transpiler && ../$(BIN) < main.~ATH > /tmp/_athtoc_san.c
	$(CC) $(CFLAGS) $(SAN_FLAGS) /tmp/_athtoc_san.c $(SRCS) -I$(RUNTIME_DIR) -ldl -lffi -o $(BIN_SAN)

# ===== libFuzzer harnesses (fuzz/) =====
# Built with clang: the runtime sources are compiled into each harness with coverage + ASan + UBSan. gnu89 rather than c89 so the harness can use snprintf. Run with `make fuzz-cake-run` (FUZZ_ARGS passes extra libFuzzer flags, e.g. FUZZ_ARGS=-max_total_time=600). Corpora and crash artifacts live under fuzz/work/ (gitignored).
FUZZ_CC    ?= clang
FUZZ_FLAGS  = -std=gnu89 -g -O1 -fno-omit-frame-pointer -fsanitize=fuzzer,address,undefined \
              -fno-sanitize-recover=undefined -I$(RUNTIME_DIR)
FUZZ_ARGS  ?=
# detect_leaks=0: schema errors longjmp past parser cleanup, so every rejected input leaks (tracked separately).
FUZZ_ENV    = ASAN_OPTIONS=detect_leaks=0:allow_user_segv_handler=1

fuzz/work/fuzz_cake: fuzz/fuzz_cake.c $(SRCS) $(wildcard $(RUNTIME_DIR)/*.h)
	@mkdir -p fuzz/work
	$(FUZZ_CC) $(FUZZ_FLAGS) fuzz/fuzz_cake.c $(SRCS) -ldl -lffi -o $@

fuzz-cake: fuzz/work/fuzz_cake

# Seeds the corpus from every .^CAKE in the repo on first run.
fuzz-cake-run: fuzz/work/fuzz_cake
	@mkdir -p fuzz/work/corpus-cake fuzz/work/crashes-cake
	@if [ -z "$$(ls -A fuzz/work/corpus-cake)" ]; then \
	  find ../.. -name '*.^CAKE' -not -path '*/fuzz/work/*' -exec cp {} fuzz/work/corpus-cake/ \; ; fi
	$(FUZZ_ENV) ./fuzz/work/fuzz_cake -dict=fuzz/cake.dict -max_len=4096 \
	  -artifact_prefix=fuzz/work/crashes-cake/ $(FUZZ_ARGS) fuzz/work/corpus-cake

fuzz/work/fuzz_ops: fuzz/fuzz_ops.c $(SRCS) $(wildcard $(RUNTIME_DIR)/*.h)
	@mkdir -p fuzz/work
	$(FUZZ_CC) $(FUZZ_FLAGS) fuzz/fuzz_ops.c $(SRCS) -ldl -lffi -o $@

fuzz-ops: fuzz/work/fuzz_ops

# The ops harness decodes raw bytes into runtime calls, so it needs no seeds.
fuzz-ops-run: fuzz/work/fuzz_ops
	@mkdir -p fuzz/work/corpus-ops fuzz/work/crashes-ops
	$(FUZZ_ENV) ./fuzz/work/fuzz_ops -max_len=2048 -rss_limit_mb=4096 \
	  -artifact_prefix=fuzz/work/crashes-ops/ $(FUZZ_ARGS) fuzz/work/corpus-ops

# ===== AFL++ on the transpiler itself (athtoc reads stdin, so no harness) =====
# afl-athtoc builds two instrumented transpilers from the current transpiler
# source: athtoc-afl (ASan + UBSan, for the main instance) and athtoc-afl-fast
# (no sanitizers, for secondaries). afl-athtoc-run minimizes seeds from the test
# cases and apps (comments stripped), then runs one main + AFL_SECONDARIES
# instances for AFL_TIME seconds. afl-check applies the "accepted input must
# compile" oracle to everything AFL queued. Requires afl++ (pacman -S afl++).
AFL_DIR          = fuzz/work/afl
AFL_TIME        ?= 3600
AFL_SECONDARIES ?= 5
AFL_ENV          = AFL_SKIP_CPUFREQ=1 AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 AFL_NO_UI=1 AFL_NO_AFFINITY=1

$(AFL_DIR)/athtoc-afl: $(TRANSPILER_SRCS) $(SRCS) $(wildcard $(RUNTIME_DIR)/*.h)
	@mkdir -p $(AFL_DIR)
	cd transpiler && ../$(BIN) < main.~ATH > ../$(AFL_DIR)/athtoc_src.c
	AFL_USE_ASAN=1 AFL_USE_UBSAN=1 afl-clang-fast -std=gnu89 -g -O1 $(AFL_DIR)/athtoc_src.c $(SRCS) \
	  -I$(RUNTIME_DIR) -ldl -lffi -o $@
	afl-clang-fast -std=gnu89 -g -O2 $(AFL_DIR)/athtoc_src.c $(SRCS) -I$(RUNTIME_DIR) -ldl -lffi -o $(AFL_DIR)/athtoc-afl-fast

afl-athtoc: $(AFL_DIR)/athtoc-afl

afl-athtoc-run: $(AFL_DIR)/athtoc-afl
	@if [ ! -d $(AFL_DIR)/seeds ]; then \
	  mkdir -p $(AFL_DIR)/seeds-all && \
	  find tests/cases ../apps -name '*.~ATH' -size -3k -exec cp {} $(AFL_DIR)/seeds-all/ \; && \
	  for f in $(AFL_DIR)/seeds-all/*; do sed -i '/^\s*\/\//d; /^\s*$$/d' "$$f"; done && \
	  $(AFL_ENV) afl-cmin -i $(AFL_DIR)/seeds-all -o $(AFL_DIR)/seeds -T all -- ./$(AFL_DIR)/athtoc-afl-fast; fi
	@for i in $$(seq 1 $(AFL_SECONDARIES)); do \
	  $(AFL_ENV) timeout $(AFL_TIME) afl-fuzz -S s$$i -i $(AFL_DIR)/seeds -o $(AFL_DIR)/out -x fuzz/athtoc.dict \
	    -t 2000 -m none -- ./$(AFL_DIR)/athtoc-afl-fast > /dev/null 2>&1 & done; \
	$(AFL_ENV) ASAN_OPTIONS=detect_leaks=0:allow_user_segv_handler=1:abort_on_error=1:symbolize=0 \
	  timeout $(AFL_TIME) afl-fuzz -M main -i $(AFL_DIR)/seeds -o $(AFL_DIR)/out -x fuzz/athtoc.dict \
	    -t 3000 -m none -- ./$(AFL_DIR)/athtoc-afl; \
	wait; afl-whatsup -s $(AFL_DIR)/out || true

afl-check:
	fuzz/afl_check_outputs.sh $(AFL_DIR)/out

fuzz-clean:
	rm -rf fuzz/work

# Build the runtime as a Windows static library (for the wine-based test target).
# Requires: mingw-w64-gcc. Uses the vendored Windows libffi headers.
lib-win64: $(LIB_WIN64)

$(LIB_WIN64): $(WIN64_OBJS)
	x86_64-w64-mingw32-ar rcs $@ $^

%.win64.o: %.c
	$(CROSS_CC) $(WIN64_CFLAGS) -I$(WIN64_LIBFFI)/include -c $< -o $@

# Build the runtime as a WASM/WASI static library (linked by transpiled programs and by the wasm test target). Requires a wasi-sdk clang (see WASI_* above).
lib-wasm: $(LIB_WASM)

# llvm-ar emits wasm-compatible archives; override AR_WASM for a custom path.
AR_WASM ?= llvm-ar
$(LIB_WASM): $(WASM_OBJS)
	$(AR_WASM) rcs $@ $^

%.wasm.o: %.c
	$(WASI_CC) $(WASM_CFLAGS) -I$(RUNTIME_DIR) -O2 -c $< -o $@

# Build the self-hosting WASM transpiler, athtoc.wasm. Same bootstrap shape as bin-win64: transpile transpiler/main.~ATH with the native athtoc-bin to fresh C89, then compile that to a WASI module against the runtime sources. The shipped athtoc.wasm in the repo is a committed bootstrap artifact.
bin-wasm: $(BIN_WASM)

$(BIN_WASM): $(TRANSPILER_SRCS) $(SRCS)
	@if [ ! -x "./$(BIN)" ]; then \
	  echo "ERROR: ./$(BIN) not found -- needed to transpile the source."; \
	  exit 1; \
	fi
	cd transpiler && ../$(BIN) < main.~ATH > $(BOOTSTRAP_C)
	$(WASI_CC) $(WASM_CFLAGS) -O2 $(BOOTSTRAP_C) $(SRCS) \
	    -I$(RUNTIME_DIR) $(WASM_LDFLAGS) -o $(BIN_WASM)

# Run the test harness against the WASM target. The harness itself runs natively (it forks runcase.sh via libruncase); ATH_TARGET=wasm tells libruncase to transpile/compile/run each case through the wasm toolchain (wasmtime + wasi clang). FFI/session, process, and connection cases report 'skip'.
test-wasm: $(LIB) $(LIB_WASM) $(BIN_WASM)
	ATH_TARGET=wasm WASMTIME="$(WASMTIME)" WASI_CLANG="$(WASI_CLANG)" \
	  WASI_SYSROOT="$(WASI_SYSROOT)" WASM_STACK="$(WASM_STACK)" \
	  $(MAKE) -C tests run

# Prove athtoc.wasm is a faithful self-host: it must reproduce its own C output, and that output must byte-match the native athtoc-bin for the same input.
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

# Run the !~ATH-based test harness (tests/harness.~ATH). The harness itself is built with the frozen tests/athtoc-stable, so a broken mainline athtoc still lets the suite run; each test case is transpiled by $(BIN) via the libruncase FFI session. The harness binary is always native; the target (native vs. win64) is selected by the ATH_TARGET env var, read by libruncase.
#
# 'test' runs all targets; 'test-linux', 'test-win64', and 'test-wasm' run one each. Cases are filtered per target by the optional platform field in manifest.txt (WASM skips FFI/session, process, and connection cases).
test: test-linux test-win64 test-wasm

test-linux: $(LIB) $(BIN)
	ATH_TARGET=native $(MAKE) -C tests run

# Native suite under ASan + UBSan: cases are transpiled by athtoc-bin-san and linked against libath_runtime_san.a (ATH_SAN=1, read by libruncase). Sanitizer reports go to tests/work/<case>.err, which the harness does not inspect, so they are summarised afterwards.
test-san: $(LIB) $(LIB_SAN) $(BIN) $(BIN_SAN)
	-ATH_TARGET=native ATH_SAN=1 \
	  ASAN_OPTIONS="$(SAN_ASAN_OPTIONS)" UBSAN_OPTIONS="$(SAN_UBSAN_OPTIONS)" \
	  $(MAKE) -C tests run
	@echo "===== sanitizer reports (tests/work/*.err) ====="
	@grep -lE 'ERROR: (Address|Leak)Sanitizer|runtime error:' tests/work/*.err 2>/dev/null \
	  | sed 's|tests/work/||; s|\.err$$||' | sort > tests/work/san-hits.txt || true
	@wc -l < tests/work/san-hits.txt | xargs echo "cases with reports:"
	@cat tests/work/san-hits.txt

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

# Lint every tracked .c/.h/.~ATH/.^CAKE file (outside ath/deprecated/,
# vendor/win64/, tests/work/, and ath/apps/golf/) for an SPDX-License-Identifier
# line and the GPLv2 "This program is free software" boilerplate. The linter itself is
# tools/check-spdx.~ATH; it shells out to `find` and reads via SCRY, so it
# must be run with the repo root as its working directory.
check-license: $(BIN)
	./$(BIN) < ../../tools/check-spdx.~ATH > /tmp/_ath_check_spdx.c
	$(CC) $(CFLAGS) /tmp/_ath_check_spdx.c $(SRCS) -I$(RUNTIME_DIR) -ldl -lffi -o /tmp/_ath_check_spdx
	cd ../.. && /tmp/_ath_check_spdx

# Note: clean does NOT remove the pre-built binaries (athtoc-bin, athtoc-bin-i686).
# Remove them manually only if you have another way to rebuild them.
# fuzz corpora are expensive to regrow, so clean only drops the fuzz binaries; fuzz-clean removes everything
clean:
	rm -f fuzz/work/fuzz_cake fuzz/work/fuzz_ops $(AFL_DIR)/athtoc-afl $(AFL_DIR)/athtoc-afl-fast $(AFL_DIR)/athtoc_src.c
	rm -f $(OBJS) $(LIB) $(SAN_OBJS) $(LIB_SAN) $(BIN_SAN) $(WIN64_OBJS) $(LIB_WIN64) $(WASM_OBJS) $(LIB_WASM) $(BOOTSTRAP_C)
	$(MAKE) -C tests clean
