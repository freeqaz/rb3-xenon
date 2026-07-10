# flow/ TU pins are phantom — retail RB3 has no Flow system (2026-07-10)

Found during the offset-drift recon (misc lane, dossier
`~/tmp/hdr2_recon_misc.txt` item 1). Registered here as a dedicated hygiene
task because it changes match accounting and touches splits.txt +
target_symbol_map.json (serialize with any other config-editing lane).

## Evidence (conclusive)

- `??1FlowIf@@UAA@XZ` is mapped to retail 0x823B4C20 (FlowIf.cpp split
  0x823B4C20..0x823B523C). Ghidra decompile of fn_823B4C20: installs the
  **CharUpperTwist** vtable (0x8204F724) and destroys exactly three
  `ObjPtr<RndTransformable>` members via fn_8228C248 — matching
  `CharUpperTwist.h` (mUpperArm/mTwist1/mTwist2), NOT FlowIf (two
  DataNodeObjTrack members + ~FlowNode). Only vbase-dtor boilerplate matched
  (99.74/99.88 fuzzy mirage).
- Address neighborhood: the "FlowIf" range is carved out of the middle of the
  CharUpperTwist.cpp TU cluster (CharUpperTwist.cpp split ends 0x823B482C;
  CharUpperTwist::PollDeps mapped at 0x823B5240, immediately after).
- **Retail string search finds no `.?AVFlowIf@@`, no `.?AVFlowNode@@`, no
  Flow-class strings at all.** This is a /GR build (every polymorphic class
  has a TypeDescriptor) and OBJ_CLASSNAME bakes class-name strings — absence
  is conclusive. `.?AVCharUpperTwist@@` IS present (0x82C3DCF0), proving the
  index works. The Milo Flow system post-dates RB3 (DC3-era).
- The dtor's −96 offset delta decoded: cross-class vbase placement
  (CharUpperTwist vbase +0x30 vs FlowIf +0x90), not layout drift.

## Scope

`splits.txt` pins ~23 more `src/system/flow/` TUs (Flow.cpp, FlowLabel.cpp,
FlowTrigger.cpp, FlowManager.cpp, ...). All are near-certain fingerprint
false positives — any "matches" they contribute are fictitious pairings of
EH/dtor boilerplate.

## Remediation plan (not yet executed)

1. Audit each flow/ split: for every mapped address, check retail RTTI /
   vtable installs / callee shapes against the claimed class. Expect ~all to
   be misattributed carve-outs of neighboring char/ or obj/ TU clusters.
2. Remove the phantom splits + their target_symbol_map.json entries (or
   remap to the true owners, e.g. fold FlowIf's range into CharUpperTwist.cpp
   with ??1/??_G CharUpperTwist entries).
3. Accept the honest negative delta this causes (false strict/fuzzy matches
   disappear). Quantify before/after with a composed A/B so the ledger notes
   it as an accounting correction, not a regression.
4. Register ??1FlowIf/??_GFlowIf as false-pairing in
   scripts/harvest/nearmiss_verdicts.json so the grind stops re-attempting.

Note `src/system/flow/FlowIf.h` is byte-identical to dc3-decomp's twin —
the SOURCE is fine for the native/DC3 tracks; only the retail pins are wrong.
