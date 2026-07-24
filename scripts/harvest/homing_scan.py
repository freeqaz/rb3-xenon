#!/usr/bin/env python3
"""Reloc-masked byte-identity homing scan against band.exe .pdata inventory.

For each function in our freshly-compiled objs (>=MINSZ bytes), find every
.pdata entry in band.exe with the SAME function length, mask our reloc offsets
on BOTH sides, and compare. UNIQUE (exactly one identical VA, not already
mapped) => confident retail home.
"""
import os, struct, sys, json, bisect
from pathlib import Path
from collections import defaultdict

ROOT = os.environ.get("HOMING_ROOT", "/home/free/code/milohax/rb3-xenon")
WT   = os.environ.get("HOMING_WT", "/home/free/tmp/wt-homing")
BAND = os.environ.get("HOMING_BAND", f"{ROOT}/orig/45410914/band.exe")
TMAP = os.environ.get("HOMING_TMAP", f"{ROOT}/scripts/target_symbol_map.json")
OUT  = os.environ.get("HOMING_OUT", "/home/free/tmp/homing_results.json")
MINSZ = int(os.environ.get("HOMING_MINSZ", "32"))

# ---------- COFF obj parse (reuse correlator logic, expose reloc offsets) ----------
def parse_obj(path):
    data = Path(path).read_bytes()
    nsec = struct.unpack_from("<H", data, 2)[0]
    symoff = struct.unpack_from("<I", data, 8)[0]
    nsym = struct.unpack_from("<I", data, 12)[0]
    optsz = struct.unpack_from("<H", data, 16)[0]
    str_start = symoff + nsym*18
    secs = {}
    base = 20 + optsz
    for i in range(nsec):
        o = base + i*40
        raw_size = struct.unpack_from("<I", data, o+16)[0]
        praw = struct.unpack_from("<I", data, o+20)[0]
        preloc = struct.unpack_from("<I", data, o+24)[0]
        nreloc = struct.unpack_from("<H", data, o+32)[0]
        chars = struct.unpack_from("<I", data, o+36)[0]
        relocs = [struct.unpack_from("<I", data, preloc+j*10)[0] for j in range(nreloc)]
        secs[i+1] = dict(raw_size=raw_size, praw=praw, relocs=relocs, chars=chars)
    syms = []
    i = 0
    while i < nsym:
        o = symoff + i*18
        nm = data[o:o+8]
        if nm[:4] == b"\0\0\0\0":
            so = struct.unpack_from("<I", nm, 4)[0]
            e = data.index(b"\0", str_start+so)
            name = data[str_start+so:e].decode("latin1")
        else:
            name = nm.split(b"\0")[0].decode("latin1")
        val = struct.unpack_from("<I", data, o+8)[0]
        secn = struct.unpack_from("<h", data, o+12)[0]
        typ = struct.unpack_from("<H", data, o+14)[0]
        sc = data[o+16]; naux = data[o+17]
        syms.append(dict(name=name, val=val, secn=secn, typ=typ, sc=sc))
        i += 1 + naux
    return data, secs, syms

IMAGE_SCN_CNT_CODE = 0x20

def extract_funcs(path):
    """Return {name: (masked_bytes, [func-relative reloc offsets])}."""
    data, secs, syms = parse_obj(path)
    by_sec = defaultdict(list)
    for sy in syms:
        if sy["secn"] > 0 and sy["secn"] in secs and sy["sc"] in (2,3) and sy["typ"] == 0x20:
            by_sec[sy["secn"]].append(sy)
    out = {}
    for secn, members in by_sec.items():
        s = secs[secn]
        if not (s["chars"] & IMAGE_SCN_CNT_CODE) or s["praw"] == 0:
            continue
        members = sorted(members, key=lambda x: x["val"])
        for k, sy in enumerate(members):
            start = sy["val"]
            end = members[k+1]["val"] if k+1 < len(members) else s["raw_size"]
            if end <= start:
                continue
            body = bytearray(data[s["praw"]+start:s["praw"]+end])
            offs = []
            for rva in s["relocs"]:
                if start <= rva < end:
                    off = rva - start
                    offs.append(off)
                    for b in range(4):
                        if off+b < len(body):
                            body[off+b] = 0
            if sy["name"] not in out:
                out[sy["name"]] = (bytes(body), sorted(set(offs)))
    return out

# ---------- band.exe PE parse ----------
def parse_band():
    data = open(BAND, "rb").read()
    e = struct.unpack_from("<I", data, 0x3C)[0]; coff = e+4
    num = struct.unpack_from("<H", data, coff+2)[0]
    optsz = struct.unpack_from("<H", data, coff+16)[0]; opt = coff+20
    baseimg = struct.unpack_from("<I", data, opt+28)[0]; st = opt+optsz
    secs = {}
    for i in range(num):
        o = st+i*40; nm = data[o:o+8].rstrip(b"\x00").decode("latin1")
        va = baseimg+struct.unpack_from("<I", data, o+12)[0]
        vs = struct.unpack_from("<I", data, o+8)[0]
        praw = struct.unpack_from("<I", data, o+20)[0]
        rs = struct.unpack_from("<I", data, o+16)[0]
        secs[nm] = (va, vs, praw, rs)
    # .pdata function inventory
    va, vs, rp, rs = secs['.pdata']
    ents = []
    for i in range(rs//8):
        b = struct.unpack_from('>I', data, rp+i*8)[0]
        w1 = struct.unpack_from('>I', data, rp+i*8+4)[0]
        if b:
            funclen = ((w1 >> 8) & 0x3FFFFF)*4
            ents.append((b, funclen))
    ents.sort()
    return data, secs, ents

def band_bytes(data, secs, va, size):
    tva, tvs, tpraw, trs = secs['.text']
    off = va - tva
    if off < 0 or off+size > trs:
        return None
    return data[tpraw+off:tpraw+off+size]

def masked_eq(our_body, band_body, offs):
    if band_body is None or len(our_body) != len(band_body):
        return False
    bb = bytearray(band_body)
    for off in offs:
        for b in range(4):
            if off+b < len(bb):
                bb[off+b] = 0
    return bytes(bb) == our_body

# ---------- main ----------
def load_map():
    m = json.load(open(TMAP))
    mapped = set()
    for k in m:
        if k.startswith('_'):
            continue
        try:
            mapped.add(int(k, 16))
        except ValueError:
            pass
    return mapped

def main():
    mapped = load_map()
    data, secs, ents = parse_band()
    by_size = defaultdict(list)
    for b, fl in ents:
        by_size[fl].append(b)

    objs = {
        'GameGem':      f"{WT}/build/45410914/src/system/beatmatch/GameGem.obj",
        'GameGemList':  f"{WT}/build/45410914/src/system/beatmatch/GameGemList.obj",
        'GameGemDB':    f"{WT}/build/45410914/src/system/beatmatch/GameGemDB.obj",
        'BeatMatchUtl': f"{WT}/build/45410914/src/system/beatmatch/BeatMatchUtl.obj",
        'TrackType':    f"{WT}/build/45410914/src/system/beatmatch/TrackType.obj",
        'Msg':          f"{WT}/build/45410914/src/system/obj/Msg.obj",
    }
    if os.environ.get("HOMING_NO_DEFAULTS"):
        objs = {}
    # optional extra objs passed as argv: name=path
    for a in sys.argv[1:]:
        n, p = a.split('=', 1)
        objs[n] = p

    results = {}
    FUNCLET = ('__unwind', '__ehhandler', '__catch', '__tls', '__GSHandler')
    if os.environ.get("HOMING_FUNCLETS"):
        FUNCLET = ()
    for tu, path in objs.items():
        if not Path(path).exists():
            results[tu] = dict(error='no obj')
            continue
        funcs = extract_funcs(path)
        tu_res = []
        for name, (body, offs) in sorted(funcs.items()):
            if len(body) < MINSZ:
                continue
            if any(f in name for f in FUNCLET):
                continue
            cands = by_size.get(len(body), [])
            hits = [va for va in cands if masked_eq(body, band_bytes(data, secs, va, len(body)), offs)]
            unmapped_hits = [va for va in hits if va not in mapped]
            rec = dict(name=name, size=len(body), nreloc=len(offs),
                       n_hits=len(hits), hits=[f"0x{v:08x}" for v in hits],
                       n_unmapped=len(unmapped_hits))
            if len(hits) == 0:
                rec['cls'] = 'NOMATCH'
            elif len(hits) == 1 and len(unmapped_hits) == 1:
                rec['cls'] = 'UNIQUE'; rec['va'] = f"0x{hits[0]:08x}"
            elif len(unmapped_hits) == 1 and len(hits) > 1:
                # one unmapped + others already mapped (ICF sibling)
                rec['cls'] = 'UNIQUE-ICF'; rec['va'] = f"0x{unmapped_hits[0]:08x}"
            elif len(hits) >= 1 and len(unmapped_hits) == 0:
                rec['cls'] = 'ALL-MAPPED'
            else:
                rec['cls'] = 'MULTI'
            tu_res.append(rec)
        results[tu] = tu_res
    json.dump(results, open(OUT, 'w'), indent=1)

    # summary
    for tu, res in results.items():
        if isinstance(res, dict):
            print(f"{tu}: {res}"); continue
        cnt = defaultdict(int)
        for r in res:
            cnt[r['cls']] += 1
        print(f"\n=== {tu}: {len(res)} funcs >= {MINSZ}B ===  {dict(cnt)}")
        for r in res:
            if r['cls'] in ('UNIQUE', 'UNIQUE-ICF'):
                print(f"  {r['cls']:11s} {r['va']} size={r['size']:4d} rel={r['nreloc']:2d}  {r['name'][:70]}")
        for r in res:
            if r['cls'] == 'MULTI':
                print(f"  MULTI({r['n_hits']}/{r['n_unmapped']}u) size={r['size']:4d} {r['name'][:60]}")

if __name__ == "__main__":
    main()
