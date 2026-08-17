#!/usr/bin/env python3
"""Gate the `fold_thunk_naming` charges one pair at a time, and emit the alias
groups that survive.

The charge
----------
`functionRelocDiffs=name_check` compares `bl` relocations BY NAME.  A charge in
this sub-class says: at N call sites retail's relocation names S -- a <=4-byte
body with a large fan-in -- while ours names F.  `scripts/wrong_callee_triage.py`
established that no source spelling can move F to S: the retail link ICF-folded
every identical COMDAT in the game onto one address and `target_symbol_map.json`,
being a VA->name FUNCTION, can record only one of the folded names.  The only
available repair is an alias group.

But "S is a folded thunk" is a statement about S, not about the PAIR.  It does
not establish that OUR F is in that fold class, and 36 pairs are 36 separate
questions.  This tool asks the one question `/OPT:ICF` actually asks:

    is our compiled COMDAT for F byte-identical -- modulo relocated fields, and
    with every relocation TARGET agreeing -- to the retail body at addr(S)?

Method (per pair)
-----------------
1. RETAIL side: read `size[addr(S)]` bytes at addr(S) out of `orig/45410914/
   band.exe`.  Apply an instruction-aware mask (branch displacement, `bc`
   displacement, 16-bit immediate) and resolve every masked branch destination
   through `target_symbol_map.json` to a NAME.
2. OUR side: read the COMDAT backing F out of `build/45410914/src/**/*.obj`
   (tools/comdat_bytes.py), apply the SAME mask, and read the relocation TARGET
   names straight out of the COFF relocation records.
3. Compare word count, masked words, and the resolved target name at every
   relocated field.

Why this is not the vacuous 4-byte compare the T1 tier guards against
--------------------------------------------------------------------
`tools/icf_alias_build.py`'s T1 tier requires >=4 words and >=50% of the body
unmasked, because a masked `b X` compares equal to every other `b Y`.  This tool
does not mask the destination away -- it RESOLVES it and compares the name.  For
a 4-byte tail branch the destination is the entire information content of the
function, so comparing it is the strongest test available, not the weakest.  A
body with NO relocation and no resolvable content (a bare `blr`) is genuinely
vacuous and is reported separately as tier FT-EMPTY.

The second gate: retail's own definition of F
---------------------------------------------
Our F being one body with S is necessary, not sufficient.  If retail's map also
places F on some OTHER, DIFFERENT body, then either that map entry is wrong or
our F is compiled wrong -- and aliasing would paper over the second case.  So:

  FT1      addr(F) is absent from the map, or retail's body at addr(F) is
           itself identical to the body at addr(S).  Nothing contradicts.
  FT2      retail's body at addr(F) differs, AND that map entry is discredited by
           the image itself -- NOTHING REFERENCES addr(F) (`Image.refs()`: no
           branch, no address-taken `lis`/`addi` immediate pair, no `.rdata`
           /`.data` pointer word), or it has no `symbols.txt` extent at all
           (padding / mid-function parking).  This used to read `Image.fanin()`,
           which counts branches only and is therefore vacuous for an
           address-taken or vtable-dispatched form -- see the CF2 note in
           tools/comdat_fold_gate.py for the false PASS that cost.
  FT3      retail's body at addr(F) differs and is a HOMONYM: dc3's LEAKED
           ham_xbox_r.map names F at more than one address, and the dc3 body at
           one of them is byte-identical (masked) to retail's body at addr(F),
           under a different module.  Two distinct functions in the image
           legitimately carry the same mangled name, so retail's addr(F) is not
           retail's copy of OUR F and cannot refute the fold.
  REFUSE   anything else, including every body comparison that fails, every
           unresolvable relocation, and every F our objects do not define.

⚠ There USED to be a fourth discredit: "our own definition of F scores <20% at
the `none` ruler against addr(F)".  It fired once, on ??3@YAXPAX@Z / 2.70%, and
it was WRONG -- that 2.70% measures objdiff pairing our 4-byte Milo
`::operator delete` against XAudio2's 148-byte one, which is a splits/pairing
defect, not evidence about the map.  A low score says the two bodies differ; it
cannot say WHICH of them is misnamed.  Removed, and replaced by FT3, which
answers that question with an oracle instead of a threshold.

Usage
-----
    python3 tools/fold_thunk_gate.py \\
        --worklist docs/plans/wrong-callee-triage-2026-08-12.json \\
        -o docs/plans/fold-thunk-alias-gate-2026-08-12.json
    python3 tools/fold_thunk_gate.py ... --install   # write scripts/symbol_aliases.json
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
from comdat_bytes import comdats                          # noqa: E402
from wrong_callee_triage import Image, load_sizes         # noqa: E402

BUILD_ID = "45410914"
EVIDENCE_TIER = "FT"          # fold-thunk tier, this lane
IMM16_OPS = {14, 15, 24, 25, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54}

# MSVC PPC COFF relocation types that patch a field we mask.
REL_BRANCH24 = {3, 6}         # ADDR24, REL24
REL_BRANCH14 = {5, 7}         # ADDR14, REL14
REL_IMM16 = {4, 0x10, 0x11}   # ADDR16, REFHI, REFLO


def mask_word(w):
    """Mask the fields a relocation can patch, keeping opcode / AA / LK."""
    op = w >> 26
    if op == 18:
        return w & 0xFC000003
    if op == 16:
        return w & 0xFFFF0003
    if op in IMM16_OPS:
        return w & 0xFFFF0000
    return w


def branch_target(w, va):
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


def parse_leaked_map(path):
    """{name: [(va, module)]} from dc3's leaked MSVC linker map."""
    import re
    rx = re.compile(r"^\s*\d{4}:[0-9a-fA-F]+\s+(\S+)\s+([0-9a-fA-F]{8})\s+(.*)$")
    out = collections.defaultdict(set)
    with open(path, errors="replace") as fh:
        for line in fh:
            m = rx.match(line)
            if m:
                mod = m.group(3).split()[-1] if m.group(3).split() else "?"
                out[m.group(1)].add((int(m.group(2), 16), mod))
    return {k: sorted(v) for k, v in out.items()}


class Retail:
    def __init__(self):
        self.img = Image(ROOT / "orig" / BUILD_ID / "band.exe")
        self.size = load_sizes()
        smap = json.loads((ROOT / "scripts" / "target_symbol_map.json").read_text())
        self.byva = {int(a, 16): n for a, n in smap.items()
                     if a.startswith("0x") and isinstance(n, str)}
        self.fanin = self.img.fanin()
        # FT2 asks "does the image discredit the map's parking spot", i.e. does
        # ANYTHING reach that address -- which `fanin()` (branches only) cannot
        # answer for an address-taken or vtable-dispatched form. See the CF2 note
        # in tools/comdat_fold_gate.py.
        self.refs = self.img.refs_detail()

    def ref_note(self, va):
        """'N reference(s) (branch B, address-taken A, pointer word P)'."""
        d = self.refs
        return ("%d reference(s) -- %d branch, %d address-taken (lis/addi immediate pair), "
                "%d data pointer word" % (d["total"][va], d["branch"][va],
                                          d["addr_taken"][va], d["data_ptr"][va]))

    def canon(self, va):
        """(masked_words, {offset: target_name}, note) or (None, None, why)."""
        n = self.size.get(va)
        o = self.img.off(va)
        if not n:
            return None, None, "no symbols.txt extent at %08x" % va
        if o is None:
            return None, None, "%08x outside the image" % va
        words, targets = [], {}
        for i in range(n // 4):
            w = struct.unpack_from(">I", self.img.data, o + 4 * i)[0]
            words.append(mask_word(w))
            op = w >> 26
            if op in (18, 16):
                t = branch_target(w, va + 4 * i)
                targets[4 * i] = self.byva.get(t) or "fn_%08x" % t
            elif op in IMM16_OPS:
                targets[4 * i] = None          # unresolvable in a linked image
        return words, targets, None


def our_index(wanted):
    """{name: [(objpath, comdat)]} restricted to `wanted`."""
    idx = collections.defaultdict(list)
    for p in glob.glob(str(ROOT / "build" / BUILD_ID / "src" / "**" / "*.obj"),
                       recursive=True):
        try:
            c = comdats(p)
        except Exception:
            continue
        for nm in wanted & c.keys():
            idx[nm].append((str(Path(p).relative_to(ROOT)), c[nm]))
    return idx


def our_canon(cd):
    """(masked_words, {offset: target_name}, note) for one of our COMDATs."""
    raw = cd["raw"]
    if len(raw) % 4:
        return None, None, "COMDAT size %d is not a multiple of 4" % len(raw)
    words = [mask_word(w) for w in struct.unpack(">%dI" % (len(raw) // 4), raw)]
    targets = {}
    for off, nm, ty in cd["relocs"]:
        if ty in REL_BRANCH24 or ty in REL_BRANCH14 or ty in REL_IMM16:
            targets[off] = nm
        else:
            targets[off] = nm
    return words, targets, None


def homonym(dc3, dc3img, retail, F, fa, nbytes):
    """Is retail's addr(F) a DIFFERENT function that merely shares F's mangled name?

    dc3 is the Rosetta Stone: same engine family, same middleware, and a LEAKED
    linker map that names every function AND its owning .obj. If that map lists F
    at more than one address -- one per module that defines its own file-scope
    copy -- then F is not a unique symbol in this codebase, and a VA->name map for
    RB3 can only record one of them. Confirm by BODY, not by the name alone.
    """
    if not dc3 or dc3img is None or nbytes <= 0:
        return None
    sites = dc3.get(F, [])
    if len(sites) < 2:
        return None
    want = retail.canon(fa)[0]
    if want is None:
        return None
    for va, mod in sites:
        o = dc3img.off(va)
        if o is None or o + nbytes > len(dc3img.data):
            continue
        got = [mask_word(w) for w in
               struct.unpack_from(">%dI" % (nbytes // 4), dc3img.data, o)]
        if got == want:
            return ("HOMONYM: dc3's leaked ham_xbox_r.map names %s at %d distinct addresses, "
                    "and the body at 0x%08x (%s) is byte-identical to retail's body at "
                    "0x%08x over %d bytes once relocated fields are masked. That address is "
                    "another module's own file-scope %s, not retail's copy of ours."
                    % (F, len(sites), va, mod, fa, nbytes, F))
    return None


def compare(rw, rt, ow, ot):
    if len(rw) != len(ow):
        return False, "body length %d words (retail) vs %d (ours)" % (len(rw), len(ow))
    if rw != ow:
        bad = [i * 4 for i in range(len(rw)) if rw[i] != ow[i]]
        return False, "masked words differ at offsets %s" % ["0x%x" % b for b in bad[:6]]
    if set(rt) != set(ot):
        return False, ("relocated fields at different offsets: retail %s vs ours %s"
                       % (sorted(rt), sorted(ot)))
    for off in sorted(rt):
        if rt[off] is None:
            return False, "retail field at 0x%x is a 16-bit immediate, unresolvable" % off
        if rt[off] != ot[off]:
            return False, ("relocation target at 0x%x: retail %s vs ours %s"
                           % (off, rt[off], ot[off]))
    return True, "identical: %d word(s), %d resolved relocation target(s)" % (len(rw), len(rt))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--worklist",
                    default="docs/plans/wrong-callee-triage-2026-08-12.json")
    ap.add_argument("--subclass", default="fold_thunk_naming")
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--install", action="store_true",
                    help="merge the admitted groups into scripts/symbol_aliases.json")
    ap.add_argument("--aliases", default="scripts/symbol_aliases.json")
    ap.add_argument("--dc3-map", default="../dc3-decomp/orig/373307D9/ham_xbox_r.map",
                    help="dc3's LEAKED ham_xbox_r.map, relative to this repo's PARENT "
                         "directory or absolute. Enables the FT3 homonym tier; without it "
                         "an FT3 pair is REFUSED, not assumed (default: %(default)s)")
    args = ap.parse_args()

    wl = json.loads((ROOT / args.worklist).read_text())
    pairs = [r for r in wl["pairs"] if r["subclass"] == args.subclass]
    retail = Retail()
    dc3p = Path(args.dc3_map)
    if not dc3p.is_absolute():
        # ROOT may be a linked worktree several levels below the checkout, so a
        # sibling-repo path does not resolve from ROOT.parent. Walk up until it does.
        for anc in [ROOT] + list(ROOT.parents):
            cand = anc / args.dc3_map
            if cand.is_file():
                dc3p = cand
                break
        else:
            dc3p = ROOT.parent / args.dc3_map
    dc3 = parse_leaked_map(dc3p) if dc3p.is_file() else {}
    print("dc3 leaked map: %s (%d names)"
          % (dc3p if dc3p.is_file() else "ABSENT -- FT3 unavailable, those pairs REFUSE",
             len(dc3)))
    dc3img = Image(dc3p.parent / "ham_xbox_r.exe") if (dc3 and (dc3p.parent / "ham_xbox_r.exe").is_file()) else None
    idx = our_index({r["base"] for r in pairs})

    rows = []
    for r in pairs:
        S, F = r["target"], r["base"]
        sa, fa = int(r["target_addr"], 16), int(r["base_addr"], 16)
        row = dict(survivor=S, folded=F, sites=r["sites"],
                   survivor_addr=r["target_addr"], folded_map_addr=r["base_addr"],
                   survivor_fanin=r["target_fanin"])

        rw, rt, err = retail.canon(sa)
        if err:
            rows.append({**row, "verdict": "REFUSE", "tier": None,
                         "reason": "retail survivor body unreadable: " + err})
            continue

        defs = idx.get(F, [])
        if not defs:
            rows.append({**row, "verdict": "REFUSE", "tier": None,
                         "reason": "no COMDAT for %s in any of our compiled objs" % F})
            continue
        canons = {}
        for objp, cd in defs:
            ow, ot, e = our_canon(cd)
            canons.setdefault((tuple(ow) if ow is not None else None,
                               tuple(sorted(ot.items())) if ot is not None else None,
                               e), []).append(objp)
        if len(canons) > 1:
            rows.append({**row, "verdict": "REFUSE", "tier": None,
                         "reason": "our objs disagree on %s: %d distinct COMDATs across %d objs"
                                   % (F, len(canons), len(defs))})
            continue
        (owt, ott, e), objs = next(iter(canons.items()))
        if e:
            rows.append({**row, "verdict": "REFUSE", "tier": None, "reason": e})
            continue
        ow, ot = list(owt or ()), dict(ott or ())
        row["our_def"] = objs[0] + ("" if len(objs) == 1 else " (+%d identical)" % (len(objs) - 1))
        row["our_words"] = len(ow)

        ok, why = compare(rw, rt, ow, ot)
        row["body_evidence"] = why
        if not ok:
            rows.append({**row, "verdict": "REFUSE", "tier": None,
                         "reason": "our COMDAT is not the retail survivor body -- " + why})
            continue

        # second gate: retail's own definition of F
        fw, ft, ferr = retail.canon(fa)
        f_refs = retail.refs["total"][fa]
        row["retail_F_refs"] = retail.ref_note(fa)
        if ferr:
            row["retail_F"] = "no body at %s (%s)" % (r["base_addr"], ferr)
            tier, disc = "FT2", "map parks %s at %s with no symbols.txt extent" % (F, r["base_addr"])
        elif (fw, ft) == (rw, rt):
            tier, disc = "FT1", None
            row["retail_F"] = "retail body at %s is the SAME body as the survivor" % r["base_addr"]
        else:
            row["retail_F"] = ("retail body at %s differs (%d words, %s)"
                               % (r["base_addr"], len(fw), retail.ref_note(fa)))
            hom = homonym(dc3, dc3img, retail, F, fa, len(fw) * 4)
            if f_refs == 0:
                tier, disc = "FT2", ("map parks %s at %s, which NOTHING IN THE IMAGE REFERENCES: "
                                     "%s" % (F, r["base_addr"], retail.ref_note(fa)))
            elif hom:
                tier, disc = "FT3", hom
            else:
                rows.append({**row, "verdict": "REFUSE", "tier": None,
                             "reason": ("retail has a DIFFERENT body named %s at %s (%s); the "
                                        "image does not discredit the parking spot and there is "
                                        "no dc3 homonym witness, so the map entry stands and "
                                        "aliasing would hide a source defect"
                                        % (F, r["base_addr"], retail.ref_note(fa)))})
                continue
        if not rt:
            tier = "FT-EMPTY"
            disc = (disc or "") + " | body carries no relocation: the fold is real but the " \
                                  "byte comparison is vacuous"
        row["tier"] = tier
        row["discredit"] = disc
        row["verdict"] = "ADMIT"
        rows.append(row)

    adm = [r for r in rows if r["verdict"] == "ADMIT"]
    ref = [r for r in rows if r["verdict"] == "REFUSE"]
    groups = collections.defaultdict(list)
    for r in adm:
        groups[(r["survivor"], r["survivor_addr"])].append(r)

    out = {
        "generated_by": "tools/fold_thunk_gate.py",
        "build": BUILD_ID,
        "totals": {
            "pairs": len(rows),
            "admitted_pairs": len(adm), "admitted_sites": sum(r["sites"] for r in adm),
            "refused_pairs": len(ref), "refused_sites": sum(r["sites"] for r in ref),
            "groups": len(groups),
        },
        "by_tier": dict(collections.Counter(r["tier"] for r in adm)),
        "pairs": rows,
    }
    Path(ROOT / args.out).write_text(json.dumps(out, indent=1) + "\n")

    print("%-6s %-8s %5s  %s" % ("verdict", "tier", "sites", "pair"))
    for r in sorted(rows, key=lambda x: -x["sites"]):
        print("%-6s %-8s %5d  %s <- %s" % (r["verdict"], r["tier"] or "-", r["sites"],
                                           r["survivor"][:46], r["folded"][:46]))
        print("         %s" % (r.get("reason") or r.get("discredit") or r["body_evidence"]))
    print("\nADMIT %d pairs / %d sites in %d groups; REFUSE %d pairs / %d sites"
          % (len(adm), sum(r["sites"] for r in adm), len(groups),
             len(ref), sum(r["sites"] for r in ref)))
    print("-> %s" % args.out)

    if args.install:
        install(groups, Path(ROOT / args.aliases))


OWNED = "Fold-thunk alias group derived by tools/fold_thunk_gate.py."


def install(groups, path):
    doc = json.loads(path.read_text())
    existing = {g["survivor"]: g for g in doc["groups"]}
    added = updated = 0
    for (S, addr), rs in sorted(groups.items()):
        folded = sorted({r["folded"] for r in rs})
        tiers = sorted({r["tier"] for r in rs})
        ev = (OWNED + " Evidence tier(s) %s. Our compiled COMDAT for each folded "
              "spelling below is byte-identical to the RETAIL body at the survivor address "
              "%s once relocation-carrying fields are masked, with every relocated branch "
              "destination RESOLVED through the map and NAME-equal rather than masked away; "
              "the survivor takes %d direct branches across .text. FT1=retail's map places "
              "the folded spelling on the same body or nowhere. FT2=it places it on a "
              "different body the image itself discredits (ZERO REFERENCES -- no branch, no "
              "address-taken immediate pair, no .rdata/.data pointer word -- or no "
              "symbols.txt extent -- padding or a mid-function word). FT3=it places it on a "
              "HOMONYM, a distinct function under another module that legitimately carries "
              "the same mangled name, witnessed by dc3's leaked ham_xbox_r.map. "
              "FT-EMPTY=the body carries no relocation at all, so the fold is real but the "
              "byte comparison is vacuous. %d folded spelling(s), %d charged name_check "
              "sites. Per spelling: %s"
              % (",".join(tiers), addr, rs[0]["survivor_fanin"], len(folded),
                 sum(r["sites"] for r in rs),
                 " ".join("[%s %s, %d sites: %s]"
                          % (x["folded"], x["tier"], x["sites"],
                             (x.get("discredit") or "nothing in the map contradicts it").strip(" |"))
                          for x in sorted(rs, key=lambda y: -y["sites"]))))
        if S in existing:
            g = existing[S]
            before = (sorted(g.get("folded", [])), g.get("evidence", ""))
            if g.get("evidence", "").startswith(OWNED):
                # a group this tool owns: regenerate it wholesale so a change in the
                # evidence rules shows up in the file instead of silently not applying
                g["folded"] = sorted(folded)
                g["evidence"] = ev
            else:
                g["folded"] = sorted(set(g.get("folded", [])) | set(folded))
                if sorted(g["folded"]) != before[0]:
                    g["evidence"] = g.get("evidence", "") + " || " + ev
            if (sorted(g["folded"]), g["evidence"]) != before:
                updated += 1
        else:
            doc["groups"].append({
                "name": S.split("@")[0].lstrip("?") or S,
                "address": addr, "survivor": S, "folded": folded, "evidence": ev,
            })
            added += 1
    # NEVER re-sort: the file is 1,347 hand-audited groups and a reorder makes the
    # diff unreviewable. New groups append; re-running is a no-op.
    note = ("FOLD-THUNK TIER (FT, lane H, 2026-08-12) -- the groups whose evidence "
            "string names tools/fold_thunk_gate.py depart from the 'exactly ONE map-resident "
            "symbol per group' gate above, DELIBERATELY. In this class BOTH spellings are "
            "map-resident, which is the whole reason the earlier residency triage misread "
            "them as wrong callees: target_symbol_map.json is a VA->name FUNCTION over an "
            "ICF-folded link, so it parks the loser of a fold on whatever address is left -- "
            "padding, a mid-function word, an unrelated thunk, or ANOTHER MODULE'S FUNCTION "
            "OF THE SAME NAME. Each FT group therefore carries, per folded spelling, either "
            "proof that the map's other address holds the same body or a measured reason it "
            "is not retail's copy of our symbol. Do NOT re-add a score-based discredit: a low "
            "`none` score says two bodies differ, never which of them is misnamed. See "
            "docs/plans/fold-thunk-alias-gate-2026-08-12.md.")
    doc["_comment"] = [c for c in doc["_comment"]
                       if not (isinstance(c, str) and c.startswith("FOLD-THUNK TIER"))]
    if note not in doc["_comment"]:
        doc["_comment"].append(note)
    path.write_text(json.dumps(doc, indent=2))
    print("installed: %d new group(s), %d updated; %d total" % (added, updated, len(doc["groups"])))


if __name__ == "__main__":
    main()
