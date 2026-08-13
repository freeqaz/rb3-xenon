#!/usr/bin/env python3
"""icf_alias_map_build.py -- populate the ICF (identical-COMDAT-folding) alias map.

WHAT AN "ICF ALIAS GROUP" IS
    Retail RB3 links with `/O1 /Oi /GR /EHsc` and ICF on, so N distinct C++
    mangled names whose machine code is byte-identical collapse onto ONE `.text`
    virtual address.  The XEX has no symbol table, so `scripts/target_symbol_map.
    json` can name that VA only ONCE -- every other spelling of the fold looks,
    to any name-based comparison, like a WRONG callee.  An alias group is the
    set of names that legitimately share one retail body.

    Before this tool the project's map held **3 groups / 7 names**
    (`scripts/symbol_aliases.json` -> `build/45410914/icf_aliases.map`).

★ READ THIS BEFORE ASSUMING THE MAP MOVES THE HEADLINE
    It does not, and cannot.  `objdiff-cli report generate` hardcodes
    `function_reloc_diffs = None` (objdiff `objdiff-cli/src/cmd/report.rs:392`).
    Under `None`, `reloc_eq` (objdiff `objdiff-core/src/diff/code.rs:874-897`)
    returns `true` as soon as the relocation FLAGS agree -- it never looks at the
    symbol names, so `symbol_equivalences` (which the map feeds) is dead code on
    the report path.  Measured, not argued: regenerating the report with a
    1,408-group / 9,325-line map moved `matched_functions`, `matched_code`,
    `fuzzy_match_percent` and `masked_equal_functions` by **exactly 0**.

    The map's real value is (a) killing `[sym]` noise in `objdiff-cli diff` runs
    that use a name-comparing reloc mode, and (b) supplying an ICF oracle to
    analysis tools -- above all `reloc_correspondence.py`, whose UNDECIDABLE
    bucket is dominated by exactly this ambiguity.

SOURCES (combined, provenance recorded per group)
    dc3      /home/free/code/milohax/dc3-decomp/orig/373307D9/ham_xbox_r.map
             A LEAKED RETAIL LINKER MAP for Dance Central 3 -- same Milo engine,
             same MSVC X360 toolchain, same `/O1 /Oi /GR /EHsc` flags, same
             linker.  Multiple names at one CODE address in that map are ICF
             folds DIRECTLY OBSERVED from the same toolchain.  This is the
             highest-trust source available.  Caveat: dc3 is a NEWER engine
             build, so a dc3 fold is strong evidence -- not proof -- that RB3
             folded the same pair.
    laneab   docs/plans/laneAB-icf-tie-alternates-2026-07-26.json -- 25
             hand-verified VA -> {names} tie groups from the bijection work.
    hand     docs/plans/lane-bo8-icf-handverified.json -- groups with a STATED
             PROOF (e.g. lane BO-1's vtable-slot proof of the NetGameMsgs
             Save/Load folds).  This source OUTRANKS the contradiction quarantine:
             when it disagrees with target_symbol_map.json, the map is what is
             wrong, so the group is emitted anchored at the hand-declared VA AND
             recorded in the contradictions worklist for the map's owner.
    derived  Our own compiled objs.  If two distinct named functions in OUR
             build have byte-identical bodies AND identical relocation shape AND
             identical relocation TARGET NAMES, the retail linker would have
             folded them.  Sound but conditional on our codegen being right, so
             it is the weakest of the three.

MERGING
    Groups are unioned across sources by name (union-find), because a fold is an
    equivalence relation.  A merged class is then adjudicated against
    `target_symbol_map.json`:
        exactly 1 mapped name -> SURVIVOR group, keyed on that name's retail VA.
        0 mapped names        -> group is real but un-anchored; emitted with a
                                 synthetic key (the map only needs a bucket).
        >= 2 mapped names     -> CONTRADICTION.  Our evidence says one body, the
                                 map says two VAs: either our codegen diverges
                                 there or the map is mispaired.  QUARANTINED (not
                                 emitted) unless a `hand` proof anchors it, and
                                 always written to the contradictions worklist.

USAGE
    python3 scripts/harvest/icf_alias_map_build.py --out scripts/icf_alias_groups.json
    python3 scripts/harvest/icf_alias_map_build.py --sources dc3,laneab --stats
"""

import argparse
import json
import os
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(HERE))

DC3_MAP = Path("/home/free/code/milohax/dc3-decomp/orig/373307D9/ham_xbox_r.map")
LANEAB = ROOT / "docs" / "plans" / "laneAB-icf-tie-alternates-2026-07-26.json"
HAND = ROOT / "docs" / "plans" / "lane-bo8-icf-handverified.json"
TSM = ROOT / "scripts" / "target_symbol_map.json"
OBJ_ROOT = ROOT / "build" / "45410914" / "src"

# Compiler-generated funclets are mask-identical binary-wide and carry no
# independent identity; folding claims about them are vacuous.  Same predicate
# byte_locate.Func.is_funclet uses, plus objdiff's `fn_`/`??__E`/`??__F` shapes.
SAMPLE = 24            # member-sample cap for the contradictions worklist

FUNCLET_PREFIXES = ("__unwind$", "__catch$", "__ehhandler$", "__tryblocktable",
                    "__unwindfunclet$", "??__E", "??__F")


def is_funclet(name):
    if name.startswith(FUNCLET_PREFIXES):
        return True
    return bool(re.fullmatch(r"fn_[0-9a-fA-F]{8}", name))


# ---------------------------------------------------------------------------
# union-find
# ---------------------------------------------------------------------------
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
            self.p[rb] = ra

    def classes(self):
        out = defaultdict(list)
        for x in self.p:
            out[self.find(x)].append(x)
        return out


# ---------------------------------------------------------------------------
# source: dc3 leaked linker map
# ---------------------------------------------------------------------------
# " 0001:00000000       ?Foo@@YAXXZ                      82331360  f i obj"
MAP_LINE = re.compile(r"^\s*(\d{4}):([0-9a-fA-F]+)\s+(\S+)\s+([0-9a-fA-F]{8})\s")
SEC_LINE = re.compile(r"^\s*(\d{4}):[0-9a-fA-F]+\s+[0-9a-fA-F]+H\s+(\S+)\s+(CODE|DATA)\s*$")


def load_dc3_groups(path=DC3_MAP):
    """-> {addr_int: [names]} for CODE sections only, groups of size >= 2."""
    if not path.exists():
        return {}, "missing"
    code_secs = set()
    by_addr = defaultdict(set)
    with open(path, "r", errors="replace") as fh:
        for line in fh:
            m = SEC_LINE.match(line)
            if m:
                if m.group(3) == "CODE":
                    code_secs.add(m.group(1))
                continue
            m = MAP_LINE.match(line)
            if not m:
                continue
            sec, _, name, addr = m.groups()
            if sec not in code_secs:
                continue
            by_addr[int(addr, 16)].add(name)
    groups = {a: sorted(ns) for a, ns in by_addr.items() if len(ns) > 1}
    return groups, "ok"


# ---------------------------------------------------------------------------
# source: laneAB hand-verified tie alternates
# ---------------------------------------------------------------------------
def load_va_name_json(path):
    """{'0xVA': [names]} -- the laneAB alternates and the hand-verified file."""
    if not path.exists():
        return {}, "missing"
    raw = json.load(open(path))
    return ({int(k, 16): sorted(set(v)) for k, v in raw.items()
             if not k.startswith("_") and len(set(v)) > 1}, "ok")


def load_laneab_groups(path=LANEAB):
    return load_va_name_json(path)


# ---------------------------------------------------------------------------
# source: derived from our own compiled objects
# ---------------------------------------------------------------------------
IMAGE_SCN_CNT_CODE = 0x20
PAD_WORDS = (b"\x00\x00\x00\x00", b"\x60\x00\x00\x00")


def _obj_bodies(path):
    """-> [(name, masked_body, reloc_shape, sel)] for one COFF obj.

    `reloc_shape` is the ordered tuple of (word_index, reloc_type, target_name)
    -- carrying the target NAME is what makes the STRICT key strict: two bodies
    whose only difference is `bl A` vs `bl B` are NOT declared folded unless A
    and B are the same name.  (That is deliberately conservative: they would in
    fact also fold if A and B themselves folded.  Computing that fixed point is
    left out -- it can only ADD groups, so the emitted set stays sound.)
    """
    import byte_locate as BL
    data, secs, syms = BL.parse_coff(path)
    by_sec = defaultdict(list)
    symname = {}
    for sy in syms:
        symname[sy["idx"]] = sy["name"]
    for sy in syms:
        if (sy["secn"] > 0 and sy["secn"] in secs and sy["typ"] == 0x20
                and sy["sc"] in (2, 3)
                and secs[sy["secn"]]["chars"] & IMAGE_SCN_CNT_CODE):
            by_sec[sy["secn"]].append(sy)
    out = []
    for secn in sorted(by_sec):
        s = secs[secn]
        members = sorted(by_sec[secn], key=lambda x: x["val"])
        for k, sy in enumerate(members):
            start = sy["val"]
            end = members[k + 1]["val"] if k + 1 < len(members) else s["raw_size"]
            if s["praw"] == 0 or end <= start:
                continue
            body = bytearray(data[s["praw"] + start:s["praw"] + end])
            shape = []
            for rva, symidx, rtype in s["relocs_full"]:
                if start <= rva < end:
                    off = rva - start
                    shape.append((off // 4, rtype, symname.get(symidx, "?")))
                    body[off:off + 4] = b"\0\0\0\0"
            masked = {w for w, _, _ in shape}
            while len(body) >= 4 and bytes(body[-4:]) in PAD_WORDS \
                    and (len(body) // 4 - 1) not in masked:
                del body[-4:]
            nw = len(body) // 4
            shape = [t for t in shape if t[0] < nw]
            if len(body) < 8:      # a 1-instruction body is a vacuous class key
                continue
            out.append((sy["name"], bytes(body), tuple(sorted(shape)),
                        s.get("sel", 0)))
    return out


def load_derived_groups(obj_root=OBJ_ROOT, min_size=8, verbose=False):
    """-> ({synthetic_key: [names]}, stats).  Byte+reloc-target identity classes
    over every named non-funclet code symbol in our compiled tree."""
    objs = sorted(obj_root.rglob("*.obj"))
    buckets = defaultdict(set)
    emitted = set()
    nfun = 0
    for i, p in enumerate(objs):
        try:
            fns = _obj_bodies(p)
        except Exception:
            continue
        for name, body, shape, sel in fns:
            emitted.add(name)
            if is_funclet(name):
                continue
            if len(body) < min_size:
                continue
            nfun += 1
            buckets[(body, shape)].add(name)
        if verbose and i % 200 == 0:
            print(f"  ... {i}/{len(objs)} objs", file=sys.stderr)
    groups = {}
    n = 0
    for key, names in buckets.items():
        if len(names) < 2:
            continue
        groups[n] = sorted(names)
        n += 1
    return groups, dict(objs=len(objs), symbols=nfun, classes=len(groups),
                        emitted=emitted)


# ---------------------------------------------------------------------------
# adjudication
# ---------------------------------------------------------------------------
def load_tsm():
    raw = json.load(open(TSM))
    name2va = {}
    for k, v in raw.items():
        if k.startswith("_"):
            continue
        name2va.setdefault(v, int(k, 16))
    return name2va


def build(sources, verbose=False):
    name2va = load_tsm()
    uf = UF()
    prov = defaultdict(set)
    stats = {}
    emitted_names = None

    if "dc3" in sources:
        g, st = load_dc3_groups()
        stats["dc3"] = dict(status=st, groups=len(g),
                            names=sum(len(v) for v in g.values()))
        for names in g.values():
            names = [n for n in names if not is_funclet(n)]
            for n in names[1:]:
                uf.union(names[0], n)
            for n in names:
                prov[n].add("dc3")
    if "laneab" in sources:
        g, st = load_laneab_groups()
        stats["laneab"] = dict(status=st, groups=len(g),
                               names=sum(len(v) for v in g.values()))
        for names in g.values():
            for n in names[1:]:
                uf.union(names[0], n)
            for n in names:
                prov[n].add("laneab")
    hand_anchor = {}
    if "hand" in sources:
        g, st = load_va_name_json(HAND)
        for va, names in g.items():
            for n in names:
                hand_anchor[n] = va
        stats["hand"] = dict(status=st, groups=len(g),
                             names=sum(len(v) for v in g.values()))
        for names in g.values():
            for n in names[1:]:
                uf.union(names[0], n)
            for n in names:
                prov[n].add("hand")
    if "derived" in sources:
        g, st = load_derived_groups(verbose=verbose)
        emitted_names = st.pop("emitted")
        stats["derived"] = dict(status="ok", **st)
        for names in g.values():
            for n in names[1:]:
                uf.union(names[0], n)
            for n in names:
                prov[n].add("derived")

    if emitted_names is None:
        emitted_names = set()
        for p in sorted(OBJ_ROOT.rglob("*.obj")):
            try:
                for name, _, _, _ in _obj_bodies(p):
                    emitted_names.add(name)
            except Exception:
                pass

    groups, contradictions, unanchored = [], [], []
    for root, members in uf.classes().items():
        members = sorted(set(members))
        if len(members) < 2:
            continue
        mapped = [m for m in members if m in name2va]
        ours = [m for m in members if m in emitted_names]
        src = sorted({s for m in members for s in prov[m]})
        rec = dict(members=members, mapped=mapped, ours=ours, sources=src)
        hva = next((hand_anchor[m] for m in members if m in hand_anchor), None)
        if len(mapped) >= 2:
            rec["vas"] = {m: "0x%08x" % name2va[m] for m in mapped}
            rec["hand_anchor"] = ("0x%08X" % hva) if hva is not None else None
            # This list is an AUDIT WORKLIST, not a map: a mega-fold class of
            # trivial `??_G` thunks can carry thousands of names and would
            # dominate the artifact. Keep the counts exact, sample the members.
            rec["n_members"], rec["n_ours"] = len(members), len(ours)
            if len(members) > SAMPLE:
                rec["members"] = members[:SAMPLE]
                rec["members_truncated"] = True
            rec["ours"] = ours[:SAMPLE]
            contradictions.append(rec)
            # A hand-verified group OUTRANKS the quarantine: the evidence file
            # carries a stated proof, so the map -- not the group -- is what is
            # wrong. Still recorded above, as the map owner's worklist.
            if hva is None:
                continue
        if not ours:
            continue                       # nothing our build can ever reference
        if hva is not None:
            survivor = next(m for m in members if m in hand_anchor)
            addr = "0x%08X" % hva
            anchored = True
        elif len(mapped) == 1:
            survivor = mapped[0]
            addr = "0x%08X" % name2va[survivor]
            anchored = True
        else:
            survivor = members[0]
            addr = "0x%08X" % (0xF0000000 + (abs(hash(members[0])) & 0x0FFFFFFF))
            anchored = False
        folded = [m for m in members if m != survivor]
        g = dict(name=survivor.lstrip("?").split("@")[0][:48] or "group",
                 address=addr, survivor=survivor, folded=folded,
                 evidence="icf_alias_map_build.py sources=%s anchored=%s"
                          % ("+".join(src), anchored),
                 _sources=src, _anchored=anchored,
                 _ours=len(ours), _size=len(members))
        (groups if anchored else unanchored).append(g)
    return groups, unanchored, contradictions, stats


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sources", default="dc3,laneab,derived")
    ap.add_argument("--out")
    ap.add_argument("--contradictions-out")
    ap.add_argument("--include-unanchored", action="store_true",
                    help="also emit groups with no name in target_symbol_map.json")
    ap.add_argument("--stats", action="store_true")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    sources = [s.strip() for s in args.sources.split(",") if s.strip()]
    groups, unanchored, contra, stats = build(sources, verbose=args.verbose)

    print("=== sources ===")
    for k, v in stats.items():
        print(" %-8s %s" % (k, v))
    print("=== merged classes ===")
    print(" anchored groups (1 mapped name)      : %d  (%d alias names)"
          % (len(groups), sum(len(g["folded"]) for g in groups)))
    print(" unanchored groups (0 mapped names)   : %d  (%d alias names)"
          % (len(unanchored), sum(len(g["folded"]) for g in unanchored)))
    print(" CONTRADICTIONS (>=2 mapped names)    : %d   <- map-audit worklist"
          % len(contra))

    if args.stats:
        from collections import Counter
        c = Counter(g["_size"] for g in groups)
        print(" anchored size histogram:",
              ", ".join("%dx%d" % (n, k) for k, n in sorted(c.items())[:12]))

    emit = groups + (unanchored if args.include_unanchored else [])
    if args.out:
        doc = {
            "_comment": [
                "GENERATED by scripts/harvest/icf_alias_map_build.py -- do not hand-edit.",
                "ICF alias groups: names that share ONE retail .text address.",
                "Schema matches scripts/symbol_aliases.json['groups'], but see the WARNING",
                "below before merging; tools/gen_symbol_alias_map.py consumes it.",
                "*** CORRECTED 2026-08-13 (lane RULER-SWEEP). The note that stood here --",
                "'adding groups here CANNOT change matched_functions, the report path",
                "hardcodes functionRelocDiffs=None ... Measured delta: 0' -- IS NO LONGER",
                "A SAFETY GUARANTEE. The report path has NOT hardcoded None since",
                "2026-08-12 (d04c83df): objdiff.json ships functionRelocDiffs=name_check,",
                "under which an ALIAS IS FORGIVENESS -- objdiff consults SymbolEquivalences",
                "and drops the charge for a name pair listed here. So merging these groups",
                "MOVES matched_code / code%. It moves matched_functions little or not at",
                "all, because that key is mpn-based and mpn excludes arg-only penalties --",
                "which makes the old note WORSE than simply wrong: a reader who spot-checks",
                "matched_functions will see it 'confirmed' and merge anyway.",
                "*** DO NOT MERGE THESE 1,400+ GROUPS WHOLESALE. *** An alias must be",
                "licensed by a PROVEN retail fold (byte-identical body INCLUDING",
                "relocations at ONE address), never by a name being 'arbitrary'. An",
                "unproven alias lifts name_check BY CONSTRUCTION with no byte evidence,",
                "and the `none` control CANNOT catch it (none ignores relocation names, so",
                "a fabricated alias reads 0 there by construction -- that flatness is the",
                "signature of the hazard, not a clearance). Measure any merge with",
                "tools/ab_measure.py and adjudicate on retail bytes.",
                "sources=%s" % ",".join(sources),
            ],
            "groups": emit,
            "contradictions": contra,
        }
        Path(args.out).write_text(json.dumps(doc, separators=(",", ":")) + "\n")
        print("wrote %s (%d groups, %d contradictions)"
              % (args.out, len(emit), len(contra)))
    if args.contradictions_out:
        Path(args.contradictions_out).write_text(json.dumps(contra, indent=1) + "\n")


if __name__ == "__main__":
    main()
