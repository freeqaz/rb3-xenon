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
 * RULER SPLIT -- see THE RULER DEFECT below.  Band membership is taken from
   report.json (the authoritative ruler, per CLAUDE.md) and the PRICING is now
   taken on the same ruler, resolved at runtime.  The tool still REFUSES if the
   two disagree enough to move >2% of rows across the band boundary.
 * INPUT STABILITY -- a run whose inputs moved under it has MEASURED NOTHING,
   and saying "nothing was measured" is not the same as saying "the regression
   is back".  VOID is a third outcome with its own exit code.  See the INPUT
   STABILITY block below.

THE RULER DEFECT (lane T2-RULER, 2026-09-01) -- FIXED HERE
==========================================================
Until this change, line ~396 passed objdiff-cli a hardcoded argv pair -- the flag
`-c` followed by `functionRelocDiffs=none` -- alongside `--include-instructions`.

⚠ Written WITHOUT the literal quoted-argv adjacency on purpose: ruler.py's
regression guard is a TEXT SEARCH over its consumers, and it cannot distinguish a
file that DOCUMENTS the defect from one that HAS it.  Spelling the old line out
verbatim here made this file fail the very guard it was just added to.  If you
quote it again, quote it in prose like this.

That constant was written by lane DQ-3 on 2026-08-03, when `none` really was the
grading ruler.  On 2026-08-12 (`d04c83df`) the project shipped
`options = {"functionRelocDiffs": "name_check"}` in objdiff.json, and because
`objdiff-cli diff` applies `-c` LAST (diff.rs:959) -- AFTER the project's
`options` block (diff.rs:953) -- the hardcoded flag did not merely duplicate the
default, it ACTIVELY OVERRODE the shipped ruler on every single row.

Lane MCPRULER-1 (2026-08-14) fixed exactly this in scripts/orchestrator/
mcp_server.py and concluded mcp_server.py was "the LONE hardcoder".  It was not:
THIS FILE predates that sweep and was missed by it.

Why it mattered here specifically, and why this tool was the DANGEROUS one:
  * BAND MEMBERSHIP came from report.json -- the GRADED ruler (`name_check`).
  * PRICING (the instruction diff that decides `mm`, the class, and therefore
    whether a row is "N mismatches from crossing") came from `diff` on `none`.
  * Under `none`, every relocation-NAME charge is UNCHARGED -- the whole ICF
    fold-alias / wrong-callee class simply does not appear as a mismatched
    instruction.  So a row whose real charge list is "3 insert/delete + 2
    relocation-name" was priced as `mm=3` and advertised as a source-reachable
    prize, when closing all three instructions buys `mpn` 100 and EXACTLY ZERO
    BYTES (matched_code keys on fuzzy == 100).
  * That is the `?Handle@CustomizePanel@@` failure named in CLAUDE.md, which
    misled three consecutive lanes.

⇒ The ruler is now RESOLVED AT RUNTIME from report.json's
`provenance.diff_config` via scripts/analysis/ruler.py -- never hardcoded, because
a second hardcoded constant rots on exactly the same silent schedule.  `none` and
`data_value` survive as EXPLICIT opt-ins (`--ruler`), and every percentage this
tool prints is labelled with the ruler that produced it.

⚠ THE DIFF CACHE IS KEYED ON THE RULER.  It has to be: tools/structural_
decompose.py and tools/shape_families.py share this cache directory by default,
so without the ruler in the key a graded run would be silently served `none`-ruler
entries -- the identical defect, laundered through a cache and therefore
invisible.  CACHE_FORMAT was bumped to 3 and every entry now carries the config
it was minted under.

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

    # pricing ruler: `graded` (default, == report.json) / `none` / `data_value`
    python3 tools/crossing_worklist.py --adjudicate --ruler none
"""
import argparse, collections, hashlib, json, os, re, subprocess, sys, threading, time
import concurrent.futures as cf

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ANON = re.compile(r'^fn_[0-9A-Fa-f]{8}$')

# The ruler is READ FROM THE ARTIFACT THE GRADING RUN WROTE, never hardcoded.
# See THE RULER DEFECT above and scripts/analysis/ruler.py for the full argument.
sys.path.insert(0, os.path.join(REPO, 'scripts'))
from analysis.ruler import (  # noqa: E402
    RULER_GRADED, RULER_NONE, VALID_RULERS, resolve_ruler,
)

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
#
# ⛔⛔ RE-RATIFIED ON THE GRADED RULER (lane T2-RULER, 2026-09-01).  THREE OF THE
# TEN PINS BELOW WERE NOT PURE AND NEVER HAD BEEN -- they were ratified by a human
# reading a diff taken on the `none` ruler, which does not show relocation-NAME
# charges at all.  So the human really did read the diff, and the diff really was
# incomplete: the ratification was honest and the instrument was not.
#
# ⇒ A HUMAN-RATIFIED CONTROL IS ONLY AS GOOD AS THE RULER IT WAS RATIFIED ON.
#   When a ruler changes, every pinned known positive must be RE-READ, not
#   inherited.  Re-reading is what found `?ParseNode@@YA_NXZ` calling a
#   completely different function from retail (below) -- a real divergence that
#   had been sitting inside a "known GOOD row" for a month.
#
# Each surviving pin below was re-read by hand on `functionRelocDiffs=name_check`
# and shows EXACTLY ONE mismatched instruction: same opcode, register operands
# transposed, `argtypes == ['register']`, and NO 'symbol' arg anywhere in the row.
KNOWN_CMP = (
    '?DeterminePhraseTimes@VocalNoteList@@QAAXABVTempoMap@@@Z',        # cmplw cr6,r30,r10 <- r10,r30
    '?MaybeAutoplayFutureCymbal@TrackWatcherImpl@@QAAXH@Z',            # cmpw  cr6,r11,r10 <- r10,r11
    '?SetState@NetSession@@QAAXW4SessionState@1@@Z',                   # cmplw cr6,r11,r10 <- r10,r11
    '?TrackNumOfExactType@PlayerTrackConfigList@@QAAHW4TrackType@@@Z',  # cmpw  cr6,r8,r4   <- r4,r8
)
KNOWN_ARITH = (
    '?HandlePhraseEnd@VocalPart@@QAAXAAHAAM10M@Z',                     # mullw r10,r29,r3  <- r3,r29
    '?Poll@BandIKEffector@@UAAXXZ',                                    # fmuls f0,f11,f0   <- f0,f11
    '?ProcessInPlace@Synapse@1DSP@@QAAXIPAM@Z',                        # add   r3,r11,r29  <- r29,r11
)

# WITHDRAWN, with the reason, rather than deleted -- so that a future lane reading
# a `none`-ruler diff does not "rediscover" them as clean positives and re-pin
# them.  A withdrawal record is cheaper than the second discovery.
WITHDRAWN_PINS = {
    # ⚠ NOT a fold-alias pair: retail calls a DESTRUCTOR, we call a file reader.
    #    Two `bl` sites, both charged only on the graded ruler.  This is a real
    #    wrong-callee divergence that the `none` ruler concealed inside a row
    #    this file advertised as a verified-good ARITH_COMMUTE example.
    '?ParseNode@@YA_NXZ':
        "graded: {ARITH_COMMUTE:1, SYMBOL:2} -- 2x `bl`, target "
        "??1Queue@@QAA@XZ vs base ?ReadEmbeddedFile@@YAPAVDataArray@@PBD_N@Z",
    '?Update@MicInputArrow@@UAAXXZ':
        "graded: {ARITH_COMMUTE:1, SYMBOL:6} -- six relocation-name charges",
    '?Dispatch@EnterFlowMsg@@UAAXXZ':
        "graded: {CMP_REVERSAL:1, SYMBOL:1} -- one relocation-name charge",
}


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
#      other 2,500.  The four GLOBAL inputs are compared by CONTENT and the
#      ~5,000 objects by mtime+size -- see _content_sig and _sig for why the
#      two halves are measured differently.
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
#
# v3 (lane T2-RULER) adds the RULER to both the cache key and the stamp.  Every
# v2 entry in every shared cache directory was minted on `none` -- the defect --
# so they are not evidence about the graded ruler either, and the version bump
# retires them wholesale rather than serving them.  A cached measurement is only
# comparable to a fresh one if it was taken with the same instrument.
#
# v4 (lane W3-F) finishes that sentence.  v3 said "the same instrument" and then
# keyed only on the RULER -- which is the instrument's *configuration*, not the
# instrument.  `bin/objdiff-cli` is a symlink into a shared build tree that three
# repos share, and it is swapped IN PLACE: main's log records two swaps in one
# day (`the second objdiff swap of the day is SCORE-NEUTRAL on this binary`).  A
# swap changes what a diff MEANS -- mismatch counts and charged-site kinds are
# tool-version-dependent -- while leaving `sym`, `unit` and the ruler key
# identical, so v3 served a pre-swap entry to a post-swap run without a word.
# InputStability already VOIDs a run whose objdiff-cli moves MID-run; that says
# nothing about an entry minted last week by a different binary.
#
# The key now carries BOTH identities, because they can disagree:
#   * report.json `provenance.tool_binary_hash` -- the binary that produced the
#     POPULATION and the prices we rank against, and
#   * the content hash of the LIVE bin/objdiff-cli -- the binary that actually
#     mints this entry.
# Keying on the report's hash alone (the obvious reading) would miss precisely
# the case that motivates this: a binary swapped under a report.json that nobody
# has regenerated yet.
#
# WHAT HAPPENS TO THE OLD ENTRIES: nothing is deleted.  The key CHANGED, so v3
# files are never opened again -- they are unreachable, not stale-and-served.
# Measured 2026-09-11 the shared dir held 11,120 entries / 420 MB, so this is
# real disk: `cache_note()` prints the size and the reclaim command on every
# run.  Deleting them is safe and costs one re-mint pass (~7 min at 8 workers).
CACHE_FORMAT = 4

# Pause before retrying a miss.  Paid only by misses.
RETRY_PAUSE_S = 0.25


def _sig(path):
    """mtime+size, for the ~5,000 object files.  Content-hashing them costs more
    than the measurement it guards, and mtime+size catches a rebuild -- the same
    call compare_bins_v2.sh makes.  Cheap false positives are acceptable here
    because ninja does not rewrite a unit it did not recompile."""
    try:
        st = os.stat(path)
    except OSError:
        return None
    return [st.st_size, st.st_mtime_ns]


def _content_sig(path):
    """Content hash, for the FOUR global inputs, where mtime is a bad proxy.

    `objdiff-cli report generate` rewrites report.json in full on every run, so
    an mtime comparison would VOID on a rebuild that changed nothing -- and the
    global void is unconditional, so that false positive would be the loudest
    one available.  These four files total ~35 MB; hashing them twice costs
    well under a second against a run measured in minutes, so pay it here and
    nowhere else."""
    try:
        with open(path, 'rb') as fh:
            h = hashlib.sha256()
            for chunk in iter(lambda: fh.read(1 << 20), b''):
                h.update(chunk)
    except OSError:
        return None
    return h.hexdigest()[:16]


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
        return {'globals': {k: _content_sig(p) for k, p in self.global_paths.items()},
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
            print(f'    global input changed (content): {k} '
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


def ruler_key(ruler):
    """Stable short digest of the FULL diff config, for cache keying."""
    return hashlib.md5(
        json.dumps(ruler.config, sort_keys=True).encode()).hexdigest()[:10]


_INSTRUMENT_CACHE = {}


def instrument_key(project_dir):
    """Short digest identifying the objdiff BINARY a cache entry was minted by.

    Memoized per project dir: the live binary is tens of MB and this is asked
    once per cached row.  See CACHE_FORMAT v4 for why the ruler is not enough.

    Deliberately NOT tolerant of a missing binary or a missing provenance block.
    An unknown instrument keys as the literal `unknown`, which is a DIFFERENT
    key from every known one, so the failure mode is "recompute", never "serve an
    entry minted by an instrument we cannot name".
    """
    key = str(project_dir)
    if key in _INSTRUMENT_CACHE:
        return _INSTRUMENT_CACHE[key]
    parts = []
    try:
        with open(report_path(project_dir)) as fh:
            prov = (json.load(fh).get('provenance') or {})
        parts.append('rpt=' + str(prov.get('tool_binary_hash', 'unknown')))
        parts.append('ver=' + str(prov.get('tool_version', 'unknown')))
    except (OSError, ValueError):
        parts.append('rpt=unknown')
    live = _content_sig(os.path.realpath(objdiff_bin(project_dir)))
    parts.append('live=' + (live or 'unknown'))
    out = hashlib.md5('\x00'.join(parts).encode()).hexdigest()[:10]
    _INSTRUMENT_CACHE[key] = out
    return out


#: `--self-break` sets this to the WIDTH THE COLUMN USED TO HAVE, reinstating the
#: original defect rather than simulating it -- the same idiom the pricing
#: sabotage uses.  Production code never sets it.
SELF_BREAK_SYM_TRUNC = None

#: The name that paid for this.  Lane L5-SYMBOLHEADS handoff 5: briefed as
#: `...PAVLocalBandUser@@@Z` (63 chars) after a silent 58-char cut was completed
#: to a plausible terminator; the real name is 65 chars and ends `@@_N@Z`.  Kept
#: as a CONSTANT so the control cannot be weakened into passing on a short name.
L5_TRUNCATION_WITNESS = (
    '?SelectNode@MusicLibrary@@QAAXPAVSortNode@@PAVLocalBandUser@@_N@Z')
OLD_SYM_COLUMN = 58


def format_worklist_row(r):
    """Render one worklist row.  THE SYMBOL IS NEVER TRUNCATED -- see the block
    at the call site for the row this cost.  Pure, so --selftest can assert on
    it without a tree, a diff or a cache."""
    flag = 'sym' if 'SYMBOL' in r['arms'] else '   '
    sym = r['sym']
    if SELF_BREAK_SYM_TRUNC:
        sym = sym[:SELF_BREAK_SYM_TRUNC]
    return (f"{r['size']:>7} B  mm={r['mm']} {flag} fz={r['fz']:>7.3f}  "
            f"{r['cls']:<16} {r['unit']:<32} {sym}")


_CACHE_NOTE_DONE = set()


def cache_note(cache_dir, out=None):
    """Say once, per dir, how much unreachable cache is lying around.

    A version bump that silently orphans 420 MB is a disk leak nobody is told
    about.  This is a NOTE, never a refusal: an orphan entry cannot corrupt a
    measurement (it is unreachable by construction), it only costs space.
    """
    out = sys.stderr if out is None else out
    d = os.path.abspath(cache_dir)
    if d in _CACHE_NOTE_DONE or not os.path.isdir(d):
        return
    _CACHE_NOTE_DONE.add(d)
    n = tot = 0
    with os.scandir(d) as it:
        for e in it:
            if e.name.endswith('.json'):
                n += 1
                try:
                    tot += e.stat().st_size
                except OSError:
                    pass
    if n:
        print(f'[cache] {n:,} entr(ies), {tot/1e6:.0f} MB in {d}. Entries minted '
              f'before CACHE_FORMAT {CACHE_FORMAT} (different ruler or objdiff '
              f'binary) are UNREACHABLE, not served -- `rm -rf {d}` to reclaim.',
              file=out)


def diff_one(project_dir, sym, unit, cache_dir, unit_sig=None, retries=1, stats=None,
             ruler=None):
    """Run objdiff-cli via argv ONLY -- never through a shell.  See shape 2 above.

    Retries a miss once and stamps the cache with the unit's object signature;
    see mechanisms 1 and 2 in the INPUT STABILITY block.

    `ruler` is a scripts/analysis/ruler.Ruler.  It defaults to the GRADED ruler
    resolved from report.json, so the two in-repo importers of this module
    (tools/structural_decompose.py, tools/shape_families.py) inherit the fix
    without an edit -- the same "inherit the guard for free" property `stab`
    already has.  It is NEVER a hardcoded constant; see THE RULER DEFECT.
    """
    if ruler is None:
        ruler = resolve_ruler(project_dir)          # memoized on report.json mtime
    cache_note(cache_dir)
    os.makedirs(cache_dir, exist_ok=True)
    rk = ruler_key(ruler)
    ik = instrument_key(project_dir)
    # ⚠ The ruler is part of the KEY, not merely the stamp.  A `none` entry and a
    # `name_check` entry for the same symbol are two DIFFERENT measurements that
    # routinely disagree on the mismatch COUNT, not just the percent (measured on
    # ?Handle@OvershellSlot@@: 0 / 2 / 641 sites at none / name_check /
    # data_value).  Sharing one key would let a cache launder the exact defect
    # this change exists to remove.
    h = hashlib.md5(
        (sym + '\x00' + unit + '\x00' + rk + '\x00' + ik).encode()
    ).hexdigest()[:20]
    p = os.path.join(cache_dir, h + '.json')
    if os.path.exists(p) and os.path.getsize(p) > 0:
        try:
            blob = json.load(open(p))
        except ValueError:
            blob = None
        if (isinstance(blob, dict) and blob.get('_cw_cache') == CACHE_FORMAT
                and blob.get('ruler') == rk
                and blob.get('instrument') == ik
                and (unit_sig is None or blob.get('inputs') == unit_sig)):
            return blob['diff']
        # v1/v2/v3 entry, torn write, a stamp from a different build, or an
        # entry minted under a different ruler or by a different objdiff BINARY:
        # not evidence about THIS tree measured with THIS instrument.  Fall
        # through and recompute.
        if stats is not None:
            stats.bump('cache_stale')
    argv = [objdiff_bin(project_dir), 'diff', sym, '-u', unit,
            '--include-instructions', *ruler.args, '-f', 'json']
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
                json.dump({'_cw_cache': CACHE_FORMAT, 'inputs': unit_sig,
                           'ruler': rk, 'ruler_config': ruler.config,
                           'instrument': ik, 'diff': d}, fh)
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


def diff_many(project_dir, rows, cache_dir, stab=None, stats=None, workers=8, ruler=None):
    """diff_many_tolerant, but a miss is terminal -- with the RIGHT diagnosis.

    A partial dump yields a plausible but WRONG census (shape 2), so this never
    returns one and never lets one be printed.  What changes in task #115 is
    not the fatality but the LABEL and the exit code: a miss whose unit's
    objects moved under the run -- or any run whose global inputs moved -- has
    MEASURED NOTHING and exits VOID (4).  A miss on a tree that held still is a
    real defect and exits REFUSE (1), unchanged.  Reporting the first as the
    second is what trains people to wave off the second.

    `stab` defaults to None and is built here when omitted, so the two in-repo
    importers of this module -- tools/structural_decompose.py and
    tools/shape_families.py, which call diff_many(project_dir, rows, cache_dir,
    workers=8) -- inherit the guard without an edit and without a signature
    break.  They run the same diff pass over the same tree and had the same
    ambiguity.
    """
    if stab is None:
        stab = InputStability(project_dir)      # must be built BEFORE the pass
    out = diff_many_tolerant(project_dir, rows, cache_dir, stab, stats, workers, ruler)
    stab.recheck()
    miss = [r for r, o in zip(rows, out) if o is None]
    stable_miss = [r for r in miss if not stab.row_moved(r['unit'])]
    if miss:
        stab.report(sys.stderr)
        if stable_miss:
            for r in stable_miss[:5]:
                # full name: this is the symbol a reader must re-run by hand.
                print(f'    STABLE MISS: {r["unit"]}  {r["sym"]}', file=sys.stderr)
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


def diff_many_tolerant(project_dir, rows, cache_dir, stab=None, stats=None, workers=8,
                       ruler=None):
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
    # Resolve ONCE, here, and pass the same object to every worker: a pass whose
    # rows were measured on two different rulers is not a measurement at all.
    if ruler is None:
        ruler = resolve_ruler(project_dir)
    with cf.ThreadPoolExecutor(max_workers=workers) as ex:
        return list(ex.map(lambda r: diff_one(project_dir, r['sym'], r['unit'], cache_dir,
                                              unit_sig=sig(r['unit']), stats=stats,
                                              ruler=ruler), rows))


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


def ruler_split(rows, diffs):
    """Measure the `diff`-vs-`report generate` disagreement, BOTH WAYS.

    ⚠ The old version tracked only `max(report - diff)` and called it "worst".
    That is one-sided, and it was blind in exactly the direction the ruler defect
    produced: pricing on `none` makes `diff` read HIGHER than the graded report
    (relocation-NAME charges are uncharged there), so `report - diff` is negative
    on every affected row and the printed "worst" stayed pinned at `+0.00` while
    thousands of rows disagreed.  A one-sided instrument reports health when the
    error runs the other way.  Both directions are measured now.
    """
    dis = flip = 0
    worst_rpt_high = worst_diff_high = 0.0
    for r, d in zip(rows, diffs):
        # protobuf-JSON omits defaults: an absent fuzzy_match_percent is 0.0.
        db = float((d or {}).get('fuzzy_match_percent') or 0.0)
        if abs(db - r['fz']) > 1e-3:
            dis += 1
            worst_rpt_high = max(worst_rpt_high, r['fz'] - db)
            worst_diff_high = max(worst_diff_high, db - r['fz'])
            if (r['fz'] >= 95) != (db >= 95):
                flip += 1
    return dis, flip, worst_rpt_high, worst_diff_high


def ruler_gate(rows, diffs, ruler=None):
    """Quantify the pricing-vs-population ruler split, and refuse if it matters.

    On the GRADED ruler this should now read ~0 disagreements: band membership
    (report.json) and pricing (`diff`) are the same instrument, which is the
    whole point of the fix.  A non-trivial split here means the pricing ruler has
    drifted from the grader again -- read it as an alarm, not as background.

    Under an EXPLICIT `--ruler none`/`data_value` opt-in the split is EXPECTED and
    is the reason the opt-in exists, so it is reported loudly and NOT refused --
    refusing there would make the opt-in unusable.  The row band still comes from
    report.json, so such a run is deliberately mixing two rulers and says so.
    """
    dis, flip, worst_rpt_high, worst_diff_high = ruler_split(rows, diffs)
    n = max(len(rows), 1)
    print(f"RULER SPLIT: pricing-vs-report disagreements {dis}/{len(rows)} "
          f"({100*dis/n:.1f}%), worst report-above-pricing {worst_rpt_high:+.2f} pp, "
          f"worst pricing-above-report {worst_diff_high:+.2f} pp, "
          f"band flips {flip} ({100*flip/n:.2f}%)")
    if ruler is not None and ruler.selector != RULER_GRADED:
        print(f"  ⚠ pricing is on the **{ruler.reloc_mode}** ruler by explicit "
              f"--ruler={ruler.selector}, but the row BAND comes from report.json "
              f"(**{ruler.graded_reloc_mode}**).\n"
              f"    This run deliberately mixes two rulers; the split above is the "
              f"EXPECTED consequence, not a defect.\n"
              f"    Do not read `mm` or the class census here as the graded "
              f"reachability of a row.")
        return
    if flip / n >= 0.02:
        sys.exit('REFUSE: ruler split would move >=2% of rows across the band boundary.')


def print_ruler(ruler):
    """A percentage without its ruler is not a measurement.  Say it every time."""
    print(ruler.banner())
    print()


def cmd_census(a):
    # The census reads report.json ONLY -- no `diff` call -- so it is already on
    # the graded ruler by construction.  It is labelled anyway: a reader cannot
    # tell "graded by construction" from "graded by luck" without being told.
    print_ruler(resolve_ruler(a.project_dir, a.ruler))
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
    ruler = resolve_ruler(a.project_dir, a.ruler)
    print_ruler(ruler)
    M, rows = load_rows(a.project_dir)
    stab = InputStability(a.project_dir)
    stats = Counters()
    cache = os.path.join(a.cache_dir, 'diffs')
    print(f"diffing {len(rows)} rows on ruler `{ruler.reloc_mode}` "
          f"(cache {cache}) ...", file=sys.stderr)
    diffs = diff_many(a.project_dir, rows, cache, stab, stats, ruler=ruler)
    if stats['rescued_by_retry']:
        print(f"note: {stats['rescued_by_retry']} row(s) missed on the first attempt and "
              f"resolved on retry (transient, not a defect)", file=sys.stderr)
    ruler_gate(rows, diffs, ruler)
    for r, d in zip(rows, diffs):
        r['arms'] = classify_arms(d)
        r['mm'] = len(r['arms'])
        r['cls'] = collections.Counter(r['arms']).most_common(1)[0][0] if r['arms'] else 'NONE'
        r['pure'] = len(set(r['arms'])) == 1

    sel = [r for r in rows if 0 < r['mm'] <= a.max_mismatch]
    TB = sum(r['size'] for r in sel)
    print(f"\n=== worklist: rows with <= {a.max_mismatch} real mismatches "
          f"[ruler {ruler.reloc_mode}]: {len(sel)} rows / {TB:,} B ===")
    nsym = sum(1 for r in sel if 'SYMBOL' in r['arms'])
    bsym = sum(r['size'] for r in sel if 'SYMBOL' in r['arms'])
    print(f"  of which {nsym} rows / {bsym:,} B carry >=1 relocation-NAME charge "
          f"(class SYMBOL).\n"
          f"  Those are NOT plain source work: a SYMBOL charge is a wrong callee OR "
          f"an ICF fold-alias,\n"
          f"  and an unproven fold cannot be closed by editing instructions. On the "
          f"`none` ruler they were\n"
          f"  INVISIBLE -- see THE RULER DEFECT at the top of this file.")
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

    print(f"\n=== top {a.top} of the worklist by size-if-it-crosses "
          f"[ruler {ruler.reloc_mode}] ===")
    print("  'sym' column marks rows carrying a relocation-NAME charge: their prize "
          "is NOT collectable\n  by instruction edits alone.")
    # ⛔ NEVER TRUNCATE THE SYMBOL.  This column used to print `r['sym'][:58]`
    # and it cost lane L5-SYMBOLHEADS a row (handoff 5,
    # docs/decomp/SYMBOL_HEADS_2026-09-10.md): row 12's real name is
    # `?SelectNode@MusicLibrary@@QAAXPAVSortNode@@PAVLocalBandUser@@_N@Z` (65
    # chars) and the cut landed mid-token at `...PAVLocalBandUse`.
    #
    # ★ THE CUT WAS SILENT, AND THAT IS THE ACTUAL DEFECT.  A reader handed a
    # name ending mid-token does not see "truncated", they see a mangled name --
    # so the name was RECONSTRUCTED to a plausible terminator,
    # `...PAVLocalBandUser@@@Z`, which is well-formed, 63 chars, and WRONG in its
    # last 5 characters.  objdiff then answers "Symbol not found in target",
    # which reads like a PHANTOM ROW (a dtk mis-carve -- a class this project
    # really has) rather than like a copy error, so the row is written off
    # instead of retried.  Verified: the briefed string and the real one share
    # exactly the first 58 characters.
    #
    # An ellipsis marker would be an improvement and is still not enough: these
    # names are COPY-PASTE INPUT to objdiff-cli and to the map, so anything less
    # than the full name is unusable.  Alignment is not worth a wrong symbol;
    # the symbol goes LAST on the line so a long one cannot misalign a column.
    for r in sorted(sel, key=lambda r: -r['size'])[:a.top]:
        print(format_worklist_row(r))


def cmd_reclaim(a):
    # report.json only, no `diff` call -- graded by construction, labelled anyway.
    print_ruler(resolve_ruler(a.project_dir, a.ruler))
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

    # ── the pricing ruler ────────────────────────────────────────────────────
    # `--self-break` REINSTATES THE ORIGINAL DEFECT rather than simulating it: it
    # forces pricing onto `none` while the row band still comes from the graded
    # report.json, which is bit-for-bit what line ~396 used to do.  An end-to-end
    # sabotage is worth more than a flag that pokes a boolean, because it also
    # proves the control would have caught the historical bug.
    graded = resolve_ruler(a.project_dir, RULER_GRADED)
    if a.self_break:
        # reinstate the symbol-column truncation too, so the control below has
        # something real to catch (see SELF_BREAK_SYM_TRUNC).
        globals()['SELF_BREAK_SYM_TRUNC'] = OLD_SYM_COLUMN

    ruler = resolve_ruler(a.project_dir, RULER_NONE) if a.self_break \
        else resolve_ruler(a.project_dir, a.ruler)
    print_ruler(ruler)

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
    diffs = diff_many_tolerant(a.project_dir, rows, cache, stab, stats, ruler=ruler)
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
                # full name: a control's FAIL names the symbol a human must
                # re-run, and 86 chars cuts real mangled names mid-token.
                print(f"          {s}")
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
                    # full name: this line is a PIN CANDIDATE a human is being
                    # asked to verify in the diff, so it must be paste-able.
                    print(f"              fz={by[s]['fz']:>7.3f} {by[s]['size']:>6} B  "
                          f"{by[s]['unit']:<30} {s}")
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
              f"{sym} -> {sorted(set(arms))}"
              f"{'' if len(live) == len(pins) else f'  [{len(live)}/{len(pins)} pins live]'}")
        msg = f'known positive {sym} classified {sorted(set(arms))}, expected pure {want}'
        if moved:
            print(f"          VOID: its unit ({unit_of.get(sym)}) or a global input moved "
                  f"under this run")
            voids.append(f'control 1b (known positive {want}): {msg} (inputs moved)')
        elif not ok:
            fails.append(msg)

    # -- control 5 (lane T2-RULER): THE PRICING RULER IS THE GRADED RULER.
    #
    #    This is the regression guard for the defect described in THE RULER
    #    DEFECT.  It is deliberately TWO checks, because either one alone can be
    #    satisfied while the tool is still lying:
    #
    #      5a is a pure CONFIG check -- it would have caught the historical bug
    #         on the day objdiff.json shipped `name_check`, with no diffs at all.
    #      5b is the MEASURED consequence.  A config that reads right while the
    #         diffs come back on another ruler (a stale cache entry, an argv
    #         ordering surprise, a future `-c` creeping back in) fails here and
    #         passes 5a.  5b is what makes 5a more than a restatement of itself.
    #
    #    ⚠ VACUITY GUARD.  `--self-break` works by forcing pricing to `none`.  If
    #    the project ever ships `none` AS the graded ruler again, that sabotage
    #    is a NO-OP and this control would pass under --self-break -- a control
    #    that cannot fail.  That case is reported DISARMED (exit 3), never PASS,
    #    following the house idiom in tools/screen_gate.py.
    if a.self_break and graded.reloc_mode == RULER_NONE:
        print(f"  DISARMED  control 5 (pricing ruler == graded ruler): the graded "
              f"ruler IS `{RULER_NONE}`, so --self-break's sabotage is a no-op")
        disarmed.append('control 5 (ruler): the graded ruler is `none`, so the '
                        '--self-break sabotage (force pricing to `none`) changes '
                        'nothing and the control cannot fail. Re-express the '
                        'sabotage against whatever the graded ruler then is -- do '
                        'not read a PASS here.')
    else:
        ok_a = ruler.reloc_mode == graded.reloc_mode
        print(f"  {mark(ok_a)}  control 5a (pricing ruler == graded ruler): "
              f"pricing `{ruler.reloc_mode}` vs report.json `{graded.reloc_mode}`"
              f"  [{graded.source}]")
        verdict('control 5a (pricing ruler == graded ruler)', ok_a,
                f'PRICING ON THE WRONG RULER: rows are banded by report.json on '
                f'`{graded.reloc_mode}` but priced by `diff` on `{ruler.reloc_mode}`, so '
                f'relocation-NAME charges are counted by one half of this tool and not '
                f'the other')

        # 5b: the measured consequence, over every row whose diff resolved.
        paired = [(r, diff_of[r['sym']]) for r in rows if diff_of.get(r['sym']) is not None]
        dis, flip, w_rpt, w_prc = ruler_split([r for r, _ in paired], [d for _, d in paired])
        share = dis / max(len(paired), 1)
        ok_b = share < 0.01
        print(f"  {mark(ok_b)}  control 5b (pricing agrees with report.json): "
              f"{dis}/{len(paired)} rows disagree ({100*share:.2f}%), worst "
              f"report-above-pricing {w_rpt:+.2f} pp, pricing-above-report {w_prc:+.2f} pp")
        verdict('control 5b (pricing agrees with report.json)', ok_b,
                f'{dis}/{len(paired)} rows ({100*share:.2f}%) score differently under the '
                f'pricing ruler than under the grader -- the two halves of this tool are '
                f'measuring with different instruments')

    print()
    stab.report()
    print(f'controls: {len(fails)} failed, {len(voids)} void, {len(disarmed)} disarmed')

    # PRECEDENCE (argued in the INPUT STABILITY block): FAIL > VOID > DISARMED.
    # A stable-tree failure is real evidence and is never downgraded to
    # "nothing was measured" -- a guard that voids everything is worse than no
    # guard at all.
    # -- control 6 (shape 3): THE SYMBOL COLUMN MUST NOT TRUNCATE.
    #    Lane L5-SYMBOLHEADS lost row 12 to a silent 58-char cut: the name was
    #    completed to a plausible mangled terminator and objdiff answered
    #    "Symbol not found in target", which reads like a PHANTOM ROW rather
    #    than a copy error.  This control cannot rot -- its subject is a
    #    CONSTANT 65-char name, not a pin into the live population, so it stays
    #    armed after every upstream fix (unlike controls 1b/2, which disarm).
    #
    #    It is also deliberately a RENDERING test, not a string-length test:
    #    the defect was in what got PRINTED, and a `len()` assertion on the row
    #    dict would have passed throughout.
    witness = {'size': 2260, 'mm': 1, 'arms': ['SYMBOL'], 'fz': 99.5,
               'cls': 'SYMBOL', 'unit': 'default/MusicLibrary',
               'sym': L5_TRUNCATION_WITNESS}
    assert len(L5_TRUNCATION_WITNESS) > OLD_SYM_COLUMN, (
        'the witness is shorter than the old column -- this control could not '
        'fail even with the defect reinstated, which is a vacuous control')
    line = format_worklist_row(witness)
    if L5_TRUNCATION_WITNESS in line:
        print('  PASS  control 6 (symbol column prints the FULL name): '
              f'{len(L5_TRUNCATION_WITNESS)}-char witness survives rendering')
    else:
        print('  FAIL  control 6 (symbol column prints the FULL name): rendered '
              f'line does not contain the {len(L5_TRUNCATION_WITNESS)}-char '
              f'witness -- a truncated symbol is UNUSABLE as objdiff input and '
              f'reads as a phantom row')
        print(f'          rendered: {line}')
        fails.append('control 6: the symbol column truncated a '
                     f'{len(L5_TRUNCATION_WITNESS)}-char mangled name (lane '
                     'L5-SYMBOLHEADS handoff 5)')

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
    ap.add_argument('--ruler', default=RULER_GRADED, choices=list(VALID_RULERS),
                    help='which diff ruler to PRICE on. `graded` (DEFAULT) is read at '
                         'runtime from report.json\'s provenance.diff_config, so it is '
                         'the same instrument the grader used and the same one the row '
                         'band comes from. `none` and `data_value` are explicit opt-ins '
                         'that change exactly one key; they deliberately MIX rulers '
                         '(band from report.json, pricing from the opt-in) and say so')
    ap.add_argument('--self-break', action='store_true',
                    help='sabotage the controls to prove they can FAIL. Run it on a '
                         'SETTLED tree: on a tree a peer is rebuilding, the sabotaged '
                         'controls come back VOID (4) rather than FAIL (2), because '
                         'that is the honest reading of a run whose inputs moved -- '
                         'the lever is not broken, the tree was. NOTE it also forces '
                         'pricing onto `none`, reinstating the historical ruler defect '
                         'end-to-end so control 5 is proven able to catch it')
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
