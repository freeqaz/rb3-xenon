#!/usr/bin/env python3
"""
symbols_hygiene_fix.py — rewrite config/45410914/symbols.txt to remove stale
TU5-era carving artifacts inside pinned .text ranges.

Ground truth = band.exe .pdata (decompressed default.xex). See symbols_hygiene.py.

Transformation (scoped to pinned .text splits only):
  For each maximal pdata GAP [g0,g1) (region between consecutive pdata-covered
  functions) that lies inside a pinned .text range AND contains a SPURIOUS
  except_data_ (an except_data_ whose named func start is NOT a func_type==3
  pdata start):
    - delete every spurious except_data_ (.text) in the gap
    - delete the matching except_record_ (.rdata, same address suffix)
    - merge all fn_ entries starting in the gap into a single fn_ at g0 with
      size (g1 - g0)   [gap == one leaf function, the Sort.cpp shape]
  VALID except_data_ (at a real func_type3 start-8) are preserved untouched, and
  their following pdata-covered function is preserved (never merged).

Only gaps containing a spurious except_data are touched — this is the exact
proven Sort.cpp pattern and keeps the blast radius auditable.

Usage:
  python3 scripts/symbols_hygiene_fix.py [--only UNIT1,UNIT2] [--apply]
Without --apply it prints the plan. With --apply it rewrites symbols.txt.
"""
import re, struct, sys, os, bisect

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BAND = os.path.join(ROOT, "orig/45410914/band.exe")
SYMS = os.path.join(ROOT, "config/45410914/symbols.txt")
SPLITS = os.path.join(ROOT, "config/45410914/splits.txt")


def parse_pdata(path):
    d = open(path, "rb").read()
    peoff = struct.unpack("<I", d[0x3C:0x40])[0]; coff = peoff + 4
    _, nsec, _, _, _, optsz, _ = struct.unpack("<HHIIIHH", d[coff:coff + 20])
    opt = coff + 20
    imagebase = struct.unpack("<I", d[opt + 28:opt + 32])[0]
    sechdr = opt + optsz
    pinfo = None
    for i in range(nsec):
        o = sechdr + i * 40
        name = d[o:o + 8].rstrip(b"\x00").decode("latin1")
        vsz, va, rawsz, rawptr = struct.unpack("<IIII", d[o + 8:o + 24])
        if name == ".pdata":
            pinfo = (imagebase + va, vsz, rawptr)
    pva, pvsz, prawptr = pinfo
    pd = d[prawptr:prawptr + pvsz]
    funcs = {}
    t3 = set()
    for i in range(0, len(pd) - 7, 8):
        sa = struct.unpack(">I", pd[i:i + 4])[0]
        if sa == 0:
            break
        w = struct.unpack(">I", pd[i + 4:i + 8])[0]
        funcs[sa] = ((w >> 8 & 0x3FFFFF) * 4, w >> 30)
        if (w >> 30) == 3:
            t3.add(sa)
    return funcs, t3


def parse_splits(path):
    ranges = []
    cur = None
    for line in open(path):
        s = line.strip()
        m = re.match(r"^([\w./\-]+\.\w+):$", s)
        if m:
            cur = m.group(1); continue
        m = re.match(r"\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)", s)
        if m and cur:
            ranges.append((cur, int(m.group(1), 16), int(m.group(2), 16)))
    return ranges


def main():
    funcs, t3 = parse_pdata(BAND)
    starts = sorted(funcs.keys())
    # covered intervals
    covered = [(s, s + funcs[s][0]) for s in starts]
    # build gap list: between covered[i].end and covered[i+1].start
    gaps = []
    for i in range(len(covered) - 1):
        e = covered[i][1]
        ns = covered[i + 1][0]
        if ns > e:
            gaps.append((e, ns))
    gap_starts = [g[0] for g in gaps]

    def gap_for(addr):
        i = bisect.bisect_right(gap_starts, addr) - 1
        if i < 0:
            return None
        g0, g1 = gaps[i]
        if g0 <= addr < g1:
            return (g0, g1)
        return None

    # protected addresses = named/paired target functions (target_symbol_map.json)
    import json as _json
    protected = set()
    name2addr = {}
    tmap = os.path.join(ROOT, "scripts/target_symbol_map.json")
    if os.path.exists(tmap):
        mp = _json.load(open(tmap))
        for k, v in mp.items():
            try:
                a = int(k, 16)
            except ValueError:
                continue
            protected.add(a)
            for nm in (v if isinstance(v, list) else [v]):
                name2addr[nm] = a
    # addresses of functions ALREADY matched at 100% in the current baseline.
    # We must never resize/merge one of these (it is already correct).
    matched_addrs = set()
    base_report = os.path.join(ROOT, "build/45410914/report.json")
    if os.path.exists(base_report):
        for u in _json.load(open(base_report)).get("units", []):
            for f in (u.get("functions") or []):
                if f.get("match_percent_normalized", 0) == 100 and f["name"] in name2addr:
                    matched_addrs.add(name2addr[f["name"]])

    ranges = parse_splits(SPLITS)
    only = None
    if "--only" in sys.argv:
        only = set(sys.argv[sys.argv.index("--only") + 1].split(","))

    # map every address -> containing pinned unit (first match)
    def unit_for(addr):
        for u, s, e in ranges:
            if s <= addr < e:
                return u
        return None

    # read symbols.txt lines, index fn_/except_data_ in .text and except_record_ in .rdata
    lines = open(SYMS).read().splitlines()
    fn_lines = {}       # addr -> (lineidx, size)
    exc_text = {}       # addr -> lineidx  (except_data_ in .text)
    exc_rec = {}        # addr_suffix(int) -> lineidx (except_record_ anywhere)
    for idx, line in enumerate(lines):
        m = re.match(r"^(\S+)\s*=\s*\.(\w+):0x([0-9A-Fa-f]+);(.*)$", line)
        if not m:
            continue
        name, sect, ah, rest = m.groups()
        addr = int(ah, 16)
        if name.startswith("fn_") and sect == "text":
            sm = re.search(r"size:0x([0-9A-Fa-f]+)", rest)
            fn_lines[addr] = (idx, int(sm.group(1), 16) if sm else 0)
        elif name.startswith("except_data_") and sect == "text":
            exc_text[addr] = idx
        elif name.startswith("except_record_"):
            m2 = re.match(r"except_record_([0-9A-Fa-f]+)", name)
            if m2:
                exc_rec[int(m2.group(1), 16)] = idx

    # Find spurious except_data in .text within pinned ranges → determine gaps to fix
    del_lines = set()
    edits = {}   # lineidx -> new text (for the merged fn_)
    plan = []    # (unit, g0, g1, removed_fns[], merged_size)
    handled_gaps = set()
    for addr, idx in exc_text.items():
        func = addr + 8
        if func in t3:
            continue  # valid, skip
        u = unit_for(addr)
        if u is None:
            continue
        if only and u not in only:
            continue
        g = gap_for(addr)
        if g is None or g in handled_gaps:
            # still delete the spurious except_data even if no clean gap
            continue
        handled_gaps.add(g)
        g0, g1 = g
        # merged region ends at g1, but if the next covered fn is func_type3 its
        # 8-byte exception data occupies [g1-8, g1) and must NOT be absorbed.
        merge_end = g1 - 8 if g1 in t3 else g1
        # Confine the ENTIRE operation to the single pinned split range that
        # contains the spurious except_data addr — never cross a split boundary.
        rng = None
        for ru, rs, re_ in ranges:
            if rs <= addr < re_:
                rng = (rs, re_)
                break
        if rng is None:
            continue
        rs, re_ = rng
        # Skip DEDICATED tiny split ranges that pin exactly the exception-data
        # object (e.g. 0x...A0..0x...A8, 8 bytes). Deleting the except_data there
        # leaves a hole dtk fills with an oversized auto-fn -> split failure.
        # These require a coordinated splits.txt edit; defer to backlog.
        if (re_ - rs) <= 0x10:
            continue
        lo = max(g0, rs)
        hi = min(merge_end, re_)
        if hi <= lo:
            continue
        # The merge anchor is the fn_ fragment that starts the real function
        # containing the spurious except_data: largest fn_ start <= addr.
        anchor_candidates = [a for a in fn_lines if lo <= a <= addr]
        if not anchor_candidates:
            continue
        anchor = max(anchor_candidates)
        # If the anchor is ALREADY matched at 100%, its current size is correct;
        # the spurious except_data lies after its true end. Resizing would break
        # it, so leave this gap untouched (backlog).
        if anchor in matched_addrs:
            continue
        # The anchor MAY be a protected (named) function — that's exactly the
        # function we're restoring by re-merging its stale-split fragments.
        # Clip merge_end so we never absorb a DIFFERENT protected function that
        # starts after the anchor, nor cross into a real pdata function start.
        for a in sorted(fn_lines):
            if anchor < a < hi and (a in protected or a in funcs):
                hi = a
                break
        merge_end = hi
        if merge_end <= anchor:
            continue
        # fragments to merge = unprotected fn_ starts in (anchor, merge_end)
        gap_fns = [anchor] + sorted(a for a in fn_lines if anchor < a < merge_end)
        # only the anchor may be protected; any other protected addr would be a
        # distinct named function we'd wrongly swallow.
        if any(a in protected for a in gap_fns[1:]):
            continue
        # spurious except_data strictly inside the merged span
        gap_exc = [a for a in exc_text if anchor <= a < merge_end and (a + 8) not in t3]
        # sanity: none of gap_fns is itself a real pdata start
        if any(a in funcs for a in gap_fns):
            continue
        # delete spurious except_data + matching except_record
        for a in gap_exc:
            del_lines.add(exc_text[a])
            # except_record named after (a+8)
            if (a + 8) in exc_rec:
                del_lines.add(exc_rec[a + 8])
        # merge fn_: keep first, resize to gap; delete rest
        if gap_fns:
            first = gap_fns[0]
            fidx, _ = fn_lines[first]
            newsize = merge_end - first
            if newsize <= 0:
                handled_gaps.discard(g)
                continue
            edits[fidx] = f"fn_{first:08X} = .text:0x{first:08X}; // type:function size:0x{newsize:X}"
            for a in gap_fns[1:]:
                del_lines.add(fn_lines[a][0])
        plan.append((u, g0, merge_end, gap_fns, merge_end - (gap_fns[0] if gap_fns else g0)))

    # report
    from collections import Counter
    unit_ct = Counter(p[0] for p in plan)
    print(f"gaps to fix: {len(plan)}  across {len(unit_ct)} units")
    print(f"lines deleted: {len(del_lines)}  fn_ merged/resized: {len(edits)}")
    for u, ct in unit_ct.most_common(30):
        print(f"   {ct:3d}  {u}")

    if "--apply" in sys.argv:
        out = []
        for idx, line in enumerate(lines):
            if idx in del_lines:
                continue
            if idx in edits:
                out.append(edits[idx])
            else:
                out.append(line)
        open(SYMS, "w").write("\n".join(out) + "\n")
        print(f"APPLIED. wrote {SYMS} ({len(lines)} -> {len(out)} lines)")


if __name__ == "__main__":
    main()
