# Merge record: u1-deepsched (recovered)

**What happened (2026-09-10, coordinator session 3cdd3c):** the u1-deepsched
lane was merged into main as `1b45fc5b` by the other rb3-xenon coordinator
session while I was amending my own preceding merge (`62b300be`,
l4-nativescatter) in the SAME shared working tree. `git commit --amend`
amends whatever HEAD is at that instant, so it rewrote `1b45fc5b` -- same
two parents, byte-identical tree -- as `2f08c770` carrying the L4 merge
message. The u1-deepsched merge message was lost from history and `2f08c770`
is mislabelled: it IS the u1-deepsched merge. That state was pushed before
the error was noticed (the peer session spotted the duplicate subject).

Main is shared with concurrent sessions that have since committed on top, so
the history is left as-is rather than force-rewritten; this file and a git
note on `2f08c770` carry the lost record. Rule recorded in memory: never
`--amend` in the shared main tree -- write the message right the first
time (`-F file`, no backticks in a zsh double-quoted string).

## The lost merge message of `1b45fc5b`, verbatim

```
Merge u1-deepsched: the unicorn fixture was BROKEN, not shallow -- and `logic` is still 0

S4 closed by saying the outstanding work was "a DEEPER schedule, not a broader
run". That framing needed inverting: the schedule was not shallow, THE FIXTURE
WAS BROKEN, and one mechanism produced BOTH of S4's headline pathologies.

⛔ MSVC on Xenon calls out-of-line register save/restore helpers, which are
external REL24 targets -- and the harness gave every such target the generic
`li r3,0; blr` stub. So `bl __savegprlr_29` DESTROYED THE INCOMING `this` AT THE
SECOND INSTRUCTION OF THE FUNCTION, on both sides, and the tail
`b __restgprlr_29` returned to a stale LR, making the function re-enter its own
tail and spin to the cap. Every verdict S4 produced sits on that fixture.

Eight fixes ported, and VERIFIED IN THIS BINARY RATHER THAN INHERITED: all 72
GPR/FPR helper bodies match `band.exe` byte-for-byte at RB3's own addresses (29
tests, 0 skipped). extractor.py was deliberately NOT ported -- ours is ahead,
carrying the $EH fix that changes which bytes get emulated.

SENSITIVITY DEMONSTRATED, AND THE PORT DID NOT BUY IT. ?ClearBones@CharBones@@
catches 3/4 single-instruction deletions with the null leg EQUIVALENT --
reproducing S4's figure -- and RE-RUN AFTER THE PORT IT IS STILL 3/4, so the
port did not purchase verdicts by desensitising the instrument. A hand-assembled
positive control (`mr r3,r4; blr`) was built for the out-param knob because the
first out-param leg measured Delta 0, and "the schedule changed nothing" and
"the knob was never connected" are not the same finding: r3 reads 0x00000000
without arg_registers and 0x20002000 with.

PRE-REGISTERED PREDICTIONS, INCLUDING THE MISS:
  helper bodies: equiv_matching_err < 50   -> MISS, 103 -> 88 only
  helper bodies: equiv_real rises          -> HIT, +52.7%
  helper bodies: divergent falls           -> HIT, -41.4%
  new data_layout absorbs call_arg/object_memory -> HIT EXACTLY (25->0, 42->1, 0->90)
  outparam: new coverage                   -> WIN (+24% divergence, -10% crashes,
                                              and 1 matched-but-wrong row the
                                              default schedule never sees)
  sentinel (non-uniform fill): crashes rise -> HIT (+23%) BUT BOUGHT NO EXTRA
                                              DIVERGENCE -- recorded as a negative
  typed                                    -> a genuine null, and PROVED APPLIED
                                              via fixture_note rather than assumed

SHARED-ERROR LAUNDERING: 30.9% -> 17.7% corpus-wide, by fixing CAUSES. The
verdict was deliberately NOT flipped: converting shared errors to DIVERGENT would
turn 88 harness failures into 88 fake bug reports -- the exact disease this lane
existed to cure. The residue is labelled per row and excluded from the worklist
by construction. The remaining 3,048 are an EMULATOR problem, not a schedule one
(the canonical shape dies on an instruction Unicorn will not execute, with zero
calls logged).

Side effect: the DEEPER schedule made the BROADER run cheap. The harness that
wedged on 51 units now audits ALL 1,045 units / 22,718 functions in 256 seconds
-- 2.9x S4's coverage. The wedge was the emulator leak the port fixes.

★★★ AND THE HEADLINE DID NOT SURVIVE ITS OWN AUDIT. Genuine `logic`-class
divergences: ZERO of 22,718, now measured on a fixture that no longer destroys
`this` at instruction 2. Screening ran 5,465 DIVERGENT -> 433 real-class -> 349
after byte-identity -> 40 scoring 100%. The 15-row cluster of `object_memory`
constructors at 100.00/100.00 -- the dropped-initializer shape DC3 found seven of
-- ALL FIFTEEN DISSOLVE: 6 differ by a CONSTANT 0x2C across six unrelated
classes, 4 are "we store a real float, retail stores exactly 0" (float-literal
asymmetry), 5 are scalar-vs-GLOBAL placement. All three escape `data_layout`
because it requires EVERY differing word to be a harness address, and arithmetic
on two harness addresses yields a scalar -- a screen gap BY CONSTRUCTION.
⇒ THE 40 IS AN UPPER BOUND, NOT A BUG COUNT, and no fix was attempted because
nothing reached the bar.

⚠ INFRASTRUCTURE DEFECT WITH FLEET REACH: the C hook's pure-Python fallback has
a signedness bug that makes every call-target lookup miss, so the wrong-callee
warning is SILENTLY LOST ENTIRELY -- and the `.so` is gitignored and
setup_worktree.sh does not build it, so EVERY FRESH WORKTREE TAKES THAT DEGRADED
PATH BY DEFAULT. Rebuild against the venv's Python 3.10; the Makefile defaults to
system 3.14 and produces a silently unimportable .so.

DRIFT EXONERATION, on evidence: this lane never invoked objdiff-cli/run_objdiff
at all, every build was a full ./tools/ninja-locked in its own worktree, its only
build was 23:19, and the worktree verified as a patched fixed point
(tree_sha256=d568d3736ef0757f) before and after every measurement. I confirmed
its reading independently: 1,204 of main's 1,205 objects share a single 23:31
mtime, i.e. a MASS REWRITE of the whole tree of which ~600 differ in content --
the shape of a full build in main whose post-compile patchers did not complete,
not a targeted `ninja <one>.obj`.

Not verified: the full-corpus `outparam` run -- THE HIGHEST-VALUE NEXT
MEASUREMENT, since it wins and was only run on 51 units; the 16 `call_count`
matched-but-wrong rows, which carry the inline-policy caveat and include two of
S4's TIER1 still unadjudicated after two lanes; the three screen gaps are
diagnosed, not fixed; sentinel/typed under-measured; decomp.db not written
(shared with live lanes -- CSVs committed gzipped instead). No src/ touched, so
the matching metric is untouched by construction.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>

```
