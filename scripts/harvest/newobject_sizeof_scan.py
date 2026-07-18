#!/usr/bin/env python3
"""
NewObject-sizeof oracle scanner.

A class's static factory ?NewObject@Cls@@SAPAVObject@Hmx@@XZ (and pool
operator-new wrappers) embeds retail's true sizeof(Cls) as an `li r3, <imm>`
immediate feeding the allocator call (operator new / PoolAlloc / MemAlloc).
Comparing retail's immediate (objdiff TARGET side) against OUR compiled
immediate (objdiff BASE side) exposes struct-size drift with zero ambiguity.

For every NewObject-shaped symbol we can pair (named in
scripts/target_symbol_map.json AND whose unit compiles), run objdiff-cli,
extract the size immediate from each side, and emit a mismatch table.

Usage:
  scripts/harvest/newobject_sizeof_scan.py [--project DIR] [--json OUT] [--md OUT]
"""
import argparse, json, os, re, subprocess, sys, hashlib

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# demangled class name from ?NewObject@Cls@@... or nested ?NewObject@Cls@Ns@@...
NEWOBJ_RE = re.compile(r'^\?NewObject@(.+?)@@SAPAVObject@Hmx@@XZ$')

# allocator callee names (base side, after our compile) that consume the size in r3
ALLOC_CALLEES = ("??2@YAPAXI@Z", "PoolAlloc", "MemAlloc", "MemOrPoolAlloc",
                 "?Alloc@", "operator new")


def load_symbols(map_path):
    with open(map_path) as f:
        m = json.load(f)
    out = {}  # class_name -> mangled symbol
    for addr, sym in m.items():
        cands = sym if isinstance(sym, list) else [sym]
        for s in cands:
            if not isinstance(s, str):
                continue
            mo = NEWOBJ_RE.match(s)
            if mo:
                out[mo.group(1)] = s
    return out


def run_objdiff(symbol, project_dir):
    objdiff_bin = os.path.join(REPO, "bin", "objdiff-cli")
    h = hashlib.md5(symbol.encode()).hexdigest()[:10]
    out = f"/tmp/claude/nobj_{h}.json"
    os.makedirs("/tmp/claude", exist_ok=True)
    cmd = [objdiff_bin, "diff", "-p", project_dir, symbol,
           "--include-instructions", "--build", "--incremental",
           "-f", "json", "-o", out]
    r = subprocess.run(cmd, cwd=project_dir, capture_output=True, text=True)
    if r.returncode != 0:
        return None, (r.stderr or r.stdout or "").strip()[:200]
    try:
        with open(out) as f:
            return json.load(f), None
    except Exception as e:
        return None, str(e)


def side_seq(instrs, side):
    """Ordered (opcode, args) list for one side, skipping rows absent on that side."""
    seq = []
    for row in instrs:
        s = row.get(side)
        if s:
            seq.append((s.get("opcode", ""), s.get("args", "")))
    return seq


def _li_r3(args):
    parts = [p.strip() for p in args.split(",")]
    if parts and parts[0] == "r3" and len(parts) >= 2:
        try:
            return int(parts[1], 0)
        except ValueError:
            return None
    return None


def size_before_alloc(seq):
    """Immediate of the `li r3,<imm>` feeding the allocator call.

    The allocator `bl` is the one whose result r3 is immediately stored /
    null-checked (`stw r3,...` or `cmplwi r3, 0x0` within the next 2 insts).
    Handles both the direct `li r3,size; bl new` shape and the
    `bl StaticClassName; ...; li r3,size; bl MemAlloc` shape.
    """
    n = len(seq)
    for i, (op, args) in enumerate(seq):
        if op != "bl":
            continue
        # is this the allocator call? look ahead for the r3 capture idiom
        alloc = False
        for op2, a2 in seq[i + 1:i + 3]:
            if op2 == "stw" and a2.strip().startswith("r3,"):
                alloc = True; break
            if op2 == "cmplwi" and a2.strip().startswith("r3,"):
                alloc = True; break
        if not alloc:
            continue
        # walk back for the last li r3 feeding it
        for j in range(i - 1, -1, -1):
            if seq[j][0] == "li":
                v = _li_r3(seq[j][1])
                if v is not None:
                    return v
            if seq[j][0] == "bl":
                break
        return None
    # fallback: last li r3 before the first bl
    last = None
    for op, args in seq:
        if op == "li":
            v = _li_r3(args)
            if v is not None:
                last = v
        elif op == "bl":
            return last
    return last


def scan(project_dir, symbols):
    rows = []
    for cls, sym in sorted(symbols.items()):
        d, err = run_objdiff(sym, project_dir)
        if d is None:
            rows.append(dict(cls=cls, sym=sym, retail=None, ours=None,
                             delta=None, pct=None, err=err))
            continue
        instrs = d.get("instructions", [])
        retail = size_before_alloc(side_seq(instrs, "target"))
        ours = size_before_alloc(side_seq(instrs, "base"))
        pct = d.get("normalized_match_percent")
        delta = (retail - ours) if (retail is not None and ours is not None) else None
        rows.append(dict(cls=cls, sym=sym, retail=retail, ours=ours,
                         delta=delta, pct=pct, err=None))
        print(f"  {cls:34s} retail={retail} ours={ours} delta={delta} pct={pct}",
              file=sys.stderr)
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project", default=REPO)
    ap.add_argument("--map", default=os.path.join(REPO, "scripts", "target_symbol_map.json"))
    ap.add_argument("--json", default="/tmp/claude/newobject_sizeof.json")
    ap.add_argument("--md", default=None)
    args = ap.parse_args()

    syms = load_symbols(args.map)
    print(f"NewObject symbols in map: {len(syms)}", file=sys.stderr)
    rows = scan(args.project, syms)

    mism = [r for r in rows if r["delta"] not in (None, 0)]
    mism.sort(key=lambda r: abs(r["delta"]))

    with open(args.json, "w") as f:
        json.dump(dict(scanned=len(rows), mismatches=len(mism), rows=rows), f, indent=1)

    def fmt(v): return "-" if v is None else (hex(v) if isinstance(v, int) else str(v))
    lines = ["# NewObject-sizeof oracle scan", "",
             f"scanned={len(rows)}  mismatches={len(mism)}", "",
             "## Mismatches (|delta| ascending)", "",
             "| class | retail | ours | delta | factory_pct |",
             "|---|---|---|---|---|"]
    for r in mism:
        lines.append(f"| {r['cls']} | {fmt(r['retail'])} | {fmt(r['ours'])} | "
                     f"{r['delta']:+d} | {r['pct']} |")
    lines += ["", "## All scanned", "",
              "| class | retail | ours | delta | pct | err |",
              "|---|---|---|---|---|---|"]
    for r in sorted(rows, key=lambda x: x["cls"]):
        d = "-" if r["delta"] is None else f"{r['delta']:+d}"
        lines.append(f"| {r['cls']} | {fmt(r['retail'])} | {fmt(r['ours'])} | "
                     f"{d} | {r['pct']} | {r['err'] or ''} |")
    md = "\n".join(lines)
    if args.md:
        with open(args.md, "w") as f:
            f.write(md + "\n")
    print(md)


if __name__ == "__main__":
    main()
