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

VIRTUAL-BASE / SHAPE-MISMATCH REJECTION (added 2026-07-18, wave-10):
  A class with virtual bases compiles its ctor with an extra "most-derived /
  fully-constructed" int flag, and the factory passes it: the ctor `bl` is
  preceded by `li r4,<const>` (typ. 0x1), i.e. `mr r3,obj; li r4,1; bl ctor`.
  Detection uses the ONLY signal visible inside the factory body (this scanner
  never diffs the ctor body, so the companion signals — ctor stores a vbtable
  ptr at [this+4] + vtordisp math — are not usable here): a `li r4,<imm>`
  feeding the first post-allocator ctor `bl`.

  Being vbase-flavored is NOT itself disqualifying — `new objType` allocates
  sizeof(most-derived) regardless of virtual bases, so a class whose TARGET and
  our BASE factory are BOTH vbase-shaped (e.g. DxMesh, TrackPanel, GemTrainer*)
  still yields a valid size oracle and stays in the normal table.

  What IS disqualifying is a SHAPE MISMATCH: the TARGET factory is vbase-shaped
  but OUR base factory is not (plain `new`, no r4 flag). That means the two
  paired functions are not the same factory — the map VA is a mis-attributed /
  foreign pin — so the `li r3,<imm>` size on the target side belongs to some
  other class and must NOT be read as sizeof(named class).
  Concrete case that motivated this: map VA 0x8268f050 was labeled
  ?NewObject@FileMergerOrganizer (our real FMO sizeof 0x3c, plain ctor) but the
  retail fn there is actually BandUser::NewLocalBandUser (LocalBandUser: virtual
  BandUser + virtual LocalUser, sizeof 0x110, vbase ctor) — target vbase vs base
  plain, implying +212B of phantom fields. Rejected candidates are reported
  separately and excluded from both the confirm and the drift/mismatch tables.

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


def _ensure_patched(project_dir):
    """Build through `post-compile` and ASSERT, instead of `--build`ing one obj.

    `objdiff-cli diff --build` (without `--full-build`) is `ninja <base .obj>`,
    a SINGLE-OBJECT target that stops one edge short of rb3-xenon's six
    post-compile patchers.  It answers from raw compiler output and leaves the
    object that way for report.json and every concurrent lane.  Measured on
    rb3-xenon: one such call cost unit default/BandUI 2.006 pp of
    matched_code_percent and read a 100.0 function as 99.7.

    Memoized per tree per process, so a per-symbol loop pays ~1 s once rather
    than per symbol.  Raises UnpatchedTreeError rather than returning a number.
    """
    import sys as _sys, os as _os
    _scripts = _os.path.dirname(_os.path.dirname(_os.path.abspath(__file__)))
    if _scripts not in _sys.path:
        _sys.path.insert(0, _scripts)
    from orchestrator.patch_guard import ensure_patched_tree_once
    return ensure_patched_tree_once(project_dir)


def run_objdiff(symbol, project_dir):
    _ensure_patched(project_dir)
    objdiff_bin = os.path.join(REPO, "bin", "objdiff-cli")
    h = hashlib.md5(symbol.encode()).hexdigest()[:10]
    out = f"/tmp/claude/nobj_{h}.json"
    os.makedirs("/tmp/claude", exist_ok=True)
    cmd = [objdiff_bin, "diff", "-p", project_dir, symbol,
           # No `--build --incremental`: `ninja <one .obj>` skips the six
           # post-compile patchers. _ensure_patched() handles the build.
           "--include-instructions",
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


def _alloc_idx(seq):
    """Index of the allocator `bl` (its result r3 is stw'd or null-checked next)."""
    for i, (op, args) in enumerate(seq):
        if op != "bl":
            continue
        for op2, a2 in seq[i + 1:i + 3]:
            a2 = a2.strip()
            if op2 == "stw" and a2.startswith("r3,"):
                return i
            if op2 == "cmplwi" and a2.startswith("r3,"):
                return i
    return None


def ctor_has_vbase_flag(seq):
    """True if the ctor `bl` (first bl after the allocator) is fed an extra
    integer arg via `li r4,<imm>` — the virtual-base most-derived flag. A plain
    NEW_OBJ factory calls its default ctor with r3 only, so no r4 setup appears.
    Vbase-ness alone is fine; a target-vs-base MISMATCH of this flag is the
    mis-attribution signal (see module docstring)."""
    ai = _alloc_idx(seq)
    if ai is None:
        return False
    for j in range(ai + 1, len(seq)):
        if seq[j][0] != "bl":
            continue
        # seq[j] is the ctor call; look for `li r4,...` in its arg-setup window
        for k in range(ai + 1, j + 1):
            if seq[k][0] == "li":
                parts = [p.strip() for p in seq[k][1].split(",")]
                if parts and parts[0] == "r4":
                    return True
        return False  # first post-alloc bl had no r4 flag -> plain ctor
    return False


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
        tgt_seq = side_seq(instrs, "target")
        base_seq = side_seq(instrs, "base")
        retail = size_before_alloc(tgt_seq)
        ours = size_before_alloc(base_seq)
        pct = d.get("normalized_match_percent")
        tgt_vbase = ctor_has_vbase_flag(tgt_seq)
        base_vbase = ctor_has_vbase_flag(base_seq)
        # mis-attribution: target factory is vbase-shaped, ours is not (or ours
        # has no comparable factory) -> paired functions differ -> size is bogus.
        reject = tgt_vbase and not base_vbase
        delta = (retail - ours) if (retail is not None and ours is not None) else None
        rows.append(dict(cls=cls, sym=sym, retail=retail, ours=ours,
                         delta=delta, pct=pct, err=None,
                         tgt_vbase=tgt_vbase, base_vbase=base_vbase, reject=reject))
        tag = " [SHAPE-MISMATCH-REJECT]" if reject else (
              " [vbase both sides]" if tgt_vbase else "")
        print(f"  {cls:34s} retail={retail} ours={ours} delta={delta} pct={pct}{tag}",
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

    rejected = [r for r in rows if r.get("reject")]
    mism = [r for r in rows if r["delta"] not in (None, 0) and not r.get("reject")]
    mism.sort(key=lambda r: abs(r["delta"]))

    with open(args.json, "w") as f:
        json.dump(dict(scanned=len(rows), mismatches=len(mism),
                       shape_rejected=len(rejected), rows=rows), f, indent=1)

    def fmt(v): return "-" if v is None else (hex(v) if isinstance(v, int) else str(v))
    lines = ["# NewObject-sizeof oracle scan", "",
             f"scanned={len(rows)}  mismatches={len(mism)}  "
             f"shape_rejected={len(rejected)}", "",
             "## Mismatches (|delta| ascending)", "",
             "| class | retail | ours | delta | factory_pct |",
             "|---|---|---|---|---|"]
    for r in mism:
        lines.append(f"| {r['cls']} | {fmt(r['retail'])} | {fmt(r['ours'])} | "
                     f"{r['delta']:+d} | {r['pct']} |")
    lines += ["", "## Shape-mismatch rejected (target vbase, our base plain "
              "-> mis-attributed pin, size NOT a valid sizeof)", "",
              "| class | target_li_r3 | our_li_r3 | reason |",
              "|---|---|---|---|"]
    for r in sorted(rejected, key=lambda x: x["cls"]):
        lines.append(f"| {r['cls']} | {fmt(r['retail'])} | {fmt(r['ours'])} | "
                     f"target vbase ctor, base plain |")
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
