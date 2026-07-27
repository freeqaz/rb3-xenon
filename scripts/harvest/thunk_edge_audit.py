#!/usr/bin/env python3
"""thunk_edge_audit -- audit and repair adjustor-thunk entries in the target
symbol map, using two independent, map-free discriminators.

The axiom, verified before it was used
--------------------------------------
An MSVC adjustor thunk fixes `this` and branches to the SAME method of the SAME
class.  Measured on our own compiled objs (real MSVC output): of 7526 `$`-
mangled code symbols with a relocated callee, **7523 are scope-exact**, and the
3 exceptions are not thunks at all -- they are templates whose `$` comes from
template-ARGUMENT mangling (`$$B`, `$07`).  Restricted to genuine adjustor
mangling (`@@$4`, `@@$2`, `@@$R`) the axiom holds 7522/7522.  A scope
disagreement in the map is therefore a proof of error, not a hint.

Discriminator 1 -- NUMBERS (bytes vs name, no map involved)
    The retail thunk encodes {vtordisp, adj} literally:
        lwz  r11, <vtordisp>(r3)
        subf r3, r11, r3
        addi r3, r3, -<adj>          (emitted iff adj != 0)
        b    <callee>
    and the mangled name encodes the same pair as `$4<vtordisp>@<adj>@` in
    MSVC's base-16 A..P digits.  Learned from all 7522 compiled-obj thunks with
    zero exceptions.  A name whose numbers contradict the bytes is provably on
    the wrong VA -- and cannot be matching, since our compiled body for that
    name carries a different `addi` immediate.

Discriminator 2 -- CALLEE (our obj's own relocation)
    For every thunk symbol MSVC emits, the obj relocation names the exact
    callee.  `compiled_callee[thunk_name] -> callee_name` is therefore ground
    truth for the naming relation, with no scope heuristic: it handles `??_E`
    vs `??_G`, cross-class vtordisp thunks and multiple-inheritance chains by
    construction.  The map is consistent at a VA iff
        map[callee_va] == compiled_callee[ map[thunk_va] ]
    with callee VAs gated to the reloc-masked BYTE-UNIQUE tier (~99.5%).

Address proximity is never an input.

The ??_E / ??_G refutation (do not "repair" these)
--------------------------------------------------
Every deleting-destructor thunk our compiler emits targets the
`??_E<X>@@UAAPAXI@Z` spelling, while the body it resolves to in retail is
reloc-masked byte-identical AND byte-unique against our `??_G<X>@@UAAPAXI@Z`;
no obj in the tree defines both spellings.  The two names denote one function,
so an `??_E` thunk pointing at a `??_G`-named VA of the same class is a naming
divergence in OUR build, invisible under objdiff's normalized diff -- not a map
defect.  Folding that equivalence in moved the proven-tier agreement from 66.7%
to 75.6% with no edit at all.  `eg_canon()` implements it.

Repair modes
------------
`--plan group`   STRICT permutation inside a (unit, vtordisp, adj) group.  Every
                 target VA and every compiled thunk symbol in such a group is
                 byte-identical modulo relocations and objdiff normalizes
                 relocation targets away, so any bijection inside the group
                 scores identically; the name multiset is unchanged, so global
                 injectivity is preserved by construction.  Provably neutral --
                 measured 38,949 -> 38,949 with 0 set churn.
`--plan unit`    per-unit bijection allowing CROSS-group moves.  These change
                 bytes (different `addi` immediate / length), so they are not
                 neutral and must be measured.  Measured +3.

Read-only unless `--apply`.  Applying rewrites only the exact map lines whose VA
is in the plan and whose value still equals the plan's `old` -- no reformat, no
re-sort (the map is hot and edited by several lanes concurrently).

Usage
    thunk_edge_audit.py                       # census + agreement rates
    thunk_edge_audit.py --plan group          # emit the neutral permutation
    thunk_edge_audit.py --plan unit --apply   # emit + apply cross-group moves
"""
import argparse
import bisect
import collections
import glob
import json
import os
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts" / "harvest"))
from size_order_automap import _ordered_funcs, _asm_target_funcs  # noqa: E402

BUILD = ROOT / "build" / "45410914"
SRC = BUILD / "src"
MAP_PATH = ROOT / "scripts" / "target_symbol_map.json"
SPLITS = ROOT / "config" / "45410914" / "splits.txt"
SYMBOLS = ROOT / "config" / "45410914" / "symbols.txt"
IMAGE = ROOT / "orig" / "45410914" / "band.exe"

CODE = 0x20
THUNK_RX = re.compile(r"@@\$(4|2|R)")


# ---------------------------------------------------------------------------
# COFF with relocation -> symbol resolution (raw index, aux entries preserved)
# ---------------------------------------------------------------------------
def parse_coff(path):
    data = Path(path).read_bytes()
    nsec = struct.unpack_from("<H", data, 2)[0]
    symoff = struct.unpack_from("<I", data, 8)[0]
    nsym = struct.unpack_from("<I", data, 12)[0]
    optsz = struct.unpack_from("<H", data, 16)[0]
    str_start = symoff + nsym * 18
    raw = [None] * nsym
    i = 0
    while i < nsym:
        o = symoff + i * 18
        nm = data[o:o + 8]
        if nm[:4] == b"\0\0\0\0":
            so = struct.unpack_from("<I", nm, 4)[0]
            e = data.index(b"\0", str_start + so)
            name = data[str_start + so:e].decode("latin1")
        else:
            name = nm.split(b"\0")[0].decode("latin1")
        raw[i] = dict(name=name,
                      val=struct.unpack_from("<I", data, o + 8)[0],
                      secn=struct.unpack_from("<h", data, o + 12)[0],
                      typ=struct.unpack_from("<H", data, o + 14)[0],
                      sc=data[o + 16])
        i += 1 + data[o + 17]
    secs = {}
    base = 20 + optsz
    for k in range(nsec):
        o = base + k * 40
        preloc = struct.unpack_from("<I", data, o + 24)[0]
        nreloc = struct.unpack_from("<H", data, o + 32)[0]
        secs[k + 1] = dict(
            raw_size=struct.unpack_from("<I", data, o + 16)[0],
            praw=struct.unpack_from("<I", data, o + 20)[0],
            chars=struct.unpack_from("<I", data, o + 36)[0],
            relocs=[struct.unpack_from("<II", data, preloc + j * 10)
                    for j in range(nreloc)])
    return data, secs, raw


# ---------------------------------------------------------------------------
# retail image
# ---------------------------------------------------------------------------
class Image:
    def __init__(self, path):
        d = open(path, "rb").read()
        pe = struct.unpack_from("<I", d, 0x3C)[0]
        nsec = struct.unpack_from("<H", d, pe + 6)[0]
        oh = struct.unpack_from("<H", d, pe + 20)[0]
        ib = struct.unpack_from("<I", d, pe + 24 + 28)[0]
        self.d, self.secs = d, []
        for i in range(nsec):
            o = pe + 24 + oh + i * 40
            vs, va, rs, ro = struct.unpack_from("<IIII", d, o + 8)
            self.secs.append((ib + va, vs, ro))

    def word(self, va):
        for sva, vs, ro in self.secs:
            if sva <= va < sva + vs:
                return struct.unpack_from(">I", self.d, ro + (va - sva))[0]
        return None


def decode_thunk(img, va, size):
    """(vtordisp, adj, callee_va) if `va` holds an adjustor thunk, else None."""
    w = [img.word(va + o) for o in range(0, min(size or 16, 20), 4)]
    if len(w) < 3 or w[0] is None or w[0] >> 26 != 32:            # lwz
        return None
    if ((w[0] >> 21) & 31, (w[0] >> 16) & 31) != (11, 3):
        return None
    d = w[0] & 0xFFFF
    if d >= 0x8000:
        d -= 0x10000
    if w[1] is None or w[1] >> 26 != 31 or (w[1] >> 1) & 0x3FF != 40:   # subf
        return None
    if ((w[1] >> 21) & 31, (w[1] >> 16) & 31, (w[1] >> 11) & 31) != (3, 11, 3):
        return None
    i, adj = 2, 0
    if w[i] is not None and w[i] >> 26 == 14:                     # addi r3,r3,-adj
        if ((w[i] >> 21) & 31, (w[i] >> 16) & 31) != (3, 3):
            return None
        imm = w[i] & 0xFFFF
        adj = -(imm - 0x10000 if imm >= 0x8000 else imm)
        i += 1
    if i >= len(w) or w[i] is None or w[i] >> 26 != 18 or (w[i] & 1):
        return None
    li = w[i] & 0x03FFFFFC
    if li & 0x02000000:
        li -= 0x04000000
    return (d, adj, (va + i * 4) + li)


# ---------------------------------------------------------------------------
# name algebra
# ---------------------------------------------------------------------------
def name_nums(name):
    """`?M@C@@$4PPPPPPPM@BGA@AA...` -> (selector, [vtordisp, adj])."""
    mo = THUNK_RX.search(name)
    if not mo:
        return None
    rest, out, i = name[mo.end():], [], 0
    for _ in range(2):
        j = rest.find("@", i)
        if j < 0:
            return None
        tok = rest[i:j]
        if not re.fullmatch(r"[A-P]+", tok):
            return None
        v = 0
        for ch in tok:
            v = v * 16 + (ord(ch) - 65)
        if len(tok) >= 8 and v >= 0x80000000:
            v -= 0x100000000
        out.append(v)
        i = j + 1
    return (mo.group(1), out)


def scope_of(name):
    if not name or not name.startswith("?"):
        return None
    i = name.find("@@")
    return name[:i + 2] if i > 0 else None


def eg_canon(n):
    """`??_E<X>@@UAAPAXI@Z` and `??_G<X>@@UAAPAXI@Z` name the SAME function.

    Our build emits the deleting-dtor BODY as the `??_E` spelling in every
    thunk's relocation but defines only `??_G`; retail has exactly one such body
    per class and it is reloc-masked byte-identical and byte-UNIQUE against our
    `??_G`.  No obj in the tree defines both spellings.  So the difference is a
    naming divergence in our build, invisible under objdiff's normalized diff --
    canonicalise before comparing or 57 non-defects read as defects.
    """
    if n and n.startswith("??_E") and n.endswith("@@UAAPAXI@Z"):
        return "??_G" + n[4:]
    return n


# ---------------------------------------------------------------------------
# inputs
# ---------------------------------------------------------------------------
def load_sizes():
    sizes = {}
    rx = re.compile(r"^(\S+) = \.(\w+):0x([0-9A-Fa-f]+);.*?type:(\w+)"
                    r"(?:.*?size:0x([0-9A-Fa-f]+))?")
    for line in open(SYMBOLS):
        mo = rx.match(line)
        if mo and mo.group(4) == "function":
            sizes[int(mo.group(3), 16)] = int(mo.group(5), 16) if mo.group(5) else 0
    return sizes


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
        return ranges[i][2] if i >= 0 and ranges[i][0] <= va < ranges[i][1] else None
    return unit_of


def unit_paths(unit):
    rel = unit[:-4] if unit.endswith(".cpp") else unit
    asm = BUILD / "asm" / (rel + ".s")
    bobj = SRC / (rel + ".obj")
    if not bobj.exists():
        c = list(SRC.rglob(os.path.basename(rel) + ".obj"))
        bobj = c[0] if len(c) == 1 else bobj
    return asm, bobj


def compiled_thunk_table():
    """{obj_stem: {thunk_name: (callee_name, (vt, adj))}} plus the global union."""
    per, uni = {}, {}
    for p in glob.glob(str(SRC / "**" / "*.obj"), recursive=True):
        try:
            data, secs, raw = parse_coff(p)
        except Exception:
            continue
        bysec = collections.defaultdict(list)
        for sy in raw:
            if sy and sy["secn"] > 0 and sy["secn"] in secs and sy["typ"] == CODE \
               and sy["sc"] in (2, 3) and secs[sy["secn"]]["chars"] & CODE:
                bysec[sy["secn"]].append(sy)
        t = {}
        for secn, mem in bysec.items():
            s = secs[secn]
            mem = sorted(mem, key=lambda x: x["val"])
            for k, sy in enumerate(mem):
                nn = name_nums(sy["name"])
                if not nn:
                    continue
                start = sy["val"]
                end = mem[k + 1]["val"] if k + 1 < len(mem) else s["raw_size"]
                cal = [raw[si]["name"] for rv, si in s["relocs"]
                       if start <= rv < end and raw[si]]
                cal = [c for c in cal if c.startswith("?")]
                if len(cal) == 1:
                    t[sy["name"]] = (cal[0], tuple(nn[1]))
                    uni.setdefault(sy["name"], (cal[0], tuple(nn[1])))
        if t:
            per[os.path.relpath(p, SRC)[:-4]] = t
    return per, uni


def byte_unique_tier(m, unit_of):
    """VAs whose reloc-masked bytes match EXACTLY ONE symbol in their own unit's
    obj and the map already assigns that symbol.  The ~99.5% evidence tier."""
    todo = collections.defaultdict(list)
    for va in m:
        u = unit_of(va)
        if u:
            todo[u].append(va)
    out = set()
    for u in sorted(todo):
        asm, bobj = unit_paths(u)
        if not (asm.exists() and bobj.exists()):
            continue
        try:
            tf = {va: mk for va, sz, mk in _asm_target_funcs(asm) if va}
            bf = _ordered_funcs(bobj)
        except Exception:
            continue
        bc = collections.defaultdict(list)
        for f in bf:
            if f["name"].startswith(("__unwind$", "$", "??_9")):
                continue
            bc[f["masked"]].append(f["name"])
        for va in todo[u]:
            mk = tf.get(va)
            if mk is not None:
                c = bc.get(mk, [])
                if len(c) == 1 and c[0] == m[va]:
                    out.add(va)
    return out


# ---------------------------------------------------------------------------
def load_map():
    raw = json.load(open(MAP_PATH))
    m = {int(k, 16): v for k, v in raw.items()
         if k.lower().startswith("0x") and isinstance(v, str)}
    arb = {int(x, 16) for x in raw.get("_bijection_arbitrary", [])}
    arb |= {int(x, 16) for x in raw.get("_icf_arbitrary", [])}
    byname = collections.defaultdict(list)
    for va, n in m.items():
        byname[n].append(va)
    dupvas = {va for n, v in byname.items() if len(v) > 1 for va in v}
    return raw, m, arb, dupvas, byname


def unit_tab(per, u):
    if u is None:
        return {}
    stem = u[:-4] if u.endswith(".cpp") else u
    if stem in per:
        return per[stem]
    b = os.path.basename(stem)
    c = [k for k in per if os.path.basename(k) == b]
    return per[c[0]] if len(c) == 1 else {}


def census(rows):
    def agree(r):
        if r["expect_callee"] is not None:
            return eg_canon(r["expect_callee"]) == eg_canon(r["callee_name"])
        return scope_of(r["name"]) == scope_of(r["callee_name"])

    def rate(sel, label):
        s = [r for r in rows if sel(r)]
        a = sum(1 for r in s if agree(r))
        print("%-48s n=%5d agree=%5d %5.1f%%"
              % (label, len(s), a, 100.0 * a / max(1, len(s))))
        return s
    rate(lambda r: r["callee_name"] is not None,
         "G1 adjustor-shaped, both ends named")
    rate(lambda r: r["callee_name"] and r["is_thunk_name"],
         "G2  + thunk carries $-thunk mangling")
    rate(lambda r: r["callee_name"] and r["is_thunk_name"]
         and r["proven"] and r["callee_proven"],
         "G3  + both endpoints proven-tier")
    g4 = rate(lambda r: r["callee_name"] and r["expect_callee"]
              and r["proven"] and r["callee_proven"],
              "G4  + compiled-obj callee oracle present")
    return [r for r in g4 if not agree(r)]


def apply_plan(plan):
    rx = re.compile(r'^(\s*")(0[xX][0-9a-fA-F]+)("\s*:\s*")(.*?)("\s*,?)$')
    lines = open(MAP_PATH).read().split("\n")
    hit = miss = 0
    for i, ln in enumerate(lines):
        mo = rx.match(ln)
        if not mo:
            continue
        p = plan.get(int(mo.group(2), 16))
        if not p:
            continue
        if mo.group(4) != p["old"]:
            print("SKIP %s: value drifted" % mo.group(2))
            miss += 1
            continue
        lines[i] = "".join(mo.group(1, 2, 3)) + p["new"] + mo.group(5)
        hit += 1
    open(MAP_PATH, "w").write("\n".join(lines))
    raw = json.load(open(MAP_PATH))
    seen = collections.defaultdict(list)
    for k, v in raw.items():
        if k.lower().startswith("0x") and isinstance(v, str):
            seen[v].append(k)
    allow = set(raw.get("_internal_linkage_allow", []))
    bad = {n: v for n, v in seen.items() if len(v) > 1 and n not in allow}
    print("applied %d, skipped %d; non-allowed duplicate names after: %d"
          % (hit, miss, len(bad)))
    if bad:
        raise SystemExit("INJECTIVITY BROKEN: %s" % list(bad)[:3])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--plan", choices=("group", "unit"))
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--emit", default="/home/free/tmp/thunkedge/plan.json")
    args = ap.parse_args()

    raw, m, arb, dupvas, byname = load_map()
    img, sizes, unit_of = Image(IMAGE), load_sizes(), load_units()
    per, uni = compiled_thunk_table()
    trusted = byte_unique_tier(m, unit_of)

    rows = []
    for va, name in m.items():
        dec = decode_thunk(img, va, sizes.get(va, 0))
        if dec is None:
            continue
        vt, adj, cva = dec
        nn = name_nums(name)
        exp = uni.get(name)
        rows.append(dict(va=va, name=name, unit=unit_of(va), vt=vt, adj=adj,
                         callee=cva, callee_name=m.get(cva),
                         is_thunk_name=bool(nn),
                         nums_ok=(list(nn[1]) == [vt, adj]) if nn else None,
                         expect_callee=exp[0] if exp else None,
                         proven=not (va in arb or va in dupvas),
                         callee_proven=not (cva in arb or cva in dupvas)))
    print("adjustor-shaped VAs in map: %d  ($-mangled name: %d)"
          % (len(rows), sum(1 for r in rows if r["is_thunk_name"])))
    print("byte-unique callee trust tier: %d VAs\n" % len(trusted))
    dis = census(rows)
    print("\nresidue: %d  (nums contradict bytes: %d)"
          % (len(dis), sum(1 for r in dis if not r["nums_ok"])))
    if not args.plan:
        return

    # ------------------------------------------------------------------
    key = (lambda r: (r["unit"], r["vt"], r["adj"])) if args.plan == "group" \
        else (lambda r: r["unit"])
    buckets = collections.defaultdict(list)
    for r in rows:
        if r["is_thunk_name"] and r["unit"]:
            buckets[key(r)].append(r)

    plan, blocked = {}, []
    for k, g in sorted(buckets.items(), key=str):
        u = g[0]["unit"]
        tab = unit_tab(per, u)
        if not tab:
            continue
        cur = {r["va"]: r["name"] for r in g}
        pool = set(cur.values())
        want = {}
        for r in g:
            cn = r["callee_name"]
            cva = r["callee"]
            if cn is None or cva not in trusted or cva in arb or cva in dupvas:
                continue
            src = pool if args.plan == "group" else set(tab)
            cands = [n for n in src
                     if eg_canon((tab if args.plan == "unit" else uni)
                                 .get(n, (None,))[0]) == eg_canon(cn)
                     and list((tab if args.plan == "unit" else uni)
                              .get(n, (None, ()))[1]) == [r["vt"], r["adj"]]]
            if len(cands) == 1:
                want[r["va"]] = cands[0]
            elif len(cands) > 1:
                blocked.append(dict(va="0x%08x" % r["va"], unit=u, cur=r["name"],
                                    cands=cands[:4], reason="ambiguous oracle"))
        byw = collections.defaultdict(list)
        for va, n in want.items():
            byw[n].append(va)
        pins = {}
        for n, vas in byw.items():
            if len(vas) == 1:
                pins[vas[0]] = n
            else:
                for va in vas:
                    blocked.append(dict(va="0x%08x" % va, unit=u, cur=cur[va],
                                        cands=[n],
                                        reason="two VAs in scope claim one name"))
        if args.plan == "group":
            assign = dict(pins)
            used = set(pins.values())
            for va in [v for v in cur if v not in assign]:
                if cur[va] not in used:
                    assign[va] = cur[va]
                    used.add(cur[va])
            spare = sorted(pool - used)
            for va, n in zip(sorted(v for v in cur if v not in assign), spare):
                assign[va] = n
            assert len(set(assign.values())) == len(assign)
            for va, n in assign.items():
                if n != cur[va]:
                    plan[va] = dict(unit=u, old=cur[va], new=n, pinned=va in pins)
        else:
            changed = {va: n for va, n in pins.items() if n != cur[va]}
            for va, n in changed.items():
                holders = [a for a in byname.get(n, []) if a != va]
                if holders and not all(h in changed for h in holders):
                    blocked.append(dict(
                        va="0x%08x" % va, unit=u, cur=cur[va], cands=[n],
                        holder=["0x%08x" % h for h in holders],
                        reason="required name held by a VA this pass cannot move"))
                    continue
                plan[va] = dict(unit=u, old=cur[va], new=n, pinned=True)

    after = dict(m)
    after.update({va: p["new"] for va, p in plan.items()})
    cnt = collections.Counter(after.values())
    allow = set(raw.get("_internal_linkage_allow", []))
    bad = {n for n, c in cnt.items() if c > 1 and n not in allow}
    for va in [v for v in plan if plan[v]["new"] in bad]:
        blocked.append(dict(va="0x%08x" % va, unit=plan[va]["unit"],
                            cur=plan[va]["old"], cands=[plan[va]["new"]],
                            reason="move would duplicate the name"))
        plan.pop(va)
    print("\nplan(%s): %d changes (%d oracle-pinned), blocked %d"
          % (args.plan, len(plan),
             sum(1 for p in plan.values() if p["pinned"]), len(blocked)))
    Path(args.emit).parent.mkdir(parents=True, exist_ok=True)
    Path(args.emit).write_text(json.dumps(
        {("0x%08x" % k): v for k, v in plan.items()}, indent=1))
    Path(args.emit.replace(".json", ".blocked.json")).write_text(
        json.dumps(blocked, indent=1))
    if args.apply:
        apply_plan(plan)


if __name__ == "__main__":
    main()
