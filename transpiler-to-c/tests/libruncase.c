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

/* libruncase.c -- per-case executor for the !~ATH test harness.
 *
 * Exported functions:
 *   long        run_case(const char *name, const char *athtoc)
 *   const char *ath_target(void)
 *
 * Transpiles cases/<name>.~ATH, compiles the result, runs it, and writes
 * stdout to work/<name>.actual.  On any toolchain failure the actual file is
 * filled with a sentinel (TRANSPILE_FAIL / COMPILE_FAIL) so the harness sees a
 * clean FAIL.
 *
 * Three targets, selected by the ATH_TARGET environment variable:
 *   "native" (default) -- transpile with athtoc (arg -> ATHTOC -> ../athtoc-bin),
 *                         compile with gcc, run the ELF directly.
 *   "win64"            -- transpile with the Windows transpiler under wine,
 *                         cross-compile with mingw + vendored libffi, run the
 *                         .exe under wine.  The win64 program emits CRLF line
 *                         endings, so the .actual file is normalised to LF
 *                         before the harness compares it.
 *   "wasm"             -- transpile with athtoc.wasm under wasmtime, compile to
 *                         a standalone .wasm with the wasi-sdk clang against
 *                         libath_runtime_wasm.a, and run under wasmtime.  Both
 *                         wasmtime invocations get a --dir grant on cases/ so
 *                         module-import watchers can read sibling .~ATH files.
 *                         Tool paths come from the env (WASMTIME, WASI_CLANG,
 *                         WASI_SYSROOT, WASM_STACK) with the defaults below.
 *
 * ath_target() lets the harness (which has no getenv) discover the target so it
 * can filter cases by their manifest platform field.
 *
 * Must be invoked with the process cwd set to the tests/ directory.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/resource.h>

/* Run argv[] with optional I/O redirects (absolute paths) and cwd.
 * NULL means inherit.  stdout_path is created/truncated; stderr_path is
 * opened in append mode.  Returns exit status, or -1 on fork/exec error. */
static int run_cmd(char *const argv[], const char *stdin_path,
                   const char *stdout_path, const char *stderr_path,
                   const char *cwd) {
    pid_t pid = fork();
    if (pid < 0) return -1;

    if (pid == 0) {
        int fd;
        struct rlimit rl;

        /* Best-effort: give the child an unbounded stack. Native test programs raise this themselves (ath_eventloop_init), but wasm programs run inside wasmtime -- which executes wasm on this host stack -- and the WASM build cannot setrlimit, so deeply-recursive cases need the room granted here before exec. Harmless for the gcc/wine children. */
        if (getrlimit(RLIMIT_STACK, &rl) == 0) {
            rl.rlim_cur = rl.rlim_max;
            setrlimit(RLIMIT_STACK, &rl);
        }

        if (cwd && chdir(cwd) != 0) _exit(127);

        if (stdin_path) {
            fd = open(stdin_path, O_RDONLY);
            if (fd < 0) _exit(127);
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        if (stdout_path) {
            fd = open(stdout_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) _exit(127);
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        if (stderr_path) {
            fd = open(stderr_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd < 0) _exit(127);
            dup2(fd, STDERR_FILENO);
            close(fd);
        }

        execvp(argv[0], argv);
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static void write_sentinel(const char *path, const char *msg) {
    FILE *f = fopen(path, "w");
    if (f) { fprintf(f, "%s\n", msg); fclose(f); }
}

static int path_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

/* Build an absolute path from cwd + rel. buf must be PATH_MAX bytes. */
static void abspath(char *buf, size_t bufsz, const char *base, const char *rel) {
    if (rel[0] == '/') {
        snprintf(buf, bufsz, "%s", rel);
    } else {
        snprintf(buf, bufsz, "%s/%s", base, rel);
    }
}

/* The active target, from ATH_TARGET: "win64", "wasm", or (by default) "native". */
enum ath_tgt { TGT_NATIVE, TGT_WIN64, TGT_WASM };
static enum ath_tgt active_target(void) {
    const char *t = getenv("ATH_TARGET");
    if (t && strcmp(t, "win64") == 0) return TGT_WIN64;
    if (t && strcmp(t, "wasm")  == 0) return TGT_WASM;
    return TGT_NATIVE;
}

/* The platform label reported to the harness (which has no getenv of its own), matching the manifest platform-field vocabulary: "win64", "wasm", or "linux". */
const char *ath_target(void) {
    switch (active_target()) {
    case TGT_WIN64: return "win64";
    case TGT_WASM:  return "wasm";
    default:        return "linux";
    }
}

/* env override with a default; returns a non-NULL string. */
static const char *env_or(const char *name, const char *dflt) {
    const char *v = getenv(name);
    return (v && *v) ? v : dflt;
}

/* Copy name into dst, replacing characters that wine mishandles when they appear in the program-path argument (notably the double quotes in some parametrised case names) with '_'. Only the intermediate .exe filename is sanitised; the .actual/.err/.c paths are opened by us directly and keep the exact case name. Brackets are kept (wine handles them and they keep names distinct). */
static void sanitize_exe(char *dst, size_t dstsz, const char *name) {
    size_t i;
    for (i = 0; name[i] && i + 1 < dstsz; i++) {
        char c = name[i];
        int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                 (c >= '0' && c <= '9') ||
                 c == '.' || c == '_' || c == '-' || c == '[' || c == ']';
        dst[i] = (char)(ok ? c : '_');
    }
    dst[i < dstsz ? i : dstsz - 1] = '\0';
}

/* Strip carriage returns from a file in place.  The win64 runtime writes its stdout in text mode, so newlines arrive as CRLF; the corpus stores LF, so we normalise before the harness does its byte-exact compare.  Harmless when the file contains no CR. */
static void strip_cr(const char *path) {
    FILE *f;
    long n, i, j;
    char *buf;

    f = fopen(path, "rb");
    if (!f) return;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return; }
    n = ftell(f);
    if (n < 0) { fclose(f); return; }
    rewind(f);
    buf = (char *)malloc((size_t)n);
    if (!buf) { fclose(f); return; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return; }
    fclose(f);

    for (i = 0, j = 0; i < n; i++) {
        if (buf[i] != '\r') buf[j++] = buf[i];
    }

    f = fopen(path, "wb");
    if (f) { fwrite(buf, 1, (size_t)j, f); fclose(f); }
    free(buf);
}

long run_case(const char *name, const char *athtoc) {
    enum ath_tgt tgt = active_target();
    int win64 = (tgt == TGT_WIN64);
    int wasm  = (tgt == TGT_WASM);

    /* wasm tool paths (env-overridable; defaults match the Makefile). */
    const char *wasmtime   = env_or("WASMTIME",     "wasmtime");
    const char *wasi_clang = env_or("WASI_CLANG",   "clang");
    const char *wasi_root  = env_or("WASI_SYSROOT", "/usr/share/wasi-sysroot");
    const char *wasm_stack = env_or("WASM_STACK",   "1073741824");
    char sysroot_flag[4096], maxstack_flag[256];

    /* Resolve the transpiler path.
     * native: argument -> ATHTOC env -> ../athtoc-bin
     * win64:  ATHTOC env -> ../athtoc-bin-win64.exe (the native arg is ignored,
     *         since the harness passes the native binary path unconditionally).
     * wasm:   ATHTOC env -> ../athtoc.wasm (likewise). */
    const char *transpiler;
    if (win64) {
        transpiler = getenv("ATHTOC");
        if (!transpiler || !*transpiler) transpiler = "../athtoc-bin-win64.exe";
    } else if (wasm) {
        transpiler = getenv("ATHTOC");
        if (!transpiler || !*transpiler) transpiler = "../athtoc.wasm";
    } else {
        transpiler = (athtoc && *athtoc) ? athtoc : getenv("ATHTOC");
        if (!transpiler || !*transpiler) transpiler = "../athtoc-bin";
    }

    /* Absolute cwd (tests/) for building paths that survive chdir in children */
    char tests_dir[4096];
    if (!getcwd(tests_dir, sizeof tests_dir)) return -1;

    char cases_dir[4096];
    snprintf(cases_dir, sizeof cases_dir, "%s/cases", tests_dir);

    /* Absolute paths for all files */
    char src[4096], c_file[4096], bin_file[4096];
    char actual[4096], err_file[4096], stdin_file[4096];
    char transpiler_abs[4096];

    snprintf(src,       sizeof src,       "%s/%s.~ATH",   cases_dir, name);
    snprintf(c_file,    sizeof c_file,    "%s/work/%s.c",      tests_dir, name);
    if (win64) {
        /* The .exe path is passed to wine as the program argument, which mishandles some characters; sanitise it. */
        char safe[4096];
        sanitize_exe(safe, sizeof safe, name);
        snprintf(bin_file, sizeof bin_file, "%s/work/%s.exe", tests_dir, safe);
    } else if (wasm) {
        snprintf(bin_file, sizeof bin_file, "%s/work/%s.wasm", tests_dir, name);
    } else {
        snprintf(bin_file, sizeof bin_file, "%s/work/%s.bin", tests_dir, name);
    }

    /* Pre-format the reused wasm flags. --dir grants are added per-invocation. */
    snprintf(sysroot_flag,  sizeof sysroot_flag,  "--sysroot=%s", wasi_root);
    snprintf(maxstack_flag, sizeof maxstack_flag, "max-wasm-stack=%s", wasm_stack);
    /* Run-step grant: map tests/work to guest ../work so INSCRIBE/SCRY cases that write a sibling-of-cases path resolve under the WASI sandbox. */
    char work_grant[4096];
    snprintf(work_grant, sizeof work_grant, "%s/work::../work", tests_dir);
    snprintf(actual,    sizeof actual,    "%s/work/%s.actual",  tests_dir, name);
    snprintf(err_file,  sizeof err_file,  "%s/work/%s.err",     tests_dir, name);
    snprintf(stdin_file,sizeof stdin_file,"%s/%s.stdin",   cases_dir, name);

    abspath(transpiler_abs, sizeof transpiler_abs, tests_dir, transpiler);

    /* Clear old error log */
    { FILE *f = fopen(err_file, "w"); if (f) fclose(f); }

    /* ------------------------------------------------------------------ */
    /* Step 1: Transpile.  cwd = cases/ so module watcher imports resolve. */
    /* win64: the transpiler is a PE binary, run under wine.               */
    /* ------------------------------------------------------------------ */
    {
        char *native_argv[] = { transpiler_abs, NULL };
        char *win64_argv[]  = { "wine", transpiler_abs, NULL };
        /* wasm: run athtoc.wasm under wasmtime with cwd = cases/ and a --dir grant on cwd so module-import watchers resolve sibling .~ATH files. */
        char *wasm_argv[] = {
            (char *)wasmtime, "run",
            "-W", "exceptions=y", "-W", maxstack_flag,
            "--dir", ".::.", transpiler_abs, NULL
        };
        char *const *argv = native_argv;
        if (win64) argv = win64_argv;
        else if (wasm) argv = wasm_argv;
        {
            int rc = run_cmd(argv, src, c_file, err_file, cases_dir);
            if (rc != 0) {
                write_sentinel(actual, "TRANSPILE_FAIL");
                return 0;
            }
        }
    }

    /* ------------------------------------------------------------------ */
    /* Step 2: Compile.  cwd = tests/ (runtime paths are relative to it). */
    /* ------------------------------------------------------------------ */
    if (win64) {
        /* Cross-compile with mingw + vendored libffi; mirrors the bin-win64 recipe.  Link order: object -> runtime .a -> libffi .a -> ws2_32. */
        char runtime_inc[4096], ffi_inc[4096], runtime_lib[4096], ffi_lib[4096];
        snprintf(runtime_inc, sizeof runtime_inc, "-I%s/../runtime", tests_dir);
        snprintf(ffi_inc,     sizeof ffi_inc,     "-I%s/../vendor/win64/libffi/include", tests_dir);
        snprintf(runtime_lib, sizeof runtime_lib, "%s/../libath_runtime_win64.a", tests_dir);
        snprintf(ffi_lib,     sizeof ffi_lib,     "%s/../vendor/win64/libffi/lib/libffi.a", tests_dir);

        char *argv[] = {
            "x86_64-w64-mingw32-gcc", "-std=c89",
            "-Wno-unused-variable", "-Wno-declaration-after-statement",
            "-Wno-pedantic-ms-format",
            c_file, runtime_inc, ffi_inc, runtime_lib,
            "-Wl,-Bstatic", ffi_lib, "-Wl,-Bdynamic", "-lws2_32",
            "-static-libgcc",
            "-o", bin_file,
            NULL
        };
        int rc = run_cmd(argv, NULL, NULL, err_file, tests_dir);
        if (rc != 0) {
            write_sentinel(actual, "COMPILE_FAIL");
            return 0;
        }
    } else if (wasm) {
        /* Compile to a standalone WASI module against libath_runtime_wasm.a. Mirrors the bin-wasm recipe: sjlj lowering to modern exnref, -lsetjmp for the helpers, and a 256 MB linear-memory stack. */
        char runtime_inc[4096], runtime_lib[4096];
        snprintf(runtime_inc, sizeof runtime_inc, "-I%s/../runtime", tests_dir);
        snprintf(runtime_lib, sizeof runtime_lib, "%s/../libath_runtime_wasm.a", tests_dir);

        char *argv[] = {
            (char *)wasi_clang, "--target=wasm32-wasi", sysroot_flag,
            "-std=c89", "-Wno-deprecated",
            "-Wno-unused-variable", "-Wno-declaration-after-statement",
            "-mllvm", "-wasm-enable-sjlj", "-mllvm", "-wasm-use-legacy-eh=false",
            "-O2", c_file, runtime_inc, runtime_lib,
            "-lsetjmp", "-Wl,-z,stack-size=268435456", "-Wl,--stack-first",
            "-o", bin_file,
            NULL
        };
        int rc = run_cmd(argv, NULL, NULL, err_file, tests_dir);
        if (rc != 0) {
            write_sentinel(actual, "COMPILE_FAIL");
            return 0;
        }
    } else {
        char runtime_inc[4096], lib_dir[4096];
        /* tests_dir is .../transpiler-to-c/tests/; runtime and lib are one level up */
        snprintf(runtime_inc, sizeof runtime_inc, "-I%s/../runtime", tests_dir);
        snprintf(lib_dir,     sizeof lib_dir,     "-L%s/..",         tests_dir);

        char *argv[] = {
            "gcc", "-std=c89", "-pedantic",
            "-Wno-unused-variable", "-Wno-declaration-after-statement",
            c_file, runtime_inc, lib_dir,
            "-lath_runtime", "-ldl", "-lffi",
            "-o", bin_file,
            NULL
        };
        /* gcc resolves -I/-L paths itself, so cwd doesn't matter. */
        int rc = run_cmd(argv, NULL, NULL, err_file, tests_dir);
        if (rc != 0) {
            write_sentinel(actual, "COMPILE_FAIL");
            return 0;
        }
    }

    /* ------------------------------------------------------------------ */
    /* Step 3: Run.  cwd = cases/ so module watcher imports resolve. */
    /* win64: run the .exe under wine, then normalise CRLF -> LF.     */
    /* ------------------------------------------------------------------ */
    {
        const char *stdin_path = path_exists(stdin_file) ? stdin_file : "/dev/null";
        char *native_argv[] = { bin_file, NULL };
        char *win64_argv[]  = { "wine", bin_file, NULL };
        /* wasm: run under wasmtime, same --dir grant so a watcher entity can stat its module file at runtime. */
        char *wasm_argv[] = {
            (char *)wasmtime, "run",
            "-W", "exceptions=y", "-W", maxstack_flag,
            "--dir", ".::.", "--dir", work_grant, bin_file, NULL
        };
        char *const *argv = native_argv;
        if (win64) argv = win64_argv;
        else if (wasm) argv = wasm_argv;
        run_cmd(argv, stdin_path, actual, err_file, cases_dir);
        /* Ignore run exit status -- harness compares output files. */
        if (win64) strip_cr(actual);
    }

    return 0;
}
