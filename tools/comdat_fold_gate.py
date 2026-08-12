#!/usr/bin/env python3
"""Sub-class-agnostic COMDAT-identity gate for the `name_check` callee charges.

What this asks
--------------
A charge says: at N call sites retail's relocation names S while ours names F.
The only question that can settle it without touching source is the one the
linker's `/OPT:ICF` asked:

    is our compiled COMDAT for F THE SAME LINKED BODY as retail's body at addr(S)?

If yes, the two spellings denote one address after linking, the emitted machine
code is identical, and an alias group states a fact.  If no, the charge stands
and an alias would hide it.  `tools/fold_thunk_gate.py` (lane H) asked this for
the `fold_thunk_naming` sub-class only.  Nothing about the question is specific
to that sub-class, so this tool asks it for all of them.

Why it is not just fold_thunk_gate.py with a different --subclass
-----------------------------------------------------------------
It is a different comparator, and the difference is the whole result.

fold_thunk_gate masks every relocation-CAPABLE field on both sides -- branch
displacements and every 16-bit immediate -- and then, because the retail side
cannot know which of those fields the linker actually patched, requires the two
sides to agree on the SET of masked offsets.  On a 4-byte `b MemFree` that is
exact.  On any real body it is not: retail's side marks every `li` / `addi` /
`lwz` / `stw` as "relocation-capable" while our COFF side lists only the offsets
carrying an actual relocation record, so the sets differ and the pair is refused.
Measured over the three untouched sub-classes that fired on 196 pairs / 436
sites -- refusals produced by the instrument, not by the evidence.

This tool uses our COFF relocation table as the oracle for WHICH fields are
relocated, which is exactly what it is:

    offset with NO relocation in our obj   compare the FULL 32-bit word.
                                           Strictly stronger than the masked
                                           compare, and it is where intra-body
                                           loop branches and every literal
                                           constant live.
    offset with a BRANCH relocation        compare opcode/AA/LK, then RESOLVE
                                           retail's branch destination through
                                           target_symbol_map.json and compare
                                           that NAME to our relocation's symbol.
                                           Not masked away -- for a tail branch
                                           the destination is the entire
                                           information content of the body.
    offset with a 16-BIT-IMMEDIATE reloc   unresolvable.  A `lis`/`addi` pair in
                                           a linked image reconstructs a data VA
                                           and we have no data symbol map, so
                                           "equal after masking" is vacuous:
                                           ??__EgNotifies and SystemConfig are
                                           12 identical bytes differing only in
                                           which global they load.  REFUSED, not
                                           accepted (11 pairs / 26 sites).

Fail-closed everywhere else too
-------------------------------
* size mismatch between our COMDAT and the retail extent -> REFUSE.  No padding
  trimming: a trim is a guess about which end is padding.
* our objs disagree on F (two distinct COMDATs for one name) -> REFUSE.
* retail's map places F on a DIFFERENT body -> REFUSE unless the image itself
  discredits that entry (tier CF2: zero `.text` fan-in, or no symbols.txt
  extent) or dc3's leaked map witnesses a HOMONYM (tier CF3, see below).
* F admitted against more than one survivor ADDRESS -> REFUSE every one of them.
  Aliasing F at two addresses would transitively assert those two addresses are
  one fold class, which is a claim this tool did not test.
* F or S already sits in an existing alias group at a DIFFERENT address ->
  REFUSE.  scripts/symbol_aliases.json already carries 842 names at more than
  one address; this lane does not add to that.

The homonym tier (CF3), and what the sweep of dc3's map found
-------------------------------------------------------------
FT3 in lane H's gate fired once, on `??3@YAXPAX@Z`: dc3's leaked ham_xbox_r.map
names it at three addresses under three different `.obj`, so a map entry naming
our spelling at a "wrong" address can be another module's own function that
legitimately carries the same mangled name.  The tier is kept here.  It cannot
fire again -- see `tools/homonym_index.py`, which sweeps all 117,960 symbols in
that map and finds the class is 25 names game-wide, bounded by the linker's own
rules to internal-linkage definitions.
"""

import argparse
import collections
import glob
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "scripts"))
from comdat_bytes import comdats                                    # noqa: E402
from wrong_callee_triage import Image, load_sizes, IMM16_OPS        # noqa: E402
from fold_thunk_gate import parse_leaked_map, mask_word             # noqa: E402

BUILD_ID = "45410914"
REL_BRANCH = {3, 5, 6, 7}          # ADDR24, ADDR14, REL24, REL14
REL_IMM16 = {4, 0x10, 0x11}        # ADDR16, REFHI, REFLO
DEFAULT_SUBCLASSES = "bijection_class,map_name_unresolved,residual"


def branch_dest(w, va):
    op = w >> 26
    if op == 18:
        d = w & 0x03FFFFFC
        if d & 0x02000000:
            d -= 0x04000000
    elif op == 16:
        d = w & 0x0000FFFC
        if d & 0x8000:
            d -= 0x10000
    else:
        return None
    return d if (w & 2) else va + d


class Retail:
    def __init__(self):
        self.img = Image(ROOT / "orig" / BUILD_ID / "band.exe")
        self.size = load_sizes()
        smap = json.loads((ROOT / "scripts" / "target_symbol_map.json").read_text())
        self.byva = {int(a, 16): n for a, n in smap.items()
                     if a.startswith("0x") and isinstance(n, str)}
        self.fanin = self.img.fanin()

    def words(self, va):
        """Raw big-endian words of the retail body at `va`, or (None, why)."""
        n = self.size.get(va)
        o = self.img.off(va)
        if not n:
            return None, "no symbols.txt extent at 0x%08x" % va
        if o is None:
            return None, "0x%08x is outside the image" % va
        return list(struct.unpack_from(">%dI" % (n // 4), self.img.data, o)), None

    def masked(self, va):
        w, err = self.words(va)
        return (None if err else [mask_word(x) for x in w]), err


def compare(rwords, sa, ourraw, relocs, byva, alias=None):
    """Our COMDAT vs the retail body at `sa`, using OUR relocation table.

    Returns (ok, evidence).  `relocs` is {offset: (symbol_name, type)}.

    `alias` is {name: group_id} built from the ALREADY-INSTALLED alias groups.  A
    branch destination is name-equal if the two names are literally equal or if a
    previously proven fold already places them at one address -- which is exactly
    what objdiff's reloc_eq does at report time, so refusing there would be this
    gate holding itself to a stricter rule than the ruler it is feeding.
    """
    alias = alias or {}

    def same_name(a, b):
        return a == b or (a in alias and alias.get(b) == alias[a])

    if len(rwords) * 4 != len(ourraw):
        return False, ("body size %d bytes (retail extent) vs %d (our COMDAT)"
                       % (len(rwords) * 4, len(ourraw)))
    owords = list(struct.unpack(">%dI" % (len(ourraw) // 4), ourraw))
    nfull = nbr = nal = 0
    for i, (rw, ow) in enumerate(zip(rwords, owords)):
        off = 4 * i
        if off not in relocs:
            if rw != ow:
                return False, ("unrelocated word at 0x%x differs: retail %08x vs ours %08x"
                               % (off, rw, ow))
            nfull += 1
            continue
        nm, ty = relocs[off]
        if ty in REL_BRANCH:
            op = rw >> 26
            if op != (ow >> 26):
                return False, "opcode at 0x%x differs (retail %d vs ours %d)" % (off, op, ow >> 26)
            if op not in (16, 18):
                return False, ("relocation at 0x%x is a branch type on a non-branch opcode %d"
                               % (off, op))
            m = 0xFC000003 if op == 18 else 0xFFFF0003
            if (rw & m) != (ow & m):
                return False, "branch word at 0x%x differs outside the displacement" % off
            d = branch_dest(rw, sa + off)
            tn = byva.get(d)
            if tn is None:
                return False, ("retail branch at 0x%x goes to 0x%08x, which the map does not "
                               "name -- destination unresolvable" % (off, d or 0))
            if not same_name(tn, nm):
                return False, ("branch destination at 0x%x: retail calls %s, we call %s"
                               % (off, tn, nm))
            nbr += 1 if tn == nm else 0
            nal += 0 if tn == nm else 1
        elif ty in REL_IMM16:
            return False, ("relocation at 0x%x is a 16-bit immediate on %s: a linked image "
                           "reconstructs a data VA we cannot name, so a masked compare here "
                           "is vacuous" % (off, nm))
        else:
            return False, "relocation type 0x%x at 0x%x is not one this gate models" % (ty, off)
    return True, ("identical: %d/%d words compared as FULL 32-bit values, %d relocated branch "
                  "destination(s) resolved through the map and name-equal%s"
                  % (nfull, len(rwords), nbr + nal,
                     "" if not nal else (" (%d of them via an already-installed alias group, "
                                         "not a literal name match)" % nal)))


def our_index(wanted):
    idx = collections.defaultdict(list)
    for p in glob.glob(str(ROOT / "build" / BUILD_ID / "src" / "**" / "*.obj"), recursive=True):
        try:
            c = comdats(p)
        except Exception:
            continue
        for nm in wanted & c.keys():
            idx[nm].append((str(Path(p).relative_to(ROOT)), c[nm]))
    return idx


def homonym(dc3, dc3img, retail, F, fa):
    """dc3's leaked map witnesses that retail's addr(F) is ANOTHER module's F."""
    if not dc3 or dc3img is None:
        return None
    sites = dc3.get(F, [])
    if len({a for a, _ in sites}) < 2:
        return None
    want, err = retail.masked(fa)
    if err:
        return None
    nbytes = len(want) * 4
    for va, mod in sites:
        o = dc3img.off(va)
        if o is None or o + nbytes > len(dc3img.data):
            continue
        got = [mask_word(w) for w in struct.unpack_from(">%dI" % (nbytes // 4), dc3img.data, o)]
        if got == want:
            return ("HOMONYM: dc3's leaked ham_xbox_r.map names %s at %d distinct addresses, and "
                    "the body at 0x%08x (%s) is byte-identical to retail's body at 0x%08x over %d "
                    "bytes once relocated fields are masked. That address is another module's own "
                    "%s, not retail's copy of ours." % (F, len({a for a, _ in sites}), va, mod, fa,
                                                        nbytes, F))
    return None


def resolve_dc3_map(arg):
    p = Path(arg)
    if p.is_absolute():
        return p
    for anc in [ROOT] + list(ROOT.parents):
        if (anc / arg).is_file():
            return anc / arg
    return ROOT.parent / arg


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--worklist", default="docs/plans/wrong-callee-triage-2026-08-12.json")
    ap.add_argument("--subclass", default=DEFAULT_SUBCLASSES,
                    help="comma-separated, or 'all' (default: %(default)s)")
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--aliases", default="scripts/symbol_aliases.json")
    ap.add_argument("--install", action="store_true")
    ap.add_argument("--dc3-map", default="dc3-decomp/orig/373307D9/ham_xbox_r.map")
    args = ap.parse_args()

    wl = json.loads((ROOT / args.worklist).read_text())
    subs = None if args.subclass == "all" else set(args.subclass.split(","))
    pairs = [r for r in wl["pairs"] if subs is None or r["subclass"] in subs]

    retail = Retail()
    dc3p = resolve_dc3_map(args.dc3_map)
    dc3 = parse_leaked_map(dc3p) if dc3p.is_file() else {}
    dc3img = Image(dc3p.parent / "ham_xbox_r.exe") if (dc3 and (dc3p.parent / "ham_xbox_r.exe").is_file()) else None
    print("dc3 leaked map: %s (%d names)"
          % (dc3p if dc3p.is_file() else "ABSENT -- CF3 unavailable, those pairs REFUSE", len(dc3)))

    idx = our_index({r["base"] for r in pairs})
    print("our objs define %d of the %d charged callee spellings" % (len(idx), len({r["base"] for r in pairs})))

    # names already aliased somewhere, so we never create a second address for one
    adoc = json.loads((ROOT / args.aliases).read_text())
    placed, alias = {}, {}
    for g in adoc["groups"]:
        for nm in [g["survivor"], *g.get("folded", [])]:
            placed.setdefault(nm, set()).add(int(g["address"], 16))
    # {name: group_id} for the branch-destination compare. A name the file places
    # at more than one address (842 of them, pre-existing) gets NO group id: its
    # equivalence class is not established, so it falls back to a literal compare.
    for nm, addrs in placed.items():
        if len(addrs) == 1:
            alias[nm] = next(iter(addrs))
    print("alias file: %d groups, %d names, %d usable as an equivalence class "
          "(%d sit at more than one address and are ignored)"
          % (len(adoc["groups"]), len(placed), len(alias), len(placed) - len(alias)))

    rows = []
    for r in pairs:
        S, F = r["target"], r["base"]
        sa, fa = int(r["target_addr"], 16), int(r["base_addr"], 16)
        row = dict(subclass=r["subclass"], survivor=S, folded=F, sites=r["sites"],
                   survivor_addr=r["target_addr"], folded_map_addr=r["base_addr"],
                   survivor_fanin=r["target_fanin"])

        def refuse(why):
            rows.append({**row, "verdict": "REFUSE", "tier": None, "reason": why})

        rw, err = retail.words(sa)
        if err:
            refuse("retail survivor body unreadable: " + err)
            continue
        defs = idx.get(F, [])
        if not defs:
            refuse("no COMDAT for %s in any of our compiled objs" % F)
            continue
        bodies = {(bytes(cd["raw"]), tuple(sorted(cd["relocs"]))) for _, cd in defs}
        if len(bodies) > 1:
            refuse("our objs disagree on %s: %d distinct COMDATs across %d objs"
                   % (F, len(bodies), len(defs)))
            continue
        objp, cd = defs[0]
        row["our_def"] = objp + ("" if len(defs) == 1 else " (+%d identical)" % (len(defs) - 1))
        row["our_bytes"] = len(cd["raw"])
        relocs = {off: (nm, ty) for off, nm, ty in cd["relocs"]}

        ok, why = compare(rw, sa, cd["raw"], relocs, retail.byva, alias)
        row["body_evidence"] = why
        if not ok:
            refuse("our COMDAT is not the retail body at the survivor address -- " + why)
            continue

        # second gate: retail's own definition of F must not contradict the fold
        fw, ferr = retail.masked(fa)
        if ferr:
            tier, disc = "CF2", "map parks %s at %s with no symbols.txt extent" % (F, r["base_addr"])
        elif fw == [mask_word(x) for x in rw]:
            tier, disc = "CF1", None
        else:
            f_fanin = retail.fanin[fa]
            hom = homonym(dc3, dc3img, retail, F, fa)
            if f_fanin == 0:
                tier, disc = "CF2", ("map parks %s at %s, which has ZERO .text fan-in"
                                     % (F, r["base_addr"]))
            elif hom:
                tier, disc = "CF3", hom
            else:
                refuse("retail has a DIFFERENT body named %s at %s (fan-in %d, %d bytes); no "
                       "zero-fan-in parking and no dc3 homonym witness, so that map entry stands "
                       "and an alias would hide a source defect"
                       % (F, r["base_addr"], f_fanin, len(fw) * 4))
                continue
        row.update(tier=tier, discredit=disc, verdict="ADMIT")
        rows.append(row)

    # --- conflict sweep: one address per name, and never a second address for a
    # name the existing alias file already places.
    adm = [r for r in rows if r["verdict"] == "ADMIT"]
    homes = collections.defaultdict(set)
    for r in adm:
        homes[r["folded"]].add(r["survivor_addr"])
        homes[r["survivor"]].add(r["survivor_addr"])
    for r in rows:
        if r["verdict"] != "ADMIT":
            continue
        sa = int(r["survivor_addr"], 16)
        for nm in (r["folded"], r["survivor"]):
            if len(homes[nm]) > 1:
                r.update(verdict="REFUSE", tier=None,
                         reason=("%s is admitted against %d different survivor addresses (%s); "
                                 "aliasing it at more than one would assert those addresses are "
                                 "one fold class, which this gate did not test"
                                 % (nm, len(homes[nm]), ",".join(sorted(homes[nm])))))
                break
            if nm in placed and sa not in placed[nm]:
                r.update(verdict="REFUSE", tier=None,
                         reason=("%s already sits in an existing alias group at %s; adding it at "
                                 "%s would give one name two addresses"
                                 % (nm, ",".join("0x%08x" % a for a in sorted(placed[nm])),
                                    r["survivor_addr"])))
                break

    adm = [r for r in rows if r["verdict"] == "ADMIT"]
    ref = [r for r in rows if r["verdict"] == "REFUSE"]
    groups = collections.defaultdict(list)
    for r in adm:
        groups[(r["survivor"], r["survivor_addr"])].append(r)

    bysub = collections.defaultdict(lambda: collections.Counter())
    for r in rows:
        bysub[r["subclass"]][r["verdict"]] += 1
        bysub[r["subclass"]][r["verdict"] + "_sites"] += r["sites"]

    out = {
        "generated_by": "tools/comdat_fold_gate.py",
        "build": BUILD_ID,
        "subclasses": sorted({r["subclass"] for r in rows}),
        "totals": {
            "pairs": len(rows),
            "admitted_pairs": len(adm), "admitted_sites": sum(r["sites"] for r in adm),
            "refused_pairs": len(ref), "refused_sites": sum(r["sites"] for r in ref),
            "groups": len(groups),
        },
        "by_subclass": {k: dict(v) for k, v in bysub.items()},
        "by_tier": dict(collections.Counter(r["tier"] for r in adm)),
        "pairs": rows,
    }
    Path(ROOT / args.out).write_text(json.dumps(out, indent=1) + "\n")

    for r in sorted(adm, key=lambda x: -x["sites"]):
        print("ADMIT  %-6s %-20s %5d  %s <- %s" % (r["tier"], r["subclass"], r["sites"],
                                                   r["survivor"][:44], r["folded"][:44]))
        print("       %s" % r["body_evidence"])
    print("\nADMIT %d pairs / %d sites in %d groups; REFUSE %d pairs / %d sites"
          % (len(adm), sum(r["sites"] for r in adm), len(groups), len(ref),
             sum(r["sites"] for r in ref)))
    for k, v in sorted(bysub.items()):
        print("  %-22s ADMIT %3d pairs/%4d sites   REFUSE %3d pairs/%4d sites"
              % (k, v["ADMIT"], v["ADMIT_sites"], v["REFUSE"], v["REFUSE_sites"]))
    print("-> %s" % args.out)

    if args.install:
        install(groups, ROOT / args.aliases)


OWNED = "COMDAT-identity alias group derived by tools/comdat_fold_gate.py."


def install(groups, path):
    doc = json.loads(path.read_text())
    existing = {g["survivor"]: g for g in doc["groups"]}
    added = updated = 0
    for (S, addr), rs in sorted(groups.items()):
        folded = sorted({r["folded"] for r in rs})
        ev = (OWNED + " Evidence tier(s) %s. Our compiled COMDAT for each folded spelling below "
              "has the SAME SIZE as the retail body at %s and every word compares equal: the "
              "words our COFF relocation table does NOT relocate compare as full 32-bit values "
              "(not masked), and every word it relocates with a BRANCH relocation has retail's "
              "destination resolved through target_symbol_map.json and name-compared. A 16-bit "
              "immediate relocation is unresolvable in a linked image and REFUSES the pair rather "
              "than being masked away. CF1=retail's map places the folded spelling on the same "
              "body or nowhere. CF2=on a different body the image discredits (zero .text fan-in, "
              "or no symbols.txt extent). CF3=on a HOMONYM witnessed by dc3's leaked "
              "ham_xbox_r.map. %d folded spelling(s), %d charged name_check sites. Per spelling: %s"
              % (",".join(sorted({r["tier"] for r in rs})), addr, len(folded),
                 sum(r["sites"] for r in rs),
                 " ".join("[%s %s %s, %d sites: %s]"
                          % (x["folded"], x["subclass"], x["tier"], x["sites"], x["body_evidence"])
                          for x in sorted(rs, key=lambda y: -y["sites"]))))
        if S in existing:
            g = existing[S]
            before = (sorted(g.get("folded", [])), g.get("evidence", ""))
            if g.get("evidence", "").startswith(OWNED):
                g["folded"] = sorted(folded)
                g["evidence"] = ev
            else:
                g["folded"] = sorted(set(g.get("folded", [])) | set(folded))
                if sorted(g["folded"]) != before[0]:
                    g["evidence"] = g.get("evidence", "") + " || " + ev
            if (sorted(g["folded"]), g["evidence"]) != before:
                updated += 1
        else:
            doc["groups"].append({"name": S.split("@")[0].lstrip("?") or S, "address": addr,
                                  "survivor": S, "folded": folded, "evidence": ev})
            added += 1
    note = ("COMDAT-IDENTITY TIER (CF, lane R, 2026-08-12) -- like the FOLD-THUNK tier below, "
            "these groups have BOTH spellings map-resident, deliberately: target_symbol_map.json "
            "is a VA->name FUNCTION over an ICF-folded link and can name a fold class only once. "
            "The difference from FT is the comparator, not the claim: CF compares the words our "
            "COFF relocation table does not relocate as FULL 32-bit values instead of masking "
            "every relocation-CAPABLE field, and REFUSES rather than accepts a 16-bit-immediate "
            "relocation, whose target a linked image cannot name. Every CF group is one survivor "
            "address; a spelling admitted against two addresses was refused at both. See "
            "docs/plans/comdat-fold-gate-2026-08-12.md.")
    doc["_comment"] = [c for c in doc["_comment"]
                       if not (isinstance(c, str) and c.startswith("COMDAT-IDENTITY TIER"))]
    doc["_comment"].append(note)
    path.write_text(json.dumps(doc, indent=2))
    print("installed: %d new group(s), %d updated; %d total" % (added, updated, len(doc["groups"])))


if __name__ == "__main__":
    main()
