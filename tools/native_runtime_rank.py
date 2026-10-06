#!/usr/bin/env python3
"""native_runtime_rank.py -- rank the in-scope gap rows by how often the native
targets EXECUTE them on real data, measured at runtime.

WHY: a static call graph says what the native executables COULD reach. This
says what they DID reach: every native target is built with clang's frontend
profile instrumentation (-fprofile-instr-generate -fcoverage-mapping), run on
the same real inputs tools/native_health.sh uses, and each function's ENTRY
COUNT is read back with llvm-cov. Frontend instrumentation counts a function
before inlining, so an inlined body is still counted.

The executed native functions are then joined to report.json's sub-100 rows
(fuzzy_match_percent < 100) whose unit source lives under src/band3 or
src/system (src/network and src/xdk are out of scope) by DEMANGLED SIGNATURE,
in three tiers, the first that matches deciding:
    sig    qualified name + normalized parameter types (+ const)
    arity  qualified name + parameter count     (type spellings differ, e.g.
           STLport vs libc++ template arguments, ILP32 vs LP64 long)
    name   qualified name alone, only when the name is unique on BOTH sides
Only native functions DEFINED in this repo's src/ (per the coverage mapping's
filename) are counted, so a milo-native-engine class with the same name as one
of ours cannot be credited to our row.

USAGE
    tools/native_runtime_rank.py [project_dir] [--no-build] [--no-run]
                                 [--out TSV] [--json OUT] [--top N]
    --no-build  reuse native/build-prof as it is (configured + built)
    --no-run    reuse the per-target profdata already in native/build-prof/prof

    Inputs: RB3_ASSETS / RB3_ARK_REF / RB3_MILOHAX / RB3_MID_* / RB3_SONGS_DTA,
    same defaults and meaning as tools/native_health.sh.

SELF-VALIDATION (exit 3 when any fails -- the vacuous-join traps):
    * the target table here equals native_health.sh's run_target list
    * every target ran rc=0 and printed its completion marker
    * Symbol::Symbol(char const*) (a function every target calls) was counted
      AND joins to its report row ??0Symbol@@QAA@PBD@Z on the sig tier
    * at least one in-scope row is executed and at least one is not

EXIT: 0 ok; 1 a target failed to run; 2 could not run (no build/tools);
      3 self-validation failed.
"""
import argparse, collections, json, os, re, subprocess, sys

# name, completion marker, argv (format keys filled from inputs below).
# MUST mirror tools/native_health.sh's run_target lines; checked at runtime.
TARGETS = [
    ("rb3-dta",     r"^Done\. Showed [0-9]+ song", ["{SONGS_DTA}"]),
    ("rb3-song",    r"^RESULT: ALL GATES PASSED",  ["{ASSETS}"]),
    ("rb3-midi",    r"^RESULT: ALL GATES PASSED",  ["{ASSETS}"]),
    ("rb3-gem",     r"^Done\.$",                   ["{MID_PILLS}"]),
    ("rb3-hit",     r"^Done\.$",                   ["{MID_PILLS}"]),
    ("rb3-score",   r"^Done\.$",                   ["{MID_PILLS}"]),
    ("rb3-score2",  r"^RESULT: OK",                []),
    ("rb3-score3",  r"^Done\.$",                   ["{MID_VICARIOUS}"]),
    ("rb3-score4",  r"^Done\.$",                   ["{MID_VICARIOUS}"]),
    ("rb3-vocal",   r"^  all-off \(\+6\) ",        ["{MID_VICARIOUS}"]),
    ("rb3-vocal2",  r"^Done\.$",                   ["{MID_VICARIOUS}"]),
    ("rb3-harmony", r"^Done\.$",                   ["{MID_CENTERFOLD}"]),
    ("rb3-crowd",   r"^=== M12 complete",          ["{MID_VICARIOUS}"]),
    ("rb3-save",    r"^=== ALL ROUND-TRIPS OK",    []),
    ("rb3-ark",     r"^RESULT: ALL GATES PASSED",  ["{ASSETS}", "{ARK_REF}"]),
    ("rb3-frame",   r"^rb3-frame: OK ",            ["{OUT}/frame.png"]),
    ("rb3-milo",    r"^RESULT: ALL GATES PASSED",  ["{ASSETS}", "ui/track/gen/tracksystem_meshes.milo_xbox"]),
    ("rb3-render",  r"^RESULT: ALL GATES PASSED",  ["{ASSETS}", "{OUT}/render_out"]),
]

PROF_FLAGS = "-fprofile-instr-generate -fcoverage-mapping"
SELFCHECK_MSVC = "??0Symbol@@QAA@PBD@Z"


def sh(cmd, **kw):
    return subprocess.run(cmd, **kw)


def inputs():
    home = os.path.expanduser("~")
    mh = os.environ.get("RB3_MILOHAX", os.path.join(home, "code/milohax"))
    return {
        "ASSETS": os.environ.get("RB3_ASSETS", os.path.join(home, "code/milohax/rb3/orig-assets/xbox-zip")),
        "ARK_REF": os.environ.get("RB3_ARK_REF", os.path.join(home, "code/milohax/rb3/orig-assets/extracted-xbox-full/songs/gen/songs.dtb")),
        "MID_VICARIOUS": os.environ.get("RB3_MID_VICARIOUS", mh + "/onyx/songs-grinnz/tool/vicarious/notes.mid"),
        "MID_PILLS": os.environ.get("RB3_MID_PILLS", mh + "/onyx/songs-cort/hurt/pills/notes.mid"),
        "MID_CENTERFOLD": os.environ.get("RB3_MID_CENTERFOLD", mh + "/rock-band-3-deluxe/_ark/songs/centerfold/centerfold.mid"),
        "SONGS_DTA": os.environ.get("RB3_SONGS_DTA", mh + "/rb3/orig-assets/extracted/songs/songs.dta"),
    }


def check_target_table(repo):
    txt = open(os.path.join(repo, "tools/native_health.sh")).read()
    names = re.findall(r"^run_target\s+(?:--gated\s+)?(rb3-[a-z0-9]+)", txt, re.M)
    mine = [t[0] for t in TARGETS]
    return names == mine, names


# ---------------------------------------------------------------- build ----
def build(repo, bdir):
    nat = os.path.join(repo, "native")
    if not os.path.exists(os.path.join(bdir, "CMakeCache.txt")):
        r = sh(["cmake", "-S", nat, "-B", bdir, "-G", "Ninja",
                "-DCMAKE_C_COMPILER=clang", "-DCMAKE_CXX_COMPILER=clang++",
                "-DCMAKE_C_FLAGS=" + PROF_FLAGS, "-DCMAKE_CXX_FLAGS=" + PROF_FLAGS,
                "-DCMAKE_EXE_LINKER_FLAGS=-fprofile-instr-generate"])
        if r.returncode:
            sys.exit(2)
    r = sh(["cmake", "--build", bdir])
    if r.returncode:
        print("native_runtime_rank: profile build FAILED", file=sys.stderr)
        sys.exit(2)


# ------------------------------------------------------------------ run ----
def run_all(bdir):
    inp = inputs()
    pdir = os.path.join(bdir, "prof")
    os.makedirs(pdir, exist_ok=True)
    inp["OUT"] = pdir
    status = {}
    for name, marker, argv in TARGETS:
        exe = os.path.join(bdir, name)
        args = [a.format(**inp) for a in argv]
        for f in os.listdir(pdir):
            if f.startswith(name + "-") and f.endswith(".profraw"):
                os.unlink(os.path.join(pdir, f))
        env = dict(os.environ, LLVM_PROFILE_FILE=os.path.join(pdir, name + "-%p.profraw"))
        try:
            p = sh(["timeout", "-k", "10", "300", exe] + args, env=env,
                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
            out, rc = p.stdout, p.returncode
        except FileNotFoundError:
            out, rc = "", 127
        ok = rc == 0 and re.search(marker, out, re.M) is not None
        raws = [os.path.join(pdir, f) for f in os.listdir(pdir)
                if f.startswith(name + "-") and f.endswith(".profraw")]
        if raws:
            sh(["llvm-profdata", "merge", "-sparse", "-o",
                os.path.join(pdir, name + ".profdata")] + raws, check=True)
        status[name] = {"rc": rc, "ok": ok, "profraw": len(raws)}
        print("  %-12s rc=%-3d %s profraw=%d" % (name, rc, "OK  " if ok else "FAIL", len(raws)))
    json.dump(status, open(os.path.join(pdir, "status.json"), "w"), indent=1)
    return status


# --------------------------------------------------------------- export ----
def export_counts(repo, bdir):
    """{mangled: {"count": total entries, "targets": {t: n}, "file": rel}}"""
    pdir = os.path.join(bdir, "prof")
    srcroot = os.path.realpath(os.path.join(repo, "src")) + os.sep
    out = {}
    for name, _, _ in TARGETS:
        pd = os.path.join(pdir, name + ".profdata")
        exe = os.path.join(bdir, name)
        if not os.path.exists(pd):
            continue
        p = sh(["llvm-cov", "export", "-skip-expansions", "-instr-profile", pd, exe],
               stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
        if p.returncode:
            print("llvm-cov export failed for", name, file=sys.stderr)
            continue
        data = json.loads(p.stdout)
        for fn in data["data"][0]["functions"]:
            fns = fn.get("filenames") or []
            if not fns:
                continue
            real = os.path.realpath(fns[0])
            if not real.startswith(srcroot):
                continue
            mangled = fn["name"]
            # file-local functions carry a "<file>;" or "<file>:" prefix
            loc = re.search(r"[;:](_Z\S+)$", mangled)
            if loc and not mangled.startswith("_Z"):
                mangled = loc.group(1)
            e = out.setdefault(mangled, {"count": 0, "targets": {}, "file": real[len(srcroot) - 4:]})
            c = int(fn["count"])
            e["count"] += c
            if c:
                e["targets"][name] = e["targets"].get(name, 0) + c
    return out


# ------------------------------------------------------------ demangling ----
def batch_demangle(tool, names, extra=()):
    if not names:
        return {}
    p = sh([tool] + list(extra), input="\n".join(names) + "\n",
           stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, errors="replace")
    lines = p.stdout.splitlines()
    if tool == "llvm-undname":   # echoes the input line, then the demangling, then blank
        res, i, it = {}, 0, iter(lines)
        for ln in it:
            if ln in names and ln not in res:
                nxt = next(it, "")
                res[ln] = nxt
        return res
    return dict(zip(names, lines))


def split_top(s, sep=","):
    out, depth, cur = [], 0, ""
    for ch in s:
        if ch in "<(":
            depth += 1
        elif ch in ">)":
            depth -= 1
        if ch == sep and depth == 0:
            out.append(cur); cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur)
    return out


def match_paren_back(s):
    """index of the '(' that opens the LAST top-level parameter list."""
    depth = 0
    for i in range(len(s) - 1, -1, -1):
        ch = s[i]
        if ch == ")":
            depth += 1
        elif ch == "(":
            depth -= 1
            if depth == 0:
                return i
    return -1


TYPE_SUBS = [
    (r"\b(class|struct|enum|union)\s+", ""),
    (r"\bunsigned __int64\b", "unsigned long long"),
    (r"\b__int64\b", "long long"),
    (r"\b__ptr64\b", ""),
    (r"`anonymous namespace'", "anon"),
    (r"\(anonymous namespace\)", "anon"),
    (r"\bstd::__1::", "std::"),
    (r"\b_STL::", "std::"),
    (r"\bstlpmtx_std::", "std::"),
]


def norm_type(t):
    for a, b in TYPE_SUBS:
        t = re.sub(a, b, t)
    return re.sub(r"\s+", "", t)


def key_from(qual, params, const):
    qual = norm_type(qual)
    ps = [norm_type(p) for p in split_top(params)]
    if ps == ["void"]:
        ps = []
    return qual, tuple(ps), const


def parse_msvc(dem, mangled):
    """public: virtual bool __cdecl Foo::Bar(int) const -> key"""
    if not dem or mangled.startswith(("??_9", "??__E", "??__F", "??_R", "??_7")):
        return None
    d = dem
    if mangled.startswith(("??_G", "??_E")):
        m = re.search(r"([\w:<>, ]+)::`(?:scalar|vector) deleting destructor'", d)
        return (norm_type(m.group(1)) + "::~D0", (), False) if m else None
    i = match_paren_back(d)
    if i < 0:
        return None
    tail = d[i:]
    const = bool(re.search(r"\)\s*const\b", tail))
    params = tail[1:tail.rfind(")")] if not const else tail[1:re.search(r"\)\s*const", tail).start()]
    head = d[:i]
    cc = re.search(r"__(cdecl|thiscall|stdcall|fastcall|clrcall|vectorcall) ", head)
    if cc:
        head = head[cc.end():]
    else:
        head = re.sub(r"^(public|protected|private): ", "", head)
        head = re.sub(r"^(virtual|static) ", "", head)
    return key_from(head.strip(), params, const)


def parse_itanium(dem, mangled):
    if not dem or dem == mangled:
        return None
    if re.search(r"D0E[v]", mangled) or "D0Ev" in mangled:
        m = re.match(r"(.*)::~[^:]+\(\)$", dem)
        if m:
            return (norm_type(m.group(1)) + "::~D0", (), False)
    i = match_paren_back(dem)
    if i < 0:
        return None
    tail = dem[i:]
    cm = re.search(r"\)\s*const\b", tail)
    const = cm is not None
    params = tail[1:cm.start()] if const else tail[1:tail.rfind(")")]
    head = dem[:i]
    # a template function's return type precedes the name: "bool Foo::bar<int>(...)"
    parts = split_top(head, " ")
    head = parts[-1] if parts else head
    return key_from(head.strip(), params, const)


# ------------------------------------------------------------------ rows ----
def in_scope_rows(repo):
    r = json.load(open(os.path.join(repo, "build/45410914/report.json")))
    o = json.load(open(os.path.join(repo, "objdiff.json")))
    sp = {u["name"]: (u.get("metadata") or {}).get("source_path", "") for u in o["units"]}
    rows = []
    for u in r["units"]:
        s = sp.get(u["name"], "")
        if not (s.startswith("src/band3/") or s.startswith("src/system/")):
            continue
        for f in u.get("functions", []):
            fz = float(f.get("fuzzy_match_percent", 0))
            rows.append({"unit": u["name"], "src": s, "name": f["name"],
                         "dem": (f.get("metadata") or {}).get("demangled_name", ""),
                         "size": int(f.get("size", 0)), "fuzzy": fz,
                         "mpn": float(f.get("match_percent_normalized", 0))})
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("project_dir", nargs="?", default=None)
    ap.add_argument("--no-build", action="store_true")
    ap.add_argument("--no-run", action="store_true")
    ap.add_argument("--out", default=None)
    ap.add_argument("--json", default=None)
    ap.add_argument("--top", type=int, default=40)
    a = ap.parse_args()
    repo = os.path.abspath(a.project_dir or os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
    bdir = os.path.join(repo, "native", "build-prof")
    fails = []

    same, theirs = check_target_table(repo)
    if not same:
        fails.append("target table differs from native_health.sh: %s" % theirs)

    if not a.no_build:
        build(repo, bdir)
    if not a.no_run:
        status = run_all(bdir)
    else:
        status = json.load(open(os.path.join(bdir, "prof", "status.json")))
    bad = [t for t, s in status.items() if not s["ok"]]
    if bad:
        print("native_runtime_rank: targets did not complete: %s" % bad, file=sys.stderr)

    counts = export_counts(repo, bdir)
    if not counts:
        print("native_runtime_rank: no profile data", file=sys.stderr)
        sys.exit(2)
    idem = batch_demangle("llvm-cxxfilt", sorted(counts))
    nat = collections.defaultdict(list)       # sig key -> [mangled]
    nat_ar = collections.defaultdict(list)
    nat_nm = collections.defaultdict(list)
    for m, d in idem.items():
        k = parse_itanium(d, m)
        if not k:
            continue
        nat[k].append(m)
        nat_ar[(k[0], len(k[1]))].append(m)
        nat_nm[k[0]].append(m)

    allrows = in_scope_rows(repo)
    need = [r["name"] for r in allrows if not r["dem"] and r["name"].startswith("?")]
    und = batch_demangle("llvm-undname", need)
    q_ct, qa_ct = collections.Counter(), collections.Counter()
    for r in allrows:            # overload uniqueness is judged over ALL rows, 100% ones too
        r["key"] = parse_msvc(r["dem"] or und.get(r["name"], ""), r["name"])
        if r["key"]:
            q_ct[r["key"][0]] += 1
            qa_ct[(r["key"][0], len(r["key"][1]))] += 1
    rows = [r for r in allrows if r["fuzzy"] < 100]

    def agg(ms):
        c, t = 0, collections.Counter()
        for m in ms:
            c += counts[m]["count"]
            t.update(counts[m]["targets"])
        return c, t, counts[ms[0]]["file"]

    for r in rows:
        k = r["key"]
        r["tier"], r["count"], r["targets"], r["native_file"] = "none", 0, {}, ""
        if not k:
            r["tier"] = "anon" if r["name"].startswith(("fn_", "lbl_")) else "unparsed"
            continue
        hit = None
        qa = (k[0], len(k[1]))
        if k in nat:
            hit, r["tier"] = nat[k], "sig"
        elif qa in nat_ar and qa_ct[qa] == 1 and \
                len({parse_itanium(idem[m], m) for m in nat_ar[qa]}) == 1:
            hit, r["tier"] = nat_ar[qa], "arity"
        elif k[0] in nat_nm and q_ct[k[0]] == 1 and \
                len({parse_itanium(idem[m], m) for m in nat_nm[k[0]]}) == 1:
            hit, r["tier"] = nat_nm[k[0]], "name"
        if hit:
            r["count"], tg, r["native_file"] = agg(hit)
            r["targets"] = dict(tg)

    # ------------------------------------------------------ self-checks ----
    sc = [r for r in rows if r["name"] == SELFCHECK_MSVC]
    symkey = parse_msvc("public: __cdecl Symbol::Symbol(char const *)", SELFCHECK_MSVC)
    if symkey not in nat or agg(nat[symkey])[0] == 0:
        fails.append("Symbol::Symbol(char const*) not counted (native key %s)" % (symkey,))
    if sc and sc[0]["tier"] != "sig":
        fails.append("Symbol::Symbol row did not join on the sig tier")
    ex = [r for r in rows if r["count"] > 0]
    if not ex:
        fails.append("no in-scope row executed -- the join is vacuous")
    if len(ex) == len(rows):
        fails.append("every in-scope row executed -- the filter is vacuous")

    rows.sort(key=lambda r: (-r["count"], -r["size"] * (100 - r["fuzzy"])))
    tiers = collections.Counter(r["tier"] for r in rows)
    exb = sum(r["size"] for r in ex)
    print("in-scope sub-100 rows: %d (%d B); joined: %s; executed: %d rows / %d B"
          % (len(rows), sum(r["size"] for r in rows), dict(tiers), len(ex), exb))
    hdr = "count\ttargets\tfuzzy\tmpn\tsize\ttier\tname\tsrc\tdemangled"
    lines = [hdr]
    for r in rows:
        lines.append("%d\t%s\t%.2f\t%.2f\t%d\t%s\t%s\t%s\t%s" % (
            r["count"], ",".join(sorted(r["targets"])) or "-", r["fuzzy"], r["mpn"], r["size"],
            r["tier"], r["name"], r["src"], r["dem"]))
    if a.out:
        open(a.out, "w").write("\n".join(lines) + "\n")
    for ln in lines[:a.top + 1]:
        print(ln[:220])
    if a.json:
        json.dump({"status": status, "rows": [{k: v for k, v in r.items() if k != "key"} for r in rows]},
                  open(a.json, "w"), indent=1)
    if fails:
        for f in fails:
            print("SELF-CHECK FAIL:", f, file=sys.stderr)
        sys.exit(3)
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
