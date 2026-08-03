#!/usr/bin/env python3
"""Lane ED-2: frame-pointer-aware base-register classifier for the
uniform-immediate-delta class.

EC-3's rule was "check the base register: r1 is the stack". That is NECESSARY
but NOT SUFFICIENT. MSVC X360 frequently establishes a SECOND frame pointer
(`subi r31, r1, N` right before `stwu r1, -N, r1`, restored with
`addi r1, r31, N`), so a `this`-looking r31 offset can be a pure stack slot.
CharIKHand::Load (980 B, the largest row in the class) is exactly this.

So: do a tiny forward dataflow over the TARGET prologue to find every register
that is frame-derived and every register that is `this`-derived, then label each
differing immediate by which family its BASE register belongs to.
"""
import json, os, re, subprocess, sys, collections
import concurrent.futures as cf

WT = os.environ.get("ED2_WT", "/home/free/tmp/laneED2/wt")
CLI = "/home/free/code/milohax/rb3-xenon/bin/objdiff-cli"
JD = "/home/free/tmp/laneED2/j2"

MOVE = {'mr', 'addi', 'subi', 'ori', 'add', 'li'}


def args(x):
    return [a.get('value') for a in (x.get('typed_args') or [])]


def regs_of(x):
    return [a['value'] for a in (x.get('typed_args') or [])
            if a.get('type') == 'Register']


def analyze_prologue(ins, n=18):
    """Return (frame_regs, this_regs) from the target stream prologue."""
    frame = {'r1'}
    this = {'r3'}
    for i in ins[:n]:
        t = i.get('target')
        if not t:
            continue
        op = t.get('opcode', '')
        if op == 'bl':
            # __savegprlr_* / __savefpr_* are register-save helpers that do NOT
            # clobber r3. Any OTHER call does, so stop trusting r3 as `this`.
            tgt = ' '.join(str(v) for v in args(t))
            if '__savegpr' in tgt or '__savefpr' in tgt or '__savevmx' in tgt:
                continue
            this.discard('r3')
            continue
        r = regs_of(t)
        if op in MOVE and len(r) >= 2:
            dst, src = r[0], r[1]
            if src in frame:
                frame.add(dst)
            if src in this:
                this.add(dst)
    frame.discard('r1') if False else None
    return frame, this


def label(basereg, frame, this):
    if basereg == 'r1':
        return 'STACK_r1'
    if basereg in frame:
        return 'STACK_frameptr'
    if basereg in this:
        return 'OBJECT_this'
    return 'OTHER_ptr'


def run(row):
    tmp = f"{JD}/{abs(hash(row['unit'] + row['sym']))}.json"
    cmd = [CLI, "diff", "-p", WT, "-u", row['unit'], row['sym'],
           "--include-instructions", "-f", "json",
           "-c", "functionRelocDiffs=none", "-o", tmp]
    r = subprocess.run(cmd, cwd=WT, capture_output=True, text=True, timeout=180)
    if r.returncode != 0 or not os.path.exists(tmp):
        row['err'] = (r.stderr or r.stdout or 'fail')[:200]
        return row
    d = json.load(open(tmp))
    ins = d['instructions']
    frame, this = analyze_prologue(ins)
    row['frame_regs'] = sorted(frame)
    row['this_regs'] = sorted(this)

    labels = collections.Counter()
    detail = []
    for i in ins:
        if i['match_type'] != 'diff_arg':
            continue
        bd = (i.get('diff_breakdown') or {}).get('arguments') or []
        t = i.get('target') or {}
        for a in bd:
            if a.get('arg_type') != 'immediate':
                continue
            rr = regs_of(t)
            base = rr[-1] if rr else '?'
            lb = label(base, frame, this)
            labels[lb] += 1
            detail.append(dict(op=t.get('opcode'), base=base, label=lb,
                               tgt=a['target']['value'], b=a['base']['value']))
    row['labels'] = dict(labels)
    row['detail'] = detail
    os.remove(tmp)
    return row


def main():
    os.makedirs(JD, exist_ok=True)
    pure = json.load(open("/home/free/tmp/laneED2/ed2_pure47.json"))
    out = []
    with cf.ThreadPoolExecutor(max_workers=8) as ex:
        for r in ex.map(run, pure):
            out.append(r)
    json.dump(out, open("/home/free/tmp/laneED2/ed2_classified.json", "w"), indent=1)
    agg = collections.Counter()
    for r in out:
        ks = set(r.get('labels', {}))
        agg['ERR' if 'err' in r else ('|'.join(sorted(ks)) or 'none')] += 1
    for k, v in agg.most_common():
        print(f"{v:>3}  {k}")


if __name__ == '__main__':
    main()
