#!/usr/bin/env python3
"""Lane EC-3: stratified defect-signature census over ALL named charged rows.

Stratifies by report.json fuzzy (authoritative ruler) and classifies with
objdiff-cli diff -c functionRelocDiffs=none (calibrated to equal report.json).
"""
import json, os, re, subprocess, sys, collections, concurrent.futures as cf

WT = "/home/free/tmp/laneEC3/wt"
CLI = "/home/free/code/milohax/rb3-xenon/bin/objdiff-cli"
OUT = "/home/free/tmp/laneEC3/ec3_census_v2.jsonl"
ANON = re.compile(r'^(fn_8[0-9A-Fa-f]{7}|sub_|lbl_)')

FLOAT_OP = re.compile(r'^(f[a-z]|lf[sd]|stf[sd])')
VMX_OP = re.compile(r'^(v[a-z]|lvx|stvx|lvl|lvr)')


def strat(f):
    return '>=99' if f >= 99 else '90-99' if f >= 90 else '70-90' if f >= 70 \
        else '40-70' if f >= 40 else '<40'


def frame():
    d = json.load(open(f"{WT}/build/45410914/report.json"))
    rows = []
    for u in d['units']:
        md = u.get('metadata') or {}
        if md.get('auto_generated') or not md.get('source_path'):
            continue
        for f in u.get('functions') or []:
            if 'fuzzy_match_percent' not in f:
                continue
            fz = f['fuzzy_match_percent']
            if fz >= 100:
                continue
            n = f['name']
            if ANON.match(n) or '__unwind$' in n or n.startswith('$'):
                continue
            rows.append(dict(unit=u['name'], sym=n, size=int(f['size']),
                             fuzzy=fz, mpn=f['match_percent_normalized'],
                             stratum=strat(fz),
                             src=md.get('source_path'),
                             cat=(md.get('progress_categories') or ['?'])[0]))
    return rows


def run(row):
    tmp = f"/home/free/tmp/laneEC3/j/{abs(hash(row['unit']+row['sym']))}.json"
    cmd = [CLI, "diff", "-p", WT, "-u", row['unit'], row['sym'],
           "--include-instructions", "-f", "json",
           "-c", "functionRelocDiffs=none", "-o", tmp]
    try:
        r = subprocess.run(cmd, cwd=WT, capture_output=True, text=True, timeout=120)
        if r.returncode != 0 or not os.path.exists(tmp):
            row['err'] = (r.stderr or r.stdout or 'fail')[:200]
            return row
        d = json.load(open(tmp))
    except Exception as e:
        row['err'] = str(e)[:200]
        return row
    finally:
        pass

    ins = d['instructions']
    row['cli_fuzzy'] = d['fuzzy_match_percent']
    row['tsz'] = d['target_size']
    row['bsz'] = d['base_size']
    mt = collections.Counter(i['match_type'] for i in ins)
    row['mt'] = dict(mt)

    topc, bopc = collections.Counter(), collections.Counter()
    tfloat = bfloat = tvmx = bvmx = tbl = bbl = 0
    argtypes = collections.Counter()
    offdeltas = collections.Counter()
    regswap = 0
    immctx = collections.Counter(); immbase = collections.Counter()
    for i in ins:
        t, b = i.get('target'), i.get('base')
        if t:
            o = t['opcode']; topc[o] += 1
            if FLOAT_OP.match(o): tfloat += 1
            if VMX_OP.match(o): tvmx += 1
            if o == 'bl': tbl += 1
        if b:
            o = b['opcode']; bopc[o] += 1
            if FLOAT_OP.match(o): bfloat += 1
            if VMX_OP.match(o): bvmx += 1
            if o == 'bl': bbl += 1
        if i['match_type'] == 'diff_arg':
            bd = (i.get('diff_breakdown') or {}).get('arguments') or []
            kinds = set()
            for a in bd:
                at = a.get('arg_type')
                argtypes[at] += 1
                kinds.add(at)
                if at in ('signed', 'unsigned'):
                    try:
                        dv = int(a['target']['value']) - int(a['base']['value'])
                        offdeltas[dv] += 1
                    except Exception:
                        pass
            if kinds and kinds <= {'register'}:
                regswap += 1
            if 'immediate' in kinds:
                ta = (t or {}).get('typed_args') or []
                regs = [a['value'] for a in ta if a.get('type') == 'Register']
                basereg = regs[-1] if regs else '?'
                op = (t or {}).get('opcode', '?')
                immctx[('r1' if basereg == 'r1' else 'obj', op[:4])] += 1
                immbase[basereg] += 1
    row['topc_n'] = sum(topc.values()); row['bopc_n'] = sum(bopc.values())
    inter = sum((topc & bopc).values()); union = sum((topc | bopc).values())
    row['opc_jaccard'] = round(inter / union, 4) if union else 1.0
    row['tfloat'] = tfloat; row['bfloat'] = bfloat
    row['tvmx'] = tvmx; row['bvmx'] = bvmx
    row['tbl'] = tbl; row['bbl'] = bbl
    row['argtypes'] = dict(argtypes)
    row['regswap_rows'] = regswap
    row['immctx'] = {f'{a}|{b}': v for (a, b), v in immctx.items()}
    row['immbase'] = dict(immbase)
    row['offdeltas'] = dict(sorted(offdeltas.items(), key=lambda x: -x[1])[:6])
    try:
        os.remove(tmp)
    except Exception:
        pass
    return row


def classify(r):
    """Signature classes. Each names WHAT WAS MEASURED (EB-3 discipline)."""
    if 'err' in r:
        return 'ERROR'
    mt = r.get('mt', {})
    nins = mt.get('insert', 0); ndel = mt.get('delete', 0)
    narg = mt.get('diff_arg', 0); nrep = mt.get('replace', 0)
    neq = mt.get('equal', 0)
    tot = sum(mt.values()) or 1
    at = r.get('argtypes', {})
    real_arg = sum(v for k, v in at.items() if k != 'symbol')
    tsz, bsz = r.get('tsz', 0) or 1, r.get('bsz', 0) or 1
    ratio = max(tsz, bsz) / max(1, min(tsz, bsz))

    # (1) FOREIGN evidence: asymmetries a source bug in the RIGHT function
    #     cannot produce. Float/VMX class presence is ICF-immune.
    if (r['tfloat'] >= 3 and r['bfloat'] == 0) or (r['bfloat'] >= 3 and r['tfloat'] == 0):
        return 'FOREIGN_FPCLASS'
    if (r['tvmx'] >= 3 and r['bvmx'] == 0) or (r['bvmx'] >= 3 and r['tvmx'] == 0):
        return 'FOREIGN_VMXCLASS'
    if r['opc_jaccard'] < 0.35 and ratio > 1.5:
        return 'FOREIGN_SHAPE'

    # (2) pure codegen: only register substitutions, nothing added/removed
    if nins == 0 and ndel == 0 and nrep == 0 and real_arg == 0 and narg > 0:
        return 'CODEGEN_REGALLOC'
    if nins == 0 and ndel == 0 and nrep == 0 and narg == 0:
        return 'CLEAN?'

    # (3) structural: instructions present on one side only
    if (nins + ndel) > 0 and r['tbl'] != r['bbl']:
        return 'SOURCE_CALLCOUNT'
    if (nins + ndel) > 0:
        return 'SOURCE_INSDEL'

    # (4) same shape, differing numeric args = offsets / immediates
    if real_arg > 0:
        return 'SOURCE_ARGVAL'
    return 'MIXED'


def main():
    os.makedirs("/home/free/tmp/laneEC3/j", exist_ok=True)
    rows = frame()
    print(f"frame: {len(rows)} named charged rows", file=sys.stderr)
    done = []
    with cf.ThreadPoolExecutor(max_workers=8) as ex:
        for n, r in enumerate(ex.map(run, rows)):
            r['sig'] = classify(r)
            done.append(r)
            if n % 200 == 0:
                print(f"  {n}/{len(rows)}", file=sys.stderr)
    with open(OUT, 'w') as fh:
        for r in done:
            fh.write(json.dumps(r) + "\n")
    print(f"wrote {OUT}", file=sys.stderr)


if __name__ == '__main__':
    main()
