#!/usr/bin/env python3
"""static_symbol_finder — worklist generator for two GAME-layer near-miss levers.

Motivation (wave-2 batch-2, Gem +8 / GuitarController +4, commit e4180d4):
two distinct retail-vs-ours divergences were discovered to be *systematic*, not
per-function, so a worklist of every remaining instance is worth more than a
hand-grind. This tool finds both.

LEVER A — STATIC_SYMBOL_GUARD (the Gem +8 pattern)
--------------------------------------------------
Retail RB3 declares widget/message Symbols as FUNCTION-LOCAL statics with
guard-bit lazy init:

    void Foo::Bar() {
        static Symbol enter_msg("enter");   // guard-bit-gated, atexit-registered
        ...
    }

Our tree resolves the *same* names against a Symbols-header extern, so for the
unconverted units objdiff shows TARGET-ONLY (delete) blocks of the shape:

    lis     rX, <guardword>            ; load guard word
    lwz     r11, <guardword>, rX
    clrlwi. r9, r11, 31                ; test guard bit 0   (or rlwinm. ...,29,29 = bit 2)
    ori     r11, r11, 0x1              ; set guard bit (0x1/0x2/0x4/0x8... decl order)
    stw     r11, <guardword>, rX
    addi    r4, r11, <"enter">         ; the C-string literal
    bl      ??0Symbol@@QAA@PBD@Z       ; Symbol(const char*) ctor
    ...
    bl      atexit                     ; register the dtor

Our compiled side lacks the whole block (it just loads the extern). FIX = port
the source to the function-local-static form; the block then appears EQUAL on
both sides. Detection is therefore: count delete-side ??0Symbol ctor calls that
co-occur with a delete-side guard-ori, where the base side does NOT carry a
matching insert-side ctor.

LEVER B — MESSAGE_TIMER (the BEGIN_HANDLERS MILO_DEBUG instrumentation pattern)
------------------------------------------------------------------------------
src/system/macros.h force-defines MILO_DEBUG tree-wide, so every BEGIN_HANDLERS
expansion (obj/ObjMacros.h:74) emits an inline MessageTimer:

    MessageTimer timer(MessageTimer::Active() ? this : 0, sym);  // mTimer.Restart()
    ...                                                          // ~ : AddTime(.., SplitMs())

Retail compiled these TUs with MILO_DEBUG *off*, so the block is absent. The
divergence shows as OUR-side (insert) instructions referencing the inlined
MessageTimer members — ?Restart@Timer@@ / ?SplitMs@Timer@@ / ?Active@MessageTimer@@ /
?AddTime@MessageTimer@@ (and the sActive static) — that the target lacks. FIX is a
per-TU `#undef MILO_DEBUG` (a force-multiplier as more Handle-bearing TUs pin);
the global flip is a layout-coupled dedicated wave.

USAGE
-----
  tools/static_symbol_finder.py                 # scan [40,99.99] named pool, both levers
  tools/static_symbol_finder.py --lo 40 --hi 99.99
  tools/static_symbol_finder.py --limit 40      # cap (debug)
  tools/static_symbol_finder.py --only-symbol '?UpdateState@OvershellSlot@@QAAXXZ'
  tools/static_symbol_finder.py --out ~/tmp/symworklist.json --doc docs/.../worklist.md

Judge a lever ONLY by a whole-binary A/B (report.json matched_functions); this
tool RANKS candidates, it does not measure delta.
"""
import sys, os, json, re, subprocess, argparse, hashlib
from collections import Counter, defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPORT = os.path.join(ROOT, 'build/45410914/report.json')
CLI = os.path.join(ROOT, 'bin', 'objdiff-cli')
CACHE_DIR = '/tmp/claude/symfinder'

# ── signature tokens ─────────────────────────────────────────────────────────
SYMBOL_CTOR_RE = re.compile(r'0Symbol@@Q[AE]A')          # ??0Symbol@@QAA@PBD@Z (32) / QEAA (64)
# guard-bit SET: ori rX,rY,0xPOW2 where the immediate is a single low bit
GUARD_ORI_IMMS = {0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80}
# guard-bit TEST opcodes (record-form), reading a single bit out of the guard word.
# A genuine guard test isolates ONE bit: `clrlwi. rX,rY,31` (keep low bit) or
# `rlwinm. rX,rY,0,N,N` (single-bit mask, MB==ME). `clrlwi. ...,24` is a byte/bool
# mask and is NOT a guard test — exclude it to keep the n_guard_test metric honest.
GUARD_TEST_OPS = {'clrlwi.', 'rlwinm.', 'andi.', 'extrwi.'}


def is_guard_test(side):
    """True iff this record-form instruction isolates a single bit of a word."""
    if not side:
        return False
    op = side.get('opcode', '')
    args = side.get('args', '')
    if op == 'clrlwi.':
        # clrlwi. rX,rY,31 keeps only the low (guard) bit
        return args.rstrip().endswith(', 31') or args.rstrip().endswith(',31')
    if op == 'rlwinm.':
        # rlwinm. rX,rY,0,N,N  (rotate 0, mask begin==end => one bit)
        m = re.search(r',\s*0,\s*(\d+),\s*(\d+)\s*$', args)
        return bool(m) and m.group(1) == m.group(2)
    if op in ('andi.', 'extrwi.'):
        return True
    return False

# MessageTimer / Timer inline members emitted by BEGIN_HANDLERS under MILO_DEBUG
TIMER_RE = re.compile(
    r'(?:Restart@Timer@@|SplitMs@Timer@@|Active@MessageTimer@@|AddTime@MessageTimer@@'
    r'|sActive@MessageTimer@@|0MessageTimer@@|1MessageTimer@@)')


def syms_of(side):
    """Mangled Symbol/BranchDest token values referenced by one side, if any."""
    if not side:
        return []
    out = []
    for a in side.get('typed_args', []):
        if a.get('type') in ('Symbol', 'BranchDest'):
            v = a.get('value')
            if isinstance(v, str):
                out.append(v)
    return out


def imm_of(side):
    """First Signed/Unsigned immediate value of a side (for ori guard-bit imm)."""
    if not side:
        return None
    for a in side.get('typed_args', []):
        if a.get('type') in ('Signed', 'Unsigned'):
            v = a.get('value')
            if isinstance(v, int):
                return v
    return None


def fmt(side):
    if not side:
        return '---'
    return f"{side.get('opcode',''):10s} {side.get('args','')}".rstrip()



def _ensure_patched(project_dir):
    """Build through `post-compile` and ASSERT -- never `--build` one object.

    Memoized per tree per process; raises UnpatchedTreeError rather than
    returning a number taken from a partially-patched tree.
    """
    import sys as _sys, os as _os
    _scripts = _os.path.join(
        _os.path.dirname(_os.path.dirname(_os.path.abspath(__file__))), 'scripts')
    if _scripts not in _sys.path:
        _sys.path.insert(0, _scripts)
    from orchestrator.patch_guard import ensure_patched_tree_once
    return ensure_patched_tree_once(project_dir)


def run_diff(symbol, no_build=False):
    """Run objdiff-cli diff for one (globally-unique mangled) symbol; cache JSON.

    Returns parsed dict or None.

    NO `--build --incremental`. That pair reads as an optimisation ("rebuild a
    stale unit obj, reuse a warm one") but it is `ninja <one .obj>`, a
    single-object target that stops one edge short of rb3-xenon's six
    post-compile patchers -- so it answered from raw compiler output and left
    the object unpatched for report.json and every concurrent lane. Measured
    on rb3-xenon: one such call cost unit default/BandUI 2.006 pp of
    matched_code_percent and read a 100.0 function as 99.7.

    The tree is instead brought to the `post-compile` fixed point once per
    process and ASSERTED; `no_build` still skips even that, for callers that
    deliberately want a read-only look at whatever is on disk."""
    if not no_build:
        _ensure_patched(ROOT)
    os.makedirs(CACHE_DIR, exist_ok=True)
    h = hashlib.md5(symbol.encode()).hexdigest()[:12]
    out = os.path.join(CACHE_DIR, f'd_{h}.json')
    if os.path.exists(out) and os.path.getsize(out) > 0:
        try:
            return json.load(open(out))
        except Exception:
            pass
    cmd = [CLI, 'diff', '-p', ROOT, symbol,
           '--include-instructions', '-f', 'json', '-o', out]
    try:
        r = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=600)
    except subprocess.TimeoutExpired:
        return None
    if not os.path.exists(out):
        return None
    try:
        return json.load(open(out))
    except Exception:
        return None


def detect_static_symbol(instructions):
    """LEVER A: target-only Symbol-ctor + guard-bit blocks our side lacks.

    Returns dict(n_symbols, n_guard_ori, n_guard_test, ctor_strings[],
    guard_words[], evidence_lines[]) or None if the pattern is absent.
    Only counts a ctor as 'still-needs-fix' when the base side does NOT carry a
    matching insert-side ??0Symbol ctor (i.e. retail builds a Symbol we don't)."""
    del_ctor = 0          # target-only Symbol ctor calls
    ins_ctor = 0          # base-only (our) Symbol ctor calls
    del_guard_ori = 0
    del_guard_test = 0
    ctor_strings = []     # C-string literal symbols feeding the ctor (the Symbol name)
    guard_words = set()
    ev = []
    n = len(instructions)
    for k, i in enumerate(instructions):
        mt = i.get('match_type')
        t = i.get('target')
        b = i.get('base')
        # base-only (insert) Symbol ctor — proof we already emit it
        if mt == 'insert':
            for s in syms_of(b):
                if SYMBOL_CTOR_RE.search(s):
                    ins_ctor += 1
        if mt != 'delete' or not t:
            continue
        op = t.get('opcode', '')
        # guard-bit TEST (single-bit isolate only)
        if is_guard_test(t):
            del_guard_test += 1
        # guard-bit SET: ori rX,rX,<single-bit imm>
        if op == 'ori':
            imm = imm_of(t)
            if imm in GUARD_ORI_IMMS:
                del_guard_ori += 1
                # the stw immediately after often writes a guard word lbl_
                for look in instructions[k:k + 2]:
                    for s in syms_of(look.get('target')):
                        if s.startswith('lbl_'):
                            guard_words.add(s)
        # target-only Symbol ctor
        for s in syms_of(t):
            if SYMBOL_CTOR_RE.search(s):
                del_ctor += 1
                # gather the preceding `addi r4,rX,<name>` C-string operand: a
                # mangled ??_C@ literal OR an unnamed lbl_ data label (the string
                # pool address). r4 is the const char* ctor arg, so restrict to
                # the addi whose destination register is r4.
                for look in instructions[max(0, k - 4):k]:
                    lt = look.get('target')
                    if not lt or lt.get('opcode') != 'addi':
                        continue
                    ta = lt.get('typed_args', [])
                    dest = ta[0].get('value') if ta and ta[0].get('type') == 'Register' else None
                    if dest != 'r4':
                        continue
                    for cs in syms_of(lt):
                        if '??_C@' in cs or cs.startswith('lbl_'):
                            ctor_strings.append(cs)
                # capture the FULL delete block for evidence: walk back to the
                # nearest guard-TEST (the start of the lazy-init prologue) and
                # forward to the atexit registration, recording target lines.
                if len(ev) < 1:   # quote only the first block (keeps doc compact)
                    lo = k
                    for j in range(k - 1, max(0, k - 12), -1):
                        jt = instructions[j].get('target')
                        if instructions[j].get('match_type') == 'delete' \
                                and is_guard_test(jt):
                            lo = j
                            break
                        lo = j
                    hi = k
                    for j in range(k + 1, min(n, k + 10)):
                        jt = instructions[j].get('target')
                        if instructions[j].get('match_type') != 'delete':
                            continue
                        hi = j
                        if jt and 'atexit' in (jt.get('args', '') or ''):
                            break
                    for j in range(lo, hi + 1):
                        ji = instructions[j]
                        if ji.get('match_type') == 'delete' and ji.get('target'):
                            ev.append(f"  [{ji['index']:4d}] DEL {fmt(ji.get('target'))}")
    if del_ctor == 0:
        return None
    # require the guard machinery to co-occur (distinguishes from plain
    # extern-vs-named ctor noise): at least one guard-ori OR guard-test delete.
    if del_guard_ori == 0 and del_guard_test == 0:
        return None
    # net = (target-only ctors) - (our extra ctors). Negative net means our side
    # constructs MORE Symbols than retail lacks here — a TWO-SIDED divergence
    # (e.g. OvershellSlot: retail also DROPS some of our Symbols), which is a
    # layout/logic wall rather than a clean one-way port. Flag it.
    net = del_ctor - ins_ctor
    return {
        'lever': 'STATIC_SYMBOL_GUARD',
        'n_symbols': del_ctor,         # target-only Symbol ctors = conversions needed
        'n_symbols_net': net,
        'n_guard_ori': del_guard_ori,
        'n_guard_test': del_guard_test,
        'base_ctor': ins_ctor,
        'two_sided': ins_ctor > 0,     # our side also emits ctors retail may drop
        'ctor_strings': sorted(set(ctor_strings)),
        'guard_words': sorted(guard_words),
        'evidence_lines': ev[:18],
    }


def detect_message_timer(instructions):
    """LEVER B: our-side (insert) MessageTimer/Timer inline block target lacks.

    Returns dict or None. Signature = base-only instructions referencing the
    inlined MessageTimer members, with NO matching target-side reference."""
    ins_hits = []   # (index, opcode, sym)
    del_hits = 0
    for i in instructions:
        mt = i.get('match_type')
        if mt == 'insert':
            b = i.get('base')
            for s in syms_of(b):
                if TIMER_RE.search(s):
                    ins_hits.append((i['index'], (b or {}).get('opcode', ''), s))
        elif mt in ('delete', 'equal', 'diff_arg', 'replace'):
            t = i.get('target')
            for s in syms_of(t):
                if TIMER_RE.search(s):
                    del_hits += 1
    if not ins_hits:
        return None
    # if the target ALSO references the timer members, the block is present on
    # both sides (already matched / not the MILO_DEBUG divergence) — skip.
    if del_hits >= len(ins_hits):
        return None
    ev = [f"  [{ix:4d}] INS our-side {op} {s}" for ix, op, s in ins_hits[:8]]
    distinct = sorted(set(s for _, _, s in ins_hits))
    return {
        'lever': 'MESSAGE_TIMER',
        'n_timer_refs': len(ins_hits),
        'distinct_refs': distinct,
        'target_refs': del_hits,
        'evidence_lines': ev,
    }


def load_targets(report_path, lo, hi, only_symbol=None):
    rep = json.load(open(report_path))
    out = []
    for unit in rep['units']:
        un = unit['name']
        for f in unit.get('functions', []):
            mp = f.get('match_percent_normalized', 0.0)
            nm = f.get('name', '')
            if only_symbol:
                if nm == only_symbol:
                    out.append((un, nm, mp, int(f.get('size', 0))))
                continue
            if nm.startswith('fn_'):
                continue
            if lo <= mp < hi:
                out.append((un, nm, mp, int(f.get('size', 0))))
    # largest first: bigger functions = more potential Symbol inits
    out.sort(key=lambda t: -t[3])
    return out, rep['measures']['matched_functions']


def main():
    ap = argparse.ArgumentParser(
        description='Worklist generator for the static-Symbol-guard (Gem +8) and '
                    'BEGIN_HANDLERS MessageTimer game levers.')
    ap.add_argument('--lo', type=float, default=40.0,
                    help='Lower bound (inclusive) match-percent band (default 40)')
    ap.add_argument('--hi', type=float, default=99.99,
                    help='Upper bound (exclusive) match-percent band (default 99.99)')
    ap.add_argument('--limit', type=int, default=0, help='Cap fns processed (0=all)')
    ap.add_argument('--report', default=REPORT)
    ap.add_argument('--only-symbol', default=None,
                    help='Process a single mangled symbol (validation / debug)')
    ap.add_argument('--no-build', action='store_true',
                    help='Do not rebuild objs (assume a warm full build exists)')
    ap.add_argument('--out', default='/tmp/static_symbol_worklist.json',
                    help='Ranked JSON worklist output path')
    ap.add_argument('--doc', default=None,
                    help='Optional markdown summary output path')
    a = ap.parse_args()

    targets, matched = load_targets(a.report, a.lo, a.hi, a.only_symbol)
    if a.limit:
        targets = targets[:a.limit]
    print(f"[static_symbol_finder] scanning {len(targets)} named fns in "
          f"[{a.lo},{a.hi})  (official matched={matched})", file=sys.stderr)

    static_rows = []
    timer_rows = []
    fail = 0
    for idx, (un, sym, mp, sz) in enumerate(targets):
        if idx and idx % 50 == 0:
            print(f"  {idx}/{len(targets)} "
                  f"(static={len(static_rows)} timer={len(timer_rows)} fail={fail})",
                  file=sys.stderr)
        d = run_diff(sym, no_build=a.no_build)
        if not d:
            fail += 1
            continue
        instrs = d.get('instructions', [])
        if not instrs:
            continue
        sd = detect_static_symbol(instrs)
        if sd:
            sd.update({'symbol': sym, 'unit': un, 'pct': round(mp, 3), 'size': sz})
            static_rows.append(sd)
        td = detect_message_timer(instrs)
        if td:
            td.update({'symbol': sym, 'unit': un, 'pct': round(mp, 3), 'size': sz})
            timer_rows.append(td)

    # rank static by: clean one-way ports first (not two_sided), then by number
    # of target-only Symbol conversions, then lower pct (more headroom). Two-sided
    # rows (our side also emits ctors retail drops = layout/logic wall) sink.
    static_rows.sort(key=lambda r: (r['two_sided'], -r['n_symbols'], r['pct']))
    timer_rows.sort(key=lambda r: (-r['n_timer_refs'], r['pct']))

    payload = {
        'band': [a.lo, a.hi],
        'scanned': len(targets),
        'diff_fail': fail,
        'static_symbol_guard': static_rows,
        'message_timer': timer_rows,
    }
    with open(os.path.expanduser(a.out), 'w') as fh:
        json.dump(payload, fh, indent=2)

    # ── human summary ────────────────────────────────────────────────────────
    print(f"\n=== STATIC_SYMBOL_GUARD worklist  ({len(static_rows)} fns) ===")
    by_unit = defaultdict(lambda: [0, 0])
    for r in static_rows:
        by_unit[r['unit']][0] += 1
        by_unit[r['unit']][1] += r['n_symbols']
    clean = [r for r in static_rows if not r['two_sided']]
    print(f"  ({len(clean)} clean one-way / {len(static_rows)-len(clean)} two-sided walls)")
    print(f"{'pct':>7} {'nsym':>4} {'gORI':>4} {'2sided':>6}  unit / symbol")
    for r in static_rows[:40]:
        print(f"{r['pct']:7.2f} {r['n_symbols']:4d} {r['n_guard_ori']:4d} "
              f"{str(r['two_sided']):>6}  {r['unit']}  {r['symbol'][:48]}")
    print(f"\n  units with target-only static-Symbol blocks "
          f"({len(by_unit)}): " +
          ", ".join(f"{u.split('/')[-1]}({n[1]})"
                    for u, n in sorted(by_unit.items(), key=lambda x: -x[1][1])[:15]))

    print(f"\n=== MESSAGE_TIMER worklist  ({len(timer_rows)} fns) ===")
    print(f"{'pct':>7} {'refs':>4}  unit / symbol")
    for r in timer_rows[:30]:
        print(f"{r['pct']:7.2f} {r['n_timer_refs']:4d}  {r['unit']}  {r['symbol'][:48]}")
    tunits = sorted(set(r['unit'] for r in timer_rows))
    print(f"\n  units with our-side MessageTimer blocks ({len(tunits)}): " +
          ", ".join(u.split('/')[-1] for u in tunits[:20]))
    print(f"\nworklist -> {os.path.expanduser(a.out)}", file=sys.stderr)

    if a.doc:
        write_doc(os.path.expanduser(a.doc), payload, matched)


def write_doc(path, payload, matched):
    lo, hi = payload['band']
    sr = payload['static_symbol_guard']
    tr = payload['message_timer']
    L = []
    L.append("# Static-Symbol-guard + MessageTimer worklist")
    L.append("")
    L.append(f"Generated by `tools/static_symbol_finder.py` over the "
             f"named near-miss pool `[{lo}, {hi})` of "
             f"`build/45410914/report.json` (official matched = {matched}).")
    L.append("")
    L.append(f"- scanned: {payload['scanned']} named fns")
    L.append(f"- diff failures: {payload['diff_fail']}")
    L.append(f"- STATIC_SYMBOL_GUARD candidates: {len(sr)}")
    L.append(f"- MESSAGE_TIMER candidates: {len(tr)}")
    L.append("")
    L.append("## Lever A — STATIC_SYMBOL_GUARD (the Gem +8 pattern)")
    L.append("")
    L.append("Retail uses `static Symbol x(\"x\")` function-local statics "
             "(guard-bit lazy init + atexit); our tree resolves the same names "
             "as Symbols-header externs. FIX = port the source to the "
             "function-local-static form per flagged function.")
    L.append("")
    if sr:
        L.append("`two_sided`=our side also emits Symbol ctors retail drops "
                 "(layout/logic wall, not a clean one-way port).")
        L.append("")
        L.append("| pct | n_sym | guard_ori | two_sided | unit | symbol |")
        L.append("|----:|------:|----------:|:---------:|------|--------|")
        for r in sr[:60]:
            L.append(f"| {r['pct']:.2f} | {r['n_symbols']} | {r['n_guard_ori']} "
                     f"| {'YES' if r['two_sided'] else '-'} "
                     f"| `{r['unit']}` | `{r['symbol']}` |")
        L.append("")
        # quote evidence for up to the first 3 candidates (verified true positives)
        for top in sr[:3]:
            L.append(f"### Evidence — `{top['symbol']}` "
                     f"({top['unit']}, {top['pct']}%)"
                     + ("  [TWO-SIDED WALL]" if top['two_sided'] else "  [clean one-way]"))
            L.append("")
            L.append(f"{top['n_symbols']} target-only `??0Symbol@@QAA@PBD@Z` ctor "
                     f"calls, {top['n_guard_ori']} guard-bit `ori` sets, "
                     f"{top['n_guard_test']} guard tests; "
                     f"C-string pool: {', '.join('`'+c+'`' for c in top['ctor_strings'][:6])}")
            L.append("")
            L.append("```")
            L.extend(top['evidence_lines'])
            L.append("```")
            L.append("")
    else:
        L.append("_No STATIC_SYMBOL_GUARD candidates in this band — pool empty._")
    L.append("")
    L.append("### Apply path (root cause, verified)")
    L.append("")
    L.append("The flagged functions call `Handle(<msg>_msg, ...)` (or build a "
             "`Symbol`) where `<msg>_msg` resolves to a **Symbols-header extern** "
             "(`extern Message <msg>;` in e.g. `src/system/utl/Messages*.h`). "
             "Retail compiled these as **function-local statics**: each function "
             "that uses the message gets its own `static Symbol <msg>(\"<msg>\")` "
             "with a guard bit (shared guard word, distinct bit in declaration "
             "order — the `ori 0x1`/`0x2`/`0x4` evidence). To fix a flagged "
             "function: replace the extern reference inside that function body "
             "with a local `static Symbol`. Verified on the clean top candidate: "
             "`Player::SetEnergy` calls `Handle(send_update_energy_msg, true)` and "
             "`send_update_energy_msg` is `extern Message` at "
             "`src/system/utl/Messages4.h:6`; retail builds a local static there "
             "(delete-block idx 54–68 above). `two_sided` rows additionally need "
             "the OTHER divergence resolved (a layout/logic wall) and will not "
             "reach 100% from the static conversion alone.")
    L.append("")
    L.append("## Lever B — MESSAGE_TIMER (BEGIN_HANDLERS MILO_DEBUG instrumentation)")
    L.append("")
    L.append("`src/system/macros.h` force-defines `MILO_DEBUG` tree-wide, so every "
             "`BEGIN_HANDLERS` emits an inline `MessageTimer` block retail "
             "(MILO_DEBUG-off) lacks. FIX = per-TU `#undef MILO_DEBUG` (a "
             "force-multiplier; global flip = layout-coupled dedicated wave).")
    L.append("")
    if tr:
        L.append("| pct | timer_refs | unit | symbol |")
        L.append("|----:|-----------:|------|--------|")
        for r in tr[:60]:
            L.append(f"| {r['pct']:.2f} | {r['n_timer_refs']} | `{r['unit']}` "
                     f"| `{r['symbol']}` |")
        L.append("")
        top = tr[0]
        L.append(f"### Top candidate evidence — `{top['symbol']}` "
                 f"({top['unit']}, {top['pct']}%)")
        L.append("")
        L.append(f"{top['n_timer_refs']} our-side MessageTimer/Timer refs "
                 f"(target has {top['target_refs']}); distinct: "
                 f"{', '.join('`'+c+'`' for c in top['distinct_refs'][:6])}")
        L.append("")
        L.append("```")
        L.extend(top['evidence_lines'])
        L.append("```")
    else:
        L.append("_No MESSAGE_TIMER candidates in this band — pool empty "
                 "(the `::Handle`/BEGIN_HANDLERS instrumentation lever is "
                 "already drained across the current named near-miss pool; "
                 "GuitarController::Handle was converted in batch 2)._")
    L.append("")
    L.append("## Limitations")
    L.append("")
    L.append("- Scope is the **named** `[lo,hi)` pool; anonymous `fn_` near-misses "
             "that carry the pattern are not scanned (re-run with a different band "
             "or extend `load_targets` to include `fn_`).")
    L.append("- `two_sided` rows have a *second* divergence (our side emits Symbol "
             "ctors retail dropped — e.g. OvershellSlot's `go_to_wiiprofilecreator` "
             "block). Converting the static-Symbol half alone will NOT reach 100% "
             "(those are layout/logic walls per the roadmap); the clean one-way "
             "rows (`two_sided=False`) are the high-EV port targets.")
    L.append("- Detection requires guard machinery (`clrlwi.`/`rlwinm.`/`andi.` test "
             "OR `ori rX,rX,0x{1,2,4,8...}` set) to co-occur with a delete-side "
             "`??0Symbol` ctor; a one-off Symbol temporary without guard bits is "
             "intentionally NOT flagged (it is not the function-local-static lever).")
    L.append("- diff failures (ambiguous/ICF-folded mangled names, STLport template "
             "instantiations) are counted, not silently dropped.")
    L.append("- The match% is `match_percent_normalized` from report.json; judge any "
             "actual landed fix by a whole-binary A/B, not this tool's ranking.")
    L.append("")
    with open(path, 'w') as fh:
        fh.write("\n".join(L) + "\n")
    print(f"doc -> {path}", file=sys.stderr)


if __name__ == '__main__':
    main()
