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

# Fixture shared library for the ffi__TestRecipe by-value struct cases (native
# Linux only; the cases are linux-tagged in manifest.txt). Cases open it as
# ../work/libcakegeo.so relative to tests/cases.
work/libcakegeo.so: cakegeo.c | work
	gcc -shared -fPIC -std=c89 $< -o $@

work/harness.c: harness.~ATH $(ATHTOC_STABLE) | work
	@test -x $(ATHTOC_STABLE) || { \
	  echo "ERROR: $(ATHTOC_STABLE) not found."; \
	  echo "       Run 'cp ../athtoc-bin tests/athtoc-stable' from ath/transpiler-to-c/."; \
	  exit 1; }
	$(ATHTOC_STABLE) < harness.~ATH > $@

work/harness: work/harness.c work/libruncase.so ../libath_runtime.a
	gcc -std=c89 -pedantic -Wno-unused-variable -Wno-declaration-after-statement \
	    $< -I$(RUNTIME) -L.. -lath_runtime -ldl -lffi -o $@

run: work/harness work/libcakegeo.so
	./work/harness

clean:
	rm -rf work
