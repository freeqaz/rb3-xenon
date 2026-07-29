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
  NOT_A_THUNK    small, ends in an unconditional `b`, but the instruction
                 sequence is a plain TAIL CALL, not an adjustor thunk
                 -> this channel says nothing about it; use another oracle
  NO_CALLEE      not thunk-shaped and no branch to read
                 -> needs a different oracle (strings / rb3-Wii / DC3)
  UNGATED_*      the same verdict, but derived from an UNGATED callee name
                 -> ADVISORY ONLY, ~75.7% reliable.  Never act on these.

★ WHY THE RESIDUE PATH WAS REWRITTEN (2026-07-29, lane docfix)
--------------------------------------------------------------
The original residue path invented a 41-class "missing virtual override"
worklist that a lane investigated in full before `26284d0d` REFUTED it
end-to-end: NO `virtual` was missing anywhere.  All 73 historical NOT_PORTED
rows were artifacts of this path, from four independent bugs:

  36  `??_G`/`??_E` SCOPE FOLD -- MSVC names the deleting-dtor BODY `??_G<C>`
      but every adjustor thunk of it `??_E<C>`.  The check demanded a `$`-name
      whose scope is `??_G<C>@@`, which CANNOT EXIST FOR ANY CLASS.
  25  NOT A THUNK AT ALL -- plain tail calls (`mr r4,r3; li r3,16; b PoolFree`
      = operator delete; `addi r3,r3,16; b ~String`; a bare `b`).  The test was
      "size <= 0x20 and ends in an unconditional `b`", which is not a thunk test.
  12  real thunk, non-dtor scope -- bad callee names inherited from the map.
   +  `"$" in name` MISSES the `W<n>@` SIMPLE-ADJUSTOR form entirely; MSVC uses
      `W`, not `$`, when there is no vtordisp, and MOST multiple-inheritance
      thunks are `W`-form.

and a fifth, systemic one: the residue path read the callee's name from the
UNGATED map (the measured-75.7% tier) while the resolved path correctly gated
it -- so residue verdicts were produced at a reliability the tool's own
docstring calls "unsafe".

Both paths now share `thunk_shape.py` with `thunk_identity_namer.py` (whose
derivation was validated per-class against retail machine code) and derive the
thunk's name as the TOTAL FUNCTION of (callee prefix, vtordisp, this-adjust)
that it is, folding `??_G`/`??_E`.  Ungated rows are still emitted -- a lane may
want the lead -- but they are prefixed `UNGATED_`, carry `trust_gated: false`,
and the summary prints a loud banner.  A silently-wrong classifier is the exact
failure mode this rewrite exists to prevent.

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
from thunk_shape import shape as thunk_shape, td, prefix, norm  # noqa: E402

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
    ap.add_argument("--map", dest="mapf", default=None,
                    help="alternate target_symbol_map.json (regression fixtures)")
    args = ap.parse_args()

    raw = json.load(open(args.mapf or MAP_PATH))
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
    # (norm(prefix), (vtordisp, this_adjust)) -> {unit}.  This is the key a thunk
    # name IS -- folding ??_G/??_E and accepting both the W- and $4- spellings --
    # so "does our tree define this thunk anywhere?" is answered on the encoding,
    # not on a scope string that can never match.
    thunk_key_units = collections.defaultdict(set)
    thunk_keys = {}   # unit -> {(normprefix, (vt, adj)): [names]}
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
        tk = collections.defaultdict(list)
        for f in bf:
            if SKIP_RX.match(f["name"]):
                continue
            bn.setdefault(f["name"], f["masked"])
            bc[f["masked"]].append(f["name"])
            name_units[f["name"]].add(u)
            enc = td(f["name"])
            if enc:
                p = prefix(f["name"])
                if p:
                    tk[(norm(p), enc)].append(f["name"])
                    thunk_key_units[(norm(p), enc)].add(u)
        base[u] = bn
        base_class[u] = bc
        thunk_keys[u] = tk
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
            tgt_key = None
            shape = None
            # ADJUSTOR THUNK -- decode the instruction sequence (thunk_shape),
            # never "small + ends in b".  The derived name is the total function
            # of (callee prefix, vtordisp, this-adjust), ??_G/??_E folded.
            th = thunk_shape(img.word, va, sz)
            if th:
                cn = trust.get(th[2])
                p = prefix(cn) if cn else None
                if p:
                    tgt_key, shape = (norm(p), (th[0], th[1])), "thunk"
            if tgt_key is None and tgt_scope is None:
                # deleting dtor: first gated bl that names a class
                for off in range(0, min(sz or 0, 0x60), 4):
                    br = img.branch(va + off)
                    if br and br[0]:
                        cn = trust.get(br[1])
                        if cn and dtor_class(cn):
                            tgt_scope, shape = "??_G" + dtor_class(cn), "delete"
                            break
            if tgt_key is None and tgt_scope is None:
                continue
            # the derived name is the symbol in THIS unit's obj that carries the
            # derived encoding AND is reloc-masked identical to the VA -- not a
            # guess, and not merely a shared scope string
            if shape == "thunk":
                want = [n for n in base_class.get(u, {}).get(mk, [])
                        if td(n) == tgt_key[1]
                        and prefix(n) and norm(prefix(n)) == tgt_key[0]]
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
    ungated = 0
    for va in sorted(unresolved):
        u, sz, mk = tgt[va]
        th = thunk_shape(img.word, va, sz)
        if not th:
            # NOT a thunk.  Say so explicitly instead of silently promoting a
            # plain tail call into a "class not ported" claim (25 of the 73
            # historical false positives were exactly this).
            last = img.branch(va + sz - 4) if sz else None
            classify.append(dict(
                va="0x%08x" % va, unit=u, old=m[va], size=sz,
                verdict="NOT_A_THUNK" if (last and not last[0]) else "NO_CALLEE",
                tail_target=("0x%08x" % last[1]) if (last and not last[0]) else None,
                tail_name=(m.get(last[1]) if (last and not last[0]) else None)))
            continue
        vt, adj, callee = th
        cn = trust.get(callee)
        gated = cn is not None
        if cn is None:
            cn = m.get(callee)          # ungated: ADVISORY ONLY (~75.7%)
        row = dict(va="0x%08x" % va, unit=u, old=m[va], size=sz,
                   vtordisp=vt, this_adjust=adj,
                   callee="0x%08x" % callee, callee_name=cn,
                   trust_gated=gated)
        if not cn:
            row["verdict"] = "CALLEE_UNNAMED"
            classify.append(row)
            continue
        p = prefix(cn)
        if not p:
            row["verdict"] = "CALLEE_NO_PREFIX"
            classify.append(row)
            continue
        key = (norm(p), (vt, adj))
        row["derived_key"] = "%s | vt=%s adj=%s" % (key[0], vt, adj)
        here = thunk_keys.get(u, {}).get(key, [])
        if not here:
            elsewhere = sorted(thunk_key_units.get(key, ()))
            verdict = "MIS_ATTRIBUTED" if elsewhere else "NOT_PORTED"
            row["also_in"] = elsewhere[:4]
        else:
            same = [n for n in here if base[u][n] == mk]
            verdict = "TAKEN" if same else "BODY_DIVERGENT"
            row["cands"] = here[:4]
        if not gated:
            verdict = "UNGATED_" + verdict
            ungated += 1
        row["verdict"] = verdict
        classify.append(row)

    Path(args.emit).write_text(json.dumps(
        {"0x%08x" % k: v for k, v in resolved.items()}, indent=1))
    Path(args.classify).write_text(json.dumps(classify, indent=1))
    for k in sorted(stat):
        print(f"{k:24s} {stat[k]}")
    print(f"\nduplicate VAs in a pinned unit : {len(dupvas & set(tgt))}")
    print(f"RESOLVED by gated callee       : {len(resolved)}")
    print("residue:", collections.Counter(
        c["verdict"] for c in classify if c["verdict"] != "RESOLVED").most_common())
    if ungated:
        print("\n" + "!" * 72)
        print("!! %d residue rows were derived from an UNGATED callee name." % ungated)
        print("!! Measured reliability of ungated callee identity on this project")
        print("!! is 75.7%%.  Those rows carry an UNGATED_ prefix and")
        print("!! trust_gated=false.  They are LEADS, NOT FINDINGS -- do not open a")
        print("!! source investigation on one without confirming it independently.")
        print("!" * 72)
    npr = [c for c in classify if c["verdict"] == "NOT_PORTED"]
    if npr:
        print("\nNOT_PORTED (gated, thunk-shape-verified, ??_G/??_E folded): %d" % len(npr))
        print("  Each means: retail has an adjustor thunk with this exact")
        print("  (vtordisp, this-adjust) encoding for this callee's class, and NO")
        print("  obj in our tree defines one.  Before calling it a missing")
        print("  `virtual`, read 26284d0d -- every such lead so far was a WRONG")
        print("  NAME in target_symbol_map.json, not a source defect.")
        for c in npr[:20]:
            print("  %s %-34s %s" % (c["va"], c["unit"][:34], c.get("callee_name", "")[:60]))


if __name__ == "__main__":
    main()
