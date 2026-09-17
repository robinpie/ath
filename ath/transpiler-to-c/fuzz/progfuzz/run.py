# SPDX-License-Identifier: GPL-2.0-only
#
# run.py -- differential testing of random whole !~ATH programs (fuzz step 8).
#
# Each program from gen.py is transpiled once by ./athtoc-bin and built six
# ways, in two groups that must each agree on stdout, uncaught-error message
# and exit status:
#
#   64-bit long: gcc -O0 (libath_runtime.a), clang -O2, gcc ASan+UBSan
#   32-bit long: gcc -m32 -O2 (i686), wasm32-wasi (wasmtime), win64 (wine)
#
# The groups are not compared with each other: integers legitimately wrap at a
# different width. Also reported: any sanitizer report, a crash (signal) or a
# timeout in any variant. Every disagreement is re-run once to weed out flakes,
# then saved under fuzz/work/progfuzz/findings/<seed>/.
#
#   python3 fuzz/progfuzz/run.py --count 500 [--start SEED] [--jobs N] [--only 64|32]
# (run from ath/transpiler-to-c; the first run builds the extra runtime libraries)
import argparse
import concurrent.futures as cf
import glob
import os
import re
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gen  # noqa: E402

ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))       # ath/transpiler-to-c
WORK = os.path.join(ROOT, "fuzz", "work", "progfuzz")
LIBS = os.path.join(WORK, "lib")
RT = os.path.join(ROOT, "runtime")
SRCS = sorted(f for f in glob.glob(os.path.join(RT, "*.c")) if not f.endswith("test_runtime.c"))
SYSROOT = os.environ.get("WASI_SYSROOT", "/usr/share/wasi-sysroot")
TIMEOUT = 20
SAN_ENV = {"ASAN_OPTIONS": "allow_user_segv_handler=1:detect_leaks=0", "UBSAN_OPTIONS": "print_stacktrace=1"}
WARN_OFF = ["-w"]


def sh(argv, cwd=None, env=None, timeout=300, stdin=None):
    e = dict(os.environ)
    if env:
        e.update(env)
    try:
        p = subprocess.run(argv, cwd=cwd, env=e, timeout=timeout, stdin=stdin,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return p.returncode, p.stdout, p.stderr
    except subprocess.TimeoutExpired as ex:
        return "TIMEOUT", ex.stdout or b"", ex.stderr or b""


def build_libs():
    os.makedirs(LIBS, exist_ok=True)
    wanted = {
        "libath_clang_o2.a": (["clang", "-std=c89", "-O2", "-w", "-c"], ".clo2.o"),
        "libath_i686.a": (["gcc", "-std=c89", "-m32", "-O2", "-w", "-c"], ".i686.o"),
    }
    for lib, (cc, suffix) in wanted.items():
        path = os.path.join(LIBS, lib)
        if os.path.exists(path) and os.path.getmtime(path) > max(os.path.getmtime(f) for f in SRCS + glob.glob(RT + "/*.h")):
            continue
        objs = []
        for src in SRCS:
            obj = os.path.join(LIBS, os.path.basename(src)[:-2] + suffix)
            rc, _, err = sh(cc + ["-I" + RT, src, "-o", obj])
            if rc != 0:
                sys.exit("building %s failed:\n%s" % (lib, err.decode()))
            objs.append(obj)
        if os.path.exists(path):
            os.remove(path)
        sh(["ar", "rcs", path] + objs)
    for need in ("libath_runtime.a", "libath_runtime_san.a", "libath_runtime_win64.a", "libath_runtime_wasm.a"):
        if not os.path.exists(os.path.join(ROOT, need)):
            sys.exit("missing %s: run make lib lib-san lib-win64 lib-wasm" % need)


# name -> (group, compile argv builder, run argv builder, env)
def variants(c_file, base):
    L = ROOT
    return {
        "gcc-O0": ("64", ["gcc", "-std=c89"] + WARN_OFF + [c_file, "-I" + RT, "-L" + L, "-lath_runtime", "-ldl", "-lffi", "-lm", "-o", base + ".gcc"],
                   [base + ".gcc"], None),
        "clang-O2": ("64", ["clang", "-std=c89", "-O2"] + WARN_OFF + [c_file, "-I" + RT, os.path.join(LIBS, "libath_clang_o2.a"), "-ldl", "-lffi", "-lm", "-o", base + ".clang"],
                     [base + ".clang"], None),
        "asan": ("64", ["gcc", "-std=c89", "-g", "-O1", "-fno-omit-frame-pointer", "-fsanitize=address,undefined"] + WARN_OFF +
                 [c_file, "-I" + RT, "-L" + L, "-lath_runtime_san", "-ldl", "-lffi", "-lm", "-o", base + ".asan"],
                 [base + ".asan"], SAN_ENV),
        "i686-O2": ("32", ["gcc", "-std=c89", "-m32", "-O2"] + WARN_OFF + [c_file, "-I" + RT, os.path.join(LIBS, "libath_i686.a"), "-L/usr/lib32", "-ldl", "-lffi", "-lm", "-o", base + ".i686"],
                    [base + ".i686"], None),
        "wasm": ("32", ["clang", "--target=wasm32-wasi", "--sysroot=" + SYSROOT, "-std=c89", "-O2"] + WARN_OFF +
                 ["-mllvm", "-wasm-enable-sjlj", "-mllvm", "-wasm-use-legacy-eh=false", c_file, "-I" + RT,
                  os.path.join(L, "libath_runtime_wasm.a"), "-lsetjmp", "-Wl,-z,stack-size=268435456", "-Wl,--stack-first",
                  "-o", base + ".wasm"],
                 ["wasmtime", "run", "-W", "exceptions=y", "-W", "max-wasm-stack=1073741824", base + ".wasm"], None),
        "win64": ("32", ["x86_64-w64-mingw32-gcc", "-std=c89"] + WARN_OFF + [c_file, "-I" + RT, "-I" + os.path.join(L, "vendor/win64/libffi/include"),
                  os.path.join(L, "libath_runtime_win64.a"), "-Wl,-Bstatic", os.path.join(L, "vendor/win64/libffi/lib/libffi.a"),
                  "-Wl,-Bdynamic", "-lws2_32", "-static-libgcc", "-o", base + ".exe"],
                  ["wine", base + ".exe"], {"WINEDEBUG": "-all"}),
    }


def observe(rc, out, err, sort_lines):
    """Normalise one run into a comparable string."""
    text = out.decode("utf-8", "replace").replace("\r\n", "\n")
    lines = text.split("\n")
    if sort_lines:
        lines = sorted(lines)
    errtext = err.decode("utf-8", "replace").replace("\r\n", "\n")
    uncaught = [l for l in errtext.split("\n") if l.startswith("Unhandled CONDEMN")]
    if rc == "TIMEOUT":
        status = "TIMEOUT"
    elif isinstance(rc, int) and (rc < 0 or rc > 128):
        status = "SIGNAL/%d" % rc
    else:
        status = "exit=%s" % ("0" if rc == 0 else "nonzero")
    return "\n".join(lines) + "\n--\n" + "\n".join(uncaught) + "\n--\n" + status


def sanitizer_report(err):
    t = err.decode("utf-8", "replace")
    m = re.search(r"(runtime error:.*|ERROR: AddressSanitizer:.*)", t)
    return m.group(1) if m else None


def run_seed(seed, groups, keep=False):
    d = os.path.join(WORK, "tmp", str(seed))
    os.makedirs(d, exist_ok=True)
    src, bifurcated = gen.generate(seed)
    athf = os.path.join(d, "prog.~ATH")
    cf_ = os.path.join(d, "prog.c")
    with open(athf, "w") as f:
        f.write(src)
    with open(athf, "rb") as fin:
        rc, cout, terr = sh([os.path.join(ROOT, "athtoc-bin")], stdin=fin, timeout=60)
    result = {"seed": seed, "issues": [], "status": "ok"}
    if rc != 0:
        result["status"] = "transpile-rejected"
        result["issues"].append(("transpile", terr.decode("utf-8", "replace")[:300]))
        return finish(result, d, keep=True)
    with open(cf_, "wb") as f:
        f.write(cout)
    obs = {}
    runs = {}
    for name, (group, cc, run, env) in variants(cf_, os.path.join(d, "prog")).items():
        if group not in groups:
            continue
        rc, _, cerr = sh(cc, cwd=d)
        if rc != 0:
            result["issues"].append(("compile:" + name, cerr.decode("utf-8", "replace")[:400]))
            continue
        runs[name] = (run, env, group)
        rc, out, err = sh(run, cwd=d, env=env, timeout=TIMEOUT)
        obs[name] = observe(rc, out, err, bifurcated)
        rep = sanitizer_report(err) if name == "asan" else None
        if rep:
            result["issues"].append(("sanitizer", rep))
            with open(os.path.join(d, "asan.stderr"), "wb") as f:
                f.write(err)
        if "TIMEOUT" in obs[name].split("--")[-1]:
            # under full load wine/wasmtime can be several times slower: only a
            # timeout that survives a much longer limit is a finding
            rc, out, err = sh(run, cwd=d, env=env, timeout=TIMEOUT * 6)
            obs[name] = observe(rc, out, err, bifurcated)
        if "TIMEOUT" in obs[name].split("--")[-1] or "SIGNAL" in obs[name].split("--")[-1]:
            result["issues"].append(("abnormal:" + name, obs[name].split("--")[-1].strip()))
    for g in groups:
        names = [n for n in obs if runs[n][2] == g]
        if len(set(obs[n] for n in names)) > 1:
            # re-run to separate flakes from real disagreement
            again = {}
            for n in names:
                run, env, _ = runs[n]
                rc, out, err = sh(run, cwd=d, env=env, timeout=TIMEOUT)
                again[n] = observe(rc, out, err, bifurcated)
            stable = all(again[n] == obs[n] for n in names)
            if len(set(again.values())) > 1:
                result["issues"].append(("diverge-%s%s" % (g, "" if stable else "-unstable"), ", ".join(names)))
    for n, o in obs.items():
        with open(os.path.join(d, n + ".obs"), "w") as f:
            f.write(o)
    if result["issues"]:
        result["status"] = "finding"
    return finish(result, d, keep)


def finish(result, d, keep):
    if result["status"] == "finding" or keep and result["status"] != "ok":
        dest = os.path.join(WORK, "findings" if result["status"] == "finding" else "rejected", str(result["seed"]))
        shutil.rmtree(dest, ignore_errors=True)
        shutil.move(d, dest)
        for ext in (".gcc", ".clang", ".asan", ".i686", ".wasm", ".exe"):
            p = os.path.join(dest, "prog" + ext)
            if os.path.exists(p):
                os.remove(p)
        with open(os.path.join(dest, "issues.txt"), "w") as f:
            for kind, detail in result["issues"]:
                f.write("%s: %s\n" % (kind, detail))
    else:
        shutil.rmtree(d, ignore_errors=True)
    return result


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--start", type=int, default=1)
    ap.add_argument("--count", type=int, default=100)
    ap.add_argument("--jobs", type=int, default=max(1, (os.cpu_count() or 2) - 2))
    ap.add_argument("--only", choices=["64", "32"])
    args = ap.parse_args()
    groups = [args.only] if args.only else ["64", "32"]
    build_libs()
    os.makedirs(WORK, exist_ok=True)
    counts = {}
    t0 = time.time()
    with cf.ThreadPoolExecutor(args.jobs) as ex:
        futs = [ex.submit(run_seed, s, groups) for s in range(args.start, args.start + args.count)]
        for i, fu in enumerate(cf.as_completed(futs), 1):
            r = fu.result()
            counts[r["status"]] = counts.get(r["status"], 0) + 1
            if r["issues"]:
                kinds = sorted(set(k for k, _ in r["issues"]))
                print("seed %d: %s" % (r["seed"], "; ".join(kinds)), flush=True)
            if i % 50 == 0:
                print("[%d/%d, %.0fs] %s" % (i, args.count, time.time() - t0, counts), flush=True)
    print("done in %.0fs: %s (findings in %s)" % (time.time() - t0, counts, os.path.join(WORK, "findings")))


if __name__ == "__main__":
    main()
