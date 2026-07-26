#!/usr/bin/env python3
"""identical_pct_cluster_scan.py — the "identical raw percentage" standing scan.

WHY
---
Lane X (docs/plans/funclet-cascade-lever-2026-07-25.md §32.6) established that

    an identical raw `match_percent_normalized` across UNRELATED units means
    ONE shared cause, not N independent jobs.

Evidence: 13 `SetType` overrides all at exactly 62.92405 -> one word per site,
13/13 flipped. 18 `SyncProperty` near-misses -> one trailing-`SYNC_SUPERCLASS`
removal, +29.

This tool turns that observation into a standing, re-runnable scan.

WHY THE PERCENTAGE IS SUCH A SHARP FINGERPRINT
----------------------------------------------
objdiff (`objdiff-core/src/diff/code.rs:276-291`) computes

    max_score  = n_instructions * PENALTY_INSERT_DELETE      (= N * 100)
    pct_norm   = (1 - normalized_diff_score / max_score) * 100      [as f32]

with PENALTY_INSERT_DELETE=100, PENALTY_REPLACE=60, PENALTY_REG_DIFF=5,
PENALTY_IMM_DIFF=1.  So the percentage is *not* a fuzzy score: it is a lossless
encoding of the rational  S / (100 * N).  Given N (recoverable from the
function's byte size / 4) you can invert it and get the **exact integer penalty
score S**.  Two functions sharing a percentage share (S, N) exactly.

That makes five bucketing axes available, all computed here:

  1. `pct`         — exact `repr()` of the f32.  The signature lane X named.
  2. `score`       — the exact integer penalty S alone.  DIAGNOSTIC ONLY: S is
                     scale-free, so it happily merges a 4-instruction stub with
                     a 128-instruction method that share a penalty total.  Use
                     it to read shapes, not to route work.
  3. `score_shape` — ★ THE GENERALISATION OF (1).  (name-shape, S).  `pct`
                     encodes (S, N) jointly, so ONE cause hitting instantiations
                     of DIFFERENT length splits into several near-but-unequal
                     percentages.  Real example: `_M_insert_overflow_aux` sits at
                     98.78049 (N=82), 98.765434 (N=81) and 98.809525 (N=84) --
                     three "different" percentages, one S=100 == exactly ONE
                     inserted/deleted instruction.  This axis merges them; the
                     `pct` axis does not.
  4. `delta`       — exact base-minus-target byte-size delta (needs
                     --with-sizes, which parses our compiled COFF objs).
                     DIAGNOSTIC ONLY: the +0/+4 bins are enormous and mean
                     nothing on their own.
  5. `delta_shape` — ★ (name-shape, byte delta).  A *constant* size surplus
                     repeated across unrelated classes is the signature of a
                     shared macro/template accretion (funclet-cascade doc
                     §23.1).  Independent confirmation channel for (3): a
                     cluster that shows up on BOTH is as close to certain as
                     this project gets.

USAGE
-----
    python3 scripts/harvest/identical_pct_cluster_scan.py                    # pct axis
    python3 scripts/harvest/identical_pct_cluster_scan.py --axis score_shape
    python3 scripts/harvest/identical_pct_cluster_scan.py --with-sizes --axis all
    python3 scripts/harvest/identical_pct_cluster_scan.py --census           # distribution only
    python3 scripts/harvest/identical_pct_cluster_scan.py --json out.json

Runs in ~0.2 s (~2 s with --with-sizes).  Re-run after every wave.

Defaults are tuned for *actionable* clusters: named symbols only (anonymous
`fn_` carry no source handle), >=3 members, >=2 unrelated units.  Use
`--include-anon` for the honest whole-pool census.

CAUTION (lane X §32.2, carried forward)
---------------------------------------
A cluster is a *hypothesis of a shared cause*, not a licence to edit a shared
macro.  Before any fleet-wide edit check whether members that ALREADY read
100.0 depend on the current form -- `ColorPalette` / `SpotlightDrawer` match
retail *because* they carry the shape an earlier sweep removed elsewhere.  Use
`--siblings` to list the 100%-matching functions that share each cluster's
name-shape; if that list is non-empty, change the CALL SITES, not the macro.
"""
from __future__ import annotations

import argparse
import collections
import json
import os
import re
import struct
import sys
from pathlib import Path

REPO_DEFAULT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

PENALTY_INSERT_DELETE = 100
PENALTY_REPLACE = 60
PENALTY_REG_DIFF = 5
PENALTY_IMM_DIFF = 1


# ---------------------------------------------------------------- score invert
def invert_score(pct: float, size_bytes: int):
    """Recover the exact integer objdiff penalty score S from (pct, size).

    Returns (S, N) or (None, N) when the f32 round-trip does not confirm.
    The round-trip check is what makes this safe: if `size` is not the side
    objdiff used for `max_score` (i.e. base and target instruction counts
    differ), the reconstruction will not reproduce the stored f32 and we say so
    rather than emit a wrong number.
    """
    n = size_bytes // 4
    if n <= 0:
        return None, 0
    # NB: report.json stores the shortest decimal repr of objdiff's f32.  Python
    # parses that to the nearest *double*, which is NOT bit-equal to the f32, so
    # both sides must be forced through f32 before comparing.
    want = struct.pack("<f", pct)
    s = int(round((100.0 - pct) * n))
    for cand in (s, s - 1, s + 1, s - 2, s + 2):
        if cand < 0:
            continue
        if struct.pack("<f", (1.0 - cand / (100.0 * n)) * 100.0) == want:
            return cand, n
    return None, n


def decompose(s: int, n: int, limit: int = 6):
    """Best-effort penalty decomposition: S = 100a + 60b + 5c + 1d.

    Only reported when a small, unambiguous solution exists -- the point is to
    label clean shapes ("exactly one inserted instruction", "one replace"), not
    to enumerate every arithmetic factorisation of a large score.
    """
    if s is None:
        return ""
    if s == 0:
        return "clean"
    best = None
    for a in range(0, min(limit, s // PENALTY_INSERT_DELETE) + 1):
        r1 = s - a * PENALTY_INSERT_DELETE
        for b in range(0, min(limit, r1 // PENALTY_REPLACE) + 1):
            r2 = r1 - b * PENALTY_REPLACE
            for c in range(0, min(limit, r2 // PENALTY_REG_DIFF) + 1):
                d = r2 - c * PENALTY_REG_DIFF
                if d < 0 or d > limit:
                    continue
                cost = a + b + c + d
                if best is None or cost < best[0]:
                    best = (cost, a, b, c, d)
    if best is None:
        return "S=%d (complex)" % s
    _, a, b, c, d = best
    parts = []
    if a:
        parts.append("%dx ins/del" % a)
    if b:
        parts.append("%dx replace" % b)
    if c:
        parts.append("%dx reg" % c)
    if d:
        parts.append("%dx imm" % d)
    return "+".join(parts) if parts else "S=%d" % s


def confidence(s: int, limit: int = 6):
    """STRUCTURAL vs ARG-ONLY -- the empirically validated discriminator.

    MEASURED, 2026-07-26 (lane Y), and it overturns the naive reading of the
    signature:

      * `?_M_insert_overflow_aux@` S=100 (one ins/del).  Two members from
        completely unrelated units -- vector<LevelData> in
        default/system/synth_xbox/Synth and vector<Keyframe@LightPreset> in
        default/LightPreset -- have a **byte-identical mismatch profile**: the
        same `delete: stw r24, 0x58, r31` at index 26 and the same six arg-diffs
        at indices 10/24/27/37/67/76.  ONE shared cause.  TRUE cluster.

      * `??$_Destroy_Range@` S=1 (one immediate).  7 members, 7 units, all at
        exactly 99.95 -- and the immediate is `addi [off:-4]` for
        `Character::Lod` but `addi [off:+84]` for `FileMerger::Merger`.
        **Same score, unrelated causes** (per-instantiation element `sizeof`).
        FALSE cluster.

    The precision of the identical-percentage signature therefore comes from the
    penalty being *structural* -- a whole instruction inserted/deleted/replaced
    at the same index implies the same source-level defect.  A penalty made only
    of immediate/register operand diffs implies nothing: templates parameterise
    exactly those immediates per instantiation, so an identical tiny score is
    the expected coincidence, not evidence.

    FOLLOW-UP, same session: ARG-ONLY turned out to be worse than "coincidence".
    The `??_G` scalar-deleting-destructor cluster (32 members, all at exactly
    99.882355) is **31 target MISPAIRS**.  Every Milo class inherits
    `Hmx::Object` virtually, so the thunk is 17 instructions whose only
    class-specific content is one vbase-offset immediate plus three
    relocations -- and normalized diff masks relocations, so ANY two such thunks
    with different vbase offsets score exactly 15/17.  Witness that needs no
    theory: in one unit, `vector<CharEyes::EyeDesc>::_M_fill_insert` reads its
    sizeof immediate as target=ours-36 while `::resize` reads target=ours+48.
    One binary cannot hold two sizeof(EyeDesc).

    So an ARG-ONLY cluster of COMPILER-GENERATED symbols (`??_G`, `??1`,
    `vector<T>` members) is a **map-mispair detector**, and the immediate is
    frequently the WRONG instantiation's constant.  Free triage, no build: a
    real layout bug shows the IDENTICAL delta on every other function of the
    same class; a mispair does not.

    Returns "STRUCTURAL" (fund it) or "ARG-ONLY" (verify two members first).
    """
    if s is None:
        return "?"
    if s >= PENALTY_REPLACE:
        return "STRUCTURAL"
    for a in range(0, min(limit, s // PENALTY_INSERT_DELETE) + 1):
        r1 = s - a * PENALTY_INSERT_DELETE
        for b in range(0, min(limit, r1 // PENALTY_REPLACE) + 1):
            r2 = r1 - b * PENALTY_REPLACE
            for c in range(0, min(limit, r2 // PENALTY_REG_DIFF) + 1):
                d = r2 - c * PENALTY_REG_DIFF
                if 0 <= d <= limit and (a or b):
                    return "STRUCTURAL"
    return "ARG-ONLY"


# ------------------------------------------------------------- name-shape keys
_TEMPLATE_ARG = re.compile(r"@\?\$")


def name_shape(mangled: str) -> str:
    """A coarse 'family' key for an MSVC mangled name.

    `?SetType@RndGroup@@...`   -> '?SetType@'
    `??$__destroy_range_aux@...` -> '??$__destroy_range_aux@'
    `?_M_insert_overflow_aux@?$vector@...` -> '?_M_insert_overflow_aux@'
    `??_GSharedGroup@@UAAPAXI@Z` -> '??_G'   (scalar deleting dtor thunk)
    `??1?$vector@...`          -> '??1'     (dtor)
    """
    if mangled.startswith("??_") and len(mangled) > 4:
        return mangled[:4]
    m = re.match(r"^\?\?\$([A-Za-z_][\w]*)@", mangled)
    if m:
        return "??$%s@" % m.group(1)
    m = re.match(r"^\?([A-Za-z_~][\w]*)@", mangled)
    if m:
        return "?%s@" % m.group(1)
    m = re.match(r"^\?\?([0-9A-Z])", mangled)
    if m:
        return "??%s" % m.group(1)
    return mangled[:8]


def class_of(mangled: str) -> str:
    """Best-effort owning class token, for reporting 'unrelated' spread."""
    m = re.match(r"^\?[A-Za-z_~][\w]*@([\w]+)@@", mangled)
    if m:
        return m.group(1)
    m = re.match(r"^\?\?_[EG]([\w]+)@@", mangled)
    if m:
        return m.group(1)
    m = re.match(r"^\?[A-Za-z_~][\w]*@\?\$([\w]+)@", mangled)
    if m:
        return m.group(1) + "<T>"
    m = re.match(r"^\?\?[0-9]\?\$([\w]+)@", mangled)
    if m:
        return m.group(1) + "<T>"
    return ""


# MSVC signature marker: "@@" followed by the calling-convention/access code
# (QAA / IAA / AAA / UAA / YA / SA / QBA ...).  Everything before it is the
# class-or-template qualification.
_SIG = re.compile(r"@@(?=[A-Z]{2,3}[A-Z@])")


def instantiation(mangled: str) -> str:
    """The class / template-instantiation blob, method name and signature removed.

    `?resize@?$vector@UEyeDesc@CharEyes@@…@@QAAXIABUEyeDesc@CharEyes@@@Z`
    and
    `?_M_fill_insert@?$vector@UEyeDesc@CharEyes@@…@@AAAXPAUEyeDesc…@Z`
    both reduce to the same key -- which is the point: they are two functions of
    ONE instantiation and must therefore agree about its layout.

    Used by --mispair-check, the free map-lane triage from
    docs/plans/identical-pct-cluster-scan-2026-07-26.md §7.2 handoff 1: a real
    layout bug moves EVERY function of an instantiation the same way, a mispair
    moves only some.
    """
    if mangled.startswith("??$"):
        m = re.match(r"^\?\?\$[A-Za-z_][\w]*@(.*)$", mangled)
        rest = m.group(1) if m else mangled
    elif mangled.startswith("??_"):
        rest = mangled[4:]
    elif re.match(r"^\?\?[0-9A-Z]", mangled):
        rest = mangled[3:]
    else:
        m = re.match(r"^\?[A-Za-z_~][\w]*@(.*)$", mangled)
        rest = m.group(1) if m else mangled
    return _SIG.split(rest, maxsplit=1)[0]


def unit_family(unit: str) -> str:
    """Unit key used for the 'unrelated units' test -- the object path."""
    return unit


# --------------------------------------------------------------- base obj scan
def _parse_obj_sizes(path: Path):
    """{symbol: size_in_bytes} for code symbols in one COFF obj.

    Same COFF walk as scripts/harvest/homing_scan.py:parse_obj, trimmed to
    sizes only.
    """
    try:
        data = path.read_bytes()
    except OSError:
        return {}
    if len(data) < 20:
        return {}
    try:
        nsec = struct.unpack_from("<H", data, 2)[0]
        symoff = struct.unpack_from("<I", data, 8)[0]
        nsym = struct.unpack_from("<I", data, 12)[0]
        optsz = struct.unpack_from("<H", data, 16)[0]
    except struct.error:
        return {}
    if symoff == 0 or nsym == 0:
        return {}
    str_start = symoff + nsym * 18
    secs = {}
    base = 20 + optsz
    for i in range(nsec):
        o = base + i * 40
        if o + 40 > len(data):
            return {}
        raw_size = struct.unpack_from("<I", data, o + 16)[0]
        praw = struct.unpack_from("<I", data, o + 20)[0]
        chars = struct.unpack_from("<I", data, o + 36)[0]
        secs[i + 1] = (raw_size, praw, chars)
    by_sec = collections.defaultdict(list)
    i = 0
    while i < nsym:
        o = symoff + i * 18
        if o + 18 > len(data):
            break
        nm = data[o:o + 8]
        if nm[:4] == b"\0\0\0\0":
            so = struct.unpack_from("<I", nm, 4)[0]
            try:
                e = data.index(b"\0", str_start + so)
                name = data[str_start + so:e].decode("latin1")
            except ValueError:
                name = ""
        else:
            name = nm.split(b"\0")[0].decode("latin1")
        val = struct.unpack_from("<I", data, o + 8)[0]
        secn = struct.unpack_from("<h", data, o + 12)[0]
        typ = struct.unpack_from("<H", data, o + 14)[0]
        sc = data[o + 16]
        naux = data[o + 17]
        if secn > 0 and secn in secs and sc in (2, 3) and typ == 0x20:
            by_sec[secn].append((val, name))
        i += 1 + naux
    out = {}
    for secn, members in by_sec.items():
        raw_size, praw, chars = secs[secn]
        if not (chars & 0x20) or praw == 0:
            continue
        members.sort()
        for k, (val, name) in enumerate(members):
            end = members[k + 1][0] if k + 1 < len(members) else raw_size
            if end > val and name and name not in out:
                out[name] = end - val
    return out


def scan_base_sizes(build_src: Path):
    sizes = {}
    n = 0
    for obj in build_src.rglob("*.obj"):
        n += 1
        for k, v in _parse_obj_sizes(obj).items():
            sizes.setdefault(k, v)
    return sizes, n


# ------------------------------------------------------------------------ main
def load_functions(report_path: Path, include_anon: bool, min_pct: float = -1.0):
    r = json.loads(report_path.read_text())
    sub, at100, allf = [], collections.defaultdict(list), {}
    for u in r["units"]:
        un = u["name"]
        for f in u.get("functions", []):
            p = f.get("match_percent_normalized")
            if p is None:
                continue
            nm = f["name"]
            sz = int(f.get("size", 0))
            if p >= 100.0:
                at100[name_shape(nm)].append((un, nm))
                allf.setdefault(instantiation(nm), []).append((un, nm, p, sz))
                continue
            if p <= min_pct:
                continue
            if p == 0.0:
                # unpaired / no diff at all: carries no shared-cause signal
                if not include_anon:
                    continue
            if not include_anon and not nm.startswith("?"):
                continue
            s, n = invert_score(p, sz)
            allf.setdefault(instantiation(nm), []).append((un, nm, p, sz))
            sub.append(dict(unit=un, name=nm, size=sz, pct=p, pct_key=repr(p),
                            score=s, nins=n, shape=name_shape(nm), cls=class_of(nm),
                            inst=instantiation(nm)))
    return sub, at100, allf


def build_clusters(funcs, axis, min_members, min_units):
    buckets = collections.defaultdict(list)
    for f in funcs:
        if axis == "pct":
            key = f["pct_key"]
        elif axis == "score":
            if f["score"] is None:
                continue
            key = f["score"]
        elif axis == "score_shape":
            # THE generalisation of the pct axis.  pct == (S, N); when one cause
            # hits instantiations of DIFFERENT length, N varies and the pct
            # splits into near-but-unequal values while S stays pinned.
            # Requiring the name-shape to agree stops S alone from merging
            # unrelated functions that happen to share a penalty total.
            if f["score"] is None:
                continue
            key = (f["shape"], f["score"])
        elif axis == "delta":
            if f.get("delta") is None:
                continue
            key = f["delta"]
        elif axis == "delta_shape":
            # A *constant* base-minus-target byte surplus repeated across
            # unrelated classes is the signature of a shared macro/template
            # accretion (funclet-cascade doc §23.1).  Pairing it with the
            # name-shape is what separates that from the huge, meaningless
            # "+0 bytes" and "+4 bytes" global bins.
            if f.get("delta") is None:
                continue
            key = (f["shape"], f["delta"])
        else:
            raise SystemExit("bad axis")
        buckets[key].append(f)
    out = []
    for key, members in buckets.items():
        units = {unit_family(m["unit"]) for m in members}
        if len(members) < min_members or len(units) < min_units:
            continue
        shapes = collections.Counter(m["shape"] for m in members)
        mean_size = sum(m["size"] for m in members) / len(members)
        out.append(dict(key=key, axis=axis, members=members, n=len(members),
                        nunits=len(units), shapes=shapes,
                        mean_size=mean_size, rank=len(members) * mean_size))
    out.sort(key=lambda c: -c["rank"])
    return out


def fmt_cluster(c, at100, siblings, verbose):
    lines = []
    axis = c["axis"]
    if axis == "pct":
        head = "pct == %s" % c["key"]
    elif axis == "score":
        head = "penalty score S == %d" % c["key"]
    elif axis == "score_shape":
        head = "shape %s + penalty S == %d" % (c["key"][0], c["key"][1])
    elif axis == "delta_shape":
        head = "shape %s + size delta == %+d B" % (c["key"][0], c["key"][1])
    else:
        head = "base-target size delta == %+d bytes" % c["key"]
    top_shape, top_n = c["shapes"].most_common(1)[0]
    purity = top_n / c["n"]
    lines.append("=" * 78)
    lines.append("%-42s  n=%-4d units=%-4d rank=%d" % (head, c["n"], c["nunits"], int(c["rank"])))
    lines.append("  dominant name-shape : %s  (%d/%d = %.0f%% pure)"
                 % (top_shape, top_n, c["n"], 100 * purity))
    ns = sorted({m["nins"] for m in c["members"]})
    ss = sorted({m["score"] for m in c["members"] if m["score"] is not None})
    lines.append("  target instrs       : %s" % (ns[:8],))
    if axis not in ("score", "score_shape") and ss:
        lines.append("  penalty score(s)    : %s  -> %s"
                     % (ss[:8], decompose(ss[0], ns[0] if ns else 0)))
    else:
        sc = c["key"][1] if axis == "score_shape" else c["key"]
        lines.append("  shape               : %s" % decompose(sc, ns[0] if ns else 0))
    conf = sorted({confidence(m["score"]) for m in c["members"] if m["score"] is not None})
    lines.append("  confidence          : %s%s"
                 % ("/".join(conf) or "?",
                    "   <-- per-instantiation immediates; likely MAP MISPAIR. "
                    "Verify TWO members on asm before funding"
                    if conf == ["ARG-ONLY"] else ""))
    classes = sorted({m["cls"] for m in c["members"] if m["cls"]})
    if classes:
        lines.append("  classes             : %s%s"
                     % (", ".join(classes[:10]), " ..." if len(classes) > 10 else ""))
    if siblings:
        sib = at100.get(top_shape, [])
        lines.append("  !! %d functions with the SAME name-shape ALREADY read 100.0" % len(sib))
        if sib:
            lines.append("     NEGATIVE CONTROL REQUIRED -- they may depend on the current form.")
            lines.append("     e.g. %s" % ", ".join(n for _, n in sib[:3]))
    show = c["members"] if verbose else c["members"][:12]
    for m in show:
        lines.append("    %6.3f %-46s %s" % (m["pct"], m["unit"][-46:], m["name"][:110]))
    if len(show) < c["n"]:
        lines.append("    ... %d more" % (c["n"] - len(show)))
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--project-dir", default=REPO_DEFAULT)
    ap.add_argument("--report", default=None)
    ap.add_argument("--axis", choices=["pct", "score", "score_shape", "delta",
                             "delta_shape", "all"],
                    default="pct")
    ap.add_argument("--min-members", type=int, default=3)
    ap.add_argument("--min-units", type=int, default=2)
    ap.add_argument("--top", type=int, default=25)
    ap.add_argument("--include-anon", action="store_true",
                    help="include anonymous fn_/__unwind$ symbols (whole-pool census)")
    ap.add_argument("--with-sizes", action="store_true",
                    help="parse compiled objs for base sizes (enables --axis delta)")
    ap.add_argument("--siblings", action="store_true", default=True,
                    help="report same-name-shape functions already at 100%% (negative control)")
    ap.add_argument("--no-siblings", dest="siblings", action="store_false")
    ap.add_argument("--min-pct", type=float, default=-1.0,
                    help="drop functions at or below this pct (use 0 to drop the "
                         "unpaired 0.0 mass from an --include-anon census)")
    ap.add_argument("--mispair-check", action="store_true",
                    help="list template instantiations that mix 100%% and sub-100%% "
                         "functions -- the free map-mispair triage (§7.2)")
    ap.add_argument("--census", action="store_true", help="print the cluster size distribution only")
    ap.add_argument("--verbose", action="store_true")
    ap.add_argument("--json", default=None)
    args = ap.parse_args()

    proj = Path(args.project_dir)
    report = Path(args.report) if args.report else proj / "build" / "45410914" / "report.json"
    if not report.exists():
        sys.exit("report.json not found at %s (build first)" % report)

    funcs, at100, allf = load_functions(report, args.include_anon, args.min_pct)

    if args.mispair_check:
        print("# --mispair-check: instantiations whose functions disagree about "
              "their own layout.\n# A REAL layout bug moves every function of the "
              "instantiation the same way; a\n# MISPAIR moves only some.  See "
              "docs/plans/identical-pct-cluster-scan-2026-07-26.md §7.2.\n")
        cands = []
        for inst, rows in allf.items():
            if not inst or inst.startswith("@"):
                continue          # free functions -- the key is degenerate
            subs = [r for r in rows if r[2] < 100.0]
            hits = [r for r in rows if r[2] >= 100.0]
            # The mispair signature: a MINORITY of an instantiation's functions
            # disagree.  If every function is off, that is a real layout bug.
            if not subs or not hits or len(subs) > len(hits):
                continue
            cands.append((len(hits), len(subs), inst, subs, hits))
        cands.sort(key=lambda c: (c[1], -c[0]))
        for nhit, nsub, inst, subs, hits in cands[:args.top]:
            print("instantiation %-70s  %d sub-100 vs %d at 100.0"
                  % (inst[:70], nsub, nhit))
            for u, n, pc, sz in sorted(subs, key=lambda r: r[2]):
                print("   SUB %8.3f  %-40s %s" % (pc, u[-40:], n.split("@")[0][:46]))
            for u, n, pc, sz in hits[:3]:
                print("   OK  %8.3f  %-40s %s" % (pc, u[-40:], n.split("@")[0][:46]))
        print("\n# %d instantiations have a MINORITY of functions disagreeing about their\n"
              "# own layout -- each is a cheap map-consistency check (no build required)."
              % len(cands))
        return

    if args.with_sizes:
        sizes, nobj = scan_base_sizes(proj / "build" / "45410914" / "src")
        hit = 0
        for f in funcs:
            b = sizes.get(f["name"])
            if b is not None:
                f["delta"] = b - f["size"]
                hit += 1
        print("[sizes] %d objs scanned, %d/%d functions resolved on the base side"
              % (nobj, hit, len(funcs)), file=sys.stderr)

    axes = ["pct", "score_shape", "delta_shape"] if args.axis == "all" else [args.axis]
    if any(a.startswith("delta") for a in axes) and not args.with_sizes:
        if args.axis == "all":
            axes = [a for a in axes if not a.startswith("delta")]
        else:
            sys.exit("--axis delta requires --with-sizes")

    payload = {}
    for axis in axes:
        clusters = build_clusters(funcs, axis, args.min_members, args.min_units)
        clustered = sum(c["n"] for c in clusters)
        print()
        print("#" * 78)
        print("# AXIS = %s   pool=%d sub-100 functions%s"
              % (axis, len(funcs), "" if args.include_anon else " (named only)"))
        print("# clusters (>=%d members, >=%d units): %d, covering %d functions (%.1f%% of pool)"
              % (args.min_members, args.min_units, len(clusters), clustered,
                 100.0 * clustered / max(1, len(funcs))))
        dist = collections.Counter()
        for c in clusters:
            if c["n"] >= 20:
                dist[">=20"] += 1
            elif c["n"] >= 10:
                dist["10-19"] += 1
            elif c["n"] >= 5:
                dist["5-9"] += 1
            else:
                dist["3-4"] += 1
        print("# size distribution: %s" % dict(dist))
        print("#" * 78)
        if not args.census:
            for c in clusters[:args.top]:
                print(fmt_cluster(c, at100, args.siblings, args.verbose))
        payload[axis] = [dict(key=(list(c["key"]) if isinstance(c["key"], tuple) else c["key"]), n=c["n"], nunits=c["nunits"],
                              rank=c["rank"], mean_size=c["mean_size"],
                              shapes=dict(c["shapes"]),
                              members=[dict(unit=m["unit"], name=m["name"], pct=m["pct"],
                                            size=m["size"], score=m["score"], nins=m["nins"],
                                            delta=m.get("delta"))
                                       for m in c["members"]])
                         for c in clusters]

    if args.json:
        Path(args.json).write_text(json.dumps(payload, indent=1))
        print("\nwrote %s" % args.json, file=sys.stderr)


if __name__ == "__main__":
    main()
