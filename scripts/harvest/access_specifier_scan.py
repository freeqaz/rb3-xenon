#!/usr/bin/env python3
"""access_specifier_scan.py -- map rows whose ONLY defect is the access code.

THE DEFECT
----------
`scripts/target_symbol_map.json` holds a retail MSVC mangled name per VA.  A row
whose name no obj of ours DEFINES is structurally unclaimable: objdiff pairs
target<->base by name, so nothing can ever score there.  Most such rows are real
source gaps.  But a subclass of them differ from a name we DO emit by exactly one
character -- the function's **access-and-storage code**, the letter right after
the scope's closing `@@`:

    A/B private       E/F private virtual     I/J protected
    M/N protected virtual                     Q/R public
    U/V public virtual

That letter is a pure *declaration* attribute.  Public vs protected vs private
changes **no codegen at all**, and virtual vs non-virtual changes the codegen of
the CALLER, not of the function body -- so the body at the VA is byte-identical
either way, and the map is simply carrying the wrong declaration (typically
transferred from DC3's `ham_xbox_r.map`, whose engine is a later revision where
the method really did have a different access or virtualness).

Measured instances that motivated this scanner (lane laneAI, 2026-07-26):

    0x82813190  ?FinishLoad@UIPanel@@**U**AAXXZ -> **M**AAXXZ   flipped to 100%
    0x82812cb8  ?Load@UIPanel@@**U**AAXXZ       -> **M**AAXXZ   flipped to 100%
    0x828027c0  ?FocusComponent@UIPanel@@**U**AAPAV... -> **Q**AA...   0% -> 99.8%

★ WHY NO EXISTING TOOL SEES THESE.  `span_predictor.py` and `joint_unblock.py`
both start from `{name: units that define it}`.  A row whose name is emitted by
NO obj is dropped before classification, so it never reaches the PAYS /
WRONG-UNIT / UNPINNED buckets at all -- it is invisible to the whole joint
pipeline.  A `??_E` vs `??_G` row has the same shape (that one was found by
hand); this scanner is the systematic form.

★ AND WHY IT IS A JOINT-LANE TOOL.  Renaming alone is not enough when the VA is
also pinned to the wrong unit: `FocusComponent` needed the rename AND a splits
MOVE of 0x828027C0-0x828027E8 from UI.cpp to UIPanel.cpp.  Map-only read 0%,
splits-only read 0%.  Feed `--out-proposals` to `span_predictor.py`; anything
not PAYS is a splits job as well.

OUTPUT / SAFETY
---------------
A candidate is emitted only when the substituted name is defined by at least one
of our objs AND is not already a mapped value anywhere (a duplicate mangled name
is a guaranteed regression once the two VAs share a unit).  Rows already reading
strict-100 are never touched.  Apply with `map_edit_textual.py`.

Usage:
  access_specifier_scan.py --worktree WT [--out-proposals p.json] [--out-edits e.json]
"""
import argparse
import json
import os
import re
import sys
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from span_predictor import build_definer_index  # noqa: E402

# `@@` closing the scope, then the access/storage letter, then the member
# modifier (`A`=near/none, `B`=const, `C`=volatile, `D`=const volatile) and the
# calling convention (`A` = __cdecl on X360).  Static members (`SA...`) have no
# member modifier and are deliberately NOT rewritten: static-vs-instance DOES
# change codegen (no `this` in r3), so a mismatch there is a real mispair, not a
# declaration typo -- that is argreg_mispair_scan.py's job.
ACCESS_RE = re.compile(r'@@([AEIMQU])([ABCD]A)')
ACCESS_LETTERS = 'AEIMQU'
LABEL = {'A': 'private', 'E': 'private virtual', 'I': 'protected',
         'M': 'protected virtual', 'Q': 'public', 'U': 'public virtual'}
VA_RE = re.compile(r'^0[xX][0-9a-fA-F]+$')


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--worktree', required=True)
    ap.add_argument('--map')
    ap.add_argument('--report')
    ap.add_argument('--out-proposals', help='span_predictor.py --proposals input')
    ap.add_argument('--out-edits', help='map_edit_textual.py rename plan')
    a = ap.parse_args()

    wt = a.worktree
    mp = a.map or os.path.join(wt, 'scripts/target_symbol_map.json')
    rp = a.report or os.path.join(wt, 'build/45410914/report.json')

    raw = json.load(open(mp))
    m = {k: v for k, v in raw.items() if isinstance(v, str) and VA_RE.match(k)}
    mapped_values = set(m.values())
    definers = build_definer_index(wt)

    rep = json.load(open(rp))
    strict100 = {f['name'] for u in rep['units'] for f in u.get('functions', [])
                 if f.get('match_percent_normalized') == 100.0}

    stats = Counter()
    cands = []
    for va, name in sorted(m.items()):
        if name in definers:
            stats['emitted-already'] += 1
            continue
        if name in strict100:
            stats['skip-strict100'] += 1
            continue
        mm = ACCESS_RE.search(name)
        if not mm:
            stats['no-access-code'] += 1
            continue
        hits = []
        for L in ACCESS_LETTERS:
            if L == mm.group(1):
                continue
            alt = name[:mm.start(1)] + L + name[mm.end(1):]
            if alt in definers and alt not in mapped_values:
                hits.append((L, alt))
        if not hits:
            stats['unemitted-no-variant'] += 1
            continue
        if len(hits) > 1:
            # two different access spellings of the same signature both emitted:
            # cannot rank them from the name alone.  Refuse rather than guess.
            stats['refuse-ambiguous-variant'] += 1
            continue
        L, alt = hits[0]
        cands.append(dict(va=va.lower(), old=name, new=alt,
                          change='%s -> %s' % (LABEL[mm.group(1)], LABEL[L])))
        stats['CANDIDATE'] += 1

    print('access-specifier scan:', dict(sorted(stats.items(), key=lambda kv: -kv[1])))
    for c in cands:
        print('  %s  %-28s %s' % (c['va'], c['change'], c['new'][:100]))

    if a.out_proposals:
        prop = {}
        for c in cands:
            tu = sorted(definers[c['new']])[0]
            prop.setdefault(tu, []).append(dict(name=c['new'], va=c['va']))
        json.dump(prop, open(a.out_proposals, 'w'), indent=1)
        print('->', a.out_proposals)
    if a.out_edits:
        json.dump({'rename': {c['va']: [c['old'], c['new']] for c in cands}},
                  open(a.out_edits, 'w'), indent=1)
        print('->', a.out_edits)


if __name__ == '__main__':
    main()
