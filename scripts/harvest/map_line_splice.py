#!/usr/bin/env python3
"""laneAV lead -- apply VA->name repairs to target_symbol_map.json by TEXTUAL
LINE SPLICE.  Never re-serialises: exactly one line changes per repair, so the
diff stays mergeable against concurrent map lanes.

An ENTRY line is  "0xVA": "name",  -- it has a COLON after the closing quote.
The map also holds provenance ARRAYS (_denylist / _icf_arbitrary /
_bijection_arbitrary) whose elements are bare `    "0xVA",` strings; a naive
startswith('"0x') test matches those too and corrupts the JSON.  Require the
colon (laneAR's fix, reproduced here).
"""
import json, re, sys

ENTRY = re.compile(r'^"0[xX][0-9a-fA-F]+"\s*:')

def main(mappath, planpath):
    plan = json.load(open(planpath))          # {"0xva": "newname", ...}
    plan = {k.lower(): v for k, v in plan.items()}
    lines = open(mappath).read().split('\n')
    out, done = [], set()
    for ln in lines:
        s = ln.strip()
        if ENTRY.match(s):
            key = s.split('"')[1]
            if key.lower() in plan:
                indent = ln[:len(ln) - len(ln.lstrip())]
                out.append('%s"%s": %s,' % (indent, key, json.dumps(plan[key.lower()])))
                done.add(key.lower())
                continue
        out.append(ln)
    missing = set(plan) - done
    assert not missing, 'plan VAs not present in map: %s' % sorted(missing)[:5]
    txt = '\n'.join(out)
    m = json.loads(txt)                        # must still parse
    # invariants
    vas = [l.strip().split('"')[1].lower() for l in out if ENTRY.match(l.strip())]
    assert len(vas) == len(set(vas)), 'duplicate VAs introduced'
    for arr in ('_bijection_arbitrary', '_icf_arbitrary', '_denylist'):
        assert isinstance(m[arr], list) and all(isinstance(x, str) for x in m[arr]), \
            '%s corrupted' % arr
    open(mappath, 'w').write(txt)
    print('applied %d repairs; entries=%d dupVA=0 arrays ok'
          % (len(done), len(vas)))

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
