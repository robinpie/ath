CC      = gcc
CFLAGS  = -std=c89 -pedantic -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable

RUNTIME_DIR = runtime
SRCS    = $(filter-out $(RUNTIME_DIR)/test_runtime.c, $(wildcard $(RUNTIME_DIR)/*.c))
OBJS    = $(SRCS:.c=.o)
LIB     = libath_runtime.a
BIN     = athtoc-bin
BOOTSTRAP_C = /tmp/_athtoc_bootstrap.c

# Source files for the self-hosted transpiler.
TRANSPILER_SRCS = transpiler/main.~ATH transpiler/lexer.~ATH transpiler/parser.~ATH transpiler/cps.~ATH transpiler/codegen.~ATH

.PHONY: all bin lib test smoke clean

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
	$(CC) $(CFLAGS) -O2 $(BOOTSTRAP_C) $(SRCS) -I$(RUNTIME_DIR) -o $(BIN)

# Build the runtime as a static library (for users who want to link against it).
lib: $(LIB)

$(LIB): $(OBJS)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(LIB) $(BIN)
	ATHTOC=$$PWD/$(BIN) python3 -m pytest tests/ -v

# Quick smoke test using the self-hosted binary.
smoke: $(BIN)
	echo 'import timer T(1ms); ~ATH(T){} EXECUTE(UTTER("Hello")); THIS.DIE();' \
	    | ./$(BIN) > /tmp/_ath_smoke.c
	$(CC) $(CFLAGS) /tmp/_ath_smoke.c $(SRCS) -I$(RUNTIME_DIR) -o /tmp/_ath_smoke
	/tmp/_ath_smoke

# Note: clean does NOT remove athtoc-bin (the bootstrap seed). Remove it manually
# only if you have another way to rebuild it.
clean:
	rm -f $(OBJS) $(LIB) $(BOOTSTRAP_C)
