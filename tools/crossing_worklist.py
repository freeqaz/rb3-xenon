#!/usr/bin/env python3
"""
crossing_worklist.py -- rank sub-100 rows by SIZE-IF-IT-CROSSES, then adjudicate
each row's actual mismatched instructions into a class, so a worklist says WHY a
row is short and not merely THAT it is.

WHY THIS EXISTS (lane DQ-3, 2026-08-03)
=======================================
`matched_code` is ALL-OR-NOTHING PER ROW: a partial improvement pays exactly
zero.  So the ranking that matters is "bytes this row is worth IF it reaches
fuzzy 100", not "bytes of penalty it currently shows".  DN-4 established that.
What DN-4 could not say is *which* rows will actually cross, and the two obvious
proxies are both broken:

  * penalty-derived "estimated mismatched instructions" is NOT a mismatch count
    (fuzzy gives partial credit per instruction: UIStats estimates 2.6, has 63);
  * objdiff's own `fixability` tier is MISCALIBRATED for at least one large
    class -- it labels COMMUTATIVE_OP_ORDER `likely_fixable`, and lane DQ-3
    proved by experiment that the implied source fix is a NO-OP (below).

So this tool reads the real instruction diff for every row and classifies it.

MEASURED AT eec0cb39 (settled worktree build, report.json regenerated)
---------------------------------------------------------------------
Named rows with 0 < fuzzy < 100:      1,727 rows / 777,104 B
  fuzzy >= 95 band:                     621 rows / 293,760 B
  ... of which <= 3 real mismatches:    367 rows / 100,136 B

Adjudication of the 461-row mm<=3 worklist (103,676 B), by row class:
    COMMUTATIVE (arith operand order)   50 rows  38,680 B   <-- PROVEN INERT
    IMMEDIATE   (const / offset)       161 rows  19,552 B
    STRUCTURAL  (insert/delete/replace)144 rows  18,396 B
    PERMUTED_NONCOMM                    40 rows  14,948 B
    SYMBOL / BRANCH / OPCODE / OTHER    39 rows   8,960 B
    REGALLOC    (BANNED permuter class) 23 rows   3,140 B   <-- only 3.0% of bytes

THE TWO VERDICTS THIS TOOL EXISTS TO RECORD
-------------------------------------------
 * ARITHMETIC operand order (`add`/`fadds`/`fmuls`/`mullw`/`xor`/`lwzx`...):
   76 pure rows / 54,972 B / 0.514 pp of total_code.  **DO NOT FUND.**
   Direct experiment: swapping the source operands of the mismatched `fmuls` in
   `?PollEnabledState@Player@@QAAXM@Z` produced a BYTE-IDENTICAL function -- MSVC
   canonicalises commutative operand order, so the edit is inert.  Worse, the
   largest opcode sub-class (`add`, 31 of 105 sites) is compiler-synthesised
   array addressing (`mulli` + `lwz` + `add` = `&mGems[i]`) where NO source-level
   operand order exists to swap at all.  Context-window clustering gives 93%
   distinct windows against a 75% random-site null => ~90 INDEPENDENT sites, so
   there is no force multiplier either.  Like REGISTER_SWAP before it,
   COMMUTATIVE_OP_ORDER is a SYMPTOM, not a diagnosis.
 * COMPARISON operand order (`cmpw`/`cmplw`, reversed operands):
   14 pure rows / 3,412 B.  **FUND -- proven, but small.**  `cmpw` order is
   directional and is NOT canonicalised, so it tracks source order exactly.
   Proven: `?GetSlot@OvershellPanel@@QAAPAVOvershellSlot@@H@Z` went
   fuzzy 99.524 -> **100.0, 0 mismatches** by rewriting
   `slot == pSlot->GetSlotNum()` as `pSlot->GetSlotNum() == slot`.

INSTRUMENT DISCIPLINE (docs/decomp/INSTRUMENT_DESIGN.md)
--------------------------------------------------------
 * shape 2 (silently-vacuous scanner) -- THIS TOOL'S OWN FIRST DRAFT HAD IT.
   The dumper shelled out via `bash -c '... <<< "{}"'`, so every symbol
   containing `$` (i.e. every C++ TEMPLATE instantiation) was eaten by parameter
   expansion and wrote a ZERO-BYTE file.  204 of 461 rows vanished, 44%, with no
   error -- and the surviving 257 still produced a plausible-looking census.
   `--selftest` asserts that EVERY `$`-bearing row in the live sub-100
   population resolves to a non-empty diff (1,377 rows as measured, not the
   single pinned symbol it used to check); that control fails on the old driver.
 * shape 3 (one-label classifier) -- asserts >= 2 distinct row classes AND that
   both of the two named veins are present, so a degenerate constant classifier
   fails loudly.
 * shape 1 (vacuous control) -- asserts KNOWN POSITIVES by name, one vein each,
   every one hand-verified in its instruction diff and pinned as a LIST so that
   a single upstream fix cannot disarm the control.  Deliberately NOT selected
   dynamically: the only selector available would be the classifier under test,
   which makes the control a tautology.  See CONTROL SUBJECTS below.
 * shape 4 (a control that cannot fail is not a passing control) -- a pin that
   has left the population is reported DISARMED and exits 3, never PASS.
 * RULER SPLIT -- `objdiff-cli diff` and `objdiff-cli report generate` do NOT
   agree: 123/1,727 rows (7.1%) differ, ALWAYS with report >= diff, up to
   +14.75 pp.  Band membership is therefore taken from report.json (the
   authoritative ruler, per CLAUDE.md) and the tool REFUSES if the split would
   move >2% of rows across the band boundary.
 * INPUT STABILITY -- a run whose inputs moved under it has MEASURED NOTHING,
   and saying "nothing was measured" is not the same as saying "the regression
   is back".  VOID is a third outcome with its own exit code.  See the INPUT
   STABILITY block below.

EXIT CODES
----------
    0  PASS      every control ran, on inputs that held still, and held
    1  REFUSE    a precondition failed, or --adjudicate would print a PARTIAL
                 census on a tree that held still (unchanged, pre-existing)
    2  FAIL      a --selftest control failed on evidence that did NOT move
    3  DISARMED  a pinned control has left the population; nothing was proven
    4  VOID      the inputs moved under the run; NOTHING WAS MEASURED

VOID is a third outcome, distinct from both PASS and FAIL, and it is the point
of the input-stability guard: a run whose inputs moved has measured nothing,
and "nothing was measured" is not "the regression is back".

USAGE
-----
    python3 tools/crossing_worklist.py --selftest
    python3 tools/crossing_worklist.py --census        [--project-dir DIR]
    python3 tools/crossing_worklist.py --adjudicate    [--max-mismatch 3]
    python3 tools/crossing_worklist.py --reclaim
"""
import argparse, collections, hashlib, json, os, re, subprocess, sys, threading, time
import concurrent.futures as cf

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ANON = re.compile(r'^fn_[0-9A-Fa-f]{8}$')

CMP_OPS = {'cmpw', 'cmplw', 'cmpd', 'cmpld', 'fcmpu', 'fcmpo'}
ARITH_OPS = {'add', 'addc', 'and', 'or', 'xor', 'eqv', 'nand', 'nor', 'mullw',
             'mulhw', 'mulhwu', 'fadd', 'fadds', 'fmul', 'fmuls', 'fsub', 'fsubs',
             'fmadds', 'fmsubs', 'fnmadds', 'fnmsubs',
             'lwzx', 'lbzx', 'lhzx', 'lfsx', 'lfdx', 'stwx'}

# ---------------------------------------------------------------------------
# CONTROL SUBJECTS -- and why two controls choose their subjects two DIFFERENT
# ways.  This is the deliberate answer to the re-pinning left open by task93.
# ---------------------------------------------------------------------------
# control 1 (shape 2, the silent-empty-diff guard) selects DYNAMICALLY from the
#   live population, and there is no constant to rot.  Its subject predicate --
#   "the mangled name contains '$'", i.e. a C++ template instantiation -- is
#   LEXICAL: it is read off report.json without running classify_arms or
#   anything else under test.  Selection and assertion ("...and it yields a
#   non-empty instruction stream") are therefore independent, so the control is
#   not circular, and it can cover the WHOLE '$'-bearing population instead of
#   one hand-picked symbol -- 1,377 rows as measured, against the 1 it used to
#   check.  It disarms only if templates vanish from the sub-100 population
#   entirely, which is a real event worth hearing about.
#
# control 1b (shape 1, the known-positive guard) stays a NAMED PIN, and that is
#   a deliberate REFUSAL of the same treatment.  The only available selector
#   for "a pure CMP_REVERSAL row" is classify_arms -- the function under test.
#   A control that picks its own subject with the code it is testing and then
#   asserts that code's verdict cannot fail: it is precisely the "confirms
#   whatever you point it at" class that tools/screen_gate.py exists to warn
#   about, and it would quietly convert a control into a tautology.  A known
#   positive has to be ratified by a human reading the instruction diff.  So it
#   stays pinned and the rot is accepted as the price of non-circularity.
#
#   Two mitigations for that rot, neither of which reintroduces circularity:
#     * each vein pins a LIST, tried in order, so one upstream fix no longer
#       disarms the control -- only exhausting the whole list does.  The
#       entries are spread across distinct units so that fixing one unit cannot
#       take them all.
#     * when a list IS exhausted, the DISARMED message prints classifier-
#       nominated replacements.  Those are SUGGESTIONS FOR A HUMAN TO VERIFY
#       AND PIN, never assertions -- which is exactly what keeps them honest.
#
# Every symbol below was verified by READING its objdiff instruction diff (a
# single diff_arg row, same opcode, register operands swapped), not by asking
# classify_arms.  Anonymous-namespace symbols (`?A0x<hash>@`) are excluded on
# purpose: that hash is build-dependent, so it is not a stable pin.
KNOWN_CMP = (
    '?DeterminePhraseTimes@VocalNoteList@@QAAXABVTempoMap@@@Z',        # cmplw cr6,r30,r10 <- r10,r30
    '?Dispatch@EnterFlowMsg@@UAAXXZ',                                  # cmpw  cr6,r3,r11  <- r11,r3
    '?MaybeAutoplayFutureCymbal@TrackWatcherImpl@@QAAXH@Z',            # cmpw  cr6,r11,r10 <- r10,r11
    '?SetState@NetSession@@QAAXW4SessionState@1@@Z',                   # cmplw cr6,r11,r10 <- r10,r11
    '?TrackNumOfExactType@PlayerTrackConfigList@@QAAHW4TrackType@@@Z',  # cmpw  cr6,r8,r4   <- r4,r8
)
KNOWN_ARITH = (
    '?ParseNode@@YA_NXZ',                                              # add   r3,r30,r11  <- r11,r30
    '?Update@MicInputArrow@@UAAXXZ',                                   # add   r3,r11,r28  <- r28,r11
    '?HandlePhraseEnd@VocalPart@@QAAXAAHAAM10M@Z',                     # mullw r10,r29,r3  <- r3,r29
    '?Poll@BandIKEffector@@UAAXXZ',                                    # fmuls f0,f11,f0   <- f0,f11
    '?ProcessInPlace@Synapse@1DSP@@QAAXIPAM@Z',                        # add   r3,r11,r29  <- r29,r11
)


def report_path(project_dir):
    return os.path.join(project_dir, 'build', '45410914', 'report.json')


def load_rows(project_dir):
    """Named rows with 0 < fuzzy < 100, from report.json (the AUTHORITATIVE ruler)."""
    d = json.load(open(report_path(project_dir)))
    rows = []
    for u in d['units']:
        for f in u.get('functions') or []:
            if ANON.match(f['name']):
                continue
            fz = f.get('fuzzy_match_percent')
            if fz is None or float(fz) <= 0.0 or float(fz) >= 100.0:
                continue
            rows.append(dict(unit=u['name'], sym=f['name'], size=int(f.get('size', 0) or 0),
                             fz=float(fz), mpn=float(f.get('match_percent_normalized') or 0.0)))
    return d['measures'], rows


def objdiff_bin(project_dir):
    p = os.path.join(project_dir, 'bin', 'objdiff-cli')
    if not os.path.exists(p):
        sys.exit(f'REFUSE: objdiff-cli not found at {p}')
    return p


# ---------------------------------------------------------------------------
# INPUT STABILITY -- VOID is a THIRD outcome, not a flavour of FAIL (task #115)
# ---------------------------------------------------------------------------
# Task #105 made control 1 dynamic: it diffs EVERY '$'-bearing row in the live
# population.  That made it sensitive to something it cannot see -- the tree
# moving underneath it.  Peer agents rebuild the primary checkout constantly;
# objdiff-cli reading an .obj mid-write returns an empty diff, so the control
# reported "the shell-quoting regression (shape 2) is back" for a transient.
# Measured 2026-08-17 on the primary checkout: run 1 = 10 misses / 2 controls
# failed / exit 2; run 2 on the SAME code and SAME tree = 0 misses / exit 0.
# All four sampled "missing" symbols diffed cleanly on direct retry, and
# report.json's mtime moved mid-run.
#
# A control that goes red whenever a peer builds trains people to ignore it,
# which is exactly how a real shape-2 regression gets waved off.  So follow the
# in-house precedent -- compare_bins_v2.sh's input-stability guard, which
# fingerprints the object trees and map files around its two arms and prints
# `*** VOID ***` if they moved rather than printing a number that looks like a
# result.  VOID is distinct from BOTH pass and fail: a run whose inputs moved
# has measured nothing, and "nothing was measured" is not "the regression is
# back".
#
# Three mechanisms, deliberately layered cheapest-first:
#
#   1. RETRY (diff_one).  A miss is retried once.  A torn read is transient and
#      the retry fixes it; the shell-quoting bug is deterministic and the retry
#      does NOT fix it.  That asymmetry is the cheapest discriminator we have,
#      and it is why retrying does not launder a real defect.
#   2. CACHE STAMPING (diff_one).  The cache entry carries the (size, mtime_ns)
#      of the unit's object files.  Without this the guard would be theatre:
#      it would notice the tree moved while the cache quietly answered from the
#      tree before it.  An entry minted against a different build is a
#      different measurement, so it is recomputed, not served.
#   3. FINGERPRINT (InputStability).  Snapshot before, re-scan after, and
#      attribute.  Per-ROW where possible, not one global verdict: a miss whose
#      own unit's objects moved is unavailable, a miss in a unit that held
#      still is a defect.  That attribution is what keeps a peer's rebuild of
#      three units from voiding a run that caught a real regression in the
#      other 2,500.
#
# PRECEDENCE, and why: FAIL outranks VOID.  A guard that voids everything is
# worse than no guard, and downgrading a stable-tree failure to "nothing was
# measured" is precisely the laundering this is supposed to prevent.  So a
# control that failed on evidence the tree did not move still exits 2, even if
# something else in the tree moved.  VOID outranks DISARMED, and both outrank
# PASS.
#
# ASYMMETRY between global and per-unit movement, on purpose:
#   * GLOBAL inputs -- report.json, objdiff.json, the map file, the objdiff-cli
#     binary -- define WHAT was measured and WITH WHAT.  If one of those moved,
#     the population we enumerated and the diffs we ran come from different
#     builds, so the run is VOID even if every control was green.  This is the
#     only unconditional VOID.
#   * PER-UNIT object movement is only void-worthy where it EXPLAINS a miss.
#     A unit that rebuilt while every row still diffed is a NOTE, not a VOID.
#     Voiding on any object touch would leave the tool permanently red in this
#     repo, which is the failure mode being fixed, not a stricter version of
#     the fix.  This deliberately parts company with compare_bins_v2.sh, which
#     voids on any object movement -- it has to, because it has TWO arms and
#     movement between them flips the sign of its answer.  There is no second
#     arm here for movement to bias.
EXIT_FAIL, EXIT_DISARMED, EXIT_VOID = 2, 3, 4

# Cache format version.  v1 entries were the raw diff blob with no record of
# which build produced them; they are not evidence about the current tree and
# are recomputed rather than trusted.
CACHE_FORMAT = 2

# Pause before retrying a miss.  Paid only by misses.
RETRY_PAUSE_S = 0.25


def _sig(path):
    """mtime+size, not content.  Content-hashing every .obj costs more than the
    measurement it guards, and mtime+size catches a rebuild (compare_bins_v2)."""
    try:
        st = os.stat(path)
    except OSError:
        return None
    return [st.st_size, st.st_mtime_ns]


class InputStability:
    """Fingerprint the run's inputs before it starts and again after it ends."""

    def __init__(self, project_dir):
        self.project_dir = project_dir
        cfg_path = os.path.join(project_dir, 'objdiff.json')
        cfg = json.load(open(cfg_path))
        self.unit_paths = {}
        for u in cfg.get('units') or []:
            self.unit_paths[u['name']] = [
                os.path.join(project_dir, u[k])
                for k in ('target_path', 'base_path') if u.get(k)]
        self.global_paths = {'report.json': report_path(project_dir),
                             'objdiff.json': cfg_path}
        if cfg.get('map_file'):
            self.global_paths['map_file'] = os.path.join(project_dir, cfg['map_file'])
        # the ruler itself: bin/objdiff-cli is a symlink into a shared build
        # tree, so stat the RESOLVED binary -- a relink under the run changes
        # what we measured with, and is exactly as void-worthy as a rebuild.
        self.global_paths['objdiff-cli'] = os.path.realpath(objdiff_bin(project_dir))
        self.moved_globals, self.moved_units = [], set()
        self.before = self._scan()
        self.after = None

    def _scan(self):
        return {'globals': {k: _sig(p) for k, p in self.global_paths.items()},
                'units': {u: [_sig(p) for p in ps] for u, ps in self.unit_paths.items()}}

    def recheck(self):
        self.after = self._scan()
        self.moved_globals = sorted(k for k, v in self.before['globals'].items()
                                    if self.after['globals'].get(k) != v)
        self.moved_units = {u for u, v in self.before['units'].items()
                            if self.after['units'].get(u) != v}
        return self

    def unit_sig(self, unit):
        return self.before['units'].get(unit)

    def row_moved(self, unit):
        """Did anything this row's verdict depends on move under the run?"""
        return bool(self.moved_globals) or unit in self.moved_units

    def report(self, out=sys.stdout):
        if not (self.moved_globals or self.moved_units):
            print('INPUT STABILITY: inputs held still for the whole run.', file=out)
            return
        print('INPUT STABILITY: inputs MOVED under this run --', file=out)
        for k in self.moved_globals:
            print(f'    global input changed: {k} '
                  f'({self.before["globals"][k]} -> {self.after["globals"][k]})', file=out)
        if self.moved_units:
            shown = sorted(self.moved_units)[:5]
            print(f'    {len(self.moved_units)} unit(s) rebuilt: '
                  f'{", ".join(shown)}{" ..." if len(self.moved_units) > 5 else ""}', file=out)


class Counters:
    def __init__(self):
        self._c, self._lk = collections.Counter(), threading.Lock()

    def bump(self, k, n=1):
        with self._lk:
            self._c[k] += n

    def __getitem__(self, k):
        return self._c[k]


def diff_one(project_dir, sym, unit, cache_dir, unit_sig=None, retries=1, stats=None):
    """Run objdiff-cli via argv ONLY -- never through a shell.  See shape 2 above.

    Retries a miss once and stamps the cache with the unit's object signature;
    see mechanisms 1 and 2 in the INPUT STABILITY block.
    """
    os.makedirs(cache_dir, exist_ok=True)
    h = hashlib.md5((sym + '\x00' + unit).encode()).hexdigest()[:20]
    p = os.path.join(cache_dir, h + '.json')
    if os.path.exists(p) and os.path.getsize(p) > 0:
        try:
            blob = json.load(open(p))
        except ValueError:
            blob = None
        if (isinstance(blob, dict) and blob.get('_cw_cache') == CACHE_FORMAT
                and (unit_sig is None or blob.get('inputs') == unit_sig)):
            return blob['diff']
        # v1 entry, torn write, or a stamp from a different build: not evidence
        # about THIS tree.  Fall through and recompute.
        if stats is not None:
            stats.bump('cache_stale')
    argv = [objdiff_bin(project_dir), 'diff', sym, '-u', unit,
            '--include-instructions', '-c', 'functionRelocDiffs=none', '-f', 'json']
    for attempt in range(retries + 1):
        r = subprocess.run(argv, cwd=project_dir, capture_output=True)
        if r.returncode == 0 and r.stdout.strip():
            if attempt and stats is not None:
                stats.bump('rescued_by_retry')
            d = json.loads(r.stdout)
            # atomic: two lanes may share a cache dir, and a half-written entry
            # reads back as a miss -- i.e. as the very defect being guarded.
            tmp = f'{p}.{os.getpid()}.tmp'
            with open(tmp, 'w') as fh:
                json.dump({'_cw_cache': CACHE_FORMAT, 'inputs': unit_sig, 'diff': d}, fh)
            os.replace(tmp, p)
            return d
        if stats is not None:
            stats.bump('retried' if attempt < retries else 'missed')
        if attempt < retries:
            # a beat, so the retry lands AFTER a writer that is mid-file rather
            # than on top of it.  Only misses pay it, so it costs nothing on a
            # healthy run and nothing on a deterministic defect either.
            time.sleep(RETRY_PAUSE_S)
    return None


def diff_many(project_dir, rows, cache_dir, stab, stats=None, workers=8):
    """diff_many_tolerant, but a miss is terminal -- with the RIGHT diagnosis.

    A partial dump yields a plausible but WRONG census (shape 2), so this never
    returns one and never lets one be printed.  What changes in task #115 is
    not the fatality but the LABEL and the exit code: a miss whose unit's
    objects moved under the run -- or any run whose global inputs moved -- has
    MEASURED NOTHING and exits VOID (4).  A miss on a tree that held still is a
    real defect and exits REFUSE (1), unchanged.  Reporting the first as the
    second is what trains people to wave off the second.
    """
    out = diff_many_tolerant(project_dir, rows, cache_dir, stab, stats, workers)
    stab.recheck()
    miss = [r for r, o in zip(rows, out) if o is None]
    stable_miss = [r for r in miss if not stab.row_moved(r['unit'])]
    if miss:
        stab.report(sys.stderr)
        if stable_miss:
            for r in stable_miss[:5]:
                print(f'    STABLE MISS: {r["unit"]}  {r["sym"][:70]}', file=sys.stderr)
            sys.exit(f'REFUSE: {len(miss)}/{len(rows)} diffs produced no output, '
                     f'{len(stable_miss)} of them on inputs that did NOT move. '
                     f'A partial dump yields a plausible but WRONG census (shape 2).')
        print(f'*** VOID *** {len(miss)}/{len(rows)} diffs were unavailable and EVERY '
              f'one of them\nis in a unit whose objects moved under the run (or the run\'s '
              f'global inputs\nmoved). This census MEASURED NOTHING -- it is not evidence '
              f'of a defect.\nRe-run on a settled tree; the diff cache makes the re-run '
              f'cheap.', file=sys.stderr)
        sys.exit(EXIT_VOID)
    if stab.moved_globals:
        stab.report(sys.stderr)
        print('*** VOID *** the population (report.json) or the ruler moved under this '
              'run,\nso the rows enumerated and the diffs measured come from different '
              'builds.\nNo census is printed. Re-run on a settled tree.', file=sys.stderr)
        sys.exit(EXIT_VOID)
    if stab.moved_units:
        stab.report(sys.stderr)
        print('NOTE: those units rebuilt but every row still resolved, so the census '
              'below\nis complete; some rows were read from a newer build than the rest.',
              file=sys.stderr)
    return out


def diff_many_tolerant(project_dir, rows, cache_dir, stab=None, stats=None, workers=8):
    """diff_many, but hands the misses back as DATA instead of exiting on them.

    cmd_adjudicate must REFUSE on a partial dump -- a plausible but WRONG
    census is the whole point of shape 2.  The SELFTEST must not: a
    silent-empty-diff regression is the very thing control 1 exists to CATCH,
    and aborting inside diff_many would kill the run before the control could
    name it, reporting a generic REFUSE where a named control failure belongs.
    So the selftest takes the misses as evidence and lets its controls
    adjudicate them (control 1 for template rows, control 2 for the rest --
    together they reproduce diff_many's guarantee, so nothing is given up).
    """
    sig = stab.unit_sig if stab is not None else (lambda _u: None)
    with cf.ThreadPoolExecutor(max_workers=workers) as ex:
        return list(ex.map(lambda r: diff_one(project_dir, r['sym'], r['unit'], cache_dir,
                                              unit_sig=sig(r['unit']), stats=stats), rows))


def classify_arms(diff):
    """One label per mismatched instruction."""
    arms = []
    for i in diff.get('instructions') or []:
        m = i.get('match_type')
        if m in (None, 'equal'):
            continue
        t, b = i.get('target') or {}, i.get('base') or {}
        top, bop = t.get('opcode'), b.get('opcode')
        args = (i.get('diff_breakdown') or {}).get('arguments') or []
        types = {a.get('arg_type') for a in args}
        tv = [a['value'] for a in (t.get('typed_args') or []) if a.get('type') == 'Register']
        bv = [a['value'] for a in (b.get('typed_args') or []) if a.get('type') == 'Register']
        reversed_regs = (m == 'diff_arg' and top == bop and tv
                         and sorted(tv) == sorted(bv) and tv != bv)
        op = (top or '').rstrip('.')
        if m in ('insert', 'delete', 'replace'):
            arms.append('STRUCTURAL')
        elif m == 'diff_op':
            arms.append('OPCODE')
        elif reversed_regs and op in CMP_OPS:
            arms.append('CMP_REVERSAL')
        elif reversed_regs and op in ARITH_OPS:
            arms.append('ARITH_COMMUTE')
        elif reversed_regs:
            arms.append('OTHER_REVERSAL')
        elif 'branch_dest' in types:
            arms.append('BRANCH')
        elif 'symbol' in types:
            arms.append('SYMBOL')
        elif 'immediate' in types:
            arms.append('IMMEDIATE')
        elif types == {'register'}:
            arms.append('REGALLOC')
        else:
            arms.append('OTHER')
    return arms


def ruler_gate(rows, diffs):
    """`diff` and `report generate` disagree. Quantify, and refuse if it matters."""
    dis = flip = 0
    worst = 0.0
    for r, d in zip(rows, diffs):
        db = d['fuzzy_match_percent']
        if abs(db - r['fz']) > 1e-3:
            dis += 1
            worst = max(worst, r['fz'] - db)
            if (r['fz'] >= 95) != (db >= 95):
                flip += 1
    print(f"RULER SPLIT: diff-vs-report disagreements {dis}/{len(rows)} "
          f"({100*dis/len(rows):.1f}%), worst report-minus-diff {worst:+.2f} pp, "
          f"band flips {flip} ({100*flip/len(rows):.2f}%)")
    if flip / max(len(rows), 1) >= 0.02:
        sys.exit('REFUSE: ruler split would move >=2% of rows across the band boundary.')


def cmd_census(a):
    M, rows = load_rows(a.project_dir)
    TOT = sum(r['size'] for r in rows)
    print(f"report: total_code {int(M['total_code']):,}  total_functions {M['total_functions']:,}  "
          f"matched_code {int(M['matched_code']):,}  matched_functions {M['matched_functions']:,}")
    print(f"named rows with 0<fuzzy<100: {len(rows):,}   value-if-all-crossed {TOT:,} B\n")

    def pen(rs):
        return sum(r['size'] * (100.0 - r['fz']) / 100.0 for r in rs)
    P = pen(rows)
    print(f"{'thresh':>7} {'rows':>6} {'value B':>10} {'%val':>7} {'penalty':>9} {'%pen':>7} {'v/p':>7}")
    for t in (99, 98, 97, 96, 95, 90, 85, 80, 50, 0):
        b = [r for r in rows if r['fz'] >= t and r['mpn'] < 100.0]
        v, q = sum(r['size'] for r in b), pen(b)
        pv, pp = 100 * v / TOT, 100 * q / P
        print(f"{t:>7} {len(b):>6} {v:>10,} {pv:>6.2f}% {q:>9,.0f} {pp:>6.2f}% {pv/pp if pp else 0:>6.2f}x")
    print("\n  NOTE: v/p rises monotonically as the threshold rises -- that is ARITHMETIC\n"
          "  (penalty -> 0 as fuzzy -> 100), NOT evidence the band is a good vein.\n"
          "  Use --adjudicate for the non-arithmetic justification.")


def cmd_adjudicate(a):
    """The REFUSE here stays deliberately fatal, and stays UNCONDITIONAL in the
    thing that matters: no partial census is ever printed, whatever moved.  All
    task #115 changes is the diagnosis -- see diff_many.  Keeping it fatal but
    mislabelled was the worse option: the operator reads "REFUSE: 10/3618 diffs
    produced no output" as a defect in the tool or the tree, and the next real
    partial dump reads the same way."""
    M, rows = load_rows(a.project_dir)
    stab = InputStability(a.project_dir)
    stats = Counters()
    cache = os.path.join(a.cache_dir, 'diffs')
    print(f"diffing {len(rows)} rows (cache {cache}) ...", file=sys.stderr)
    diffs = diff_many(a.project_dir, rows, cache, stab, stats)
    if stats['rescued_by_retry']:
        print(f"note: {stats['rescued_by_retry']} row(s) missed on the first attempt and "
              f"resolved on retry (transient, not a defect)", file=sys.stderr)
    ruler_gate(rows, diffs)
    for r, d in zip(rows, diffs):
        r['arms'] = classify_arms(d)
        r['mm'] = len(r['arms'])
        r['cls'] = collections.Counter(r['arms']).most_common(1)[0][0] if r['arms'] else 'NONE'
        r['pure'] = len(set(r['arms'])) == 1

    sel = [r for r in rows if 0 < r['mm'] <= a.max_mismatch]
    TB = sum(r['size'] for r in sel)
    print(f"\n=== worklist: rows with <= {a.max_mismatch} real mismatches: "
          f"{len(sel)} rows / {TB:,} B ===")
    cb, cn = collections.Counter(), collections.Counter()
    for r in sel:
        cb[r['cls']] += r['size']; cn[r['cls']] += 1
    for k, v in cb.most_common():
        print(f"  {k:>18} {cn[k]:>4} rows {v:>8,} B  {100*v/TB:>5.1f}%")

    print("\n=== PURE veins over ALL named sub-100 rows (a pure row should CROSS) ===")
    for label, want, note in (('CMP_REVERSAL', 'CMP_REVERSAL', 'PROVEN fixable'),
                              ('ARITH_COMMUTE', 'ARITH_COMMUTE', 'PROVEN INERT -- do not fund'),
                              ('IMMEDIATE', 'IMMEDIATE', 'const/offset; mixed'),
                              ('REGALLOC', 'REGALLOC', 'BANNED permuter class')):
        p = [r for r in rows if r['arms'] and set(r['arms']) == {want}]
        v = sum(r['size'] for r in p)
        print(f"  PURE {label:<15} {len(p):>4} rows {v:>8,} B  "
              f"({100*v/int(M['total_code']):.4f} pp)   {note}")

    print(f"\n=== top {a.top} of the worklist by size-if-it-crosses ===")
    for r in sorted(sel, key=lambda r: -r['size'])[:a.top]:
        print(f"{r['size']:>7} B  mm={r['mm']}  fz={r['fz']:>7.3f}  {r['cls']:<16} "
              f"{r['unit'][:32]:<32} {r['sym'][:58]}")


def cmd_reclaim(a):
    d = json.load(open(report_path(a.project_dir)))
    units = []
    for u in d['units']:
        fns = u.get('functions') or []
        if not fns:
            continue
        res = [f for f in fns if float(f.get('fuzzy_match_percent') or 0.0) < 100.0]
        if not res:
            continue
        anon = [f for f in res if ANON.match(f['name'])]
        units.append(dict(name=u['name'], res=len(res), anon=len(anon),
                          named=len(res) - len(anon),
                          resb=sum(int(f.get('size', 0) or 0) for f in res)))
    blocked = [u for u in units if u['anon']]
    reach = [u for u in units if not u['anon']]
    print(f"units with residue: {len(units)}")
    print(f"  BLOCKED by anon residue (can NEVER reach 100%): {len(blocked)} "
          f"({100*len(blocked)/len(units):.1f}%)")
    print(f"  reachable:                                      {len(reach)} "
          f"({100*len(reach)/len(units):.1f}%), {sum(u['resb'] for u in reach):,} B residue")
    trap = [u for u in blocked if u['named'] <= 2]
    print(f"\n*** THE TRAP: {len(trap)} units have <=2 NAMED residue rows but >=1 ANON row.")
    print("    A 'within N rows of 100%' worklist that does not filter anon content")
    print(f"    would be {100*len(trap)/(len(trap)+len([u for u in reach if u['res']<=2])):.0f}% false.")
    print("\n=== reachable units with <=3 residue rows, by residue bytes ===")
    for u in sorted([x for x in reach if x['res'] <= 3], key=lambda x: -x['resb'])[:25]:
        print(f"  {u['resb']:>6,} B  {u['res']} row(s)  {u['name']}")


def cmd_selftest(a):
    """Controls that MUST be able to fail.  --self-break proves they do.

    ⚠ A control whose pinned constant has left the population is DISARMED, not
    passing (lane task93, 2026-08-16). The named-known-positive controls used to
    print `SKIP` and fall through without touching `fails`, so the run still
    ended on "SELFTEST PASSED (and every control above can fail)" -- a claim that
    is false for a control that did not run.

    That was not hypothetical. Measured against build/45410914/report.json on the
    primary checkout: the then-pinned KNOWN_CMP (?Swing@DrumTrackWatcherImpl@@..)
    and KNOWN_DOLLAR (??$_Copy_Construct@UEventSink@MsgSource@@..) had both
    reached 100.0% (fixed upstream) and left the sub-100 population, so TWO of
    the four named controls -- including the shape-2 guard against the
    silent-empty-diff regression that once ate 204 of 461 rows -- were already
    disarmed and invisible.

    Reaching 100% is good news, so a disarmed control is not a FAILURE. It is
    also not a pass: the shape it guarded is no longer guarded until the constant
    is re-pinned. Following the house idiom in tools/screen_gate.py -- "an
    untestable screen is NOT a passing screen; this run exits non-zero" -- a
    disarmed control is reported separately and exits 3.

    Task #105 (2026-08-17) supplies the re-pinning task93 deliberately left
    open, and does it two different ways on purpose. Control 1 no longer has a
    constant at all: it selects every '$'-bearing row in the live population by
    a LEXICAL predicate, so it cannot rot and cannot be circular. Control 1b
    keeps human-ratified named pins, now LISTS rather than single symbols,
    because the only dynamic selector available to it is the classifier it is
    testing. See the CONTROL SUBJECTS block at the top of this file.

    Task #115 (2026-08-17) supplies what task #105 could not see. Making
    control 1 dynamic made it sensitive to the tree moving underneath it: a
    peer rebuilding the primary checkout makes objdiff read objects mid-write,
    and the control reported "the shell-quoting regression (shape 2) is back"
    for a transient. Measured: run 1 = 10 misses / exit 2, run 2 on the SAME
    code and SAME tree = 0 misses / exit 0. So a control can now come back
    VOID (exit 4) as well as PASS, FAIL and DISARMED -- and FAIL outranks VOID,
    so a stable-tree failure is never laundered into "nothing was measured".
    See the INPUT STABILITY block at the top of this file.
    """
    fails = []
    disarmed = []
    voids = []
    M, rows = load_rows(a.project_dir)
    by = {r['sym']: r for r in rows}
    unit_of = {r['sym']: r['unit'] for r in rows}
    cache = os.path.join(a.cache_dir, 'diffs')

    # Fingerprint the inputs BEFORE the diff pass and again after it, so a
    # control that goes red can say whether the tree held still while it did.
    # See the INPUT STABILITY block at the top of this file.
    stab = InputStability(a.project_dir)
    stats = Counters()

    # ONE diff pass over the whole sub-100 population; every control below
    # reads it.  Misses are DATA here rather than a REFUSE -- see the docstring
    # of diff_many_tolerant for why the selftest must not abort on them.
    diffs = diff_many_tolerant(a.project_dir, rows, cache, stab, stats)
    stab.recheck()
    diff_of = {r['sym']: d for r, d in zip(rows, diffs)}
    if stats['rescued_by_retry']:
        print(f"  note: {stats['rescued_by_retry']} row(s) missed on the first attempt "
              f"and resolved on retry -- transient, not counted as misses")

    def adjudicate(name, subjects, message, moved_message):
        """Route a control's failure by whether its EVIDENCE moved under the run.

        `subjects` are the rows the control failed ON.  If every one of them is
        in a unit that moved (or any global input moved), the control did not
        observe a defect -- it observed a tree in motion, and says VOID.  If any
        subject's inputs held still, that is real and stays a FAIL: FAIL
        outranks VOID, so a guard can never launder a stable-tree failure."""
        stable = [s for s in subjects if not stab.row_moved(unit_of.get(s, s))]
        if stable:
            print(f"  FAIL  {name}: {message(len(stable), len(subjects))}")
            for s in stable[:5]:
                print(f"          {s[:86]}")
            fails.append(message(len(stable), len(subjects)))
        else:
            print(f"  VOID  {name}: {moved_message(len(subjects))}")
            voids.append(f'{name}: {moved_message(len(subjects))}')

    # -- control 1 (shape 2): EVERY '$'-bearing template symbol in the live
    #    population must produce a non-empty diff.  The first driver shelled
    #    out through bash, so '$' was eaten by parameter expansion and 204 of
    #    461 rows silently came back EMPTY.  Subject selection is lexical, so
    #    this control cannot be disarmed by a row reaching 100% -- only by the
    #    template population emptying out completely.
    dollar = [r for r in rows if '$' in r['sym']]
    if not dollar:
        print("  DISARMED  control 1 (template symbols resolve): the sub-100 "
              "population contains NO '$'-bearing symbol")
        disarmed.append('control 1 (shape 2, template symbols): no template '
                        'instantiation is sub-100 any more, so the '
                        'silent-empty-diff shape is unguarded. Widen the '
                        'population or retire the control deliberately -- do '
                        'not just delete it.')
    else:
        empty = [r['sym'] for r in dollar
                 if a.self_break or not ((diff_of.get(r['sym']) or {}).get('instructions'))]
        if not empty:
            print(f"  PASS  control 1 (template symbols resolve): "
                  f"{len(dollar)}/{len(dollar)} '$'-bearing symbols produced a diff")
        else:
            adjudicate(
                'control 1 (template symbols resolve)', empty,
                lambda n, t: (f"{n}/{len(dollar)} '$'-bearing symbols produced no diff on "
                              f"inputs that did NOT move ({t} unavailable in total) -- "
                              f'a real silent-empty-diff defect; the shell-quoting '
                              f'regression (shape 2) is the known cause, an unreadable '
                              f'object is the other one'),
                lambda t: (f"{t}/{len(dollar)} '$'-bearing symbols were unavailable, and "
                           f"every one of them\n        sits behind an input that moved "
                           f"under this run. NOTHING WAS MEASURED here;\n        this is "
                           f"NOT evidence the shell-quoting regression is back."))

    # -- control 2: the same guarantee for everything else.  The selftest reads
    #    its diffs tolerantly, so without this a missing NON-template diff would
    #    slip through where diff_many would have REFUSED.  Controls 1 + 2
    #    together restore that invariant.
    rest = [r for r in rows if '$' not in r['sym']]
    gone = [r['sym'] for r in rest if diff_of.get(r['sym']) is None]
    if not gone:
        print(f"  PASS  control 2 (non-template rows resolve): "
              f"{len(rest)}/{len(rest)} resolved")
    else:
        adjudicate(
            'control 2 (non-template rows resolve)', gone,
            lambda n, t: (f'{n}/{len(rest)} non-template rows produced no diff on inputs '
                          f'that did NOT move ({t} unavailable in total) -- a partial '
                          f'dump yields a plausible but WRONG census'),
            lambda t: (f'{t}/{len(rest)} non-template rows were unavailable, and every '
                       f"one of them\n        sits behind an input that moved under this "
                       f'run -- a racing rebuild, not a defect.'))

    # -- control 3 (shape 3): the classifier must emit >= 2 labels, and BOTH
    #    named veins must be present.  A constant classifier fails here.
    #    --self-break installs exactly that: a degenerate classifier that calls
    #    every mismatch STRUCTURAL, which must take 3, 3b AND 1b red.
    labels = collections.Counter()
    cls_of = {}
    for rr in rows:
        dd = diff_of.get(rr['sym'])
        if dd is None:
            continue
        arms = classify_arms(dd)
        if a.self_break:
            arms = ['STRUCTURAL'] * len(arms)
        labels.update(arms)
        cls_of[rr['sym']] = (arms, len(set(arms)) == 1)
    # A classifier verdict is only as good as the diffs it read.  If the
    # POPULATION or the RULER moved (a global input), a red here says nothing;
    # per-unit churn does not reach a statistic taken over thousands of rows,
    # so it is not grounds to void one.
    def verdict(name, ok, msg):
        if ok:
            return
        if stab.moved_globals:
            print(f"          VOID: {', '.join(stab.moved_globals)} moved under this run, "
                  f"so the diffs behind this verdict\n          are not a measurement of "
                  f"any one build")
            voids.append(f'{name}: {msg} (global inputs moved)')
        else:
            fails.append(msg)

    def mark(ok):
        return 'PASS' if ok else ('VOID' if stab.moved_globals else 'FAIL')

    nlab = len([k for k, v in labels.items() if v])
    ok = nlab >= 2
    print(f"  {mark(ok)}  control 3 (not a one-label classifier): {nlab} distinct labels")
    verdict('control 3 (not a one-label classifier)', ok,
            f'classifier emitted {nlab} label(s) -- constant function')
    for want in ('CMP_REVERSAL', 'ARITH_COMMUTE'):
        ok = labels.get(want, 0) > 0
        print(f"  {mark(ok)}  control 3b (vein '{want}' present): n={labels.get(want,0)}")
        verdict(f"control 3b (vein '{want}' present)", ok,
                f'vein {want} absent -- classifier cannot discriminate the two verdicts')

    # -- control 1b (shape 1): named known positives, one per vein, both PURE.
    #    Each vein pins a LIST; the first entry still in the population is the
    #    subject, so one upstream fix no longer disarms the control.
    for pins, want in ((KNOWN_CMP, 'CMP_REVERSAL'), (KNOWN_ARITH, 'ARITH_COMMUTE')):
        live = [s for s in pins if s in cls_of]
        if not live:
            print(f"  DISARMED  control 1b (known positive {want}): none of the "
                  f"{len(pins)} pinned symbol(s) is sub-100 any more")
            nominees = sorted((s for s, (arms, pure) in cls_of.items()
                               if pure and set(arms) == {want} and '?A0x' not in s),
                              key=lambda s: -by[s]['size'])
            if nominees:
                print(f"            {len(nominees)} candidate(s) nominated BY THE "
                      f"CLASSIFIER ITSELF -- verify each in the diff by hand "
                      f"before pinning;")
                print(f"            a self-selected known positive is a tautology, "
                      f"not a control:")
                for s in nominees[:5]:
                    print(f"              fz={by[s]['fz']:>7.3f} {by[s]['size']:>6} B  "
                          f"{by[s]['unit'][:30]:<30} {s[:70]}")
            disarmed.append(f'control 1b (shape 1, known positive {want}): all '
                            f'{len(pins)} pinned symbol(s) have left the sub-100 '
                            f'population -- re-pin to a {want} row a HUMAN has '
                            f'verified in the instruction diff')
            continue
        sym = live[0]
        arms, pure = cls_of[sym]
        ok = pure and set(arms) == {want}
        moved = (not ok) and stab.row_moved(unit_of.get(sym, ''))
        print(f"  {'PASS' if ok else 'VOID' if moved else 'FAIL'}  "
              f"control 1b (known positive {want}): "
              f"{sym[:38]}... -> {sorted(set(arms))}"
              f"{'' if len(live) == len(pins) else f'  [{len(live)}/{len(pins)} pins live]'}")
        msg = f'known positive {sym} classified {sorted(set(arms))}, expected pure {want}'
        if moved:
            print(f"          VOID: its unit ({unit_of.get(sym)}) or a global input moved "
                  f"under this run")
            voids.append(f'control 1b (known positive {want}): {msg} (inputs moved)')
        elif not ok:
            fails.append(msg)

    print()
    stab.report()
    print(f'controls: {len(fails)} failed, {len(voids)} void, {len(disarmed)} disarmed')

    # PRECEDENCE (argued in the INPUT STABILITY block): FAIL > VOID > DISARMED.
    # A stable-tree failure is real evidence and is never downgraded to
    # "nothing was measured" -- a guard that voids everything is worse than no
    # guard at all.
    if fails:
        print('SELFTEST FAILED:')
        for f in fails:
            print('  -', f)
        if voids or stab.moved_globals:
            print('  (some inputs also moved under this run -- see INPUT STABILITY above.')
            print('   The failures listed are the ones whose evidence held still.)')
        sys.exit(EXIT_FAIL)
    # The one UNCONDITIONAL void: report.json is the population and objdiff-cli
    # is the ruler.  If either moved, the rows enumerated and the diffs measured
    # come from different builds, so even an all-green run proved nothing about
    # any one build.
    if voids or stab.moved_globals:
        print('*** VOID *** this run MEASURED NOTHING -- its inputs moved under it.',
              file=sys.stderr)
        print('A void run is not a pass and it is NOT a regression: it is a racing '
              'rebuild.\nRe-run on a settled tree before reading anything into it.',
              file=sys.stderr)
        for msg in voids:
            print('  - ' + msg, file=sys.stderr)
        if stab.moved_globals:
            print(f'  - global input(s) moved: {", ".join(stab.moved_globals)}',
                  file=sys.stderr)
        sys.exit(EXIT_VOID)
    if disarmed:
        print('SELFTEST INCONCLUSIVE -- %d control(s) DISARMED, so this run did '
              'NOT validate\nevery shape it claims to. A disarmed control is not '
              'a passing control.' % len(disarmed), file=sys.stderr)
        for msg in disarmed:
            print('  - ' + msg, file=sys.stderr)
        print('\nRe-pin the constant(s) above, then re-run. Do not read this as a '
              'PASS.', file=sys.stderr)
        sys.exit(EXIT_DISARMED)
    print('SELFTEST PASSED (inputs held still, and every control above can fail -- '
          'try --self-break)')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--project-dir', default=REPO)
    ap.add_argument('--cache-dir', default=os.path.expanduser('~/tmp/crossing_worklist'))
    ap.add_argument('--max-mismatch', type=int, default=3)
    ap.add_argument('--top', type=int, default=30)
    ap.add_argument('--census', action='store_true')
    ap.add_argument('--adjudicate', action='store_true')
    ap.add_argument('--reclaim', action='store_true')
    ap.add_argument('--selftest', action='store_true')
    ap.add_argument('--self-break', action='store_true',
                    help='sabotage the controls to prove they can FAIL. Run it on a '
                         'SETTLED tree: on a tree a peer is rebuilding, the sabotaged '
                         'controls come back VOID (4) rather than FAIL (2), because '
                         'that is the honest reading of a run whose inputs moved -- '
                         'the lever is not broken, the tree was')
    a = ap.parse_args()
    if not os.path.exists(report_path(a.project_dir)):
        sys.exit(f'REFUSE: no report.json under {a.project_dir}. Build first.')
    if a.selftest:
        cmd_selftest(a)
    elif a.census:
        cmd_census(a)
    elif a.adjudicate:
        cmd_adjudicate(a)
    elif a.reclaim:
        cmd_reclaim(a)
    else:
        ap.print_help()


if __name__ == '__main__':
    main()
