"""Minimal COFF (XCOFF-ish PE/COFF for PPCBE) reader + reimplementation of
objdiff's funclet_signature / is_funclet_like, so we can independently verify
which target symbols pair with which base symbols and why.

Deliberately NOT using tools/coff_bodies_ext.py: its function_bodies() has a
documented symbol-counting bug (requires exactly one defining symbol at
offset 0), and this lane's whole question is about symbol-counting assumptions.
"""
import struct, sys, collections

IMAGE_SCN_CNT_CODE = 0x00000020


def parse(path):
    d = open(path, 'rb').read()
    machine, nsec, tstamp, symptr, nsyms, optsz, chars = struct.unpack_from('<HHIIIHH', d, 0)
    off = 20 + optsz
    strtab_off = symptr + nsyms * 18
    strtab = d[strtab_off:]

    def strname(raw):
        if raw[:4] == b'\0\0\0\0':
            o = struct.unpack_from('<I', raw, 4)[0]
            e = strtab.index(b'\0', o)
            return strtab[o:e].decode('utf-8', 'replace')
        return raw.rstrip(b'\0').decode('utf-8', 'replace')

    sections = []
    for i in range(nsec):
        b = d[off + i * 40: off + (i + 1) * 40]
        name = strname(b[:8]) if b[:4] != b'\0\0\0\0' else strname(b[:8])
        nm = b[:8].rstrip(b'\0').decode('utf-8', 'replace')
        if nm.startswith('/'):  # long section name
            o = int(nm[1:])
            e = strtab.index(b'\0', o)
            nm = strtab[o:e].decode('utf-8', 'replace')
        vsize, vaddr, rawsize, rawptr, relptr, lnoptr, nrel, nlno, schars = struct.unpack_from('<IIIIIIHHI', b, 8)
        data = d[rawptr:rawptr + rawsize] if rawptr else b''
        relocs = []
        for r in range(nrel):
            ra, rsym, rtype = struct.unpack_from('<IIH', d, relptr + r * 10)
            relocs.append((ra, rsym, rtype))
        sections.append(dict(index=i + 1, name=nm, size=rawsize, data=data,
                             rawptr=rawptr, relocs=relocs, chars=schars,
                             is_code=bool(schars & IMAGE_SCN_CNT_CODE)))

    syms = []
    i = 0
    while i < nsyms:
        raw = d[symptr + i * 18: symptr + (i + 1) * 18]
        name = strname(raw[:8])
        value, secnum, stype, sclass, naux = struct.unpack_from('<IhHBB', raw, 8)
        syms.append(dict(idx=i, name=name, value=value, sec=secnum,
                         type=stype, sclass=sclass, naux=naux))
        i += 1 + naux
    return dict(sections=sections, symbols=syms, nsyms=nsyms)


def is_funclet_like(name):
    """Verbatim port of objdiff-core/src/diff/mod.rs:815 is_funclet_like."""
    for p in ('__unwind$', '__catch$'):
        if name.startswith(p):
            rest = name[len(p):]
            return len(rest) > 0 and all(c.isdigit() for c in rest)
    if name.startswith('__unwind__merged_'):
        return True
    if name.startswith('fn_'):
        rest = name[3:]
        return len(rest) == 8 and all(c in '0123456789abcdefABCDEF' for c in rest)
    if name.startswith('??__E') or name.startswith('??__F'):
        return True
    return False


def code_defs(obj):
    """Defining symbols in code sections, with inferred size = extent to the next
    defining symbol in the same section, else section end (objdiff's inference)."""
    per = collections.defaultdict(list)
    for s in obj['symbols']:
        if s['sec'] <= 0:
            continue
        sec = obj['sections'][s['sec'] - 1]
        if not sec['is_code']:
            continue
        if s['sclass'] not in (2, 3):  # EXTERNAL, STATIC
            continue
        if s['type'] != 0x20:  # Function kind only. type 0x00 = label ($M#####,
            continue           # $L####), which objdiff SKIPS as a size boundary.
        per[s['sec']].append(s)
    out = []
    for secnum, lst in per.items():
        sec = obj['sections'][secnum - 1]
        lst.sort(key=lambda s: s['value'])
        for j, s in enumerate(lst):
            nxt = next((o['value'] for o in lst[j + 1:] if o['value'] > s['value']), sec['size'])
            # ArchPpc::infer_function_size: trim trailing zero, unrelocated words.
            relocd = {r[0] for r in sec['relocs']}
            while nxt >= s['value'] + 4 and sec['data'][nxt - 4:nxt] == b'\0\0\0\0' \
                    and not any(nxt - 4 <= a < nxt for a in relocd):
                nxt -= 4
            out.append(dict(name=s['name'], sec=secnum, off=s['value'],
                            size=nxt - s['value'], sclass=s['sclass'], idx=s['idx']))
    return out


def signature(obj, d):
    """Port of funclet_signature (mod.rs:840): raw bytes with each relocation's
    4-byte instruction word zeroed."""
    sec = obj['sections'][d['sec'] - 1]
    b = bytearray(sec['data'][d['off']: d['off'] + d['size']])
    for (ra, rsym, rtype) in sec['relocs']:
        if ra < d['off'] or ra >= d['off'] + d['size']:
            continue
        o = ra - d['off']
        for k in range(o, min(o + 4, len(b))):
            b[k] = 0
    return bytes(b)
