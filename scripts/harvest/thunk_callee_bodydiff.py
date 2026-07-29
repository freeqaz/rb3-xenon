#!/usr/bin/env python3
"""thunk_callee_bodydiff -- derive the virtual-function bodies whose NAME is
handed out for free by a retail adjustor thunk, and classify them by whether our
compiled obj already has that symbol and whether its bytes agree.

A thunk is 2-4 instructions whose reloc-masked bytes encode exactly
(vtordisp, this-adjust, length).  So a target thunk and a compiled thunk with
identical masked bytes are the same thunk shape; when the shape occurs exactly
once on each side inside a unit, the pairing is forced, and the compiled thunk's
own relocation names its callee.  That name then belongs to the target VA the
retail thunk branches to -- with no naming heuristic anywhere.

Output classes for each derived (calleeVA -> calleeName):
  SAME   our obj has the symbol, reloc-masked bytes equal  (already matching)
  DIFF   our obj has the symbol, bytes differ              <- the work pool
  ABSENT our obj lacks the symbol entirely
Read-only.
"""
import sys, json, struct, re, collections
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts" / "harvest"))
from size_order_automap import _ordered_funcs, _asm_target_funcs, _parse_coff

BUILD = ROOT / "build" / "45410914"
SPLITS = ROOT / "config" / "45410914" / "splits.txt"
IMAGE = ROOT / "orig" / "45410914" / "band.exe"

raw = json.load(open(ROOT / "scripts" / "target_symbol_map.json"))
MAP = {int(k, 16): v for k, v in raw.items()
       if k.lower().startswith("0x") and isinstance(v, str)}

d = open(IMAGE, 'rb').read()
pe = struct.unpack_from("<I", d, 0x3C)[0]
nsec = struct.unpack_from("<H", d, pe + 6)[0]
oh = struct.unpack_from("<H", d, pe + 20)[0]
ib = struct.unpack_from("<I", d, pe + 24 + 28)[0]
secs = []
for i in range(nsec):
    o = pe + 24 + oh + i * 40
    vs, va, rs, ro = struct.unpack_from("<IIII", d, o + 8)
    secs.append((ib + va, vs, ro))


def word(va):
    for sva, vs, ro in secs:
        if sva <= va < sva + vs:
            return struct.unpack_from(">I", d, ro + (va - sva))[0]


def shape(va, sz):
    """Return (vtordisp, adj, calleeVA) if the bytes at va are an adjustor thunk."""
    if not sz or sz > 0x20:
        return None
    ws = [word(va + 4 * i) for i in range(sz // 4)]
    if any(w is None for w in ws):
        return None
    while ws and ws[-1] == 0:
        ws.pop()
    if not ws:
        return None
    idx = 0; vt = None; adj = 0; reg = None
    if len(ws) >= 3 and (ws[0] >> 26) == 32 and ((ws[0] >> 21) & 31) == 11:
        rA = (ws[0] >> 16) & 31; imm = ws[0] & 0xFFFF
        if imm >= 0x8000: imm -= 0x10000
        if ((ws[1] >> 26) == 31 and ((ws[1] >> 1) & 0x3FF) == 40
                and ((ws[1] >> 21) & 31) == rA and ((ws[1] >> 16) & 31) == 11
                and ((ws[1] >> 11) & 31) == rA):
            vt = imm; reg = rA; idx = 2
        else:
            return None
    if idx < len(ws) and (ws[idx] >> 26) == 14:
        rD = (ws[idx] >> 21) & 31; rA = (ws[idx] >> 16) & 31
        imm = ws[idx] & 0xFFFF
        if imm >= 0x8000: imm -= 0x10000
        if rD == rA and imm < 0 and (reg is None or rD == reg):
            adj = imm; reg = rD; idx += 1
        elif vt is None:
            return None
    if idx != len(ws) - 1:
        return None
    b = ws[idx]
    if b >> 26 != 18 or (b & 1) or ((b >> 1) & 1):
        return None
    li = b & 0x03FFFFFC
    if li & 0x02000000: li -= 0x04000000
    if vt is None and adj == 0:
        return None
    if reg not in (3, 4):
        return None
    return (vt, -adj, va + 4 * idx + li)


def load_units():
    u = collections.defaultdict(list); cur = None
    for line in open(SPLITS):
        if not line.strip() or line.startswith("Sections:"):
            continue
        if not line[0].isspace():
            cur = line.strip().rstrip(":"); continue
        p = line.split()
        if len(p) >= 3 and p[0] == ".text" and p[1].startswith("start:"):
            u[cur].append((int(p[1].split(":")[1], 16), int(p[2].split(":")[1], 16)))
    return u


def paths(u):
    rel = u[:-4] if u.endswith(".cpp") else u
    a = BUILD / "asm" / (rel + ".s")
    b = BUILD / "src" / (rel + ".obj")
    if not b.exists():
        c = list((BUILD / "src").rglob(Path(rel).name + ".obj"))
        b = c[0] if len(c) == 1 else b
    return a, b


def coff_callees(path):
    """{symbol_name: [callee_symbol_names]} using per-section relocations."""
    data = path.read_bytes()
    nsecs = struct.unpack_from("<H", data, 2)[0]
    symoff = struct.unpack_from("<I", data, 8)[0]
    nsym = struct.unpack_from("<I", data, 12)[0]
    optsz = struct.unpack_from("<H", data, 16)[0]
    str_start = symoff + nsym * 18
    names = []
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
        naux = data[o + 17]
        val = struct.unpack_from("<I", data, o + 8)[0]
        secn = struct.unpack_from("<h", data, o + 12)[0]
        typ = struct.unpack_from("<H", data, o + 14)[0]
        names.append(dict(name=name, val=val, secn=secn, typ=typ))
        for _ in range(naux):
            names.append(None)
        i += 1 + naux
    base = 20 + optsz
    out = collections.defaultdict(list)
    # function symbols per section
    fn_by_sec = collections.defaultdict(list)
    for s in names:
        if s and s["secn"] > 0 and s["typ"] == 0x20:
            fn_by_sec[s["secn"]].append(s)
    for si in range(nsecs):
        o = base + si * 40
        preloc = struct.unpack_from("<I", data, o + 24)[0]
        nreloc = struct.unpack_from("<H", data, o + 32)[0]
        chars = struct.unpack_from("<I", data, o + 36)[0]
        if not (chars & 0x20):
            continue
        secn = si + 1
        members = sorted(fn_by_sec.get(secn, []), key=lambda x: x["val"])
        for j in range(nreloc):
            rva, symidx = struct.unpack_from("<II", data, preloc + j * 10)
            tgt = names[symidx] if symidx < len(names) else None
            if not tgt:
                continue
            owner = None
            for k, mmb in enumerate(members):
                nxt = members[k + 1]["val"] if k + 1 < len(members) else 1 << 30
                if mmb["val"] <= rva < nxt:
                    owner = mmb["name"]; break
            if owner:
                out[owner].append(tgt["name"])
    return out


results = []
stats = collections.Counter()
for u in sorted(load_units()):
    asm, bobj = paths(u)
    if not (asm.exists() and bobj.exists()):
        continue
    try:
        tf = [(va, sz, mk) for va, sz, mk in _asm_target_funcs(asm) if va]
        bf = _ordered_funcs(bobj)
        callees = coff_callees(bobj)
    except Exception:
        continue
    tgt_by_mask = collections.defaultdict(list)
    tgt_body = {}
    for va, sz, mk in tf:
        tgt_body[va] = mk
    base_by_name = {f['name']: f for f in bf}
    # compiled thunks = tiny code syms whose body is <=0x20 and ends in a masked b
    comp_thunk_by_mask = collections.defaultdict(list)
    for f in bf:
        if f['size'] <= 0x20 and re.search(r"@@(W[0-9A-P?]|\$[24R])", f['name']):
            comp_thunk_by_mask[f['masked']].append(f)
    for va, sz, mk in tf:
        s = shape(va, sz)
        if not s:
            continue
        stats['target_thunks'] += 1
        tgt_by_mask[mk].append((va, s))
    for mask, tl in tgt_by_mask.items():
        cl = comp_thunk_by_mask.get(mask, [])
        if len(tl) != 1 or len(cl) != 1:
            stats['ambiguous_group'] += len(tl)
            continue
        (tva, s), cf = tl[0], cl[0]
        cands = [c for c in callees.get(cf['name'], []) if c and not c.startswith('.')]
        cands = [c for c in cands if c in base_by_name or c not in ('',)]
        # the single code callee
        code_c = [c for c in set(cands) if c in base_by_name or re.match(r"\?", c)]
        if len(set(code_c)) != 1:
            stats['callee_reloc_ambiguous'] += 1
            continue
        cname = code_c[0]
        cva = s[2]
        stats['derived'] += 1
        cur = MAP.get(cva)
        if cva not in tgt_body:
            stats['callee_outside_unit'] += 1
            cls = 'OUTSIDE'
        elif cname not in base_by_name:
            cls = 'ABSENT'
        elif base_by_name[cname]['masked'] == tgt_body[cva]:
            cls = 'SAME'
        else:
            cls = 'DIFF'
        stats[cls] += 1
        results.append(dict(
            unit=u, thunk_va="0x%08x" % tva, callee_va="0x%08x" % cva,
            name=cname, cls=cls, cur_map=cur,
            tgt_size=len(tgt_body.get(cva, b'')),
            base_size=base_by_name[cname]['size'] if cname in base_by_name else None,
        ))

print(dict(stats))
out = Path.home() / "tmp" / "bodyport_thunk_callees.json"
out.parent.mkdir(parents=True, exist_ok=True)
json.dump(results, open(out, "w"), indent=1)
print("wrote", out, len(results))
diffs = [r for r in results if r['cls'] == 'DIFF']
diffs.sort(key=lambda r: abs((r['tgt_size'] or 0) - (r['base_size'] or 0)))
print("\nDIFF pool, smallest |dsize| first:")
for r in diffs[:60]:
    dsz = (r['tgt_size'] or 0) - (r['base_size'] or 0)
    print(f"  d={dsz:+6d} tgt={r['tgt_size']:5d} {r['unit'][:40]:40s} {r['name'][:80]}")
