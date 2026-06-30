# Post-class-A pivot results — body-port tails + class-B identity-transfer (2026-06-24)

After the class-A TU-pure span harvest hit practical exhaustion (+403 this session, wave-8 +0),
the owner chose BOTH pivots (#1 body-port near-miss tails, #2 class-B identity-transfer). Both ran as
concurrent workflows. Net: **+3 landed** (body-port +2, class-B +1). Main 10662→10664.

## Stream #1 — body-port near-miss tails (scripts/wf_bodyport_tails.js) = +2
Scouted 92-99.99% near-misses in already-wired TUs, classified divergence (BODY vs permuter-regalloc vs
struct-lever), reconstructed only the BODY-fixable ones to TRUE 100%.

| fn | TU | before | result |
|---|---|---:|---|
| CharBones::AddBoneInternal | system/char/CharBones.cpp | 97.65% | **100% LANDED** |
| RndAmbientOcclusion::OnGetRecvMeshes | system/rndobj/AmbientOcclusion.cpp(+.h) | 98.57% | **100% LANDED** |

Composed-verified 10662→10664 deterministic, 0 regressions (AmbientOcclusion.h is shared — clean).

### ⚠ The inlined-strcpy NUL-terminator instruction-selection WALL (characterized negative)
4 near-misses REFUTED honest +0, ALL the same root cause — do NOT re-chase these as body-ports:
BandCharacter::OnChangeFaceGroup (98.72%), FirstSortChar (98.85%), CharUtlFindBone (98.67%) [+ the
strcpy family generally]. The single mismatched instruction is the NUL-terminator test inside an INLINED
`strcpy` intrinsic copy loop: retail emits `cmplwi rN,0x0` (unsigned imm compare, byte stays live for the
store) but our cl.exe 16.00.11886.00 emits `extsb. rM,rN` (signed-record) or `mr. rM,rN` (record-move) —
a dead value-materialization op the retail compiler's peephole elided. Each agent tried ~12 source forms
(plain strcpy / `unsigned char` / `unsigned int` / two-pointer / offset-addressed / manual loops / casts):
- `unsigned char` removes the `extsb` sign-extend but lands on `mr.` (record-move), still not `cmplwi`.
- `unsigned int ch` reaches `cmplwi rN,0` BUT with a companion dead `mr rM,rN` the target lacks.
- manual byte loops destroy the intrinsic shape entirely (drop to 83-86%, emit a real `bl`).
- decomp_synth permuter (10rd/100var, chain-depth 5) converged 0/all on every one.
CLASSIFICATION: compiler-internal instruction-selection / char-signedness materialization, SOURCE- AND
PERMUTER-UNREACHABLE. Same family as the wave-20 SHA1-K / FP-regalloc walls. The decomp.db in fresh
worktrees has no `functions` table → permuter symbol-resolution returns 0 candidates (tooling note).

## Stream #2 — class-B identity-transfer (scripts/wf_idt_classb.js) = +1
Per-method micro-pins on the wave-8 validated-OWN-but-SCATTERED TUs (not span-pinnable):

| TU | result |
|---|---|
| OvershellSlotState.cpp | **+1 LANDED** (UsesRemoteStatusView micro-pin, splits-only) |
| SessionUsersProviders.cpp | DEFER — oracle-misattribution + body-divergence (2 methods killed) |
| Leaderboard.cpp | DEFER — body-divergence (6 methods killed) |

Re-confirms the thin ceiling: even on TUs that genuinely OWN real scattered methods, the per-method
bodies body-diverge (MWCC→MSVC); micro-pins capture only trivial/own-funclet ones (1 win / ~9 candidate
methods). Matches the prior 0/10 fresh-TU finding. The class-B belt is NOT cheaply recoverable via IDT.

## Verdict
The cheap matching levers are now broadly exhausted across ALL three classes tried this session: class-A
span harvest (exhausted at +403), class-B identity-transfer (thin, +1), and body-port tails (+2, the
remaining near-misses are the strcpy/regalloc instruction-selection wall). The honest frontier is now the
hard one (the structural-levers-exhausted capstone + fuzzy-reconstruction docs): scattered-TU
identification + body-divergence reconstruction needs a better identifier/codegen-match than any current
oracle. Session matching total: +413 (class-A +403 / class-B +1 / body-port +2 / OvershellSlotState in
class-B counted once / wave-7 etc. included), main @fb13b46 = 10664 matched.

---

## ⛔ COMPILER-FLAG /J test (2026-06-30) — KILL (closes the flag avenue for the strcpy wall)
Hypothesis: the strcpy NUL-test `extsb.`(signed) vs retail `cmplwi`(unsigned) is the signature of MSVC
`/J` (default char unsigned); the body-port agents tried source forms but never the FLAG. RESULT = KILL:
- **Base cflags AUDIT: rb3-xenon == dc3 EXACTLY** (`/nologo /wd4355 /wd4164 /c /GR /O1 /Oi /EHsc`; /GS only on
  per-config curl/net/jpeg sub-flags in BOTH; neither uses /J). No systematic flag-divergence — our build
  is correctly configured vs the oracle.
- Adding `/J` to base = **−18 net** (10664→10646, deterministic 2×). objdiff on OnChangeFaceGroup +
  FirstSortChar: the strcpy NUL-test STILL emits signed `extsb.` under /J (target wants `cmplwi`) — /J does
  NOT reach the strcpy intrinsic's internal NUL-test codegen, doesn't even flip it to `mr.`. Meanwhile /J
  reinterprets char elsewhere, breaking 18 signed-char-dependent fns.
- CONCLUSION: the inlined-strcpy `extsb`/`cmplwi` wall is INTERNAL to the X360 strcpy intrinsic codegen,
  NOT governed by the char-signedness flag. Flag avenue CLOSED. Confirms body-divergence wall #2 is a
  compiler-codegen wall, not a config/source/flag-fixable one.
