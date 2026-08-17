#!/usr/bin/env python3
"""BinStreamRev "LEAD B" per-TU rev-static signature scanner (2026-07-17).

Enumerates the true LEAD-B *wave-2* population: sub-100 streaming functions
whose residual is exactly the "retail reads a per-TU rev static vs. our code
reads the (LEAD-A-)shifted BinStreamRev member" codegen mismatch.

BACKGROUND (see ~/tmp/p5w6/binstreamrev-landable.md, LEAD B section):
  LEAD A (already landed on main, src/system/utl/BinStream.h) made
  `class BinStreamRev : public BinStream` (base@0), so `d >> field`
  dispatches on &d directly and the `rev`/`altRev`/`stream` members shifted
  to +0xC/+0x10/+0x14. That closed the composed-load residual but left one
  class of mismatch: functions that read `d.rev`/`d.altRev` inside a
  versioned struct Load. Retail compiled each such streaming TU with a
  *file-scope* `static` archive revision (a `lbl_82C*` label in .data/.rdata),
  set once by the outer Load and read by the inner sub-Loads. Our post-LEAD-A
  code instead reads the shifted member. LEAD B introduces a per-TU
  `static unsigned short sXxxRev;` set at Load entry and read in place of
  `d.rev`, reproducing retail's static read and (empirically) also resolving
  the accompanying whole-function r30<->r31 regalloc swap.

THE SIGNATURE (per function, from objdiff mismatch rows):
  * TARGET (retail) side has a load of a per-TU rev static:
        opcode in {lhz, lwz, lha, lhau, lwzu}  AND  arg contains `lbl_82C`.
    Halfword (`lhz`) is the canonical `unsigned short` rev (LightPreset
    0x82CC6E90, EventTrigger 0x82CC6C20, CharHair 0x82CBF3F8); a few TUs
    compile the rev as a full word (`lwz`, e.g. Morph 0x82CC65DC).
    NOTE the region is ~0x82CB**-0x82CC** (NOT only 0x82CC**); an early
    0x82CC-only prototype filter *missed CharHair* (0x82CBF3F8). Do not
    re-narrow it.
  * OURS (base) side has a mismatched load of the shifted BinStreamRev
    member: `lwz/lhz rX, 0x{c,10,14}, rY`  (rev@0xC, altRev@0x10, stream@0x14).
  A function that shows BOTH (in its mismatch set) is CONFIRMED LEAD-B.

POPULATION / FALSE-POSITIVE GUARDS:
  * Candidate population = sub-100 functions whose *mangled name* contains
    `BinStreamRev` (i.e. `Load(BinStreamRev&)` or `operator>>(BinStreamRev&,
    T&)`). A raw `.rev` source grep is far too broad (200+ files read some
    `.rev` member); the BinStreamRev-in-name filter is precise and pairs
    correctly through the target-symbol renamer.
  * `lwz lbl_82C*` alone is NOT proof of a rev read — a word load of a 0x82C
    label is very often a Symbol* / global-object pointer. We only treat it
    as a rev read when it co-occurs with a shifted-member base load in the
    SAME function's mismatch set (require_member=True, the default).
  * 0.00% functions with a template/container operator>> name
    (`??$?5...@stlpmtx_std@@`) are ICF-folded / unpaired collapses, NOT
    LEAD-B (flipping a static cannot recover a 0% pairing collapse, and a
    vector/list loader forwards to element Load without reading d.rev). They
    are classed COLLAPSE and excluded from the ranked wave unless --keep-collapse.

WHY OBJDIFF, NOT STATIC .s SCANNING:
  The dtk .s `.fn` labels are mis-nested (jeff asm mis-nest) and the
  target-symbol map records the mis-nested *label* VA, so neither
  report-offset+base nor map-VA reliably locates a function's real code
  across all TUs (verified: it caught EventTrigger but missed LightPreset's
  SpotlightDrawerEntry::Load). objdiff pairs target<->base by symbol and
  emits the true mismatched instruction pair, so the per-function detector
  runs on objdiff mismatch rows, which is authoritative.

GROUND TRUTH (must flag as CONFIRMED):
  * LightPreset  ?Load@SpotlightDrawerEntry@LightPreset@@...  82.0%  (lhz 0x82CC6E90)
  * EventTrigger ??5@...BinStreamRev@@...Anim@EventTrigger@@   83.5%  (lhz 0x82CC6C20)

Output JSON: {"records": [...], "summary": {...}}.

Usage:
  # Full scan across all streaming TUs (needs a buildable project_dir):
  python3 scripts/harvest/leadb_signature_scan.py \
      --project-dir . -o ~/tmp/p5w6/leadb_scan.json

  # Single function:
  python3 scripts/harvest/leadb_signature_scan.py \
      --sym '?Load@SpotlightDrawerEntry@LightPreset@@QAAXAAVBinStreamRev@@@Z' \
      --project-dir .

  # Enumerate the candidate population only (no builds, fast triage):
  python3 scripts/harvest/leadb_signature_scan.py --enumerate-only

Flags: --limit N, --resume, --min-pct P (population floor, default 5.0 to drop
0% collapses), --keep-collapse, --report PATH (report.json), --asm-dir DIR.
"""
import argparse
import json
import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "analysis"))
import diff_inspect  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DEFAULT_REPORT = os.path.join(REPO, "build", "45410914", "report.json")
DEFAULT_ASM = os.path.join(REPO, "build", "45410914", "asm")

# Wave-1 lane assignment (~/tmp/p5w6/leadb-wave-lanes.md). Used only to mark
# NEW (not-in-wave-1) candidates; not a correctness input.
WAVE1_LANES = {
    "LightPreset": "A", "EventTrigger": "A", "Font": "A",
    "CharHair": "B", "CharBone": "B", "Morph": "B",
    "PartAnim": "C", "Anim": "C", "Sequence": "C", "CameraShot": "C",
    "ColorPalette": "D", "HamNavProvider": "D", "InlineHelp": "D",
    "BustAMoveData": "D", "SampleZone": "D",
}

# TARGET-side per-TU rev static read: a small-int load of a data label in the
# empirical rev-static cluster 0x82CB**-0x82CC**. Every ground-truth rev sits
# here: LightPreset 0x82CC6E90, EventTrigger 0x82CC6C20, CharHair 0x82CBF3F8,
# Morph 0x82CC65DC, Font 0x82CC6490, InlineHelp 0x82CBDC14. NOT the broader
# 0x82C7** region — 0x82C71838 turned up as a *shared* global (CameraShot +
# InlineHelp both load it) and is a false positive for "per-TU rev".
REV_STATIC_RE = re.compile(r"\blbl_82C[BC][0-9A-Fa-f]{4}\b")
REV_STATIC_OPS = {"lhz", "lwz", "lha", "lhau", "lwzu"}
# OURS-side shifted BinStreamRev member: rev@0xC / altRev@0x10 / stream@0x14.
MEMBER_RE = re.compile(r",\s*0x(c|10|14),\s*r\d+\b", re.IGNORECASE)
MEMBER_OPS = {"lwz", "lhz"}
# small setup/compare ops that get rescheduled by the r30<->r31 swap and show
# up as unpaired insert/delete rows — soft noise, not independent residuals.
MECH_OPS = {"li", "mr", "cmpwi", "cmplwi", "cmplw", "cmpw", "addi", "lis",
            "nop", "cror"}
# Collapsed template/container operator>> (ICF / unpaired) — not LEAD-B.
COLLAPSE_NAME_RE = re.compile(r"\?\?\$\?5.*stlpmtx_std@@")


def _side(ins, which):
    s = ins.get(which)
    return s or {}


def _opcode(side):
    return (side.get("opcode") or "").strip()


def _args(side):
    """The DISPLAY spelling, as objdiff-cli prints it.

    Used only by _is_r30_r31_swap, which is a purely textual r30<->r31
    substitution test and is symmetric under either spelling.
    """
    return side.get("args") or ""


def _flat_args(side):
    """The pre-4.2 flat comparison-join spelling, rebuilt from `typed_args`.

    objdiff-cli 4.2.x prints a *display* string in `side["args"]`: d-form
    operands are parenthesised (`r11, 0xc(r30)`), COFF relocations carry an
    `@h`/`@l` suffix, and a relocation that is not part of the printed
    operand is DROPPED from the string altogether. Both signature halves
    below read the old flat join, which is exactly `", ".join(typed_args)`:

      MEMBER_RE     wants `..., 0xc, r30` and sees `..., 0xc(r30)` -> 0 hits.
      REV_STATIC_RE wants the `lbl_82C[BC]....` token, which survives inside
                    `lbl_82CC6E90@l(r30)` but VANISHES on a plain d-form load
                    whose relocation is the dropped one. Measured on 25 rb3
                    near-miss functions: 389 hits on the display string vs 647
                    on the rebuild -- so the rev half was NOT merely "still
                    working", it was down 40%.

    Rebuilding is the only way to get the second one back: the relocation is
    not in the display string at all, so no amount of paren-tolerance in the
    regex can recover it.
    """
    if not side:
        return ""
    out = []
    for a in side.get("typed_args") or []:
        t, v = a.get("type"), a.get("value")
        if t == "Signed" and isinstance(v, int):
            out.append(("-0x%x" % -v) if v < 0 else "0x%x" % v)
        elif t in ("Unsigned", "BranchDest") and isinstance(v, int):
            out.append("0x%x" % v)
        else:
            out.append(str(v))
    return ", ".join(out)


def _swap_r30_r31(s):
    return (s.replace("r30", "\0").replace("r31", "r30").replace("\0", "r31"))


def _is_reloc_noise(t, b):
    """True if target/base are the same op differing only in Symbol operands.

    objdiff's *normalized* score (functionRelocDiffs=none — the metric
    report.json's fuzzy_match_percent uses) treats relocations that point at
    differently-named/addressed symbols as equal, since addresses shift
    between builds. run_objdiff_for_symbol here builds a non-normalized diff,
    so it surfaces those rows (`bl fn_X` vs `bl ?Load@...`, `lfs lbl_82000D78`
    vs `lfs __real@00000000`); they are not real residuals. Detect them by
    comparing typed_args: Register/Signed/Unsigned operands must be identical;
    Symbol operands may differ.
    """
    if _opcode(t) != _opcode(b) or not _opcode(t):
        return False
    ta = t.get("typed_args") or []
    ba = b.get("typed_args") or []
    if len(ta) != len(ba) or not ta:
        return False
    saw_sym_diff = False
    for x, y in zip(ta, ba):
        if x.get("type") == "Symbol" and y.get("type") == "Symbol":
            if x.get("value") != y.get("value"):
                saw_sym_diff = True
            continue
        if x != y:
            return False
    return saw_sym_diff


def _is_r30_r31_swap(t, b):
    """True if target/base are the same op differing only by an r30<->r31 swap.

    These rows are the whole-function LEAD-A regalloc-swap companion noise
    that the per-TU sXxxRev flip empirically resolves along with the rev read
    (proven on LightPreset::SpotlightDrawerEntry::Load 82->100). Not a real
    residual, so they must not count against confirmation confidence.
    """
    top, bop = _opcode(t), _opcode(b)
    if not top or top != bop:
        return False
    targ, barg = _args(t), _args(b)
    if targ == barg:
        return False
    return ("r30" in targ or "r31" in targ) and _swap_r30_r31(targ) == barg


def classify_function(instrs):
    """Return a signature record for one function's objdiff instruction list."""
    mismatches = [i for i in instrs
                  if i.get("match_type") not in ("equal", None)]
    rev_static_reads = []   # target-side rev static loads
    rev_ops = []            # opcode used for each rev read (lhz=halfword etc)
    member_reads = []       # base-side shifted-member loads
    regswap_rows = 0        # r30<->r31 diff_arg noise (LEAD-A companion)
    reloc_rows = 0          # Symbol-only diffs (normalized-mode ignores these)
    other_rows = 0

    for ins in mismatches:
        t, b = _side(ins, "target"), _side(ins, "base")
        top, bop = _opcode(t), _opcode(b)
        # Both signature halves read the FLAT spelling -- see _flat_args.
        targ, barg = _flat_args(t), _flat_args(b)
        is_rev = top in REV_STATIC_OPS and REV_STATIC_RE.search(targ)
        # a bare `lis rX, lbl_82C..@ha` companion of the rev load also counts
        is_lis_rev = top == "lis" and REV_STATIC_RE.search(targ)
        is_member = bop in MEMBER_OPS and MEMBER_RE.search(barg)
        is_regswap = _is_r30_r31_swap(t, b)
        if is_rev:
            rev_static_reads.append(REV_STATIC_RE.search(targ).group(0))
            rev_ops.append(top)
        if is_member:
            member_reads.append(MEMBER_RE.search(barg).group(0).strip(", "))
        if is_rev or is_lis_rev or is_member:
            continue
        if is_regswap:
            regswap_rows += 1
            continue
        if _is_reloc_noise(t, b):
            reloc_rows += 1
            continue
        # A whole-function r30<->r31 swap also reschedules the small setup ops
        # around the rev read (li r5, mr r3, the cmplwi rev compare, the
        # shifted addi), which objdiff emits as unpaired insert/delete rows.
        # These are swap-displacement companions, not independent residuals.
        op = top or bop
        if op in MECH_OPS:
            regswap_rows += 1
            continue
        other_rows += 1

    n_mismatch = len(mismatches)
    rec = {
        "n_mismatch": n_mismatch,
        "rev_static_reads": rev_static_reads,
        "rev_static_addrs": sorted(set(rev_static_reads)),
        "n_rev_static": len(rev_static_reads),
        "member_reads": member_reads,
        "n_member_reads": len(member_reads),
        "regswap_rows": regswap_rows,
        "reloc_rows": reloc_rows,
        "other_rows": other_rows,
        "rev_ops": rev_ops,
        "rev_halfword": ("lhz" in rev_ops),
    }
    return rec


def confirm_and_score(rec, pct, require_member=True):
    """Decide verdict + confidence + est. strict gain from a signature record."""
    has_rev = rec["n_rev_static"] >= 1
    has_member = rec["n_member_reads"] >= 1
    confirmed = has_rev and (has_member or not require_member)

    # residual purity: are the ONLY remaining mismatches the rev-static rows,
    # the shifted-member rows, and the r30<->r31 regalloc swap? If so, flipping
    # the static to a per-TU sXxxRev should take the fn to strict-100.
    other = rec["other_rows"]
    if not confirmed:
        verdict, conf, est = "NO_SIGNATURE", "n/a", 0.0
    elif other == 0:
        verdict, conf = "CONFIRMED", "high"
        est = 1.0
    elif other <= 2:
        verdict, conf = "CONFIRMED", "medium"
        est = 1.0
    else:
        verdict, conf = "CONFIRMED", "low"
        est = 0.5  # rev static is present but other residuals remain
    rec["verdict"] = verdict
    rec["confidence"] = conf
    rec["est_strict_gain"] = est
    return rec


def load_population(report_path, min_pct, keep_collapse):
    r = json.load(open(report_path))
    pop = []
    for u in r.get("units", []):
        bn = u["name"].split("/")[-1]
        for f in (u.get("functions") or []):
            nm = f["name"]
            if "BinStreamRev" not in nm:
                continue
            pct = f.get("fuzzy_match_percent")
            if pct is None:
                pct = f.get("match_percent_normalized", 0.0)
            if pct >= 99.995:
                continue
            is_collapse = bool(COLLAPSE_NAME_RE.search(nm)) and pct < min_pct
            if is_collapse and not keep_collapse:
                continue
            pop.append({
                "unit": bn,
                "sym": nm,
                "pct": round(pct, 2),
                "size": int(f.get("size") or 0),
                "demangled": (f.get("metadata") or {}).get("demangled_name", ""),
                "lane": WAVE1_LANES.get(bn),
                "is_collapse": is_collapse,
            })
    pop.sort(key=lambda e: -(100.0 - e["pct"]) / 100.0 * e["size"])
    return pop


def scan_one(entry, project_dir, require_member):
    sym = entry["sym"]
    try:
        jp = diff_inspect.run_objdiff_for_symbol(
            sym, project_dir=project_dir, unit="default/" + entry["unit"])
    except SystemExit:
        entry["scan_status"] = "objdiff_failed"
        entry["verdict"] = "ERROR"
        return entry
    try:
        instrs = json.load(open(jp)).get("instructions", [])
    except Exception as e:  # noqa: BLE001
        entry["scan_status"] = f"json_error:{e}"
        entry["verdict"] = "ERROR"
        return entry
    rec = classify_function(instrs)
    confirm_and_score(rec, entry["pct"], require_member=require_member)
    entry.update(rec)
    entry["scan_status"] = "ok"
    return entry


def print_table(records):
    print(f"\n{'UNIT':17} {'%':>6} {'sz':>4} {'wave1':>5} "
          f"{'verdict':11} {'conf':6} {'rev@':16} {'mem':10} est")
    for e in records:
        if e.get("verdict") in (None, "NO_SIGNATURE", "ERROR"):
            continue
        rev = ",".join(a.replace("lbl_", "") for a in e.get("rev_static_addrs", []))
        mem = ",".join(sorted(set(e.get("member_reads", []))))
        lane = e.get("lane") or "NEW"
        print(f"{e['unit']:17} {e['pct']:6} {e['size']:4} {lane:>5} "
              f"{e['verdict']:11} {e.get('confidence',''):6} {rev:16} "
              f"{mem:10} +{e.get('est_strict_gain',0)}")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--sym", help="Scan a single mangled symbol")
    ap.add_argument("--project-dir", default=REPO,
                    help="Buildable project dir for objdiff (default: repo root)")
    ap.add_argument("--report", default=DEFAULT_REPORT)
    ap.add_argument("--asm-dir", default=DEFAULT_ASM)
    ap.add_argument("-o", "--out", help="Output JSON path")
    ap.add_argument("--limit", type=int)
    ap.add_argument("--resume", action="store_true")
    ap.add_argument("--min-pct", type=float, default=5.0,
                    help="Drop <min-pct collapse-template ops (default 5.0)")
    ap.add_argument("--keep-collapse", action="store_true")
    ap.add_argument("--no-require-member", action="store_true",
                    help="Confirm on target rev-static alone (looser)")
    ap.add_argument("--enumerate-only", action="store_true",
                    help="List candidate population without running objdiff")
    args = ap.parse_args()
    require_member = not args.no_require_member

    if args.sym:
        entry = {"unit": "?", "sym": args.sym, "pct": 0.0, "size": 0,
                 "lane": None, "is_collapse": False}
        # try to locate unit/pct from report
        try:
            r = json.load(open(args.report))
            for u in r.get("units", []):
                for f in (u.get("functions") or []):
                    if f["name"] == args.sym:
                        entry["unit"] = u["name"].split("/")[-1]
                        entry["pct"] = round(f.get("fuzzy_match_percent") or 0, 2)
                        entry["size"] = int(f.get("size") or 0)
                        entry["lane"] = WAVE1_LANES.get(entry["unit"])
        except Exception:  # noqa: BLE001
            pass
        scan_one(entry, args.project_dir, require_member)
        print(json.dumps(entry, indent=2))
        return

    pop = load_population(args.report, args.min_pct, args.keep_collapse)
    print(f"Candidate population (sub-100 BinStreamRev-named): {len(pop)}",
          file=sys.stderr)
    if args.enumerate_only:
        for e in pop:
            print(f"{e['unit']:17} {e['pct']:6} sz{e['size']:4} "
                  f"lane={e['lane'] or 'NEW':3} {e['sym'][:60]}")
        return

    done = {}
    if args.resume and args.out and os.path.exists(args.out):
        prev = json.load(open(args.out))
        for rec in prev.get("records", []):
            done[rec["sym"]] = rec

    records = []
    n = 0
    for e in pop:
        if args.limit and n >= args.limit:
            records.append(e)
            continue
        if e["sym"] in done and done[e["sym"]].get("scan_status") == "ok":
            records.append(done[e["sym"]])
            continue
        print(f"[{n+1}/{len(pop)}] {e['unit']} {e['pct']}% {e['sym'][:50]}",
              file=sys.stderr)
        scan_one(e, args.project_dir, require_member)
        records.append(e)
        n += 1

    # rank: confirmed first, then by (confidence, est gain, deficit*size)
    conf_rank = {"high": 0, "medium": 1, "low": 2, "n/a": 3, "": 4}
    records.sort(key=lambda e: (
        0 if e.get("verdict") == "CONFIRMED" else 1,
        conf_rank.get(e.get("confidence", ""), 5),
        -(e.get("est_strict_gain") or 0),
        -(100.0 - e["pct"]) / 100.0 * e["size"],
    ))

    confirmed = [e for e in records if e.get("verdict") == "CONFIRMED"]
    new_confirmed = [e for e in confirmed if not e.get("lane")]
    summary = {
        "population": len(pop),
        "scanned": n,
        "confirmed": len(confirmed),
        "confirmed_new_tus": sorted({e["unit"] for e in new_confirmed}),
        "confirmed_high": len([e for e in confirmed
                               if e.get("confidence") == "high"]),
        "est_strict_total": round(sum(e.get("est_strict_gain") or 0
                                      for e in confirmed), 1),
        "sanity_lightpreset": any(
            e["unit"] == "LightPreset" and "SpotlightDrawerEntry" in e["sym"]
            and e.get("verdict") == "CONFIRMED" for e in records),
        "sanity_eventtrigger": any(
            e["unit"] == "EventTrigger" and e.get("verdict") == "CONFIRMED"
            for e in records),
    }
    print_table(records)
    print(f"\nSUMMARY: {json.dumps(summary, indent=2)}")
    if args.out:
        os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
        json.dump({"records": records, "summary": summary},
                  open(args.out, "w"), indent=2)
        print(f"\nWrote {args.out}", file=sys.stderr)


# --- selftest ──────────────────────────────────────────────────────────────
# EVERY fixture is a verbatim instruction side from live `objdiff-cli diff
# -f json --include-instructions` (4.2.3, 0fd82159607c) over rb3-xenon
# build/45410914 objects. Re-capture from the CLI rather than hand-editing:
# hand-authored fixtures are precisely what let a dead parser stay green.
_ST_REV_VISIBLE = {"opcode": "lwz", "args": "r11, lbl_82CC79AC@l(r30)",
                   "typed_args": [
                       {"type": "Register", "value": "r11"},
                       {"type": "Symbol", "value": "lbl_82CC79AC"},
                       {"type": "Register", "value": "r30"}]}
# Same rev-static read, but here the relocation is the one 4.2 stopped
# printing -- `args` is a bare `0x4(r26)` and the lbl_82CB.... token exists
# only in typed_args. This is the 40% of the rev half that went missing.
_ST_REV_HIDDEN = {"opcode": "lhz", "args": "r11, 0x4(r26)", "typed_args": [
    {"type": "Register", "value": "r11"}, {"type": "Signed", "value": 4},
    {"type": "Register", "value": "r26"},
    {"type": "Symbol", "value": "lbl_82CBD98C"}]}
# Shifted BinStreamRev member read (stream@0x14) -- MEMBER_RE's whole job.
_ST_MEMBER = {"opcode": "lwz", "args": "r11, 0x14(r31)", "typed_args": [
    {"type": "Register", "value": "r11"}, {"type": "Signed", "value": 20},
    {"type": "Register", "value": "r31"}]}
_ST_PLAIN = {"opcode": "lwz", "args": "r10, 0x0(r11)", "typed_args": [
    {"type": "Register", "value": "r10"}, {"type": "Signed", "value": 0},
    {"type": "Register", "value": "r11"}]}


def _selftest():
    def rev(s):
        return bool(REV_STATIC_RE.search(_flat_args(s)))

    def member(s):
        return bool(MEMBER_RE.search(_flat_args(s)))

    checks = [
        ("rev read, relocation printed", rev(_ST_REV_VISIBLE), True),
        ("rev read, relocation HIDDEN", rev(_ST_REV_HIDDEN), True),
        ("plain member load is not a rev read", rev(_ST_MEMBER), False),
        ("member read at 0x14", member(_ST_MEMBER), True),
        ("offset 0x0 is not a member read", member(_ST_PLAIN), False),
        # _is_r30_r31_swap stays on the DISPLAY string: it is a textual
        # substitution test and both spellings are symmetric under it.
        ("r30<->r31 swap on the display spelling",
         _is_r30_r31_swap({"opcode": "lwz", "args": "r11, 0x14(r31)"},
                          {"opcode": "lwz", "args": "r11, 0x14(r30)"}), True),
    ]
    bad = 0
    for name, got, want in checks:
        if got != want:
            bad += 1
            print(f"FAIL {name}: want {want!r}, got {got!r}")
    print(f"selftest: {len(checks) - bad}/{len(checks)} passed")
    return 1 if bad else 0


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        sys.exit(_selftest())
    main()
