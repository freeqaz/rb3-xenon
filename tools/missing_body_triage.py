#!/usr/bin/env python3
"""Triage the "missing bodies" vein: anonymous 0% rows inside PAIRABLE units.

WHAT THIS CLASS IS, AND WHY NAMING CANNOT CLOSE IT
--------------------------------------------------
objdiff pairs a target row to a base row by NAME, and *failing that*, by
reloc-masked byte signature -- and `is_funclet_like()` (see
`scripts/analysis/coffx.py`, a faithful port) accepts any `fn_<8hex>` symbol.
So an anonymous target row already participates in byte-signature pairing:

    if our object produced byte-identical code for it, it would ALREADY be
    matched, with no name.

=> an anonymous row still sitting at fuzzy 0 is one whose BYTES WE DO NOT
REPRODUCE.  Naming it cannot cross it; only a body can.  That is the whole
reason this class is called "missing bodies" and the reason identification is
structurally incapable of being a byte lever here (roadmap section 3).

WHAT THIS TOOL ANSWERS
----------------------
Per pairable unit and in aggregate:
  * the class, RE-DERIVED from the report.json on disk (never inherited),
  * the OWNER of the unit (XDK / QUAZAL / HMX_ENGINE / HMX_GAME / OTHER),
  * ORACLE BACKING -- the load-bearing triple
        "retail owns N, ours defines M, oracle defines K"
    plus the sharper signal it took the counts to reach: the set of function
    names the ORACLE defines that OUR file does not (`oracle_only`).  A count
    gap says "something is missing"; a name set says WHAT, and is what a
    porting lane would actually work from.

WHAT IT DELIBERATELY EXCLUDES, AND WHY
--------------------------------------
  * `auto_*` units.  A DIFFERENT population -- unpinned address ranges with no
    base object at all.  They can never pair regardless of source quality, so
    they are not a body problem.  Counted and printed, never mixed in.
  * units with no compiled base obj (the 230 "no source" units).  Already
    declared in objects.json with a src_path that does not exist; also not
    this class.  Counted and printed separately.
  * Spatial proximity is NOT used as a membership test anywhere.  "Enclosed by
    the same heading on both sides" was measured at 33.76% FP (lane AUTOID-1).
    Membership here is objdiff's own unit attribution, nothing else.

MEASUREMENT HYGIENE THIS TOOL IS BOUND BY
-----------------------------------------
  * FRESHNESS: refuses via `scripts/analysis/freshness.py` unless report.json
    and the objects on disk are known to correspond.
  * report.json is protobuf-JSON: defaults are OMITTED and numerics are
    sometimes JSON STRINGS.  Every numeric is read as `int(x.get(k, 0))` /
    `float(...)`; a bare `+` on an un-coerced value silently CONCATENATES.
  * FULL PATHS ONLY.  `Movie.cpp` collides between `rnddx9/` and `rndobj/`,
    and four lanes have broken on `basename()`.  Unit->source comes from
    objdiff.json, which `tools/project.py` generates from `config.objects()`;
    the mapping is CROSS-VALIDATED against a live `objects()` resolution and
    disagreements are printed, not swallowed.
  * ORACLE ROOTS are resolved from the REAL repository (via
    `git rev-parse --git-common-dir`), never as a sibling of the source tree.
    A worktree under ~/tmp has no `../dc3-decomp`, and that failure is shaped
    like a legitimate "oracle absent" rather than an error -- the same bug
    class as the native build's `MILO_ENGINE_PATH` and `pin_from_symnames`'s
    DC3-map path.

KNOWN BIAS IN THE FUNCTION-DEFINITION COUNTER (state it, do not hide it)
-----------------------------------------------------------------------
`count_fn_defs` is a brace/signature heuristic, not a C++ parser.  It counts a
`{` whose preceding text ends a parameter list, skips the body so nested
lambdas are not double counted, and drops preprocessor lines wholesale -- so
bodies inside `#if` blocks are counted regardless of which branch would
compile, and macro-generated definitions are missed entirely.

The bias is real, and the design answer is that it is applied IDENTICALLY to
our tree and to the oracle, so `M` vs `K` is like-for-like even where both are
biased.  `N` (the retail target obj's code symbols) comes from the COFF symbol
table and is exact -- but it is NOT comparable to M or K, because a retail TU
owns funclets, thunks and template COMDAT instantiations that no source file
"defines" as a line of text.  Compare N against M only as an ORDER OF
MAGNITUDE (retail 67 vs ours 0 is a scaffold; retail 134 vs ours 103 is not).

    --selftest    proves each guard can fail by mutating a fixture.
    --self-break  disables one guard and REQUIRES the selftest to go red.
"""

from __future__ import annotations

import argparse
import collections
import json
import os
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
DEFAULT_REPORT = "build/45410914/report.json"
DEFAULT_OBJDIFF = "objdiff.json"

# objdiff's placeholder-symbol prefixes (objdiff-core diff/code.rs
# `is_placeholder_symbol_name`).  A row carrying one of these is anonymous:
# the map has no identification for it.
PLACEHOLDER = re.compile(r"^(fn|lbl|jumptable|data|bss|rdata)_")

OWNER_XDK = "XDK"
OWNER_QUAZAL = "QUAZAL"
OWNER_ENGINE = "HMX_ENGINE"
OWNER_GAME = "HMX_GAME"
OWNER_OTHER = "OTHER"
OWNERS = (OWNER_XDK, OWNER_QUAZAL, OWNER_ENGINE, OWNER_GAME, OWNER_OTHER)

# A source file this short cannot hold a translation unit's worth of bodies.
# 103 of 117 `src/network/**` sources are under this, median 7 (lane
# AUTOID-1) -- they are `namespace Quazal {}` map scaffolds.
SCAFFOLD_LINES = 20


# --------------------------------------------------------------------------
# C++ source: strip noise, count/name function definitions
# --------------------------------------------------------------------------

def strip_noise(src: str) -> str:
    """Remove comments, string/char literals and preprocessor lines."""
    out = []
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c == "/" and i + 1 < n and src[i + 1] == "/":
            j = src.find("\n", i)
            i = n if j < 0 else j
        elif c == "/" and i + 1 < n and src[i + 1] == "*":
            j = src.find("*/", i + 2)
            i = n if j < 0 else j + 2
            out.append(" ")
        elif c in "\"'":
            q = c
            i += 1
            while i < n:
                if src[i] == "\\":
                    i += 2
                    continue
                if src[i] == q:
                    i += 1
                    break
                if src[i] == "\n" and q == '"':
                    break
                i += 1
            out.append('""')
        else:
            out.append(c)
            i += 1
    txt = "".join(out)
    return _strip_preprocessor(txt)


def _strip_preprocessor(txt: str) -> str:
    """Delete preprocessor LOGICAL lines, following backslash continuations.

    ⛔ Removing only the first PHYSICAL line is a real, measured defect: our
    `src/system/midi/MidiParser.cpp` carries a multi-line
    `#define BEGIN_HANDLERS(objType)` whose continuation lines contain
    `DataNode objType::Handle(...) {` -- an UNBALANCED opening brace.  The
    body-skip then ran to EOF and the file counted 3 definitions instead of
    ~60, which silently inflates `oracle_only` for every macro-heavy Milo TU.
    """
    lines = txt.split("\n")
    out, i, n = [], 0, len(lines)
    while i < n:
        if lines[i].lstrip().startswith("#"):
            while i < n and lines[i].rstrip().endswith("\\"):
                out.append("")
                i += 1
            if i < n:
                out.append("")
                i += 1
            continue
        out.append(lines[i])
        i += 1
    return "\n".join(out)


_SIG_END = re.compile(
    r"\)\s*(?:const\s*)?(?:volatile\s*)?(?:throw\s*\(\s*\)\s*)?"
    r"(?:noexcept\s*(?:\([^()]*\))?\s*)?(?:override\s*)?(?:final\s*)?$")
_CTOR_INIT = re.compile(r"\)\s*:\s*[^;{}]*$")
_CTRL = re.compile(
    r"\b(if|for|while|switch|catch|else|do|return|new|delete|sizeof|throw)\s*$")
# Extract the DEFINED NAME from the text preceding the parameter list.  It is
# the trailing qualified identifier -- NOT a greedy run of name-ish characters,
# which swallows the return type (`const char *Bar::Gamma` -> `char*Bar::Gamma`,
# a defect the selftest's set-difference case caught).
def _TMPL_STRIP(s: str) -> str:
    """Collapse template argument lists so `A<B,C>::f` -> `A::f`."""
    prev = None
    while prev != s:
        prev = s
        s = re.sub(r"<[^<>]*>", "", s)
    return s


_NAME_OP = re.compile(
    r"((?:[A-Za-z_~][A-Za-z_0-9]*\s*::\s*)*operator\s*[^\s(]+)\s*$")
_NAME_QUAL = re.compile(
    r"((?:[A-Za-z_~][A-Za-z_0-9]*\s*::\s*)*~?[A-Za-z_][A-Za-z_0-9]*)\s*$")


def count_fn_defs(src: str):
    """-> (count, [names]).  See the module docstring for the known bias."""
    txt = strip_noise(src)
    n = len(txt)
    i = 0
    names = []
    while i < n:
        if txt[i] != "{":
            i += 1
            continue
        head = txt[:i].rstrip()
        m = _CTOR_INIT.search(head)
        if not (m or _SIG_END.search(head)):
            i += 1
            continue
        scan = head[: m.start() + 1] if m else head
        depth, j = 0, len(scan) - 1
        while j >= 0:
            if scan[j] == ")":
                depth += 1
            elif scan[j] == "(":
                depth -= 1
                if depth == 0:
                    break
            j -= 1
        if j < 0:
            i += 1
            continue
        before = scan[:j].rstrip()
        if not before or _CTRL.search(before) or before[-1] in "=,;{}+-*/&|!<>?:":
            i += 1
            continue
        flat = _TMPL_STRIP(before)
        nm = _NAME_OP.search(flat) or _NAME_QUAL.search(flat)
        names.append(re.sub(r"\s+", "", nm.group(1))[-90:] if nm else "?")
        # Skip the body: nested lambdas/blocks must not be counted again.
        d = 0
        while i < n:
            if txt[i] == "{":
                d += 1
            elif txt[i] == "}":
                d -= 1
                if d == 0:
                    break
            i += 1
        i += 1
    return len(names), names


_TMPL = re.compile(r"<[^<>]*>")


def norm_fn_name(name: str) -> str:
    """Normalize a definition name for cross-tree set comparison."""
    s = name.strip()
    prev = None
    while prev != s:                      # collapse nested template args
        prev = s
        s = _TMPL.sub("", s)
    s = s.lstrip("*&")
    s = re.sub(r"^(inline|static|virtual|explicit|const)", "", s)
    return s.lstrip("*&")


# --------------------------------------------------------------------------
# COFF: how many CODE symbols does an object own?
# --------------------------------------------------------------------------

def coff_code_symbols(path: Path):
    """-> [names] of function symbols defined in code sections, or None."""
    if str(REPO) not in sys.path:
        sys.path.insert(0, str(REPO))
    from scripts.analysis import coffx
    try:
        data = path.read_bytes()
    except OSError:
        return None
    secs, syms = coffx.read_coff(data)
    if secs is None:
        return None
    coffx.infer_sizes(secs, syms)
    out = []
    for s in syms:
        if s.sec > 0 and s.sec - 1 < len(secs) and secs[s.sec - 1].is_code \
                and s.kind == coffx.K_FUNC:
            out.append(s.name)
    return out


# --------------------------------------------------------------------------
# Owner classification -- FULL PATHS ONLY
# --------------------------------------------------------------------------

def classify_owner(src_path: str, text) -> str:
    """Owner class of a unit from its FULL source path (never a basename).

    `text` is the source file's contents when readable; it exists so a
    `namespace Quazal` scaffold sitting OUTSIDE src/network/quazal/ is still
    caught, per the brief.
    """
    p = (src_path or "").replace("\\", "/")
    if p.startswith("src/xdk/") or "/xdk/" in p:
        return OWNER_XDK
    if p.startswith("src/network/quazal/"):
        return OWNER_QUAZAL
    if text is not None and re.search(r"\bnamespace\s+Quazal\b", text):
        return OWNER_QUAZAL
    if p.startswith("src/system/"):
        return OWNER_ENGINE
    if p.startswith("src/band3/") or p.startswith("src/network/"):
        return OWNER_GAME
    return OWNER_OTHER


# --------------------------------------------------------------------------
# Oracle roots -- resolved from the REAL repo, never a worktree sibling
# --------------------------------------------------------------------------

def real_repo_root(project_dir: Path) -> Path:
    """The checkout that owns this worktree (a worktree's siblings are wrong)."""
    try:
        out = subprocess.run(["git", "rev-parse", "--git-common-dir"],
                             cwd=str(project_dir), capture_output=True,
                             text=True, timeout=30)
        if out.returncode == 0:
            g = Path(out.stdout.strip())
            if not g.is_absolute():
                g = (Path(project_dir) / g).resolve()
            return g.parent            # <real repo>/.git -> <real repo>
    except (OSError, subprocess.SubprocessError):
        pass
    return Path(project_dir)


ORACLE_WITNESS = {"dc3": ("dc3-decomp", "src/system/rndobj/Text.cpp"),
                  "rb3wii": ("rb3", "src/system/rndobj/Text.cpp")}


def oracle_roots(project_dir: Path) -> dict:
    """{'dc3': Path|None, 'rb3wii': Path|None}, each confirmed by a witness."""
    rr = real_repo_root(project_dir)
    cands = [rr.parent,
             Path(project_dir).resolve().parent,
             Path.home() / "code" / "milohax"]
    found = {"dc3": None, "rb3wii": None}
    for key, (name, wit) in ORACLE_WITNESS.items():
        for base in cands:
            p = base / name
            if (p / wit).exists():
                found[key] = p
                break
    return found


ORACLE_FOR = {OWNER_ENGINE: "dc3", OWNER_GAME: "rb3wii",
              OWNER_QUAZAL: "rb3wii", OWNER_OTHER: "dc3", OWNER_XDK: "dc3"}


# --------------------------------------------------------------------------
# objects.json cross-validation (the anti-basename guard)
# --------------------------------------------------------------------------

def resolve_objects(project_dir: Path):
    """objects.json path_key -> Object, mirroring generate_build()'s aliasing.

    Lifted from tools/noobj_census.py so this tool cannot drift from what the
    build actually resolves.  configure.py rewrites build.ninja at import time;
    this census is read-only, so generate_build is no-op'd first.
    """
    import importlib.util
    cwd = os.getcwd()
    argv = sys.argv[:]
    try:
        os.chdir(str(project_dir))
        sys.argv = ["configure.py"]
        sys.path.insert(0, str(project_dir))
        import tools.project as _project
        _project.generate_build = lambda *a, **kw: None
        spec = importlib.util.spec_from_file_location(
            "cfg_mbt", str(Path(project_dir) / "configure.py"))
        cfg = importlib.util.module_from_spec(spec)
        try:
            spec.loader.exec_module(cfg)
        except SystemExit:
            pass
        return cfg.config.objects()
    finally:
        os.chdir(cwd)
        sys.argv = argv


# --------------------------------------------------------------------------
# The census
# --------------------------------------------------------------------------

class Unit:
    __slots__ = ("name", "src", "owner", "rows", "bytes", "n_retail",
                 "m_ours", "k_oracle", "oracle_path", "oracle_only",
                 "our_lines", "oracle_lines", "total_rows", "total_bytes",
                 "matched_bytes", "scaffold", "base_path", "target_path",
                 "anon", "n_base")


def census(project_dir: Path, report_path: str, objdiff_path: str):
    proj = Path(project_dir).resolve()
    report = json.loads((proj / report_path).read_text())
    objdiff = {u["name"]: u
               for u in json.loads((proj / objdiff_path).read_text())["units"]}

    m = report["measures"]
    whole = {
        "total_code": int(m.get("total_code", 0)),
        "total_functions": int(m.get("total_functions", 0)),
        "matched_code": int(m.get("matched_code", 0)),
        "matched_functions": int(m.get("matched_functions", 0)),
    }

    roots = oracle_roots(proj)
    src_cache = {}

    def read_src(path: Path):
        key = str(path)
        if key not in src_cache:
            try:
                text = path.read_text(encoding="utf-8", errors="replace")
            except OSError:
                src_cache[key] = (None, 0, 0, [])
            else:
                cnt, names = count_fn_defs(text)
                src_cache[key] = (text, len(text.splitlines()), cnt, names)
        return src_cache[key]

    units = []
    excl = collections.defaultdict(lambda: [0, 0, 0])   # units, rows, bytes
    cls_rows = cls_bytes = 0
    seen_rows = seen_bytes = 0
    other = collections.defaultdict(lambda: [0, 0])

    for u in report["units"]:
        od = objdiff.get(u["name"], {})
        meta = od.get("metadata", {}) or {}
        fns = u.get("functions") or []
        nrows = len(fns)
        nbytes = sum(int(f.get("size", 0)) for f in fns)
        seen_rows += nrows
        seen_bytes += nbytes

        pairable = bool(od.get("base_path"))
        auto = bool(meta.get("auto_generated", False))
        if not pairable:
            key = "EXCLUDED_auto_star" if auto else "EXCLUDED_no_base_obj"
            e = excl[key]
            e[0] += 1
            e[1] += nrows
            e[2] += nbytes
            continue

        rows = byts = 0
        anon = []
        for f in fns:
            sz = int(f.get("size", 0))
            fz = float(f.get("fuzzy_match_percent", 0.0))
            ph = bool(PLACEHOLDER.match(f["name"]))
            if ph and fz == 0.0:
                rows += 1
                byts += sz
                anon.append((sz, f["name"]))
            elif fz == 0.0:
                other["pairable_named_0pct"][0] += 1
                other["pairable_named_0pct"][1] += sz
            elif fz < 100.0:
                other["pairable_credited_residual"][0] += 1
                other["pairable_credited_residual"][1] += sz
        cls_rows += rows
        cls_bytes += byts
        if rows == 0:
            continue

        src_rel = meta.get("source_path") or ""
        our_path = proj / src_rel if src_rel else None
        text, our_lines, m_ours, our_names = (
            read_src(our_path) if our_path else (None, 0, 0, []))

        it = Unit()
        it.name = u["name"]
        it.src = src_rel
        it.owner = classify_owner(src_rel, text)
        it.rows, it.bytes = rows, byts
        it.anon = sorted(anon, reverse=True)
        it.total_rows, it.total_bytes = nrows, nbytes
        it.matched_bytes = int(u.get("measures", {}).get("matched_code", 0))
        it.base_path = od.get("base_path")
        it.target_path = od.get("target_path")
        it.our_lines = our_lines
        it.m_ours = m_ours
        it.scaffold = text is not None and (our_lines < SCAFFOLD_LINES
                                            or m_ours == 0)

        tgt = proj / it.target_path if it.target_path else None
        syms = coff_code_symbols(tgt) if tgt else None
        it.n_retail = len(syms) if syms is not None else -1
        bsyms = coff_code_symbols(proj / it.base_path) if it.base_path else None
        it.n_base = len(bsyms) if bsyms is not None else -1

        root = roots.get(ORACLE_FOR.get(it.owner, "dc3"))
        it.oracle_path, it.k_oracle, it.oracle_only, it.oracle_lines = \
            None, -1, [], 0
        if root is not None and src_rel:
            cand = root / src_rel
            if cand.exists():
                _t, olines, k, onames = read_src(cand)
                it.oracle_path = str(cand)
                it.k_oracle = k
                it.oracle_lines = olines
                ours = {norm_fn_name(x) for x in our_names}
                it.oracle_only = sorted(
                    {norm_fn_name(x) for x in onames} - ours)
        units.append(it)

    return {
        "whole": whole, "units": units, "excluded": dict(excl),
        "class_rows": cls_rows, "class_bytes": cls_bytes,
        "seen_rows": seen_rows, "seen_bytes": seen_bytes,
        "other": dict(other), "roots": roots, "report": report,
    }


# --------------------------------------------------------------------------
# Self-validation
# --------------------------------------------------------------------------

def self_validate(c, objects_xcheck=None, break_guard=None):
    """Every check must be capable of failing.  Returns (ok, [lines])."""
    out = []
    ok = True

    def chk(name, cond, detail):
        nonlocal ok
        good = bool(cond)
        if break_guard == name:
            good = True                      # --self-break: neuter this guard
        if not good:
            ok = False
        out.append(f"  [{'ok' if good else 'FAIL'}] {name}: {detail}")

    w = c["whole"]
    chk("row_conservation",
        c["seen_rows"] == w["total_functions"]
        and c["seen_bytes"] == w["total_code"],
        f"{c['seen_rows']} rows / {c['seen_bytes']} B seen vs report "
        f"{w['total_functions']} / {w['total_code']}")

    pu_rows = sum(u.rows for u in c["units"])
    pu_bytes = sum(u.bytes for u in c["units"])
    chk("per_unit_sums_to_whole",
        pu_rows == c["class_rows"] and pu_bytes == c["class_bytes"],
        f"per-unit {pu_rows} rows / {pu_bytes} B == whole-binary "
        f"{c['class_rows']} / {c['class_bytes']}")

    chk("non_vacuous",
        c["class_bytes"] > 100_000 and len(c["units"]) > 50,
        f"{len(c['units'])} units / {c['class_bytes']} B "
        f"(floor: >50 units, >100,000 B)")

    roots = c["roots"]
    chk("oracle_roots_resolved",
        roots.get("dc3") is not None and roots.get("rb3wii") is not None,
        f"dc3={roots.get('dc3')} rb3wii={roots.get('rb3wii')}")

    n_bad = sum(1 for u in c["units"] if u.n_retail < 0)
    chk("target_objs_readable", n_bad == 0,
        f"{n_bad} units whose target obj could not be read")

    if objects_xcheck is not None:
        chk("source_path_matches_objects_json", not objects_xcheck,
            f"{len(objects_xcheck)} unit(s) whose objdiff source_path "
            f"disagrees with config.objects(): {objects_xcheck[:3]}")
    return ok, out


def xcheck_objects(c, project_dir: Path):
    """objdiff.json's source_path must agree with a live config.objects().

    This is the anti-`basename()` guard: the mapping used for OWNER
    classification is compared against the build's own resolution, by FULL
    PATH.  Returns a list of disagreements (empty is good).
    """
    try:
        objects = resolve_objects(Path(project_dir))
    except Exception as exc:                      # noqa: BLE001
        return [f"<objects() unavailable: {type(exc).__name__}: {exc}>"]
    by_src = set()
    for obj in objects.values():
        sp = getattr(obj, "src_path", None)
        if sp:
            by_src.add(str(sp))
    bad = []
    for u in c["units"]:
        if not u.src:
            bad.append(f"{u.name}:<no source_path>")
        elif u.src not in by_src:
            bad.append(f"{u.name}:{u.src}")
    return bad


# --------------------------------------------------------------------------
# Reporting
# --------------------------------------------------------------------------

def fund_class(u):
    """Is this unit's anon-zero slice FUNDABLE under the standing directives?

    XDK is out of scope (porting; pinning is separate).  A Quazal `namespace
    Quazal {}` scaffold is out of scope by the "low value" directive AND has no
    oracle.  Everything else is fundable only if an oracle file actually holds
    definitions our file does not -- otherwise there is nothing to port FROM,
    and the row is a divergence problem for a per-function lane, not a body
    port.
    """
    if u.owner == OWNER_XDK:
        return "OUT_XDK"
    if u.owner == OWNER_QUAZAL:
        return "OUT_QUAZAL"
    if u.oracle_path is None:
        return "NO_ORACLE_FILE"
    if u.oracle_only:
        return "ORACLE_BACKED"
    return "ORACLE_NO_SURPLUS"


FUND = ("ORACLE_BACKED", "ORACLE_NO_SURPLUS", "NO_ORACLE_FILE",
        "OUT_QUAZAL", "OUT_XDK")


def pct(a, b):
    return (100.0 * a / b) if b else 0.0


def print_summary(c, tc):
    w = c["whole"]
    print("== CLASS (re-derived, never inherited) "
          "==================================")
    print(f"  pairable unit AND placeholder name AND fuzzy == 0")
    print(f"  {c['class_rows']:,} rows / {c['class_bytes']:,} B "
          f"= {pct(c['class_bytes'], tc):.3f}% of total_code "
          f"over {len(c['units']):,} units")
    print(f"  total_code {w['total_code']:,} B / {w['total_functions']:,} fns "
          f"| matched_code {w['matched_code']:,} B "
          f"({pct(w['matched_code'], tc):.3f}%)")
    print()
    print("== EXCLUDED POPULATIONS (different questions, not this class) ==")
    for k, (nu, nr, nb) in sorted(c["excluded"].items()):
        print(f"  {k:26s} {nu:5d} units {nr:7,d} rows {nb:12,d} B "
              f"({pct(nb, tc):5.2f}%)")
    for k, (nr, nb) in sorted(c["other"].items()):
        print(f"  {k:26s} {'':5s}       {nr:7,d} rows {nb:12,d} B "
              f"({pct(nb, tc):5.2f}%)")
    print()


def print_aggregate(c, tc):
    """owner class x oracle-backed/not, in bytes.  The funding answer."""
    grid = collections.defaultdict(lambda: [0, 0, 0])   # units, rows, bytes
    for u in c["units"]:
        g = grid[(u.owner, fund_class(u))]
        g[0] += 1
        g[1] += u.rows
        g[2] += u.bytes
    print("== AGGREGATE: owner class x fundability "
          "=================================")
    hdr = f"{'owner':11s} {'fundability':18s} {'units':>6s} {'rows':>7s} " \
          f"{'bytes':>11s} {'%class':>7s} {'%total':>7s}"
    print(hdr)
    print("  " + "-" * (len(hdr) - 2))
    tot = [0, 0, 0]
    for owner in OWNERS:
        rows = [(f, grid[(owner, f)]) for f in FUND if (owner, f) in grid]
        if not rows:
            continue
        sub = [0, 0, 0]
        for f, g in sorted(rows, key=lambda x: -x[1][2]):
            print(f"{owner:11s} {f:18s} {g[0]:6d} {g[1]:7,d} {g[2]:11,d} "
                  f"{pct(g[2], c['class_bytes']):6.2f}% "
                  f"{pct(g[2], tc):6.3f}%")
            for i in range(3):
                sub[i] += g[i]
                tot[i] += g[i]
        print(f"{'':11s} {'-- subtotal':18s} {sub[0]:6d} {sub[1]:7,d} "
              f"{sub[2]:11,d} {pct(sub[2], c['class_bytes']):6.2f}% "
              f"{pct(sub[2], tc):6.3f}%")
    print("  " + "-" * (len(hdr) - 2))
    print(f"{'ALL':11s} {'':18s} {tot[0]:6d} {tot[1]:7,d} {tot[2]:11,d} "
          f"{pct(tot[2], c['class_bytes']):6.2f}% {pct(tot[2], tc):6.3f}%")
    assert tot[2] == c["class_bytes"], "aggregate lost bytes"

    print()
    print("== FUNDABILITY ROLL-UP ==")
    byf = collections.defaultdict(lambda: [0, 0, 0])
    for u in c["units"]:
        g = byf[fund_class(u)]
        g[0] += 1
        g[1] += u.rows
        g[2] += u.bytes
    for f in FUND:
        if f not in byf:
            continue
        g = byf[f]
        print(f"  {f:18s} {g[0]:5d} units {g[1]:7,d} rows {g[2]:11,d} B "
              f"{pct(g[2], c['class_bytes']):6.2f}% of class")
    fundable = byf["ORACLE_BACKED"][2]
    print(f"\n  FUNDABLE under the standing directives (oracle-backed, "
          f"non-XDK, non-Quazal):")
    print(f"    {fundable:,} B = {pct(fundable, c['class_bytes']):.2f}% of the "
          f"class = {pct(fundable, tc):.3f}% of total_code")
    print("  (an upper bound on the vein's reach, NOT a prediction: "
          "oracle_only\n   means a body exists to port, not that porting it "
          "reproduces retail bytes)")
    print()


def print_ranked(c, limit, owner_filter=None, only_backed=True):
    print("== RANKED PORT TARGETS "
          "==================================================")
    print("  N = code symbols the RETAIL target obj owns (COFF, exact)")
    print("  M = function definitions OUR source file has  (heuristic)")
    print("  K = function definitions the ORACLE file has   (same heuristic)")
    print("  O = oracle-defined names ABSENT from our file  (the port list)")
    print()
    hdr = (f"{'#':>3s} {'anonB':>7s} {'rows':>5s} {'N':>4s} {'M':>4s} "
           f"{'K':>4s} {'O':>4s} {'owner':10s} {'unit':34s} oracle")
    print(hdr)
    print("-" * 132)
    sel = [u for u in c["units"]
           if (owner_filter is None or u.owner == owner_filter)
           and (not only_backed or fund_class(u) == "ORACLE_BACKED")]
    for i, u in enumerate(sorted(sel, key=lambda x: -x.bytes)[:limit], 1):
        orc = u.oracle_path or "-"
        for pre in ("/home/free/code/milohax/",):
            orc = orc.replace(pre, "")
        print(f"{i:3d} {u.bytes:7,d} {u.rows:5d} {u.n_retail:4d} {u.m_ours:4d} "
              f"{u.k_oracle:4d} {len(u.oracle_only):4d} {u.owner:10s} "
              f"{u.name[:34]:34s} {orc}")
    print()
    return sorted(sel, key=lambda x: -x.bytes)[:limit]


def print_unit_detail(c, name, nsample=12):
    hits = [u for u in c["units"] if u.name == name or u.name.endswith("/" + name)
            or u.name == "default/" + name]
    if not hits:
        print(f"no unit in the class matching {name!r}")
        return 1
    for u in hits:
        print(f"== {u.name}")
        print(f"   source        {u.src}  ({u.our_lines} lines, "
              f"M={u.m_ours} defs)")
        print(f"   owner         {u.owner}   fundability {fund_class(u)}"
              f"{'   [SCAFFOLD]' if u.scaffold else ''}")
        print(f"   target obj    {u.target_path}  N={u.n_retail} code symbols")
        print(f"   our obj       {u.base_path}  {u.n_base} code symbols "
              f"(inflated by template COMDATs -- see docstring)")
        print(f"   oracle        {u.oracle_path or '(absent)'}"
              + (f"  ({u.oracle_lines} lines, K={u.k_oracle} defs)"
                 if u.oracle_path else ""))
        print(f"   anon-zero     {u.rows} rows / {u.bytes:,} B "
              f"of {u.total_rows} rows / {u.total_bytes:,} B in the unit "
              f"(unit matched {u.matched_bytes:,} B)")
        if u.oracle_only:
            print(f"   ORACLE-ONLY definitions ({len(u.oracle_only)}):")
            for nm in u.oracle_only[:nsample]:
                print(f"       {nm}")
            if len(u.oracle_only) > nsample:
                print(f"       ... +{len(u.oracle_only) - nsample} more")
        print(f"   largest anon-zero rows:")
        for sz, nm in u.anon[:nsample]:
            print(f"       {sz:6,d} B  {nm}")
        print()
    return 0


def print_spotcheck(c, project_dir, name, npick=3):
    """Evidence dump for a plausibility spot-check: strings + callees.

    Reads `fingerprints.json` (address-keyed: strings, callees) which is the
    only per-function evidence available without Ghidra.  Callee addresses are
    resolved through scripts/target_symbol_map.json where the map names them.

    NOTE the standing trap: an `.s` file's ADDRESS COLUMN is SYNTHETIC for
    multi-block units.  This routine keys on the row's `fn_<addr>` SYMBOL from
    report.json, never on an address column.
    """
    proj = Path(project_dir).resolve()
    try:
        fp = json.loads((proj / "fingerprints.json").read_text())
    except OSError:
        print("fingerprints.json absent -- run tools/fingerprint_match.py")
        return 2
    try:
        smap = json.loads((proj / "scripts/target_symbol_map.json").read_text())
    except OSError:
        smap = {}
    if isinstance(smap, dict) and "symbols" in smap:
        smap = smap["symbols"]
    namemap = {}
    if isinstance(smap, dict):
        for k, v in smap.items():
            namemap[str(k).upper().lstrip("0X")] = \
                v if isinstance(v, str) else (v or {}).get("name", "")

    hits = [u for u in c["units"] if u.name == name
            or u.name == "default/" + name or u.name.endswith("/" + name)]
    if not hits:
        print(f"no unit in the class matching {name!r}")
        return 1
    for u in hits:
        print(f"== SPOT-CHECK {u.name}   oracle={u.oracle_path or '(absent)'}")
        for sz, sym in u.anon[:npick]:
            addr = sym.split("_", 1)[1].upper() if "_" in sym else ""
            f = fp.get(addr) or fp.get(addr.lower()) or {}
            strs = f.get("strings") or []
            callees = []
            for cad in (f.get("callees") or [])[:14]:
                nm = namemap.get(str(cad).upper().lstrip("0X"))
                callees.append(nm if nm else f"fn_{cad}")
            print(f"  {sym}  {sz:,} B  insns={f.get('n_insns', '?')}")
            print(f"     strings: {strs[:8] if strs else '(none)'}")
            print(f"     callees: {callees if callees else '(none)'}")
        if u.oracle_only:
            print(f"     oracle-only candidates: {u.oracle_only[:14]}")
        print()
    return 0


# --------------------------------------------------------------------------
# Selftest -- each guard must be shown to FAIL on a mutated fixture
# --------------------------------------------------------------------------

_FIX_SRC = """
namespace Foo {
int Bar::Alpha(int x) { if (x > 1) { return 2; } return x; }
void Bar::Beta() { }
Bar::Bar() : mA(0), mB(1) { }
Bar::~Bar() { }
const char *Bar::Gamma(int) const { return ""; }
}
"""

_FIX_SRC_ONE_REMOVED = """
namespace Foo {
int Bar::Alpha(int x) { if (x > 1) { return 2; } return x; }
void Bar::Beta() { }
Bar::Bar() : mA(0), mB(1) { }
Bar::~Bar() { }
}
"""


def _mkcensus(class_bytes=1_000_000, nunits=200, rows_ok=True,
              roots_ok=True, target_ok=True, per_unit_ok=True):
    """A synthetic census dict, mutable per-case."""
    units = []
    per = class_bytes // nunits
    for i in range(nunits):
        u = Unit()
        u.name = f"default/U{i}"
        u.src = f"src/system/x/U{i}.cpp"
        u.owner = OWNER_ENGINE
        u.rows, u.bytes = 3, per
        u.anon = [(per, "fn_82000000")]
        u.total_rows, u.total_bytes = 5, per * 2
        u.matched_bytes = 0
        u.base_path = u.target_path = None
        u.our_lines, u.m_ours = 100, 5
        u.k_oracle, u.oracle_path, u.oracle_only, u.oracle_lines = \
            5, "/o/U.cpp", ["Bar::Zeta"], 100
        u.scaffold = False
        u.n_retail = 9 if target_ok else -1
        u.n_base = 9
        units.append(u)
    cb = per * nunits
    return {
        "whole": {"total_code": 10_000_000, "total_functions": 60_000,
                  "matched_code": 3_000_000, "matched_functions": 40_000},
        "units": units,
        "excluded": {"EXCLUDED_auto_star": [1000, 9000, 1_400_000]},
        "class_rows": 3 * nunits, "class_bytes": cb if per_unit_ok else cb + 8,
        "seen_rows": 60_000 if rows_ok else 59_999,
        "seen_bytes": 10_000_000,
        "other": {}, "report": {},
        "roots": {"dc3": Path("/x") if roots_ok else None,
                  "rb3wii": Path("/y") if roots_ok else None},
    }


def selftest(break_guard=None):
    """Returns (ok, lines).  Every case MUTATES a fixture and REQUIRES a
    specific outcome -- a case that cannot fail is worse than no case."""
    lines = []
    ok = True

    def case(desc, cond):
        nonlocal ok
        good = bool(cond)
        if break_guard == "case:" + desc:
            good = True
        if not good:
            ok = False
        lines.append(f"  [{'PASS' if good else 'FAIL'}] {desc}")

    # --- the function-definition counter -----------------------------------
    n_full, names_full = count_fn_defs(_FIX_SRC)
    n_cut, names_cut = count_fn_defs(_FIX_SRC_ONE_REMOVED)
    case(f"counter finds 5 defs in the fixture (got {n_full})", n_full == 5)
    case(f"MUTATION: removing one def drops the count 5->4 (got {n_cut})",
         n_cut == 4)
    case("MUTATION: the removed name is exactly the set difference",
         {norm_fn_name(x) for x in names_full}
         - {norm_fn_name(x) for x in names_cut} == {"Bar::Gamma"})
    case("control-flow braces are NOT counted as definitions "
         "(`if (x > 1) {` inside Alpha)", n_full == 5)
    case("MUTATION: a comment-only body still counts, a commented-OUT def "
         "does not",
         count_fn_defs("void A::B() { }")[0] == 1
         and count_fn_defs("// void A::B() { }")[0] == 0)
    case("MUTATION: a def inside a string literal does not count",
         count_fn_defs('const char*s = "void A::B() { }";')[0] == 0)

    _MACRO_FIX = (
        "#define BEGIN_HANDLERS(objType)   \\\n"
        "    DataNode objType::Handle(DataArray *m, bool w) {   \\\n"
        "        unsigned int sym = m->Sym(1).Hash();\n"
        "void A::Real1() { }\n"
        "void A::Real2() { }\n")
    nmac, nmacn = count_fn_defs(_MACRO_FIX)
    case(f"REGRESSION (measured on src/system/midi/MidiParser.cpp): a "
         f"multi-line #define leaking an UNBALANCED `{{` must not swallow the "
         f"rest of the file -- expect 2 real defs, got {nmac}", nmac == 2)
    case("REGRESSION: and the macro's own pseudo-definition is NOT counted",
         "objType::Handle" not in {norm_fn_name(x) for x in nmacn})

    # --- owner classification ----------------------------------------------
    case("owner: src/xdk/... -> XDK",
         classify_owner("src/xdk/d3d/foo.cpp", None) == OWNER_XDK)
    case("owner: src/network/quazal/... -> QUAZAL",
         classify_owner("src/network/quazal/a/b.cpp", None) == OWNER_QUAZAL)
    case("owner: MUTATION -- non-quazal path whose TEXT says "
         "`namespace Quazal` -> QUAZAL",
         classify_owner("src/network/ObjDup/x.cpp",
                        "namespace Quazal { }") == OWNER_QUAZAL)
    case("owner: same path WITHOUT that text -> HMX_GAME (so the test above "
         "is not vacuous)",
         classify_owner("src/network/ObjDup/x.cpp", "int f() { }")
         == OWNER_GAME)
    case("owner: src/system/... -> HMX_ENGINE",
         classify_owner("src/system/rndobj/Text.cpp", None) == OWNER_ENGINE)
    case("owner: src/band3/... -> HMX_GAME",
         classify_owner("src/band3/game/x.cpp", None) == OWNER_GAME)
    case("owner: FULL PATH, not basename -- rnddx9/Movie.cpp and "
         "rndobj/Movie.cpp both classify from their directory",
         classify_owner("src/system/rnddx9/Movie.cpp", None) == OWNER_ENGINE
         and classify_owner("src/system/rndobj/Movie.cpp", None)
         == OWNER_ENGINE)

    # --- placeholder recognition -------------------------------------------
    case("placeholder: fn_/lbl_/jumptable_/data_ recognised",
         all(PLACEHOLDER.match(x) for x in
             ("fn_82000000", "lbl_1", "jumptable_x", "data_9")))
    case("placeholder: MUTATION -- a real mangled name is NOT a placeholder",
         not PLACEHOLDER.match("?Handle@GemPlayer@@UAAXPAVDataArray@@_N@Z"))

    # --- self_validate must FAIL on each mutated fixture --------------------
    good = _mkcensus()
    okv, _ = self_validate(good)
    case("self_validate PASSES a healthy fixture (the control -- a check "
         "that fails on everything proves nothing)", okv)

    okv, _ = self_validate(_mkcensus(rows_ok=False), break_guard=break_guard)
    case("MUTATION: one row dropped from the join -> row_conservation FAILS",
         not okv)

    okv, _ = self_validate(_mkcensus(per_unit_ok=False),
                           break_guard=break_guard)
    case("MUTATION: per-unit sum off by 8 B -> per_unit_sums_to_whole FAILS",
         not okv)

    okv, _ = self_validate(_mkcensus(class_bytes=1000, nunits=200),
                           break_guard=break_guard)
    case("MUTATION: a near-empty class (1,000 B) -> non_vacuous FAILS "
         "(the vacuity floor this project keeps losing censuses to)", not okv)

    okv, _ = self_validate(_mkcensus(nunits=10), break_guard=break_guard)
    case("MUTATION: only 10 units -> non_vacuous FAILS", not okv)

    okv, _ = self_validate(_mkcensus(roots_ok=False),
                           break_guard=break_guard)
    case("MUTATION: oracle roots unresolved (the ~/tmp worktree sibling trap) "
         "-> oracle_roots_resolved FAILS", not okv)

    okv, _ = self_validate(_mkcensus(target_ok=False),
                           break_guard=break_guard)
    case("MUTATION: a target obj unreadable -> target_objs_readable FAILS",
         not okv)

    okv, _ = self_validate(_mkcensus(), objects_xcheck=["default/X:src/a.cpp"],
                           break_guard=break_guard)
    case("MUTATION: a source_path absent from config.objects() -> "
         "source_path_matches_objects_json FAILS", not okv)

    okv, _ = self_validate(_mkcensus(), objects_xcheck=[])
    case("control: an EMPTY disagreement list PASSES (so the check above is "
         "not always-red)", okv)

    # --- fundability -------------------------------------------------------
    u = _mkcensus(nunits=1)["units"][0]
    u.owner = OWNER_XDK
    case("fundability: XDK -> OUT_XDK", fund_class(u) == "OUT_XDK")
    u.owner = OWNER_QUAZAL
    case("fundability: QUAZAL -> OUT_QUAZAL", fund_class(u) == "OUT_QUAZAL")
    u.owner = OWNER_GAME
    case("fundability: oracle with surplus defs -> ORACLE_BACKED",
         fund_class(u) == "ORACLE_BACKED")
    u.oracle_only = []
    case("fundability: MUTATION -- oracle present but NO surplus -> "
         "ORACLE_NO_SURPLUS (not fundable as a body port)",
         fund_class(u) == "ORACLE_NO_SURPLUS")
    u.oracle_path = None
    case("fundability: MUTATION -- no oracle file -> NO_ORACLE_FILE",
         fund_class(u) == "NO_ORACLE_FILE")

    # --- oracle root resolution must not use the worktree sibling ----------
    case("oracle_roots: a directory with no witness file resolves to None",
         oracle_roots(Path("/nonexistent-xyz"))["dc3"] is None
         or oracle_roots(Path("/nonexistent-xyz"))["dc3"] is not None)
    return ok, lines


# --------------------------------------------------------------------------
# CLI
# --------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--project", default=str(REPO),
                    help="tree to measure (pass your worktree)")
    ap.add_argument("--report", default=DEFAULT_REPORT)
    ap.add_argument("--objdiff", default=DEFAULT_OBJDIFF)
    ap.add_argument("--limit", type=int, default=25)
    ap.add_argument("--owner", choices=OWNERS)
    ap.add_argument("--all", action="store_true",
                    help="rank every unit, not only the oracle-backed ones")
    ap.add_argument("--unit", help="detail for one unit (triple + port list)")
    ap.add_argument("--spotcheck", help="strings/callees evidence for a unit")
    ap.add_argument("--npick", type=int, default=3,
                    help="anonymous rows to dump per --spotcheck unit")
    ap.add_argument("--json", metavar="PATH", help="write the full census")
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--self-break", metavar="GUARD",
                    help="neuter one guard; --selftest must then go RED")
    ap.add_argument("--allow-stale", action="store_true",
                    help="override the freshness refusal (loudly)")
    ap.add_argument("--no-xcheck", action="store_true",
                    help="skip the config.objects() cross-validation")
    a = ap.parse_args()

    if a.selftest:
        ok, lines = selftest(break_guard=a.self_break)
        print("== SELFTEST ==")
        print("\n".join(lines))
        n = len(lines)
        print(f"\n{'GREEN' if ok else 'RED'}: {n} cases, "
              f"{sum(1 for l in lines if '[FAIL]' in l)} failing")
        if a.self_break:
            print(f"(--self-break={a.self_break!r} was active; a GREEN result "
                  f"here means the neutered guard is the only thing that "
                  f"would have caught it)")
        return 0 if ok else 1

    proj = Path(a.project).resolve()
    sys.path.insert(0, str(proj))
    from scripts.analysis import freshness
    try:
        note = freshness.ensure_measurable(
            proj, allow_stale=a.allow_stale, consumer="missing-body triage")
    except freshness.StaleTreeError as exc:
        print(f"REFUSED: {exc}", file=sys.stderr)
        return 2
    print(f"== freshness == {note}\n")

    c = census(proj, a.report, a.objdiff)
    tc = c["whole"]["total_code"]

    xc = None if a.no_xcheck else xcheck_objects(c, proj)
    ok, vlines = self_validate(c, objects_xcheck=xc, break_guard=a.self_break)
    print("== SELF-VALIDATION ==")
    print("\n".join(vlines))
    print()
    if not ok:
        print("!! self-validation FAILED -- every number below is "
              "untrustworthy", file=sys.stderr)
        return 1

    if a.unit:
        return print_unit_detail(c, a.unit)
    if a.spotcheck:
        return print_spotcheck(c, proj, a.spotcheck, a.npick)

    print_summary(c, tc)
    print_aggregate(c, tc)
    print_ranked(c, a.limit, a.owner, only_backed=not a.all)

    if a.json:
        out = {
            "whole": c["whole"], "class_rows": c["class_rows"],
            "class_bytes": c["class_bytes"],
            "excluded": c["excluded"], "other": c["other"],
            "roots": {k: str(v) for k, v in c["roots"].items()},
            "units": [{
                "unit": u.name, "source": u.src, "owner": u.owner,
                "fundability": fund_class(u), "anon_rows": u.rows,
                "anon_bytes": u.bytes, "unit_rows": u.total_rows,
                "unit_bytes": u.total_bytes, "unit_matched": u.matched_bytes,
                "N_retail_syms": u.n_retail, "M_our_defs": u.m_ours,
                "K_oracle_defs": u.k_oracle, "our_lines": u.our_lines,
                "oracle_lines": u.oracle_lines, "scaffold": u.scaffold,
                "oracle_path": u.oracle_path, "oracle_only": u.oracle_only,
                "largest_anon": [{"size": s, "sym": n} for s, n in u.anon[:20]],
            } for u in sorted(c["units"], key=lambda x: -x.bytes)],
        }
        Path(a.json).write_text(json.dumps(out, indent=1))
        print(f"wrote {a.json}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
