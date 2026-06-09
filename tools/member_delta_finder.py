#!/usr/bin/env python3
"""member_delta_finder.py — detect the DC3 DROPPED/ADDED-MEMBER force-multiplier.

THE PATTERN (proven wins: CharSleeve/CharIKSliderMidi mMe -0xC; Gem Tail -0x14;
postproc pad). DC3-decomp is *newer* than retail RB3. When DC3 ADDED a member to a
class (or RB3 DROPPED one), every this-relative member access in that class's
methods AT OR ABOVE the member's offset shifts by a uniform constant C = the
member's size. Below the offset, accesses match exactly. In objdiff this shows as:

    lfs f13, 0x1ec, r31   == 0x1ec, r31    (below threshold: MATCH, delta 0)
    lfs f12, 0x214, r31   vs 0x224, r31    (at/above:        delta -0x10)
    stfs f0, 0x218, r31   vs 0x228, r31    (                 delta -0x10)

target < base (negative C) => our compiled struct (base) is BIGGER than retail
(target) => DC3 added / we have an EXTRA member => REMOVE/shrink it.
target > base (positive C) => our struct is SMALLER => ADD a member.

The fix is a ONE-LINE header edit at the threshold offset that force-multiplies
across every method of the class.

DISTINGUISHING REAL MEMBER-SHIFTS FROM NOISE (the hard part):
  * REAL: base register is the THIS pointer (r3, or rN where `mr rN,r3` /
    `mr r31,r3` was seen in the prologue), the delta is CONSISTENT across many
    distinct offsets, and there's a clean THRESHOLD (matching accesses below,
    shifted above).
  * STACK NOISE (reject): base register is r1 (frame). Differing r1 displacements
    are regalloc / stack-slot reordering, NOT layout. (Verified: MD5::finalize's
    11 STRUCT_OFF are all off r1 with non-uniform deltas — pure noise.)
  * FUNCLET FRAME-RECON (reject): base register is r11/r12 reconstructing a parent
    frame (e.g. dtor unwind `subi r11,r11,N; addi r3,r11,M`). Non-this.
  * PARAM/LOCAL POINTER (report SEPARATELY): base register traces to a parameter
    (r4..r10 at entry) or a pointer loaded from `this`. A uniform delta here is a
    real member-shift but of ANOTHER class (the param's type), so the fix lives in
    a different header — flagged as `base_kind: param/derived`, lower confidence.
  * COUPLED-BASE / VBASE WALL (note, do NOT call clean): if a class shows MULTIPLE
    distinct deltas at different thresholds, or the delta only holds for a subset
    of methods, it's likely a multiple-inheritance / vbase relayout — deep, not a
    one-member fix. Flagged with a warning, ranked low.

Pipeline:
  1. Load STRUCT_WORK pool from /tmp/true_progress.json (bucket==STRUCT_WORK).
  2. objdiff each fn (worktree build, read-only). For every instruction (matching
     AND differing), parse this-relative mem/addi accesses, trace the base reg to
     this / param / frame / derived. Record (offset_target, delta) per fn.
  3. Per FUNCTION: find the dominant uniform delta C and its threshold T (lowest
     target offset where the shift begins); require below-T accesses to match.
  4. GROUP by CLASS (demangled class of the method, fallback unit). A class with a
     CONSISTENT (C, ~T) across multiple methods = a clean candidate. Rank by
     n_affected * consistency.
  5. Emit ~/tmp/forcemult/member_candidates.json ranked, with fix_hint + oracle
     cross-check pointers (DC3 / rb3-Wii / Ghidra) for member identity.

Usage:
  tools/member_delta_finder.py                 # full STRUCT_WORK pool
  tools/member_delta_finder.py --limit 40      # quick smoke
  tools/member_delta_finder.py --tp /tmp/true_progress.json --out ~/tmp/forcemult/member_candidates.json
"""
import sys, os, re, json, subprocess, argparse
from collections import defaultdict, Counter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLI = os.path.join(ROOT, 'bin', 'objdiff-cli')

# objdiff arg conventions seen in the JSON:
#   mem  load/store : "reg, disp, basereg"   e.g. "r11, 0x7c, r4"  => [r4 + 0x7c]
#   addi/subi       : "dst, src, imm"         e.g. "r10, r11, 0x64" => r10 = r11 + 0x64
#   mr              : "dst, src"
HEX_RE = re.compile(r'^-?0x[0-9A-Fa-f]+$|^-?\d+$')
REG_RE = re.compile(r'^[rf]\d+$')
MEM_LOADS_STORES = {
    'lwz','lbz','lhz','lha','lwzu','lbzu','lfs','lfd','lwa','ld','ldu',
    'stw','stb','sth','stwu','stfs','stfd','std','stdu','lhau',
}
ADDI = {'addi', 'addic', 'addic.'}
SUBI = {'subi'}
FRAME_REGS = {'r1'}                 # stack frame -> reject
FUNCLET_RECON = {'r11', 'r12'}      # parent-frame recon when not traced to this


def toi(s):
    s = s.strip()
    try:
        return int(s, 16) if s.lower().startswith(('0x', '-0x')) else int(s)
    except Exception:
        return None


def tokens(s):
    return [t.strip() for t in (s or '').split(',') if t.strip()]


def diff_fn(unit, sym, proj):
    """Run objdiff for one symbol; return parsed JSON or None."""
    tmp = f'/tmp/_mdf_{os.getpid()}.json'
    args = [CLI, 'diff', '-p', proj, sym, '-f', 'json', '-o', tmp,
            '--include-instructions']
    # prefer -u unit form when available (disambiguates ICF-shared names)
    args_u = [CLI, 'diff', '-p', proj, '-u', unit, sym, '-f', 'json', '-o', tmp,
              '--include-instructions']
    for a in (args_u, args):
        try:
            r = subprocess.run(a, capture_output=True, text=True, timeout=120)
            if os.path.exists(tmp):
                d = json.load(open(tmp))
                if d.get('instructions'):
                    return d
        except Exception:
            continue
    return None


def _is_store(op):
    return op in MEM_LOADS_STORES and op.startswith('st')


def _step_dataflow(x, this_regs, param_regs):
    """Update the LIVE this/param register sets after executing instruction x
    (target side). Returns nothing; mutates the sets in place.

    r3 is `this` on entry; `mr rDST, <this-reg>` propagates this-ness. r4..r10 on
    entry are params. ANY other write to a register (incl. `addi r31,...`,
    `lwz r3,...`, a `bl` clobbering r3..r12) kills that register's this/param
    status FROM THAT POINT ON. Crucially this is POSITIONAL: a reg can be `this`
    for the first half of a function and not the second (e.g. r31 reused for a
    global pointer after the member stores), so callers must classify each access
    with the set that is live AT that instruction — not a whole-function summary.
    """
    t = x.get('target') or {}
    op = t.get('opcode')
    tk = tokens(t.get('args') or '')
    if not op:
        return
    if op in ('bl', 'bla', 'bctrl', 'blrl'):
        # volatile call: clobbers r3..r12, f0..f13 (return + scratch). Kill any
        # this/param reg in that range.
        for s in list(this_regs):
            n = int(s[1:]) if s[1:].isdigit() else -1
            if s[0] == 'r' and 3 <= n <= 12:
                this_regs.discard(s)
        for s in list(param_regs):
            n = int(s[1:]) if s[1:].isdigit() else -1
            if s[0] == 'r' and 3 <= n <= 12:
                param_regs.discard(s)
        return
    if not tk:
        return
    dst = tk[0]
    if op == 'mr' and len(tk) >= 2:
        src = tk[1]
        if src in this_regs:
            this_regs.add(dst); param_regs.discard(dst)
        elif src in param_regs:
            param_regs.add(dst); this_regs.discard(dst)
        else:
            this_regs.discard(dst); param_regs.discard(dst)
        return
    if REG_RE.match(dst):
        # stores write memory, not the first-token register's meaning
        if _is_store(op):
            return
        this_regs.discard(dst); param_regs.discard(dst)


def base_kind(reg, this_regs, param_regs):
    if reg in this_regs:
        return 'this'
    if reg in FRAME_REGS:
        return 'frame'
    if reg in param_regs:
        return 'param'
    if reg in FUNCLET_RECON:
        return 'funclet'
    return 'derived'


def collect_accesses(d):
    """Return list of (kind, basereg, target_off, base_off, is_diff) for every
    this/param/derived-relative member access (mem load/store + addi/subi &member).
    Matching accesses included (delta 0) so we can find the threshold.

    Single forward pass with POSITIONAL dataflow: each access is classified with
    the this/param register set live at that instruction, THEN the instruction's
    effect on the sets is applied (so an access off r31 is `this` if r31 holds
    this *at* that access, even if r31 is later reused for something else)."""
    ins = d.get('instructions', [])
    this_regs = {'r3'}
    param_regs = {'r4', 'r5', 'r6', 'r7', 'r8', 'r9', 'r10'}
    out = []
    for x in ins:
        # classify this access with the register sets LIVE BEFORE this insn
        mt = x.get('match_type')
        t = x.get('target') or {}
        b = x.get('base') or {}
        op = t.get('opcode')
        bop = b.get('opcode')
        if op is None:
            continue
        tk = tokens(t.get('args') or '')
        bk = tokens(b.get('args') or '')
        is_diff = mt in ('diff_arg', 'replace', 'mismatch')
        # MEM form: "reg, disp, basereg"
        if op in MEM_LOADS_STORES and len(tk) == 3 and REG_RE.match(tk[2]):
            to = toi(tk[1]); breg = tk[2]
            bo = None
            if bop == op and len(bk) == 3 and bk[2] == breg:
                bo = toi(bk[1])
            elif not is_diff:
                bo = to
            if to is not None and bo is not None:
                out.append((base_kind(breg, this_regs, param_regs), breg, to, bo, is_diff))
        # addi/subi computing &member: "dst, src, imm"
        elif op in ADDI and len(tk) == 3 and REG_RE.match(tk[1]):
            srcreg = tk[1]
            to = toi(tk[2]); bo = None
            if bop == op and len(bk) == 3 and bk[1] == srcreg:
                bo = toi(bk[2])
            elif not is_diff:
                bo = to
            if to is not None and bo is not None:
                out.append((base_kind(srcreg, this_regs, param_regs), srcreg, to, bo, is_diff))
        # NOTE: `subi rX, this, N` is deliberately NOT collected. It appears almost
        # exclusively in destructor/vbase-adjust thunks (??_G/??_D/??_E funclets)
        # where N is a frame/subobject pointer-adjust, not a member offset; negating
        # it produced spurious large-delta "candidates" with garbage negative
        # thresholds (Shockwave +/-724, Waypoint -592). Real member &-of computes
        # use addi, which we DO collect above.
        # THEN apply this instruction's dataflow effect (positional this-tracking)
        _step_dataflow(x, this_regs, param_regs)
    return out


def analyze_fn(accesses):
    """Given the access list of ONE function, decide if it shows a clean uniform
    member-shift and at what threshold. Returns dict or None.

    Strategy: restrict to `this`-based accesses (the high-confidence case). Compute
    delta = target_off - base_off for the differing ones. If there's a single
    dominant non-zero delta C, find the threshold T = min target_off among the
    C-shifted accesses, and verify that all matching (delta 0) this-accesses are
    BELOW T (the clean-threshold invariant). Report C, T, counts, and whether the
    invariant held. Also produce a fallback summary for param/derived bases.
    """
    by_kind = defaultdict(list)
    for kind, breg, to, bo, is_diff in accesses:
        by_kind[kind].append((to, bo, is_diff))

    def summarize(rows):
        diffs = [(to, bo, to - bo) for (to, bo, isd) in rows if isd and to != bo]
        matches = [to for (to, bo, isd) in rows if not isd or to == bo]
        if not diffs:
            return None
        dc = Counter(d for (_, _, d) in diffs)
        C, n_c = dc.most_common(1)[0]
        shifted = [to for (to, bo, d) in diffs if d == C]
        T = min(shifted) if shifted else None
        # clean-threshold invariant: every MATCH access is below T (for C<0, our
        # build is bigger -> matched members sit below the insertion point). For
        # C>0 it's symmetric on the target side; use target offsets uniformly.
        below = [m for m in matches if T is not None and m < T]
        above_match = [m for m in matches if T is not None and m >= T]
        consistency = n_c / len(diffs)
        clean = (len(above_match) == 0) and consistency >= 0.75 and len(dc) <= 2
        return {
            'delta': C, 'threshold': T, 'n_shifted': len(shifted),
            'n_diff_total': len(diffs), 'n_distinct_deltas': len(dc),
            'distinct_deltas': dict(dc), 'consistency': round(consistency, 3),
            'n_match_below': len(below), 'n_match_above': len(above_match),
            'clean': clean,
        }

    this_s = summarize(by_kind.get('this', []))
    param_s = summarize(by_kind.get('param', []))
    der_s = summarize(by_kind.get('derived', []))
    # count frame/funclet noise for transparency
    n_frame = sum(1 for (to, bo, isd) in by_kind.get('frame', []) if isd and to != bo)
    n_funclet = sum(1 for (to, bo, isd) in by_kind.get('funclet', []) if isd and to != bo)
    if not (this_s or param_s or der_s):
        return None
    return {'this': this_s, 'param': param_s, 'derived': der_s,
            'n_frame_noise': n_frame, 'n_funclet_noise': n_funclet}


CLASS_RE = re.compile(r'^\?[~A-Za-z0-9_]+@([A-Za-z0-9_]+)@')


def demangle_class(sym, unit):
    """Best-effort: pull the class name out of an MSVC mangled instance-method
    name `?Method@Class@@...`. Free funcs / unparseable -> the unit basename."""
    m = CLASS_RE.match(sym)
    if m:
        return m.group(1)
    # nested like ?Foo@Bar@Baz@@ -> take the immediately-following scope (Bar)
    if sym.startswith('?'):
        parts = sym[1:].split('@')
        if len(parts) >= 2 and parts[1] and not parts[1].startswith('?'):
            return parts[1]
    return os.path.basename(unit)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--tp', default='/tmp/true_progress.json')
    ap.add_argument('--proj', default=ROOT)
    ap.add_argument('--out', default=os.path.expanduser('~/tmp/forcemult/member_candidates.json'))
    ap.add_argument('--limit', type=int, default=0)
    ap.add_argument('--bucket', default='STRUCT_WORK')
    a = ap.parse_args()

    # compiler-generated thunks: ??_G scalar-deleting dtor, ??_D vbase dtor,
    # ??_E vector-deleting, ??_F/??__F static-init, ??_B local-static guard. These
    # are funclets whose `subi this,N` adjusts are NOT member accesses; including
    # them produced garbage candidates. Skip at ingest.
    THUNK_RE = re.compile(r'^\?\?_[GDEFB]|^\?\?__[EF]')
    # STL template helpers (?? $ _Destroy_Range / _Copy / _Uninitialized... etc.):
    # the demangler reads the ELEMENT type as the "class", and the +/-delta is a
    # loop-pointer-stride regalloc artifact, not a member-layout shift. The real
    # fix (if any) lives in the element class, found via its own methods. Drop.
    STL_HELPER_RE = re.compile(r'@stlpmtx_std@@|^\?\?\$_(Destroy|Copy|Uninitialized|Move|Fill)')

    def keep(r):
        s = r['sym']
        if THUNK_RE.match(s) or STL_HELPER_RE.search(s):
            return False
        return True

    tp = json.load(open(a.tp))
    pool = [r for r in tp['rows'] if r['bucket'] == a.bucket and keep(r)]
    pool.sort(key=lambda r: -r.get('size', 0))
    if a.limit:
        pool = pool[:a.limit]
    print(f"member_delta_finder: {len(pool)} {a.bucket} fns from {a.tp}", file=sys.stderr)

    # per-class aggregation of clean this-based fn deltas
    class_fns = defaultdict(list)       # class -> list of fn-level analysis
    per_fn_out = []
    for i, r in enumerate(pool):
        if i and i % 50 == 0:
            print(f"  {i}/{len(pool)}", file=sys.stderr)
        unit, sym = r['unit'], r['sym']
        d = diff_fn(unit, sym, a.proj)
        if not d:
            continue
        acc = collect_accesses(d)
        an = analyze_fn(acc)
        if not an:
            continue
        cls = demangle_class(sym, unit)
        rec = {'unit': unit, 'sym': sym, 'mp': r.get('mp'), 'class': cls, 'analysis': an}
        per_fn_out.append(rec)
        class_fns[cls].append(rec)

    # build ranked CLASS candidates from the `this`-based clean deltas
    candidates = []
    for cls, fns in class_fns.items():
        this_clean = [f for f in fns if f['analysis'].get('this') and f['analysis']['this']['clean']]
        if not this_clean:
            continue
        # group those by delta C
        by_C = defaultdict(list)
        for f in this_clean:
            by_C[f['analysis']['this']['delta']].append(f)
        for C, group in by_C.items():
            thresholds = [g['analysis']['this']['threshold'] for g in group]
            # Each method's threshold = its lowest SHIFTED offset, an UPPER BOUND on
            # the true insertion point (members below it matched in that method). The
            # tightest bound across methods is the smallest such threshold. The true
            # insertion offset lies in (max matching-below, T_min]; report T_min as
            # the best single estimate and the modal threshold for context.
            T_min = min(thresholds)
            T_counter = Counter(thresholds)
            T_mode = T_counter.most_common(1)[0][0]
            n_aff = len(group)
            # A real member sits at a non-negative this-offset. A negative threshold
            # means the "this"-reg was actually a base-subobject/vbase pointer that
            # had been adjusted below the object base (funclet/dtor residue that
            # slipped the dataflow tracker) — not a member-layout shift. Reject.
            if T_min < 0:
                continue
            # multi-delta warning: does the SAME class also have a *different*
            # clean delta? then likely coupled-base/vbase, not a single member.
            other_deltas = set(by_C.keys()) - {C}
            coupled_warn = len(other_deltas) > 0
            # BOUNDARY confidence: did ANY method observe a MATCHED this-access
            # BELOW the threshold? If yes, we've directly seen the insertion point
            # (members below match, above shift) => HIGH confidence this is a member
            # OF THIS CLASS at ~T_min. If NO method shows a matched-below access,
            # every observed this-access is uniformly shifted — which is ALSO what a
            # BASE-CLASS size difference looks like (the whole derived object slides).
            # That's AMBIGUOUS: the fix might belong in a base class, not `cls`.
            has_boundary = any(g['analysis']['this'].get('n_match_below', 0) > 0
                               for g in group)
            confidence = 'high' if (has_boundary and not coupled_warn) else (
                'low' if coupled_warn else 'medium')
            # consistency score: avg per-fn consistency * threshold-agreement,
            # boosted when we directly observed the insertion boundary.
            avg_cons = sum(g['analysis']['this']['consistency'] for g in group) / n_aff
            thr_agree = T_counter.most_common(1)[0][1] / n_aff
            score = n_aff * avg_cons * thr_agree * (1.5 if has_boundary else 1.0)
            units = sorted(set(g['unit'] for g in group))
            # oracle hint: band3/network = GAME code (rb3-Wii oracle); else ENGINE
            # (DC3 oracle, the newer twin). The dropped/added-member framing differs:
            # engine divergence is "DC3 added vs retail", game is "rb3-Wii vs retail".
            is_game = any('/band3/' in u or '/network/' in u for u in units)
            src_word = 'rb3-Wii (game code)' if is_game else 'DC3 (engine; newer twin)'
            direction = ('our build (base) is %d bytes LARGER than retail -> we carry an '
                         'EXTRA member the retail struct lacks (likely from %s); '
                         'REMOVE/shrink it' % (-C, src_word)) if C < 0 else \
                        ('our build (base) is %d bytes SMALLER than retail -> retail has a '
                         'member we LACK; ADD it (cross-check %s)' % (C, src_word))
            off_hex = '0x%x' % T_min
            fix_hint = (
                f"Class {cls}: uniform this-relative member-offset delta C={C} "
                f"(0x{abs(C):x}) across {n_aff} method(s) at threshold offset "
                f"~{off_hex} (target side). {direction}. "
                f"Insert/remove a {abs(C)}-byte ({abs(C)//4 if abs(C)%4==0 else abs(C)}-word) "
                f"member at offset {off_hex} (target-side upper bound; true insertion "
                f"is in (max-matched-below, {off_hex}]) in the {cls} header. "
                + (f"Cross-check identity in ../rb3 (rb3-Wii game oracle) `class {cls}` "
                   f"and DC3 only if shared; "
                   if is_game else
                   f"Cross-check identity: grep ../dc3-decomp for `class {cls}` (DC3 is "
                   f"the newer twin; the extra member is likely THERE), compare vs ../rb3 "
                   f"(rb3-Wii) which lacks it; ")
                + f"verify the offset in Ghidra (struct-info / ghidra-struct skill)."
            )
            if has_boundary:
                fix_hint += (f"  CONFIDENCE high: at least one method shows matched "
                             f"this-accesses BELOW {off_hex} and shifted ones at/above "
                             f"— the insertion boundary is directly observed, so the "
                             f"member is in {cls} itself (not a base class).")
            else:
                fix_hint += (f"  CONFIDENCE medium: ALL observed this-accesses are "
                             f"uniformly shifted — no matched-below boundary was seen, "
                             f"so this could equally be a BASE-CLASS size difference "
                             f"(check {cls}'s bases first: the whole object slides if a "
                             f"base grew/shrank). Disambiguate before editing.")
            if coupled_warn:
                fix_hint += (f"  WARNING: class also shows other clean deltas "
                             f"{sorted(other_deltas)} -> possible coupled-base/vbase "
                             f"relayout, NOT a single one-member fix; investigate "
                             f"before applying.")
            candidates.append({
                'class': cls, 'delta': C, 'offset': off_hex, 'offset_int': T_min,
                'offset_mode': '0x%x' % T_mode,
                'n_affected': n_aff, 'units': units,
                'consistency': round(avg_cons, 3),
                'threshold_agreement': round(thr_agree, 3),
                'threshold_hist': {('0x%x' % k): v for k, v in T_counter.items()},
                'coupled_base_warning': coupled_warn,
                'has_boundary': has_boundary,
                'confidence': confidence,
                'score': round(score, 3),
                'methods': [g['sym'] for g in group],
                'fix_hint': fix_hint,
            })

    candidates.sort(key=lambda c: -c['score'])

    # also surface param/derived clean shifts SEPARATELY (lower confidence: the fix
    # is in another class's header), and note coupled-base classes explicitly.
    param_notes = []
    for cls, fns in class_fns.items():
        pf = [f for f in fns if f['analysis'].get('param') and f['analysis']['param']['clean']]
        if pf:
            by_C = Counter(f['analysis']['param']['delta'] for f in pf)
            param_notes.append({'class_context': cls, 'n_methods': len(pf),
                                'param_deltas': dict(by_C),
                                'note': 'uniform delta on a PARAMETER pointer; fix is '
                                        'in the parameter type header, not this class'})

    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    json.dump({
        'pool': a.bucket, 'n_fns_scanned': len(pool),
        'n_class_candidates': len(candidates),
        'candidates': candidates,
        'param_derived_notes': param_notes,
        'per_fn': per_fn_out,
    }, open(a.out, 'w'), indent=1)

    print(f"\n=== member-delta CLASS candidates (this-relative, clean) ===")
    print(f"{'score':>6} {'C':>5} {'off':>7} {'#fn':>4} {'cons':>5} {'conf':>6} cls")
    for c in candidates[:30]:
        warn = ' [COUPLED?]' if c['coupled_base_warning'] else ''
        print(f"{c['score']:6.2f} {c['delta']:5d} {c['offset']:>7} {c['n_affected']:4d} "
              f"{c['consistency']:5.2f} {c['confidence']:>6} {c['class']}{warn}")
    nhi = sum(1 for c in candidates if c['confidence'] == 'high')
    print(f"\nwrote {a.out}  ({len(candidates)} class candidates, {nhi} high-confidence, "
          f"{len(per_fn_out)} fns with this/param/derived shifts)")


if __name__ == '__main__':
    main()
