#!/usr/bin/env python3
"""Frame-size-delta scanner (laneAT, 2026-07-26), from laneAT-f2's measured
mechanism.

★ Measured fact this is built on: an EH funclet whose only divergence is its
first word (`addi rN, r12, -frame_size`) flips to 100% when the PARENT's stack
frame becomes the right SIZE. The parent itself does not have to match --
verified by holding a parent at 99.4% with the frame corrected and watching all
6 of its funclets stay at 100.0%. So the FRAME_OFFSET class (1,088 functions
binary-wide) is priced as "make the frame the right size", not "body-port the
parent".

The most mechanical cause found: our source declares a named `String` local
where retail passed an inline temporary into a by-value `String` parameter.
Each such local costs sizeof(String) = 0x10 of frame. The tell is a call-count
delta on String's copy-ctor/dtor together with a frame delta of k * 0x10.

This scanner computes both sides straight from the COFF objs -- no build, no
objdiff run:
  * frame size  = immediate of the prologue `stwu r1, -N(r1)` (or the `addi/
                  subi r12, r1, -N` frame-pointer setup), target vs base
  * call counts = relocations resolving to a given mangled symbol inside the
                  function body, target vs base

Reports every named sub-100% function whose frame differs, ranked by whether
the delta is *explained* by a String (or other fixed-size type) call-count
delta -- those are the mechanical ones.

Requires a FULL build in the worktree first: setup_worktree.sh reflinks main's
dirty build dir, and a pre-build scan reads other lanes' uncommitted objs.

Usage: python3 scripts/harvest/frame_delta_scan.py <worktree> [--json out.json]
"""
import argparse, collections, glob, json, os, struct, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'analysis'))
try:
    from coffx import read_coff, infer_sizes, K_SEC
except ImportError:
    sys.path.insert(0, '/home/free/tmp/laneAM')
    from coffx import read_coff, infer_sizes, K_SEC

# type -> sizeof, for explaining a frame delta by a call-count delta
SIZED = {
    '??0String@@QAA@ABV0@@Z': 0x10, '??1String@@UAA@XZ': 0x10,
    '??0String@@QAA@PBD@Z': 0x10, '??0String@@QAA@XZ': 0x10,
}


def frame_size(code, off, size):
    """-> frame bytes from the prologue, or None."""
    for i in range(off, min(off + size, off + 40), 4):
        w = struct.unpack('>I', code[i:i + 4])[0]
        if (w >> 26) == 37 and ((w >> 21) & 31) == 1 and ((w >> 16) & 31) == 1:
            imm = w & 0xffff                      # stwu r1, -N(r1)
            return (0x10000 - imm) if imm & 0x8000 else imm
    return None


def scan_obj(path):
    try:
        data = open(path, 'rb').read()
    except OSError:
        return None
    secs, syms = read_coff(data)
    if secs is None:
        return None
    infer_sizes(secs, syms)
    # ★ COFF relocation SymbolTableIndex is the TRUE symbol index, which is
    # NOT the list position: coffx.read_coff skips aux records (i += 1 + naux).
    # Indexing a list by it names a random symbol. laneAT-f4 found this after
    # the bad columns made 110 of 112 rows look non-mechanical.
    by_idx = {s.index: s for s in syms}
    out = {}
    for s in syms:
        if s.sec <= 0 or s.size == 0 or s.kind == K_SEC or s.cls not in (2, 3):
            continue
        sec = secs[s.sec - 1]
        if not sec.is_code:
            continue
        calls = collections.Counter()
        for (va, si, typ) in sec.relocs:
            if s.value <= va < s.value + s.size and si in by_idx:
                calls[by_idx[si].name] += 1
        out.setdefault(s.name, (frame_size(sec.data, s.value, s.size), calls, s.size))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('worktree')
    ap.add_argument('--json')
    a = ap.parse_args()
    wt = a.worktree
    rep = json.load(open(os.path.join(wt, 'build/45410914/report.json')))
    pct = {f['name']: f['match_percent_normalized']
           for u in rep['units'] for f in (u.get('functions') or [])}
    base_by_name = collections.defaultdict(list)
    for p in glob.glob(os.path.join(wt, 'build/45410914/src/**/*.obj'), recursive=True):
        base_by_name[os.path.basename(p)].append(p)
    root = os.path.join(wt, 'build/45410914/obj')
    stats, rows = collections.Counter(), []
    for tp in sorted(glob.glob(os.path.join(root, '**', '*.obj'), recursive=True)):
        rel = os.path.relpath(tp, root)
        bp = os.path.join(wt, 'build/45410914/src', rel)
        if not os.path.exists(bp):
            c = base_by_name.get(os.path.basename(tp))
            if not c or len(c) != 1:
                continue
            bp = c[0]
        ts, bs = scan_obj(tp), scan_obj(bp)
        if not ts or not bs:
            continue
        for nm, (tf, tc, tsz) in ts.items():
            if nm.startswith('fn_') or nm.startswith('__') or '$' in nm[:2]:
                continue
            p = pct.get(nm)
            if p is None or p >= 100.0:
                continue
            b = bs.get(nm)
            if not b:
                continue
            bf, bc, bsz = b
            if tf is None or bf is None or tf == bf:
                stats['frame_equal_or_unknown'] += 1
                continue
            delta = bf - tf                      # >0 = our frame is too big
            stats['frame_differs'] += 1
            expl, resid = [], abs(delta)
            for sym, width in SIZED.items():
                d = bc.get(sym, 0) - tc.get(sym, 0)
                if d:
                    expl.append({'sym': sym, 'call_delta': d, 'width': hex(width)})
            netcalls = sum(bc.get(s, 0) - tc.get(s, 0) for s in SIZED)
            k = abs(delta) // 0x10
            mech = bool(expl) and netcalls != 0 and abs(delta) % 0x10 == 0
            if mech:
                stats['★MECHANICAL(String local vs inline temporary)'] += 1
            rows.append({'unit': rel, 'sym': nm, 'pct': p, 'size': tsz,
                         'frame_tgt': hex(tf), 'frame_base': hex(bf),
                         'delta': hex(delta), 'k_x10': k,
                         'string_call_delta': netcalls, 'explain': expl,
                         'mechanical': mech})
    print(json.dumps(stats, indent=1))
    rows.sort(key=lambda r: (not r['mechanical'], -r['pct']))
    if a.json:
        json.dump(rows, open(a.json, 'w'), indent=1)
    print('\ntop mechanical candidates:')
    for r in [x for x in rows if x['mechanical']][:25]:
        print('  %-26s %-52s pct=%-9s frame %s->%s (k=%d, Stringcalls%+d)'
              % (r['unit'][:26], r['sym'][:52], r['pct'], r['frame_tgt'],
                 r['frame_base'], r['k_x10'], r['string_call_delta']))


if __name__ == '__main__':
    main()
