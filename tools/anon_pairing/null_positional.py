"""NULL for the byte-signature model: how well does a POSITIONAL pairing model
predict the same 197 outcomes?

H_byte  : pair by reloc-masked byte signature (objdiff mod.rs:1410).
H_pos   : pair the i-th unmatched target funclet-like symbol with the i-th
          unmatched base funclet-like symbol (symbol-table order).
H_rand  : random permutation (the random-offset null this project demands).

For H_pos / H_rand a pair "predicts 100" iff the two paired symbols happen to be
byte-identical after masking (otherwise it could never score 100 under ANY
pairing rule -- so this is the most generous possible reading of the positional
hypothesis).
"""
import sys, json, random, collections
sys.path.insert(0, '/home/free/tmp/laneCX4')
import coffsig as C

WT = '/home/free/tmp/laneCX4/wt'


def load(un):
    cfg = json.load(open(f'{WT}/objdiff.json'))
    uc = next(u for u in cfg['units'] if u['name'] == un)
    tgt = C.parse(f"{WT}/{uc['target_path']}")
    bas = C.parse(f"{WT}/{uc['base_path']}")
    L = [d for d in C.code_defs(tgt) if C.is_funclet_like(d['name']) and d['size'] > 0]
    R = [d for d in C.code_defs(bas) if C.is_funclet_like(d['name']) and d['size'] > 0]
    ls = {d['name']: C.signature(tgt, d) for d in L}
    rs = {d['name']: C.signature(bas, d) for d in R}
    return L, R, ls, rs


def truth(un):
    rep = json.load(open(f'{WT}/build/45410914/report.json'))
    u = next(x for x in rep['units'] if x['name'] == un)
    out = {}
    for f in u.get('functions', []):
        mpn = f.get('match_percent_normalized')
        mpn = mpn if mpn is not None else f.get('fuzzy_match_percent', 0.0)
        out[f['name']] = (mpn == 100.0)
    return out


def score(pairs, ls, rs, t):
    """pairs: list of (lname, rname_or_None). Returns (#decided, #correct)."""
    n = c = 0
    for l, r in pairs:
        if l not in t:
            continue
        pred = (r is not None and ls[l] == rs[r])
        n += 1
        c += (pred == t[l])
    return n, c


for un in ['default/Char', 'default/HamCamTransform']:
    L, R, ls, rs = load(un)
    t = truth(un)
    print(f'== {un}  (target funclet-likes {len(L)}, base funclet-likes {len(R)})')

    # H_pos: symbol-table order zip
    Lp = sorted(L, key=lambda d: (d['sec'], d['off']))
    Rp = sorted(R, key=lambda d: (d['sec'], d['off']))
    pairs = [(a['name'], Rp[i]['name'] if i < len(Rp) else None) for i, a in enumerate(Lp)]
    n, c = score(pairs, ls, rs, t)
    print(f'   H_pos  (symbol-table order zip) : {c}/{n} = {100*c/n:.1f}%')

    # H_rand: 200 random permutations
    accs = []
    rng = random.Random(20260802)
    for _ in range(200):
        perm = list(range(len(R)))
        rng.shuffle(perm)
        pairs = [(a['name'], R[perm[i]]['name'] if i < len(perm) else None)
                 for i, a in enumerate(L)]
        n, c = score(pairs, ls, rs, t)
        accs.append(100 * c / n)
    print(f'   H_rand (200 perms)              : mean {sum(accs)/len(accs):.1f}%  '
          f'max {max(accs):.1f}%  min {min(accs):.1f}%')
