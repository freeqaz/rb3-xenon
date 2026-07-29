# RB3-360 vs rb3-Wii vs DC3 — oracle coverage map (Lane BB, 2026-07-29)

**Question asked:** *"Do we know what functions are rb3-xenon specific that aren't on the Wii version?"*

**Answer:** Before today, **no** — nobody had measured it (see §7 prior art). Now we
have, and the headline is a **negative result that is more useful than the positive
one would have been**:

> **RB3-360 game code that is genuinely exclusive to the 360 SKU *and* has no
> oracle in either reference tree is a rounding error — 1 pinned unit, 34
> functions, 5,128 bytes.** The 360-vs-Wii delta that *does* exist is almost
> entirely the **platform layer** (`*_Xbox.cpp`, `rnddx9`, `synth_xbox`), and that
> layer has a **full DC3 oracle**. There is no large hidden body of
> reverse-engineer-from-scratch RB3-360 game code.
>
> The actionable frontier is the **opposite** of what the question implies: 143
> Wii TUs (2,505 Wii functions, 0.72 MB) that **do** have a Wii oracle and whose
> 360 counterpart we simply have not located yet.

---

## 1. The four-cell matrix

Universe: `build/45410914/report.json` (Jul 27) — 69,378 functions / 10.580 MB code
/ 3,967 units. Join key: canonical TU identity `<module>/<subdir>/<Stem>`,
case-folded, with a stem-only fallback.

Oracles are **binary censuses, not decompiled source** — this is the key
methodological choice (§5):
* **Wii** — `../rb3/orig/SZBE69_B8/files/band_r_wii.map` (CodeWarrior link map).
  Complete census of the Wii SKU: 42,599 `.text` symbols, **1,358 Harmonix TUs**.
* **DC3** — `../dc3-decomp/orig/373307D9/ham_xbox_r.map` (MSVC map, leaked PDB).
  2,149 objects.

| cell | meaning | units | fns | % of 69,378 | code | matched |
|---|---|---|---:|---:|---:|---:|
| **(c)** | **BOTH** oracles | 446 | 26,079 | 37.6 % | 3.181 MB | 20,566 |
| **(a)** | **DC3 only** — *no Wii counterpart* | 182 | 2,803 | 4.0 % | 0.360 MB | 1,678 |
| **(b)** | **Wii only** — no DC3 counterpart | 265 | 21,573 | 31.1 % | 2.407 MB | 17,116 |
| **(d)** | **NEITHER** — the no-oracle frontier | **1** | **34** | **0.05 %** | **0.005 MB** | 22 |
| **(u)** | UNATTRIBUTED — not pinned to any source file | 3,073 | 18,889 | 27.2 % | 4.627 MB | 0 |
| | **TOTAL** | 3,967 | 69,378 | 100 % | 10.580 MB | 39,382 |

By progress category (functions):

| category | (c) BOTH | (a) DC3-only | (b) Wii-only | (d) NEITHER | (u) UNATTRIB |
|---|---:|---:|---:|---:|---:|
| engine | 20,786 | 2,760 | 8,116 | 0 | 0 |
| game | 5,261 | 43 | 12,905 | **34** | 0 |
| network | 32 | 0 | 552 | 0 | 5 |
| *(unattributed)* | 0 | 0 | 0 | 0 | 18,884 |

### ★ Cell (d) is near-empty for a reason — and it is not the reason you want

**This is a selection-bias tautology and must not be read as "the 360 has no
exclusive code."** We pin a TU in `splits.txt` *because* an oracle told us where
to look. A unit therefore cannot be in cell (d) unless it arrived by some other
route. Cell (d) among *attributed* units measures our sourcing pipeline, not the
binary. The honest statement is:

> At **unit** granularity, oracle-driven attribution **structurally cannot**
> discover 360-exclusive code. Any real cell (d) must therefore live either inside
> the 27.2 % unattributed residue — **run down to ~0 in §7** — or *inside* shared
> TUs at function granularity (§6), or be visible only in the binaries themselves
> (§3). All three were checked.

The one genuine cell-(d) unit is nonetheless instructive — see §2.4.

---

## 2. What is actually in the "no Wii counterpart" cells

### 2.1 Cell (a) — 2,803 fns / 0.360 MB, present in RB3-360, absent from Wii

| subsystem | units | fns | bytes | matched |
|---|---:|---:|---:|---:|
| `system/hamobj` | 39 | 697 | 73,776 | 335 |
| `system/rndobj` | 19 | 414 | 67,984 | 272 |
| `system/synth_xbox` | 20 | 270 | 47,180 | 179 |
| `system/rnddx9` | 3 | 268 | 33,960 | 165 |
| `system/os` | 11 | 249 | 28,428 | 188 |
| `system/utl` | 11 | 241 | 26,632 | 175 |
| `system/gesture` | 18 | 167 | 18,420 | 85 |
| `system/flow` | 25 | 155 | 14,992 | 58 |
| `system/world` | 7 | 80 | 14,440 | 56 |
| `system/meta` | 4 | 61 | 6,688 | 45 |
| `system/net` | 10 | 60 | 8,964 | 41 |
| `system/synth` | 8 | 47 | 8,436 | 22 |
| `system/char`, `Memory_Xbox`, `keygen_xbox`, `xdk/LIBCMT`, `movie`, `moviebink` | 7 | 95 | 10,268 | 57 |

Two distinct populations, and they matter very differently:

1. **360 platform layer** (`rnddx9` 268, `synth_xbox` 270, `Memory_Xbox`,
   `keygen_xbox`, `xdk/LIBCMT`, the 15 `*_Xbox.cpp` files) — **≈600 fns**.
   360-exclusive *by construction*. **Not a frontier: DC3 supplies all of it.**
2. **Engine subsystems the Wii SKU does not have at all** (`hamobj` 697,
   `gesture` 167, `flow` 155, `net` 60, plus per-file gaps in `rndobj`/`os`/`utl`)
   — **≈1,300 fns**. Also DC3-covered.

**Verified these are really in RB3-360, not phantom pins.** `hamobj` has 381
byte-exact matched functions, 43 of them >128 bytes; `gesture` 85 matched (7
>128 B); `flow` 58 matched (5 >128 B). Byte-exact agreement at >128 bytes does
not happen by coincidence. *Caveat:* memory topic `project_offset_drift_sweep`
records a "FlowIf phantom-pin — retail has NO Flow system". `flow`'s evidence is
the weakest of the three (only 5 matched fns >128 B); treat `system/flow` as
unresolved rather than confirmed.

### 2.2 The mirror image — Wii-exclusive code

The reverse test is clean and symmetric. Of the 188 non-network Wii TUs with no
counterpart anywhere in our tree or DC3, **47 TUs / 1,292 fns / 0.245 MB** are the
**Wii platform layer**: `contentmgr_wii` (136 fns), `commercemgr_wii` (93),
`platformmgr_wii` (91), `synth_wii` (81), `wiiprofilemgr` (74),
`wiifriendsprovider` (74), `joypad_wii` (52), `mic_wii` (52), `homemenu_wii` (46),
`memcardmgr_wii` (46), `memcard_wii` (38), `voicewii` (33), `usbwii` (33),
`wiiprofilepanel` (42), `bufstreamnand` (22), plus `synthwii/`, `rndwii/`.

**This is the answer's real shape: the two SKUs differ by a swapped platform
layer of ~600 fns (360) vs ~1,292 fns (Wii), and essentially nothing else.**

### 2.3 Hypotheses TESTED AND FALSIFIED

Candidate "360-only subsystem" hypotheses from the brief, checked against the Wii
binary's own TU census:

| hypothesis | verdict | evidence |
|---|---|---|
| 360-only DLC / marketplace / store | **FALSIFIED** | Wii has **24** `*Store*` TUs: `StoreMainPanel`, `StoreOffer`, `StorePanel`, `SyncStore`, `AssetStore`, `PersistentStoreClient`, `StorePreviewMgr`, `BandStoreOffer`, … A full store subsystem exists on Wii. |
| 360-only online / party / multiplayer | **FALSIFIED** | Wii ships the same Quazal NetZ middleware — **433 `network/` TUs, 5,062 fns, 1.086 MB**. |
| 360 rendering (`rnddx9`) is a no-oracle frontier | **FALSIFIED** | DC3 covers it (cell (a), not (d)); consistent with `docs/plans/engine-reuse-and-asset-rendering.md`. |
| 360 audio path (`synth_xbox`) is 360-exclusive | **PARTLY FALSIFIED** | 15 of ~35 `synth_xbox` files match a Wii TU under a *different directory* — `FxSendChorus/Compress/Delay/Distortion/EQ/Flanger/Wah/Reverb/Synapse/PitchShift`→`system/synth/`, `PitchDetector`→`system/dsp/`, SoundTouch→`system/synthwii/soundtouch/`. Only the genuinely 360-specific files (`SampleInst360`, `StreamReceiver360`, `SynapseAPO`, `IPP_basicmath_xbox`, `HeadsetXferEffect`, `FftIpp`) are exclusive. |
| Xbox-specific input / storage / profile | **CONFIRMED but not a frontier** | `Joypad_Xbox`, `Memcard_Xbox`, `MemcardMgr_Xbox`, `ContentMgr_Xbox`, `System_Xbox`, `VirtualKeyboard_Xbox`, `MapFile_Xbox` — all real, all DC3-covered. |

**Corollary — directory prefix is NOT evidence of exclusivity.** `native_scope_map.py`
classifies "360-ONLY" by directory prefix (`rnddx9|Dx*|xdk|Quazal|synth_xbox|codecs|*_Xbox`)
and reports 116 units / 2,174 fns. Our measurement shows **25 of 66** units under
those prefixes have a real Wii counterpart under a different directory. That
heuristic **over-counts 360-exclusivity by roughly a third** and should not be
cited as an exclusivity measure.

### 2.4 The single genuine cell-(d) unit

`src/band3/meta_band/MusicLibraryStore.cpp` — 34 fns, 5,128 bytes, 22 matched.
Wii has `MusicLibrary.o` and `MusicLibraryNetSetlists.o` but **no
`MusicLibraryStore.o`**, and DC3 has no `MusicLibrary*` at all. Given Wii *does*
have 24 other Store TUs, this is a genuine 360-side refactor of the
library/store bridge, not a missing feature. It is the only one.

---

## 3. The instrument that beats attribution: RTTI class names

Unit-level attribution is selection-biased (§1). We need an instrument that reads
the **binaries** and is independent of how much we have decompiled. Two were run.

### 3.1 Raw string-pool diff — mostly useless, quantified

Pools (min-len 5, after a mandatory text-plausibility gate; 13,261 of 19,415 naive
"360-only" hits at n=5 were PPC opcode bytes forming accidental ASCII, e.g. `8}k8P`):

| | raw unique | gated |
|---|---:|---:|
| RB3-360 (`orig/45410914/band.exe`, the decompressed PE) | 42,584 | 16,370 |
| RB3-Wii (`main.dol`) | 31,727 | 21,321 |
| DC3-360 | 54,336 | 20,591 |

both **9,982** · 360-only **6,388** · Wii-only **11,339**.

**Measured false-positive rate for "360-only string ⇒ 360-only feature": >80 %.**
Derived three ways: (a) on the reverse axis, **71.2 %** — 111 of 156 "Wii-only"
class tokens exist as files in `rb3-xenon/src` or `dc3-decomp/src`; (b) a 400-sample
forward audit found 34.2 % have a Wii counterpart under looser matching; (c) 62 %
of 360-only strings also appear in DC3-360, i.e. they are platform, not RB3.
**Do not cite single-string evidence.**

Two confounds this exposed, both important:
* **Wii-only is *larger* than 360-only** (11,339 vs 6,388) — because `../rb3` is a
  **DEV** build retaining `MILO_ASSERT` paths (2,020 dev-assert strings vs 327).
  That asymmetry is build configuration, not features.
* **Quazal NetZ is on BOTH SKUs** (Wii: `Quazal::SessionDiscoveryProtocol`,
  `DNetZ`, `NetSession_RV`). 195 vs 516 strings. The "Quazal is 360-only"
  hypothesis is **false**.

### 3.2 ★ RTTI class names — the good instrument

RB3-360 ships `/GR`, so every polymorphic class leaves a `.?AVFoo@@` type
descriptor. This enumerates the **complete** polymorphic class inventory of the
360 SKU — **coverage-independent**, immune to mangling differences, and immune to
our own decomp progress. **1,207 classes.**

| | classes |
|---|---:|
| present in the Wii DOL | 1,059 |
| **absent from Wii** | **143** |
| ↳ of which also in DC3-360 ⇒ 360 **platform layer** (HARD SKIP) | 106 |
| ↳ **absent from Wii AND DC3 = candidate RB3-360-exclusive** | **37** |
| ↳ surviving a 4-test filter (spelling, token co-occurrence, DC3 presence, `_Wii`/`_RV` rename twins) | **~18–19** |

The 106 platform-layer classes are exactly what CLAUDE.md predicts: `Dx*`
(`DxRnd`/`DxTex`/`DxShader`), `Ng*` (`NgRnd`/`NgFur`/`NgPostProc`), 20× `RndShader*`,
`FxSend*360`, XAPO/XMA, `MemcardXbox`/`CacheXbox`/`MicXbox`.

Measured FP rate on this axis: **38–51 %** before the filter. Hard FPs found:
`AssetStore`→Wii `assetStore`, `AssetOffer`→`has_asset_offer`,
`PremiumAssetProvider`→`AssetProvider`, `RegisterArbitrationJob`→Quazal
`CallContextRegister`. ~10 more are **platform renames** with confirmed `_Wii`/`_RV`
twins: `XboxEntityUploader`↔`EntityUploader_Wii`, `XSessionSearcher`↔`SessionSearcher_RV`,
`XboxServer`↔`Server_Wii`, `XSessionData`↔`NetSession_RV`, `XboxJob`↔`Jobs_Wii`.

### 3.3 ★★ The answer: Rock Band Network

The ~18–19 survivors cluster into **one coherent subsystem**:

* **UGCNet (12 classes)** — `UGCNet`, `UGCNetDiscovery`, `UGCNetDiscovery_Server`,
  `UGCNetTCPListenSocket`, `UGCNetTCPSessionSocket`, `UGCNetUDPSocket`,
  `UGCNetResource`, …
* **Audition / RBN (4)** — `AuditionMgr`, `AuditionSessionBuilder`,
  `AuditionSessionPanel`, `AutoplayAuditionUser`
* **Content validation (2)** — `MidiValidator`, `OggValidatorFileSource`
* (+ `XMAReader`, `Label3d`)

Corroborating string evidence: `UGCNet: connection failed to init (%d)`, 15×
`kUgcNet*` state names, `ugc_audition_temp_song`, `rbn/audition/fail`,
`./ui/audition/gen/audition.dtb`, `Validating Mogg (%s)...`.

**This is the Rock Band Network author-audition pipeline** — the PC-authoring →
console-preview link RBN creators used. RBN shipped on Xbox 360 only; the Wii SKU
never had it. Wii's sole UGC trace is `UGCPurchasePanel` (consume-only).

This is the genuine cell (d): **~18 polymorphic classes.**

**Independently re-verified (three ways, by a different agent than the one that
found it):**
* `strings band.exe | grep -iE 'ugcnet|audition'` → rich hits (`UGCNet: mem fail 3`,
  `kUgcNetDisconnected`, `audition_mgr`, `can_enter_audition`, `enter_audition`).
* `strings main.dol | grep -icE 'ugcnet|audition'` → **0**. Wii's UGC surface is
  purchase/consume only (`UGCPurchasePanel`, `allow_ugc`, `is_ugc`,
  `/album_art/UGC_%d_keep.png`) — Wii can *play* RBN songs, not *author/audition* them.
* `grep -icE 'audition|ugcnet' band_r_wii.map` → **0** TUs.
* No `UGCNet*` / `AuditionMgr*` / `MidiValidator*` source file exists in `../rb3/src`,
  `../dc3-decomp/src`, **or our own `src/`**.

### 3.4 ★ We could NOT size it — two location claims retracted

The string-reference clustering proposed two TU spans. **Both are wrong**, and
checking them against `scope_map.json` is what caught it:
* `0x8266F3B0–0x8266F800` — 13 fns, all `pinned:src/band3/meta_band/Leaderboard.cpp`.
* `0x82B8C990–0x82B8D630` — 31 fns, all **`thirdparty.json:libcurl`**.

Neither is RBN code. Cause: `fingerprints.json` carries strings for only 8,496 of
67,285 functions (12.6 %), so its clustering is far below the noise floor.

A second attempt — MSVC RTTI type-descriptor → Complete Object Locator → vtable
chase over the PE (`orig/45410914/band.exe`, imagebase `0x82000000`,
`.text` = `0x82270000`+10,341,948 B) — resolved 10 RBN classes to **75 distinct
virtual methods**. But tiering those against `scope_map` shows the instrument is
measuring the wrong thing: 15 are already `pinned` to `BandUser.cpp` (8) and
`UIStats.cpp` (6), i.e. **inherited base-class implementations**
(`AutoplayAuditionUser` derives from `BandUser`), 14 are `spatial:after-game`,
and only **3** land in `unknown/residual`.

> **Honest limit: the RBN subsystem's existence is proven, its `.text` footprint
> is NOT measured.** A derived class's vtable is dominated by base-class slots, so
> vtable chasing cannot size class-owned code. Order of magnitude is plausibly
> tens-to-low-hundreds of functions, but that is an estimate, not a measurement.
> Data: `~/tmp/laneBB/rbn_vtables.json`.

---

## 4. The reverse direction — where the actionable work actually is

Because the Wii map is a **complete** census of the Wii SKU, the reverse question
("which Wii TUs have we not located in the 360 binary?") is **not** selection-biased.
It is the more useful measurement.

RB3-Wii Harmonix `.text` census (`band_r_wii.map`): **38,127 fns / 10.211 MB / 1,358 TUs**
— `system` 21,737 fns / 6.267 MB / 621 TUs; `band3` 11,328 / 2.858 MB / 304 TUs;
`network` (Quazal) 5,062 / 1.086 MB / 433 TUs. (Plus 4,472 fns / 1.211 MB in 65 Wii SDK libs.)

| Wii TU status vs our 360 work | system | band3 | network |
|---|---:|---:|---:|
| **PINNED** in our splits | 483 TUs / 19,163 fns | 209 / 9,708 | 24 / 576 |
| in our tree but unpinned | 35 / 319 | 6 / 47 | 3 / 16 |
| exists only in DC3 | 1 / 2 | 3 / 29 | 7 / 119 |
| **ABSENT everywhere** | 102 / 2,253 | 86 / 1,544 | 399 / 4,351 |

Dropping `network` (Quazal — low value per user directive), the 188 ABSENT
non-network Wii TUs split cleanly:

* **47 TUs / 1,292 fns / 0.245 MB — Wii platform layer.** Correctly absent. See §2.2.
* **141 TUs / 2,505 fns / 0.722 MB — `REAL_UNLOCATED`.** Real RB3 engine/game code
  that **has a Wii oracle** and whose 360 counterpart we have not located.

`REAL_UNLOCATED` by directory (Wii fns): `band3/meta_band` 707, `system/bandobj` 490,
`band3/game` 339, `band3/tour` 228, `system/meta` 142, `system/beatmatch` 129,
`system/speex` 82, `system/ui` 80, `system/utl` 78, `system/track` 53, `system/math` 44,
`band3/net_band` 40, `system/char` 34, `system/os` 20, `band3/bandtrack` 19.

Largest individual targets (Wii fn count): `storepackedmetadata` 124,
`charkeyhandmidi` 82, `chordshapegenerator` 73, `unisonicon` 66, `charsync` 57,
`bandbutton` 55, `reviewdisplay` 53, `bandusermgr` 53, `lockstepmgr` 51,
`trackwidgetimp` 49, `uiproxy` 47, `patchrenderer` 46, `tourdesc` 43,
`realguitargemplayer` 40, `practicesectionprovider` 40, `appscoredisplay` 37,
`charprovider` 35, `bandperformer` 34.

**This list — not cell (d) — is the thing that changes what we decompile next.**
Full data: `~/tmp/laneBB/wii_reverse.json`.

---

## 5. Methodology: "no counterpart" vs "we failed to find it"

This is the load-bearing question and it is why the whole study uses **binary
censuses** (`band_r_wii.map`, `ham_xbox_r.map`) rather than the decompiled source
trees. Grepping `../rb3/src` conflates *"the Wii SKU lacks it"* with *"the Wii
decomp hasn't got there yet"*. The Wii **map** has no such gap: it names all
42,599 `.text` symbols in the shipped Wii binary, decompiled or not.

### 5.1 Two-sided control on the unit-level join

Both control rules were declared **a priori** from naming convention, not from the
join's answers:

| control | rule | n | result |
|---|---|---:|---|
| **positive** | same relative path exists under `../rb3/src` | 673 | TP 673, FN 0 → **recall 1.0000**, **false-negative rate 0.0000** |
| **negative** | filename `*_Xbox.cpp` / `*360.cpp` / under `src/xdk/` | 22 | TN 22, FP 0 → **specificity 1.0000** |

### 5.2 ★ The control CAN fail — verified, not assumed

A control containing only true positives measures nothing. Two liveness proofs:

1. **The same machinery matched 22/22 of the negative-control units to DC3.** The
   matcher was live on exactly those inputs; a "no Wii counterpart" answer was a
   real decision, not a silent failure.
2. **On a broader, deliberately looser negative set** (any path matching
   `synth_xbox|rnddx9|/xdk/|_Xbox`), the join returned **25 Wii matches out of 66** —
   so it demonstrably *does* return positives on "xbox-looking" paths.

### 5.3 ★ Adjudicating those 25 — the label was wrong, not the join

Every one of the 25 was inspected. **They are correct matches against a bad label:**
`synth_xbox/FxSendChorus|Compress|Delay|Distortion|EQ|Flanger|Wah|Reverb|Synapse|PitchShift`
→ Wii `system/synth/…`; `synth_xbox/PitchDetector` → Wii `system/dsp/pitchdetector`;
`synth_xbox/soundtouch/*` → Wii `system/synthwii/soundtouch/*` (third-party, both SKUs);
`synth_xbox/{Synth,SynthSample,FxSend,Mic}` → Wii `system/synth/…`.
The only debatable ones are `rnddx9/{CubeTex,Rnd,Movie}` → `rndobj/{cubetex,rnd}`,
`movie/movie`: the **class** exists on both, the **implementation** does not.

Consequences:
* The a-priori negative rule (platform *suffix*) is sound; the **directory-prefix**
  rule is not. See the `native_scope_map.py` over-count in §2.3.
* "Counterpart" is granularity-dependent. We report it at **TU identity**. A
  DX9-vs-GX reimplementation of the same class counts as *having* a counterpart
  here, which is the right call for oracle purposes (the Wii source tells you the
  interface and the logic) but the wrong call if you are asking "can I copy the body".

### 5.4 Known residual weaknesses

* **Stem-fallback ambiguity.** 43 of 711 Wii assignments used the stem-only
  fallback (`W:stem`/`W:srcstem`), which picks the lexicographically first
  candidate when a stem is non-unique. Affects <1 % of the matrix.
* **Unit granularity misses intra-TU exclusivity.** A 360 TU that exists on Wii can
  still contain 360-only methods. That is the function-level question (§4 of the
  brief); see §8 for its status.
* **1,405 Wii map lines unparsed** (of ~45.8k in the `.text` block) and 493 Wii TUs
  outside the `band3_wii` path convention (SDK/libs, 4,472 fns) — excluded by design.
* **`system/flow` is unresolved** (§2.1) — 58 matched fns but only 5 >128 B, against
  a memory note claiming retail has no Flow system.

---

## 6. Function granularity — inside shared TUs

Unit granularity cannot see a 360-only method on a class that exists on both SKUs.
So the join was pushed to function level, using a cheap normalized key rather than
full demangling: MSVC `?Method@Class@@…` and MWCC `Method__5ClassFv` both reduce to
`(class, method)`; `??0`/`__ct__` → ctor, `??1`/`__dt__` → dtor.

**Mangling is not the limiting factor.** Parse failure: MWCC **0.70 %** (256 of
36,770), MSVC **0.00 %** (0 of 20,418 named). The limiting factor is that
**59.6 % of RB3-360 functions have no name at all** (30,071 anonymous `fn_` of
50,489 in pinned units).

Forward join over 708 paired units / 690 paired Wii TUs / 47,629 fns:

| verdict | fns | % | bytes |
|---|---:|---:|---:|
| key present in the paired Wii TU | 12,944 | 27.2 % | 2,395 KB |
| present elsewhere in the Wii map | 1,887 | 4.0 % | 204 KB |
| **no Wii counterpart anywhere (non-STL)** | **2,025** | **4.3 %** | **233 KB** |
| no counterpart, STL instantiation | 2,098 | 4.4 % | 279 KB |
| unparseable (anonymous `fn_`) | 28,675 | 60.2 % | 2,342 KB |

No-counterpart rate among *named* functions: **engine 12.9 %, game 6.4 %, network
6.2 %** — platform divergence concentrates in the engine, as CLAUDE.md predicts.

### 6.1 Confusion matrix (independent oracle, and a mutation test)

Positive control: 4,166 `src/band3/` functions in `W:exact` units where an
**independent** oracle (a literal `Class::Method` token in the rb3-Wii *source*
tree, 18,280 tokens) confirms a Wii counterpart. Negative control: 531 fns across
50 classes whose *name* is provably Xbox/D3D/XAudio-only (`Dx*`, `FxSend*360`,
`*Xbox`, `CXAPOBase`, `Synth360`).

```
                       predicted HAS-Wii   predicted NO-Wii
truth HAS-Wii (4166)       TP = 4123           FN =  43
truth NO-Wii  ( 531)       FP =    0           TN = 531

recall 0.990   specificity 1.000   precision 1.000   accuracy 0.991
false-negative rate 1.0%   (per-TU recall, stricter: 0.956)
```

**★ Mutation test proves the control can fail.** Degrading the join to
method-name-only (dropping the class) collapses specificity **1.000 → 0.181**
(435 of 531 become false positives). The negative control has teeth; the
instrument passes it.

*(A second, unit-provable negative control — `src/xdk/`, `src/system/synth_xbox/`,
`*_Xbox.cpp`, N=755 — reads specificity 0.799. Those 152 "FPs" are the same bad
directory-prefix label diagnosed in §5.3, not join errors. Trust the first control.)*

### 6.2 ★ Most of the 2,025 is NOT exclusivity — it is codegen policy

The method-name histogram kills the naive reading:

* **19.9 % (404 fns) are macro-generated boilerplate MWCC never emits out-of-line.**
  `StaticClassName` appears **293×** on 360 vs **exactly 1** in the entire Wii map;
  `StaticByteCode` 23 vs **0**; `Type` 47, `ClassName` 29, `NewObject` 12. That is
  `OBJ_CLASSNAME`/`DECLARE_MESSAGE` codegen divergence, not a feature difference.
* **~28 % (564 fns) are ctors/dtors** of small/template classes MWCC inlined away
  (every `ObjList<T>::~ObjList`, etc.).
* The genuine remainder is the **already-known platform layer**: `Dx*` (`DxRnd`,
  `DxTex`, `DxCubeTex`, `DxLight`, `DxCam`, `DxMesh`, `DxShader*`, `DxParticleSys`),
  the `FxSend*360` family (~130 fns), `Synth360`, `MicXbox`/`MicManagerXbox`,
  XAudio2 `CXAPOBase`/`CSampleXAPOBase`, `CacheXbox`/`CacheMgrXbox`/`NetCacheMgrXbox`,
  `MemcardXbox`/`MCFileXbox`/`MCContainerXbox`, plus engine-version drift
  (`RndAmbientOcclusion`, `RndShaderMgr`, `ObjRefConcrete` — Wii has `ObjRef`).

> **Honest figure: ~1,000 fns / ~120 KB of real 360-only logic among *labelled*
> functions** — not 2,025 / 233 KB. And it is overwhelmingly the platform layer
> that DC3 already supplies.

### 6.3 What this method cannot do

* **The reverse-direction absolute numbers are worthless.** "15,136 Wii functions
  with no 360 counterpart (52.7 %)" is an artifact of the 59.6 % anonymous 360
  side. Only the *ranking* is informative, and it ranks by inlining asymmetry
  (`_Vector_impl` 793, `stlpmtx_std::make_pair` 740 — MWCC emits STL out-of-line
  where MSVC `/Ob2` inlines it).
* **★ Residual bias that must be carried forward:** the 360 "named" universe is
  **not ground truth** — it is `scripts/target_symbol_map.json`, names *transferred
  from the DC3 and rb3-Wii oracles*. Names transferred from rb3-Wii carry a
  guaranteed counterpart by construction. Function granularity removes the
  *unit*-level tautology but introduces a weaker naming-provenance one.
* **The 59.6 % anonymous remainder is entirely unmeasured, and that is exactly
  where undiscovered 360-exclusive code would live.** This method characterizes
  360-exclusivity *within the labelled 40 %*; it cannot bound it over the binary.

Data: `~/tmp/laneBB-fnjoin/{fnjoin,controls}.json`.

---

## 7. Reconciliation with `scope_map.json` — nested, not independent

`tools/scope_map.py` tiers all 68,747 functions and reports **`unknown` = 6,167 fns
/ 1.022 MB**. It was proposed that this might be an independent estimate of cell (d).
**It is not independent, and it is not cell (d).** The two are *nested*:

| | fns |
|---|---:|
| my (u) UNATTRIBUTED (report.json units with no `source_path`) | 18,889 |
| scope_map non-`pinned` provenance (8,084 spatial + 3,915 name + 6,167 residual + 584 json + 68 uid) | 18,818 |

Those are the **same set** (Δ71 = scope_map's dedup). scope_map then resolves
12,651 of them by **spatial adjacency** to a pinned unit and by **name**, leaving
6,167 `residual`. So `unknown` is a *refinement* of my (u), computed by a stricter
attribution pass over the identical population — corroboration of set identity,
**not** a second opinion on exclusivity.

Critically, **`unknown` measures "we have not attributed this", not "no oracle
exists"** — the same limitation as cell (d).

### 7.1 ★ The 1.022 MB, fully accounted for

Every one of the 6,167 functions was classified using Ghidra (port 8002, project
`default_tu5.xex-c5a170`), XEX section geometry, and a `lis`/`addi` → string
cross-reference sweep of `.text` (11,797 code→string edges). Totals reconcile
exactly to 6,167 fns / 1,022,268 B.

| class | fns | bytes | share |
|---|---:|---:|---:|
| **(iii) vendor / middleware** | 4,133 | 831,036 | **81.3 %** |
| **(ii) compiler-CRT boilerplate + XDK runtime** | 1,973 | 184,876 | **18.1 %** |
| **(i) ordinary unpinned Harmonix code** | 61 | 6,356 | 0.6 % |
| **(iv) genuinely RB3-360-exclusive game logic** | **~0** | **~0** | **0 %** |

Class (iii) breaks down as **Quazal NetZ/Rendez-Vous 603,596 B**, XDK D3D9/XGRAPHC
shader compiler 65,624 B, **BINK 65,292 B**, and zlib / libtomcrypt /
ogg-vorbis-theora / WMA / XMA 96,524 B. Class (ii) is `.text$yc`/`.text$yd`
34,496 B (1,324 `??__E`/`??__F` thunks) plus the LIBCMT/XAPILIB/XONLINE/XBOXKRNL
startup zone `0x8282A000–0x82860000`, 150,380 B.

Structural facts that made this decisive:
* `.text` = `0x82270000–0x82C4CE3C`; **`BINK` = `0x82C4D000–0x82C5D010`**. The
  141-fn / 65,552 B run is *byte-for-byte the BINK section* — RAD video, vendor.
  RB3 has **no** `.xidata` inside `.text`.
* `.data@0x82C64418` holds a **311-entry CRT initializer pointer table** whose
  targets all land in `0x82C3E7C8–0x82C43E20`. Combined with DC3's map
  (`.text$yc` = 388 `??__E`, `.text$yd` = 685 `??__F`, avg 27.8 B) this confirms the
  RB3 tail runs (avg 24–30 B, disassembling to `stw <vtbl>; lwz r3,4(r9); b <ctor>`)
  are static-init/atexit thunks.
* `0x82BBBB90` is **`setjmp`** (LIBCMT). The "169 fns in 780 bytes" was a scope_map
  artifact — overlapping records over the setjmp/longjmp family, not real functions.

Only **4 of 6,167** appear in `target_symbol_map.json`; they are simply anonymous.

> **Conclusion: the `unknown` tier is not a frontier.** 99.4 % is vendor, XDK/CRT,
> or MSVC boilerplate; 0.6 % (6.4 KB, 61 fns) is ordinary Harmonix code wedged
> inside already-pinned TUs (`CharIKHand`, `Font`, `BandProfile`,
> `AccomplishmentPanel`, `VocalPart`, `FxSendDistortion`, `Rnd_Xbox`, `UIPanel`).

### 7.2 Side finding: Quazal is 59 % of the residue and is *entirely unwired*

The single largest slice of `unknown` is **Quazal NetZ at 603 KB / 59 %**. It is
**not** 360-exclusive: extracting the 103 Quazal TU names from RB3-360's surviving
assert strings and checking each against the Wii DOL, **97 of 103 are present in
the Wii binary**. rb3-Wii's decomp already carries `src/network/` with 178 `.cpp`
(31 with real bodies). rb3-xenon has 16 `.cpp` there and **zero entries in
`objects.json`** — the whole channel is unwired.

The only true no-Wii-counterpart residue here is 6 Xbox-platform Quazal plugins
(`XboxLSP\LSPBackEndServices`, `JobLSPLogin`, `JobLSPLoginBypassSG`,
`XboxConnectivityTester`, `MD5`, `Core\StringConversion`) ≈ **9–15 KB** — Quazal
SDK code, not Harmonix logic.

*(Per user directive Quazal is low value; recorded as an observation, not a
recommendation.)*

---

## 8. Prior art — what existed before this document

Nobody had run this measurement. Every prior study runs the **forward** direction
(Wii/DC3 → 360 identification) and its residue conflates *"no counterpart exists"*
with *"the matcher failed"*.

| prior work | what it measured | why it is not this |
|---|---|---|
| `docs/plans/bindiff-vs-rb3wii.md` [HIST] | 9,301 RB3-360→Wii game matches over 703 TUs; 196 high-confidence | forward recall; its 57k complement was never claimed as exclusivity. Output `unified_id_rb3wii.json` is **TU0-era — addresses invalid on current main** |
| `../rb3/docs/decomp/xenon-hardening/scout-recall-levers.md` | ~64,742 unmatched Xenon fns; pipeline precision ~51.9 %, two hashers at 0.0 % | explicitly a recall/precision study, explicitly not exclusivity |
| `docs/plans/carve-pilot-2026-07-20.md` + `scripts/harvest/carve_pilot_classify.py` | the only true 3-way: 563 BinDiff hints bucketed by source availability; ~72 % (408/563) have no portable source | 563 hints only, not the binary |
| `scripts/native_scope_map.py` | "360-ONLY = 116 units / 2,174 fns / 358,800 B" | **directory-prefix heuristic** — §2.3/§5.3 show it over-counts by ~⅓ |
| `tools/gen_sysnet_port_worklist.py` | `dc3_cannot_provide` flag | right shape, but aimed at DC3-unreachability over Wii identities |
| `docs/plans/engine-reuse-and-asset-rendering.md` | hand-built rndobj 3-way feature matrix | qualitative; the doc itself says "re-derive with grep/ls" |

Never previously measured: any **whole-binary** partition into has-Wii-twin /
has-DC3-twin / neither; any measurement **separating true exclusivity from matcher
recall failure**; any 3-way comparison at source-file granularity across the tree.

---

## 9. Verdict — actionable or descriptive?

**Mostly DESCRIPTIVE, with one small concrete work item and one significant
roadmap correction. It does not open a new decomp frontier.**

**Descriptive (the honest bulk).** Four independent instruments agree:

| instrument | 360-exclusive game code found |
|---|---|
| unit-level TU join (§1) | 1 unit / 34 fns / 5 KB |
| RTTI class inventory (§3) | ~18 classes (RBN), footprint unmeasured |
| function-level `(class,method)` join (§6) | ~1,000 fns, but ~all platform layer |
| exhaustive audit of the `unknown` tier (§7) | **~0 fns / ~0 bytes** |

Cell (d) is a rounding error at unit granularity; the 1.022 MB residue that looked
like a candidate frontier is 99.4 % vendor/CRT/boilerplate; and the only *named*
360-exclusive subsystem recoverable from the binaries — Rock Band Network
audition/UGCNet/validation, ~18 classes — could not be localized in `.text` by any
instrument we ran (§3.4). **There is no large hidden body of RB3-360 game code
needing from-scratch reverse engineering.** This is a reassuring result: the
two-oracle strategy in CLAUDE.md is sound and the engine-reuse thesis holds.

**The one real work item.** RBN authoring (`UGCNet`, `AuditionMgr`,
`AuditionSessionPanel`, `AuditionSessionBuilder`, `AutoplayAuditionUser`,
`MidiValidator`, `OggValidatorFileSource`) has **no oracle anywhere** — not
rb3-Wii, not DC3, not our tree. If the native port ever needs it, it must be
reverse-engineered from scratch. **For the native-port goal it is almost certainly
skippable**: RBN's servers are long dead and the feature is an authoring-preview
path, not gameplay. Recommend explicitly marking it out of scope rather than
scheduling it.

**The roadmap correction — this is the part worth acting on.** The question's
premise is inverted. The gap is not 360-exclusive code without an oracle; it is
**141 Wii TUs / 2,505 Wii fns / 0.72 MB that DO have a full Wii oracle and whose
360 counterpart we have never located** (§4), concentrated in `band3/meta_band`
(707), `system/bandobj` (490), `band3/game` (339), `band3/tour` (228). Combined
with `REAL_UNLOCATED`'s top targets (`storepackedmetadata` 124, `charkeyhandmidi`
82, `chordshapegenerator` 73, `unisonicon` 66, `charsync` 57), that is a ready-made
worklist for the existing `gameport`/`splits`-bootstrap pipeline — oracle in hand,
only the `.text` span missing.

**Three corrections to retire from circulation:**
1. `native_scope_map.py`'s "360-ONLY 116 units / 2,174 fns" over-counts by ~⅓
   (directory prefix ≠ exclusivity — §5.3).
2. "Quazal/NetZ is 360-only" is **false** — the Wii ships 433 Quazal TUs / 5,062
   fns, and 97 of the 103 Quazal TUs named in RB3-360's assert strings are present
   in the Wii binary (§7.2).
3. scope_map's `unknown` tier is **not** a proxy for "no oracle exists" — it is
   99.4 % vendor/CRT/boilerplate (§7.1).

**Do not cite** raw string-pool differences (>80 % FP, §3.1) or the
reverse-direction function counts (§6.3) as exclusivity evidence.

---

## 10. Reproduction

```bash
cd /home/free/code/milohax/rb3-xenon

# 1. Unit-level 4-cell matrix (+ the reverse Wii census). ~15 s, read-only.
venv/bin/python scripts/harvest/oracle_coverage_matrix.py --reverse
venv/bin/python scripts/harvest/oracle_coverage_matrix.py --control   # recall/specificity
#   -> ~/tmp/laneBB/coverage_matrix.json   (per_unit cells, wii_tus, dc3_objs)
#   -> ~/tmp/laneBB/wii_reverse.json       (Wii TU -> PINNED/IN_TREE/DC3_ONLY/ABSENT)

# 2. The RTTI class inventory (the coverage-independent instrument).
#    RB3-360 ships /GR, so `.?AV<Class>@@` enumerates every polymorphic class.
strings -a orig/45410914/band.exe            | grep -oE '\.\?AV[A-Za-z0-9_]+@@' | sort -u   # 1207
strings -a ../rb3/orig/SZBE69_B8/sys/main.dol                                                # Wii side
strings -a ../dc3-decomp/orig/373307D9/default.xex                                           # DC3 side

# 3. The headline finding, verified in three commands.
strings -a orig/45410914/band.exe             | grep -iE 'ugcnet|audition'    # many hits
strings -a ../rb3/orig/SZBE69_B8/sys/main.dol | grep -icE 'ugcnet|audition'   # 0
grep -icE 'audition|ugcnet' ../rb3/orig/SZBE69_B8/files/band_r_wii.map        # 0

# 4. Wii binary census by module (38,127 Harmonix fns / 10.211 MB / 1,358 TUs).
#    Parses the `.text section layout` block of the CodeWarrior map.
venv/bin/python scripts/harvest/oracle_coverage_matrix.py --wii-census
```

**Do NOT** rebuild `config/45410914/scope_map.json` while other lanes are reading it.

### Inputs (none committed; all read-only)
| role | path |
|---|---|
| RB3-360 universe | `build/45410914/report.json` (Jul 27: 39,382/69,378) |
| RB3-360 binary | `orig/45410914/default.xex`; decompressed PE `orig/45410914/band.exe` |
| RB3-360 names | `scripts/target_symbol_map.json` (25,675 — a **pairing aid**, not an oracle) |
| RB3-360 tiers | `config/45410914/scope_map.json` |
| **Wii census** | `../rb3/orig/SZBE69_B8/files/band_r_wii.map` + `sys/main.dol` |
| **DC3 census** | `../dc3-decomp/orig/373307D9/ham_xbox_r.map` + `default.xex` |

### Scratch artifacts (under `~/tmp`, not committed)
`~/tmp/laneBB/{coverage_matrix,wii_reverse,wii_map_text,rbn_vtables}.json` ·
`~/tmp/laneBB-strings/` (string-pool diff + RTTI inventory) ·
`~/tmp/laneBB-fnjoin/{mangle,fnjoin,controls}.py` + JSON ·
`~/tmp/laneBB-unknown/`

### Re-run triggers
Re-run when `splits.txt` gains TUs in bulk (cells shift as attribution grows) or
after a TU-target flip (all addresses move). The oracle censuses are **static** —
`band_r_wii.map` and `ham_xbox_r.map` never change.
