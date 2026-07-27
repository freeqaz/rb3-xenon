#!/usr/bin/env python3
"""dupname_identity_resolver — what is ACTUALLY at a duplicate-name VA.

The question
------------
`dupname_rebijection.py` makes the map injective by moving a surplus VA onto
some *other* byte-class-identical name.  That is score-correct but identity-
arbitrary: it answers "a name that fits", not "the function that lives here".
When no spare name is left the only remaining move is to drop the entry, which
is an admission of defeat.  This tool answers the real question instead.

The instrument
--------------
Thunks and deleting destructors *name themselves through their relocations*:

  adjustor thunk   lwz r11,-N(r3); subf r3,r11,r3; b   ?Meth@Class@@UAA<sig>
  scalar-del dtor  ...            ; bl  ??1Class@@UAA@XZ ; bl ??3@YAXPAX@Z
  vector-del dtor  ...            ; bl  ??_EClass@@ / ??1Class@@

So the *callee* determines the caller's identity by construction -- not by
similarity.  Once the callee is known, the thunk's name is not guessed: it is
the unique symbol in this unit's own obj whose name carries the same
`Meth@Class@@` prefix in thunk form AND is reloc-masked identical to the VA.

Why it needs a trust gate
-------------------------
Callee names come from the same map, and much of the map is
`_bijection_arbitrary` (byte-class assignments that score but do not assert
identity).  Resolving against those would launder arbitrary names into
"proven" ones.  Measured discriminator reliability on this project:

    byte-identity 99.5%  >  strings  >  floats
      >  trust-gated callees 95.1%  >>  ungated callees 75.7% (unsafe)

So the callee set is gated to a trust set T, and T is grown to a fixpoint:

  T0  VAs whose reloc-masked bytes match EXACTLY ONE symbol in their own unit's
      obj, and the map already assigns that symbol.  Byte-unique identity, the
      99.5% tier.  Excludes every `_bijection_arbitrary` / `_icf_arbitrary` /
      duplicated entry by construction.
  T(n+1) = T(n) + every VA resolved this round (each resolution is itself
      anchored on byte identity in the VA's own obj, so it enters at the same
      tier it was gated on -- the induction does not decay).

Residue classification
----------------------
When a VA cannot be resolved, WHY is the actionable output:

  NOT_PORTED     the derived name does not exist in this unit's obj at all
                 -> source work: that class/method is missing from our tree
  MIS_ATTRIBUTED the derived name exists, but in a DIFFERENT unit's obj
                 -> splits/unit attribution is wrong, not the source
  BODY_DIVERGENT the derived name exists in this unit's obj but the bytes
                 differ -> ordinary decomp work, with a measured byte delta
  NO_CALLEE      the VA is not thunk-shaped and has no gated callee
                 -> needs a different oracle (strings / rb3-Wii / DC3)

Read-only.  `--emit` writes a fragment; nothing is applied by this tool.
"""
import argparse
import bisect
import collections
import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts" / "harvest"))
from size_order_automap import _ordered_funcs, _asm_target_funcs  # noqa: E402

BUILD = ROOT / "build" / "45410914"
MAP_PATH = ROOT / "scripts" / "target_symbol_map.json"
SPLITS = ROOT / "config" / "45410914" / "splits.txt"
IMAGE = ROOT / "orig" / "45410914" / "band.exe"
SKIP_RX = re.compile(r"^__unwind\$|^\$|^\?\?_9")


# --------------------------------------------------------------------------
# retail image
# --------------------------------------------------------------------------
class Image:
    def __init__(self, path):
        d = open(path, "rb").read()
        pe = struct.unpack_from("<I", d, 0x3C)[0]
        nsec = struct.unpack_from("<H", d, pe + 6)[0]
        oh = struct.unpack_from("<H", d, pe + 20)[0]
        ib = struct.unpack_from("<I", d, pe + 24 + 28)[0]
        self.d = d
        self.secs = []
        for i in range(nsec):
            o = pe + 24 + oh + i * 40
            vs, va, rs, ro = struct.unpack_from("<IIII", d, o + 8)
            self.secs.append((ib + va, vs, ro))

    def word(self, va):
        for sva, vs, ro in self.secs:
            if sva <= va < sva + vs:
                off = ro + (va - sva)
                return struct.unpack_from(">I", self.d, off)[0]
        return None

    def branch(self, va):
        """(is_link, target) for a b/bl at va, else None."""
        i = self.word(va)
        if i is None:
            return None
        if i >> 26 != 18:
            return None
        li = i & 0x03FFFFFC
        if li & 0x02000000:
            li -= 0x04000000
        aa = (i >> 1) & 1
        return (bool(i & 1), li if aa else va + li)


def load_units():
    units = collections.defaultdict(list)
    cur = None
    for line in open(SPLITS):
        if not line.strip() or line.startswith("Sections:"):
            continue
        if not line[0].isspace():
            cur = line.strip().rstrip(":")
            continue
        p = line.split()
        if len(p) >= 3 and p[0] == ".text" and p[1].startswith("start:"):
            units[cur].append((int(p[1].split(":")[1], 16),
                               int(p[2].split(":")[1], 16)))
    ranges = sorted((s, e, u) for u, rs in units.items() for s, e in rs)
    starts = [r[0] for r in ranges]

    def unit_of(va):
        i = bisect.bisect_right(starts, va) - 1
        if i >= 0 and ranges[i][0] <= va < ranges[i][1]:
            return ranges[i][2]
        return None
    return sorted(units), unit_of


def unit_paths(unit):
    rel = unit[:-4] if unit.endswith(".cpp") else unit
    asm = BUILD / "asm" / (rel + ".s")
    bobj = BUILD / "src" / (rel + ".obj")
    if not bobj.exists():
        c = list((BUILD / "src").rglob(Path(rel).name + ".obj"))
        bobj = c[0] if len(c) == 1 else bobj
    return asm, bobj


# --------------------------------------------------------------------------
# name algebra: Meth@Class@@ prefix shared by a virtual and its thunks
# --------------------------------------------------------------------------
def scope_of(name):
    """'?Copy@UIComponent@@$4PP...' / '?Copy@UIComponent@@UAA...' -> '?Copy@UIComponent@@'"""
    if not name.startswith("?"):
        return None
    i = name.find("@@")
    return name[:i + 2] if i > 0 else None


def dtor_class(name):
    """'??1Foo@@UAA@XZ' or '??_GFoo@@...' -> 'Foo@@'"""
    for p in ("??1", "??0", "??_G", "??_E"):
        if name.startswith(p):
            i = name.find("@@", len(p))
            return name[len(p):i + 2] if i > 0 else None
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--emit", default="/home/free/tmp/dupname/identity.json")
    ap.add_argument("--classify", default="/home/free/tmp/dupname/classify.json")
    ap.add_argument("--rounds", type=int, default=6)
    args = ap.parse_args()

    raw = json.load(open(MAP_PATH))
    m = {}
    for k, v in raw.items():
        if k.lower().startswith("0x") and isinstance(v, str):
            m[int(k, 16)] = v
    arbitrary = {int(x, 16) for x in raw.get("_bijection_arbitrary", [])}
    arbitrary |= {int(x, 16) for x in raw.get("_icf_arbitrary", [])}
    byname = collections.defaultdict(list)
    for va, n in m.items():
        byname[n].append(va)
    dups = {n: sorted(v) for n, v in byname.items() if len(v) > 1}
    dupvas = {va for v in dups.values() for va in v}

    img = Image(IMAGE)
    unit_list, unit_of = load_units()

    # ------------------------------------------------------------------
    # per-unit tables
    # ------------------------------------------------------------------
    tgt = {}          # va -> (unit, size, masked)
    base = {}         # unit -> {name: masked}
    base_class = {}   # unit -> {masked: [names]}
    name_units = collections.defaultdict(set)   # name -> units whose obj defines it
    todo = collections.defaultdict(list)
    for va in m:
        u = unit_of(va)
        if u:
            todo[u].append(va)
    stat = collections.Counter()
    for u in sorted(todo):
        asm, bobj = unit_paths(u)
        if not (asm.exists() and bobj.exists()):
            stat["unit_no_artifacts"] += 1
            continue
        try:
            tf = {va: (sz, mk) for va, sz, mk in _asm_target_funcs(asm) if va}
            bf = _ordered_funcs(bobj)
        except Exception:
            stat["unit_parse_fail"] += 1
            continue
        bn, bc = {}, collections.defaultdict(list)
        for f in bf:
            if SKIP_RX.match(f["name"]):
                continue
            bn.setdefault(f["name"], f["masked"])
            bc[f["masked"]].append(f["name"])
            name_units[f["name"]].add(u)
        base[u] = bn
        base_class[u] = bc
        for va in todo[u]:
            t = tf.get(va)
            if t:
                tgt[va] = (u, t[0], t[1])

    # ------------------------------------------------------------------
    # T0: byte-unique identity (the 99.5% tier)
    # ------------------------------------------------------------------
    trust = {}
    for va, (u, sz, mk) in tgt.items():
        if va in arbitrary or va in dupvas:
            continue
        cands = base_class.get(u, {}).get(mk, [])
        if len(cands) == 1 and cands[0] == m[va]:
            trust[va] = m[va]
    stat["T0_byte_unique"] = len(trust)

    # ------------------------------------------------------------------
    # iterate: resolve duplicate VAs from gated callees
    # ------------------------------------------------------------------
    resolved = {}
    classify = []
    unresolved = set(dupvas & set(tgt))
    for rnd in range(args.rounds):
        got = 0
        for va in sorted(unresolved):
            u, sz, mk = tgt[va]
            # collect relocation-carried callees
            tgt_scope = None
            shape = None
            if sz and sz <= 0x20:
                for off in range(0, sz, 4):
                    br = img.branch(va + off)
                    if br and not br[0]:                 # unconditional b -> thunk
                        cn = trust.get(br[1])
                        if cn:
                            tgt_scope, shape = scope_of(cn), "thunk"
                        break
            if tgt_scope is None:
                # deleting dtor: first gated bl that names a class
                for off in range(0, min(sz or 0, 0x60), 4):
                    br = img.branch(va + off)
                    if br and br[0]:
                        cn = trust.get(br[1])
                        if cn and dtor_class(cn):
                            tgt_scope, shape = "??_G" + dtor_class(cn), "delete"
                            break
            if tgt_scope is None:
                continue
            # the derived name is the symbol in THIS unit's obj that shares the
            # scope and is reloc-masked identical to the VA -- not a guess
            if shape == "thunk":
                want = [n for n in base_class.get(u, {}).get(mk, [])
                        if scope_of(n) == tgt_scope and "$" in n]
            else:
                cls = tgt_scope[4:]
                want = [n for n in base_class.get(u, {}).get(mk, [])
                        if n.startswith("??_G" + cls) or n.startswith("??_E" + cls)]
            if len(want) == 1:
                resolved[va] = want[0]
                trust[va] = want[0]
                unresolved.discard(va)
                got += 1
                classify.append(dict(va="0x%08x" % va, unit=u, old=m[va],
                                     derived=want[0], verdict="RESOLVED",
                                     shape=shape, round=rnd))
        stat["round%d_resolved" % rnd] = got
        if not got:
            break

    # ------------------------------------------------------------------
    # residue classification
    # ------------------------------------------------------------------
    for va in sorted(unresolved):
        u, sz, mk = tgt[va]
        scope = None
        shape = None
        if sz and sz <= 0x20:
            for off in range(0, sz, 4):
                br = img.branch(va + off)
                if br and not br[0]:
                    scope, shape = scope_of(m.get(br[1], "")), "thunk"
                    break
        if not scope:
            classify.append(dict(va="0x%08x" % va, unit=u, old=m[va],
                                 verdict="NO_CALLEE", size=sz))
            continue
        want_here = [n for n in base.get(u, {}) if scope_of(n) == scope and "$" in n]
        if not want_here:
            elsewhere = sorted({uu for n, us in name_units.items()
                                for uu in us if scope_of(n) == scope and "$" in n})
            classify.append(dict(
                va="0x%08x" % va, unit=u, old=m[va], scope=scope, size=sz,
                verdict="MIS_ATTRIBUTED" if elsewhere else "NOT_PORTED",
                also_in=elsewhere[:4]))
        else:
            same = [n for n in want_here if base[u][n] == mk]
            classify.append(dict(va="0x%08x" % va, unit=u, old=m[va], scope=scope,
                                 size=sz,
                                 verdict="BODY_DIVERGENT" if not same else "TAKEN",
                                 cands=want_here[:4]))

    Path(args.emit).write_text(json.dumps(
        {"0x%08x" % k: v for k, v in resolved.items()}, indent=1))
    Path(args.classify).write_text(json.dumps(classify, indent=1))
    for k in sorted(stat):
        print(f"{k:24s} {stat[k]}")
    print(f"\nduplicate VAs in a pinned unit : {len(dupvas & set(tgt))}")
    print(f"RESOLVED by gated callee       : {len(resolved)}")
    print("residue:", collections.Counter(
        c["verdict"] for c in classify if c["verdict"] != "RESOLVED").most_common())


if __name__ == "__main__":
    main()
