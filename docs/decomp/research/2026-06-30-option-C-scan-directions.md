# Option-C "different scan" investigation — 4 Opus probes + synthesis (2026-06-30)

The class-A STRING-anchored span harvest is exhausted (+403/session). User picked option C:
a DIFFERENT scan for engine/string-rich TUs. 4 parallel Opus subagents probed 4 methods with
hard-number feasibility. main @~10664 matched.

## Verdicts (triangulated)
| Direction | Verdict | Key number |
|---|---|---|
| String-free TU-purity (topology-only) | NO-GO | 0.9% boundary precision; /Ob2 inlines intra-TU calls; class-A winners ≈ noise |
| DC3-map span-transfer (locate via ham_xbox_r.map) | NO-GO standalone | forward IoU 0.00 on 6/8; ICF decoys beat real span. Salvage: reverse plurality vote 6/7 = fused confirm signal |
| **DC3-oracle engine BODY harvest** | **GO (thin tail)** | +40-70 / 19 fresh engine TUs / ~146 core methods |
| **Stub-filtered contiguity port-then-pin** | **GO (moderate)** | ~+30-70; ~480 real-body ceiling / 125 stub-contaminated clusters |
| map-augmentation | DEAD | +0-4 (in-pin∩0% reveal = 4 fns, version-divergent) |
| clean near-miss-to-100 | THIN | +20-40 @ +1 each; ~50% regalloc walls; clean = small-insert/no-reg-swap (CharEyes::EyesOnTarget) |

## The decisive insight
LOCATING new spans is dead (topology + DC3-map both hit the SAME oracle recall wall — ICF
folding builds denser DECOY clusters than the real span). BUT the oracle locates a TU's dense
VA CLUSTER well enough, and engine bodies (DC3 same-engine twin) often BYTE-MATCH. So the
productive method = **oracle-cluster-located PORT-THEN-PIN, gated by a STUB-FILTER** (reject
≤48-64B ICF stub-fold inflation — the ProfileMgr 116/140-stub trap). Directions 1 + 4a are the
SAME method; merged = ~+60-100 strict, front-loaded in the cleanest TUs.

⭐ KEY REFRAME (DC3 engine agent): the oracle sim is a LOCATOR/NAMER, NOT a feasibility scorer —
sim is NON-PREDICTIVE of port success (MetaMusic matched@sim0.443; MeshAnim diverged@sim0.985).
The real predictor = BODY COMPOSITION (clean-logic vs STL-template) + locatability + stub-ratio.

## THE PLAN: oracle-cluster port-then-pin harvest (merged 1+4a) + near-miss batch (4c)
Per TU: locate dense VA core-cluster from dc3_oracle.json -> REFINE to real fn boundaries ->
wire objects.json (if unwired) -> pin splits.txt -> gen target map from oracle mangled names ->
PORT body from ../dc3-decomp/src/system (engine) or ../rb3/src (game), cross-check the other ->
build -> icf_alias_check (REJECT stub-folds, count only real >44B bodies) -> composed A/B run1==run2
-> land honest. Gate selection on real-body-count x core-purity x clean-logic, NOT sim.

### First targets (deduped, ranked by real-bodies x cleanliness x low-risk)
1. synth/Sound.cpp — engine, ~0x8243a10c core, 30 methods purity 1.0 sim 1.0, DC3 body. Biggest clean win.
2. CharClipGroup.cpp — engine, ~0x824A7D88-0x824A8E58, 26 real/12 stub, WIRED-unpinned, CharClip/CharBones family solved. Lowest friction (pin+match, no port).
3. utl/SongInfoAudioType.cpp — engine, ~0x82530f50-0x82531198, 18 purity 1.0, DC3+rb3-Wii.
4. NavListNode.cpp — game meta_ham, ~0x826454E8-0x82646220, 19 real/5 stub, rb3-Wii source.
5. synth/MoggClip.cpp — engine, ~0x82448254-0x8244922c, 12, sim 0.971, DC3+rb3-Wii.
6. AccomplishmentProgress.cpp — game, ~0x825786F8-0x82578D70, 13 real/3 stub, WIRED, family solved.
7. rndobj/MotionBlur.cpp + SoftParticles.cpp — engine, 6 small perfect cores, AmbientOcclusion sibling.
8. ProfileMgr.cpp — game keystone, ~0x825FDA94-0x826006A0, 23 real / 116 STUB — LAST, icf_alias_check-GUARDED.
+ near-miss batch (4c): CharEyes::EyesOnTarget (2-instr insert) + same "small-insert/no-reg-swap" signature.

### Spans are ±NOISY (oracle cluster) — each harvest agent MUST refine to real fn boundaries + run the stub-filter BEFORE pinning.

## DEAD / don't attempt
String-free topology scan; DC3-map standalone locator; map-augmentation; the 41 scattered unwired
engine TUs (un-locatable); bulk oracle-naming (dc3-naming-pilot +0); XDK xgraphics/d3dx9 blobs (no .cpp).

## Reusable artifact to build (future): a STUB-FILTERED CONTIGUITY SCAN tool
oracle dense-cluster ∩ fingerprints.json real-body sizes -> rank unpinned TUs by HONEST matchable
bodies (not inflated oracle counts). Would make target-selection rigorous for future waves.
