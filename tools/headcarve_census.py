#!/usr/bin/env python3
"""Census the HEAD-side over-carve class: dtk function symbols that are not
real function starts because execution FALLS THROUGH into them.

This is the mirror of lane SPLITBLOCK-1's census, which keyed on over-carved
TAILS reached by an internal BRANCH (jeff's Class-4
`merge_branch_reached_overcarve_tails`).  A fall-through-reached head is a
DIFFERENT sub-class: no branch exists, so Class-4 cannot fire on it, and the
NAMED row is the spurious piece rather than the run's head.

Worked instance (lane SIZE4-1's residual): retail 0x8268FC18..0x8268FC5C is one
vbase-dtor closure ending in a tail call; dtk carved fn_8268FC18 (0x1C) +
fn_8268FC38 (0x24) and left 0x8268FC34 claimed by NEITHER.  The map then put
`??_DPropertyEventProvider@@QAAXXZ` on the TAIL, so the row's body opens
`add r10, r10, r11` on registers defined above the boundary.

TWO INDEPENDENT SIGNATURES, deliberately not combined into one score:

  A. CONTROL FLOW -- walk forward from the predecessor's last instruction; if no
     terminator (unconditional b / blr / bctr / rfi / sc / trap) is reached
     before the successor's address, execution falls through into it.  A real
     function is never entered by fall-through.

  B. DATA FLOW -- the successor reads a register before defining it, restricted
     to registers that are NOT live on entry under the MSVC X360 ABI (r11 and
     the callee-saved r14-r31).  r3-r10 are argument registers and r12 arrives
     holding the parent frame in an EH funclet, so both are excluded: including
     them would fire on every legitimate function.

Controls this script runs on itself (a detector that cannot fail is worthless):
  * NULL      -- the fall-through rate over ALL adjacent function pairs.
  * NEGATIVE  -- flagged rate among rows the grader scores at fuzzy == 100.
                 A head at 100% PROVES its carve is right (SPLITBLOCK-1).
  * POSITIVE  -- the known instance 0x8268FC38 must appear.
"""
import json
import re
import struct
import sys
from pathlib import Path

import capstone

ROOT = Path(__file__).resolve().parent.parent
SYM = ROOT / "config/45410914/symbols.txt"
PE = ROOT / "orig/45410914/band.exe"
MAP = ROOT / "scripts/target_symbol_map.json"
REPORT = ROOT / "build/45410914/report.json"

# ---------------------------------------------------------------- PE sections
def load_image():
    data = PE.read_bytes()
    e = struct.unpack_from("<I", data, 0x3C)[0]
    assert data[e:e + 4] == b"PE\0\0"
    coff = e + 4
    nsec = struct.unpack_from("<H", data, coff + 2)[0]
    optsz = struct.unpack_from("<H", data, coff + 16)[0]
    opt = coff + 20
    base = struct.unpack_from("<I", data, opt + 28)[0]
    st = opt + optsz
    secs = []
    for i in range(nsec):
        o = st + i * 40
        name = data[o:o + 8].rstrip(b"\0").decode(errors="replace")
        vsz, va, rsz, ptr = struct.unpack_from("<IIII", data, o + 8)
        secs.append((name, base + va, vsz, ptr, rsz))
    return data, secs


DATA, SECS = load_image()


def word(va):
    for _, b, vsz, ptr, rsz in SECS:
        if b <= va < b + vsz:
            off = ptr + (va - b)
            if off + 4 <= len(DATA):
                return struct.unpack_from(">I", DATA, off)[0]
    return None


# ---------------------------------------------------------------- symbol table
FN_RE = re.compile(
    r"^(\S+)\s*=\s*\.text:0x([0-9A-Fa-f]+);.*?\btype:function\b.*?\bsize:0x([0-9A-Fa-f]+)"
)
OBJ_RE = re.compile(
    r"^(\S+)\s*=\s*\.text:0x([0-9A-Fa-f]+);.*?\btype:object\b.*?\bsize:0x([0-9A-Fa-f]+)"
)

funcs = []
objs = []
for line in SYM.read_text().splitlines():
    m = FN_RE.search(line)
    if m:
        funcs.append((int(m.group(2), 16), int(m.group(3), 16), m.group(1)))
        continue
    m = OBJ_RE.search(line)
    if m:
        objs.append((int(m.group(2), 16), int(m.group(3), 16)))
funcs.sort()
objs.sort()
obj_starts = {a for a, _ in objs}
obj_spans = objs

# ---------------------------------------------------------------- disassembler
MD = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN)
MD.detail = True
_cache = {}


def decode(w, va):
    key = (w, va & 3)
    if key in _cache:
        return _cache[key]
    r = None
    for i in MD.disasm(struct.pack(">I", w), va):
        r = i
        break
    _cache[key] = r
    return r


def is_terminator(w):
    """True if this word unconditionally transfers control away (no fall-through)."""
    if w is None:
        return True  # unreadable -> be conservative, assume flow stops
    op = (w >> 26) & 0x3F
    if op == 18:                              # b / ba / bl / bla
        return (w & 1) == 0                   # LK==0 -> not a call -> terminator
    if op == 16:                              # bc  -- terminator only if BO says always
        bo = (w >> 21) & 0x1F
        return (w & 1) == 0 and (bo & 0b10100) == 0b10100
    if op == 19:                              # bclr / bcctr
        xo = (w >> 1) & 0x3FF
        if xo in (16, 528):
            bo = (w >> 21) & 0x1F
            return (w & 1) == 0 and (bo & 0b10100) == 0b10100
        if xo == 50:                          # rfi
            return True
        return False
    if op == 17:                              # sc
        return True
    if op == 3:                               # twi
        return True
    if op == 31 and ((w >> 1) & 0x3FF) == 4:  # tw
        return True
    return False


def is_padding(w):
    return w in (0, 0x60000000)  # zero fill or nop


# ------------------------------------------------------- read/write extraction
LOADS = re.compile(r"^l(bz|hz|ha|wz|wa|mw|fs|fd|swi|d|q|vx?|ve|vs)")
STORES = re.compile(r"^st")


def rw(ins):
    """(reads, writes) GPR-name sets. None if we cannot decode."""
    if ins is None:
        return None
    mn = ins.mnemonic
    reads, writes = set(), set()
    ops = ins.operands
    def gpr(n):
        return n if (n and re.fullmatch(r"r\d+", n)) else None

    for idx, o in enumerate(ops):
        if o.type == capstone.ppc.PPC_OP_REG:
            n = gpr(ins.reg_name(o.reg))
            if not n:
                continue
            first = idx == 0
            if STORES.match(mn) or mn.startswith(("cmp", "b", "tw", "mt")):
                reads.add(n)
            elif first:
                writes.add(n)
            else:
                reads.add(n)
        elif o.type == capstone.ppc.PPC_OP_MEM:
            n = gpr(ins.reg_name(o.mem.base))
            if n:
                reads.add(n)
                if mn.endswith("u") and (LOADS.match(mn) or STORES.match(mn)):
                    writes.add(n)
    return reads, writes


# r3-r10 are argument registers and r12 carries the parent frame into an EH
# funclet, so all are legitimately live on entry.  r14-r31 are callee-saved and
# EVERY prologue stores them before defining them (`std r30, -0x18(r1)`), which
# made an earlier version of this flag fire on ?SaveForEndGame@Stats@@ at
# fuzzy==100.  r11 is the one scratch register that is neither an argument nor
# saved, so reading it before defining it is the only sound tell -- and it is
# the register both worked instances (PropertyEventProvider, DataNode) trip.
NOT_LIVE_ON_ENTRY = {"r11"}


def use_before_def(start, end):
    """True if the body reads a register that cannot be live on entry.
    None if the body could not be fully decoded."""
    defined = set()
    va = start
    while va < end:
        w = word(va)
        if w is None:
            return None
        ins = decode(w, va)
        if ins is None:
            return None
        got = rw(ins)
        if got is None:
            return None
        reads, writes = got
        for r in reads:
            if r in NOT_LIVE_ON_ENTRY and r not in defined:
                return True
        defined |= writes
        if is_terminator(w):
            break
        va += 4
    return False


# --------------------------------------------------------------- the sweep
def looks_like_ptr(w):
    return w is not None and 0x82000000 <= w < 0x83000000


def is_eh_prefix(addr, size):
    """The documented 8-byte EH prefix: a .text pointer + an .rdata pointer
    sitting immediately before a real function.  dtk types these as
    `type:function`, and disassembling the two pointer words yields plausible
    `lwz`s that are not terminators -- which made an earlier version of this
    sweep flag ?Poll@BandCharacter@@ (fuzzy==100) as fall-through-reached."""
    return size == 8 and looks_like_ptr(word(addr)) and looks_like_ptr(word(addr + 4))


def eh_prefix_before(succ_addr):
    """CLAUDE.md's documented artifact: an 8-byte EH prefix (a .text pointer
    followed by an .rdata pointer) sits immediately before a real function, and
    97% of the time `addr+8` is a genuine function start.  dtk types those two
    words as code, and they disassemble to plausible non-terminator `lwz`s.
    Keying on the two words BEFORE the head (rather than on the predecessor's
    declared size) catches the case where dtk sized the prefix 4 instead of 8 --
    which is what flagged ?SyncProperty@CharBlendBone@@ at fuzzy==100."""
    return looks_like_ptr(word(succ_addr - 8)) and looks_like_ptr(word(succ_addr - 4))


def falls_through(prev_addr, prev_end, succ_addr):
    """Walk prev_end-4 .. succ_addr. Returns (reached, gap_kind)."""
    if is_eh_prefix(prev_addr, prev_end - prev_addr) or eh_prefix_before(succ_addr):
        return False, "eh_prefix"
    # last REAL instruction of the predecessor: dtk's declared size can include
    # trailing alignment padding, whose 0x00000000 word is not a terminator and
    # would fake a fall-through.
    va = prev_end - 4
    if is_padding(word(va)):
        # MSVC does not emit alignment padding mid-function, so a padded tail is
        # itself proof the predecessor really ended here.
        return False, "padding_tail"
    w = word(va)
    if w is None:
        return False, "unreadable"
    if is_terminator(w):
        return False, "terminated"
    if (w >> 26) & 0x3F == 18 and (w & 1) == 1:
        return False, "ends_in_bl"      # trailing call: ambiguous, excluded
    # how the predecessor ends: a conditional branch is a WEAK tell (a loop can
    # legitimately sit at the end of a carved fragment), whereas ending
    # mid-computation on a non-branch is a STRONG one -- no real function ends
    # on `lwz`.  Reported separately; never pooled.
    _op = (w >> 26) & 0x3F
    strength = "cond_branch" if _op in (16, 19) else "nonbranch"
    # walk any unclaimed words in the gap
    va = prev_end
    kinds = []
    while va < succ_addr:
        gw = word(va)
        if gw is None:
            return False, "unreadable"
        if is_padding(gw):
            return False, "padding"     # padding => predecessor really ended
        if va in obj_starts:
            return False, "except_data"
        kinds.append("code")
        if is_terminator(gw):
            return False, "gap_terminated"
        va += 4
    return True, (strength + ("|adjacent" if not kinds else f"|hole:{len(kinds)}w"))


rows = []
null_total = 0
null_ft = 0
for i in range(len(funcs) - 1):
    a, sz, name = funcs[i]
    na, nsz, nname = funcs[i + 1]
    end = a + sz
    if end > na:
        continue                       # overlapping table entry, skip
    null_total += 1
    ft, kind = falls_through(a, end, na)
    if not ft:
        continue
    null_ft += 1
    rows.append(
        dict(pred=name, pred_addr=a, pred_size=sz,
             head=nname, head_addr=na, head_size=nsz,
             gap=na - end, kind=kind, strength=kind.split("|")[0])
    )

# second, independent signature
for r in rows:
    r["ubd"] = use_before_def(r["head_addr"], r["head_addr"] + r["head_size"])

# ------------------------------------------------------------------ enrichment
_raw = json.load(MAP.open())
_ADDR_K = re.compile(r"^0x[0-9A-Fa-f]{8}$")
symmap = {int(k, 16): v for k, v in _raw.items() if v and _ADDR_K.match(k)}
_skipped = [k for k in _raw if not _ADDR_K.match(k)]
if _skipped:
    print(f"[map] skipped {len(_skipped)} non-address keys: {_skipped[:5]}")
rep = json.load(REPORT.open())
by_name = {}
for unit in rep.get("units", []):
    for fn in unit.get("functions", []):
        nm = fn.get("name")
        if nm:
            by_name[nm] = (
                float(fn.get("fuzzy_match_percent", 0) or 0),
                float(fn.get("match_percent_normalized", 0) or 0),
                int(fn.get("size", 0) or 0),
                unit.get("name", ""),
            )

for r in rows:
    nm = symmap.get(r["head_addr"])
    r["mapped"] = nm
    if nm and nm in by_name:
        f, m, s, u = by_name[nm]
        r.update(fuzzy=f, mpn=m, rsize=s, unit=u)

# ------------------------------------------------------------------- controls
# NEGATIVE control: flagged rate among heads the grader scores at fuzzy == 100
flagged_addrs = {r["head_addr"] for r in rows}
addr_of = {}
for a, v in symmap.items():
    addr_of.setdefault(v, a)
at100 = [addr_of[n] for n, (f, m, s, u) in by_name.items() if f >= 100.0 and n in addr_of]
at100_flagged = sum(1 for a in at100 if a in flagged_addrs)

print("=" * 74)
print("HEAD-SIDE OVER-CARVE CENSUS  (fall-through into a declared function start)")
print("=" * 74)
print(f"function symbols in .text          : {len(funcs)}")
print(f"adjacent pairs examined            : {null_total}")
print(f"NULL: fall-through-reached heads   : {null_ft}  ({100*null_ft/max(null_total,1):.3f}%)")
print()
print(f"NEGATIVE control -- fuzzy==100 rows: {len(at100)} mapped, flagged {at100_flagged}"
      f"  ({100*at100_flagged/max(len(at100),1):.3f}%)")
_bys = {}
for _r in rows:
    _bys.setdefault(_r["strength"], set()).add(_r["head_addr"])
for _k, _v in sorted(_bys.items()):
    _n100 = sum(1 for a in at100 if a in _v)
    print(f"   stratum {_k:12s}: total {len(_v):5d} ({100*len(_v)/max(null_total,1):.3f}% null)"
          f" | fuzzy==100 flagged {_n100:4d} ({100*_n100/max(len(at100),1):.3f}%)"
          f" | enrichment {(len(_v)/max(null_total,1))/max(_n100/max(len(at100),1),1e-12):.2f}x")
pos = [r for r in rows if r["head_addr"] == 0x8268FC38]
print(f"POSITIVE control -- 0x8268FC38     : {'FOUND' if pos else 'ABSENT (detector broken)'}")
print()

from collections import Counter
print("by gap kind:", dict(Counter(r["kind"] for r in rows)))
print("use-before-def agreement:", dict(Counter(str(r["ubd"]) for r in rows)))
named = [r for r in rows if r.get("mapped")]
print(f"\nflagged heads that are NAMED in the map: {len(named)}")
live = [r for r in named if r.get("fuzzy", 0) > 0 or r.get("mpn", 0) > 0]
print(f"  of which score > 0 on either ruler   : {len(live)}")
print(f"  total bytes of NAMED flagged heads   : {sum(r.get('rsize', 0) for r in named)}")
print(f"  total bytes of ALL flagged heads     : {sum(r['head_size'] for r in rows)}")
print(f"  total bytes of pred+head runs        : {sum(r['head_size']+r['pred_size'] for r in rows)}")

out = ROOT / "docs/decomp/headcarve-census-PINFIX2.json"
out.write_text(json.dumps(rows, indent=1))
print(f"\nwrote {out}  ({len(rows)} rows)")

if named:
    print("\nNAMED flagged heads (all):")
    for r in sorted(named, key=lambda r: -r.get("rsize", 0)):
        print(f"  0x{r['head_addr']:08x} {r['kind']:12s} ubd={str(r['ubd']):5s} "
              f"fuzzy={r.get('fuzzy',0):7.3f} mpn={r.get('mpn',0):7.3f} "
              f"sz={r.get('rsize',0):5d} {r['mapped'][:60]}  [{r.get('unit','')}]")
