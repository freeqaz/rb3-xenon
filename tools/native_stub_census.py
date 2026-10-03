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
    r"m3b_link_stubs\.s|m4_save_link_stubs\.cpp|m6_link_stubs\.cpp|m8_link_stubs\.cpp|"
    r"m10_link_stubs\.cpp|m10_leaf_stubs\.cpp|m12_link_stubs\.cpp|milo_link_stubs\.cpp|"
    r"native_undecomp_stubs\.cpp|native_job_stubs\.cpp|thunk_stubs\.cpp|x7_band_stubs\.cpp|"
    r"x20_bandpatchmesh_link\.cpp|native_link_glue\.cpp|xdk_shims\.cpp|"
    r"m1_symbols\.cpp|m3_symbols\.cpp|m6_symbols\.cpp|m8_support\.cpp|m10_support\.cpp|"
    r"beatmatch_native_support\.cpp|rb3_render_glue\.cpp|milo_object_factories\.cpp)\.o$"
)

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

json.dump(dict(targets={k: dict(objs=len(v[0]), libs=v[1]) for k, v in targets.items()},
               rows=rows), open(OUT, "w"), indent=1)
if "--authored" in sys.argv:
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
print("targets", len(targets), "rows", len(rows))
