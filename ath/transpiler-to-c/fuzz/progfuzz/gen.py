# SPDX-License-Identifier: GPL-2.0-only
#
# gen.py -- random whole-program generator for !~ATH (fuzz step 8).
#
# Produces syntactically valid, terminating programs that print a lot of state,
# for differential testing across compilers/optimisation levels/targets (see
# run.py). Programs are built type-aware so most operations are well-typed, with
# a controlled share of runtime errors (division by zero, bad indices, wrong
# types, CONDEMN, reassigning an ENTOMBed name), usually inside ATTEMPT/SALVAGE.
#
# Termination: rites only call rites defined before them, except for one
# self-recursive pattern with a decreasing counter; iteration uses that
# pattern or chains of short timers. Determinism: no RANDOM/TIME/OUIJA; timers
# on one control path are awaited sequentially; bifurcated programs are marked
# so the runner compares sorted output.
#
#   python3 gen.py SEED            # print one program
import random
import sys

INT_OPS = ["+", "-", "*", "/", "%", "&", "|", "^", "<<", ">>"]
CMP_OPS = ["<", ">", "<=", ">=", "==", "!="]
WORDS = ["alpha", "beta", "karkat", "vriska", "", " pad ", "a,b,c", "x", "UPPER", "tab\\tq", "nl\\nq",
         "quote\\\"q", "back\\\\q", "12", "-7", "3.5", "  7 ", "café", "☺"]
KEYS = ["k", "name", "age", "x", "y", "long_key_name"]


class Gen:
    def __init__(self, seed):
        self.r = random.Random(seed)
        self.n = 0
        self.scopes = [{}]          # name -> type ("int", "float", "str", "bool", "arr", "map", "syl:KIND")
        self.consts = set()
        self.rites = []             # (name, arity, is_async)
        self.preds = []             # 2-arg predicate rites usable by TECHHOP
        self.bifurcated = False
        self.lines = []
        self.budget = 400           # rough statement budget per program

    # ---------- helpers ----------
    def fresh(self, prefix):
        self.n += 1
        return "%s%d" % (prefix, self.n)

    def chance(self, p):
        return self.r.random() < p

    def pick(self, seq):
        return self.r.choice(seq)

    def vars_of(self, t):
        out = []
        for sc in self.scopes:
            out.extend(n for n, ty in sc.items() if ty == t)
        return out

    def declare(self, name, t):
        self.scopes[-1][name] = t

    def emit(self, depth, text):
        self.lines.append("    " * depth + text)

    # ---------- expressions ----------
    def e_int(self, d=0):
        vs = self.vars_of("int")
        c = self.r.randrange(16) if d < 3 else self.r.randrange(3)
        if c == 0 or (c in (1, 2) and not vs):
            if self.chance(0.08):
                return self.pick(["2147483647", "2147483648", "-2147483648", "4611686018427387904", "(1 << 31)",
                                  "(1 << 62)", "(1 << 63)", "9223372036854775807"])
            return str(self.r.randint(-20, 100))
        if c in (1, 2):
            return self.pick(vs)
        if c in (3, 4, 5):
            op = self.pick(INT_OPS)
            a, b = self.e_int(d + 1), self.e_int(d + 1)
            if op in ("/", "%"):   # a zero divisor belongs to e_risky
                b = str(self.pick([1, 2, 3, 7, -4]))
            if op in ("<<", ">>") and self.chance(0.9):
                b = str(self.r.randint(0, 12))
            return "(%s %s %s)" % (a, op, b)
        if c == 6:
            return "-" + self.e_int(d + 1)
        if c == 7:
            return "LENGTH(%s)" % self.pick([self.e_str(d + 1), self.e_arr(d + 1)])
        if c == 8:   # INT of a bounded float; INT of an arbitrary one may be out of range (e_risky)
            return "INT(%s)" % self.pick(["2.5", "-7.75", "FLOAT(%s)" % self.r.randint(-50, 50), "1000.5"])
        if c == 9:
            return 'PARSE_INT("%s")' % self.r.randint(-999, 99999)
        if c == 10:
            syls = [n for sc in self.scopes for n, ty in sc.items() if ty.startswith("syl:")]
            if syls:
                return "COUNT(%s)" % self.pick(syls)
            return "CODE(%s)" % self.q(self.pick(["A", "z", "0", "☺", "é"]))
        if c == 11:
            sync = [r for r in self.rites if not r[2]]
            if sync:
                name, ar, _ = self.pick(sync)
                return "%s(%s)" % (name, ", ".join(self.e_int(d + 2) for _ in range(ar)))
            return "~" + self.e_int(d + 1)
        if c == 12:
            arrs = self.vars_of("arr")
            if arrs and self.chance(0.5):
                return "LENGTH(%s)" % self.pick(arrs)
            return "LENGTH(KEYS(%s))" % self.e_map(d + 1)
        if c == 13:
            return "INT(FLOAT(%s) / 2.0)" % self.e_int(d + 1)
        return str(self.r.randint(0, 9))

    def e_float(self, d=0):
        vs = self.vars_of("float")
        c = self.r.randrange(8) if d < 3 else 0
        if c == 0 or (c == 1 and not vs):
            return self.pick(["0.5", "1.25", "-3.75", "100.0", "0.1", "2.718281828", "3.14159265358979",
                              "1234567.891", "0.000125", "-0.0", "999999.5", "1000000.5", "0.00001",
                              "123456789012345.0", "1234567890123456789.0", "0.30000000000000004",
                              "4294967296.0", "2147483648.5", "-9876543210.0", "1.0e0" if False else "1.5"])
        if c == 1:
            return self.pick(vs)
        if c in (2, 3):
            if self.chance(0.3):   # divide only by a nonzero literal; a zero divisor is e_risky's job
                return "(%s / %s)" % (self.e_float(d + 1), self.pick(["2.0", "-0.5", "3.25"]))
            return "(%s %s %s)" % (self.e_float(d + 1), self.pick(["+", "-", "*"]), self.e_float(d + 1))
        if c == 4:
            return "FLOAT(%s)" % self.e_int(d + 1)
        if c == 5:
            return 'PARSE_FLOAT("%s")' % self.pick(["2.5", "-0.125", "1e3", "7", "6.02e23"])
        if c == 6:
            return "(%s + %s)" % (self.e_float(d + 1), self.e_int(d + 1))
        return "-" + self.e_float(d + 1)

    def q(self, s):
        return '"' + s + '"'

    def e_str(self, d=0):
        vs = self.vars_of("str")
        c = self.r.randrange(14) if d < 3 else 0
        if c == 0 or (c == 1 and not vs):
            return self.q(self.pick(WORDS))
        if c == 1:
            return self.pick(vs)
        if c == 2:
            return "(%s + %s)" % (self.e_str(d + 1), self.e_any(d + 1))
        if c == 3:
            return "STRING(%s)" % self.e_any(d + 1)
        if c == 4:
            return "SUBSTRING(%s, %s, %s)" % (self.e_str(d + 1), self.r.randint(-2, 5), self.r.randint(0, 9))
        if c == 5:
            return "%s(%s)" % (self.pick(["UPPERCASE", "LOWERCASE", "TRIM"]), self.e_str(d + 1))
        if c == 6:
            return "REPLACE(%s, %s, %s)" % (self.e_str(d + 1), self.q(self.pick(["a", "", ",", " "])), self.e_str(d + 2))
        if c == 7:
            return 'JOIN(%s, "%s")' % (self.e_arr(d + 1), self.pick([",", "", " - "]))
        if c == 8:
            return "CHAR(%s)" % self.pick(["65", "97", "9786", "233", "(65 + %d)" % self.r.randint(0, 25)])
        if c == 9:
            return "%s(%s)" % (self.pick(["HEX", "BIN"]), self.pick([str(self.r.randint(0, 100000)), self.e_int(d + 1)]))
        if c == 10:
            return "TYPEOF(%s)" % self.e_any(d + 1)
        if c == 11:
            return "(%s + %s)" % (self.e_any(d + 1), self.e_str(d + 1))
        if c == 12:
            arrs = self.vars_of("arr")
            if arrs:
                return "STRING(%s)" % self.pick(arrs)
        return self.q(self.pick(WORDS))

    def e_bool(self, d=0):
        c = self.r.randrange(8) if d < 3 else 0
        vs = self.vars_of("bool")
        if c == 0:
            return self.pick(["ALIVE", "DEAD"])
        if c == 1 and vs:
            return self.pick(vs)
        if c in (1, 2, 3):
            return "(%s %s %s)" % (self.e_int(d + 1), self.pick(CMP_OPS), self.e_int(d + 1))
        if c == 4:
            return "(%s %s %s)" % (self.e_str(d + 1), self.pick(["==", "!="]), self.e_str(d + 1))
        if c == 5:
            return "(%s %s %s)" % (self.e_bool(d + 1), self.pick(["AND", "OR"]), self.e_bool(d + 1))
        if c == 6:
            return "NOT %s" % self.e_bool(d + 1)
        return 'HAS(%s, "%s")' % (self.e_map(d + 1), self.pick(KEYS))

    def e_arr(self, d=0):
        vs = self.vars_of("arr")
        c = self.r.randrange(9) if d < 3 else 0
        if c == 0 or (c == 1 and not vs):
            n = self.r.randint(0, 4)
            return "[" + ", ".join(self.e_scalar(d + 1) for _ in range(n)) + "]"
        if c == 1:
            return self.pick(vs)
        if c == 2:
            return "%s(%s, %s)" % (self.pick(["APPEND", "PREPEND"]), self.e_arr(d + 1), self.e_scalar(d + 1))
        if c == 3:
            return "SLICE(%s, %s, %s)" % (self.e_arr(d + 1), self.r.randint(-1, 3), self.r.randint(0, 6))
        if c == 4:
            return "CONCAT(%s, %s)" % (self.e_arr(d + 1), self.e_arr(d + 1))
        if c == 5:
            return 'SPLIT(%s, "%s")' % (self.e_str(d + 1), self.pick([",", "", " ", "a"]))
        if c == 6:
            return "KEYS(%s)" % self.e_map(d + 1)
        if c == 7:
            return "VALUES(%s)" % self.e_map(d + 1)
        return "[%s]" % self.e_arr(d + 1)

    def e_map(self, d=0):
        vs = self.vars_of("map")
        c = self.r.randrange(5) if d < 3 else 0
        if c == 0 or (c == 1 and not vs):
            n = self.r.randint(0, 3)
            keys = self.r.sample(KEYS, n)
            return "{" + ", ".join("%s: %s" % (k, self.e_scalar(d + 1)) for k in keys) + "}"
        if c == 1:
            return self.pick(vs)
        if c in (2, 3):
            return 'SET(%s, "%s", %s)' % (self.e_map(d + 1), self.pick(KEYS), self.e_scalar(d + 1))
        return 'DELETE(%s, "%s")' % (self.e_map(d + 1), self.pick(KEYS))

    def e_scalar(self, d=0):
        return self.pick([self.e_int, self.e_str, self.e_bool, self.e_float])(d)

    def e_any(self, d=0):
        return self.pick([self.e_int, self.e_int, self.e_str, self.e_bool, self.e_float, self.e_arr, self.e_map])(d)

    def e_of(self, t, d=0):
        return {"int": self.e_int, "float": self.e_float, "str": self.e_str, "bool": self.e_bool,
                "arr": self.e_arr, "map": self.e_map}[t](d)

    # risky expressions that may raise
    def e_risky(self):
        c = self.r.randrange(9)
        if c == 0:
            return self.pick(["(%s / %s)" % (self.e_int(), self.pick(["0", "(1 - 1)"])),
                              "(%s / 0.0)" % self.e_float(), "(%s %% 0)" % self.e_int()])
        if c == 1:
            return "%s[%s]" % (self.e_arr(), self.r.randint(-2, 6))
        if c == 2:
            return 'PARSE_INT("%s")' % self.pick(["abc", "1.5", "", "12x"])
        if c == 3:
            return "FIRST(%s)" % self.e_arr()
        if c == 4:
            return "(%s + %s)" % (self.e_arr(), self.e_int())
        if c == 5:
            return "%s.%s" % (self.e_map(), self.pick(KEYS))
        if c == 6:
            return "INT(%s)" % self.pick([self.e_str(), self.e_float()])
        if c == 7:
            return "(%s < %s)" % (self.e_str(), self.e_int())
        return "LAST(%s)" % self.e_arr()

    # ---------- statements ----------
    def utter(self, depth):
        n = self.r.randint(1, 3)
        self.emit(depth, "UTTER(%s);" % ", ".join(self.e_any() for _ in range(n)))

    def stmt(self, depth, in_rite=False, allow_async=True):
        self.budget -= 1
        c = self.r.randrange(22)
        if self.budget <= 0:
            c = 0
        if c in (0, 1, 2, 3):
            t = self.pick(["int", "int", "str", "float", "bool", "arr", "map"])
            name = self.fresh("v")
            self.emit(depth, "BIRTH %s WITH %s;" % (name, self.e_of(t)))
            self.declare(name, t)
            if self.chance(0.5):
                self.emit(depth, "UTTER(%s);" % name)
        elif c in (4, 5):
            self.utter(depth)
        elif c == 6:
            t = self.pick(["int", "str", "arr", "map", "float"])
            vs = [v for v in self.vars_of(t) if v not in self.consts]
            if vs:
                v = self.pick(vs)
                self.emit(depth, "%s = %s;" % (v, self.e_of(t)))
                self.emit(depth, "UTTER(%s);" % v)
            else:
                self.utter(depth)
        elif c == 7:
            name = self.fresh("C")
            self.emit(depth, "ENTOMB %s WITH %s;" % (name, self.e_int()))
            self.declare(name, "int")
            self.consts.add(name)
        elif c in (8, 9):
            self.cond(depth, in_rite, allow_async)
        elif c in (10, 11):
            self.attempt(depth, in_rite, allow_async)
        elif c == 12 and depth == 0 and not in_rite:
            self.rite_def(depth)
        elif c == 13 and depth == 0 and not in_rite:
            self.rec_rite(depth)
        elif c in (14, 15):
            self.sylladex(depth)
        elif c == 16 and allow_async and not in_rite:
            self.timer_wait(depth, allow_async)
        elif c == 17 and depth == 0 and not in_rite:
            self.async_rite(depth)
        elif c == 18:
            asyncs = [r for r in self.rites if r[2]]
            if asyncs and allow_async and not in_rite:
                name, ar, _ = self.pick(asyncs)
                self.emit(depth, "%s(%s);" % (name, ", ".join(self.e_int() for _ in range(ar))))
            else:
                self.utter(depth)
        elif c == 19 and depth == 0 and not in_rite and not self.bifurcated and self.chance(0.3):
            self.bifurcate(depth)
        elif c == 21 and self.chance(0.5):
            m = self.fresh("M")
            self.emit(depth, "BIRTH %s WITH {};" % m)
            for i in range(self.r.randint(5, 30)):
                self.emit(depth, '%s = SET(%s, "%s%d", %s);' % (m, m, self.pick(KEYS), i, self.e_scalar(2)))
            if self.chance(0.5):
                self.emit(depth, '%s = DELETE(%s, "%s%d");' % (m, m, self.pick(KEYS), self.r.randint(0, 10)))
            self.declare(m, "map")
            self.emit(depth, "UTTER(%s, LENGTH(KEYS(%s)), VALUES(%s));" % (m, m, m))
        elif c == 20:
            self.emit(depth, "UTTER(%s == %s, %s != %s);" % ((self.e_any(),) * 2 + (self.e_any(),) * 2))
        else:
            self.utter(depth)

    def block(self, depth, count, in_rite=False, allow_async=True):
        self.scopes.append({})
        for _ in range(count):
            self.stmt(depth, in_rite, allow_async)
        self.scopes.pop()

    def cond(self, depth, in_rite, allow_async):
        self.emit(depth, "SHOULD %s {" % self.e_bool())
        self.block(depth + 1, self.r.randint(1, 3), in_rite, allow_async)
        while self.chance(0.3):
            self.emit(depth, "} LEST SHOULD %s {" % self.e_bool())
            self.block(depth + 1, self.r.randint(1, 2), in_rite, allow_async)
        if self.chance(0.5):
            self.emit(depth, "} LEST {")
            self.block(depth + 1, self.r.randint(1, 2), in_rite, allow_async)
        self.emit(depth, "}")

    def attempt(self, depth, in_rite, allow_async):
        self.emit(depth, "ATTEMPT {")
        self.scopes.append({})
        for _ in range(self.r.randint(0, 2)):
            self.stmt(depth + 1, in_rite, False)
        k = self.r.randrange(4)
        if k == 0:
            self.emit(depth + 1, "CONDEMN %s;" % self.e_str())
        elif k == 1 and self.consts:
            self.emit(depth + 1, "%s = 1;" % self.pick(sorted(self.consts)))
        else:
            self.emit(depth + 1, "UTTER(%s);" % self.e_risky())
        self.emit(depth + 1, "UTTER(\"not reached?\");")
        self.scopes.pop()
        e = self.fresh("err")
        self.emit(depth, "} SALVAGE %s {" % e)
        self.scopes.append({e: "str"})
        self.emit(depth + 1, "UTTER(\"caught:\", %s);" % e)
        if self.chance(0.3):
            self.stmt(depth + 1, in_rite, False)
        self.scopes.pop()
        self.emit(depth, "}")

    def rite_def(self, depth):
        name = self.fresh("r")
        ar = self.r.randint(0, 3)
        params = [self.fresh("p") for _ in range(ar)]
        self.emit(depth, "RITE %s(%s) {" % (name, ", ".join(params)))
        saved = self.scopes
        self.scopes = [saved[0], {p: "int" for p in params}]
        for _ in range(self.r.randint(0, 3)):
            self.stmt(depth + 1, in_rite=True, allow_async=False)
        self.emit(depth + 1, "BEQUEATH %s;" % self.e_int())
        self.scopes = saved
        self.emit(depth, "}")
        self.rites.append((name, ar, False))
        if self.chance(0.4):
            p1, p2 = self.fresh("p"), self.fresh("p")
            pname = self.fresh("pred")
            self.emit(depth, "RITE %s(%s, %s) { BEQUEATH %s; }" % (
                pname, p1, p2, self.pick(["ALIVE", "%s == 0" % p2, "%s %% 2 == 0" % p2,
                                         'TYPEOF(%s) == "INTEGER"' % p1, "%s >= 1" % p2])))
            self.preds.append(pname)

    def rec_rite(self, depth):
        name = self.fresh("rec")
        n, acc = self.fresh("n"), self.fresh("acc")
        self.emit(depth, "RITE %s(%s, %s) {" % (name, n, acc))
        saved = self.scopes
        self.scopes = [saved[0], {n: "int", acc: "int"}]
        # the counter bound keeps later calls with arbitrary integers shallow and short
        self.emit(depth + 1, "SHOULD %s <= 0 OR %s > 64 { BEQUEATH %s; }" % (n, n, acc))
        step = "(%s %s %s)" % (acc, self.pick(["+", "*", "-", "^"]), self.e_int(2))
        if self.chance(0.5):
            # tail call (compiled to a loop)
            self.emit(depth + 1, "BEQUEATH %s(%s - 1, %s);" % (name, n, step))
        else:
            self.emit(depth + 1, "BEQUEATH %s + %s(%s - 1, %s);" % (self.e_int(2), name, n, step))
        self.scopes = saved
        self.emit(depth, "}")
        self.rites.append((name, 2, False))
        self.emit(depth, "UTTER(\"%s\", %s(%d, %s));" % (name, name, self.r.randint(0, 60), self.e_int()))

    def async_rite(self, depth):
        name = self.fresh("ar")
        p = self.fresh("p")
        t = self.fresh("T")
        self.emit(depth, "RITE %s(%s) {" % (name, p))
        saved = self.scopes
        self.scopes = [saved[0], {p: "int"}]
        self.emit(depth + 1, "UTTER(\"%s start\", %s);" % (name, p))
        self.emit(depth + 1, "import timer %s(%dms);" % (t, self.r.randint(1, 4)))
        self.emit(depth + 1, "~ATH(%s) {" % t)
        self.emit(depth + 1, "} EXECUTE(")
        self.scopes.append({})
        for _ in range(self.r.randint(1, 3)):
            self.stmt(depth + 2, in_rite=True, allow_async=False)
        self.scopes.pop()
        self.emit(depth + 2, "UTTER(\"%s done\", %s)" % (name, p))
        self.emit(depth + 1, ");")
        self.scopes = saved
        self.emit(depth, "}")
        self.rites.append((name, 1, True))

    def timer_wait(self, depth, allow_async):
        t = self.fresh("T")
        self.emit(depth, "import timer %s(%dms);" % (t, self.r.randint(1, 3)))
        self.emit(depth, "~ATH(%s) {" % t)
        self.emit(depth, "} EXECUTE(")
        self.block(depth + 1, self.r.randint(1, 3), False, allow_async and depth < 2)
        self.emit(depth + 1, "UTTER(\"%s fired\")" % t)
        self.emit(depth, ");")

    def bifurcate(self, depth):
        self.bifurcated = True
        a, b = self.fresh("BR"), self.fresh("BR")
        self.emit(depth, "bifurcate THIS[%s, %s];" % (a, b))
        for br in (a, b):
            self.emit(depth, "~ATH(%s) {" % br)
            self.block(depth + 1, self.r.randint(1, 3), False, False)
            self.emit(depth, "} EXECUTE(UTTER(\"%s done\"));" % br)
        self.emit(depth, "[%s, %s].DIE();" % (a, b))

    def sylladex(self, depth):
        kind = self.pick(["STACK", "QUEUE", "TREE", "HASHMAP", "BOTTLE", "TECHHOP", "TREE_B"])
        name = self.fresh("S")
        if kind == "TECHHOP" and not self.preds:
            kind = "STACK"
        if kind in ("STACK", "QUEUE"):
            self.emit(depth, "BIRTH %s WITH %s(%d);" % (name, kind, self.r.randint(0, 4)))
        elif kind == "TREE":
            self.emit(depth, "BIRTH %s WITH TREE();" % name)
        elif kind == "TREE_B":
            kind = "TREE"
            self.emit(depth, "BIRTH %s WITH TREE(ALIVE);" % name)
        elif kind in ("HASHMAP", "BOTTLE"):
            self.emit(depth, "BIRTH %s WITH %s(%d);" % (name, kind, self.r.randint(1, 5)))
        else:
            g, s = self.pick(self.preds), self.pick(self.preds)
            self.emit(depth, "BIRTH %s WITH TECHHOP(%d, %d, %s, %s);" % (name, self.r.randint(1, 3),
                                                                       self.r.randint(1, 3), g, s))
        self.declare(name, "syl:" + kind)
        for _ in range(self.r.randint(1, 5)):
            v = self.e_scalar(1)
            if kind == "HASHMAP":
                self.emit(depth, "CAPTCHALOGUE %s WITH %s INTO %s;" % (v, self.e_scalar(2), name))
            else:
                self.emit(depth, "CAPTCHALOGUE %s INTO %s;" % (v, name))
        self.emit(depth, "UTTER(STRING(%s), COUNT(%s));" % (name, name))
        for _ in range(self.r.randint(0, 3)):
            if kind in ("STACK", "QUEUE"):
                ej = "EJECT FROM %s" % name
            elif kind == "TREE":
                ej = "EJECT %s FROM %s" % (self.pick(["ROOT", "LEAF", "LEAF"]), name)
            elif kind == "HASHMAP":
                ej = self.pick(["EJECT SLOT %d FROM %s" % (self.r.randint(0, 4), name),
                                "EJECT %s FROM %s" % (self.e_scalar(2), name)])
            elif kind == "BOTTLE":
                ej = self.pick(["EJECT FROM %s" % name, "EJECT SLOT %d FROM %s" % (self.r.randint(0, 4), name)])
            else:
                ej = "EJECT GROOVE %d SHADE %d FROM %s" % (self.r.randint(0, 2), self.r.randint(0, 2), name)
            e = self.fresh("err")
            self.emit(depth, "ATTEMPT { UTTER(%s); } SALVAGE %s { UTTER(\"eject failed:\", %s); }" % (ej, e, e))
        self.emit(depth, "UTTER(STRING(%s), %s);" % (name, name))

    # ---------- program ----------
    def program(self):
        for _ in range(self.r.randint(8, 40)):
            self.stmt(0)
        self.emit(0, "THIS.DIE();")
        header = "// generated by fuzz/progfuzz/gen.py%s\n" % (" [bifurcated]" if self.bifurcated else "")
        return header + "\n".join(self.lines) + "\n"


def generate(seed):
    g = Gen(seed)
    src = g.program()
    return src, g.bifurcated


if __name__ == "__main__":
    sys.stdout.write(generate(int(sys.argv[1]) if len(sys.argv) > 1 else 0)[0])
