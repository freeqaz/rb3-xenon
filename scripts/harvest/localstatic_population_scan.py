#!/usr/bin/env python3
"""Local-static Symbol/Message population scan (laneAT, 2026-07-26).

★ THE FINDING (laneAT-f3, measured): retail RB3 declares each property `Symbol`
and each `Handle()` `Message` as a FUNCTION-LOCAL `static`, where our source
references the corresponding global from Symbols*.h / Messages*.h. Converting
13 such functions took them to 100%, several climbing from ~45-55%.

★ AND the 0x10/0x20 stack-frame deltas this lane spent a day chasing are a
SYMPTOM of it, not an independent defect: each function-local static adds
register pressure, which costs 1-2 extra callee-saves, which is exactly the
observed frame delta. "Resize the frame" was the wrong framing; "add the local
static" fixes the frame AND the body.

★ THE LAW (measured precedent in this repo): convert a TU's local-static form
ALL AT ONCE. Converting a single function measured -7; converting the whole TU
measured +177. Guard words, ??__E/??__F dynamic-init and atexit thunks are
emitted and ordered per-TU, so a partial conversion leaves a mixture that pairs
worse than either pure form. Hence this tool ranks by TU, not by function.

THE TELL, computable with no build: our target obj calls Symbol's const-char*
ctor / Message's Symbol ctor / atexit MORE times than our compiled obj does.
Those extra calls are the local statics' one-time initialisation.

Relocation targets are resolved through scripts/target_symbol_map.json: the
renamer only renames symbols DEFINED in an obj, so an external call appears as
fn_<VA> on the target side and must be matched by the callee's mapped VA.

★ Indexes relocations by Sym.index, the true COFF SymbolTableIndex -- NOT by
list position, because coffx.read_coff skips aux records (i += 1 + naux). That
bug silently named a random symbol for every relocation and invalidated an
entire column of an earlier tool in this lane.

Requires a FULL build in the worktree first.

Usage: python3 scripts/harvest/localstatic_population_scan.py <worktree> [--json out]
"""
import argparse, collections, glob, json, os, struct, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'analysis'))
from coffx import read_coff, infer_sizes, K_SEC

# mangled name -> what an extra target-side call means
TELLS = {
    '??0Symbol@@QAA@PBD@Z': 'Symbol(const char*)',
    '??0Message@@QAA@VSymbol@@@Z': 'Message(Symbol)',
    'atexit': 'atexit',
}


def has_guard_block(code, off, size):
    """★ The function-local-static GUARD-WORD test block, in the TARGET code:
        lwz  rX, guard ; rlwinm./andi. ; ori ; stw rX, guard
    MSVC emits exactly this once per function-local static (one bit of a shared
    guard word per static). Its PRESENCE is what separates a real local-static
    site from a call-count coincidence.

    laneAT-f3 measured the false positive this exists to kill:
    NextSongPanel::FinishLoad showed +4 Symbol ctor calls but adding the
    statics REGRESSED it 52.1 -> 40.6, because the extra calls came from a
    genuinely different body (Find<RndAnimatable> calls we lack), not from
    local statics. The call-count tell alone is NOT sufficient evidence.
    """
    n = size // 4
    if n < 4:
        return False
    w = [struct.unpack_from('>I', code, off + 4 * i)[0] for i in range(n)]
    def op(x):
        return x >> 26
    for i in range(n - 3):
        if op(w[i]) != 32:                       # lwz
            continue
        j = i + 1
        # rlwinm. (op21, Rc=1) or andi. (op28) within a few instructions
        while j < min(i + 5, n):
            if (op(w[j]) == 21 and (w[j] & 1)) or op(w[j]) == 28:
                break
            j += 1
        else:
            continue
        rest = w[j:min(j + 10, n)]
        if any(op(x) == 24 for x in rest) and any(op(x) == 36 for x in rest):
            return True
    return False


def counts_by_func(path, want_names, name_of):
    """-> {func_name: Counter(tell -> n)} for Code symbols in one obj."""
    try:
        data = open(path, 'rb').read()
    except OSError:
        return None
    secs, syms = read_coff(data)
    if secs is None:
        return None
    infer_sizes(secs, syms)
    by_idx = {s.index: s for s in syms}
    out = {}
    for s in syms:
        if s.sec <= 0 or s.size == 0 or s.kind == K_SEC or s.cls not in (2, 3):
            continue
        sec = secs[s.sec - 1]
        if not sec.is_code:
            continue
        c = collections.Counter()
        for (va, si, typ) in sec.relocs:
            if not (s.value <= va < s.value + s.size):
                continue
            t = by_idx.get(si)
            if t is None:
                continue
            tell = name_of(t.name)
            if tell in want_names:
                c[tell] += 1
        out.setdefault(s.name, (c, has_guard_block(sec.data, s.value, s.size)))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('worktree')
    ap.add_argument('--json')
    a = ap.parse_args()
    wt = a.worktree
    tmap = {k.lower(): v for k, v in
            json.load(open(os.path.join(wt, 'scripts/target_symbol_map.json'))).items()
            if isinstance(v, str) and k.startswith('0x')}
    rev = {}
    for k, v in tmap.items():
        rev.setdefault(v, k)
    # fn_<VA> of each tell, for the target side
    va_of = {rev[n]: TELLS[n] for n in TELLS if n in rev}

    def tgt_name_of(nm):
        if nm.startswith('fn_'):
            return va_of.get('0x' + nm[3:].lower())
        return TELLS.get(nm)

    def base_name_of(nm):
        return TELLS.get(nm) or TELLS.get(nm.lstrip('_'))

    rep = json.load(open(os.path.join(wt, 'build/45410914/report.json')))
    pct = {}
    for u in rep['units']:
        for f in (u.get('functions') or []):
            pct[(u['name'].split('/', 1)[-1], f['name'])] = f['match_percent_normalized']

    base_by_name = collections.defaultdict(list)
    for p in glob.glob(os.path.join(wt, 'build/45410914/src/**/*.obj'), recursive=True):
        base_by_name[os.path.basename(p)].append(p)

    want = set(TELLS.values())
    per_tu, rows = collections.Counter(), []
    unconfirmed = collections.Counter()
    tu_fns = collections.Counter()
    root = os.path.join(wt, 'build/45410914/obj')
    for tp in sorted(glob.glob(os.path.join(root, '**', '*.obj'), recursive=True)):
        rel = os.path.relpath(tp, root)
        bp = os.path.join(wt, 'build/45410914/src', rel)
        if not os.path.exists(bp):
            c = base_by_name.get(os.path.basename(tp))
            if not c or len(c) != 1:
                continue
            bp = c[0]
        tc = counts_by_func(tp, want, tgt_name_of)
        bc = counts_by_func(bp, want, base_name_of)
        if not tc or not bc:
            continue
        key = rel[:-4]
        for nm, (ct, guard) in tc.items():
            if nm.startswith('fn_') or nm.startswith('__') or '$' in nm[:2]:
                continue
            p = pct.get((key, nm))
            if p is None or p >= 100.0:
                continue
            b = bc.get(nm)
            if b is None:
                continue
            cb = b[0]
            excess = {k: ct[k] - cb.get(k, 0) for k in want if ct[k] - cb.get(k, 0) > 0}
            if not excess:
                continue
            n = sum(excess.values())
            if guard:
                per_tu[rel] += n
                tu_fns[rel] += 1
            else:
                unconfirmed[rel] += n
            rows.append({'unit': rel, 'sym': nm, 'pct': p, 'excess': excess,
                         'n': n, 'guard_block': guard})
    conf = [r for r in rows if r['guard_block']]
    unc = [r for r in rows if not r['guard_block']]
    print('functions with a target-only local-static call signature: %d' % len(rows))
    print('  ★ GUARD-BLOCK CONFIRMED (actionable): %d across %d TUs, %d excess calls'
          % (len(conf), len(per_tu), sum(per_tu.values())))
    print('  unconfirmed -- call-count only, DO NOT EDIT on this alone: %d (%d calls)'
          % (len(unc), sum(unconfirmed.values())))
    print('\nranked TUs (convert each ALL AT ONCE -- see the law above):')
    for u, n in per_tu.most_common(30):
        print('  %4d excess calls  %3d fns  %s' % (n, tu_fns[u], u))
    if a.json:
        json.dump({'rows': rows,
                   'per_tu': [{'unit': u, 'excess': n, 'fns': tu_fns[u]}
                              for u, n in per_tu.most_common()]},
                  open(a.json, 'w'), indent=1)


if __name__ == '__main__':
    main()
