# DC3 engine-vein yield pilot (LightPreset) — NEGATIVE on cheap matching; the real lever is foundational struct-reconstruction

Date: 2026-06-21. Pilot branch `engpilot-lightpreset @4b9c99e` (empty commit, NOT landed).
Tests whether the ~8,599 "unmatched in pinned engine TUs" from the DC3-engine-oracle
feasibility (`2026-06-21-dc3-engine-oracle-feasibility.md`) is a CHEAP-matchable vein.

## VERDICT: the 8,599 headcount is INFLATED; cheap-matchable fraction ≈ 0%

Target `world/LightPreset.cpp` (pinned, 224/356 = "132 unmatched"). The 132 decompose:

**93 are NOT near-misses (base_size=0 in objdiff — nothing in our obj pairs to them) = an IDENTIFICATION wall:**
- **84** raw anonymous `fn_8XXXXXXX` target functions — the target obj has 239 anonymous
  `fn_*` the symbol-map never named; our obj emits these bodies under mangled names that
  do not pair to an anonymous address. (`objdiff diff` confirms `base_size: 0` for each.)
- **9** foreign STL template helpers (`RecordedFrame`/`CharClipDisplay`/`CamShotFrame`/
  `Vector3`…) **ICF-folded from OTHER TUs** physically sitting in LightPreset's `.text`
  span. Our LightPreset.obj genuinely doesn't emit these.

**39 genuine paired near-misses:**
| Count | Bucket |
|------:|--------|
| **2** | CASCADING-KEYSTONE (RndEnviron 0xB0 layout drift) |
| 22 | RELOC / ICF-NAME-NOISE (≥95%; anonymized data-labels `lbl_82C926B8` vs our mangled static-guard/string symbols — code identical, a NAMING wall) |
| 10 | PERMUTER-WALL (`diff_arg` commutative-swap / regalloc — low EV) |
| 5 | MIS-PAIRED (map named a method onto the WRONG target body — a LOCATION bug, not body-fixable) |
| **0** | INDIVIDUAL-CHEAP |

`real_net_delta = 0` (composed `fresh_report.sh` twice, stable 9834). Nothing cheap to harvest.

## The one real lever: RndEnviron 0xB0 struct-reconstruction keystone
- Retail reads `RndEnviron::mAmbientFogOwner` at `[r30+0x7c]`; our build at `[r30+0x12c]`
  (Δ0xB0). `mFogStart` `+0x64` retail vs `+0x114` ours. Uniform across `AnimateEnvFromPreset`
  + `FillEnvPresetData`, bleeds into the EnvironmentEntry PropSync family.
- ROOT = DC3-drift: DC3 `RndEnviron` derives `RndTransformable + RndDrawable` (large dual
  base, `mAmbientFogOwner@0x14c`); rb3-Wii derives `Hmx::Object` (`@0x74`); **retail-Xbox is
  its OWN layout (~0xB0 SMALLER than DC3, `@0x7c`)**. Our port copied DC3 verbatim ⇒ 0xB0
  too big. Per CLAUDE.md, DC3 must NOT be assumed correct — here it's demonstrably wrong.
- **Generalizes**: Env.h is consumed by ~40 engine TUs (the whole renderer), so a correct
  retail RndEnviron base-class + member layout would CASCADE binary-wide (Handle-keystone
  class of lever). BUT it is a FOUNDATIONAL shared-header reconstruction gated on a
  whole-binary composed A/B across rendering — NOT a cheap harvest. Left unapplied (correct
  per the honesty constraint; forcing it for +2 local would be reckless).

## What this means for the campaign (the consolidated conclusion)
Both major veins probed today resolve to the SAME wall:
1. **Scattered game layer** — identification wall, no good oracle (rb3-Wii near-random; BSim
   seed-prop NO-GO; string-anchoring sparse). Mostly unrecoverable.
2. **DC3 engine vein** — the "8,599 cheap matches" is INFLATED by exactly the same effects
   seen here: most are unpaired anonymous/ICF-foreign (identification wall) or reloc-name-
   noise, not tractable near-misses. The DC3 BinDiff oracle is NOT needed for the already-
   paired near-misses (read divergence straight from objdiff); it would only help NAME the
   84 unpaired anonymous targets — and for LightPreset even those are gated on the SAME
   RndEnviron struct fix (the drift cascades), so naming alone won't match them until the
   layout is reconstructed.

⇒ **The cheap-matching era is over** (confirms wave-20's PRACTICAL-EXHAUSTION declaration and
the MEMORY's "IDENTIFICATION wall, not a body wall" reframe — now proven on ENGINE code too).
The remaining levers are TWO expensive/foundational classes:
- **(A) Foundational struct-reconstruction keystones** (RndEnviron = the concrete, proven,
  high-ceiling example; the DC3-drift struct-lever class generalized). Each is a retail-layout
  reconstruction + whole-binary A/B, high ceiling (binary-wide cascade) but high cost/risk.
- **(B) The identification wall** (anonymous `fn_*` + ICF-foreign targets binary-wide). The
  DC3 same-compiler oracle could NAME engine-side anonymous targets (where rb3-Wii can't), but
  the payoff is gated on (A) for any TU carrying a struct drift.

## Recommendation
Do NOT scale the engine vein as a cheap-yield body-port wave (proven ≈0% here). IF pursued, scope
it as a **struct-layout-reconstruction campaign**: reconstruct retail RndEnviron (and the
DC3-drift struct family) layouts from Ghidra (RTTI COL base-chain + vtable + member xref anchors
like `mAmbientFogOwner@0x7c` + the 0xB0 delta), fix the shared headers, gate each on a whole-binary
composed A/B. The RndEnviron keystone is the highest-ceiling concrete next lever (renderer-wide
cascade), in the same class as the Handle +217 / truncation +108 / std::list +58 wins — but it is
foundational work, not a wave of cheap reveals.
