#!/usr/bin/env python3
"""Triage the `different_function` charges that name_check raises, and split the
ones that are SOURCE defects from the ones that are SYMBOL-NAMING artifacts.

Background
----------
`functionRelocDiffs=name_check` charges a call site whose relocation names a
different callee than retail's.  On rb3-xenon that is ~10.6k sites in one lane,
`different_function`.  A previous triage split those by residency in
`scripts/target_symbol_map.json`:

    survivor mapped, ours unmapped -> ICF-alias candidate
    both mapped at DIFFERENT VAs   -> "wrong callee", i.e. a source defect

The second half of that rule does not hold.  Retail's map is a VA->name
function: it can express exactly ONE name per address, and the retail link
ICF-folded thousands of identical COMDATs.  "Both names are map-resident at
different VAs" is therefore satisfied whenever the map happened to hand our
callee's spelling to some *other* address -- including addresses that are
padding, that belong to a different function entirely, or that were assigned by
a bijection over a byte-identical class (the map's own `_bijection_arbitrary`
list, 1,109 VAs, says as much in its comment).

This script re-derives the split from the retail image itself, not from map
residency.  For each charged pair it reads both retail bodies out of
`orig/45410914/band.exe`, measures the fan-in of the target VA over the whole
`.text`, and assigns a sub-class and a verdict.

Sub-classes (most-to-least "this is a naming artifact")
-------------------------------------------------------
fold_thunk_naming      retail's callee is a <=4-byte tail-jump thunk with a large
                       fan-in.  Every `{ MemFree(v); }` / `{ return MemAlloc(s,0); }`
                       body in the game folds onto one address and the map can
                       name it only once.  `??3BinStream@@SAXPAX@Z` @0x8240DDB0 is
                       `b MemFree` with 2,308 callers; `??2CriticalSection@@SAPAXI@Z`
                       @0x827BD2F0 is `li r4,0; b MemAlloc` with 1,048.  No source
                       spelling can make our callee's NAME equal to those, so this
                       is an alias-map lane, not a source lane.
bijection_class        the two retail bodies are byte-identical once relocation
                       -carrying fields are masked.  Which name sits on which VA is
                       not established; the map flags many of these itself.
map_name_unresolved    the map explicitly lists the VA in `_bijection_arbitrary`
                       or `_icf_arbitrary`.
map_misassignment      our own definition of the callee scores <20% at `none`
                       against the VA the map gives that name.  The map entry for
                       OUR symbol is wrong, so the charge says nothing about the
                       call site.
wrapper_not_inlined /  one side's callee is a 1-instruction thunk to the other's.
wrapper_inlined_by_us  An inlining difference on a trivial wrapper, not a wrong
                       callee.
transposition          a 2-cycle: (a,b) and (b,a) are both charged.  The metric
                       cannot adjudicate these -- swapping always clears both --
                       so each needs semantic evidence.
residual               everything else: candidate genuine wrong callee.

Usage
-----
    python3 scripts/wrong_callee_triage.py --sites <triage>/sites.jsonl \\
        --none-report <triage>/report_none.json -o docs/plans/<name>.json
"""

import argparse
import collections
import json
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD_ID = "45410914"


# `addr_taken` pairing window, in instructions.  Measured on band.exe @45410914:
# window 2 finds 2,402 distinct `.text` VAs, 8 finds 2,727 and 32 finds 2,739 --
# so 8 is where the curve flattens (99.6% of what 32 finds).  Widening it only
# over-counts, which is the safe direction.
ADDR_TAKEN_WINDOW = 8

# Sections scanned for pointer words.  BINK/BINKDATA/.reloc were measured and are
# pure noise -- 0, 0 and 1 of their 87 combined `.text`-ranged words land on a
# symbols.txt function start, against 36,467 of `.rdata`'s and 745 of `.data`'s.
POINTER_SECTIONS = (".rdata", ".data")

# Opcodes we are SURE write rT (bits 6-10), used only to retire a pending `lis`.
# Conservative by construction: a missed clobber leaves the `lis` live and
# over-counts references (safe); a wrong one drops a real reference (unsafe), so
# nothing goes in here that is not a plain D-form rT destination.
D_FORM_RT_DEST = frozenset({
    7, 8, 12, 13, 14, 15,                      # mulli subfic addic addic. addi addis
    24, 25, 26, 27, 28, 29,                    # ori oris xori xoris andi. andis.
    32, 33, 34, 35, 40, 41, 42, 43, 46, 58,    # lwz lwzu lbz lbzu lhz lhzu lha lhau lmw ld
})


# --------------------------------------------------------------------------- PE
class Image:
    def __init__(self, path):
        self.data = data = path.read_bytes()
        lfanew = struct.unpack_from("<I", data, 0x3C)[0]
        assert data[lfanew:lfanew + 4] == b"PE\0\0"
        coff = lfanew + 4
        nsec = struct.unpack_from("<H", data, coff + 2)[0]
        optsz = struct.unpack_from("<H", data, coff + 16)[0]
        opt = coff + 20
        base = struct.unpack_from("<I", data, opt + 28)[0]
        p = opt + optsz
        self.secs = []
        self.text = None
        for _ in range(nsec):
            name = data[p:p + 8].rstrip(b"\0").decode()
            vsz, va, rawsz, raw = struct.unpack_from("<IIII", data, p + 8)
            self.secs.append((name, base + va, raw, rawsz))
            if name == ".text":
                self.text = (base + va, raw, rawsz)
            p += 40

    def off(self, va):
        for _, sva, raw, rawsz in self.secs:
            if sva <= va < sva + rawsz:
                return raw + (va - sva)
        return None

    def word(self, va, i=0):
        o = self.off(va)
        return None if o is None else struct.unpack_from(">I", self.data, o + 4 * i)[0]

    def fanin(self):
        """BRANCHES only: how many `b`/`bl` in `.text` target each address.

        Deliberately unchanged in meaning -- `fold_thunk_naming` thresholds and
        the caller-demand comparisons in tools/ourside_fold_sweep.py are all
        statements about CALL SITES.  For "is this address referenced at all",
        which is what a zero-fan-in discredit needs, use `refs()`: a function
        whose address is TAKEN is never branched to and is invisible here.
        """
        base, raw, rawsz = self.text
        c = collections.Counter()
        for i in range(rawsz // 4):
            w = struct.unpack_from(">I", self.data, raw + 4 * i)[0]
            if (w >> 26) == 18 and not ((w >> 1) & 1):     # relative b / bl
                li = w & 0x03FFFFFC
                if li & 0x02000000:
                    li -= 0x04000000
                c[base + 4 * i + li] += 1
        return c

    def addr_taken(self, window=ADDR_TAKEN_WINDOW):
        """`.text` addresses MATERIALISED by an instruction-immediate pair.

        A static factory, a callback, a registration-table entry: its address is
        taken, never branched to.  On PPC the value is built from two 16-bit
        immediates -- `lis rX, hi` then `addi rD, rX, lo` (signed, `@ha`/`@l`) or
        `ori rD, rX, lo` (unsigned) -- so it appears NOWHERE as a 32-bit word and
        is invisible both to `fanin()` and to any literal-word scan.  A linked
        X360 image carries no relocation records, so the only way to see the
        reference is to reconstruct the immediates.

        Over-counting is the SAFE direction here (a spurious reference makes a
        zero-reference discredit refuse, i.e. fail closed), so the register model
        is deliberately lax: a `lis` stays live until something we are SURE
        writes that register does, and pairing is capped at `window`
        instructions rather than at a basic-block boundary.
        """
        base, raw, rawsz = self.text
        n = rawsz // 4
        words = struct.unpack_from(">%dI" % n, self.data, raw)
        c = collections.Counter()
        pend = {}                              # reg -> (hi << 16, index of the lis)
        for i, w in enumerate(words):
            op = w >> 26
            rt, ra = (w >> 21) & 31, (w >> 16) & 31
            if op == 15 and ra == 0:           # lis rT, hi  ==  addis rT, r0, hi
                pend[rt] = ((w & 0xFFFF) << 16, i)
                continue
            if op in (14, 24) and ra in pend:  # addi / ori rD, rA, lo
                hi, j = pend[ra]
                if i - j <= window:
                    imm = w & 0xFFFF
                    if op == 14:
                        va = (hi + (imm - 0x10000 if imm & 0x8000 else imm)) & 0xFFFFFFFF
                    else:
                        va = hi | imm
                    if base <= va < base + rawsz and not (va & 3):
                        c[va] += 1
            if op in D_FORM_RT_DEST:
                pend.pop(rt, None)
        return c

    def data_pointers(self, sections=POINTER_SECTIONS):
        """`.text` addresses appearing as a POINTER WORD in a data section.

        Vtable slots, jump/registration tables, static-initialiser lists.
        `.pdata` is excluded on purpose: the unwind table names EVERY function
        by construction, so counting it would discredit nothing.  `.text` is
        excluded because its words are instructions, not pointers -- see the
        residual blind spot in `refs()`.
        """
        base, _, rawsz = self.text
        c = collections.Counter()
        for name, _sva, raw, srawsz in self.secs:
            if name not in sections:
                continue
            for i in range(srawsz // 4):
                w = struct.unpack_from(">I", self.data, raw + 4 * i)[0]
                if base <= w < base + rawsz and not (w & 3):
                    c[w] += 1
        return c

    def refs_detail(self, window=ADDR_TAKEN_WINDOW, sections=POINTER_SECTIONS):
        """{'branch': …, 'addr_taken': …, 'data_ptr': …, 'total': …} of Counters."""
        b, a, d = self.fanin(), self.addr_taken(window), self.data_pointers(sections)
        t = collections.Counter()
        for x in (b, a, d):
            t.update(x)
        return {"branch": b, "addr_taken": a, "data_ptr": d, "total": t}

    def refs(self, window=ADDR_TAKEN_WINDOW, sections=POINTER_SECTIONS):
        """ALL inbound references: branches + address-taken pairs + pointer words.

        This is the counter a "nothing in the image references this address"
        discredit must use.  `fanin() == 0` is NOT that claim -- measured on
        band.exe @45410914, 38,713 of the 48,724 zero-fan-in `.text` functions
        are referenced by one of the other two channels.

        RESIDUAL BLIND SPOTS -- this shrinks the hole, it does not close it:
          * a COMPUTED address (base + runtime index, e.g. a table walked with
            `lwzx`) is not an immediate pair and is not seen;
          * an address materialised across a basic-block boundary further apart
            than `window`, or through a register this scan believes clobbered;
          * `li`/`oris` and other two-immediate orders are not modelled;
          * a pointer word living inside `.text` (an in-code jump table);
          * a reference from a module outside this image.
        So a zero from `refs()` is "no reference in the four forms scanned",
        which is stronger evidence than a zero from `fanin()` and still not
        proof that the address is unreferenced.
        """
        return self.refs_detail(window, sections)["total"]


# 16-bit-immediate opcodes whose field carries a relocation
IMM16_OPS = {14, 15, 24, 25, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54}


def masked_body(img, va, size, limit=0x8000):
    """Body with branch displacements and 16-bit immediates masked out."""
    n = size.get(va)
    o = img.off(va)
    if not n or o is None or n > limit:
        return None
    out = []
    for i in range(n // 4):
        w = struct.unpack_from(">I", img.data, o + 4 * i)[0]
        op = w >> 26
        if op == 18:
            w &= 0xFC000003
        elif op == 16:
            w &= 0xFFFF0003
        elif op in IMM16_OPS:
            w &= 0xFFFF0000
        out.append(w)
    return tuple(out)


def thunk_target(img, va, size):
    """If the body at `va` is exactly one unconditional `b X`, return X."""
    if size.get(va) != 4:
        return None
    w = img.word(va)
    if w is None or (w >> 26) != 18 or (w & 1) or ((w >> 1) & 1):
        return None
    li = w & 0x03FFFFFC
    if li & 0x02000000:
        li -= 0x04000000
    return va + li


def load_sizes():
    rx = re.compile(r"^\S+\s*=\s*\.text:0x([0-9A-Fa-f]+);.*?size:0x([0-9A-Fa-f]+)")
    size = {}
    with open(ROOT / "config" / BUILD_ID / "symbols.txt") as fh:
        for line in fh:
            m = rx.match(line)
            if m:
                size[int(m.group(1), 16)] = int(m.group(2), 16)
    return size


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sites", required=True, help="sites.jsonl from namecheck_triage.py")
    ap.add_argument("--none-report", required=True, help="objdiff report at functionRelocDiffs=none")
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--fold-fanin", type=int, default=50)
    args = ap.parse_args()

    img = Image(ROOT / "orig" / BUILD_ID / "band.exe")
    size = load_sizes()
    fanin = img.fanin()

    smap = json.loads((ROOT / "scripts" / "target_symbol_map.json").read_text())
    rev = {}
    for a, n in smap.items():
        if a.startswith("0x") and isinstance(n, str):
            rev.setdefault(n, []).append(int(a, 16))
    bij = {int(x, 16) for x in smap.get("_bijection_arbitrary", [])}
    icf = {int(x, 16) for x in smap.get("_icf_arbitrary", [])}

    nonepct = {}
    for u in json.loads(Path(args.none_report).read_text())["units"]:
        for f in u.get("functions", []):
            nonepct[f["name"]] = max(nonepct.get(f["name"], 0.0),
                                     f.get("fuzzy_match_percent", 0.0))

    sites = [json.loads(l) for l in open(args.sites) if l.strip()]
    charged = []
    for s in sites:
        if s["lane"] != "different_function":
            continue
        ta, ba = rev.get(s["target"]), rev.get(s["base"])
        if ta and ba and set(ta) != set(ba):
            charged.append(s)

    pc = collections.Counter((s["target"], s["base"]) for s in charged)
    units = collections.defaultdict(set)
    callers = collections.defaultdict(set)
    for s in charged:
        units[(s["target"], s["base"])].add(s["unit"])
        callers[(s["target"], s["base"])].add(s["func"])
    S = set(pc)
    cyc = {(t, b) for (t, b) in S if (b, t) in S}

    rows = []
    for (t, b), n in pc.most_common():
        ta, ba = rev[t][0], rev[b][0]
        tt, bt = thunk_target(img, ta, size), thunk_target(img, ba, size)
        mt, mb = masked_body(img, ta, size), masked_body(img, ba, size)
        arb_t = "bijection" if ta in bij else "icf" if ta in icf else None
        arb_b = "bijection" if ba in bij else "icf" if ba in icf else None
        r = dict(
            target=t, base=b, sites=n,
            units=len(units[(t, b)]), calling_functions=len(callers[(t, b)]),
            target_addr="0x%08x" % ta, base_addr="0x%08x" % ba,
            target_size=size.get(ta), base_size=size.get(ba),
            target_fanin=fanin[ta], base_fanin=fanin[ba],
            target_arbitrary=arb_t, base_arbitrary=arb_b,
            target_none_pct=round(nonepct.get(t, -1.0), 2),
            base_none_pct=round(nonepct.get(b, -1.0), 2),
            two_cycle=(t, b) in cyc,
            reloc_masked_identical=(mt is not None and mt == mb),
        )
        if bt == ta:
            cls = "wrapper_not_inlined"
            v = ("our callee is a 1-instruction thunk that tail-jumps to retail's callee; "
                 "retail inlined the wrapper away at this site. Inlining difference, not a "
                 "wrong callee.")
        elif tt == ba:
            cls = "wrapper_inlined_by_us"
            v = ("retail's callee is a 1-instruction thunk to OUR callee; retail kept the "
                 "wrapper and we inlined it. Inlining difference, not a wrong callee.")
        elif (r["target_size"] or 0) <= 4 and r["target_fanin"] >= args.fold_fanin:
            cls = "fold_thunk_naming"
            v = ("retail's callee is a %d-byte thunk with %d call sites -- an ICF fold "
                 "survivor whose name is one arbitrary pick among identical bodies. No "
                 "source spelling can reproduce that name; alias lane."
                 % (r["target_size"] or 0, r["target_fanin"]))
        elif arb_t or arb_b:
            cls = "map_name_unresolved"
            v = ("the map itself flags this VA's name as arbitrary (%s), so the charge is a "
                 "naming artifact until an oracle resolves the assignment."
                 % (arb_t or arb_b))
        elif r["reloc_masked_identical"]:
            cls = "bijection_class"
            v = ("both retail bodies are byte-identical once relocation-carrying fields are "
                 "masked -- an unflagged bijection class. Which name belongs on which VA is "
                 "not established.")
        elif 0 <= r["base_none_pct"] < 20:
            cls = "map_misassignment"
            v = ("our own definition of this callee scores %.1f%% at `none` against the VA "
                 "the map gives its name, so the map entry for OUR symbol is wrong and the "
                 "charge is not evidence about the call site." % r["base_none_pct"])
        elif r["two_cycle"]:
            cls = "transposition"
            v = ("2-cycle: (a,b) and (b,a) are both charged. Either our two call sites are "
                 "swapped or the map's two names are. Swapping always clears both, so the "
                 "metric cannot adjudicate -- semantic evidence required.")
        else:
            cls = "residual"
            v = "unclassified: candidate genuine wrong callee."
        r["subclass"] = cls
        r["verdict"] = v
        rows.append(r)

    tally = collections.Counter(r["subclass"] for r in rows)
    st = collections.Counter()
    for r in rows:
        st[r["subclass"]] += r["sites"]
    out = {
        "generated_by": "scripts/wrong_callee_triage.py",
        "build": BUILD_ID,
        "totals": {"pairs": len(rows), "sites": sum(st.values())},
        "by_subclass": {k: {"pairs": tally[k], "sites": st[k]} for k in tally},
        "pairs": rows,
    }
    Path(args.out).write_text(json.dumps(out, indent=1) + "\n")
    print("%-24s %6s %6s" % ("subclass", "pairs", "sites"))
    for k, _ in tally.most_common():
        print("%-24s %6d %6d" % (k, tally[k], st[k]))
    print("%-24s %6d %6d" % ("TOTAL", len(rows), sum(st.values())))
    print("-> %s" % args.out)


if __name__ == "__main__":
    main()
