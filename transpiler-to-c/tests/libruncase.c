/* libruncase.c — per-case executor for the !~ATH test harness.
 *
 * Exported function:
 *   long ath_run_case(const char *name, const char *athtoc)
 *
 * Transpiles cases/<name>.~ATH with athtoc (falling back to the ATHTOC
 * environment variable, then "../athtoc-bin"), compiles the result with
 * gcc, runs the binary, and writes stdout to work/<name>.actual.
 * On any toolchain failure the actual file is filled with a sentinel
 * (TRANSPILE_FAIL / COMPILE_FAIL) so the harness sees a clean FAIL.
 *
 * Must be invoked with the process cwd set to the tests/ directory.
 *
 * Copyright (C) 2026 robinpie
 * GPL v2.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>

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

long run_case(const char *name, const char *athtoc) {
    /* Resolve the transpiler path: argument → ATHTOC env → default */
    const char *transpiler = (athtoc && *athtoc) ? athtoc : getenv("ATHTOC");
    if (!transpiler || !*transpiler) transpiler = "../athtoc-bin";

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
    snprintf(bin_file,  sizeof bin_file,  "%s/work/%s.bin",    tests_dir, name);
    snprintf(actual,    sizeof actual,    "%s/work/%s.actual",  tests_dir, name);
    snprintf(err_file,  sizeof err_file,  "%s/work/%s.err",     tests_dir, name);
    snprintf(stdin_file,sizeof stdin_file,"%s/%s.stdin",   cases_dir, name);

    abspath(transpiler_abs, sizeof transpiler_abs, tests_dir, transpiler);

    /* Clear old error log */
    { FILE *f = fopen(err_file, "w"); if (f) fclose(f); }

    /* ------------------------------------------------------------------ */
    /* Step 1: Transpile.  cwd = cases/ so module watcher imports resolve. */
    /* ------------------------------------------------------------------ */
    {
        char *argv[] = { transpiler_abs, NULL };
        int rc = run_cmd(argv, src, c_file, err_file, cases_dir);
        if (rc != 0) {
            write_sentinel(actual, "TRANSPILE_FAIL");
            return 0;
        }
    }

    /* ------------------------------------------------------------------ */
    /* Step 2: Compile.  cwd = tests/ (runtime paths are relative to it). */
    /* ------------------------------------------------------------------ */
    {
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
    /* ------------------------------------------------------------------ */
    {
        const char *stdin_path = path_exists(stdin_file) ? stdin_file : "/dev/null";
        char *argv[] = { bin_file, NULL };
        run_cmd(argv, stdin_path, actual, err_file, cases_dir);
        /* Ignore run exit status — harness compares output files. */
    }

    return 0;
}
