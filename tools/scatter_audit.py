#!/usr/bin/env python3
"""scatter_audit.py -- map the decomp's scatter-include graph against every
native target's ACTUAL compiled source set, and report both failure directions.

WHY
---
The X360 decomp packs multiple original TUs into one by having a .cpp
`#include` another .cpp ("scatter includes", ~250 of them). That packing exists
purely to reproduce retail COMDAT placement for objdiff SCORING. The X360 build
never links, so a scatter edge is structurally invisible there.

The NATIVE build is the only place it surfaces, and it can fail two ways:

  DIRECTION A (missing)  a target needs symbols from file G, but G is neither
                         compiled standalone in that target nor emitted by any
                         scatter host the target compiles => undefined refs.

  DIRECTION B (extra)    a target deliberately EXCLUDES file G (platform filter,
                         explicit exclude list, or a module it does not build),
                         but a scatter host it does compile drags G in anyway
                         => the target silently links code it excluded. Silent,
                         and much harder to notice than direction A.

METHOD
------
Ground truth for "what does target T compile" is the ninja graph, not the
CMakeLists (the CMakeLists list is pre-prune). We read build.ninja's link edge
for each target, map each .o back to its source, and that is S_T.

The scatter closure E_T is then computed with the SAME rules
native/cmake/ScatterIncludes.cmake uses -- unconditional edges only, quoted
include resolved includer-dir-first then the include roots -- so this
instrument and the build agree by construction. A disagreement between them is
itself a finding, and is reported.
"""

import json
import os
import re
import sys
from collections import defaultdict

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
NATIVE = os.path.join(REPO, "native")
BUILD = os.path.join(NATIVE, "build")

ROOTS = [
    os.path.join(REPO, "src"),
    os.path.join(REPO, "src", "system"),
    os.path.join(REPO, "src", "band3"),
    os.path.join(REPO, "src", "network"),
    os.path.join(NATIVE, "src"),
]

# Mirrors the MILO_FORK_SOURCES filters in native/CMakeLists.txt.
PLATFORM_EXCLUDE = re.compile(r"_(Xbox|Win|Wii|X360)\.cpp$")
GFX_EXCLUDE = re.compile(r"(Dx9|DX9|Wgpu|D3D|Xenon)")

DIRECTIVE = re.compile(
    r'#[ \t]*(if[a-z]*[^\n]*|endif|include[ \t]*"[^"]*\.cpp")'
)
INC = re.compile(r'#[ \t]*include[ \t]*"([^"]+)"')

_scan_cache = {}


def scan(path):
    """Return (unconditional, conditional, hx_conditional) includee lists.

    Byte-for-byte the same state machine as _rb3_scatter_scan in
    native/cmake/ScatterIncludes.cmake: a stack of open #if blocks, each
    tagged with whether its condition mentions HX_NATIVE.
    """
    if path in _scan_cache:
        return _scan_cache[path]
    try:
        with open(path, "r", errors="replace") as fh:
            content = fh.read()
    except OSError:
        r = ([], [], [])
        _scan_cache[path] = r
        return r

    stack = []
    uncond, cond, hx = [], [], []
    for d in DIRECTIVE.findall(content):
        d = "#" + d
        if re.match(r"#[ \t]*if", d):
            stack.append(1 if "HX_NATIVE" in d else 0)
        elif re.match(r"#[ \t]*endif", d):
            if stack:
                stack.pop()
        else:
            m = INC.search(d)
            if not m:
                continue
            inc = m.group(1)
            if not stack:
                uncond.append(inc)
            else:
                cond.append(inc)
                if 1 in stack:
                    hx.append(inc)
    r = (uncond, cond, hx)
    _scan_cache[path] = r
    return r


def resolve(inc, includer):
    for root in [os.path.dirname(includer)] + ROOTS:
        cand = os.path.join(root, inc)
        if os.path.exists(cand):
            return os.path.abspath(cand)
    return None


def closure(sources):
    """Transitive closure over UNCONDITIONAL scatter edges from `sources`.

    Returns (reached, edges) where reached is the set of files emitted by some
    TU in `sources` and edges is the list of (host, guest) pairs traversed.
    """
    queue = list(sources)
    seen, reached, edges = set(), set(), []
    while queue:
        cur = queue.pop(0)
        if cur in seen:
            continue
        seen.add(cur)
        uncond, _, _ = scan(cur)
        for inc in uncond:
            res = resolve(inc, cur)
            if res:
                edges.append((cur, res))
                reached.add(res)
                queue.append(res)
    return reached, edges


def target_sources():
    """S_T per target, read from the ninja graph (post-prune ground truth)."""
    bn = os.path.join(BUILD, "build.ninja")
    with open(bn, errors="replace") as fh:
        text = fh.read()

    # object -> source, from each compile edge's DEP_FILE/source comment.
    obj2src = {}
    for m in re.finditer(
        r"^build ([^:\n]+?\.o): CXX_COMPILER__\S+ (\S+)", text, re.M
    ):
        obj2src[m.group(1).strip()] = m.group(2).strip()

    targets = {}
    for m in re.finditer(
        r"^build ([^:\n]+?): CXX_EXECUTABLE_LINKER__(\S+?)_ (.+?)(?=\n[a-zA-Z])",
        text,
        re.M | re.S,
    ):
        name = m.group(2)
        srcs = set()
        for tok in m.group(3).split():
            tok = tok.strip()
            if tok in obj2src:
                s = obj2src[tok]
                if not os.path.isabs(s):
                    s = os.path.normpath(os.path.join(BUILD, s))
                srcs.add(os.path.abspath(s))
        if srcs:
            targets[name] = srcs
    return targets


def rel(p):
    try:
        return os.path.relpath(p, REPO)
    except ValueError:
        return p


def hosts_map():
    """guest -> [(host, kind)] over the WHOLE tree, kind in uncond/cond/hx."""
    all_cpp = []
    for base in (os.path.join(REPO, "src"), os.path.join(NATIVE, "src")):
        for dirpath, _, files in os.walk(base):
            for f in files:
                if f.endswith(".cpp"):
                    all_cpp.append(os.path.abspath(os.path.join(dirpath, f)))

    hosts = defaultdict(list)
    for f in all_cpp:
        u, c, h = scan(f)
        for inc in u:
            r = resolve(inc, f)
            if r:
                hosts[r].append((f, "uncond"))
        for inc in c:
            r = resolve(inc, f)
            if r:
                hosts[r].append((f, "hx" if inc in h else "cond"))
    return hosts


def main():
    targets = target_sources()
    if not targets:
        sys.exit("no targets parsed from build.ninja -- configure first")

    hosts = hosts_map()

    report = {"targets": {}, "direction_a": [], "direction_b": [],
              "multi_host": [], "graph": {}}

    for g, hs in sorted(hosts.items()):
        report["graph"][rel(g)] = [[rel(a), k] for a, k in hs]
        uh = [a for a, k in hs if k == "uncond"]
        if len(uh) > 1:
            report["multi_host"].append(
                {"guest": rel(g), "unconditional_hosts": [rel(x) for x in uh]}
            )

    for name, S in sorted(targets.items()):
        E, edges = closure(S)
        emitted_not_listed = sorted(E - S)

        # DIRECTION B: emitted although the target's own filters exclude it.
        b = []
        for f in emitted_not_listed:
            reasons = []
            if PLATFORM_EXCLUDE.search(f):
                reasons.append("platform-filtered (_Xbox/_Win/_Wii/_X360)")
            if GFX_EXCLUDE.search(f):
                reasons.append("gfx-filtered (Dx9/Wgpu/D3D/Xenon)")
            if reasons:
                via = [rel(h) for h, gg in edges if gg == f]
                b.append({"target": name, "file": rel(f),
                          "reasons": reasons, "via": via})
        report["direction_b"].extend(b)

        # DIRECTION A: a file that is scatter-hosted ONLY by hosts this target
        # does not compile => its symbols are absent from this target.
        a = []
        for g, hs in hosts.items():
            uh = [x for x, k in hs if k == "uncond"]
            if not uh:
                continue
            if g in S or g in E:
                continue
            if any(x in S or x in E for x in uh):
                continue
            a.append({"target": name, "file": rel(g),
                      "only_hosts": [rel(x) for x in uh]})
        report["direction_a"].extend(a)

        report["targets"][name] = {
            "compiled": len(S),
            "emitted_via_scatter": len(emitted_not_listed),
            "emitted_files": [rel(x) for x in emitted_not_listed],
        }

    print(json.dumps(report, indent=1))


if __name__ == "__main__":
    main()
