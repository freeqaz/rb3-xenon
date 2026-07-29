from _paths import SCRATCH, REPO, BANDEXE, WII_SRC  # noqa: E402
import os, json, random, string
import str_locate as locate

WII = locate.WII

def report(name, lits):
    sel, fnhits = locate.locate(lits)
    cl = locate.cluster(fnhits)
    top = cl[0] if cl else None
    span = '-' if not top else '[%08X,%08X)' % (top['lo'], top['hi'])
    print('%-44s nlits=%4d selective_hits=%3d clusters=%3d top_corr=%d top=%s' % (
        name, len(lits), len(sel), len(cl), len(top['lits']) if top else 0, span))

# 1. Wii-only render backend + OS
for d in ['rndwii', 'os']:
    for sub in ['']:
        pass
cands = {
  'system/rndwii/*': os.path.join(WII,'system/rndwii'), 'system/synthwii/*': os.path.join(WII,'system/synthwii'), 'system/usbwii/*': os.path.join(WII,'system/usbwii'),
}
for nm, d in cands.items():
    if not os.path.isdir(d):
        print(f'{nm}: MISSING'); continue
    files = [os.path.join(d, f) for f in os.listdir(d) if f.endswith(('.cpp', '.h'))]
    report(nm, locate.lits_of(files))

# 2. synthetic literals: random ASCII words, must yield nothing
rnd = set()
random.seed(7)
for i in range(300):
    rnd.add(''.join(random.choice(string.ascii_lowercase) for _ in range(9)))
report('SYNTHETIC random 9-char words', rnd)

# 3. plausible-but-absent: real English words never in a Milo TU
report('SYNTHETIC plausible words', {'quarterback','pomegranate','defenestrate','trombonist','xylophone','marsupial','tessellate','hovercraft'})

# 4. shuffled-literal control: take a real located unit's literals, shuffle characters
import str_control as control
r = control.run('BandWardrobe.cpp')
sh = set()
for L in list(r['fnhits'] and locate.lits_of([r['src']]))[:200]:
    l = list(L); random.shuffle(l); sh.add(''.join(l))
report('SHUFFLED BandWardrobe literals', sh)
