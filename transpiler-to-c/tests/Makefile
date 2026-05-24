ATHTOC_STABLE = ./athtoc-stable
RUNTIME       = ../runtime

.PHONY: run clean

# If invoked directly (not via the parent 'make test'), ensure the runtime
# library exists first.
../libath_runtime.a:
	$(MAKE) -C .. lib

work:
	@mkdir -p work

work/libruncase.so: libruncase.c | work
	gcc -shared -fPIC -std=c99 $< -o $@

work/harness.c: harness.~ATH $(ATHTOC_STABLE) | work
	@test -x $(ATHTOC_STABLE) || { \
	  echo "ERROR: $(ATHTOC_STABLE) not found."; \
	  echo "       Run 'cp ../athtoc-bin tests/athtoc-stable' from transpiler-to-c/."; \
	  exit 1; }
	$(ATHTOC_STABLE) < harness.~ATH > $@

work/harness: work/harness.c work/libruncase.so ../libath_runtime.a
	gcc -std=c89 -pedantic -Wno-unused-variable -Wno-declaration-after-statement \
	    $< -I$(RUNTIME) -L.. -lath_runtime -ldl -lffi -o $@

run: work/harness
	./work/harness

clean:
	rm -rf work
