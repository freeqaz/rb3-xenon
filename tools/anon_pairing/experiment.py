"""BEHAVIOURAL EXPERIMENT: positional vs byte-signature pairing.

Setup: copy the target+base objs for one unit to scratch. In the BASE copy, SWAP
the raw code bytes of two funclet-like symbols F1, F2 that
  * have the same inferred size,
  * have DIFFERENT reloc-masked signatures,
  * carry no relocations in range (so swapping bytes is semantically clean),
  * are each currently the exact-signature partner of a target `fn_` row at 100%.

Predictions (these DIFFER, which is the point):
  H_byte : T1 now finds its signature at F2 and T2 at F1  -> BOTH still 100%.
  H_pos  : T1 still pairs positionally with F1, which now holds F2's bytes
           -> BOTH drop below 100%.

Controls in the same batch (gate 2: a known-good row in every batch):
  KG-named : an untouched named function at 100%  -> must stay 100.
  KG-anon  : an untouched anon fn_ at 100%        -> must stay 100.
  NEG      : a THIRD base funclet whose bytes we corrupt to a value matching
             nothing -> its target row MUST fall. Proves the harness can detect
             a change at all (an experiment that cannot fail proves nothing).
"""
import sys, json, shutil, subprocess, os, collections
sys.path.insert(0, '/home/free/tmp/laneCX4')
import coffsig as C

WT = '/home/free/tmp/laneCX4/wt'
SCR = '/home/free/tmp/laneCX4/exp'
CLI = '/home/free/code/milohax/objdiff/target/release/objdiff-cli'
UNIT = sys.argv[1] if len(sys.argv) > 1 else 'default/HamCamTransform'


def sig_of(obj, d):
    return C.signature(obj, d)


def relocs_in(obj, d):
    """Reloc SHAPE relative to the symbol start: (offset, type). Two symbols with
    the same shape mask identically, so swapping their raw bytes is a clean
    swap of their masked signatures. Reloc TARGETS are left attached to their
    original site (and are masked out anyway)."""
    sec = obj['sections'][d['sec'] - 1]
    return tuple(sorted((r[0] - d['off'], r[2]) for r in sec['relocs']
                        if d['off'] <= r[0] < d['off'] + d['size']))


def query(tobj, bobj, sym):
    r = subprocess.run([CLI, 'diff', '-1', tobj, '-2', bobj, '-f', 'json', '-o', '-',
                        '-c', 'functionRelocDiffs=none', sym],
                       capture_output=True, text=True)
    if r.returncode != 0:
        return None
    try:
        j = json.loads(r.stdout)
    except Exception:
        return None
    return j


def main():
    cfg = json.load(open(f'{WT}/objdiff.json'))
    uc = next(u for u in cfg['units'] if u['name'] == UNIT)
    tpath, bpath = f"{WT}/{uc['target_path']}", f"{WT}/{uc['base_path']}"
    tgt, bas = C.parse(tpath), C.parse(bpath)

    L = [d for d in C.code_defs(tgt) if C.is_funclet_like(d['name']) and d['size'] > 0]
    R = [d for d in C.code_defs(bas) if C.is_funclet_like(d['name']) and d['size'] > 0]
    L.sort(key=lambda d: d['name']); R.sort(key=lambda d: d['name'])
    ls = {d['name']: sig_of(tgt, d) for d in L}
    rs = {d['name']: sig_of(bas, d) for d in R}

    # base funclets that are the UNIQUE exact partner of exactly one target row
    lcount = collections.Counter(ls.values())
    rcount = collections.Counter(rs.values())
    cands = []
    for rd in R:
        s = rs[rd['name']]
        if rcount[s] != 1 or lcount[s] != 1:
            continue
        ld = next(d for d in L if ls[d['name']] == s)
        cands.append((rd, ld, s))
    # group by (size, reloc shape) so a byte swap is signature-clean
    bysize = collections.defaultdict(list)
    for rd, ld, s in cands:
        bysize[(rd['size'], relocs_in(bas, rd))].append((rd, ld, s))
    # Choose the triple whose two swap members are MOST dissimilar, so that under
    # H_pos the cross-scored pair would collapse (weak pairs make a weak test).
    pair = None; best = -1
    for key, lst in bysize.items():
        if len(lst) < 3:
            continue
        for i in range(len(lst)):
            for j in range(i + 1, len(lst)):
                si, sj = lst[i][2], lst[j][2]
                d = sum(1 for x, y in zip(si, sj) if x != y)
                if d > best:
                    k = next(x for x in range(len(lst)) if x not in (i, j))
                    best = d; pair = [lst[i], lst[j], lst[k]]
    print(f'  chosen swap pair differs in {best} of {pair[0][0]["size"]} masked bytes')
    if not pair:
        print('no candidate triple found in', UNIT); return
    (r1, t1, s1), (r2, t2, s2), (r3, t3, s3) = pair
    print(f'unit {UNIT}  size {r1["size"]}')
    print(f'  SWAP  base {r1["name"]} <-> {r2["name"]}   (targets {t1["name"]}, {t2["name"]})')
    print(f'  NEG   base {r3["name"]} corrupted           (target {t3["name"]})')

    # known-good untouched rows
    rep = json.load(open(f'{WT}/build/45410914/report.json'))
    ru = next(x for x in rep['units'] if x['name'] == UNIT)
    def mpn(f):
        v = f.get('match_percent_normalized')
        return v if v is not None else f.get('fuzzy_match_percent', 0.0)
    touched = {t1['name'], t2['name'], t3['name']}
    kg_named = next(f['name'] for f in ru['functions']
                    if mpn(f) == 100.0 and not f['name'].startswith('fn_'))
    kg_anon = next(f['name'] for f in ru['functions']
                   if mpn(f) == 100.0 and f['name'].startswith('fn_')
                   and f['name'] not in touched)
    print(f'  KG-named {kg_named}\n  KG-anon  {kg_anon}')

    os.makedirs(SCR, exist_ok=True)
    tcopy, bA, bB = f'{SCR}/t.obj', f'{SCR}/base_A.obj', f'{SCR}/base_B.obj'
    shutil.copy(tpath, tcopy); shutil.copy(bpath, bA); shutil.copy(bpath, bB)

    data = bytearray(open(bpath, 'rb').read())
    def fo(d):
        return bas['sections'][d['sec'] - 1]['rawptr'] + d['off']
    n = r1['size']
    a, b = fo(r1), fo(r2)
    blk1, blk2 = bytes(data[a:a + n]), bytes(data[b:b + n])
    assert blk1 != blk2, 'chosen blocks identical -- swap would be a no-op'
    data[a:a + n], data[b:b + n] = blk2, blk1
    c = fo(r3)
    for k in range(0, n, 4):          # corrupt: unique nonsense word
        data[c + k:c + k + 4] = b'\x7f\x7f\x7f\x7f'
    open(bB, 'wb').write(bytes(data))

    # Leg C: NON-reciprocal overwrite -- F1 is given F2's bytes but F2 keeps its
    # own. T1's signature now exists nowhere; T2's exists twice. Under H_byte T1
    # must lose its exact partner (drop) while T2 holds. This proves the leg-B
    # "both stayed 100" is a property of the RECIPROCAL swap, not inertness.
    bC = f'{SCR}/base_C.obj'
    dc = bytearray(open(bpath, 'rb').read())
    dc[a:a + n] = blk2
    open(bC, 'wb').write(bytes(dc))

    rows = [('T1(swapped)', t1['name']), ('T2(swapped)', t2['name']),
            ('T3(NEG corrupt)', t3['name']), ('KG-anon', kg_anon), ('KG-named', kg_named)]
    print(f'\n{"row":18s} {"symbol":44s} {"legA":>7s} {"legB":>7s} {"legC":>7s}  B?  C?')
    for label, sym in rows:
        r = {}
        for k, ob in (('A', bA), ('B', bB), ('C', bC)):
            j = query(tcopy, ob, sym)
            r[k] = j and j.get('normalized_match_percent')
        print(f'{label:18s} {sym:44s} {str(r["A"]):>7s} {str(r["B"]):>7s} {str(r["C"]):>7s}  '
              f'{"=" if r["A"] == r["B"] else "CHG":>3s} {"=" if r["A"] == r["C"] else "CHG":>3s}')


main()
