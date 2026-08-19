#!/usr/bin/env python3
"""header_offset_triage.py -- triage the header-offset audit's disagreements.

The audit (tools/header_offset_audit.py, docs/decomp/HEADER_OFFSET_AUDIT_2026-08-18.md)
compares each `// 0xHEX` member comment against OUR OWN COMPILER.  It therefore
finds *disagreement* and says nothing about which side is wrong:

  class A -- stale comment, layout right  => comment-only fix, metric-neutral
  class B -- comment right, OUR LAYOUT WRONG => a real bug

The audit shipped with **562 of 740 classes UN-TRIAGED (76% of rows)** because it
joined headers to report.json units with a *substring heuristic* that resolved
only 178 classes.

This tool replaces that heuristic with an **exact, name-keyed join**: every MSVC
mangled symbol carries its own immediate class qualifier, so `?IsLoaded@MasterAudio@@QAA_NXZ`
names class `MasterAudio` with no path reconstruction at all.  That sidesteps
both the substring heuristic AND the bare-vs-nested `basename()` hazard that
broke four pinning lanes.

## The discriminator

  A class with a function scoring `fuzzy_match_percent == 100` has a PROVABLY
  CORRECT layout for every member that function touches -- any offset error
  changes the instruction encoding and would break the byte match.

Two strengths of witness are reported separately, because they are not equally
strong:

  T1  a matching function's body actually CONTAINS a D-form memory access at the
      compiler-reported offset, and does NOT contain one at the commented
      offset.  Direct evidence for that row.
  T2  a matching ctor/dtor/member exists and the class carries a UNIFORM shift.
      A uniform shift claims EVERY member is displaced by the same delta, so any
      single matching member function refutes it.

## Buckets

  A_PROVEN      >=1 fuzzy==100 member fn  => comment is the wrong side
  NO_WITNESS    member fns exist, none at fuzzy==100 => cannot rule out class B
  UNADJUDICABLE no mangled member fn in the RB3 binary at all (e.g. the
                hamobj/ and gesture/ Dance Central headers inherited verbatim
                from dc3-decomp -- RB3 retail contains no identified member, so
                no RB3-side evidence exists in either direction)

Usage:
  python3 tools/header_offset_triage.py --findings docs/decomp/header_offset_audit_2026-08-18.json
  python3 tools/header_offset_triage.py --class MasterAudio --verbose
  python3 tools/header_offset_triage.py --bodies          # add T1 displacement evidence
  python3 tools/header_offset_triage.py --selftest        # prove the instrument discriminates
"""
import argparse
import collections
import json
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(ROOT, 'scripts', 'harvest'))

DEFAULT_FINDINGS = 'docs/decomp/header_offset_audit_2026-08-18.json'
DEFAULT_REPORT = 'build/45410914/report.json'


# --------------------------------------------------------------------------
# exact class-qualifier extraction from MSVC mangled names
# --------------------------------------------------------------------------
def owner_class(sym):
    """Immediate (innermost) class qualifier of an MSVC mangled name, or None.

    ?Method@Class@@...      -> Class
    ??0Class@@...           -> Class   (constructor)
    ??1Class@@ / ??_GClass@@-> Class   (dtor / scalar deleting dtor)
    ?Method@Inner@Outer@@   -> Inner   (MSVC nests innermost-first)

    Templates (`?$vector@...`) deliberately return None: their qualifier is not
    a plain identifier and the audit's findings never key on one.
    """
    if not sym or not sym.startswith('?'):
        return None
    if sym.startswith('??'):
        m = re.match(r'\?\?(?:_[A-Za-z]|[0-9])([A-Za-z_]\w*)@', sym)
        return m.group(1) if m else None
    m = re.match(r'\?[A-Za-z_]\w*@([A-Za-z_]\w*)@', sym)
    return m.group(1) if m else None


def is_ctor_dtor(sym):
    return sym.startswith('??0') or sym.startswith('??1') or sym.startswith('??_G') \
        or sym.startswith('??_E')


# --------------------------------------------------------------------------
# report.json -> class -> member functions
# --------------------------------------------------------------------------
def load_report(path):
    with open(path) as fh:
        return json.load(fh)


def class_function_index(rep):
    """class name -> list of dicts(name, fuzzy, mpn, size, unit)."""
    idx = collections.defaultdict(list)
    for u in rep.get('units', []):
        uname = u.get('name', '')
        for f in u.get('functions', []):
            o = owner_class(f.get('name', ''))
            if not o:
                continue
            idx[o].append(dict(
                name=f['name'],
                # protobuf-json omits defaults: absent fuzzy == 0
                fuzzy=float(f.get('fuzzy_match_percent', 0) or 0),
                mpn=float(f.get('match_percent_normalized', 0) or 0),
                size=int(f.get('size', 0) or 0),
                unit=uname,
            ))
    return idx


# --------------------------------------------------------------------------
# shift shape
# --------------------------------------------------------------------------
def shift_shape(rows):
    """rows = [[line, member, comment_off, compiler_off], ...]

    Returns (kind, delta) where kind is 'UNIFORM' or 'MIXED'.  A UNIFORM shift
    asserts that EVERY member is displaced by the same delta -- which is why a
    single matching member function is enough to refute it.
    """
    deltas = {int(r[2]) - int(r[3]) for r in rows}
    if len(deltas) == 1:
        return 'UNIFORM', deltas.pop()
    return 'MIXED', None


# --------------------------------------------------------------------------
# COFF bodies + PowerPC D-form displacement scan
# --------------------------------------------------------------------------
# D-form: [opcode:6][rD/rS:5][rA:5][D:16(signed)]
_DFORM_LOADSTORE = {
    32, 33, 34, 35, 36, 37, 38, 39,      # lwz lwzu lbz lbzu stw stwu stb stbu
    40, 41, 42, 43, 44, 45,              # lhz lhzu lha lhau sth sthu
    46, 47,                              # lmw stmw
    48, 49, 50, 51, 52, 53, 54, 55,      # lfs lfsu lfd lfdu stfs stfsu stfd stfdu
}
_DS_FORM = {58, 62}                       # ld/lwa/ldu ; std/stdu
_ADDI = 14                                # addi rD,rA,D -- taking a member address


def this_displacements(body):
    """Displacements of memory accesses whose BASE REGISTER holds `this`.

    Counting *every* D-form displacement is far too weak to classify a row: a
    body is full of stack accesses off r1, so a fabricated offset 'appears'
    ~30% of the time and the commented offset ~51%.  Enrichment of 2.4x is a
    description of the detector, not of the defect -- exactly the mistake the
    refuted 'callee absent from map => fold-alias' model made at 1.95x.

    So track `this` instead.  MSVC X360 passes `this` in r3 and usually parks it
    in a callee-saved register in the prologue.  The tracker is deliberately
    CONSERVATIVE: any instruction whose destination register we cannot decode
    with certainty EVICTS that register from the `this` set.  It therefore
    UNDER-reports rather than inventing evidence -- the same 'under-report
    rather than corrupt' rule that fixed audit_header().
    """
    tracked = {3}
    disp = set()
    n_acc = 0
    for i in range(0, len(body) - 3, 4):
        w = struct.unpack_from('>I', body, i)[0]
        op = (w >> 26) & 0x3F
        rd = (w >> 21) & 0x1F
        ra = (w >> 16) & 0x1F

        if op in _DFORM_LOADSTORE or op in _DS_FORM:
            if op in _DS_FORM:
                d = w & 0xFFFC
            else:
                d = w & 0xFFFF
            if d & 0x8000:
                d -= 0x10000
            if ra in tracked:
                disp.add(d)
                n_acc += 1
            # load forms write rd; update forms also write ra
            is_store = op in (36, 37, 38, 39, 44, 45, 47, 52, 53, 54, 55) or op == 62
            if not is_store:
                tracked.discard(rd)
            if op in (33, 35, 37, 39, 41, 43, 45, 49, 51, 53, 55):
                tracked.discard(ra)
            continue

        if op == 31:
            xo = (w >> 1) & 0x3FF
            rb = (w >> 11) & 0x1F
            # mr rA,rS  ==  or rA,rS,rS   (dest is rA field for X-form logical)
            if xo == 444 and rd == rb:
                if rd in tracked:
                    tracked.add(ra)
                else:
                    tracked.discard(ra)
                continue
            # indexed load/store: base rA, index rB -- no constant displacement
            # to learn, but the dest register is clobbered.
            tracked.discard(rd)
            tracked.discard(ra)
            continue

        if op in (7, 8, 12, 13, 14, 15):        # mulli subfic addic addic. addi addis
            tracked.discard(rd)
        elif op in (24, 25, 26, 27, 28, 29, 21, 20, 23, 30):  # ori..andis., rlw*, rld*
            tracked.discard(ra)
        elif op in (16, 18, 19, 11, 10, 3):     # branches, compares, trap -- no GPR dest
            pass
        else:
            tracked.discard(rd)
            tracked.discard(ra)
    return disp, n_acc


def obj_symbol_body(objpath, symname):
    """Raw code bytes of `symname` in `objpath`, or None.

    Extent = [sym.val, next symbol's val in the same section | section rawsz).
    Deliberately NOT the whole COMDAT span: billing the whole span is what made
    a COMDAT reader attribute a SUCCESSOR symbol's EH funclet prefix to the
    function (see the STLPORT-1 correction in CLAUDE.md).
    """
    import coff_func_bodies as cfb
    try:
        d, secs, syms, _ = cfb.parse(objpath)
    except Exception:
        return None
    hit = None
    for (name, val, secnum, typ, sc, i) in syms:
        if name == symname and secnum > 0:
            hit = (name, val, secnum)
            break
    if hit is None:
        return None
    _, val, secnum = hit
    sec = secs[secnum - 1]
    if not sec['name'].startswith('.text'):
        return None
    ends = [v for (n, v, sn, t, s, i) in syms
            if sn == secnum and v > val and not n.startswith('$')]
    end = min(ends) if ends else sec['rawsz']
    end = min(end, sec['rawsz'])
    if end <= val:
        return None
    off = sec['rawptr'] + val
    return d[off:off + (end - val)]


def dform_displacements(body):
    """Set of signed displacements used by D-form/DS-form memory accesses.

    Image content is BIG-ENDIAN (CLAUDE.md); decoding little-endian here would
    silently yield garbage displacements that match nothing -- a vacuity shaped
    exactly like 'no evidence found'.
    """
    disp = set()
    addr_disp = set()
    for i in range(0, len(body) - 3, 4):
        w = struct.unpack_from('>I', body, i)[0]
        op = (w >> 26) & 0x3F
        ra = (w >> 16) & 0x1F
        if op in _DFORM_LOADSTORE:
            d16 = w & 0xFFFF
            if d16 & 0x8000:
                d16 -= 0x10000
            disp.add(d16)
        elif op in _DS_FORM:
            d14 = w & 0xFFFC
            if d14 & 0x8000:
                d14 -= 0x10000
            disp.add(d14)
        elif op == _ADDI and ra != 0:
            d16 = w & 0xFFFF
            if d16 & 0x8000:
                d16 -= 0x10000
            addr_disp.add(d16)
    return disp, addr_disp


# --------------------------------------------------------------------------
# locate the compiled obj holding a symbol
# --------------------------------------------------------------------------
class ObjFinder:
    """Find the compiled .obj that defines a mangled symbol.

    Candidates are ranked by basename against the report unit's last path
    component, then CONFIRMED by actually finding the symbol.  Confirmation is
    what makes the collision safe: `Movie.obj` genuinely exists in both
    `rnddx9/` and `rndobj/`, and a basename match alone would pick the wrong
    one.  A miss falls back to a full scan rather than reporting 'absent'.
    """

    def __init__(self, project_dir):
        self.root = project_dir
        self.objdir = os.path.join(project_dir, 'build', '45410914', 'src')
        self._all = None
        self._sym_cache = {}

    def all_objs(self):
        if self._all is None:
            out = []
            for dirpath, _dirs, files in os.walk(self.objdir):
                for fn in files:
                    if fn.endswith('.obj'):
                        out.append(os.path.join(dirpath, fn))
            self._all = out
        return self._all

    def find(self, symname, unit_hint=''):
        key = (symname, unit_hint)
        if key in self._sym_cache:
            return self._sym_cache[key]
        base = unit_hint.split('/')[-1] if unit_hint else ''
        objs = self.all_objs()
        ranked = [p for p in objs if base and os.path.basename(p) == base + '.obj']
        ranked += [p for p in objs if p not in ranked]
        found = None
        for p in ranked:
            if obj_symbol_body(p, symname) is not None:
                found = p
                break
        self._sym_cache[key] = found
        return found


# --------------------------------------------------------------------------
# triage
# --------------------------------------------------------------------------
def triage_class(cls, rows, fns):
    """Classify one disagreeing class.  Returns a dict."""
    kind, delta = shift_shape(rows)
    f100 = [f for f in fns if f['fuzzy'] == 100.0]
    cd100 = [f for f in f100 if is_ctor_dtor(f['name'])]
    if not fns:
        bucket = 'UNADJUDICABLE'
    elif f100:
        bucket = 'A_PROVEN'
    else:
        bucket = 'NO_WITNESS'
    return dict(
        cls=cls, rows=len(rows), shift=kind, delta=delta, bucket=bucket,
        n_fns=len(fns), n_100=len(f100), n_ctor100=len(cd100),
        best=sorted(f100, key=lambda f: -f['size'])[:6],
        units=sorted({f['unit'] for f in fns}),
    )


def run_triage(findings, cls_fns, only=None):
    out = []
    for hdr, classes in findings.items():
        for cls, rows in classes.items():
            if only and cls != only:
                continue
            t = triage_class(cls, rows, cls_fns.get(cls, []))
            t['header'] = hdr
            t['raw_rows'] = rows
            out.append(t)
    return out


# --------------------------------------------------------------------------
# T1: does a matching body actually touch the compiler offset?
# --------------------------------------------------------------------------
def body_evidence(t, finder, max_fns=8, null_shift=0x40):
    """Accumulate D-form displacements over the class's fuzzy==100 functions.

    For each disagreeing row, report whether the COMPILER offset and/or the
    COMMENTED offset appears as a memory-access displacement.

    NULL CONTROL: the same test is run against a fabricated offset
    (compiler + null_shift).  If the fabricated offset 'appears' about as often
    as the real one, the instrument is not discriminating and its verdicts are
    worthless -- report it rather than quoting a number from it.
    """
    seen = set()
    used = []
    n_acc = 0
    for f in t['best'][:max_fns]:
        p = finder.find(f['name'], f['unit'])
        if not p:
            continue
        body = obj_symbol_body(p, f['name'])
        if not body:
            continue
        d, na = this_displacements(body)
        seen |= d
        n_acc += na
        used.append(f['name'])
    if not used or not n_acc:
        return None
    hit_compiler = hit_comment = hit_null = 0
    per_row = []
    for (line, member, c_off, k_off) in t['raw_rows']:
        hc = int(k_off) in seen
        hk = int(c_off) in seen
        hn = (int(k_off) + null_shift) in seen
        hit_compiler += hc
        hit_comment += hk
        hit_null += hn
        per_row.append(dict(line=line, member=member, comment=int(c_off),
                            compiler=int(k_off), compiler_seen=hc,
                            comment_seen=hk, null_seen=hn))
    return dict(fns_used=used, n_disp=len(seen), n_this_accesses=n_acc,
                disp=sorted(seen),
                hit_compiler=hit_compiler, hit_comment=hit_comment,
                hit_null=hit_null, rows=per_row)


def header_comments(project_dir, header, cls):
    """member -> commented offset, for EVERY commented member of the class body.

    ⚠ Using only the audit's DISAGREEING rows misses the decisive pair.
    `SongParser::DifficultyInfo` is refuted by `mGemsInProgress -> mActivePlayers`
    (comment gap 8 vs a 12-byte STLport vector) -- but `mGemsInProgress`'s own
    comment AGREES with the compiler, so it is absent from the findings and the
    first version of this test reported the class "not refuted".
    Scoped to the class body for the reason `audit_header` was: `utl/Str.h`
    declares three classes and a whole-file scan attributes one's comment to
    another.
    """
    sys.path.insert(0, os.path.join(project_dir, 'scripts', 'harvest'))
    try:
        import class_layout_report as clr
    except Exception:
        return {}
    path = header if os.path.isabs(header) else os.path.join(project_dir, header)
    try:
        lines = open(path).read().splitlines()
    except Exception:
        return {}
    try:
        span = clr.class_body_span(lines, cls)
    except Exception:
        span = None
    # ⛔ class_body_span returns (start, end, base_depth) and None when the
    # declaration cannot be located unambiguously.  Its docstring is explicit
    # that callers MUST treat None as "do not audit" -- falling back to the
    # whole file is the exact bug that made audit_header attribute `String`'s
    # retail-verified `mStr // 0x8` to `FixedString` in utl/Str.h.  Under-report.
    if not span:
        return {}
    lo, hi = span[0], span[1]
    out = {}
    for s in lines[lo:hi]:
        m = re.search(r'//\s*(0[xX][0-9a-fA-F]+)\s*$', s)
        if not m:
            continue
        decl = s[:m.start()]
        for ident in re.findall(r'\b(\w+)\s*(?:\[[^\]]*\])?\s*;', decl):
            out[ident] = int(m.group(1), 16)
    return out


def structural_verdict(project_dir, cls, rows, header=None, timeout=1800):
    """Refute a comment WITHOUT any retail evidence, by structural impossibility.

    The compiler's own layout gives each member's true extent as the gap to the
    NEXT member.  If the COMMENT's gap between two consecutive members is
    SMALLER than that, the comment claims the earlier member occupies fewer
    bytes than its declared type can -- impossible for THIS source.

    Worked case: `SongParser::DifficultyInfo` comments put `mActivePlayers` at
    0x8 immediately after a `std::vector`, implying an 8-byte vector; the
    compiler reports STLport `vector` = 12 B (_M_start/_M_finish/
    _M_end_of_storage).  The comment is refuted with no retail bytes at all --
    which is why this reaches the NO_WITNESS and UNADJUDICABLE classes that no
    name-keyed instrument can touch.

    ⚠ SCOPE: this proves the comment does not describe OUR layout, hence the
    comment is stale.  It does NOT prove our layout matches RETAIL.  A class
    with no matching function stays unconstrained against retail either way.
    """
    import subprocess
    cmd = [sys.executable,
           os.path.join(project_dir, 'scripts', 'harvest', 'class_layout_report.py'),
           cls, '--project-dir', project_dir, '--json', '--no-vtable']
    try:
        out = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        data = json.loads(out.stdout)
    except Exception as e:
        return dict(cls=cls, status='LAYOUT_FAILED', detail=str(e)[:120])
    info = data.get('classes', {}).get(cls)
    if not info:
        return dict(cls=cls, status='CLASS_NOT_IN_REPORT')
    members = [m for m in info.get('members', []) if not m.get('is_vfptr')]
    off = {m['name']: int(m['offset']) for m in members}
    order = sorted(members, key=lambda m: int(m['offset']))
    comment = {r[1]: int(r[2]) for r in rows}
    if header:
        full = header_comments(project_dir, header, cls)
        for k, v in full.items():
            comment.setdefault(k, v)
    findings = []
    for a, b in zip(order, order[1:]):
        na, nb = a['name'], b['name']
        if na not in comment or nb not in comment:
            continue
        true_gap = int(b['offset']) - int(a['offset'])
        cmt_gap = comment[nb] - comment[na]
        if cmt_gap < true_gap:
            findings.append(dict(prev=na, nxt=nb, true_gap=true_gap,
                                 comment_gap=cmt_gap,
                                 prev_type=a.get('type', '')))
    return dict(cls=cls, status='OK', size=info.get('size'),
                n_members=len(members), impossible=findings)


_COMMENT_RE = re.compile(r'^(?P<pre>.*?//\s*)(?P<val>0[xX][0-9a-fA-F]+)(?P<post>\s*)$')


def proven_rows(tri):
    """Rows where a fuzzy==100 body makes a this-relative access at the
    COMPILER's offset for that member.  Since fuzzy==100 means our bytes EQUAL
    retail's bytes, that access exists in retail too => retail puts the member
    there => the comment is the wrong side.  This is the only set --apply will
    touch."""
    out = []
    for t in tri:
        e = t.get('evidence')
        if not e:
            continue
        seen = set(e['disp'])
        for (line, mem, c, k) in t['raw_rows']:
            if int(k) in seen:
                out.append(dict(header=t['header'], cls=t['cls'], line=int(line),
                                member=mem, comment=int(c), compiler=int(k)))
    return out


def apply_fixes(project_dir, rows, dry_run=True):
    """Rewrite proven-stale `// 0xHEX` comments.  Comments only, never code.

    EVERY row is gated before it is touched.  A row is REFUSED (never guessed)
    when the line no longer names the member, when the trailing comment is not a
    single `0x` value, or when that value is not the exact one the audit
    recorded.  Any of those means the header moved under us, and the audit's
    line numbers can no longer be trusted -- the failure mode that would
    silently rewrite a CORRECT comment to a wrong value.
    """
    by_hdr = collections.defaultdict(list)
    for r in rows:
        by_hdr[r['header']].append(r)

    applied = refused = skipped_multi = 0
    reasons = collections.Counter()
    changed_files = []
    for hdr, rr in sorted(by_hdr.items()):
        path = hdr if os.path.isabs(hdr) else os.path.join(project_dir, hdr)
        try:
            lines = open(path).read().splitlines(keepends=True)
        except Exception:
            refused += len(rr)
            reasons['header unreadable'] += len(rr)
            continue
        dirty = False
        for r in sorted(rr, key=lambda x: x['line']):
            i = r['line'] - 1
            if i < 0 or i >= len(lines):
                refused += 1
                reasons['line out of range'] += 1
                continue
            s = lines[i].rstrip('\n')
            if r['member'] not in s:
                refused += 1
                reasons['member name absent from line'] += 1
                continue
            m = _COMMENT_RE.match(s)
            if not m:
                if '//' in s and ',' in s.split('//', 1)[1]:
                    skipped_multi += 1
                    reasons['multi-offset array comment (skipped)'] += 1
                else:
                    refused += 1
                    reasons['no single trailing 0xHEX comment'] += 1
                continue
            if int(m.group('val'), 16) != r['comment']:
                refused += 1
                reasons['comment value != audited value (header drifted)'] += 1
                continue
            new = f"{m.group('pre')}0x{r['compiler']:x}{m.group('post')}"
            if new != s:
                lines[i] = new + '\n'
                dirty = True
                applied += 1
        if dirty and not dry_run:
            with open(path, 'w') as fh:
                fh.write(''.join(lines))
            changed_files.append(hdr)
        elif dirty:
            changed_files.append(hdr)

    print(f"{'DRY RUN -- ' if dry_run else ''}rows applied={applied} "
          f"refused={refused} skipped_multi={skipped_multi} "
          f"files={len(changed_files)}")
    for k, v in reasons.most_common():
        print(f"    {v:5d}  {k}")
    return applied, refused, changed_files


def selftest(project_dir):
    """Prove the pieces discriminate.  A check that cannot fail proves nothing."""
    ok = True

    cases = [
        ('?IsLoaded@MasterAudio@@QAA_NXZ', 'MasterAudio'),
        ('??0BeatMatchSink@@QAA@XZ', 'BeatMatchSink'),
        ('??_GBeatMatchSink@@UAAPAXI@Z', 'BeatMatchSink'),
        ('?Foo@Inner@Outer@@QAAXXZ', 'Inner'),
        ('?f@?$vector@H@std@@QAAXXZ', None),
        ('fn_8277B530', None),
        ('', None),
    ]
    for sym, want in cases:
        got = owner_class(sym)
        status = 'ok ' if got == want else 'FAIL'
        if got != want:
            ok = False
        print(f"  [{status}] owner_class({sym!r:56s}) -> {got!r} (want {want!r})")

    # shift_shape must separate uniform from mixed
    uni = [[1, 'a', 20, 16], [2, 'b', 32, 28]]
    mix = [[1, 'a', 20, 16], [2, 'b', 32, 24]]
    for rows, want in ((uni, 'UNIFORM'), (mix, 'MIXED')):
        k, _d = shift_shape(rows)
        status = 'ok ' if k == want else 'FAIL'
        if k != want:
            ok = False
        print(f"  [{status}] shift_shape -> {k} (want {want})")

    # big-endian decode must find plausible small displacements, and a
    # deliberately little-endian decode must NOT agree with it
    body = struct.pack('>I', (32 << 26) | (3 << 21) | (4 << 16) | 0x0020)
    d, _a = dform_displacements(body)
    status = 'ok ' if d == {0x20} else 'FAIL'
    if d != {0x20}:
        ok = False
    print(f"  [{status}] dform_displacements(lwz r3,0x20(r4)) -> {sorted(d)} (want [32])")

    swapped = struct.pack('<I', (32 << 26) | (3 << 21) | (4 << 16) | 0x0020)
    d2, _ = dform_displacements(swapped)
    status = 'ok ' if d2 != {0x20} else 'FAIL'
    if d2 == {0x20}:
        ok = False
    print(f"  [{status}] byte-order control: LE-encoded word does NOT decode to 0x20 -> {sorted(d2)}")

    # this-tracking must ACCEPT an access off r3 and REJECT one off r1 (stack).
    off_r3 = struct.pack('>I', (32 << 26) | (4 << 21) | (3 << 16) | 0x0018)
    off_r1 = struct.pack('>I', (32 << 26) | (4 << 21) | (1 << 16) | 0x0018)
    d3, n3 = this_displacements(off_r3)
    d1, n1 = this_displacements(off_r1)
    status = 'ok ' if (d3 == {0x18} and n3 == 1) else 'FAIL'
    if not (d3 == {0x18} and n3 == 1):
        ok = False
    print(f"  [{status}] this_displacements(lwz r4,0x18(r3)) -> {sorted(d3)} n={n3} (want [24] n=1)")
    status = 'ok ' if (d1 == set() and n1 == 0) else 'FAIL'
    if not (d1 == set() and n1 == 0):
        ok = False
    print(f"  [{status}] stack control: lwz r4,0x18(r1) is NOT this-relative -> {sorted(d1)} n={n1}")

    # `mr r31,r3` must PROPAGATE this; a clobber of r3 must EVICT it.
    mr = struct.pack('>I', (31 << 26) | (3 << 21) | (31 << 16) | (3 << 11) | (444 << 1))
    prop = mr + struct.pack('>I', (32 << 26) | (4 << 21) | (31 << 16) | 0x0010)
    dp, npv = this_displacements(prop)
    status = 'ok ' if dp == {0x10} else 'FAIL'
    if dp != {0x10}:
        ok = False
    print(f"  [{status}] mr r31,r3 propagates this -> {sorted(dp)} (want [16])")

    clob = struct.pack('>I', (14 << 26) | (3 << 21) | (0 << 16) | 0x0001) \
        + struct.pack('>I', (32 << 26) | (4 << 21) | (3 << 16) | 0x0010)
    dc, ncv = this_displacements(clob)
    status = 'ok ' if dc == set() else 'FAIL'
    if dc != set():
        ok = False
    print(f"  [{status}] clobber control: li r3,1 EVICTS this -> {sorted(dc)} (want [])")

    print("\nSELFTEST", "PASS" if ok else "FAIL")
    return 0 if ok else 1


# --------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--project-dir', default=ROOT)
    ap.add_argument('--findings', default=DEFAULT_FINDINGS)
    ap.add_argument('--report', default=DEFAULT_REPORT)
    ap.add_argument('--class', dest='cls', help='triage only this class')
    ap.add_argument('--bucket', help='list only this bucket')
    ap.add_argument('--bodies', action='store_true',
                    help='add T1 displacement evidence (parses compiled objs)')
    ap.add_argument('--json', help='write full triage to this path')
    ap.add_argument('--top', type=int, default=25)
    ap.add_argument('--structural', metavar='BUCKET',
                    help="refute comments by structural impossibility (no retail "
                         "evidence): NO_WITNESS | UNADJUDICABLE | A_PROVEN | ALL")
    ap.add_argument('--selftest', action='store_true')
    ap.add_argument('--apply', action='store_true',
                    help='rewrite proven-stale // 0xHEX comments (implies --bodies)')
    ap.add_argument('--execute', action='store_true',
                    help='with --apply: actually write (default is a dry run)')
    args = ap.parse_args()
    if args.apply:
        args.bodies = True

    if args.selftest:
        return selftest(args.project_dir)

    fpath = args.findings if os.path.isabs(args.findings) else os.path.join(args.project_dir, args.findings)
    rpath = args.report if os.path.isabs(args.report) else os.path.join(args.project_dir, args.report)
    findings = json.load(open(fpath))['findings']
    rep = load_report(rpath)
    cls_fns = class_function_index(rep)

    tri = run_triage(findings, cls_fns, only=args.cls)
    if not tri:
        print(f"no disagreeing class matched {args.cls!r}")
        return 1

    finder = ObjFinder(args.project_dir) if args.bodies else None
    if finder:
        for t in tri:
            if t['bucket'] == 'A_PROVEN':
                t['evidence'] = body_evidence(t, finder)

    by = collections.Counter()
    rows_by = collections.Counter()
    for t in tri:
        by[t['bucket']] += 1
        rows_by[t['bucket']] += t['rows']

    m = rep['measures']
    print(f"report: matched_functions={m['matched_functions']} "
          f"matched_code={m['matched_code']} total_code={m['total_code']} "
          f"code%={m['matched_code_percent']:.6f}")
    print(f"ruler:  {rep.get('provenance', {}).get('diff_config', {})}")
    print()
    print(f"{'bucket':<16} {'classes':>8} {'rows':>8}")
    for b in ('A_PROVEN', 'NO_WITNESS', 'UNADJUDICABLE'):
        print(f"{b:<16} {by[b]:>8} {rows_by[b]:>8}")
    print(f"{'TOTAL':<16} {sum(by.values()):>8} {sum(rows_by.values()):>8}")

    if args.bucket:
        sel = [t for t in tri if t['bucket'] == args.bucket]
        sel.sort(key=lambda t: -t['rows'])
        print(f"\n--- {args.bucket} (top {args.top} by rows) ---")
        print(f"{'rows':>5} {'shift':<8} {'d':>5} {'100fn':>6} {'ctor':>5}  class / header")
        for t in sel[:args.top]:
            d = '' if t['delta'] is None else f"{t['delta']:+d}"
            print(f"{t['rows']:>5} {t['shift']:<8} {d:>5} {t['n_100']:>6} "
                  f"{t['n_ctor100']:>5}  {t['cls']} :: {t['header']}")

    if args.cls:
        for t in tri:
            print(f"\n=== {t['cls']}  ({t['header']})")
            print(f"    bucket={t['bucket']} shift={t['shift']} delta={t['delta']} "
                  f"rows={t['rows']} fns={t['n_fns']} fuzzy100={t['n_100']} ctor100={t['n_ctor100']}")
            for f in t['best']:
                print(f"      witness {f['size']:>6}B  {f['name']}")
            ev = t.get('evidence')
            if ev:
                print(f"    T1 over {len(ev['fns_used'])} bodies, {ev['n_disp']} distinct displacements")
                print(f"       compiler-offset seen: {ev['hit_compiler']}/{t['rows']}   "
                      f"comment-offset seen: {ev['hit_comment']}/{t['rows']}   "
                      f"NULL seen: {ev['hit_null']}/{t['rows']}")
                for r in ev['rows'][:40]:
                    mark = 'A' if (r['compiler_seen'] and not r['comment_seen']) else \
                           ('?' if r['comment_seen'] else '.')
                    print(f"       [{mark}] {r['member']:<28} comment=0x{r['comment']:x} "
                          f"compiler=0x{r['compiler']:x} seen(c/k/null)="
                          f"{int(r['compiler_seen'])}/{int(r['comment_seen'])}/{int(r['null_seen'])}")

    if args.structural:
        sel = [t for t in tri if t['bucket'] == args.structural] if args.structural != 'ALL' \
            else tri
        sel.sort(key=lambda t: -t['rows'])
        if args.top:
            sel = sel[:args.top]
        print(f"\nstructural-impossibility test over {len(sel)} classes "
              f"(one compile each -- no retail evidence used)\n")
        n_ref = 0
        for t in sel:
            v = structural_verdict(args.project_dir, t['cls'], t['raw_rows'], header=t['header'])
            if v['status'] != 'OK':
                print(f"  {t['cls']:<26} {v['status']} {v.get('detail','')}")
                continue
            imp = v['impossible']
            if imp:
                n_ref += 1
                print(f"  {t['cls']:<26} REFUTED  ({len(imp)} impossible gaps, "
                      f"sizeof={v['size']}, {t['rows']} disagreeing rows)")
                for f in imp[:3]:
                    print(f"      {f['prev']} -> {f['nxt']}: comment gap "
                          f"{f['comment_gap']} < true {f['true_gap']}"
                          f"  ({f['prev_type'] or 'member'})")
            else:
                print(f"  {t['cls']:<26} not refuted by this test "
                      f"(sizeof={v['size']}, {t['rows']} rows)")
        print(f"\ncomments REFUTED without retail evidence: {n_ref} / {len(sel)}")
        return 0

    if args.apply:
        rows = proven_rows(tri)
        print(f"\nproven class-A rows: {len(rows)} "
              f"in {len({r['header'] for r in rows})} headers")
        apply_fixes(args.project_dir, rows, dry_run=not args.execute)

    if args.json:
        with open(args.json, 'w') as fh:
            json.dump(dict(triage=tri, measures=m), fh, indent=1)
        print(f"\nwrote {args.json}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
