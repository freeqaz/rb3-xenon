#!/usr/bin/env python3
"""Native link-stub census (lane W16-PD, 2026-10-03).

For every symbol a native stub/shim TU (native/src/*stubs*, *_link*, glue,
support, xdk_shims, ...) DEFINES, per target, classify it against the real
objects linked into that target and across all targets. The instrument is `nm`
over the objects on each target's link edge as parsed from
native/build/build.ninja -- not from CMakeLists text.

Per row: overridden_in_target (a non-stub object defines it strongly, so the
stub copy is dead there), referenced_by_real / referenced_by_stub (an UNDEF in
another object), real_in_targets (which targets compile a src/ object that
defines it).

KNOWN BLIND SPOT, measured: references from INSIDE the stub's own object are
resolved by the assembler and never appear as UNDEF, so "unreferenced" is only
a candidate list -- TheNetMessageFactory read as unreferenced and was used by an
inline StaticByteCode() in the same TU. Always delete, then re-link all targets.

Reachability is a separate instrument: -DRB3_STUB_PROBE=ON for the C++ stub TUs
(native/src/cc5_stub_probe.c), and one-shot gdb breakpoints for the .s stubs.

Every native/src object on a link edge must be classified as a stub (STUB_RE)
or a non-stub driver/phase (NON_STUB_RE); an unclassified one exits 3, so a
new stub file cannot be silently left out of the census (W16-TF).

Usage: native_stub_census.py <native_build_dir> <out.json> [--authored]
  --authored: keep only symbols the stub TU authors (all .s weak stubs; strong
  T/D/B/R from .cpp stubs, which drops header-inline COMDATs; plus the
  deliberately-weak native_undecomp_stubs.cpp), and print a per-file summary.
"""
import json, os, re, subprocess, sys
from collections import defaultdict

BD = sys.argv[1]
OUT = sys.argv[2]
NINJA = os.path.join(BD, "build.ninja")

STUB_RE = re.compile(
    r"/src/(dta_link_stubs\.s|m1_link_stubs\.s|m2_link_stubs\.s|m3_link_stubs\.s|"
    r"m3b_link_stubs\.s|m4_save_link_stubs\.cpp|m8_link_stubs\.cpp|"
    r"m10_leaf_stubs\.cpp|m12_link_stubs\.cpp|milo_link_stubs\.cpp|"
    r"native_undecomp_stubs\.cpp|native_job_stubs\.cpp|thunk_stubs\.cpp|x7_band_stubs\.cpp|"
    r"x20_bandpatchmesh_link\.cpp|native_link_glue\.cpp|xdk_shims\.cpp|"
    r"m1_symbols\.cpp|m3_symbols\.cpp|m6_symbols\.cpp|m8_support\.cpp|m10_support\.cpp|"
    r"beatmatch_native_support\.cpp|rb3_render_glue\.cpp|milo_object_factories\.cpp|"
    r"bandtrack_link_stubs\.cpp|w16sh_link_support\.cpp|"
    # W16-UR: W16-TR's XboxEnumeration/XboxPurchaser aborting stubs and W16-TS's phase link edge.
    r"w16tr_link_stubs\.cpp|w16ts_link_support\.cpp|"
    # A0: rb3-app's own stubs, each tagged `// ring: <tag>` (read by ring_map below).
    r"rb3_app_link_stubs\.cpp|rb3_app_link_stubs\.s|"
    # A1: rb3-app's real native substitutes for src/ globals and the Quazal data
    # slice; each definition tagged `// ring: native-real` (or its online ring).
    r"rb3_app_native_glue\.cpp|rb3_app_quazal_native\.cpp|"
    # profpic: rb3-app's native D3D texture calls backed by engine RndTex objects,
    # each tagged `// ring: native-real`.
    r"rb3_app_d3d_native\.cpp)\.o$"
)

# native/src objects that are linked but are NOT stubs: drivers and runtime
# phases (their symbols are test code, not substitutes for src/ code) and the
# reachability probe. Every other native/src object a target links must match
# STUB_RE, or the run refuses (exit 3). W16-TF: bandtrack_link_stubs.cpp (26
# definitions) and w16sh_link_support.cpp (10) were linked by main for a whole
# round while STUB_RE did not list them, and the census read clean.
NON_STUB_RE = re.compile(
    r"/src/(main_[a-z0-9]+\.cpp|bandtrack_phase\.cpp|w16sh_phase\.cpp|"
    r"w16tf_phase\.cpp|w16tj_phase\.cpp|score_engine\.cpp|cc5_stub_probe\.c|"
    # W16-UR: the gate phases W16-TM .. W16-UJ added to rb3-render (drivers, not stubs).
    r"w16t[mrswy]_phase\.cpp|w16u[bfj]_phase\.cpp|"
    # A8: rb3-milo's cascade-notification gates (test code, not a stub).
    r"cascade_notify_phase\.cpp|async_unload_phase\.cpp|"
    # A9: rb3-milo's proxy-chain gate (test code, not a stub).
    r"proxy_chain_phase\.cpp|"
    # charclip: rb3-milo's CharClip transition ref-count gates (test code, not a stub).
    r"charclip_refs_phase\.cpp|"
    # A11: rb3-milo's Rnd::Mode table gate (test code, not a stub).
    r"rnd_mode_phase\.cpp|"
    # A13: rb3-milo's Spotlight::Generate gate (test code, not a stub).
    r"spotlight_generate_phase\.cpp|"
    # A15: rb3-milo's RndText::UpdateLineColor gate (test code, not a stub).
    r"text_line_color_phase\.cpp|"
    # scripttask: rb3-milo's ScriptTask watched-object gates (test code, not a stub).
    r"script_task_watch_phase\.cpp|"
    # lightpreset: rb3-milo's LightPreset held-object ref gates (test code, not a stub).
    r"lightpreset_refs_phase\.cpp|"
    # A1: rb3-app's boot-gate --wrap shims (call the real step, then print).
    r"rb3_app_gates\.cpp|"
    # A14: rb3-app's joypad layer (retail Joypad_Xbox.cpp's lifecycle over the
    # engine's scripted input; real bodies, nothing aborts) and its gate (test code).
    r"rb3_app_joypad\.cpp|rb3_app_joypad_gate\.cpp)\.o$"
)
NATIVE_SRC_RE = re.compile(r"CMakeFiles/[^/]+\.dir/src/[^/]+\.o$")

targets = {}
with open(NINJA) as f:
    for line in f:
        m = re.match(r"^build (rb3-[a-z0-9]+): CXX_EXECUTABLE_LINKER__\S+ (.*)$", line)
        if not m:
            continue
        rest = m.group(2)
        explicit = rest.split(" || ")[0].split(" | ")[0].split()
        implicit = rest.split(" || ")[0].split(" | ")[1].split() if " | " in rest.split(" || ")[0] else []
        libs = [x for x in implicit if x.endswith(".a")]
        targets[m.group(1)] = (explicit, libs)

# Refuse on an unclassified native/src object (the STUB_RE drift guard).
unclassified = sorted({o for objs, _ in targets.values() for o in objs
                       if NATIVE_SRC_RE.search(o) and not STUB_RE.search(o)
                       and not NON_STUB_RE.search(o)})
if not targets:
    print("native_stub_census: no link edges parsed from %s" % NINJA, file=sys.stderr)
    sys.exit(2)
if unclassified:
    print("native_stub_census: REFUSED -- native/src object(s) linked by a target are in neither "
          "STUB_RE nor NON_STUB_RE; classify them first:", file=sys.stderr)
    for o in unclassified:
        print("  " + o, file=sys.stderr)
    sys.exit(3)

nm_cache = {}
def nm(path):
    if path in nm_cache:
        return nm_cache[path]
    p = os.path.join(BD, path)
    defs, undefs = {}, set()
    if not os.path.exists(p):
        nm_cache[path] = None
        return None
    r = subprocess.run(["nm", "-A", p], capture_output=True, text=True)
    for ln in r.stdout.splitlines():
        toks = ln.split()
        if len(toks) < 3:
            continue
        t, s = toks[-2], toks[-1]
        if t == "U":
            undefs.add(s)
        elif t in "TDBRWVGSi":
            defs[s] = t
    nm_cache[path] = (defs, undefs)
    return nm_cache[path]

def is_stub(o):
    return bool(STUB_RE.search(o))

def stubname(o):
    return STUB_RE.search(o).group(1)

# global strong defs from REPO src/ objects (non-native/src)
global_real = defaultdict(set)   # sym -> {target}
lib_defs = defaultdict(set)
for t, (objs, libs) in targets.items():
    for o in objs:
        if is_stub(o) or "/src/" not in o:
            continue
        if "CMakeFiles/%s.dir/src/" % t in o:   # native/src non-stub (main_*.cpp etc.)
            continue
        r = nm(o)
        if not r:
            continue
        for s, ty in r[0].items():
            if ty not in "Wv":
                global_real[s].add(t)
    for a in libs:
        r = nm(a)
        if not r:
            continue
        for s, ty in r[0].items():
            if ty not in "Wv":
                lib_defs[s].add(a)

rows = []
for t, (objs, libs) in targets.items():
    stub_objs = [o for o in objs if is_stub(o)]
    real_objs = [o for o in objs if not is_stub(o)]
    tdefs_strong = defaultdict(list)
    tundef_real = set()
    for o in real_objs:
        r = nm(o)
        if not r:
            continue
        for s, ty in r[0].items():
            tdefs_strong[s].append((o, ty))
        tundef_real |= r[1]
    for a in libs:
        r = nm(a)
        if r:
            for s, ty in r[0].items():
                tdefs_strong[s].append((a, ty))
            tundef_real |= r[1]
    stub_defs = defaultdict(list)
    stub_undef = set()
    for o in stub_objs:
        r = nm(o)
        if not r:
            continue
        for s, ty in r[0].items():
            stub_defs[s].append((stubname(o), ty))
        stub_undef |= r[1]
    for o in stub_objs:
        r = nm(o)
        if not r:
            continue
        for s, ty in r[0].items():
            other = [x for x in tdefs_strong.get(s, []) if x[1] not in "Wv"]
            rows.append(dict(
                target=t, stub=stubname(o), sym=s, type=ty,
                overridden_in_target=[x[0] for x in other],
                also_in_other_stub=[x[0] for x in stub_defs[s] if x[0] != stubname(o)],
                referenced_by_real=s in tundef_real,
                referenced_by_stub=s in stub_undef,
                real_in_targets=sorted(global_real.get(s, [])),
                in_libs=sorted(lib_defs.get(s, [])),
            ))

# ---- RING TAGS (A0, 2026-11-22) ---------------------------------------------
# A stub source may tag each definition with a trailing `// ring: <tag>` (the
# out-of-scope ring it stands in for), and a whole class with a line
# `// ring-class: <Class> <tag>` (covers its vtable, typeinfo and members).
# The tag is matched to the nm row by demangled qualified name. Every row gets a
# `ring` field (None = untagged). --rings <target> lists that target's authored
# stub rows with their tags and exits 4 if a row of a ring-tagged stub file
# (one that carries any tag at all) is untagged.
SRC_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "native", "src")
_ring_cache = {}
def ring_map(stub):
    if stub in _ring_cache:
        return _ring_cache[stub]
    names, classes = {}, {}
    path = os.path.join(SRC_DIR, stub)
    if os.path.exists(path) and stub.endswith(".s"):
        # generated stubs: `<mangled>: // ring: <tag>` on the label line
        for line in open(path, errors="replace"):
            m = re.match(r"^(\S+):\s*//\s*ring:\s*(\S+)", line)
            if m:
                names[m.group(1)] = m.group(2)
    elif os.path.exists(path) and stub.endswith(".cpp"):
        for line in open(path, errors="replace"):
            m = re.match(r"\s*//\s*ring-class:\s*(\S+)\s+(\S+)", line)
            if m:
                classes[m.group(1)] = m.group(2)
                continue
            m = re.search(r"//\s*ring:\s*(\S+)", line)
            if not m:
                continue
            code = line[:m.start()]
            a = re.search(r'__asm__\("([^"]+)"\)', code)
            if a:
                names[a.group(1)] = m.group(1)
                continue
            f = re.search(r"((?:[A-Za-z_]\w*::)*~?[A-Za-z_]\w*)\s*\(", code)
            if f and f.group(1) not in ("alignas", "sizeof", "decltype"):
                names[f.group(1)] = m.group(1)
                continue
            d = re.search(r"((?:[A-Za-z_]\w*::)*[A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*(?:=[^;]*)?;", code)
            if d:
                names[d.group(1)] = m.group(1)
    _ring_cache[stub] = (names, classes)
    return _ring_cache[stub]

_dm = {}
def demangle_all(syms):
    todo = sorted(set(syms) - set(_dm))
    if todo:
        r = subprocess.run(["c++filt"], input="\n".join(todo), capture_output=True, text=True)
        for a, b in zip(todo, r.stdout.splitlines()):
            _dm[a] = b

def ring_of(stub, sym):
    names, classes = ring_map(stub)
    if not names and not classes:
        return None
    if sym in names:
        return names[sym]
    d = _dm.get(sym, sym)
    for pre in ("vtable for ", "typeinfo for ", "typeinfo name for "):
        if d.startswith(pre):
            return classes.get(d[len(pre):])
    key = d.split("(")[0].strip()
    if key in names:
        return names[key]
    cls = key.rsplit("::", 1)[0] if "::" in key else None
    while cls:
        if cls in classes:
            return classes[cls]
        cls = cls.rsplit("::", 1)[0] if "::" in cls else None
    return None

demangle_all([r["sym"] for r in rows])
for r in rows:
    r["ring"] = ring_of(r["stub"], r["sym"])

json.dump(dict(targets={k: dict(objs=len(v[0]), libs=v[1]) for k, v in targets.items()},
               rows=rows), open(OUT, "w"), indent=1)
if "--authored" in sys.argv or "--rings" in sys.argv:
    # native_undecomp_stubs.cpp defines every stub __attribute__((weak)) on
    # purpose (the real src/ TU wins where it links), so its W defs are authored,
    # not header-inline COMDATs. Dropping them undercounts the inventory by 3.
    EXPLICIT_WEAK = {"native_undecomp_stubs.cpp"}
    rows = [r for r in rows if r["stub"].endswith(".s") or r["type"] in "TDBRGS"
            or (r["stub"] in EXPLICIT_WEAK and r["type"] in "Wv")]
    json.dump(dict(targets={k: dict(objs=len(v[0]), libs=v[1]) for k, v in targets.items()},
                   rows=rows), open(OUT, "w"), indent=1)
    by = defaultdict(set)
    for r in rows:
        by[r["stub"]].add(r["sym"])
    for k in sorted(by):
        print("%6d  %s" % (len(by[k]), k))
if "--rings" in sys.argv:
    want = sys.argv[sys.argv.index("--rings") + 1]
    mine = [r for r in rows if r["target"] == want]
    untagged = []
    print("--- %s: authored stub rows with their ring ---" % want)
    for r in sorted(mine, key=lambda r: (r["stub"], r["ring"] or "", _dm.get(r["sym"], r["sym"]))):
        tagged_file = bool(ring_map(r["stub"])[0] or ring_map(r["stub"])[1])
        print("  %-24s %-22s %s" % (r["stub"], r["ring"] or "-", _dm.get(r["sym"], r["sym"])))
        if tagged_file and not r["ring"]:
            untagged.append(r)
    rc = defaultdict(int)
    for r in mine:
        rc[(r["stub"], r["ring"] or "-")] += 1
    for (st, rg), n in sorted(rc.items()):
        print("%6d  %-24s ring=%s" % (n, st, rg))
    print("%s: %d authored stub row(s); %d untagged in a ring-tagged file" % (want, len(mine), len(untagged)))
    if untagged:
        sys.exit(4)
print("targets", len(targets), "rows", len(rows))
