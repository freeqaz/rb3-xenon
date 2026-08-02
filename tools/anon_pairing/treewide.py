"""Tree-wide: for every anonymous fn_<8hex> target row, does its reloc-masked
signature exist among the BASE object's funclet-like symbols, and if so what
KIND of base symbol supplies it?

Simplified predictor (exact-signature presence) covers objdiff passes 1/2/2b,
which are the only paths that yield a 100% score. Pass-3 fuzzy yields <100 and
is therefore irrelevant to the matched_functions count.

Validation: the predictor's agreement with report.json's 100/not-100 bucket is
printed, along with a random-offset-style null, so the number is not taken on
faith.
"""
import sys, json, collections, random
sys.path.insert(0, '/home/free/tmp/laneCX4')
import coffsig as C

WT = '/home/free/tmp/laneCX4/wt'


def kind_of(name):
    if name.startswith('__unwind$'):
        return 'base __unwind$ (EH funclet)'
    if name.startswith('__catch$'):
        return 'base __catch$ (EH funclet)'
    if name.startswith('__unwind__merged_'):
        return 'base __unwind__merged_'
    if name.startswith('??__E'):
        return 'base ??__E (dynamic init)'
    if name.startswith('??__F'):
        return 'base ??__F (dynamic dtor)'
    if name.startswith('fn_'):
        return 'base fn_ (anonymous!)'
    return 'other'


cfg = json.load(open(f'{WT}/objdiff.json'))
rep = json.load(open(f'{WT}/build/45410914/report.json'))
repu = {u['name']: u for u in rep['units']}
ucfg = {u['name']: u for u in cfg['units']}

partner_kind = collections.Counter()
partner_bytes = collections.Counter()
sizes_by_kind = collections.defaultdict(list)
agree = collections.Counter()
nullagree = collections.Counter()
units_done = units_skipped = 0
unit_sigs = []
unit_targets = __import__('collections').defaultdict(list)
err = collections.Counter()
rng = random.Random(20260802)

for uname, u in repu.items():
    uc = ucfg.get(uname)
    if not uc or not uc.get('target_path') or not uc.get('base_path'):
        units_skipped += 1
        continue
    try:
        tgt = C.parse(f"{WT}/{uc['target_path']}")
        bas = C.parse(f"{WT}/{uc['base_path']}")
    except Exception as e:
        err[type(e).__name__] += 1
        units_skipped += 1
        continue
    L = {d['name']: d for d in C.code_defs(tgt) if C.is_funclet_like(d['name']) and d['size'] > 0}
    R = [d for d in C.code_defs(bas) if C.is_funclet_like(d['name']) and d['size'] > 0]
    rsig = collections.defaultdict(list)
    for d in R:
        rsig[C.signature(bas, d)].append(d['name'])
    # NULL — random-offset: test this unit's target signatures against a DIFFERENT
    # unit's base funclet signature set. (An earlier null shuffled the value lists
    # while keeping the same KEYS, so `presence` was identical by construction and
    # the null scored exactly the same 99.08% -- vacuous. Fixed.)
    unit_sigs.append((uname, rsig))
    units_done += 1
    for f in u.get('functions', []):
        nm = f['name']
        if nm not in L:
            continue
        mpn = f.get('match_percent_normalized')
        mpn = mpn if mpn is not None else f.get('fuzzy_match_percent', 0.0)
        is100 = (mpn == 100.0)
        s = C.signature(tgt, L[nm])
        hit = rsig.get(s)
        agree[(bool(hit), is100)] += 1
        unit_targets[uname].append((s, is100))
        if hit and is100:
            k = kind_of(hit[0])
            partner_kind[k] += 1
            partner_bytes[k] += int(f['size'])
            sizes_by_kind[k].append(int(f['size']))

print(f'units measured {units_done}, skipped (no base/parse error) {units_skipped} {dict(err)}')
tot = sum(agree.values())
corr = agree[(True, True)] + agree[(False, False)]
print(f'\nPREDICTOR "exact signature present in base funclet set" vs report 100/not-100:')
print(f'   agreement {corr}/{tot} = {100*corr/tot:.2f}%')
print(f'   sig-hit & 100      {agree[(True, True)]}')
print(f'   sig-hit & NOT 100  {agree[(True, False)]}')
print(f'   no-hit  & 100      {agree[(False, True)]}   <-- would falsify the model')
print(f'   no-hit  & NOT 100  {agree[(False, False)]}')
# NULL: score each unit's target signatures against ANOTHER unit's base sig set
sigmap = dict(unit_sigs)
names = [n for n, _ in unit_sigs]
accs = []
for shift in (1, 7, 53, 211, 499):
    n = c = 0
    for i, un in enumerate(names):
        other = sigmap[names[(i + shift) % len(names)]]
        for s, is100 in unit_targets[un]:
            n += 1
            c += (bool(other.get(s)) == is100)
    accs.append(100 * c / n)
    print(f'   NULL (base sig set from unit +{shift:3d}): {c}/{n} = {100*c/n:.2f}%')
print(f'   NULL mean {sum(accs)/len(accs):.2f}%  vs model {100*corr/tot:.2f}%')

print('\nWHAT THE anon@100 ROWS ACTUALLY PAIR WITH:')
for k, v in partner_kind.most_common():
    print(f'   {v:7d}  {partner_bytes[k]:9d} B   {k}')
print(f'   {sum(partner_kind.values()):7d}  {sum(partner_bytes.values()):9d} B   TOTAL')
