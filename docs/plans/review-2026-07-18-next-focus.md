# Review 2026-07-18 — next best areas of focus

Post-mega-run review (main @ `3c3f9113` = **17,445 strict / 69,202 fns**, session
+2,081, zero real regressions). Three read-only Opus scouts sized the three
candidate value pools empirically against `build/45410914/report.json`
(strict = `match_percent_normalized == 100.0` exactly). Scout raw reports were
written to `~/tmp/review_{99band,mapped0,recarve}_2026-07-18.md`; this doc is
the durable synthesis.

## Pool census (report.json 2026-07-18)

| pool | size | verdict |
|---|---|---|
| ≥99 fuzzy band | 1,263 fns | **spent** — ~80% funclet mirage, rest walls; est. 20-30 fixable |
| 95–99 / 90–95 / 75–90 | 203 / 319 / 242 | mostly regalloc/permuter-class (permuter BANNED) |
| named-unit 0%, mapped name | 1,051 fns / 161 KB | **518 real bodies; ~250-290 oracle-backed ports — TOP PLAY** |
| named-unit 0%, anonymous fn_ | 6,304 fns | drained lane-B residue + ICF walls — do NOT re-crack |
| auto_* scatter 0x8220–0x8284 | 2,870 blobs / 31.6k fns / 5.24 MB | **recarve/attribution opportunity — ceiling raiser** |
| auto_03_828378B0 (XDK mega-blob) | 5,130 fns / 2.49 MB | oracle-less, LOW priority |

## Rank 1 — body-port campaign on the mapped-but-0% pool

The 1,051 mapped-name 0% functions decompose as: **518 real bodies** + 461 STL
template instantiations + 62 deleting dtors + 10 stream glue (the 533
compiler-generated ones materialize/ICF-fold as a *side effect* of porting the
real bodies — never chase directly). Live-objdiff sampling shows the dominant
class is **UNIMPLEMENTED stub** (symbol entirely absent from our obj, all-insert)
— pure port targets, not divergence grinds. Oracle coverage 321/518 (DC3 290,
rb3-Wii 212) → honest **~250-290 cheap-portable bodies / ~37 KB**.

**First wave (dense × DC3/soundtouch-backed):**

| unit | oracle/real | notes |
|---|---|---|
| DirLoader | 17/18 | MakeFileList, Object::InitObject/HandleProperty/HandleType |
| Debug | 14/14 | SystemConfig overloads, SupportedLanguages |
| MemHeap | 13/14 | MemFree/MemResizeElem/MemPrint |
| Console | 11/12 | span-tail = `RndMultiMesh::*` |
| Env_NG | 10/11 | NgRnd::UpdateOverlay (1052B), SetShadowMap, PreInit |
| MeshAnim | 9/10 | RndMultiMesh + RndShaderMgr |
| TDStretch | 8/8 | soundtouch RateTransposer, clean library port |
| MidiSynth | 7/7 | span-tail WorldDir::PropSync + RingBuffer |

Runners-up: DataPointMgr (rb3-Wii, game/net), rnddx9/CubeTex, BandCamShot,
EventTrigger, Anim, DataNode, PropSync, Rnd_Xbox, CrowdAudio(InlineHelp),
VocalPlayer.

**Method notes:** port by the **demangled owning class**, not the unit label
(unit attribution is `.text`-span tail — "Console" holds RndMultiMesh,
"MidiSynth" holds WorldDir). Trust live objdiff per fn — ~5-10% of the ≤16B
entries are already 100% (stale report/ICF pairing). Skip the ~197 oracle-poor:
System/LEAPCORE (32, no oracle), Compress/XGRAPHICS (10), Xbox voice, FFT
VMX128 (DC3's FFT unit is only 23%), rtti/osfinfo CRT.

**Bonus cascade:** each port materializes STL instantiations from the 461-pool
and flips sibling EH funclets — the 2026-07-16 fixwave's +60 was mostly this
cascade class, and this pool refills it.

## Rank 2 — recarve/attribute the 5 warm mid-address auto blobs

Everything probed in 0x8220–0x8284 resolves to **game/engine classes** (not
XDK/Quazal), and the owning TUs are **already wired + pinned** — these blobs are
orphaned multi-TU scatter (attribution wall, not codegen). Ghidra named-density
is ~0 here (identification never reached these regions), so this is Stage B/C
recarve per `docs/plans/recarve-pipeline.md`: carve per-TU contiguous runs,
attribute names via 2-of-3 evidence (fingerprint + oracle order + Ghidra),
extend/add map entries so the renamer pairs.

**Ranked targets** (real = fns ≥64B after funclet screen):

1. **0x825F71A0 Accomplishment** — 91 real, **32 already mapped** (warmest seed)
2. **0x826DD570 Skills/Campaign/SongSort** — 99 real, 17 mapped; SongSort vein landed @528c51c7
3. **0x82796C48 TrackWatcherImpl/BeatMatchController** — 60 real; `docs/decomp/handoff/w3-port-beatmatcher-handoff.md` exists
4. **0x8234FCEC DataArray/ObjectDir** — 94 real (biggest), cold map (3) = more identity work; wide ripple
5. **0x82560660 UI-message run** — 64 real (ButtonDownMsg/UIScreenChangeMsg/UIComponent)

**Traps (skip/defer):** 0x8264519C (308 fns, **4** real — funclet farm),
0x82500688 (258 fns, 6 real — stub farm), 0x8282A230 (**CRT runtime** despite
232 map entries — the high map count is atexit/strchr/savefpr), 0x822FC4F8
(stlport tiny). Prologue funclet screen (`~/tmp/recarve/funclet_vas.json`,
16,821 VAs) is MANDATORY before scoring; screen ICF-fold victims with
`lookup_merged_symbol`.

**Strategic side effect:** carving/attributing these regions generates the
200+ new names needed to revive identification round-4 (calibration collapsed
at ~0.031/name — see `project_correlator_global_sweep_2026-07-18`).

## Rank 3 — REJECTED: ≥99 fixwave round 2

Band = 1,263: **1,008 anonymous** (993 ≤48B EH-funclet mirage — flip free with
parents only), 110 STL walls, 43 `??_G/??_E` pairing thunks, **102 real bodies**.
Diagnose-sample of 8 real bodies: **0 clean source wins** (regalloc f29↔f30 /
r18↔r20, spill staircases, frame pairing; zero missing-field/wrong-constant
wins). The cascade veins that made 2026-07-16's +60 (PROPSYNC, struct→funclet
flips) are documented drained; band-top explicitly closed by the wave-9 crack
campaign. **Estimated total fixable ≈ 20-30, no cascade — poor EV.**
Residual option: ONE scoped agent probing shared levers in the Geo (4 math),
Mesh (4 vertex), BandCamShot (4 accessor) sibling clusters.

## Banked singles (cheap, unordered)

- DxRnd::UpdateScalerParams (0x82739948) body-port — paired at 0% post-vtable-fix
- BandCharacter −4 container compaction in [mDircuts, unk734] (cr6 lead)
- ByteCode 0x825ad768 tie-break (parked in `scripts/harvest/vtable_unpinned_review.json`)
- 5 unpinned BandList names in the same review bucket

## Do-NOT-re-hunt census (unchanged)

Permuter (user directive), lane-B near-pair residue, identification round-4
(until 200+ new names), A_TOOLING ICF mirage, pad-probe deferred struct walls,
local-static mechanical wave, BandSwatch walled 62, 99-band top.
