CC      = gcc
CFLAGS  = -std=c89 -pedantic -Wall -Wextra -Wno-unused-function -Wno-unused-parameter

RUNTIME_DIR = runtime
SRCS    = $(filter-out $(RUNTIME_DIR)/test_runtime.c, $(wildcard $(RUNTIME_DIR)/*.c))
OBJS    = $(SRCS:.c=.o)
LIB     = libath_runtime.a

.PHONY: all test clean

all: $(LIB)

$(LIB): $(OBJS)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(LIB)
	python3 -m pytest tests/ -v

# Quick smoke test: hello world via transpiler
smoke: $(LIB)
	echo 'import timer T(1ms); ~ATH(T){} EXECUTE(UTTER("Hello")); THIS.DIE();' \
	    | ./athtoc > /tmp/_ath_smoke.c
	$(CC) $(CFLAGS) /tmp/_ath_smoke.c $(SRCS) -I$(RUNTIME_DIR) -o /tmp/_ath_smoke
	/tmp/_ath_smoke

clean:
	rm -f $(OBJS) $(LIB)
