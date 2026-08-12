#!/usr/bin/env python3
"""Close the ICF alias set under the fold relation, and supply candidates from the
CHARGE LIST rather than from the site census.

Two gaps in `tools/icf_alias_build.py`, both in candidate supply / adjudicator
completeness rather than in gate strictness.  Nothing here weakens a gate.

1. T1 IS NOT ITERATED, BUT /OPT:ICF IS A FIXPOINT
   ------------------------------------------------
   `icf_fold_evidence.py` already says it, for the T2 side: "Target classes are
   refined ITERATIVELY to a fixpoint, which is exactly what a real ICF pass does
   (a fold can enable another fold)."  `icf_alias_build`'s T1 adjudicator does
   not iterate.  It compares relocation TARGET NAMES literally, so a pair of
   template twins whose only discriminator is a callee we have ALREADY PROVEN is
   one folded body is refused as `reject_RELOC_TARGETS_DIFFER`:

       retail  ?_M_create_node@list<Plane>...            calls  ??2@YAPAXI@Z
       ours    ?_M_create_node@list<AccomplishmentCond>  calls  ??2CriticalSection@@SAPAXI@Z

   Those two callees are group #2 of the landed alias set -- one address.  So
   every relocation in the two bodies resolves to the same address, the masked
   bytes and the (offset, type) sequence are identical, and the linker folds
   them.  That is the /OPT:ICF condition itself, applied one level up.

   So: relax the NAME comparison in `relocs_agree` to equality-under-the-current-
   alias-equivalence, and iterate.  Round 0's equivalence is the audited, landed
   `scripts/symbol_aliases.json`.  Byte identity, size identity and the full
   (offset, reloc_type) sequence are still required exactly as before, and the
   CD-9 strict-placeholder refutation is still applied.  Only names we have
   already proven share an address are treated as equal.

   ⚠ THIS IS THE DECOY POPULATION.  `tools/icf_decoy_control.py` defines its
   negative control as "passes masked bytes, relocations resolve to DIFFERENT
   BOTH-NAMED symbols", which is precisely the shape being relaxed.  The control
   stays valid only because the relaxation is not "different names are fine" but
   "these two specific names are one proven address".  `--decoy` re-runs that
   control WITH the relaxation applied and reports the rejection rate; a
   relaxation that makes the decoy control inert is a relaxation that has become
   an over-merge.  Precision is also bounded by round 0: one bad landed group
   propagates, so `--max-rounds` is capped and each round is reported separately.

2. THE SURVIVOR'S ADDRESS DOES NOT HAVE TO COME FROM THE MAP
   ----------------------------------------------------------
   `reject_survivor_not_mapped` is a coverage limit of `target_symbol_map.json`,
   and for a dtk placeholder the address is IN THE NAME (`vftable_8205BE8C`).
   But see `scripts/namecheck_gate_accounting.py`: of the 23,024 refused pairs,
   22,983 have an `fn_`/`lbl_` survivor that objdiff's `name_check` already
   tolerates unconditionally, so clearing them buys nothing.  Only the
   `vftable_<hex>` shape -- 41 charged pairs -- is both unmapped AND charged,
   because objdiff's `is_placeholder_symbol_name` does not cover it.  Those are
   DATA symbols, so there is no function body to adjudicate; `--vftable` emits
   them only where the vtable CONTENTS agree slot for slot (retail's function
   pointers at the placeholder address, against the map addresses of the symbols
   our COMDAT relocates to).

Output is a DELTA artifact -- only groups not already in the installed set --
written wherever `--out` says.  It never writes `scripts/symbol_aliases.json`.
Install with:

    python3 scripts/icf_alias_merge.py --into scripts/symbol_aliases.json \\
        --delta <artifact> && python3 tools/gen_symbol_alias_map.py
"""

import argparse
import collections
import json
import pickle
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from icf_alias_build import placeholder, vacuous  # noqa: E402

BUILD_ID = "45410914"
ADDR_IN_NAME = re.compile(r"^(fn|lbl|jumptable|vftable)_([0-9a-fA-F]{8})$")
# objdiff-core/src/diff/code.rs::is_placeholder_symbol_name -- the shapes objdiff
# already forgives, so an alias for them cannot move the metric.
OBJDIFF_TOLERATED = re.compile(
    r"^_?(fn_|lbl_|jumptable_|code_|data_|bss_|rdata_)[0-9a-fA-F_]+$")


class UF:
    def __init__(self):
        self.p = {}

    def find(self, x):
        self.p.setdefault(x, x)
        while self.p[x] != x:
            self.p[x] = self.p[self.p[x]]
            x = self.p[x]
        return x

    def union(self, a, b):
        ra, rb = self.find(a), self.find(b)
        if ra != rb:
            self.p[ra] = rb

    def eq(self, a, b):
        return a in self.p and b in self.p and self.find(a) == self.find(b)


def relocs_agree_eq(rt, ob, mapped, uf, tally=None, resolver=None, grounded_only=False):
    """`icf_alias_build.relocs_agree` (strict), with two changes, neither of which
    weakens a gate:

      * name equality is relaxed to equality under the PROVEN alias equivalence;
      * a retail-side `lbl_<hex>` placeholder can be RESOLVED BY CONTENT against
        the retail image instead of merely tolerated (`resolver`).

    `grounded_only=True` withdraws the remaining information-free placeholder
    tolerance entirely -- the strongest statement the evidence supports.
    Returns (agree, n_relaxed, n_tolerated)."""
    rr, orr = rt[1], ob[1]
    if len(rr) != len(orr):
        return False, 0, 0
    relaxed = tolerated = 0
    for (ro, rn, rty), (oo, on, oty) in zip(rr, orr):
        if ro != oo or rty != oty:
            return False, 0, 0
        if rn == on:
            continue
        if uf.eq(rn, on):
            relaxed += 1
            continue
        # CD-9: a retail placeholder is a REFUTATION when our side is map-resident.
        if rn.startswith(("fn_", "lbl_")) and on in mapped:
            if tally is not None:
                tally["refuted_mapped_callee_vs_placeholder"] += 1
            return False, 0, 0
        if resolver is not None:
            v = resolver(rn, on)
            if v is True:
                relaxed += 1
                continue
            if v is False:
                if tally is not None:
                    tally["refuted_by_content"] += 1
                return False, 0, 0
        if placeholder(rn) or placeholder(on):
            if grounded_only:
                return False, 0, 0
            tolerated += 1
            continue
        return False, 0, 0
    return True, relaxed, tolerated


class ContentResolver:
    """Adjudicate a `lbl_<hex>` / `fn_<hex>` retail placeholder against our data
    COMDAT by CONTENT -- the address is in the name, so the bytes are readable.

    This is the same argument as T1, one level down and on the data side: the
    condition /OPT:ICF tests is byte identity of the COMDAT, and a placeholder
    name is an absence of information, not a licence.  `icf_alias_build` merely
    TOLERATES these slots; a tolerance is what let dc3's alias wave walk a
    rename into a string literal with no ruler noticing.

    Returns True (same content), False (different content -- a REFUTATION), or
    None (cannot read one side / too little content to be worth anything).
    """

    MIN_BYTES = 4

    def __init__(self, img, our_data):
        self.img = img
        self.our = our_data
        self.stats = collections.Counter()

    def __call__(self, rn, on):
        m = ADDR_IN_NAME.match(rn)
        if not m or m.group(1) not in ("lbl", "fn"):
            return None
        rec = self.our.get(on)
        if rec is None:
            self.stats["unreadable_ours"] += 1
            return None
        raw, relocs, size = rec
        if size < self.MIN_BYTES:
            self.stats["too_small"] += 1
            return None
        va = int(m.group(2), 16)
        off = self.img.off(va)
        if off is None or off + size > len(self.img.data):
            self.stats["unreadable_retail"] += 1
            return None
        theirs = bytearray(self.img.data[off:off + size])
        mine = bytearray(raw)
        for o, _n in relocs:                     # mask our patched fields both sides
            for k in range(o, min(o + 4, size)):
                theirs[k] = mine[k] = 0
        unmasked = sum(1 for i in range(size) if mine[i] or theirs[i]) \
            if size else 0
        if size - 4 * len(relocs) < self.MIN_BYTES:
            self.stats["vacuous"] += 1
            return None
        same = bytes(mine) == bytes(theirs)
        self.stats["same" if same else "DIFFERENT"] += 1
        return same


# --------------------------------------------------------------------- PE image
class Image:
    def __init__(self, path):
        self.data = data = path.read_bytes()
        lfanew = struct.unpack_from("<I", data, 0x3C)[0]
        coff = lfanew + 4
        nsec = struct.unpack_from("<H", data, coff + 2)[0]
        optsz = struct.unpack_from("<H", data, coff + 16)[0]
        opt = coff + 20
        base = struct.unpack_from("<I", data, opt + 28)[0]
        p = opt + optsz
        self.secs = []
        for _ in range(nsec):
            vsz, va, rawsz, raw = struct.unpack_from("<IIII", data, p + 8)
            self.secs.append((base + va, raw, rawsz))
            p += 40

    def off(self, va):
        for sva, raw, rawsz in self.secs:
            if sva <= va < sva + rawsz:
                return raw + (va - sva)
        return None

    def word(self, va):
        o = self.off(va)
        return None if o is None else struct.unpack_from(">I", self.data, o)[0]


def load_bodies(cache: Path):
    """(ours, retail, referenced, target_named) over the COFF objs, via the same
    corrected reader `icf_alias_build.collect` uses."""
    if cache.exists():
        return pickle.load(cache.open("rb"))
    import glob
    from icf_fold_evidence import masked_body
    from coff_bodies_ext import function_bodies_ext
    from icf_alias_finder import coff_referenced_symbols
    sys.path.insert(0, str(ROOT / "scripts" / "harvest"))
    try:
        from live_units import filter_live
    except Exception:
        filter_live = None

    def collect(paths):
        out = {}
        for p in paths:
            for n, raw, relocs, _e in function_bodies_ext(Path(p)):
                out.setdefault(n, (masked_body(raw, relocs), relocs, len(raw)))
        return out

    our_objs = sorted(glob.glob(str(ROOT / f"build/{BUILD_ID}/src/**/*.obj"), recursive=True))
    ours = collect(our_objs)
    referenced = set()
    for p in our_objs:
        referenced |= coff_referenced_symbols(Path(p).read_bytes())
    tgt = sorted(glob.glob(str(ROOT / f"build/{BUILD_ID}/obj/*.obj")))
    if filter_live:
        try:
            tgt = filter_live(tgt, str(ROOT))
        except Exception:
            pass
    retail = collect(tgt)
    target_named = set()
    for p in tgt:
        target_named |= coff_referenced_symbols(Path(p).read_bytes())
    cache.parent.mkdir(parents=True, exist_ok=True)
    pickle.dump((ours, retail, referenced, target_named), cache.open("wb"))
    return ours, retail, referenced, target_named


def charged_pairs(path):
    sites = collections.Counter()
    fns = collections.defaultdict(set)
    for line in open(path):
        r = json.loads(line)
        t, b = r["target"], r["base"]
        if not isinstance(t, str) or not isinstance(b, str) or not t or not b:
            continue
        sites[(t, b)] += 1
        fns[(t, b)].add((r["unit"], r["func"]))
    return sites, fns


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--charges", default="", help="sites.jsonl from namecheck_triage.py")
    ap.add_argument("--census", default="", help="also take candidates from icf_site_census output")
    ap.add_argument("--installed", default=str(ROOT / "scripts" / "symbol_aliases.json"),
                    help="round-0 equivalence (READ ONLY; never written)")
    ap.add_argument("--out", required=True, help="DELTA artifact (new groups only)")
    ap.add_argument("--max-rounds", type=int, default=4)
    ap.add_argument("--vftable", action="store_true",
                    help="also mint groups for unmapped vftable_<hex> survivors, "
                         "gated on vtable CONTENTS agreeing slot for slot")
    ap.add_argument("--grounded-only", action="store_true",
                    help="withdraw the information-free placeholder tolerance entirely: "
                         "every differing relocation slot must be resolved, by the alias "
                         "equivalence or by CONTENT against the retail image")
    ap.add_argument("--no-resolve", action="store_true",
                    help="do not resolve lbl_/fn_ placeholders by content (the shipped "
                         "adjudicator's behaviour, for A/B)")
    ap.add_argument("--decoy", action="store_true",
                    help="run the negative control instead: icf_decoy_control.py with the "
                         "alias-class relaxation applied, reporting both selectivities")
    ap.add_argument("--limit", type=int, default=8)
    ap.add_argument("--cache", default=str(ROOT / "work" / "laneK" / "bodies.pkl"))
    args = ap.parse_args()

    tm = json.loads((ROOT / "scripts" / "target_symbol_map.json").read_text())
    addr_of = {v: k for k, v in tm.items()
               if isinstance(k, str) and k.lower().startswith("0x") and isinstance(v, str)}
    mapped = set(addr_of)
    ours, retail, referenced, target_named = load_bodies(Path(args.cache))
    print("bodies: ours %d, retail %d, referenced %d, target_named %d"
          % (len(ours), len(retail), len(referenced), len(target_named)), file=sys.stderr)

    installed = json.loads(Path(args.installed).read_text()).get("groups", [])
    uf = UF()
    installed_names = set()
    for g in installed:
        installed_names.add(g["survivor"])
        for f in g.get("folded", []):
            uf.union(g["survivor"], f)
            installed_names.add(f)
    installed_pairs = {(g["survivor"], f) for g in installed for f in g.get("folded", [])}
    print("round 0 equivalence: %d installed groups, %d names"
          % (len(installed), len(uf.p)), file=sys.stderr)

    if args.decoy:
        return decoy_control(ours, retail, mapped, uf, args.limit)

    sites, fns = charged_pairs(args.charges)
    cands = set(sites)
    if args.census:
        for _u, _f, rr in json.loads(Path(args.census).read_text())["records"]:
            for _k, t, b in rr:
                if isinstance(t, str) and isinstance(b, str) and t and b:
                    cands.add((t, b))
    print("candidates: %d (charged %d)" % (len(cands), len(sites)), file=sys.stderr)

    resolver = None
    if not args.no_resolve:
        resolver = ContentResolver(Image(ROOT / "orig" / BUILD_ID / "band.exe"),
                                   data_comdats())

    # ---- hard gates, unchanged from icf_alias_build ------------------------
    gated, reasons = [], collections.Counter()
    for t, b in cands:
        if (t, b) in installed_pairs:
            reasons["already_installed"] += 1
            continue
        if b.startswith("__") or t.startswith("__"):
            reasons["ingest: __ prefix"] += 1
            continue
        if t.startswith("except_data_") or "unwind" in b or "chain" in b:
            reasons["ingest: eh/unwind"] += 1
            continue
        if b not in referenced:
            reasons["reject_folded_not_referenced"] += 1
            continue
        if t not in target_named or b in target_named:
            reasons["reject_gate_c_target_naming"] += 1
            continue
        gated.append((t, b))
    # ---- the fixpoint -------------------------------------------------------
    accepted = {}
    pool = [p for p in gated if p[0] in mapped]
    reasons["reject_survivor_not_mapped"] = len(gated) - len(pool)
    for rnd in range(1, args.max_rounds + 1):
        new = []
        rest = []
        for t, b in pool:
            rt, ob = retail.get(t), ours.get(b)
            if rt is None or ob is None:
                reasons["reject_no_body"] += 1
                continue
            if vacuous(rt):
                reasons["reject_vacuous"] += 1
                continue
            if rt[0] != ob[0] or rt[2] != ob[2]:
                reasons["reject_RETAIL_DIFFER"] += 1
                continue
            ok, relaxed, tol = relocs_agree_eq(
                rt, ob, mapped, uf, resolver=resolver,
                grounded_only=args.grounded_only)
            if ok:
                new.append((t, b, relaxed, rnd, tol))
            else:
                rest.append((t, b))
        for t, b, relaxed, r, tol in new:
            accepted[(t, b)] = (relaxed, r, tol)
            uf.union(t, b)
        print("round %d: +%d accepts (%d relaxed by an alias class), %d still open"
              % (rnd, len(new), sum(1 for x in new if x[2]), len(rest)), file=sys.stderr)
        pool = rest
        if not new:
            break
    reasons["reject_RELOC_TARGETS_DIFFER"] = len(pool)

    # ---- vftable coverage lever --------------------------------------------
    vft = {}
    if args.vftable:
        img = Image(ROOT / "orig" / BUILD_ID / "band.exe")
        our_data = resolver.our if resolver is not None else data_comdats()
        for t, b in cands:
            m = ADDR_IN_NAME.match(t)
            if not m or m.group(1) != "vftable" or t in mapped:
                continue
            va = int(m.group(2), 16)
            rec = our_data.get(b)
            if not rec:
                continue
            slots = [n for _o, n in rec[1]]
            good = True
            for i, name in enumerate(slots):
                want = addr_of.get(name)
                got = img.word(va + 4 * i)
                if want is None or got is None or int(want, 16) != got:
                    good = False
                    break
            if good and len(slots) >= 2:
                vft.setdefault(t, []).append(b)
        print("vftable: %d survivors, %d folded names" % (len(vft), sum(map(len, vft.values()))),
              file=sys.stderr)

    # ---- emit ---------------------------------------------------------------
    groups = {}
    for (t, b), (relaxed, rnd, tol) in accepted.items():
        g = groups.setdefault(t, {"name": None, "address": addr_of[t], "survivor": t,
                                  "folded": [], "_meta": []})
        g["folded"].append(b)
        g["_meta"].append({"folded": b, "tier": 1, "round": rnd, "relaxed_slots": relaxed,
                           "tolerated_slots": tol, "fully_grounded": tol == 0,
                           "sites": sites.get((t, b), 0),
                           "fns": len(fns.get((t, b), ()))})
    for t, bl in vft.items():
        g = groups.setdefault(t, {"name": None, "address": "0x" + ADDR_IN_NAME.match(t).group(2),
                                  "survivor": t, "folded": [], "_meta": []})
        for b in bl:
            g["folded"].append(b)
            g["_meta"].append({"folded": b, "tier": "vftable-contents",
                               "sites": sites.get((t, b), 0),
                               "fns": len(fns.get((t, b), ()))})
    # ONE NAME, ONE ADDRESS.  `gen_symbol_alias_map.py` renders every group member
    # as a `<name> <address>` line and objdiff's parse_msvc_map buckets by address,
    # so a name emitted at two addresses would silently merge two groups.  Drop a
    # folded name that the installed set already places elsewhere, and drop a
    # delta group whose survivor is a folded member of some other group.
    inst_addr = {}
    for g in installed:
        a = g["address"].lower()
        inst_addr[g["survivor"]] = a
        for f in g.get("folded", []):
            inst_addr[f] = a
    all_folded = {b for g in groups.values() for b in g["folded"]}
    for t in list(groups):
        a = groups[t]["address"].lower()
        if t in all_folded or (t in inst_addr and inst_addr[t] != a):
            del groups[t]
            reasons["drop_name_address_collision"] += 1
    for g in groups.values():
        a = g["address"].lower()
        keep = []
        for b in sorted(set(g["folded"])):
            if uf.eq(b, g["survivor"]) and (g["survivor"], b) in installed_pairs:
                continue                          # already installed in this group
            if b in inst_addr and inst_addr[b] != a:
                reasons["drop_name_address_collision"] += 1
                continue
            keep.append(b)
        g["folded"] = keep
        g["_meta"] = [m for m in g["_meta"] if m["folded"] in keep]
    groups = {t: g for t, g in groups.items() if g["folded"]}
    # ... and the same invariant WITHIN the delta.  A name accepted against two
    # different survivor addresses means we proved X == A and X == B: either the
    # two retail bodies are themselves one fold class (and the two groups should
    # be one, which this generator does not attempt), or one proof is wrong.
    # Precision-first: drop the ambiguous name from every group and report it.
    claim = collections.defaultdict(set)
    for t, g in groups.items():
        for n in (t, *g["folded"]):
            claim[n].add(g["address"].lower())
    ambiguous = {n for n, a in claim.items() if len(a) > 1}
    if ambiguous:
        for t in list(groups):
            if t in ambiguous:
                del groups[t]
                reasons["drop_ambiguous_name"] += 1
                continue
            g = groups[t]
            drop = [b for b in g["folded"] if b in ambiguous]
            if drop:
                reasons["drop_ambiguous_name"] += len(drop)
                g["folded"] = [b for b in g["folded"] if b not in ambiguous]
                g["_meta"] = [m for m in g["_meta"] if m["folded"] in g["folded"]]
        groups = {t: g for t, g in groups.items() if g["folded"]}
    for t, g in groups.items():
        g["name"] = t.split("@")[0].lstrip("?") or g["address"]
        rounds = sorted({m.get("round") for m in g["_meta"] if m.get("round")})
        g["evidence"] = (
            "ICF fold group derived by scripts/icf_alias_fixpoint.py. T1 byte identity "
            "(masked bytes, size and the full (offset, reloc_type) sequence) against the "
            "retail split object at the survivor address, with relocation-target NAME "
            "equality closed under the already-proven alias equivalence -- the /OPT:ICF "
            "fixpoint. Round(s) %s. %s"
            % (rounds or ["vftable-contents"],
               "Survivor is a dtk vftable placeholder whose address is in the name; "
               "adjudicated by vtable CONTENTS, slot for slot."
               if t.startswith("vftable_") else ""))

    out = {"_comment": [
        "DELTA over scripts/symbol_aliases.json -- generated, NOT installed.",
        "Produced by scripts/icf_alias_fixpoint.py; see that file for the argument.",
        "Install: python3 scripts/icf_alias_merge.py --into scripts/symbol_aliases.json "
        "--delta <this file> && python3 tools/gen_symbol_alias_map.py",
    ], "groups": sorted(groups.values(),
                        key=lambda g: -sum(m["sites"] for m in g["_meta"]))}
    Path(args.out).write_text(json.dumps(out, indent=1) + "\n")

    nsites = sum(sites.get((g["survivor"], m["folded"]), 0)
                 for g in out["groups"] for m in g["_meta"])
    nfns = set()
    for g in out["groups"]:
        for m in g["_meta"]:
            nfns |= fns.get((g["survivor"], m["folded"]), set())
    if resolver is not None:
        print("\ncontent resolver on placeholder slots: %s"
              % dict(resolver.stats), file=sys.stderr)
    print("\n=== refusal census over the gated candidate set ===", file=sys.stderr)
    for k, v in reasons.most_common():
        print("  %-36s %7d" % (k, v), file=sys.stderr)
    print("\nDELTA: %d groups, %d folded names, %d CHARGED sites, %d charged functions"
          % (len(out["groups"]), sum(len(g["folded"]) for g in out["groups"]), nsites,
             len(nfns)), file=sys.stderr)
    print("-> %s" % args.out, file=sys.stderr)
    return 0


def decoy_control(ours, retail, mapped, uf, limit):
    """`tools/icf_decoy_control.py`, re-run WITH the alias-class relaxation.

    The decoy population is the one a naive masked-byte comparator accepts: same
    masked bytes, same size.  The relocation gate must kill the twins in it.  If
    the relaxation drops selectivity materially, it has stopped being "these two
    names are one proven address" and become "different names are fine".
    """
    from icf_alias_build import relocs_agree
    buckets = collections.defaultdict(list)
    for F, rec in ours.items():
        buckets[(rec[0], rec[2])].append(F)
    naive = rej_strict = rej_eq = 0
    converted = []
    for S, rt in retail.items():
        if S not in mapped or vacuous(rt):
            continue
        for F in buckets.get((rt[0], rt[2]), ()):
            if F == S:
                continue
            naive += 1
            ob = ours[F]
            s_ok = relocs_agree(rt, ob, mapped, True, None)
            e_ok, _r, _t = relocs_agree_eq(rt, ob, mapped, uf)
            rej_strict += not s_ok
            rej_eq += not e_ok
            if e_ok and not s_ok and len(converted) < limit:
                # only the slots the RELAXATION carried -- the others were already
                # tolerated by the strict gate and are not what changed the verdict
                diff = [(rn, on) for (_ro, rn, _t), (_oo, on, _u) in zip(rt[1], ob[1])
                        if rn != on and uf.eq(rn, on)]
                converted.append((S, F, diff[:2]))
    print("=== NEGATIVE / DECOY CONTROL, with the alias-class relaxation ===")
    print("  naive masked-byte comparator would accept : %d pairs" % naive)
    print("  selectivity, relocation gate STRICT       : %.2f%% (%d rejected)"
          % (100.0 * rej_strict / naive if naive else 0.0, rej_strict))
    print("  selectivity, gate + alias-class relaxation: %.2f%% (%d rejected)"
          % (100.0 * rej_eq / naive if naive else 0.0, rej_eq))
    print("  decoys the relaxation CONVERTS            : %d (%.4f%% of the population)"
          % (rej_strict - rej_eq,
             100.0 * (rej_strict - rej_eq) / naive if naive else 0.0))
    print("\n  Every converted decoy's differing callees must be a PROVEN one-address")
    print("  pair -- that is the whole claim.  Sample:")
    for S, F, diff in converted:
        print("\n   survivor %s" % S[:78])
        print("   folded   %s" % F[:78])
        for rn, on in diff:
            print("      retail calls %-40s  ours calls %-40s  proven one address: %s"
                  % (rn[:40], on[:40], uf.eq(rn, on)))
    return 0


def _obj_data_relocs(path):
    """{external data symbol: (bytes, [(offset, target name)], size)} for one COFF."""
    d = path.read_bytes()
    nsec, = struct.unpack_from("<H", d, 2)
    psym, nsym = struct.unpack_from("<II", d, 8)
    if not psym or not nsym:
        return {}
    opt, = struct.unpack_from("<H", d, 16)
    sh = 20 + opt
    sec = []
    for s in range(nsec):
        b = sh + s * 40
        size, = struct.unpack_from("<I", d, b + 16)
        praw, = struct.unpack_from("<I", d, b + 20)
        prel, = struct.unpack_from("<I", d, b + 24)
        nrel, = struct.unpack_from("<H", d, b + 32)
        chars, = struct.unpack_from("<I", d, b + 36)
        sec.append((size, praw, prel, nrel, chars))
    strt = psym + nsym * 18
    names, recs, i = {}, [], 0
    while i < nsym:
        rec = d[psym + i * 18: psym + i * 18 + 18]
        if rec[:4] == b"\0\0\0\0":
            off, = struct.unpack_from("<I", rec, 4)
            nm = d[strt + off:d.index(b"\0", strt + off)].decode("latin1")
        else:
            nm = rec[:8].rstrip(b"\0").decode("latin1")
        secnum, = struct.unpack_from("<h", rec, 12)
        names[i] = nm
        recs.append((nm, secnum, rec[16], rec[17]))
        i += 1 + rec[17]
    out = {}
    for nm, secnum, sclass, _naux in recs:
        if secnum <= 0 or sclass != 2:
            continue
        size, praw, prel, nrel, chars = sec[secnum - 1]
        if chars & 0x20:            # code, not data
            continue
        rel = []
        for r in range(nrel):
            va, si = struct.unpack_from("<II", d, prel + r * 10)
            rel.append((va, names.get(si, "?")))
        raw = d[praw:praw + size] if praw else bytes(size)
        out[nm] = (raw, sorted(rel), size)
    return out


def data_comdats():
    """{symbol: (bytes, [(offset, target name)], size)} over our data COMDATs --
    enough to compare a vtable's contents, or any datum's, against the image."""
    import glob
    out = {}
    for p in sorted(glob.glob(str(ROOT / f"build/{BUILD_ID}/src/**/*.obj"), recursive=True)):
        for name, rel in _obj_data_relocs(Path(p)).items():
            out.setdefault(name, rel)
    return out


if __name__ == "__main__":
    raise SystemExit(main())
