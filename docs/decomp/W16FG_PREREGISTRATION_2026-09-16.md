# W16-FG pre-registration — BEFORE the authoritative A/B

Written after a first in-worktree build but BEFORE `tools/ab_measure.py`.
Disclosure: the point estimates below are informed by that in-worktree
`report.json` read. That read is NOT authoritative — CLAUDE.md records cases
where an unsettled in-worktree reading had the WRONG SIGN (lane DF-2: +23
matched in-worktree, 0 settled). The A/B is the measurement; this is the
commitment.

## Change under test

- `src/system/synth_xbox/FxSendReverb.cpp` — define
  `FxSendReverb360::SyncEffectParams(IXAudio2SubmixVoice*) const` (was declared
  in the header, defined nowhere).
- `src/xdk/xaudio2/xaudio2fx.h` — NEW, ported from DC3 with
  `XAUDIO2FX_REVERB_PARAMETERS` truncated 0x38 -> 0x34.

⚠ A/B SCOPE NOTE: the new header is UNTRACKED, so `--from-dirty` cannot revert
it and it is present in BOTH legs. Verified inert in leg A: `grep -rn xaudio2fx
src/` matches only the one `.cpp` under test, so with that `.cpp` reverted to
its stub nothing includes the header and it contributes no code. The measured
delta is therefore the full delta of the feature.

## Predicted DELTAS (not absolutes)

| measure | predicted delta |
|---|---|
| `matched_functions`      | **+1** |
| `matched_code`           | **+3,280 B** |
| `matched_code_percent`   | **+0.032014 pp** |
| `masked_equal_functions` | **0** |

Reasoning: `matched_code` is all-or-nothing per row, and the only row that can
cross is the 3,280 B `?SyncEffectParams@FxSendReverb360@@UBAXPAUIXAudio2SubmixVoice@@@Z`.
3,280 / 10,247,068 = 0.0320094%. No other TU is touched, so I predict zero
collateral in either direction. `masked_equal` should not move: this row is a
real body-for-body match, not a funclet byte-signature pairing.

## Falsifiers — rows that MUST NOT move

If any of these moves, the result is contaminated and the number is void:

1. `?ReverbConvertI3DL2ToNative@@YAXPBUXAUDIO2FX_REVERB_I3DL2_PARAMETERS@@PAUXAUDIO2FX_REVERB_PARAMETERS@@@Z`
   (740 B, same unit) must STAY `fuzzy 0 / mpn 0`. It is Microsoft vendor
   implementation, explicitly out of scope. If it moves, I accidentally emitted
   a body for it (e.g. via an inline definition in the header) and the row is
   not honestly earned.
2. `?CreateFx@FxSendReverb360@@MAAPAUIUnknown@@XZ` (40 B) must STAY `fuzzy 100`.
   I edited this function's surroundings (removed its local `extern "C"
   CreateAudioReverb` declaration in favour of the header's). If it falls off
   100, the header's declaration is not equivalent to the local one.
3. `fn_82B67B40` (8 B) must STAY `fuzzy 0` — an unrelated unnamed row; no reason
   for it to move, so movement means the unit's pairing shifted under me.
4. `??0FxSendReverb360@@QAA@XZ` (100 B) and `?SetType@FxSendReverb360@@UAAXVSymbol@@@Z`
   (252 B) must STAY `fuzzy 100` — the ctor and SetType share the TU and the
   class layout; movement would mean the new header perturbed the layout.

## What would make me report a NEGATIVE

If `Delta matched_code` is 0 while the row reads below 100, I report the exact
charged-site list and call the vein open-but-uncrossed, per the brief. Partial
fuzzy movement is diagnosis, NOT progress.
