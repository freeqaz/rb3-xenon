# TU5 P5 — post-flip matching progress

**Status:** ACTIVE. Single current-state tracker for the post-TU5-flip matching
campaign. Companion to `tu5-landing-runbook.md` (the flip itself) and
`tu5-p5-manifest.md` (the enumerated-drop attribution). Updated per wave.

## Scoreboard

| point | matched | note |
|---|---|---|
| TU5 flip landed | 14,847 | `d9c44305`, 2026-07-15 |
| struct-rebase waves 1–4 | 15,100 | +253, keystone header re-bases (Player/BandDirector/Game/…) |
| fixwaves (concurrent) | 15,160 | +60, ≥99% near-miss triage lanes |
| wave 5 + 5b | 15,205 | +45, online/SongSort/TrackConfig **map re-anchors** + 1 permuter flip |
| wave 6 | 15,207 | +2 (LightPreset body port + 1 unicorn fix); **3 veins closed** |
| BinStreamRev lever | 15,226 | +19 foundational (base-at-0 inheritance; streaming family); LEAD B ~+15-20 pending |
| wave 7 | 15,227 | +1 (Game::Poll gameplay RE); bodyport premise refuted, re-anchor vein found |
| re-anchor | 15,236 | +9 (EndingBonus/GemSmasher drifted-map refresh; size-match guardrail) |
| jeff Class-2 merge | 15,303 | +67 (leaf-fragment merge pass; `eb4863cc`, jeff `7f69b9e`) — fleet tooling lever |
| BinStreamRev LEAD B w1 | 15,319 | +16 (per-TU rev statics, 4-lane wave; `677fc117`) — 9 ICF/sibling bonus flips rode along |
| jeff Class-4 over-carve merge | 15,354 | +35 (post-blr/branch over-carve tail merge; `f03b9719`, jeff `b50881e`) — 2nd fleet tooling lever |
| SongSort identification | 15,364 | +10 (verified map entries, SongStatusMgr family; `528c51c7`) — vein drained, rest ICF-walled |
| nearmiss crack sweep w1 | 15,370 | +6 (Opus-triaged 40–80% <400B pool → Sonnet crack workflow; `40688918`). Wins = source-genuinely-incomplete: DataArraySongInfo::Save (+7 fields), RockCentral::Verify{Char,Band}Name (DP_KEYS2 twin), TypeProps::operator=+ClearAll (ring-walk ReleaseObjects/AddRefObjects, inlined), RndPostProc::Reset (drop UnSet). 12 co-triaged fns walled = **all source==oracle** (structural/permuter/layout). |
| nearmiss crack sweep w2 | 15,373 | +3 (`4fa04926`). DataReadStream (CritSecTracker RAII + drop dead gNode=0; +ICF sibling fn_8276C0F8), Gem::AddWidgetInstanceImpl (out-of-line Hmx::Scale helper vs 9 inline fmuls). 5 walled = all source==oracle. |
| nearmiss crack sweep w3 | 15,376 | +3 strict +fuzzy+185.8 (`9b88b27b`). Strict: GroupOwner (RefPtrOf(it)->RefOwner X360 + drop null guard), UIListWidget::CalcXfm (retail adds x,z only not y), TrackWidget::Poll (hoist CutOffY float local). Fuzzy: PatchPanel::Load 94.8 (local-static Symbol), NextBuf 97.1 (MILO_ASSERT vs snprintf), PoolAlloc ~97 (strip MemTrackAlloc). |
| structural leads x3 | 15,382 | +6 (`8886591c`). All 3 deferred leads confirmed+landed: ChunkAllocator MAX_FIXED_ALLOCS=32 (retail `new(0x80)`; HX_NATIVE keeps 64) → PoolAlloc 100; HttpGet::State true retail order Downloaded=3/Failed=4/FailedSend=5/Pending=6 (SetState/Poll disasm) → DetachBuffer/IsDownloaded/HasFailed 100 (+2 map entries); SampleInst setters NON-VIRTUAL in retail (direct `bl`, no PlayableSample vtable slots — resolves the SfxInst +0x28 this-adjust) → SetSpeed/SetReverbMixDb 100, UpdateVolume fuzzy 57→77. |
| nearmiss crack sweep w4 | 15,385 | +3 strict +54 fuzzy (`c6d0b969`), 400–800B band. EditSetlistPanel::SyncProperty SYNC_PROP_SET→**SYNC_PROP_SET_STATIC** (retail fn-local static Symbol per prop) 70.3→99.3 + flips 3 guard funclets strict; StoreMainPanel::FinishLoad (fn-local static Symbols + MarkDirty(2) vs Wii mShaderVariation|=2) 71.4→93; StoreMainPanel::Poll (.empty() vs .size()==0) 80.8; Campaign::ConfigureCampaignLevelData (drop no-op'd MILO_WARN dead block) 73.6. TrackPanel ctor walled = ObjPtr 2-arg-ctor inline (closed at-limit). **NEW LEAD: sweep other BEGIN_PROPSYNCS users for the SYNC_PROP_SET_STATIC pattern.** |
| **global correlator sweep** | 16,878 | **+1,493** (`366709b9` map, driver preserved `3325c7cd`). Global `tu5_reloc_masked_correlate` run over all 573 paired units at the post-Class-2/4 baseline: 1,495 CLEAN 1↔1 reloc-masked byte-identity map entries across 313 units (Stats 32, UIList/GemManager/Accomplishment* 27, obj/Dir 22, TrackPanel 20…). Verified twice (independent worktree A/Bs), +1,493/−0, fuzzy +149,070. The earlier "SongSort real_clean=0" result was unit-local, not global. |
| ICF disambiguation (base-side) | 16,996 | **+118** (`01c1414f`). Reloc-target-identity discriminator over MULTI groups (51 reloc-discriminated + 67 ICF-identical folds). Key correction: the "5,722 MULTI" pool was ~4/5 `__unwind$`/`__ehhandler$` COMDAT pollution (CODE+fn flags in MSVC X360 objs) — honest base-side yield 118. Scanners `tu5_icf_disambiguate.py` + `tu5_reloc_seq.py`. |
| target-twin disambiguation (Lever 3) | 17,230 | **+234** (`1000b661`). Mirror discriminator on target-side twin groups (231 reloc-discriminated + 3 identical folds; 922 coincidental matches rejected). Scanner `tu5_target_twin_disambiguate.py`. **Identification stack total: +1,845 this session.** Residual identification: skip_no_S 922 (coincidental) + nomatch ~5,300 (genuine body divergence → body-port lane). |
| nearmiss crack sweep w5 | 17,293 | **+63** (`d7aacbbd`), 99-band cheap fixes. Star: BandWardrobe SYNC_PROP*→_STATIC 57.8→99.7 cascading **51 flips**; Game::Properties AllowOverdrivePhrases really at Prop+0x5 (old +0x3 = mUnkTU5_movieSync) → 3 flips; BandUser TU5 virtual precedes IsParticipating (vtable+0x8 proof) → CreateTracks+2; Debug sizeof 0x144→0x100 (strip DC3 Crucible) → DataWriteFile; BandProfile +4 filler → GrantAward/GetBandLogoTex; TubePlate drop mNumVerts; BlurRT PShaderConstant 0x21/0x31; PoseMeshes. **PROPSYNCS vein DRAINED** (only ObjMacros.h-including TUs can flip — Object.h's macros are inherently static; BandWardrobe was the last). Walls: Synth360::Terminate/PlayNextShot/AddDircut/StreamReceiver360::Poll = regalloc/CSE permuter-class. ~~PlatformMgr keystone lead~~ REFUTED next wave (see below). |
| nearmiss crack sweep w6 | 17,300 | **+7** (`ac0b1763`). LoadMgr TU5 field reorder (mPlatform/mEditMode/mCacheMode → tail) flips AddLoader+LoadStream+LoadMgr::Poll; Splash::EndSplasher retail calls **Resume() not Suspend()** (Ghidra vtable+0x118); XboxContentMgr unk-offset fixes; StreamPlayer NewStream arg; Singer mLastFrameMicEnergy. 6 walls (regalloc; OnEnterCloset permuter-exhausted 185 variants) + 1 pool artifact (MeshAnim list<Color> symbol nonexistent). **PlatformMgr keystone REFUTED** (`docs commit`): layout genuinely diverges (retail MsgSource-lineage compact, mSigninMask@0x1c/mConnected@0x26/mScreenSaver@0x2c) but ZERO strict gain — accessors unpinned, ThePlatformMgr global-reloc access offset-normalized by objdiff. DEAD lever, comment banner in PlatformMgr.h prevents re-hunting. |
| nearmiss crack sweep w7 | 17,329 | **+29** (`dcb2444c`). Star: **NewObject factory family** — one FXSEND360_NEW macro line (explicit `.Str()` on the discarded StaticClassName() temp forces retail's separate EH-temp stack slot) flips 10 FxSend*360 factories + 10 Synth funclets. Plus sizeof/layout proofs from `li r3,<size>` in factories: PracticePanel 0x94 (reinstate unk59), HighFiveGestureFilter 0x30, Voice 0x74 (PoolFree proof — also cracked w5-wall StreamReceiver360::Poll), StreamReceiver reorder, UIFontImporter layout (+GetGennedFont), RndLight::Save bool-temp form. **Transferable lever: NewObject `li r3,N` = ground-truth sizeof for ANY class with a pinned factory — cheap layout oracle.** Walls: HamNavProvider op<<, Keys::AtFrame, EventTrigger erase, yylex; ~~FlowAnimate/FlowDistance = phantom pins~~ (RETRACTED next row). |
| sizeof-oracle scan | 17,332 | **+3** (`c48f6527`). Scanner `scripts/harvest/newobject_sizeof_scan.py` (retail `li r3,<size>` vs ours, per pinned factory): 38 classes, 35 match (validates w7 fixes), 3 drift. FlowAnimate/FlowDistance +16 trailing pad (retail FlowNode-derived = dc3+0x10 in the virtual-Hmx::Object tail) → 3 flips incl. ??_GFlowDistance. The w7 "phantom pin" verdict on these two was WRONG — real size drift. Leads: FileMergerOrganizer +212 (structural RE), FlowSound +16 (factory unwired), ~~durable FlowNode-seam fix~~ REJECTED in w8 (shared vbase loses ??_GFlowDistance — dtor codegen; per-class pads stay). |
| nearmiss crack sweep w8 | **17,339** | **+7** (`e1f0d6cb`). StorePreviewMgr field order (3 flips); UIListElementDrawState fabricated DC3-only 16B overlay removed (Draw + funclet); TrackInterface GetTrackIcon/UserName vtable slots swapped vs retail. Gate save: FlowNode shared-vbase refactor REJECTED by combined A/B (lost ??_GFlowDistance). FlowSound lead sharpened: TU compiled + map entry exists but NO splits.txt pin. New flag: ~5 Flow-family fns "Stub(High)" mispinned to unrelated units. **Full session summary: `docs/plans/session-2026-07-18-summary.md`** (15,364→17,339, +1,975/-0). |
| opus wave 9 (99-band top + structural) | **17,340** | **+1** (`07409503`). 12 all-Opus agents. Crack lane 9/9 WALLS — **the 98.7-98.9 band top is CLOSED for crack waves** (regalloc/scheduler cascades; new named wall class: **MSVC strcpy-intrinsic terminator test** — retail `cmplwi r,0` vs our `extsb.` inside the inlined strcpy; /J and `#pragma intrinsic(strcpy)` both tested, not source-expressible; hits SetPathName@ObjectDir, FirstSortChar, OnChangeFaceGroup@BandCharacter). The +1: **BandCharacter::OnPreClear** — crashed agent's in-flight `ObjPtrList` hoist finished by coordinator + **unk738/unk734 member swap** (retail `stw 0x7c4` proof; both edits load-bearing). Structural verdicts: **FileMergerOrganizer "+212B gap" = FALSE POSITIVE** — pinned NewObject@0x8268f050 is a foreign vbase/LocalUser class (vbtable, vtordisps, LocalUser@~0x104); real FMO is 0x3c, proven by the 100% dtor destroying no LocalUser. Follow-ups: re-pin true FMO NewObject; teach `newobject_sizeof_scan.py` to reject vbase-ctor callees. **FlowSound pinned** (NewObject island [0x8256ED88,0x8256EDEC) carved from ContextChecker; net-0 — we never emit ?NewObject@FlowSound, factory registration unwired). **Mispinned-Flow-5 = COMDAT-placement wall**, map+spans already correct, nothing deterministic to fix. Permuter BANNED by user directive (low yield + grinds box). |

| lane-C scanner-stack re-run (round 2) | **17,366** | **+26** (`16816838`). Re-ran the 3-stage identification stack after the +109 named landings strengthened the reloc-name resolver: stage-1 clean sweep +21, base-side MULTI +2, target-twin +3; **round-2 fixed point (0 new)**. 26/26 adversarial Opus/Ghidra spot-check PASS. Rejection gates held (skip_no_S 412+874 stays rejected; `__unwind$` filtered). **The stack is now re-runnable end-to-end**: `scripts/harvest/TU5_SCANNER_STACK.md` runbook + `tu5_gen_pairs.py`/`tu5_correlate_stage1.py`/`tu5_map_apply_fragment.py`. Confirms the standing rule: re-run scanners after named-set growth — marginal yield ~26 per ~109 new names. |
| lane-B nomatch near-pair vein | **17,401** | **+35** (`29cf4bd5`). Fable coordinator + 8 Opus crack agents on the ~6,300 identification-nomatch residue. New scanner `scripts/harvest/tu5_nearpair_scan.py` (union-reloc-masked word hamming, size + reloc-count + no-name-contradiction gates) ranked 4,420 near-pairs; ~60 real targets. **+10 free map flips** (masked diffs absorbed by objdiff reloc normalization) + **+25 source cracks** (24/43 dispatched cracked-100): Object.h `op>>(BinStreamRev&)` returns `bs` not `bs.stream` (4 instantiations, general lever); SfxInst rb3-Wii `mMoggClips` ObjPtrList port (+ sibling SetReverbMixDb); CameraShot.h mCrowdStateOverride before mCrowds (CamShot trio + 2 funclets); Campaign 0x825a6358 mispair→IsUserOnLastCampaignLevel (+3 funclets); fabsf double-promotion; named-local Symbol return slots x2; DC3-only mWritingHeader strip; kContentMgrState 6; KeylessHash::Find jeff over-carve merge (symbols.txt). Denylisted 0x82685750 (PracticeSection dtor ICF mispair); 3 mispairs excluded. **Vein verdict: DRAINED** — residue = 2,633 reloc-count-differs + 1,887 no-candidate deep divergence + 1,034 tiny stubs + walls. **Structural leads surfaced**: Rnd vtable 3 extra slots (ours 0x130 vs retail 0x124 — force multiplier, scout in flight); UIComponent-chain layout drift (Load family); Flow missing ~0xD0 of members; LyricPlate +0x40 (RndText::Style 0x24 vs DC3 0x4c). Independent A/B at HEAD: +35/−0. |
| Rnd vtable residual +0xC | **17,403** | **+2** (`ad9376d5`, after FMO scanner fix `4c11e0a7` net-0). Read-only Opus scout triple-anchored the delta: retail DxRnd vtable ENDS at slot 73 (UpdateScalerParams@0x124, null@0x128) — DC3-only virtuals NgRnd CreateLargeQuad/DrawLargeQuad/SetVertShaderTex gated behind RND_DC3_VIRTUAL (existing idiom; native keeps dispatch). Flips SetAspect + SetShrinkToSafeArea (sole mismatch = vcall 0x130→0x124). UpdateScalerParams paired (new map entry), 0% = genuine body-port lead. NOT the hoped multiplier — the 8-virtual pass landed earlier; this was the residual. Rnd_Xbox unit still 52/103 anonymous fns (correlator-walled, body-divergent). Scout report: ~/tmp/rnd_vtable_scout.md. |
| naming wave (vtable positional alignment) | **17,409** | **+6 strict, +110 names** (`034a448a`). NEW INSTRUMENT: align compiled `??_7Class@@6B@` method order against retail .rdata vtable pointer runs via mapped anchor slots → anon fn_ slots inherit names (2-anchor + 1-anchor-recovery gates, fixed-point iterated). `scripts/harvest/vtable_{global,1anchor}.py`. Yield: 6 strict flips, 39 pair >0%, 65 named-at-0% crack fodder; +2 jeff over-carve merges; 20 map keys case-normalized. Prior veins confirmed exhausted (Ghidra-bank∩map 99.8%). **Correlator REARMED** (~163 new names since round-2 fixed point) → round-3 re-run dispatched. Also: Flow.cpp pin is a wrong-TU pin (BandSwatch/ColorPalette TU — RTTI-proven; the "Flow -0xD0" lead was this mispair), re-pin agent in flight. |
| Flow.cpp re-pin → BandSwatch unit | **17,412** | **+3** (`88bd166b`). The wrong pin was scoring 16 SPURIOUS generic-stub matches under Flow.cpp; range re-pinned as BandSwatch.cpp (RTTI owner), BandSwatch.cpp ported from rb3-Wii + compiled (19/109 at 100% — 16 migrated honestly + 3 net-new). Stale 0x822acfe8 FlowNode map entry dropped. LyricPlate/RndText::Style = DEFER/WALL (retail Style 0x24 vs DC3 0x44; 272-fn blast radius; rb3-Wii re-port recipe banked). Remaining ~90 BandSwatch fns = gameport follow-up vein. |
| lane-C round 3 (post-rearm) | **17,417** | **+5** (`23e6cbb8`). Stage-1 +4, target-twin +1, MULTI 0; fixed point round 2. 3/5 = more BinStreamRev ObjVector op>> instantiations flipped by lane-B's Object.h `return bs` lever. **Calibration collapsed to ~0.031 flips/name** (~8x below round-2) — round 4 only after 200+ names or a new pinned cluster. |
| UIComponent Load-family verdict + StarDisplay ctor | **17,418** | **+1** (`a2a96d57`). "UIComponent-chain layout drift" REFUTED: StarDisplay ctor pinned = 100% (91/91) → layout byte-identical; Load/PostLoad residuals (+0x14/+4) = vbase-displacement codegen for sibling-virtual dispatch, per-class WALL. **NEW DIAGNOSTIC RULE: pin+diff the CTOR first — ctor 100% ⟹ codegen wall (skip member hunts); ctor <100% w/ offset shifts ⟹ real layout drift.** 3 diagnostic Load entries paired at 99.8 (honest attribution). |
| crack-fodder body-ports (named-at-0% pool) | **17,421** | **+3** (`dea64514`). RndShaderParticles::CheckError DC3-vs-RB3 constant divergence (enum {0,2}→{1,3}, drawmode !=4→!=3 — our source == DC3 which is WRONG for RB3); SavedSetlist SetTitle/SetDescription unwired out-of-line defs carved into SongSortMgr TU. **PRECISION CAVEAT on naming wave: 60/65 named-at-0% entries are FOREIGN-ADDRESS** — vtable-positional naming named a slot but the address belongs to a different/ICF-fold TU (run_objdiff "symbol not found"); inert (0%, no regression) but dead weight → follow-up = re-attribute/ICF-resolve, not body-port. Only 3/65 were in-range genuine stubs. |
| BandSwatch gameport (byte-pairing) | **17,443** | **+22** (`6c98d9f7`). rb3-Wii port already byte-correct; 19/109 baseline was UNPAIRED (anon fn_ w/ no map entry = false-0%). 22 reloc-masked byte-identity map entries. Remaining 62 walled (regalloc/INIT_REVS/foreign-TU templates). |
| RndText::Style re-port 0x44→0x24 | **17,445** | **+2** (`1d7e8356`). **OVERTURNS the DEFER/WALL** — the "272-fn blast radius" was theorized without building; a 0x20 Style-pad probe proved LOST 0/GAIN 0 (no matched fn encodes a Style-relative offset — recompiles ≠ regresses). Real rewrite surface = 2 files. Flips LyricPlate::CheckSync + VocalTrack::GetLastBakedLyric. **LESSON: prove blast radius empirically (pad-probe), don't theorize it.** |
| pad-probe deferred struct walls | **17,445** | **+0** (drained). Probed 4 deferred "blast radius" walls: BandCamShot (3/0 tail + 11/0 mid = genuine vbase-MI reconstruction wall, vbase 0x1f0 vs retail 0x270 + ~0x84 interleaved — NOT a tail-size fix), GamePanel MsgSource MI-base (31/0 = load-bearing correct; near-misses are call-arg body diffs), XboxSessionJob (0/0 SAFE but MOOT — near-misses are map mispairs), RndMat/BaseMaterial (already reconciled). **Verdict: deferred struct-blast-radius vein DRAINED** — all named DC3-strip/layout leads landed in prior waves; residuals = vbase-MI recon / mispairs / codegen. Pad-probe earned its keep in REVERSE: killed a tempting BandCamShot +0x80 reconstruction before wasting effort. |
| foreign-address naming re-attribution | **17,445** | **+0** (`938c3529`, hygiene). Audited all 110 naming-wave entries: the crackfodder "60 foreign" alarm was overstated — 44 in-range, 11 correctly address-placed in another pinned unit (4 pair, incl. Priority@SpotlightDrawer 100%), 53 correct base/ICF names in unpinned space (FlowSound precedent). Foreignness = per-unit PAIRING artifact, not naming error. Removed 2 genuine mislabels (purecall thunk mislabeled SecBetweenUploads; wrong-slot ByteCode). Banked: gate vtable_global.py (route cross-unit → review bucket + purecall guard + return-shape check). |
| tooling: vtable-namer 4-part gate | **17,445** | **+0** (`eecf7853`). Implemented the gate: owning-unit router (splits.txt spans; cross-unit/unpinned → vtable_unpinned_review.json), R6025 purecall guard, empirically-safe return-shape hard-reject, conflict discipline. Gated re-run: 11 raw → 1 new clean name (UITransitionHandler::StartValueChange 85.9% fuzzy) + 5 review-bucketed, both prior audit mislabels auto-intercepted. Also: Ghidra TU5 oracle SYNCED (run_apply_symbols.sh, 2,173 names, 15/15 spot-checks, :8002 healthy) — bank current thru all today's IDs. |
| **2026-07-19 review** | 17,445 | 3 Opus scouts: body-port pool + recarve ranked, 99-band round-2 REJECTED (`d9798afe`) |
| rc1 Accomplishment gap-fill | 17,575 | **+130** (`605af89b`) — blob 0x825F71A0 = missing middle of AccomplishmentPanel.cpp; 58 map entries, 0 LOST |
| bp3 COMDAT-scatter includes | 17,601 | +26 (`177cb4b9`) — owner-TU #include trick (TDStretch/DataPointMgr/MidiSynth) |
| bp1 retail-TU reunification | 17,637 | +36 net (`ce936710`) — Debug/DirLoader/MemHeap bodies duplicated into span-owner under !HX_NATIVE; 1 funclet-echo slip accepted |
| rc3 TrackWatcherImpl gap-fill | 17,651 | +14 (`3a7e3677`) — single-TU gap, 8 byte-identity entries + 6 funclets; 121 flat-0% = beatmatch bodyport vein. rc2/SongSort blob = COMDAT catch-all from UNWIRED classes (kill test: pre-mapped names emitted by no wired obj) — 0, correctly skipped |
| MemHeap 0x24 | 17,652 | +1 (`7fd6745a`) — gate DC3-era mMinFreeBytes; rest of cluster = grouped-globals codegen wall |
| bp2r rndobj scatter includes | **17,736** | **+84** (`c25769a8`) — Console←MultiMesh, Env_NG←Rnd_NG, MeshAnim←MultiMesh+ShaderMgr+mtx (gRev macro-rename); 25 named + 59 funclets; overturned bp2 "tooling wall". Fresh near-misses exposed (UpdateOverlay 94.2, UpdateCache 99.8) |
| scatter-sweep w1 (scanner + 26 units) | **17,910** | **+174** (`9e08aed8`) — NEW INSTRUMENT `scripts/harvest/comdat_scatter_scan.py` (COFF scan: named-0% → SCATTER vs UNWIRED); 26 owner-TU includes, 3 lossy auto-reverted; +82 named +92 funclets. Biggest: PropSync←Dir +26, BandDirector←CharClip +22, FileMerger←CharIKHand +15. Residue: 509 SCATTER (include-safety blockers: PROPSYNC barewords, d.rev, math.h pow) + 327 UNWIRED (gameport pool); wave-2 dispatched |
| scatter-sweep w2 (include-safety) | **18,339** | **+429** (`26fba899`) — 164 owner-TU includes / 123 files; probe→A/B pipeline. +172 named +259 funclets, LOST 2 (both anon funclet-echo @99.9%, named 0). Root-caused w1 blockers = Object.h vs ObjMacros.h macro dialects (PROPSYNC/d.rev/INIT_REVS). Top: ContextChecker←Char +30, ByteGrinder←HamBattleData +20, Stats←BandDirector +18. Residue 227 proposals / 300 SCATTER. Wave-3 lead: Object.h/ObjMacros.h dialect shim → BandCamShot←HamCamShot 1480B + class |
| scatter-sweep w3 (cross-dialect shim) | **18,397** | **+58** (`6db605e0`) — NEW byte-neutral `obj/dialect_object_{push,pop}.h`: ObjMacros-dialect consumer whole-file-includes Object-dialect owner .cpp (push_macro/undef/reinstall/pop over INIT/LOAD/ASSERT_REVS+SYNC_PROP+HANDLE families) + SW_SCATTER_OWNER_INCLUDE transitive-collision sentinel. 9 levers: BandCamShot←HamCamShot +24, TrackWidget←ClipCollide +14, TrackDir←Anim +9. GAINED 59 LOST 1 (anon funclet-echo, named 0). Residue 218 proposals / 275 candidates. Reverse-dialect vein (Object←ObjMacros) EMPTY; same-dialect residue = wave-2 net-0 pool |
| UIStats recarve gap-fill (rc4) | **18,445** | **+48** (`5f13960a`) — blob 0x82560660 single-TU gap under UIStats.cpp; UIStats::Handle anchor + 46 funclets. Blob 0x8234FCEC SKIPPED (kill test: unwired gesture catch-all) |
| near-miss gates (nm) | **18,448** | **+3** (`1795eef9`) — NgStats mSpotlights strip (UpdateOverlay 94→100 + 15 offset drifts), NgRnd::Terminate, Geo.h Sphere-multiply force-multiplier. Verify NOTE: header patch needed FULL rebuild A/B (report-only warm-seed gave false 585-LOST). Leads: RndShaderMgr ShaderType enum 26 vs 38 |
| identification round-4 (id4) | **18,618** | **+170** (`39038c09`) — scanner stack RE-OPENED by the scatter campaign's +250 names & 3 new clusters (round-3 fixed-point gate cleared). Stage-1 +121/ICF +16/twin +32, fixed point 3 rounds, ~0.157 flips/name = 5x collapsed rate, 6/6 Ghidra PASS. The scatter vein FEEDS identification — subsystems compound |
| ShaderType enum 38→26 (sm) | **18,621** | **+3** (`d0864f7c`) — retail-asm-grounded (li r4,0x14 + subi r11,r4,0x1a prove kMaxShaderTypes=26); DC3's 12 Dance-Central shaders HX_NATIVE-guarded. Flips Terminate/Invalidate/InitShaderOptions. Full-rebuild verified. Leads: FindShader 80.3, SetTransform 82.5 |
| synth near-miss (InitParams) | **18,622** | **+1** (`d6b0a586`) — FxSendMeterEffect360::InitParams 98.3→100: r29/r30 order via mChannels pointer-local + named LevelData mono temp + direct-member tail load (lwz-before-stw). Clean-worktree A/B +1/LOST 0. PreInit STOPPED (frame-size/temp-coalescing divergence, DC3 stub=no oracle). FindShader/SetTransform AT_LIMIT (regalloc + RB3<DC3 vtable slot 0x5c vs 0x60) |
| scatter expose-and-fix Wave1 (Fable-directed) | **18,637** | **+15** (F1 `662788c7` +10 freebies; D1 `d225d546` +2 UniqueFilename splits-pin + SystemMs static-fold; D2 `6c2875b1` +3 PropAnim←PropKeys SetKey body-dup) — captain reframe: net-0 scatter residue = near-miss discovery engine. **RECALIBRATION: the ≥88 exposed band is a MIRAGE across all 3 taxonomies** (mispair F4 / reloc-co-location F3 / struct-artifact S1-S3). Only exact-100-on-include freebies + true body-dups flip. Struct-recon vein DEAD (5/5 leads ICF/foreign-offset mirages, 0 regr via verify-before-edit). Grouped-globals fold = static+reverse-decl (not extern). New durable rules: rm report.cache before A/B; nested-owner shim unsafe; sw2-parent-leak #ifndef gRev guard |

**Ceiling note (2026-07-18):** the old 15,804 "recover-all-but-48" ceiling measured
only the TU5-flip recovery scope and is now OBSOLETE — the global correlator sweep
overshot it by +1,074. Residual identification headroom: 6,419 MULTI/amb_tgt
ICF-ambiguous pairings (need spatial/xref disambiguation) + 6,493 nomatch
(genuine body divergence).

**Near-miss crack method (works; wave 2 in flight):** mine `report.json` for
40–80% & <400B fns → **Opus** tractability triage (Ghidra TU5 bank + dc3/rb3wii
oracle, read-only) splitting TRACTABLE vs WALL → **Sonnet** crack workflow, each
fn in its own `setup_worktree.sh` worktree with a **mandatory source-vs-oracle
gate** (source==oracle ⇒ structural/permuter wall, abort — do NOT hand-crack) →
coordinator harvests worktree diffs, one isolated whole-binary A/B (clean-HEAD
baseline set vs patched set), land +N/−0. Wave-1 signal: **wins come only from
genuinely-incomplete source** (body-ports, missing macro/guard/fields); every
`source==oracle` fn at 40–80% is a structural/regalloc/block-sinking wall for
hand-cracking → route to permuter or leave. **Deferred structural leads** (scoped follow-ups, each cross-cutting → own A/B):
- `SampleInst` carries a DC3-only `PlayableSample` virtual-MI base absent from
  retail (forces `+0x28` this-adjust on `SfxInst::SetSpeed/SetReverbMixDb` — fuzzy
  74.9→95.6; real fix = drop the base, cross-cutting header change).
- `ChunkAllocator` size: `MAX_FIXED_ALLOCS`=64 gives `new(0x100)` but retail is
  `new(0x80)` → real bound is 32 (blocks PoolAlloc's last insn; touches
  ChunkAllocator ctor/Alloc/Free/Print across the TU).
- `HttpGet::State` enum ordering: `kHttpGet_FailedSend` compiles to 7 but retail
  uses 5 (blocks StartSending row 13 + SetState switch + Poll; re-derive true
  ordering from SetState/Poll evidence). HttpPost::StartSending likely shares the
  mPath/mHeaders-semantics drift.
- Timer::ClearSlowFrame + NoDeviceChosenMsg: possible `target_symbol_map.json`
  mis-attribution (source==oracle yet sub-80%); low-value single fns. **All cheap/tooling/identification
veins are drained.** New reusable instrument: `scripts/harvest/tu5_reloc_masked_
correlate.py` (byte-identity pairing after masking COFF relocs — bypasses TU5
address drift; the right tool for whole-unit-0% drifted units). Note:
`gen_game_target_map.py` is DEAD for TU5 (its `unified_id_rb3wii.json` oracle is
TU0-addressed → all `out_of_span`) — use the correlator instead. **Cheap veins + both tooling
multipliers (BinStreamRev LEAD A, jeff leaf-split) are landed, and LEAD B wave 1
is in.** Remainder is BinStreamRev LEAD B wave 2 (evidence-selected by
`scripts/harvest/leadb_signature_scan.py` — incl. cross-TU rev owners like
SampleZone←MidiInstrument::Load), permuter-harness tooling, jeff Class-1/3
(terminatorless fragments, stray except_data), and genuine C_DIVERGED body ports
(Matchmaker rewrites, SongSort gen_game_target_map identification wave).

## Vein status (what's drained vs live)

**DRAINED / CLOSED (do not re-run — evidence in memory `project_tu5_p5_wave6`):**
- **Struct-rebase keystones** — the cascading member-insert wins are captured
  (Player/BandDirector/Game/BandUser/GameMode/User/Loader/MemMgr + wave-6
  residual). Yields decayed 111→112→26→4→~0.
- **Stale-anchor map sweep** — Ghidra ground truth agrees with the map on all
  6,178 zero-scoring named entries. Reusable tool
  `scripts/harvest/tu5_stale_anchor_sweep.py` emits 0 proposals at this baseline.
- **B_STRUCT_OFFSET residual** — no clean cascading struct cause left; remainder
  is tooling/permuter (see live veins).

**LIVE (ranked by leverage):**
1. **jeff leaf-split fragment merges — Class 2 (+67, `eb4863cc`) + Class 4 (+35,
   `f03b9719`) BOTH LANDED; Class 1 & 3 NO-GO.** Class 2 = fall-through PDATA-less
   leaf merge (`merge_fallthrough_leaf_fragments`). Class 4 = post-`blr`/branch
   over-carve tail merge (`merge_branch_reached_overcarve_tails`, jeff `b50881e`),
   its exact complement — the ~85% root cause of the low-% (0.3-1%) named-fn mass
   (110 branch-proven groups, 193 tails; branch-target-proof P2′ is load-bearing).
   Census (`~/tmp/jeffc13/design.md`) settled the rest: **Class 3 NO-GO** (pop 0,
   already solved by b1bc97c write-gate) and **Class 1 NO-GO** (64% false
   positives — 522 guard-c noreturn-`bl` would CORRUPT if extended; genuine ~33
   low-yield, cleanest already swept by Class 4). Convergence note: committed
   symbols.txt must be the FIXED POINT (Class-4 climbs 15,342→15,354 over re-splits
   via symbols.txt feedback; byte-stable d12af934). Remaining jeff work: none
   high-value — the ~15% genuine-divergence remainder (OnSetMode-class) is a
   body-port lane, NOT jeff. Handoffs: `docs/plans/jeff-leaf-split-fix-status.md`,
   `~/tmp/jeffc13/design.md`, `~/tmp/lowpct-diag/verdict.md`.
2. **BinStreamRev base-at-0 inheritance** — LEAD A LANDED (`2b7b557a`, +19),
   LEAD B wave 1 LANDED (`677fc117`, +16). Per-TU `static unsigned short sXxxRev`
   (retail's `lbl_82CC*` halfword) set once at the outer `Load` entry from the
   popped rev, replacing `d.rev`/`d.altRev` reads in inner sub-Loads. **Width is
   per-TU** — `unsigned short`/`lhz`+`cmpwi` (most) vs signed `int`/`lwz`+`cmpwi`
   (Morph). Wave 2 selected by `scripts/harvest/leadb_signature_scan.py` (target
   `lhz lbl_82CC*` vs our `lwz 0xC(rN)` member). NOTE from wave 1: LEAD-A residual
   backlog was optimistic — several entries walled by non-rev causes (element
   ctor, Symbol default-construct, struct-size); and some sub-loaders inherit
   their rev **cross-TU** from a different outer Load (SampleZone←`MidiInstrument
   ::Load`) — wave 2 must set the static in the owner TU. Codegen fingerprint
   (reusable): extra `lwz <off>` where retail does `mr r3,&this` =
   composition-vs-inheritance mismatch on any Milo wrapper type.
3. **Re-anchor drifted whole-unit-0% units** (REPLACES the refuted "969
   unported" vein — wave 7 proved game .cpp is all wired+ported; the ~6,113
   absent symbols are 95% out-of-scope XDK/audio). Real vein: fully-ported
   units reading whole-unit-0% because their target_symbol_map addresses
   drifted base→TU5 (EndingBonus map@0x822C1xxx vs split@0x822D39xx). The
   wave-6 sweep gated these out ("unit has ≥1 matched fn"). Being tested; if
   it lands, relax `scripts/harvest/tu5_stale_anchor_sweep.py`'s gate. Also:
   Matchmaker/Session misanchors, Ham↔Band DC3-leak naming.
4. **Real TU5 gameplay RE** — Game::Poll-class (divergence sketches in
   `~/tmp/p5w6/cbodies-notes.md`: cached demo bool@Game+0x30, movie-sync block
   calling fn_826C91C8), Matchmaker genuine rewrites.
5. **Permuter sweeps** — CharHair/GamePanel regalloc + 11/14 of the wave-5
   permuter queue still walled.
6. **Unicorn behavioral vein** — object_memory/call_arg classes actionable on
   sub-100 fns (calibrated wave 6; modest yield, best decomp-synth training
   signal). Structural leads: BandCamShot layout, XboxSessionJob base size.

## Documented walls (source-side unfixable — need tooling/permuter)

CharClip rbtree 0x18/0x1c ODR coupling; MusicLibrary::PlaySetlist GameMode
vbase vtordisp; CharClipSet fn_823D0AFC anon inner-class +4; BandDirector::
SyncProperty local-static (−38); Character::Lod / HamNavProvider::NavItem
shrink mirages; LightPreset EnvironmentEntry-vs-SpotlightEntry target-map
mispair (needs re-anchor not header).

## Method (stable across waves)

Workflow crack→review, both Opus in isolated worktrees; coordinator harvests the
**review** worktree's clean diff (never the JSON — round-trips corrupt),
path-limited commit, whole-binary A/B on main vs a snapshot, 0-regression gate.
Map edits need `touch config/45410914/config.yml` before each A/B leg (renamer
re-split trap). Reviewers re-baseline against live main (concurrent-session
drift). Training rows to `~/tmp/grind_runs/` → B2 corpus.
