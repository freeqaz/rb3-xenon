#!/usr/bin/env python3
"""inline_policy_finder — detect the INLINE-POLICY force-multiplier near-miss class.

THE PATTERN (proven win: String::operator==/!= — commit ce16bfa, +6):
DC3 (the newer engine source we inherited) made a small method INLINE in a
header; retail RB3 kept it OUT-OF-LINE. So OUR build inlines the body while
retail emits a `bl <callee>`. The fix is to move that one method out-of-line
(header decl + .cpp def) — and it is a FORCE-MULTIPLIER because every near-miss
that calls the method flips to byte-exact at once.

How it shows up in objdiff's aligned instruction stream (objdiff-cli diff -f json
--include-instructions):

  Direction OUTLINE  (we inlined; retail out-of-line → fix = MOVE OUT-OF-LINE)
    TARGET side has a `bl <callee>` (the retail out-of-line call) while the BASE
    side has a contiguous run of *insert* records (instructions present only in
    base = the body OUR compiler inlined in place of the call). The bl record is
    a `delete` (target-only) or a `diff_arg`/`replace`/`mismatch` whose TARGET
    opcode is `bl`. The callee symbol = the target bl's symbol token.

  Direction INLINE   (we out-of-line; retail inlined → fix = MAKE INLINE)
    BASE side has a `bl <callee>` while the TARGET side has a contiguous run of
    *delete* records (target-only = the body retail inlined). The callee symbol
    = the base bl's symbol token.

We scan the named near-miss pool, detect the signature per function, resolve the
callee symbol (demangled), GROUP by callee, and rank by the number of distinct
near-miss functions exhibiting it (the multiplier). For each callee we emit a
fix_hint: which header method to move out-of-line / inline, and — when locatable
— whether it is CURRENTLY inline in our src/ header (the actionable signal).

Anonymous fn_ callees are resolved using fn_resolver.py's identity index
(~/tmp/fn_resolver_index.json) which aggregates 7 evidence tiers. Resolved
anonymous callees become fully actionable candidates instead of dead-ends.

Output: ~/tmp/forcemult/inline_candidates.json — ranked
  [{callee, callee_demangled, direction, n_affected, affected[], header,
    current_form, fix_hint, resolved_from_fn (when resolved)}]

Usage:
  tools/inline_policy_finder.py                 # band [90,100), named fns
  tools/inline_policy_finder.py --lo 95 --hi 100
  tools/inline_policy_finder.py --limit 50      # cap fns scanned (debug)
  tools/inline_policy_finder.py --sym '<one symbol>'   # single-fn dump
  tools/inline_policy_finder.py --no-resolver   # skip fn_resolver (faster, old behavior)
"""
import sys, os, re, json, subprocess, argparse
from collections import defaultdict, Counter

# fn_resolver integration: load the pre-built index once
_FN_RESOLVER_INDEX_PATH = os.path.expanduser('~/tmp/fn_resolver_index.json')
_fn_resolver_index: dict | None = None  # lazy loaded
_fn_resolver_tried = False


def _load_fn_resolver_index():
    """Load (lazily) the fn_resolver identity index. Returns dict or None."""
    global _fn_resolver_index, _fn_resolver_tried
    if _fn_resolver_tried:
        return _fn_resolver_index
    _fn_resolver_tried = True
    idx_path = _FN_RESOLVER_INDEX_PATH
    if not os.path.exists(idx_path):
        print(f'[fn_resolver] index not found at {idx_path}; skipping anonymous resolution',
              file=sys.stderr)
        _fn_resolver_index = None
        return None
    try:
        with open(idx_path) as f:
            raw = json.load(f)
        # Normalise keys to uppercase 0x8XXXXXXX for lookup
        _fn_resolver_index = {}
        for k, v in raw.items():
            # keys are already 0x8XXXXXXX (possibly upper or lower); normalise
            try:
                addr = int(k, 16)
                _fn_resolver_index[f'0x{addr:08X}'] = v
            except ValueError:
                pass
        print(f'[fn_resolver] loaded {len(_fn_resolver_index)} identities',
              file=sys.stderr)
    except Exception as e:
        print(f'[fn_resolver] failed to load index: {e}', file=sys.stderr)
        _fn_resolver_index = None
    return _fn_resolver_index


# Minimum confidence threshold for resolving an anonymous callee to a named identity.
# We require >=0.75 so fuzzy/speculative matches don't produce false actionables.
_RESOLVER_CONF_MIN = 0.75

# Sources considered reliable enough to treat as "named" (not just unit-placement)
_RELIABLE_SOURCES = {
    'decomp_db_named', 'target_symbol_map', 'dc3_content_match',
    'game_content_match', 'fuzzy_pairs', 'bindiff_dc3', 'vtable', 'rtti',
}


def resolve_fn_addr(fn_sym: str) -> tuple[str | None, str | None, float, str | None]:
    """Attempt to resolve an anonymous fn_ symbol to a mangled name.

    Returns (mangled, demangled, confidence, source) or (None, None, 0.0, None).
    Only returns a result when confidence >= _RESOLVER_CONF_MIN and source is
    considered reliable (not just unit-placement).
    """
    if not fn_sym.startswith('fn_') and not fn_sym.startswith('lbl_'):
        return None, None, 0.0, None
    try:
        addr_hex = fn_sym[fn_sym.index('_') + 1:]  # strip 'fn_' or 'lbl_'
        addr = int(addr_hex, 16)
    except ValueError:
        return None, None, 0.0, None

    idx = _load_fn_resolver_index()
    if idx is None:
        return None, None, 0.0, None

    key = f'0x{addr:08X}'
    entry = idx.get(key)
    if entry is None:
        return None, None, 0.0, None

    best = entry.get('best', {})
    conf = float(best.get('confidence', 0.0))
    source = best.get('source', '')
    mangled = best.get('mangled', '')
    demangled = best.get('demangled', mangled)

    # Filter: must be reliable + confident + actually named (not just anon unit)
    if conf < _RESOLVER_CONF_MIN:
        return None, None, 0.0, None
    if source not in _RELIABLE_SOURCES:
        return None, None, 0.0, None
    if not mangled or mangled.startswith('fn_') or mangled.startswith('<anon'):
        return None, None, 0.0, None

    return mangled, demangled, conf, source

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPORT = os.path.join(ROOT, 'build/45410914/report.json')
CLI = os.path.join(ROOT, 'bin', 'objdiff-cli')
SRC = os.path.join(ROOT, 'src')

# Symbols that are NOT inline-policy candidates even when they appear as a
# bl-vs-inlined: compiler runtime / frame helpers (savegpr/restgpr/memcpy etc),
# and our own unnamed fn_ targets (no header method to toggle).
# Compiler runtime / intrinsics / libm: NOT header methods we can toggle. The
# "inlined body" around these is incidental (stack adjust, soft-float), not an
# inline-policy lever. `alloca`/`floor`/`memcpy` etc. are intrinsics MSVC always
# inlines or emits per its own rules — out of scope.
RUNTIME_RE = re.compile(
    r'^(__save|__rest|__savef|__restf|_savegpr|_restgpr|__c_|_fltused|'
    r'memcpy|memset|memmove|memcmp|__security|__CxxFrameHandler|__alloca|alloca|'
    r'_purecall|__c0|__C_specific|__chkstk|floor|ceil|sqrt|fabs|__fixunsdfdi|'
    r'_aullshr|_allshl|_allmul|_alldiv|__abnormal|except_data)')
UNNAMED_RE = re.compile(r'^(fn_|lbl_|sub_|loc_|loc_8|j_)[0-9A-Fa-f]+$')

# An "inlinable body" instruction — arithmetic / load-store / compare / move
# the kind of thing a small accessor or operator expands into. We use the
# presence of a run of these on the opposite side from the bl as corroboration.
BODY_OPS = {
    'addi','add','addic','addic.','subi','subf','subfic','neg','mr','mr.',
    'li','lis','lwz','lbz','lhz','lha','lwzx','lbzx','lhzx','stw','stb','sth',
    'lfs','lfd','stfs','stfd','cmpw','cmpwi','cmplw','cmplwi','cmpd','cmpdi',
    'and','and.','or','or.','xor','nand','nor','andi.','ori','xori','slw',
    'srw','sraw','srawi','rlwinm','rlwimi','extsb','extsh','clrlwi','mulli',
    'mullw','divw','divwu','fadds','fmuls','fsubs','fdivs','fmr','fcmpu',
    'mfcr','isel','cntlzw','not','rotlwi','crxor','beq','bne','blt','bgt',
    'cror','sub','cmp',
}


def run_diff(sym, unit=None):
    cmd = [CLI, 'diff', '-p', ROOT, sym, '-f', 'json', '--include-instructions']
    if unit:
        cmd = [CLI, 'diff', '-p', ROOT, '-u', unit, sym, '-f', 'json',
               '--include-instructions']
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
        if r.returncode != 0 or not r.stdout.strip():
            return None
        return json.loads(r.stdout)
    except Exception:
        return None


def opc(side):
    return side.get('opcode') if side else None


def first_sym(side):
    """First Symbol-typed arg of an instruction side (the bl callee)."""
    if not side:
        return None
    for ta in (side.get('typed_args') or []):
        if ta.get('type') == 'Symbol':
            return ta.get('value')
    # fallback: parse args string
    args = (side.get('args') or '')
    for tok in [t.strip() for t in args.split(',')]:
        if tok and not re.match(r'^[rf]\d+$|^cr\d+$|^-?0x[0-9A-Fa-f]+$|^-?\d+$', tok):
            return tok
    return None


def is_branch_call(o):
    return o in ('bl', 'b', 'bla', 'ba')


STRCMP_OPS = {'lbz', 'lbzu', 'subf', 'cmpwi', 'cmpw'}  # byte-loop fingerprint


def _body_fingerprint(insns, lo, hi, side):
    """Heuristic identity of the single-sided inlined body in [lo,hi) on `side`.
    Returns a short tag used to resolve an anonymous callee (e.g. a strcmp loop
    => String comparison / strcmp)."""
    ops = []
    for j in range(lo, hi):
        if insns[j].get('match_type') == side:
            s = insns[j].get('base') if side == 'insert' else insns[j].get('target')
            if s:
                ops.append(opc(s))
    so = set(ops)
    if {'lbz'} <= so and (so & {'subf', 'cmpwi'}) and any(
            o in ('beq', 'bne') for o in ops):
        return 'strcmp/byte-loop'
    if so & {'fadds', 'fmuls', 'fsubs', 'fmadds', 'fdivs', 'fmr', 'fctiwz',
             'frsp', 'fcmpu'}:
        return 'float-math'
    if len(ops) <= 4 and so <= {'lwz', 'lbz', 'lhz', 'lfs', 'addi', 'mr'}:
        return 'accessor/load'
    return f'body[{len(ops)}ops]'


def detect(insns):
    """Return list of (direction, callee_key, callee_hint, body_tag) signatures.

    OUTLINE: target has `bl <callee>` ; base has a nearby run of `insert`
             (base-only) BODY_OPS => we inlined, retail out-of-line.
             fix = move the callee OUT-OF-LINE.
    INLINE : base has `bl <callee>` ; target has a nearby run of `delete`
             (target-only) BODY_OPS => we out-of-line, retail inlined.
             fix = make the callee INLINE.

    The bl may surface as a `delete`/`insert` (single-sided) OR inside a
    `replace`/`diff_op`/`diff_arg` record (one side a bl, the other a body op).
    The callee symbol is used as the grouping key; if it is anonymous (`fn_`),
    we still group on it (same address => same out-of-line method across callers)
    and tag it with a body fingerprint so a human can resolve identity.
    """
    n = len(insns)
    WIN = 8           # records around the bl to scan for the inlined body
    MINBODY = 3       # minimum single-sided body-op run to call it inlining
    hits = []

    def window(center, side):
        lo = max(0, center - WIN)
        hi = min(n, center + WIN + 1)
        run = 0
        for j in range(lo, hi):
            if insns[j].get('match_type') == side:
                s = insns[j].get('base') if side == 'insert' else insns[j].get('target')
                if s and opc(s) in BODY_OPS:
                    run += 1
        return run, lo, hi

    for i, ins in enumerate(insns):
        if ins.get('match_type') == 'equal':
            continue
        t, b = ins.get('target'), ins.get('base')
        to, bo = opc(t), opc(b)

        # OUTLINE: target-side bl, base side NOT the same bl (it inlined the body)
        if is_branch_call(to):
            cal = first_sym(t)
            base_same = is_branch_call(bo) and first_sym(b) == cal
            if cal and not base_same and not RUNTIME_RE.match(cal):
                run, lo, hi = window(i, 'insert')
                if run >= MINBODY:
                    tag = _body_fingerprint(insns, lo, hi, 'insert')
                    hits.append(('OUTLINE', cal, UNNAMED_RE.match(cal) is not None, tag))

        # INLINE: base-side bl, target side NOT the same bl (it inlined the body)
        if is_branch_call(bo):
            cal = first_sym(b)
            tgt_same = is_branch_call(to) and first_sym(t) == cal
            if cal and not tgt_same and not RUNTIME_RE.match(cal):
                run, lo, hi = window(i, 'delete')
                if run >= MINBODY:
                    tag = _body_fingerprint(insns, lo, hi, 'delete')
                    hits.append(('INLINE', cal, UNNAMED_RE.match(cal) is not None, tag))

    return sorted(set(hits))


# ---- demangling + header location -------------------------------------------

def demangle(sym):
    """Best-effort MSVC demangle via undname if available, else heuristic."""
    if not sym.startswith('?'):
        return sym
    try:
        r = subprocess.run(['undname', sym], capture_output=True, text=True,
                           timeout=5)
        m = re.search(r'is :- "(.*)"', r.stdout)
        if m:
            return m.group(1)
    except Exception:
        pass
    return sym


def callee_basename(demangled, sym):
    """Pull (class, method) out of a mangled symbol. class=None for free fns."""
    d = demangled or sym
    # ?Foo@Bar@@... -> Bar::Foo
    m = re.match(r'^\?([A-Za-z_~][\w]*)@([A-Za-z_][\w]*)@', sym)
    if m:
        return m.group(2), m.group(1)          # (Class, method)
    # ?Foo@@YA...  -> free function Foo (no class; @@ right after the name)
    m = re.match(r'^\?([A-Za-z_][\w]*)@@', sym)
    if m:
        return None, m.group(1)                # (None free-fn, method)
    m = re.match(r'^\?\?([0-9A-DG-Z])@([A-Za-z_][\w]*)@', sym)  # ctor/dtor/op
    if m:
        return m.group(2), None
    m = re.search(r'(\w+)::(\w+|operator\S+)', d)
    if m:
        return m.group(1), m.group(2)
    return None, None


def find_header_form(klass, method, sym):
    """Search src/ headers for the method and report whether it is currently
    inline (body in header) or just declared. Returns (header_path, form).

    Handles both class methods (grep the defining class) and free functions
    (grep the bare name across all headers)."""
    if not klass and method:
        # free function: grep its name in headers, judge inline vs decl
        try:
            r = subprocess.run(
                ['grep', '-rln', '--include=*.h', rf'\b{re.escape(method)}\b', SRC],
                capture_output=True, text=True, timeout=30)
            headers = [h for h in r.stdout.splitlines() if h]
        except Exception:
            headers = []
        for h in headers:
            try:
                txt = open(h, 'r', errors='ignore').read()
            except Exception:
                continue
            if re.search(re.escape(method) + r'\s*\([^;{]*\)\s*\{', txt):
                return h, 'INLINE (body in header)'
            if re.search(re.escape(method) + r'\s*\([^;{]*\)\s*;', txt):
                return h, 'DECL-only (already out-of-line)'
        return (headers[0] if headers else None), 'free-fn; form unclear'
    if not klass:
        return None, 'unknown'
    # find headers defining the class
    try:
        r = subprocess.run(
            ['grep', '-rln', '--include=*.h',
             rf'\bclass {klass}\b', SRC],
            capture_output=True, text=True, timeout=30)
        headers = [h for h in r.stdout.splitlines() if h]
    except Exception:
        headers = []
    op_names = {
        '0': '~', 'operator==': 'operator==', 'operator!=': 'operator!=',
    }
    needle = method
    if method and method.startswith('operator'):
        needle = method
    for h in headers:
        try:
            txt = open(h, 'r', errors='ignore').read()
        except Exception:
            continue
        if needle and needle in txt:
            # crude: a line with the method ident followed by `{` on same/next
            # tokens => inline body; one ending in `;` => out-of-line decl.
            inline = bool(re.search(
                re.escape(needle) + r'\s*\([^;{]*\)\s*(const\s*)?\{', txt))
            decl_only = bool(re.search(
                re.escape(needle) + r'\s*\([^;{]*\)\s*(const\s*)?;', txt))
            if inline:
                return h, 'INLINE (body in header)'
            if decl_only:
                return h, 'DECL-only (already out-of-line)'
            return h, 'present (form unclear)'
    return (headers[0] if headers else None), 'not found in headers'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--lo', type=float, default=90.0)
    ap.add_argument('--hi', type=float, default=100.0)
    ap.add_argument('--limit', type=int, default=0)
    ap.add_argument('--report', default=REPORT)
    ap.add_argument('--sym', default=None, help='dump one symbol and exit')
    ap.add_argument('--named-only', action='store_true', default=True)
    ap.add_argument('--no-resolver', action='store_true', default=False,
                    help='skip fn_resolver index (old behavior, faster)')
    ap.add_argument('--out',
                    default=os.path.expanduser('~/tmp/forcemult/inline_candidates.json'))
    a = ap.parse_args()
    if a.no_resolver:
        global _fn_resolver_tried
        _fn_resolver_tried = True  # prevent loading

    if a.sym:
        d = run_diff(a.sym)
        if not d:
            print('no diff', file=sys.stderr); return
        print('detect:', detect(d.get('instructions', [])))
        return

    rep = json.load(open(a.report))
    targets = []
    for unit in rep['units']:
        un = unit['name']
        for f in unit.get('functions', []):
            mp = f.get('match_percent_normalized', 0.0)
            nm = f['name']
            if not (a.lo <= mp < a.hi):
                continue
            if nm.startswith('fn_') or nm.startswith('__unwind') \
                    or nm.startswith('lbl_'):
                continue
            targets.append((un, nm, mp, int(f.get('size', 0))))
    targets.sort(key=lambda t: -t[3])
    if a.limit:
        targets = targets[:a.limit]
    print(f'scanning {len(targets)} named near-misses in [{a.lo},{a.hi}) ...',
          file=sys.stderr)

    # (direction, callee) -> {affected fns}, and the observed body tags
    by_key = defaultdict(set)
    body_tags = defaultdict(Counter)
    unnamed = {}
    for i, (un, sym, mp, sz) in enumerate(targets):
        if i and i % 100 == 0:
            print(f'  {i}/{len(targets)}', file=sys.stderr)
        d = run_diff(sym, un)
        if not d:
            continue
        for direction, callee, is_unnamed, tag in detect(d.get('instructions', [])):
            k = (direction, callee)
            by_key[k].add((un, sym, round(mp, 2)))
            body_tags[k][tag] += 1
            unnamed[k] = is_unnamed

    # Pre-load fn_resolver index now (before building candidates) so the
    # one-time load message appears before the per-candidate output.
    if not a.no_resolver:
        _load_fn_resolver_index()

    cands = []
    n_resolved_anon = 0
    for (direction, callee), affected in by_key.items():
        dem = demangle(callee)
        is_unnamed = unnamed[(direction, callee)]
        tag = body_tags[(direction, callee)].most_common(1)[0][0]
        resolved_from_fn = None   # set when an anonymous callee was resolved

        if is_unnamed:
            # Try fn_resolver to convert anonymous fn_ to a real identity
            r_mangled, r_demangled, r_conf, r_source = resolve_fn_addr(callee)
            if r_mangled:
                # Successfully resolved: treat as a named callee from here on
                n_resolved_anon += 1
                resolved_from_fn = {
                    'original_fn': callee,
                    'resolved_mangled': r_mangled,
                    'resolved_demangled': r_demangled,
                    'resolver_conf': r_conf,
                    'resolver_source': r_source,
                }
                # Override: use the resolved identity
                callee_for_lookup = r_mangled
                dem = r_demangled or r_mangled
                klass, method = callee_basename(dem, r_mangled)
                header, form = find_header_form(klass, method, r_mangled)
                # It's no longer truly "unnamed" for actionability purposes
                is_effectively_unnamed = False
            else:
                klass, method, header, form = None, None, None, 'anonymous callee'
                callee_for_lookup = callee
                is_effectively_unnamed = True
        else:
            klass, method = callee_basename(dem, callee)
            header, form = find_header_form(klass, method, callee)
            callee_for_lookup = callee
            is_effectively_unnamed = False

        # actionable now = a NAMED callee whose header form matches the lever:
        #   OUTLINE + currently INLINE-in-header  (the Str case: move it out)
        #   INLINE  + currently DECL-only         (the MakeShortAng case: inline it)
        actionable = (
            (direction == 'OUTLINE' and form.startswith('INLINE')) or
            (direction == 'INLINE' and form.startswith('DECL-only')))
        name = (klass + '::' if klass else '') + (method or callee_for_lookup)
        if direction == 'OUTLINE':
            if is_effectively_unnamed:
                fix = (f'Retail calls OUT-OF-LINE `bl {callee}` (anonymous) where '
                       f'OUR build inlines a {tag}. Resolve {callee} (its body is a '
                       f'{tag}); if it maps to a header method we inline, move that '
                       f'method OUT-OF-LINE (header decl + .cpp def).')
            else:
                fix = (f'Move {name} OUT-OF-LINE: change the header to a forward '
                       f'decl ( ...(args) const; ) and put the body in the matching '
                       f'.cpp. Retail emits `bl {callee}`; we inline a {tag}.')
                if resolved_from_fn:
                    fix += (f' [fn_resolver: {callee} → {dem} '
                            f'conf={r_conf:.2f} via {r_source}]')
        else:
            if is_effectively_unnamed:
                fix = (f'Retail INLINES a {tag} where OUR build emits `bl {callee}` '
                       f'(anonymous). Resolve {callee} and make it INLINE in the '
                       f'header so the body folds into callers.')
            else:
                fix = (f'Make {name} INLINE: move its body from the .cpp into the '
                       f'header. Retail inlines a {tag}; we emit `bl {callee}`.')
                if resolved_from_fn:
                    fix += (f' [fn_resolver: {callee} → {dem} '
                            f'conf={r_conf:.2f} via {r_source}]')
        cands.append({
            'callee': callee,
            'callee_demangled': dem,
            'callee_unnamed': bool(is_unnamed),
            'resolved_from_fn': resolved_from_fn,
            'body_tag': tag,
            'direction': direction,
            'n_affected': len(affected),
            'actionable_now': actionable,
            'class': klass,
            'method': method,
            'header': header,
            'current_form': form,
            'fix_hint': fix,
            'affected': sorted([{'unit': u, 'sym': s, 'mp': m}
                                for (u, s, m) in affected],
                               key=lambda x: x['sym'])[:40],
        })
    print(f'  fn_resolver resolved {n_resolved_anon} anonymous callees to named identities',
          file=sys.stderr)
    # rank: actionable first, then named over anonymous, then by multiplier
    cands.sort(key=lambda c: (not c['actionable_now'], c['callee_unnamed'],
                              -c['n_affected']))

    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    json.dump({'band': [a.lo, a.hi], 'n_scanned': len(targets),
               'n_candidates': len(cands), 'candidates': cands},
              open(a.out, 'w'), indent=1)

    print(f'\n=== INLINE-POLICY candidates  band=[{a.lo},{a.hi}) '
          f'scanned={len(targets)} ===')
    print(f'{"DIR":8s} {"#fn":>4s} {"ACT":>4s}  {"form":22s} {"body":16s}  callee')
    for c in cands[:50]:
        print(f'{c["direction"]:8s} {c["n_affected"]:4d} '
              f'{"YES" if c["actionable_now"] else "":>4s}  '
              f'{c["current_form"][:22]:22s} {c["body_tag"][:16]:16s}  '
              f'{c["callee_demangled"][:60]}')
    print(f'\nwrote {a.out}  ({len(cands)} candidates)')


if __name__ == '__main__':
    main()
