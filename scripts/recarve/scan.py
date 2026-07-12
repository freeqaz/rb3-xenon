#!/usr/bin/env python3
"""Recarve Stage A: scan pinned TUs for attribution-repair candidates.

Merges three deterministic signals per pinned TU (see
docs/plans/recarve-pipeline.md):

  EXTEND   — the pin's .text end abuts a contiguous UNOWNED run of REAL
             (>44B, non-ICF-stub) retail functions (find_underpins.py logic).
             Candidate for the Stage B boundary hill-climb.
  IN-UNIT  — the wave-16 recarve signature from report.json: the unit has
             solid matched/hi-fuzzy functions sitting next to a wall of ~0%
             functions ("correct code, wrong boundary/identity").
  MAP-GAP  — retail fn_ addresses inside the pinned ranges with NO entry in
             target_symbol_map.json: the pre-compile renamer can't pair them,
             so they read a false 0% (Stage C identity work).

Screens applied before counting (wave-17 ops lessons):
  * suspected EH funclets (~0x20-0x30B, hi-fuzzy) are counted separately and
    EXCLUDED from the hi count — they are parent-dependent, zero standalone
    yield. (--verify-funclets confirms via the r12 prologue in the unit asm.)
  * except_data_* symbols are excluded everywhere (structural 8-32B blobs).
  * stub-only (<=44B) unowned runs are dropped: extending into the ICF-fold
    farm manufactures fake matches.

Deterministic pre-filter + ranking only. Real membership is confirmed
empirically by Stage B (extend pin, rebuild, captured fns match).
"""
import argparse, bisect, json, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
SYM = ROOT / "config/45410914/symbols.txt"
SPLITS = ROOT / "config/45410914/splits.txt"
REPORT = ROOT / "build/45410914/report.json"
TMAP = ROOT / "scripts/target_symbol_map.json"
ASM = ROOT / "build/45410914/asm"

STUB = 0x2C           # <=44B = ICF-stub-farm size class
FUNCLET_LO, FUNCLET_HI = 0x20, 0x30   # suspected EH-funclet size band
HI_FUZZY = 90.0
ZERO_FUZZY = 5.0


def load_functions():
    """All .text type:function symbols from symbols.txt -> sorted (addr, size, name)."""
    funcs = []
    rx = re.compile(
        r"^(\S+) = \.text:0x([0-9A-Fa-f]+);.*?\btype:function\b.*?size:0x([0-9A-Fa-f]+)")
    for l in SYM.read_text().splitlines():
        m = rx.match(l)
        if m:
            funcs.append((int(m.group(2), 16), int(m.group(3), 16), m.group(1)))
    funcs.sort()
    return funcs


def load_ranges():
    """Pinned .text ranges from splits.txt -> sorted (start, end, tu)."""
    ranges, cur = [], None
    for l in SPLITS.read_text().splitlines():
        h = re.match(r"^(\S+\.(?:cpp|c|cc)):\s*$", l)
        if h:
            cur = h.group(1)
            continue
        t = re.search(r"\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)", l)
        if t and cur:
            ranges.append((int(t.group(1), 16), int(t.group(2), 16), cur))
    ranges.sort()
    return ranges


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--top", type=int, default=40, help="rows to print")
    ap.add_argument("--unit", help="only scan this TU (splits.txt name)")
    ap.add_argument("--json", default=str(Path.home() / "tmp/recarve/scan.json"))
    ap.add_argument("--verify-funclets", action="store_true",
                    help="confirm suspected funclets via r12 prologue in unit asm")
    args = ap.parse_args()

    funcs = load_functions()
    faddr = [f[0] for f in funcs]
    ranges = load_ranges()
    rstart = [r[0] for r in ranges]
    tmap = json.load(open(TMAP))
    mapped = {int(k, 16) for k in tmap if k.startswith("0x")}
    report = json.load(open(REPORT))
    # splits "band3/game/Foo.cpp" <-> report unit "default/band3/game/Foo"
    runits = {u["name"]: u for u in report["units"]}

    def owned(addr):
        i = bisect.bisect_right(rstart, addr) - 1
        while i >= 0 and ranges[i][0] > addr - 0x20000:
            s, e, tu = ranges[i]
            if s <= addr < e:
                return tu
            i -= 1
        return None

    def fn_at(addr):
        i = bisect.bisect_left(faddr, addr)
        return funcs[i] if i < len(faddr) and funcs[i][0] == addr else None

    def unowned_run(end):
        """Contiguous unowned function run starting exactly at `end`."""
        run_fns = run_bytes = real_fns = real_bytes = 0
        a = end
        while True:
            f = fn_at(a)
            if not f or owned(a):
                break
            _, sz, name = f
            if sz == 0:
                break
            if not name.startswith("except_data_"):
                run_fns += 1
                run_bytes += sz
                if sz > STUB:
                    real_fns += 1
                    real_bytes += sz
            a += sz
        return run_fns, run_bytes, real_fns, real_bytes, a

    def funclet_prologue_confirmed(tu, want_sizes):
        """Return set of fn VAs in the unit asm with an r12-relative prologue
        (EH funclets compute the parent frame via r12) among `want_sizes`."""
        asm = ASM / (tu[:-4] + ".s") if tu.endswith(".cpp") else ASM / (tu + ".s")
        if not asm.exists():
            return set()
        confirmed, cur_va, cur_lines = set(), None, []
        for l in asm.read_text(errors="replace").splitlines():
            m = re.match(r"\.fn (\S+),", l)
            if m:
                cur_va, cur_lines = m.group(1), []
                continue
            if l.startswith(".endfn"):
                cur_va = None
                continue
            if cur_va and len(cur_lines) < 4 and "\t" in l:
                cur_lines.append(l)
                if re.search(r"\b(subi?c?|subf|addi)\s+r\d+,\s*r12\b", l.split("\t")[-1]):
                    va = re.search(r"fn_([0-9A-Fa-f]+)", cur_va)
                    if va:
                        confirmed.add(int(va.group(1), 16))
        return confirmed

    # per-TU pinned ranges (a TU may have several)
    tu_ranges = {}
    for s, e, tu in ranges:
        tu_ranges.setdefault(tu, []).append((s, e))

    cands = []
    for tu, rs in sorted(tu_ranges.items()):
        if args.unit and tu != args.unit:
            continue

        # --- EXTEND signal: check each range end (usually the last matters)
        ext = {"real_fns": 0, "real_bytes": 0, "run_fns": 0, "at_end": None, "run_end": None}
        for s, e in rs:
            f = fn_at(e)
            if not f:
                continue
            run_fns, run_bytes, real_fns, real_bytes, run_end = unowned_run(e)
            if real_fns > ext["real_fns"]:
                ext = {"real_fns": real_fns, "real_bytes": real_bytes,
                       "run_fns": run_fns, "at_end": e, "run_end": run_end}

        # --- MAP-GAP signal: retail fns in pinned ranges missing from the map
        retail_fns = unmapped = 0
        for s, e in rs:
            i = bisect.bisect_left(faddr, s)
            while i < len(faddr) and funcs[i][0] < e:
                a, sz, name = funcs[i]
                i += 1
                if name.startswith("except_data_") or sz <= 8:
                    continue
                retail_fns += 1
                if a not in mapped:
                    unmapped += 1

        # --- IN-UNIT signature from report.json
        ru = runits.get("default/" + tu[:-4] if tu.endswith(".cpp") else "default/" + tu)
        matched = hi = zero = suspects = total = 0
        if ru:
            for fn in ru.get("functions") or []:
                total += 1
                fz = fn.get("fuzzy_match_percent") or 0.0
                nm = fn.get("match_percent_normalized")
                sz = int(fn.get("size") or 0)
                if nm == 100.0:
                    matched += 1
                elif fz >= HI_FUZZY:
                    if FUNCLET_LO <= sz <= FUNCLET_HI:
                        suspects += 1   # suspected EH funclet: no standalone yield
                    else:
                        hi += 1
                elif fz <= ZERO_FUZZY:
                    zero += 1

        if args.verify_funclets and suspects:
            conf = funclet_prologue_confirmed(tu, None)
            # informational only: report how many suspects are prologue-confirmed
            ext["funclets_confirmed"] = len(conf)

        # --- composite rank (documented in docs/plans/recarve-pipeline.md):
        # EXTEND fns are near-certain yield (x2); zeros only count as recarve
        # signal when the unit has proven source (>=5 matched); unmapped
        # retail fns are Stage C work at lower confidence.
        zero_w = 1.0 if matched >= 5 else 0.25
        score = 2 * ext["real_fns"] + zero_w * zero + 0.5 * unmapped
        if score <= 0:
            continue
        cands.append({
            "tu": tu, "score": round(score, 1),
            "extend": ext, "retail_fns_pinned": retail_fns, "unmapped": unmapped,
            "matched": matched, "hi": hi, "zero": zero,
            "suspect_funclets": suspects, "ours_total": total,
        })

    cands.sort(key=lambda c: -c["score"])
    out = Path(args.json)
    out.parent.mkdir(parents=True, exist_ok=True)
    json.dump(cands, open(out, "w"), indent=1)

    print(f"{len(tu_ranges)} pinned TUs scanned; {len(cands)} candidates "
          f"(hi>={HI_FUZZY:.0f} zero<={ZERO_FUZZY:.0f} funclet={FUNCLET_LO:#x}-{FUNCLET_HI:#x})")
    hdr = (f"{'TU':<42}{'score':>7}{'ext_fns':>8}{'ext_KB':>7}"
           f"{'match':>6}{'hi':>4}{'zero':>5}{'fclt':>5}{'unmap':>6}")
    print(hdr)
    print("-" * len(hdr))
    for c in cands[:args.top]:
        print(f"{c['tu']:<42}{c['score']:>7}{c['extend']['real_fns']:>8}"
              f"{c['extend']['real_bytes']/1024:>7.1f}{c['matched']:>6}{c['hi']:>4}"
              f"{c['zero']:>5}{c['suspect_funclets']:>5}{c['unmapped']:>6}")
    print(f"\n[json] {out} ({len(cands)} candidates)")


if __name__ == "__main__":
    main()
