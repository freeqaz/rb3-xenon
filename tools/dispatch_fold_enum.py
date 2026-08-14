#!/usr/bin/env python3
"""dispatch_fold_enum.py -- enumerate ICF fold classes from RETAIL'S OWN DISPATCH TABLE.

Lane ONMSG-1 (2026-08-14). Answers, for a Milo Handle()-style dispatcher, the
question "did these two handlers ICF-fold, or is the map name wrong?" -- which
objdiff CANNOT answer (it charges a folded callee exactly as it charges a wrong
one; see CLAUDE.md, MPNGAP-1).

METHOD. A Milo `Handle(DataArray*, bool)` builds a temp message per arm and calls
the handler. Each temp's vtable carries an MSVC Complete Object Locator at
vtable[-1]; COL+12 -> type descriptor, TD+8 -> the literal ".?AV<Class>@@" name.
So we can name every arm's message class FROM RETAIL BYTES, independently of
scripts/target_symbol_map.json, and pair it with the arm's `bl` target.

  ONE address reached from N arms that construct PROVABLY DIFFERENT message
  types == a fold, PROVEN BY INTERNAL INCONSISTENCY. No fold model, no body
  comparator, no byte hashing: one address cannot be N distinct handlers.

Two properties that make this better than a pairwise byte comparator:
  * it enumerates a fold class EXHAUSTIVELY (the dispatcher lists every member),
    which is how ONMSG-1 found that two ALREADY-DECLARED groups in
    scripts/symbol_aliases.json were SUBSETS of the real class; and
  * disagreement is directional -- an arm whose RTTI class contradicts the map
    name at its bl target is a WRONG MAP NAME to repair, not an alias.

CONTROLS (run these before believing a new verdict; an instrument that only ever
says "map wrong" is useless):
  * reproduces the existing gated alias groups at 0x825b5620 (3/3) and
    0x82538160 (5/5) EXACTLY;
  * agrees with target_symbol_map.json on 20 of 22 sibling-controller dispatch
    arms (ButtonGuitar/RealGuitar/JoypadGuitar/Keyboard Controller);
  * its two disagreements were both real: 0x8279ce28 was GuitarController's
    ButtonUp handler mapped as ButtonDown (repaired, +372 B), and
    ManageBandPanel::OnMsg(SigninChangedMsg) was declared folded at an address
    its own dispatcher refutes.

USAGE
  python3 tools/dispatch_fold_enum.py <vtableVA> [...]      # name message classes
  from dispatch_fold_enum import scan_handle, vtable_class  # enumerate a Handle

CAVEAT: absence from ONE dispatcher is not refutation of membership -- a class
member can be dispatched from another Handle. Only a CONTRADICTION (this arm's
message type != the map name at its target) is a refutation.
"""

import struct, sys, json

import os
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PE = os.path.join(ROOT, 'orig', '45410914', 'band.exe')
data = open(PE,'rb').read()

pe_off = struct.unpack_from('<I', data, 0x3c)[0]
assert data[pe_off:pe_off+4] == b'PE\0\0'
nsec  = struct.unpack_from('<H', data, pe_off+6)[0]
opt   = struct.unpack_from('<H', data, pe_off+20)[0]
imgbase = struct.unpack_from('<I', data, pe_off+24+28)[0]
secs=[]
so = pe_off+24+opt
for i in range(nsec):
    b = data[so+i*40: so+i*40+40]
    name=b[0:8].rstrip(b'\0').decode()
    vsz,va,rsz,ptr = struct.unpack_from('<IIII', b, 8)
    secs.append((name, imgbase+va, vsz, ptr, rsz))

def v2o(va):
    for name,sva,vsz,ptr,rsz in secs:
        if sva <= va < sva+max(vsz,rsz):
            return ptr + (va-sva)
    return None

def u32(va):   # big-endian: Xbox360 PPC
    o=v2o(va)
    return struct.unpack_from('>I', data, o)[0]

def cstr(va):
    o=v2o(va); e=data.index(b'\0',o)
    return data[o:e].decode('latin1')

def vtable_class(vt_va):
    """MSVC: COL pointer sits at vtable[-1]; COL+12 -> type descriptor; TD+8 -> name."""
    col = u32(vt_va-4)
    td  = u32(col+12)
    return cstr(td+8), col, td

if __name__ == '__main__':
    for a in sys.argv[1:]:
        va=int(a,16)
        try:
            nm,col,td = vtable_class(va)
            print(f'vtable 0x{va:08X}  COL 0x{col:08X}  TD 0x{td:08X}  ->  {nm}')
        except Exception as e:
            print(f'vtable 0x{va:08X}  ERROR {e}')

def scan_handle(va, maxbytes=0x600):
    """Walk a Handle body; emit (constructed_vtable_class, bl_target) in order."""
    regs={}; events=[]; pend=[]
    for off in range(0, maxbytes, 4):
        ins = u32(va+off)
        op  = ins>>26
        if op==15:                       # addis (lis when rA==0)
            rD=(ins>>21)&31; rA=(ins>>16)&31
            imm=ins&0xffff
            base = 0 if rA==0 else regs.get(rA,0)
            regs[rD]=(base + (imm<<16)) & 0xffffffff
        elif op==14:                     # addi
            rD=(ins>>21)&31; rA=(ins>>16)&31
            imm=ins&0xffff
            if imm & 0x8000: imm -= 0x10000
            if rA==0: regs[rD]=imm & 0xffffffff
            elif rA in regs:
                v=(regs[rA]+imm)&0xffffffff
                regs[rD]=v
                if 0x82000000 <= v < 0x82D00000:
                    try: pend.append((v, vtable_class(v)[0]))
                    except Exception: pass
        elif op==18 and (ins&1):         # bl
            li = ins & 0x03fffffc
            if li & 0x02000000: li -= 0x04000000
            tgt=(va+off+li)&0xffffffff
            events.append((f'0x{va+off:08X}', pend[-1] if pend else None, f'0x{tgt:08X}'))
            pend=[]
        elif op==18 and not (ins&1):
            pass
    return events
