#!/usr/bin/env python3
"""Lane CJ-4: whole-map DEFINEDNESS audit.

QUESTION: for every row (addr -> mangled name) in scripts/target_symbol_map.json,
does the obj objdiff will compare against actually DEFINE that name?  A row
naming a symbol nothing defines can never pair.

INSTRUMENT: COFF symbol table (coff.py).  NOT a substring scan -- an undefined
external *reference* carries identical name bytes to a definition.

HOME UNIT: determined EMPIRICALLY.  The renamer rewrites fn_<addr>/lbl_<addr> in
the dtk-split TARGET objs, so the unit whose target obj defines the mangled name
IS the home unit.  Cross-checked against splits.txt .text ranges.

RULER: objdiff pairs on get_normalized_symbol_name (norm.py), not raw bytes.
"""
import json, os, re, sys, collections, glob
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cj4_coff as coff, cj4_norm as norm

WT = sys.argv[1] if len(sys.argv) > 1 else '/home/free/tmp/laneCJ4/wt'
MAP = os.path.join(WT, 'scripts/target_symbol_map.json')


def load_units():
    cfg = json.load(open(os.path.join(WT, 'objdiff.json')))
    out = []
    for u in cfg['units']:
        out.append(dict(name=u['name'],
                        tgt=os.path.join(WT, u['target_path']) if u.get('target_path') else None,
                        base=os.path.join(WT, u['base_path']) if u.get('base_path') else None,
                        src=(u.get('metadata') or {}).get('source_path')))
    return out


def load_splits():
    """unit basename (no .cpp) -> list of (start,end) .text ranges."""
    rng = {}
    cur = None
    for line in open(os.path.join(WT, 'config/45410914/splits.txt')):
        m = re.match(r'^(\S.*):\s*$', line)
        if m:
            cur = m.group(1)
            if cur == 'Sections':
                cur = None
            continue
        if cur and '.text' in line:
            mm = re.search(r'start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)', line)
            if mm:
                rng.setdefault(cur, []).append((int(mm.group(1), 16), int(mm.group(2), 16)))
    return rng


def family(name):
    """Symbol family buckets.  NOTE the naming trap recorded in CLAUDE.md:
    @@$A@ / @@$DE@ / @@$CI@ / @@$3 DO NOT EXIST (they are displacement subfields
    inside $4PPPPPPPM@XX@).  The real adjustor families are $4, W (NO $), $2, $0.
    Order matters: test the vtable-thunk families BEFORE the ctor/dtor ones,
    because an adjustor thunk FOR a dtor carries both markers."""
    if name.startswith('??_E') or name.startswith('??_G'):
        # scalar/vector deleting destructor -- may itself be an adjustor form
        if '@@W' in name or '@@$4' in name or '@@$2' in name or '@@$0' in name:
            return 'dtor-adjustor'
        return '??_E/??_G dtor'
    if '@@$4' in name:
        return '$4 vbase-adjustor'
    if re.search(r'@@W[0-9]', name):
        return 'W adjustor'
    if '@@$2' in name or '@@$0' in name:
        return '$2/$0 adjustor'
    if name.startswith('??_7'):
        return 'vtable ??_7'
    if name.startswith('??_R'):
        return 'RTTI ??_R'
    if name.startswith('??0') or name.startswith('??1'):
        return 'ctor/dtor ??0/??1'
    if name.startswith('??__E') or name.startswith('??__F'):
        return 'dyninit ??__E/??__F'
    if name.startswith('?'):
        return 'ordinary ?'
    return 'other'


def main():
    m = json.load(open(MAP))
    rows = {k: v for k, v in m.items() if k.lower().startswith('0x') and isinstance(v, str)}
    print(f'map rows (0x keys, string values): {len(rows)}   (raw keys {len(m)})')

    units = load_units()
    print(f'objdiff units: {len(units)}')

    # ---- index every LIVE target obj and base obj by symbol state ----
    tgt_def = {}     # normkey -> [unit]
    base_state = {}  # unit -> {normkey: state}
    base_missing, tgt_missing = 0, 0
    for u in units:
        if u['tgt'] and os.path.exists(u['tgt']):
            try:
                c = coff.classify(open(u['tgt'], 'rb').read())
            except Exception:
                c = {}
            for n, st in c.items():
                if st == 'DEFINED':
                    tgt_def.setdefault(norm.key(n), []).append(u['name'])
        else:
            tgt_missing += 1
        if u['base'] and os.path.exists(u['base']):
            try:
                base_state[u['name']] = {norm.key(n): st
                                         for n, st in coff.classify(open(u['base'], 'rb').read()).items()}
            except Exception:
                base_state[u['name']] = {}
        else:
            base_missing += 1
    print(f'live target objs missing: {tgt_missing}   base (compiled) objs missing: {base_missing}')

    # global: where is each name DEFINED across all compiled base objs?
    base_def_anywhere = collections.defaultdict(list)
    for un, d in base_state.items():
        for n, st in d.items():
            if st == 'DEFINED':
                base_def_anywhere[n].append(un)

    splits = load_splits()
    span = []
    for unit, rs in splits.items():
        for a, b in rs:
            span.append((a, b, unit))
    span.sort()

    def unit_by_addr(a):
        import bisect
        i = bisect.bisect_right(span, (a, float('inf'), '')) - 1
        if i >= 0 and span[i][0] <= a < span[i][1]:
            return span[i][2]
        return None

    cls = collections.Counter()
    fam_cls = collections.defaultdict(collections.Counter)
    detail = collections.defaultdict(list)
    homedisagree = 0

    for addr, name in rows.items():
        a = int(addr, 16)
        nk = norm.key(name)
        homes = tgt_def.get(nk, [])
        su = unit_by_addr(a)
        # NOTE: objdiff unit names are 'default/<path-without-.cpp>'; splits.txt
        # keys are '<path>.cpp'.  Comparing basename-vs-full-path here silently
        # "disagreed" on EVERY subdirectory unit (4,987 phantom flags) until the
        # prefix was stripped properly.
        su_unit = ('default/' + su[:-4]) if (su and su.endswith('.cpp')) else (
            ('default/' + su) if su else None)
        # empirical home wins; splits used as cross-check / fallback
        if len(homes) == 1:
            home = homes[0]
        elif su_unit is not None:
            cand = [h for h in homes if h == su_unit]
            home = cand[0] if cand else (su_unit if not homes else homes[0])
        else:
            home = homes[0] if homes else None
        if su_unit is not None and homes and su_unit not in homes:
            homedisagree += 1

        fam = family(name)
        if home is None:
            c = 'A_unpinned_no_target'
        elif home not in base_state:
            # dtk auto-carves EVERY unsplit region into 'auto_<n>_<addr>_<sect>'
            # units.  A row homed there is UNPINNED address space -- there is no
            # source TU to compare, so it is inert, NOT a porting backlog item.
            c = ('A_unpinned_no_target' if '/auto_' in home
                 else 'B_home_not_compiled')
        else:
            st = base_state[home].get(nk)
            if st == 'DEFINED':
                c = 'C_ok_defined'
            elif st == 'WEAK':
                c = 'D_weak_external'
            elif st in ('UNDEF', 'COMMON'):
                c = 'E_undefined_ref'
            else:
                c = ('F_absent_but_defined_elsewhere'
                     if base_def_anywhere.get(nk) else 'G_absent_everywhere')
        cls[c] += 1
        fam_cls[fam][c] += 1
        if c in ('D_weak_external', 'E_undefined_ref', 'F_absent_but_defined_elsewhere'):
            detail[c].append((addr, name, home, base_def_anywhere.get(nk, [])[:3]))

    tot = len(rows)
    print(f'\n=== DEFINEDNESS CENSUS  (denominator {tot} map rows) ===')
    for k in sorted(cls):
        print(f'  {k:34s} {cls[k]:6d}   {100*cls[k]/tot:6.2f}%')
    print(f'\nempirical-home vs splits-home disagreements: {homedisagree}')

    print(f'\n=== BY SYMBOL FAMILY (denominator per family) ===')
    keys = sorted(cls)
    hdr = 'family'.ljust(22) + 'N'.rjust(7) + ''.join(k[0].rjust(8) for k in keys)
    print(hdr)
    for f in sorted(fam_cls, key=lambda x: -sum(fam_cls[x].values())):
        n = sum(fam_cls[f].values())
        print(f.ljust(22) + str(n).rjust(7) + ''.join(str(fam_cls[f][k]).rjust(8) for k in keys))
    print('legend: ' + ' | '.join(f'{k[0]}={k}' for k in keys))

    json.dump({k: v for k, v in detail.items()},
              open(os.environ.get('CJ4_DETAIL','/home/free/tmp/laneCJ4/detail.json'), 'w'), indent=1)
    print('\ndetail -> /home/free/tmp/laneCJ4/detail.json')
    for c in ('D_weak_external', 'E_undefined_ref'):
        print(f'\n--- {c} (showing up to 25 of {len(detail[c])}) ---')
        for r in detail[c][:25]:
            print(' ', r[0], r[1][:88], '|home', r[2], '|def@', r[3])


if __name__ == '__main__':
    main()
