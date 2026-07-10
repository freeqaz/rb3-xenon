#!/usr/bin/env python3
"""Spill-store signature scanner (campaign T1, 2026-07-10).

Batch classifier over a near-miss pool for the cracked spill-store-count
mechanism (docs/decomp/research/2026-07-10-spill-store-homing-mechanism.md,
docs/plans/spill-leverage-campaign-2026-07-10.md #T1).

Per function:
  1. Run objdiff (diff_inspect.run_objdiff_for_symbol), collect instruction rows.
  2. Signature gate: how much of the insert/delete set is STORES to a frame
     slot (stw/stb/sth/stfs/stfd/stmw with mem base r31/r30/r1)?
     purity: 'sole_diff' (ALL indels are such stores and nothing else really
     differs), 'dominant' (>=80% of indels), 'present' (any), else no hit.
  3. Direction per implicated slot: ours-extra / ours-missing / moved.
  4. Classify each implicated slot A/B/C from the FULL listing of the side
     that owns the store:
       A (address-taken): addi rX,<base>,<slot> or a load from <slot>(<base>)
       B (EH homing):     write-only slot + EH evidence for the function
       C (pressure):      write-only, no EH evidence
     EH evidence, in strength order:
       - except_record_<FNADDR> present in config/45410914/symbols.txt, where
         FNADDR comes from scripts/target_symbol_map.json reverse lookup
         (mangled name -> retail address) or a direct symbols.txt name match.
         If the address IS resolvable and there is NO except_record, that is
         strong negative evidence -> class C even if a call is adjacent.
       - only when the address cannot be resolved: a bl adjacent (+/-2 rows)
         to the store counts as WEAK evidence -> class B, marked 'weak:'.

DIRECTION CALIBRATION (hard-won; do not re-derive from intuition):
  Ground truth specimen ??1FaderGroup@@QAA@XZ (unit Faders.cpp, 0x826EF120):
  OUR compiled side has 41 instructions with TWO `stw r27, 0x54(r31)`;
  RETAIL has 40 with ONE. The sole indel objdiff emits is:
      index 14  match_type='insert'  TGT: ---   SRC: stw r27, 0x54, r31
  Therefore, in objdiff JSON rows:
      match_type == 'insert'  -> target(retail) is None, base(ours) populated
                              -> instruction exists in OURS only  -> ours-extra
      match_type == 'delete'  -> target populated, base None
                              -> instruction exists in RETAIL only -> ours-missing
  (Verified 2026-07-10 against dtk asm build/45410914/asm/Faders.s, which
  shows a single `stw r27, 0x54(r31)` in fn_826EF120.)

Output JSON: {"records": [...], "summary": {...}}; --resume reads either this
shape or a bare list (offset_drift_sweep compatibility).

Usage:
  python3 scripts/harvest/spill_signature_scan.py ~/tmp/spill_pool.json \
      -o ~/tmp/spill_scan.json [--project-dir .] [--limit N] [--resume]
  python3 scripts/harvest/spill_signature_scan.py --sym '??1FaderGroup@@QAA@XZ' \
      [-o out.json] [--project-dir .]
"""
import argparse
import json
import os
import re
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "analysis"))
import diff_inspect  # noqa: E402

REPO_ROOT = os.path.dirname(os.path.dirname(HERE))

# objdiff arg text for loads/stores: "rD, offset, rBase" (optionally a
# trailing ", reloc_symbol" for global addressing -- excluded below).
MEM_RE = re.compile(r",\s*(-?0x[0-9a-fA-F]+|-?\d+),\s*(r\d+)\b")
# addi arg text: "rD, rA, imm"
ADDI_RE = re.compile(
    r"^\s*(r\d+),\s*(r\d+),\s*(-?0x[0-9a-fA-F]+|-?\d+)\s*$")

FRAME_BASES = ("r31", "r30", "r1")
STORE_OPS = {"stw", "stb", "sth", "stfs", "stfd", "stmw"}
LOAD_OPS = {"lwz", "lbz", "lhz", "lha", "lfs", "lfd", "lmw", "lwa"}

SYMBOLS_TXT = os.path.join(REPO_ROOT, "config", "45410914", "symbols.txt")
TARGET_MAP = os.path.join(REPO_ROOT, "scripts", "target_symbol_map.json")
EXCEPT_RECORD_RE = re.compile(r"^except_record_([0-9A-Fa-f]{8})\s*=")
SYMTXT_FN_RE = re.compile(r"^(\S+)\s*=\s*\.text:0x([0-9A-Fa-f]{8})")


def parse_int(s):
    s = s.strip()
    return int(s, 16) if s.lower().startswith(("0x", "-0x")) else int(s)


def frame_store_slot(side):
    """If `side` is a store to a frame slot, return (base, off); else None."""
    if not side:
        return None
    op = (side.get("opcode") or "").strip()
    if op not in STORE_OPS:
        return None
    args = side.get("args", "") or ""
    m = MEM_RE.search(args)
    if not m:
        return None
    # trailing ", symbol" after base reg => global+offset addressing, not frame
    if args[m.end():].strip().startswith(","):
        return None
    base = m.group(2)
    if base not in FRAME_BASES:
        return None
    return (base, parse_int(m.group(1)))


def frame_load_slot(side):
    """If `side` is a load from a frame slot, return (base, off); else None."""
    if not side:
        return None
    op = (side.get("opcode") or "").strip()
    if op not in LOAD_OPS:
        return None
    args = side.get("args", "") or ""
    m = MEM_RE.search(args)
    if not m:
        return None
    if args[m.end():].strip().startswith(","):
        return None
    base = m.group(2)
    if base not in FRAME_BASES:
        return None
    return (base, parse_int(m.group(1)))


def addi_slot(side):
    """If `side` is `addi rX, <frame base>, imm`, return (base, off)."""
    if not side:
        return None
    if (side.get("opcode") or "").strip() != "addi":
        return None
    m = ADDI_RE.match(side.get("args", "") or "")
    if not m:
        return None
    if m.group(2) not in FRAME_BASES:
        return None
    return (m.group(2), parse_int(m.group(3)))


def diff_arg_is_real(ins):
    """True if a diff_arg row differs in something other than symbol naming.

    Symbol-name-only diffs (retail fn_/lbl_ vs our mangled names) are
    relocation noise that normalized diffing ignores; register/immediate/
    branch_dest diffs are real.
    """
    bd = ins.get("diff_breakdown") or {}
    for arg in bd.get("arguments", []):
        at = arg.get("arg_type", "")
        tv = (arg.get("target") or {}).get("value")
        bv = (arg.get("base") or {}).get("value")
        if at in ("register", "immediate") and tv != bv:
            return True
        if at == "branch_dest":
            return True
    return False


# ── EH evidence ─────────────────────────────────────────────────────────────

_except_addrs = None
_name_to_addrs = None


def _load_eh_tables():
    global _except_addrs, _name_to_addrs
    if _except_addrs is not None:
        return
    _except_addrs = set()
    _name_to_addrs = {}
    try:
        with open(SYMBOLS_TXT) as f:
            for line in f:
                m = EXCEPT_RECORD_RE.match(line)
                if m:
                    _except_addrs.add(m.group(1).upper())
                    continue
                m = SYMTXT_FN_RE.match(line)
                if m and not m.group(1).startswith(("fn_", "lbl_", "except_")):
                    _name_to_addrs.setdefault(m.group(1), set()).add(
                        m.group(2).upper())
    except OSError:
        pass
    try:
        with open(TARGET_MAP) as f:
            tmap = json.load(f)
        for addr, name in tmap.items():
            a = addr.lower().replace("0x", "").upper().zfill(8)
            names = name if isinstance(name, list) else [name]
            for n in names:
                if isinstance(n, str):
                    _name_to_addrs.setdefault(n, set()).add(a)
    except (OSError, ValueError):
        pass


def eh_evidence_for(sym):
    """Return (verdict, evidence_str).

    verdict: True (has EH), False (address known, no except_record),
             None (address unresolvable -- undetermined).
    """
    _load_eh_tables()
    addrs = sorted(_name_to_addrs.get(sym, ()))
    if not addrs:
        return None, "fn address unresolvable (no target_symbol_map/symbols.txt entry)"
    hits = [a for a in addrs if a in _except_addrs]
    if hits:
        return True, f"except_record_{hits[0]} in symbols.txt (EH funclet/frame)"
    return False, f"no except_record for {'/'.join(addrs)} in symbols.txt"


# ── Per-function scan ───────────────────────────────────────────────────────

def classify_slot(slot, dirs, instrs, sym):
    """Classify one implicated (base, off) slot as A/B/C.

    dirs: set of directions that implicated it ('ours-extra'/'ours-missing').
    Scan the side that owns the store: retail (target) for ours-missing,
    ours (base) for ours-extra ("the extra store's slot semantics live on
    our side"); if both (moved), scan retail.
    """
    base, off = slot
    if "ours-missing" in dirs:
        side_key, side_name = "target", "retail"
    else:
        side_key, side_name = "base", "ours"

    addr_taken = False
    reloaded = False
    store_rows = []
    for ins in instrs:
        side = ins.get(side_key)
        if addi_slot(side) == slot:
            addr_taken = True
        if frame_load_slot(side) == slot:
            reloaded = True
        if frame_store_slot(side) == slot:
            store_rows.append(ins.get("index", -1))

    if addr_taken or reloaded:
        ev = []
        if addr_taken:
            ev.append(f"addi rX,{base},{off:#x}")
        if reloaded:
            ev.append(f"reload from {off:#x}({base})")
        return "A", f"address-taken on {side_name} side: " + ", ".join(ev)

    eh, eh_ev = eh_evidence_for(sym)
    note = (" (extra store; slot semantics scanned on OUR side)"
            if side_name == "ours" else "")
    if eh is True:
        return "B", f"write-only slot; {eh_ev}{note}"
    if eh is False:
        return "C", f"write-only slot; {eh_ev}{note}"

    # Address unresolvable: fall back to weak adjacency evidence -- a bl
    # within +/-2 rows of any implicated store on the scan side.
    by_index = {ins.get("index"): ins for ins in instrs}
    for ridx in store_rows:
        for d in (-2, -1, 1, 2):
            nb = by_index.get(ridx + d)
            if not nb:
                continue
            s = nb.get(side_key)
            if s and (s.get("opcode") or "").strip() == "bl":
                callee = (s.get("args", "") or "").strip()[:60]
                return "B", (f"write-only slot; weak: bl {callee} adjacent "
                             f"to store; {eh_ev}{note}")
    return "C", f"write-only slot; {eh_ev}{note}"


def scan_one(entry, project_dir):
    sym = entry["sym"]
    unit = entry.get("unit")
    rec = {
        "sym": sym,
        "unit": unit,
        "pct": entry.get("pct"),
        "size": entry.get("size"),
        "demangled": entry.get("demangled", ""),
        "purity": None,
        "n_indels": 0,
        "n_store_indels": 0,
        "n_other_real_diffs": 0,
        "direction": None,
        "slots": [],
        "status": "ok",
    }
    try:
        json_path = diff_inspect.run_objdiff_for_symbol(
            sym, project_dir=project_dir, unit=unit)
    except SystemExit:
        rec["status"] = "objdiff_failed"
        return rec
    except Exception as e:  # noqa: BLE001
        rec["status"] = f"error: {e}"
        return rec

    try:
        with open(json_path) as f:
            data = json.load(f)
    except Exception as e:  # noqa: BLE001
        rec["status"] = f"json_error: {e}"
        return rec

    if rec["pct"] is None:
        rec["pct"] = data.get("normalized_match_percent")
    if not rec["demangled"]:
        rec["demangled"] = data.get("demangled", "")

    instrs = data.get("instructions", [])

    # Signature gate over indel rows.
    slot_dirs = {}   # (base, off) -> set of directions
    n_indels = 0
    n_store_indels = 0
    n_other_real = 0
    for ins in instrs:
        mt = ins.get("match_type")
        if mt == "insert":
            # See DIRECTION CALIBRATION in module docstring: insert = OURS-only.
            n_indels += 1
            slot = frame_store_slot(ins.get("base"))
            if slot:
                n_store_indels += 1
                slot_dirs.setdefault(slot, set()).add("ours-extra")
        elif mt == "delete":
            # delete = RETAIL-only (present in retail, missing from ours).
            n_indels += 1
            slot = frame_store_slot(ins.get("target"))
            if slot:
                n_store_indels += 1
                slot_dirs.setdefault(slot, set()).add("ours-missing")
        elif mt in ("replace", "diff_op"):
            n_other_real += 1
        elif mt == "diff_arg" and diff_arg_is_real(ins):
            n_other_real += 1

    rec["n_indels"] = n_indels
    rec["n_store_indels"] = n_store_indels
    rec["n_other_real_diffs"] = n_other_real

    if n_store_indels == 0:
        return rec  # no hit; purity stays None

    if n_store_indels == n_indels and n_other_real == 0:
        rec["purity"] = "sole_diff"
    elif n_store_indels >= 0.8 * n_indels:
        rec["purity"] = "dominant"
    else:
        rec["purity"] = "present"

    # Overall direction.
    all_dirs = set()
    for dirs in slot_dirs.values():
        all_dirs |= dirs
    if all_dirs == {"ours-extra"}:
        rec["direction"] = "ours-extra"
    elif all_dirs == {"ours-missing"}:
        rec["direction"] = "ours-missing"
    else:
        rec["direction"] = "mixed"

    # Per-slot classification.
    for slot in sorted(slot_dirs):
        dirs = slot_dirs[slot]
        cls, ev = classify_slot(slot, dirs, instrs, sym)
        rec["slots"].append({
            "off": f"{slot[1]:#x}",
            "base": slot[0],
            "direction": ("moved" if len(dirs) > 1 else next(iter(dirs))),
            "class": cls,
            "evidence": ev,
        })
    return rec


def build_summary(records):
    by_purity_class = {}
    by_purity = {}
    by_direction = {}
    n_hits = 0
    for r in records:
        p = r.get("purity")
        if not p:
            continue
        n_hits += 1
        by_purity[p] = by_purity.get(p, 0) + 1
        d = r.get("direction") or "?"
        by_direction[d] = by_direction.get(d, 0) + 1
        for s in r.get("slots", []):
            key = p
            by_purity_class.setdefault(key, {})
            c = s["class"]
            by_purity_class[key][c] = by_purity_class[key].get(c, 0) + 1
    return {
        "n_scanned": len(records),
        "n_hits": n_hits,
        "fns_by_purity": by_purity,
        "fns_by_direction": by_direction,
        "slots_by_purity_x_class": by_purity_class,
    }


def unit_for_sym(sym, project_dir):
    """Best-effort unit/size/demangled lookup from report.json (for --sym)."""
    path = os.path.join(project_dir or REPO_ROOT,
                        "build", "45410914", "report.json")
    try:
        with open(path) as f:
            report = json.load(f)
    except (OSError, ValueError):
        return {}
    for u in report.get("units", []):
        for fn in u.get("functions", []):
            if fn.get("name") == sym:
                return {
                    "unit": u.get("name"),
                    "size": int(fn.get("size", 0)),
                    "demangled": fn.get("metadata", {}).get(
                        "demangled_name", ""),
                    "pct": fn.get("match_percent_normalized",
                                  fn.get("fuzzy_match_percent")),
                }
    return {}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pool", nargs="?",
                    help="gen_nearmiss_pool.py JSON (omit with --sym)")
    ap.add_argument("--sym", help="single mangled symbol (smoke mode)")
    ap.add_argument("-o", "--out")
    ap.add_argument("--project-dir", default=".")
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--resume", action="store_true",
                    help="skip syms already present in the output file")
    args = ap.parse_args()

    if args.sym:
        entry = {"sym": args.sym}
        entry.update(unit_for_sym(args.sym, args.project_dir))
        entry["sym"] = args.sym
        pool = [entry]
    elif args.pool:
        with open(args.pool) as f:
            pool = json.load(f)
    else:
        ap.error("need a pool JSON or --sym")
    if args.limit:
        pool = pool[:args.limit]

    done = {}
    if args.resume and args.out and os.path.exists(args.out):
        with open(args.out) as f:
            prev = json.load(f)
        prev_records = prev["records"] if isinstance(prev, dict) else prev
        done = {r["sym"]: r for r in prev_records}

    def dump(results):
        if not args.out:
            return
        with open(args.out, "w") as f:
            json.dump({"records": results,
                       "summary": build_summary(results)}, f, indent=1)

    results = list(done.values())
    t0 = time.time()
    for i, entry in enumerate(pool):
        if entry["sym"] in done:
            continue
        rec = scan_one(entry, args.project_dir)
        results.append(rec)
        if args.out and ((i + 1) % 20 == 0 or i == len(pool) - 1):
            dump(results)
            print(f"[{i+1}/{len(pool)}] {time.time()-t0:.0f}s elapsed",
                  flush=True)

    dump(results)
    summary = build_summary(results)
    print(json.dumps({"summary": summary}, indent=1))
    if args.sym:
        print(json.dumps(results[-1], indent=1))


if __name__ == "__main__":
    main()
