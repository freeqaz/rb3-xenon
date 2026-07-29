#!/usr/bin/env python3
"""eh_ground_truth.py -- retail's per-function EXCEPTION-HANDLER truth, cheaply.

WHAT IT ANSWERS
    "Does the retail binary have an EH frame for this function, and do we?"
    Per function, without Ghidra, without a diff, without a build.

METHOD
    TARGET (retail) side.  dtk emits the retail `.pdata` verbatim into
    `build/45410914/asm/<Unit>.s` as pairs of words:

        .obj "pdata@8223B780", global
            .4byte fn_8274AA08
            .4byte 0xC0001F04
        .endobj "pdata@8223B780"

    The second word is the X360 PowerPC RUNTIME_FUNCTION packed field:

        bits  0..7   PrologLen     (instructions)
        bits  8..29  FunctionLen   (instructions; x4 = size in bytes)
        bit   30     ThirtyTwoBit
        bit   31     ExceptionFlag <-- retail has an EH frame for this function

    So `0xC0001F04` = EH present, `0x40000A04` = no EH.  The FunctionLen decode
    was validated against objdiff's own target sizes: 25/25 exact on
    default/DataNode, which is what certifies the bit layout.

    The `fn_<addr>` label is joined to a mangled name through
    `scripts/target_symbol_map.json`.

    BASE (our build) side.  Our compiled obj's COFF symbol table carries
    `__unwindtable$<mangled>` for exactly the functions MSVC gave an EH frame.
    Presence/absence there is our side of the answer.

    Cross-product per function:
        BOTH        retail EH, we emit EH                      (agreement)
        NEITHER     retail no EH, we emit none                 (agreement)
        EH_DELETED  retail EH, we emit NONE  <-- the interesting class
        EH_ADDED    retail no EH, we emit one

USES
    - Certifying whether a sub-100 function's residual is EH-shaped at all
      before spending a lane on it (an `EH_DELETED` verdict is a prerequisite
      for any funclet/nothrow theory; `BOTH` refutes it outright).
    - Funclet/parent pairing and at_limit certification.
    - Screening a codegen hypothesis across the whole tree in ~15 minutes, with
      a control group, instead of extrapolating from one site.

KNOWN LIMITS -- read before trusting a row
    1. ICF-FOLDED TARGETS.  One retail function can be the pair partner of many
       different base symbols after identical-COMDAT folding, and every one of
       those pairings then reports the same target `.pdata` word.  This
       manufactures a cluster of identical-size false `EH_DELETED` hits.
       Measured: 12 of 37 scatter-unit `EH_DELETED` rows shared target size 316
       -- a single folded blob mapped onto 12 base symbols.  `--census` flags
       any target size recurring >= ICF_SUSPECT_MIN times as `ICF-SUSPECT` and
       reports the tight count with them removed.  A single row is not evidence;
       check the size histogram.
    2. UNMAPPED TARGETS.  A `fn_<addr>` with no `target_symbol_map.json` entry
       cannot be joined to a base symbol and is skipped (counted as `unmapped`).
       Roughly a third of EH-bearing target functions are in this state, so the
       denominator here is "mapped EH-bearing", never "all functions".
    3. `__unwindtable$` ABSENCE IS NOT ALWAYS EH ABSENCE.  A handful of forms
       express EH through `__ehfuncinfo$` alone.  Verified false-positive rate is
       low but nonzero -- confirm a single-row finding with an actual diff
       (target larger than base AND target-only `delete` instructions).
    4. A missing EH frame is far more often a symptom of an unported body or a
       map mispair than of a codegen subtlety.  Rank by match% and size ratio.

STALENESS GUARD
    `build/45410914/asm/` is append-only: ninja never deletes the `.s` of a unit
    whose split moved or was renamed.  Measured 2026-07-29: ~8,939 of 12,730
    `.s` files on disk are stale orphans, ~2,439 of them predating the TU5 flip
    and containing bytes for a DIFFERENT binary revision.  Reading those
    silently is how two prior analyses were corrupted.  This tool therefore
    joins through `scripts/harvest/live_units.py` (objdiff.json membership,
    which tracks current splits.txt) and:
        --unit / --symbol on an orphan  -> hard ERROR, exit 2, nothing read
        --census                        -> iterates objdiff.json's live units
                                           ONLY, and opens with a loud banner
                                           counting the orphans it refused to
                                           read (9,024 of 12,952 as of
                                           2026-07-29 on a warm worktree)
    `--allow-stale` overrides.  On an orphan it reports the retail side only,
    with a per-invocation warning, because there is no live base obj to
    compare against.

USAGE
    scripts/harvest/eh_ground_truth.py --unit default/DataNode
    scripts/harvest/eh_ground_truth.py --symbol '?Insert@DataArray@@QAAXHABVDataNode@@@Z'
    scripts/harvest/eh_ground_truth.py --census
    scripts/harvest/eh_ground_truth.py --census --scatter-split --json out.json
    scripts/harvest/eh_ground_truth.py --census --project ~/tmp/wt-laneXX-1
"""
import argparse
import collections
import glob
import json
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from live_units import live_unit_names  # noqa: E402

ICF_SUSPECT_MIN = 3

# ".4byte fn_XXXXXXXX" immediately followed by ".4byte 0xPPPPPPPP" == one
# retail RUNTIME_FUNCTION.  Anchored to the pair so ordinary code relocations
# that happen to name an fn_ label are never mistaken for a .pdata entry.
PDATA_RE = re.compile(r"^\t\.4byte (fn_[0-9A-Fa-f]{8})\n\t\.4byte 0x([0-9A-Fa-f]{8})", re.M)


# --------------------------------------------------------------------------
# COFF
# --------------------------------------------------------------------------
def coff_symbol_names(path):
    """Every symbol name in a COFF obj (PE/COFF, X360 PPC).  Long names live in
    the string table at psym + nsym*18."""
    with open(path, "rb") as f:
        d = f.read()
    if len(d) < 20:
        return set()
    _mach, _nsec, _ts, psym, nsym, _osz, _ch = struct.unpack_from("<HHIIIHH", d, 0)
    if not psym or not nsym:
        return set()
    strtab = psym + nsym * 18
    out = set()
    i = 0
    while i < nsym:
        o = psym + i * 18
        raw = d[o:o + 8]
        if raw[:4] == b"\0\0\0\0":
            off = struct.unpack_from("<I", raw, 4)[0]
            e = d.index(b"\0", strtab + off)
            name = d[strtab + off:e].decode("utf8", "replace")
        else:
            name = raw.rstrip(b"\0").decode("utf8", "replace")
        _val, _sec, _typ, _cls, naux = struct.unpack_from("<IhHBB", d, o + 8)
        out.add(name)
        i += 1 + naux
    return out


def base_eh_symbols(obj_path):
    """Mangled names for which OUR build emitted an EH frame."""
    pre = "__unwindtable$"
    return {n[len(pre):] for n in coff_symbol_names(obj_path) if n.startswith(pre)}


# --------------------------------------------------------------------------
# .pdata
# --------------------------------------------------------------------------
def decode_pdata_word(v):
    return dict(
        prolog_len=v & 0xFF,
        size=((v >> 8) & 0x3FFFFF) * 4,
        thirty_two_bit=bool((v >> 30) & 1),
        retail_eh=bool((v >> 31) & 1),
    )


def target_pdata(asm_path):
    """{fn_label: decoded} for every retail RUNTIME_FUNCTION in a unit's .s."""
    with open(asm_path, encoding="utf8", errors="replace") as f:
        txt = f.read()
    return {m.group(1): decode_pdata_word(int(m.group(2), 16)) for m in PDATA_RE.finditer(txt)}


# --------------------------------------------------------------------------
# project wiring
# --------------------------------------------------------------------------
class Project:
    def __init__(self, root, allow_stale=False):
        self.root = os.path.abspath(root)
        self.allow_stale = allow_stale
        self.build = os.path.join(self.root, "build", "45410914")
        self.asm_dir = os.path.join(self.build, "asm")
        with open(os.path.join(self.root, "objdiff.json")) as f:
            od = json.load(f)
        self.units = {u["name"]: u for u in od.get("units", [])}
        self.live = live_unit_names(self.root)
        with open(os.path.join(self.root, "scripts", "target_symbol_map.json")) as f:
            self.tmap = {k.lower(): v for k, v in json.load(f).items()}
        self.report = None
        rp = os.path.join(self.build, "report.json")
        if os.path.exists(rp):
            with open(rp) as f:
                rep = json.load(f)
            self.report = {}
            for u in rep.get("units", []):
                for fn in u.get("functions", []):
                    self.report[(u["name"], fn["name"])] = fn.get("match_percent_normalized")

    def stem(self, unit_name):
        u = self.units.get(unit_name)
        if not u:
            return None
        return os.path.splitext(os.path.basename(u["target_path"]))[0]

    def is_live(self, unit_name):
        s = self.stem(unit_name)
        return s is not None and s in self.live

    def paths(self, unit_name):
        u = self.units[unit_name]
        asm = os.path.join(self.asm_dir, self.stem(unit_name) + ".s")
        bp = u.get("base_path")  # target-only pins have no compiled counterpart
        base = os.path.join(self.root, bp) if bp else None
        return asm, base

    def mangled(self, fn_label):
        return self.tmap.get("0x" + fn_label[3:].lower())

    def rows(self, unit_name):
        """Per-function EH rows for one unit.  Raises on a stale unit unless
        --allow-stale."""
        if unit_name not in self.units:
            raise KeyError("unit not in objdiff.json: %s" % unit_name)
        if not self.is_live(unit_name):
            msg = ("unit %r is STALE (not in objdiff.json's live set -- its split "
                   "moved or was removed; its .s may hold pre-TU5 bytes)" % unit_name)
            if not self.allow_stale:
                raise RuntimeError(msg)
            print("WARNING: %s" % msg, file=sys.stderr)
        asm, base = self.paths(unit_name)
        if not os.path.exists(asm):
            return [], "no-asm"
        have_base = bool(base) and os.path.exists(base)
        ours = base_eh_symbols(base) if have_base else set()
        out = []
        for fn, dec in sorted(target_pdata(asm).items()):
            mang = self.mangled(fn)
            row = dict(unit=unit_name, fn=fn, symbol=mang, size=dec["size"],
                       retail_eh=dec["retail_eh"], base_eh=None, verdict=None,
                       pct=None, stale=not self.is_live(unit_name))
            if mang is None:
                row["verdict"] = "UNMAPPED"
            elif not have_base:
                row["verdict"] = "NO_BASE_OBJ"
            else:
                row["base_eh"] = mang in ours
                row["verdict"] = {
                    (True, True): "BOTH",
                    (True, False): "EH_DELETED",
                    (False, True): "EH_ADDED",
                    (False, False): "NEITHER",
                }[(row["retail_eh"], row["base_eh"])]
                if self.report:
                    row["pct"] = self.report.get((unit_name, mang))
            out.append(row)
        return out, None


# --------------------------------------------------------------------------
# scatter classification (the control-group split that made this decisive)
# --------------------------------------------------------------------------
SCATTER_RE = re.compile(r'^\s*#include\s+"([^"]+\.cpp)"', re.M)


def scatter_sources(root):
    out = set()
    for pat in ("src/**/*.cpp", "src/**/*.h"):
        for f in glob.glob(os.path.join(root, pat), recursive=True):
            with open(f, encoding="utf8", errors="replace") as fh:
                if SCATTER_RE.search(fh.read()):
                    out.add(os.path.relpath(f, root))
    return out


# --------------------------------------------------------------------------
# output
# --------------------------------------------------------------------------
def print_rows(rows, only=None):
    shown = [r for r in rows if only is None or r["verdict"] in only]
    if not shown:
        print("(no rows)")
        return
    print("%-11s %-6s %-6s %6s %7s  %s" % ("VERDICT", "RETAIL", "OURS", "SIZE", "MATCH%", "SYMBOL"))
    for r in shown:
        pct = "" if r["pct"] is None else "%6.1f" % r["pct"]
        print("%-11s %-6s %-6s %6d %7s  %s"
              % (r["verdict"],
                 "EH" if r["retail_eh"] else "-",
                 ("EH" if r["base_eh"] else "-") if r["base_eh"] is not None else "?",
                 r["size"], pct, r["symbol"] or r["fn"]))


def census(proj, scatter_split=False):
    scat_srcs = scatter_sources(proj.root) if scatter_split else set()
    groups = collections.defaultdict(lambda: dict(
        units=0, entries=0, retail_eh=0, mapped_eh=0, deleted=0, added=0, rows=[]))
    stale_units = []
    skipped_no_asm = 0
    for name in sorted(proj.units):
        if not proj.is_live(name):
            stale_units.append(name)
            if not proj.allow_stale:
                continue
        try:
            rows, why = proj.rows(name)
        except RuntimeError:
            continue
        if why == "no-asm":
            skipped_no_asm += 1
            continue
        grp = "all"
        if scatter_split:
            sp = proj.units[name].get("metadata", {}).get("source_path")
            grp = "scatter" if sp in scat_srcs else "plain"
        g = groups[grp]
        g["units"] += 1
        for r in rows:
            g["entries"] += 1
            if not r["retail_eh"]:
                if r["verdict"] == "EH_ADDED":
                    g["added"] += 1
                continue
            g["retail_eh"] += 1
            if r["verdict"] in ("UNMAPPED", "NO_BASE_OBJ"):
                continue
            g["mapped_eh"] += 1
            if r["verdict"] == "EH_DELETED":
                g["deleted"] += 1
                g["rows"].append(r)

    orphans = []
    for f in glob.glob(os.path.join(proj.asm_dir, "**", "*.s"), recursive=True):
        if os.path.splitext(os.path.basename(f))[0] not in proj.live:
            orphans.append(f)
    if orphans:
        print("=" * 72)
        print("STALE GUARD: %d of %d .s files under build/45410914/asm/ are ORPHANS"
              % (len(orphans), len(orphans) + sum(1 for _ in glob.iglob(
                  os.path.join(proj.asm_dir, "**", "*.s"), recursive=True)) - len(orphans)))
        print("             (rel-path not in objdiff.json's live set -- their split moved,")
        print("             was renamed, or predates the TU5 flip).  NOT READ.")
        print("=" * 72)

    if stale_units:
        print("=" * 72)
        print("STALE: %d of %d objdiff units are NOT in the live set%s"
              % (len(stale_units), len(proj.units),
                 " -- INCLUDED because --allow-stale" if proj.allow_stale
                 else " -- SKIPPED (use --allow-stale to include)"))
        print("       stale .s files may hold pre-TU5 bytes for a different binary revision")
        print("=" * 72)
    if skipped_no_asm:
        print("note: %d live units have no .s on disk (never split) -- skipped" % skipped_no_asm)

    print()
    hdr = "%-9s %6s %9s %10s %11s %9s %7s %7s" % (
        "GROUP", "UNITS", "PDATA", "RETAIL_EH", "MAPPED_EH", "DELETED", "RATE", "TIGHT")
    print(hdr)
    print("-" * len(hdr))
    for grp in sorted(groups):
        g = groups[grp]
        sizes = collections.Counter(r["size"] for r in g["rows"])
        icf = {s for s, n in sizes.items() if n >= ICF_SUSPECT_MIN}
        tight = [r for r in g["rows"] if r["size"] not in icf]
        rate = (100.0 * g["deleted"] / g["mapped_eh"]) if g["mapped_eh"] else 0.0
        print("%-9s %6d %9d %10d %11d %9d %6.2f%% %7d"
              % (grp, g["units"], g["entries"], g["retail_eh"], g["mapped_eh"],
                 g["deleted"], rate, len(tight)))
        g["icf_sizes"] = sorted(icf)
    print()
    print("RATE  = EH_DELETED / MAPPED_EH.  TIGHT = EH_DELETED minus ICF-SUSPECT rows")
    print("        (target sizes recurring >=%d times = one ICF-folded target paired" % ICF_SUSPECT_MIN)
    print("        onto many base symbols; see KNOWN LIMITS #1).")
    for grp in sorted(groups):
        if groups[grp]["icf_sizes"]:
            print("        %-8s ICF-SUSPECT target sizes: %s"
                  % (grp, ", ".join(str(s) for s in groups[grp]["icf_sizes"])))
    if scatter_split and "scatter" in groups and "plain" in groups:
        print()
        print("★ CONTROL GROUP: if a hypothesis blames scatter-composition, the scatter")
        print("  RATE must EXCEED plain.  A scatter rate at or below plain refutes it.")
    return groups


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--project", default=os.getcwd(),
                    help="repo/worktree root (default: cwd)")
    ap.add_argument("--unit", help="objdiff unit name, e.g. default/DataNode")
    ap.add_argument("--symbol", help="mangled symbol; searched across live units")
    ap.add_argument("--census", action="store_true", help="tree-wide EH table")
    ap.add_argument("--scatter-split", action="store_true",
                    help="partition --census into scatter-composed vs plain (control group)")
    ap.add_argument("--only", help="comma-separated verdicts to print "
                                   "(BOTH,NEITHER,EH_DELETED,EH_ADDED,UNMAPPED)")
    ap.add_argument("--allow-stale", action="store_true",
                    help="read units absent from the live set (DANGEROUS: pre-TU5 bytes)")
    ap.add_argument("--json", help="also dump rows/summary as JSON to this path")
    a = ap.parse_args()

    proj = Project(a.project, allow_stale=a.allow_stale)
    only = set(a.only.split(",")) if a.only else None

    if a.census:
        g = census(proj, scatter_split=a.scatter_split)
        if a.json:
            with open(a.json, "w") as f:
                json.dump({k: {kk: vv for kk, vv in v.items()} for k, v in g.items()}, f, indent=1)
            print("\nwrote %s" % a.json)
        return 0

    if a.unit:
        unit = a.unit
        if unit not in proj.units:
            byname = {proj.stem(n): n for n in proj.units}
            if unit in byname:
                unit = byname[unit]
            else:
                orphan = os.path.join(proj.asm_dir, unit + ".s")
                if os.path.exists(orphan):
                    if not a.allow_stale:
                        print("ERROR: %r has a .s on disk but is NOT in objdiff.json's "
                              "live set -- it is a STALE ORPHAN from a previous split "
                              "layout and may contain bytes for a different binary "
                              "revision. Refusing to read it (--allow-stale to override)."
                              % unit, file=sys.stderr)
                        return 2
                    print("WARNING: %r is a STALE ORPHAN; retail side only, no live base "
                          "obj to compare against. Bytes may be from a different binary "
                          "revision." % unit, file=sys.stderr)
                    rows = []
                    for fn, dec in sorted(target_pdata(orphan).items()):
                        rows.append(dict(unit=unit, fn=fn, symbol=proj.mangled(fn),
                                         size=dec["size"], retail_eh=dec["retail_eh"],
                                         base_eh=None, verdict="NO_BASE_OBJ", pct=None,
                                         stale=True))
                    print("# %s  (STALE ORPHAN, %d retail .pdata entries)" % (unit, len(rows)))
                    print_rows(rows, only)
                    return 0
        try:
            rows, why = proj.rows(unit)
        except (KeyError, RuntimeError) as e:
            print("ERROR: %s" % e, file=sys.stderr)
            return 2
        if why == "no-asm":
            print("ERROR: no .s on disk for %s (unit never split)" % unit, file=sys.stderr)
            return 2
        print("# %s  (%d retail .pdata entries)" % (unit, len(rows)))
        print_rows(rows, only)
        if a.json:
            with open(a.json, "w") as f:
                json.dump(rows, f, indent=1)
        return 0

    if a.symbol:
        # Reverse the symbol map first so we only have to open the .s files that
        # actually mention one of this symbol's fn_ labels.
        labels = {"fn_" + k[2:].upper() for k, v in proj.tmap.items() if v == a.symbol}
        if not labels:
            print("symbol absent from scripts/target_symbol_map.json: %s" % a.symbol,
                  file=sys.stderr)
            return 1
        hits = []
        for name in sorted(proj.units):
            if not proj.is_live(name):
                continue
            asm, _ = proj.paths(name)
            if not os.path.exists(asm):
                continue
            with open(asm, encoding="utf8", errors="replace") as fh:
                txt = fh.read()
            if not any(lb in txt for lb in labels):
                continue
            rows, why = proj.rows(name)
            hits.extend(r for r in rows if r["symbol"] == a.symbol)
        if not hits:
            print("symbol not found in any LIVE unit's retail .pdata: %s" % a.symbol,
                  file=sys.stderr)
            print("(a function with no .pdata entry has no retail EH frame and no "
                  "unwind data at all)", file=sys.stderr)
            return 1
        for r in hits:
            print("# %s" % r["unit"])
        print_rows(hits, only)
        if len(hits) > 1:
            print("\nNOTE: %d units claim this symbol -- likely an ICF fold or a map "
                  "mispair; see KNOWN LIMITS #1." % len(hits))
        if a.json:
            with open(a.json, "w") as f:
                json.dump(hits, f, indent=1)
        return 0

    ap.print_help()
    return 1


if __name__ == "__main__":
    sys.exit(main())
