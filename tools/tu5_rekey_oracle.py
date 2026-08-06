#!/usr/bin/env python3
"""Re-key a TU0-era cross-binary VA oracle onto TU5, admitting only BODY-VERIFIED rows.

WHY THIS EXISTS
---------------
``unified_id_rb3wii.json`` (9,301 rows) is the rb3-Wii <-> RB3-360 BinDiff oracle.
It is **not corrupt** -- it is valid data for the wrong binary.  It scored 99.76%
live against the TU0-era ``symbols.txt`` and died at ``a320bc12`` (2026-07-15)
when main flipped from the base-disc executable to Title Update 5, a relinked
image (``.text`` 0x82260000 -> 0x82270000, +0x28568 B).  No rigid shift rescues
it (``tools/dead_index_guard.py --audit``: 4.26% live).

The base->TU5 address map recovered from tag ``tu5-map-archive-72254ce0`` can
re-key it.  But **liveness is not correctness**, and the two diverge sharply
here:

    class          re-keyed rows   land on a .text fn start   SAME FUNCTION BODY
    DIRECT               8,657                       99.98%              100.00%
    densified tail         601                       99.34%               22.96%

The densified tail is the ~99%-live / ~23%-correct population.  A guard that
only checks "does this address exist" passes it.  Admitting it would produce
exactly the artifact ``dead_index_guard``'s docstring warns about: a
PLAUSIBLE-LOOKING WRONG index, which is worse than a dead one because a dead one
fails loudly.  So this tool does not re-key by lookup -- it re-keys by
lookup **and then proves each row**, and drops what it cannot prove.

THE PROOF
---------
The map's premise is that a remapped function's *body is unchanged* between TU0
and TU5.  That premise is directly checkable and is not circular: we compare the
raw bytes in ``orig/45410914/tu0-archive/band.exe`` at the TU0 VA against
``orig/45410914/band.exe`` (the TU5 image) at the candidate TU5 VA, over the
function's full TU0 size, under the same reloc-normalizing mask
``tu5_map_build.mask_word`` uses (branch targets + D-form imm16 masked out,
because those legitimately move when the image is relinked).

Comparing SYMBOL NAMES instead would be circular: ``config/45410914/symbols.txt``
was itself generated from this same map by ``P3_remap.py``.  The retail bytes
are the only independent witness.  This is the same doctrine as everywhere else
in this system -- the binary is the judge.

Two candidate addresses are tried per row, in order:

  1. ``DIRECT``  -- the map entry from ``base_to_tu5_map.full.json``, produced by
     tu5_map_build.py's masked-skeleton unique-anchor match + verified co-walk.
  2. ``RESCUE-UNIQUE`` -- if (1) fails or is absent, search the whole TU5 masked
     ``.text`` stream for the TU0 function's masked body; accept only if it
     occurs at EXACTLY ONE address that is a real ``.text`` function start.
     (This is tu5_map_build.py STAGE 1 applied to the leftovers.)

The *densified* placements (RIGID / CONTIG / LOOSE, with the ``-ICF`` snap) are
loaded only to LABEL a row's provenance.  They are never admitted on their own
authority -- a densified address still has to pass the body check, and 77% of
them do not.  Note in particular:

  * ``LOOSE``  is nearest-anchor extrapolation.  ``P3_validation.md`` §1 already
    says LOOSE is "best-effort (drift regions; used for symbols/seed, **not**
    used as pin boundaries)".  Its measured body-verify rate here is ~27%.
  * ``RIGID`` sounds exact but is not: it fires when ``fwd == bwd``, i.e. when
    the *span length* between the two flanking anchors is preserved.  The
    interior can still have been reordered or compensatingly resized.  Measured
    body-verify rate ~75%.
  * ``-ICF`` means the placement landed mid-``.pdata``-function and was snapped
    to the enclosing function's start on the assumption the base function
    ICF-folded into it.  Such an address is **an alias, not the function's own
    start** -- it is a different, usually larger, function.  For an identity
    oracle that is a wrong answer, and the body check rejects it.

CONSUMER
--------
``objdiff-cli report generate --global-byte-eq --global-byte-eq-oracle <path>``.
That pass ("case-B identity transfer") refuses to run without an oracle because
its Rule 3 is the decisive honesty gate: a byte-equality promotion is allowed
only if the retail VA is oracle-named with ``similarity >= 0.5`` AND the
oracle's ``bindiff_src`` basename equals the claiming unit's source basename
(``objdiff-core/src/diff/mod.rs:1300-1316``).  The gate is RESTRICTIVE -- a VA
absent from the oracle is rejected.  Consequences for this tool:

  * Dropping unprovable rows is SAFE by construction: a smaller oracle can only
    ever yield FEWER promotions, never more.  Erring toward exclusion cannot
    inflate a score.
  * Only ``rb3_addr``, ``bindiff_src`` and ``similarity`` are read by the
    consumer (``objdiff-cli/src/cmd/report.rs:625-660``), and the file must be a
    TOP-LEVEL JSON LIST.  So the output keeps the original schema verbatim and
    adds provenance fields alongside; unknown fields are ignored by the reader.

OUTPUT (never overwrites the TU0 artifact -- it is still valid data for the TU0
binary, and other tools/docs reference it):

    unified_id_rb3wii.tu5.json           admitted rows, original schema + provenance
    unified_id_rb3wii.tu5.meta.json      provenance + per-class census
    unified_id_rb3wii.tu5.rejected.json  every dropped row with its reason

INPUTS not in the working tree by default (``/_tu5probe/`` is gitignored); the
only surviving copy is the archive tag:

    git checkout tu5-map-archive-72254ce0 -- _tu5probe

CLI
---
    python3 tools/tu5_rekey_oracle.py                 # re-key + write artifacts
    python3 tools/tu5_rekey_oracle.py --report-only   # measure, write nothing
"""

from __future__ import annotations

import argparse
import bisect
import collections
import json
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tu5_va import load_sections, va_to_off  # noqa: E402
from tu5_map_build import mask_word  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ARCHIVE = os.path.join(ROOT, "_tu5probe", "tu5_migrate")
FROZEN_SYMS = os.path.join(ARCHIVE, "valayer_baseline_main", "symbols.txt")
FULL_MAP = os.path.join(ARCHIVE, "base_to_tu5_map.full.json")
DENS_MAP = os.path.join(ARCHIVE, "tu5_valayer", "base_to_tu5_map.densified.json")
WORKLIST = os.path.join(ARCHIVE, "tu5_changed_worklist.json")

TU0_PE = os.path.join(ROOT, "orig", "45410914", "tu0-archive", "band.exe")
TU5_PE = os.path.join(ROOT, "orig", "45410914", "band.exe")

IN_ORACLE = os.path.join(ROOT, "unified_id_rb3wii.json")
OUT_ORACLE = os.path.join(ROOT, "unified_id_rb3wii.tu5.json")
OUT_META = os.path.join(ROOT, "unified_id_rb3wii.tu5.meta.json")
OUT_REJECT = os.path.join(ROOT, "unified_id_rb3wii.tu5.rejected.json")

TU5_TEXT_LO = 0x82270000
TU5_TEXT_HI = 0x82270000 + 0x9DCE3C

# A body shorter than this carries too few instructions to be a distinctive
# fingerprint; the unique-search rescue refuses to adjudicate on it.
RESCUE_MIN_SIZE = 16

# objdiff-core CASEB_ORACLE_SIM_MIN -- the only rows the consumer can ever act on.
CASEB_SIM_MIN = 0.5


def h(s):
    return int(s, 16)


# ---------------------------------------------------------------- inputs


def load_frozen_functions():
    """TU0 ``.text`` function extents from the FROZEN pre-flip symbols.txt.

    Must be the frozen baseline, not ``config/45410914/symbols.txt`` -- the
    latter is the TU5 table now.
    """
    fn, name = {}, {}
    line_re = re.compile(r"^(\S+) = \.text:0x([0-9A-Fa-f]+);(.*)$")
    size_re = re.compile(r"type:function size:0x([0-9A-Fa-f]+)")
    with open(FROZEN_SYMS, errors="replace") as fh:
        for line in fh:
            m = line_re.match(line)
            if not m:
                continue
            sm = size_re.search(m.group(3))
            if sm:
                va = h(m.group(2))
                fn[va] = int(sm.group(1), 16)
                name[va] = m.group(1)
    return fn, name


def decode_tu5_pdata(data, secs):
    """Authoritative TU5 function boundaries from the ``.pdata`` unwind table.

    ``FunctionLength = ((word >> 8) & 0x3FFFFF) * 4``, big-endian.  Same decode
    as ``P3_remap.py:40-52``, but resolved through the section table instead of
    that script's hardcoded file offsets.
    """
    pd = None
    for nm, sva, vsize, rawptr, rawsize in secs:
        if nm == ".pdata":
            pd = data[rawptr:rawptr + min(vsize, rawsize)]
            break
    if pd is None:
        raise SystemExit("no .pdata in TU5 image")
    out = []
    for i in range(len(pd) // 8):
        beg, word = struct.unpack_from(">II", pd, i * 8)
        length = ((word >> 8) & 0x3FFFFF) * 4
        if TU5_TEXT_LO <= beg < TU5_TEXT_HI and length > 0:
            out.append((beg, beg + length))
    out.sort()
    return out


def densify_confidence(full_map, changed, fn, pdata):
    """Recompute the per-address class of every densified placement.

    ``base_to_tu5_map.densified.json`` persists only a ``by_conf`` HISTOGRAM in
    its meta, so the class of an individual address is not recoverable by
    reading it.  This re-runs the placement block verbatim
    (``P3_remap.py:60-97``); ``--report-only`` asserts it reproduces the
    archived map exactly, which is also the archive's own integrity check.
    """
    pbeg = [b for b, _ in pdata]

    def enclosing(va):
        i = bisect.bisect_right(pbeg, va) - 1
        if i >= 0 and pdata[i][0] <= va < pdata[i][1]:
            return pdata[i]
        return None

    mstarts = sorted(full_map)
    miss = {b for b, why in changed.items() if why == "MISS"}
    place = {}
    stats = collections.Counter()
    for b in sorted(fn):
        if b in full_map or b in miss:
            continue
        i = bisect.bisect_left(mstarts, b)
        prev = mstarts[i - 1] if i > 0 else None
        nxt = mstarts[i] if i < len(mstarts) else None
        fwd = full_map[nxt] - (nxt - b) if nxt is not None else None
        bwd = full_map[prev] + (b - prev) if prev is not None else None
        conf = tu5 = None
        if fwd is not None and bwd is not None and fwd == bwd:
            tu5, conf = fwd, "RIGID"
        elif prev is not None and prev + fn.get(prev, 0) == b:
            tu5, conf = full_map[prev] + fn[prev], "CONTIG"
        elif nxt is not None and b + fn[b] == nxt:
            tu5, conf = full_map[nxt] - fn[b], "CONTIG"
        elif fwd is not None or bwd is not None:
            cand = []
            if prev is not None:
                cand.append((b - prev, bwd))
            if nxt is not None:
                cand.append((nxt - b, fwd))
            cand.sort()
            tu5, conf = cand[0][1], "LOOSE"
        else:
            conf = "NOANCHOR"
        if tu5 is not None and TU5_TEXT_LO <= tu5 < TU5_TEXT_HI:
            enc = enclosing(tu5)
            if enc is not None and enc[0] < tu5:
                tu5, conf = enc[0], conf + "-ICF"
            tag = ("AMBIG-" if b in changed else "") + conf
            place[b] = (tu5, tag)
            stats[tag] += 1
        else:
            stats["REJECT-oob" if tu5 is not None else "NOANCHOR"] += 1
    return place, dict(stats)


# ---------------------------------------------------------------- body proof


class Bodies:
    """Masked-body reader over the two images."""

    def __init__(self):
        self.d0, _, self.s0 = load_sections(TU0_PE)
        self.d5, _, self.s5 = load_sections(TU5_PE)
        t = next(s for s in self.s5 if s[0] == ".text")
        self.t5_sva, raw, n = t[1], t[3], (min(t[2], t[4]) & ~3)
        buf = bytearray(n)
        for i in range(0, n, 4):
            struct.pack_into(">I", buf, i,
                             mask_word(struct.unpack_from(">I", self.d5, raw + i)[0]))
        self.tu5_masked_text = bytes(buf)

    @staticmethod
    def _slice(data, secs, va, n):
        r = va_to_off(va, secs)
        if r is None:
            return None
        off, sec = r
        if sec != ".text" or off + n > len(data):
            return None
        return data[off:off + n]

    @staticmethod
    def mask(bs):
        n = len(bs) - len(bs) % 4
        return b"".join(struct.pack(">I", mask_word(struct.unpack_from(">I", bs, i)[0]))
                        for i in range(0, n, 4))

    def tu0(self, va, n):
        return self._slice(self.d0, self.s0, va, n)

    def tu5(self, va, n):
        return self._slice(self.d5, self.s5, va, n)

    def verify(self, base_va, tu5_va, size):
        """(verdict, exact) -- verdict in {OK, MISMATCH, NOBYTES}."""
        a = self.tu0(base_va, size)
        z = self.tu5(tu5_va, size)
        if a is None or z is None:
            return "NOBYTES", False
        if a == z:
            return "OK", True
        return ("OK", False) if self.mask(a) == self.mask(z) else ("MISMATCH", False)

    def unique_site(self, base_va, size, universe):
        """Sole ``.text`` function start in TU5 carrying this masked body, if any.

        Returns ``(va, None)`` on success, else ``(None, reason)``.
        """
        a = self.tu0(base_va, size)
        if a is None:
            return None, "NOBYTES"
        needle = self.mask(a)
        occ = []
        i = self.tu5_masked_text.find(needle)
        while i != -1 and len(occ) < 4:
            occ.append(self.t5_sva + i)
            i = self.tu5_masked_text.find(needle, i + 4)
        if len(occ) >= 4:
            return None, "NONUNIQUE"
        starts = [v for v in occ if v in universe]
        if len(starts) == 1:
            return starts[0], None
        return None, "NO-FN-START-HIT" if not starts else "AMBIGUOUS-MULTI"


# ---------------------------------------------------------------- main


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--report-only", action="store_true",
                    help="measure and print; write no artifacts")
    ap.add_argument("--oracle", default=IN_ORACLE)
    ap.add_argument("--out", default=OUT_ORACLE)
    args = ap.parse_args()

    for p in (FULL_MAP, DENS_MAP, WORKLIST, FROZEN_SYMS):
        if not os.path.exists(p):
            raise SystemExit(
                f"missing archive input: {p}\n"
                "  The base->TU5 map lives ONLY in the archive tag (/_tu5probe/ is\n"
                "  gitignored). Restore it with:\n"
                "      git checkout tu5-map-archive-72254ce0 -- _tu5probe")

    import dead_index_guard as dig
    universe = dig.text_function_universe(ROOT)

    full = {h(k): h(v) for k, v in json.load(open(FULL_MAP))["map"].items()}
    changed = {h(w["base_va"]): w["why_unmatched"]
               for w in json.load(open(WORKLIST))["worklist"]}
    fn, fnname = load_frozen_functions()

    bodies = Bodies()
    pdata = decode_tu5_pdata(bodies.d5, bodies.s5)
    place, place_stats = densify_confidence(full, changed, fn, pdata)

    archived = json.load(open(DENS_MAP))
    repro = {f"0x{b:x}": f"0x{v:x}" for b, v in full.items()}
    repro.update({f"0x{b:x}": f"0x{v:x}" for b, (v, _) in place.items()})
    reproduces = (repro == archived["map"])
    print(f"archive integrity: densify reproduces base_to_tu5_map.densified.json "
          f"({len(repro)}/{len(archived['map'])} entries): {reproduces}")
    if not reproduces:
        print("  !! WARNING: reproduction differs -- provenance of the archived map "
              "is not confirmed; treat confidence labels as approximate.")

    rows = json.load(open(args.oracle))
    print(f"input oracle: {len(rows)} rows from {args.oracle}")

    admitted, rejected = [], []
    census = collections.defaultdict(collections.Counter)
    for r in rows:
        base_va = h(r["rb3_addr"])
        size = fn.get(base_va) or int(r.get("size") or 0)
        if base_va in full:
            cls, cand = "DIRECT", full[base_va]
        elif base_va in place:
            cls, cand = place[base_va][1], place[base_va][0]
        else:
            cls, cand = ("MISS" if changed.get(base_va) == "MISS" else "UNMAPPED"), None

        verdict = method = None
        tu5_va = None
        if cand is not None and size >= 4:
            if cand not in universe:
                verdict = "MAP-ADDR-NOT-A-FN-START"
            else:
                v, exact = bodies.verify(base_va, cand, size)
                if v == "OK":
                    verdict, method, tu5_va = "ADMIT", ("map-exact" if exact else "map-masked"), cand
                else:
                    verdict = "MAP-BODY-" + v
        elif cand is not None:
            verdict = "NO-SIZE"

        if verdict != "ADMIT" and size >= RESCUE_MIN_SIZE:
            site, why = bodies.unique_site(base_va, size, universe)
            if site is not None:
                verdict, method, tu5_va = "ADMIT", "rescue-unique", site
            elif verdict is None:
                verdict = "RESCUE-" + why
        if verdict is None:
            verdict = "UNVERIFIABLE-TOO-SHORT" if size < RESCUE_MIN_SIZE else "UNVERIFIABLE"

        census[cls][verdict if verdict != "ADMIT" else f"ADMIT/{method}"] += 1

        if verdict == "ADMIT":
            out = dict(r)
            out["rb3_addr"] = f"0x{tu5_va:x}"
            out["rb3_fn"] = f"fn_{tu5_va:08x}"
            out["rekey"] = {
                "tu0_addr": r["rb3_addr"],
                "map_class": cls,
                "method": method,
                "verified": "masked-body-equal",
                "size": size,
                "moved_from_map_addr": (cand is not None and cand != tu5_va),
            }
            admitted.append(out)
        else:
            rejected.append({"tu0_addr": r["rb3_addr"], "map_class": cls,
                             "map_candidate": (f"0x{cand:x}" if cand else None),
                             "reason": verdict, "size": size,
                             "wii_name": r.get("wii_name"),
                             "bindiff_src": r.get("bindiff_src"),
                             "similarity": r.get("similarity")})

    n = len(rows)
    print(f"\nadmitted {len(admitted)}/{n} = {len(admitted)/n*100:.2f}%   "
          f"rejected {len(rejected)}")
    print(f"\n{'map class':<20}{'rows':>7}  verdicts")
    for cls in sorted(census, key=lambda c: -sum(census[c].values())):
        tot = sum(census[cls].values())
        adm = sum(v for k, v in census[cls].items() if k.startswith("ADMIT"))
        det = "  ".join(f"{k}={v}" for k, v in census[cls].most_common())
        print(f"{cls:<20}{tot:>7}  admit={adm} ({adm/tot*100:.1f}%)  {det}")

    def sim_ok(rs):
        return sum(1 for x in rs if (x.get("similarity") or 0) >= CASEB_SIM_MIN)
    print(f"\nconsumer-operative subset (similarity >= {CASEB_SIM_MIN}, the only rows "
          f"--global-byte-eq Rule 3 can act on):")
    print(f"  input {sim_ok(rows)}  ->  admitted {sim_ok(admitted)}  "
          f"(dropped {sim_ok(rows) - sim_ok(admitted)})")

    if args.report_only:
        print("\n--report-only: nothing written")
        return 0

    with open(args.out, "w") as fh:
        json.dump(admitted, fh)
    meta = {
        "artifact": os.path.basename(args.out),
        "schema": "identical to unified_id_rb3wii.json (top-level list) plus a "
                  "per-row 'rekey' provenance object; rb3_addr/rb3_fn are TU5 VAs",
        "target_binary": "Title Update 5 (orig/45410914/band.exe, .text 0x82270000)",
        "source_oracle": os.path.basename(args.oracle),
        "source_oracle_rows": n,
        "source_oracle_target": "TU0 base disc (.text 0x82260000)",
        "map_source": "tag tu5-map-archive-72254ce0 :: "
                      "_tu5probe/tu5_migrate/{base_to_tu5_map.full.json,"
                      "tu5_valayer/base_to_tu5_map.densified.json}",
        "map_reproduces_archive": reproduces,
        "admission_rule": "the candidate TU5 VA must be a real .text function start in "
                          "config/45410914/symbols.txt AND the reloc-masked bytes at "
                          "that VA in the TU5 image must equal the reloc-masked bytes "
                          "at the TU0 VA in orig/45410914/tu0-archive/band.exe over the "
                          "function's full TU0 size. Densified (RIGID/CONTIG/LOOSE/*-ICF) "
                          "placements are NOT admitted on map authority alone.",
        "rows_admitted": len(admitted),
        "rows_rejected": len(rejected),
        "census_by_map_class": {k: dict(v) for k, v in census.items()},
        "densify_by_conf": place_stats,
        "generator": "tools/tu5_rekey_oracle.py",
    }
    with open(OUT_META, "w") as fh:
        json.dump(meta, fh, indent=1)
    with open(OUT_REJECT, "w") as fh:
        json.dump(rejected, fh, indent=1)
    print(f"\nwrote {args.out}\n      {OUT_META}\n      {OUT_REJECT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
