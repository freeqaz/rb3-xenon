# Capstone: cheap + structural matching levers exhausted across BOTH game and engine (2026-06-21)

The end-state of a long lever-hunt session (6932 → 9878, +2946). Every promising
vein was rigorously tested; this records WHAT REMAINS and why the cheap/structural
levers are exhausted, so a future session does not re-litigate.

## What was proven exhausted this session (with evidence docs)
1. **Scattered game layer = identification wall, mostly unrecoverable**
   (`fuzzy-locator-reconstruction-design.md` CONSOLIDATED VERDICT): rb3-Wii oracle
   near-random on game TUs; BSim seed-propagation NO-GO (`2026-06-21-bsim-seedprop-
   densification.*`); string-anchoring sparse (`2026-06-21-string-anchor-recall-probe.md`).
   Splits into class A (string-rich locatable core) + class B (un-anchorable panels).
2. **Class-A TU-pure span harvest = MODEST but real** (gated): GemManager **+35 landed**
   (TU-pure span + cheap compile → own funclets byte-reproduce even unnamed). GemPlayer
   REJECTED (mixed/foreign). Worth a small gated harvest of the remaining TU-pure isolated
   TUs (AppLabel/Matchmaker/PitchArrow/TrackPanelDirBase/OvershellSlot), NOT a big vein.
3. **DC3 engine vein "8599 unmatched" = INFLATED** (`2026-06-21-dc3-engine-vein-yield-pilot.md`):
   LightPreset's 132 = 93 unpaired (identification wall) + 22 reloc-name-noise + 10 permuter
   + 5 mis-paired + 0 individual-cheap. The same-compiler DC3 oracle helps NAME engine
   anonymous targets but the bodies often still diverge (permuter noise).
4. **DC3-drift base-class family = a SINGLE keystone, not a family** (this hunt, 7 lanes):
   **RndEnviron +9 landed** (retail derives Hmx::Object, our port copied DC3's
   RndTransformable+RndDrawable dual-base = uniform 0xB0 drift). The hunt found **0 other
   confirmed candidates**: the remaining DC3-vs-Wii base divergences in rndobj
   (RndMat:BaseMaterial, RndFont:RndFontBase, RndWind:RndHighlightable) are confirmed
   **RB3-360 == DC3** by the project's own 3-way Evidence-1 cross-check
   (`docs/plans/engine-reuse-and-asset-rendering.md`) — re-basing them would REGRESS. All
   char base-chains are already OUR==DC3==WII. RndEnviron was the lone case where retail-360
   kept the leaner Wii lineage despite DC3 adding the dual base, AND it's not in the
   Evidence-1 table — a genuine exception, not the start of a family.

## The decisive signature (for future hunts)
A TRUE base-class drift lands EVERY method of the class uniformly in the 40-70% band with a
CONSTANT lwz/stw member-offset delta (RndEnviron = 0xB0). This signature is now ABSENT from
all remaining pinned engine TUs. What fills their unmatched pools instead:
- **0% UNPAIRED** functions (anonymous `fn_*` the symbol-map never named) = identification wall.
- **95-99.99% permuter/codegen noise** (FP regalloc, instruction scheduling, commutative-swap).
- **STL template instantiations** (allocator/_M_* helpers) = codegen noise.
- Occasional **isolated single-member** shifts (e.g. CharEyes ctor +0x10, localized, not class-wide).

## What remains (the low-EV tail — honest EV ordering)
- **(highest-confidence) Class-A TU-pure span harvest** — ~5 isolated candidate game TUs;
  each pure+compilable one nets +2-35 (GemManager was the windfall end). Gate: validate
  span purity (string-content/call-topology, NOT the near-random oracle) + cheap compile
  BEFORE porting. Realistic total +20-60.
- **(low-confidence) Permuter sweep** on the 95-99.99% near-misses — historically converges
  ~0 on FP-regalloc/scheduling walls; needs decomp_synth; low EV.
- **(uncertain) DC3 same-compiler oracle (dc3_oracle.json)** to NAME engine anonymous targets
  (recipe in `2026-06-21-dc3-engine-oracle-feasibility.md`) — unblocks pairing but bodies
  often still diverge; build only if a naming pilot shows the named bodies actually match.
- **(one-off) Isolated single-member struct levers** (CharEyes +0x10 etc.) — small, per-class.

## Bottom line
The structural/cheap matching era is genuinely over (confirms wave-20's PRACTICAL-EXHAUSTION).
The big foundational keystones (Handle +217, truncation +108, MakeString, std::list +58,
RndEnviron +9) are all spent. Remaining matching progress is the low-EV tail above; the
honest big frontier (class B panels + the anonymous-target identification wall) needs a
fundamentally better identifier than any current oracle provides.
