# Body-port pool re-rank — 2026-06-10 (post-refill, 6851 baseline)

**Read-only research handoff for the next body-port wave (2–4 Opus agents, 1–2 TUs each).**
Produced per `docs/decomp/playbooks/bodyport-wave.md` §2 from the FRESH main-tree report
(`build/45410914/report.json`, `measures.matched_functions = 6851 / 65544`, main @ `20590dd`).
All percentages below are **report `match_percent_normalized`** (the official metric — never
rank by live objdiff %). Spot-checks were run with
`python3 scripts/analysis/diff_inspect.py --diagnose --symbol '<mangled>' --unit <unit>`
against existing build artifacts on 2026-06-10.

## Method + pool stats

Pool = NAMED (non-`fn_`, non-`lbl_`) functions with `40 <= match_percent_normalized < 95`:
**261 functions across 146 units.** Ranked by headroom `sum((100-pct)*size)` per unit, then
re-weighted by *matched-fn yield* (a unit with one 9 KB fn maxes out at +1; multi-fn units
win for the counter). Excluded per the roadmap addendum + constraints:
at-limit `rndobj/Utl` (12 fns — biggest pool entry, but verdict "unit at-limit" 06-09/10),
`LightPreset` (11), `Geo` (6), `Mesh` (6), `MemTracker` (6), `MasterAudio`; refuted/deferred
`CameraShot` (7 — VBASE_WALL `fe0aaaa`), `Mat_NG` (matng-deferral.md), `TrainerGemTab` (4 —
UI-MI wall), `band3/game/Player` (vbase hierarchy confirmed wall), RndEnviron, UIComponent.

Top of the headroom table after exclusions (headroom / pool-fns / pool-bytes):

```
 426484   1fn   8948B  default/VocalTrack            (game; 1 giant fn, partial port)
 222612   1fn   5856B  default/SHA1                  (vendor-ish; regalloc-dominated)
 206204   2fns  8984B  default/OvershellSlot         (game; 2 giant fns)
  75736   5fns  2776B  default/Rnd                   (engine; best multi-fn unit)
  67132   2fns  2360B  default/FFT                   (synth_xbox)
  63924   3fns  3084B  default/band3/bandtrack/Gem   (game)
  63820   1fn   6068B  default/network/.../MD5       (Quazal; permuter-class)
  61160   2fns  2656B  default/Spotlight             (engine world)
  45412   3fns  1732B  default/SpotlightDrawer_NG    (engine world)
  44768   3fns  4772B  default/mapping0              (vorbis vendor)
  23784   3fns   524B  default/VocalTrackDir         (bandobj)
  23352   2fns  2304B  default/TourDescPanel         (game)
  22148   2fns   996B  default/CalibrationPanel      (game)
  17732   4fns  2084B  default/psy                   (vorbis vendor)
  15196   3fns   444B  default/GuitarController      (beatmatch)
  13660   3fns   496B  default/BinStream             (engine utl; concrete hypothesis)
```

---

## Per-unit dossiers

### 1. BinStream — `src/system/utl/BinStream.cpp` (unit `default/BinStream`) — **HIGH confidence, +2-3**
| fn | norm% | size |
|---|---|---|
| `?WriteEndian@BinStream@@QAAXPBXH@Z` | 49.94 | 72 |
| `?Read@BinStream@@QAAXPAXH@Z` | 60.28 | 172 |
| `?Write@BinStream@@QAAXPBXH@Z` | 87.21 | 252 |

- **Oracle:** rb3-Wii `../rb3/src/system/utl/BinStream.cpp` (Read @ ~line 95, Write @104,
  WriteEndian @179). DC3 `../dc3-decomp/src/system/utl/BinStream.cpp` is the FALSE friend here.
- **Spot-check (Read, diagnose):** 55 instrs, 41.8% equal, **12 insert / 5 delete** = small
  missing/extra logic blocks; minor regalloc.
- **ROOT CAUSE FOUND (verified by source 3-way diff):** our `BinStream::Read` is **verbatim
  DC3**, including the DC3-added `AutoGlitchReport report(50.0f, __FUNCTION__);` RAII object
  and DC3's crypto-loop form. rb3-Wii (retail-era intent) has **no AutoGlitchReport** and a
  different XOR-loop shape (`unsigned char *ptr/end; while (ptr < end) *ptr++ ^= mCrypto->Int();`).
  Fix hypothesis: port the rb3-Wii bodies for Read/Write/WriteEndian (drop AutoGlitchReport,
  match the loop form). Note rb3-xenon DOES compile `utl/GlitchFinder.cpp` — test with and
  without the report object; the insert/delete count (~12 instrs) is about the size of an
  AutoGlitchReport ctor/dtor pair.
- Unit history: layout already fixed (`fa9450d` mRevStack drop +11, `9517690` ReadAsync
  vtable drop +1) — the remaining residue is exactly these 3 bodies. **Bodyport-viable.**

### 2. Rnd — `src/system/rndobj/Rnd.cpp` (unit `default/Rnd`) — **MEDIUM-HIGH, +2-4**
| fn | norm% | size |
|---|---|---|
| `?DrawTimers@Rnd@@IAAMM@Z` | 49.46 | 1080 |
| `?OnToggleHeap@Rnd@@IAA?AVDataNode@@PBVDataArray@@@Z` | 80.65 | 124 |
| `?UpdateRate@Rnd@@IAAXXZ` | 85.99 | 424 |
| `?CreateDefaults@Rnd@@IAAXXZ` | 88.46 | 908 |
| `?WordWrap@@YAXPBDHPADH@Z` | 90.25 | 240 |

- **Oracles:** DC3 `../dc3-decomp/src/system/rndobj/Rnd.cpp` (1443 lines; DrawTimers @1059) and
  rb3-Wii `../rb3/src/system/rndobj/Rnd.cpp` (DrawTimers @457) — **both have every fn**;
  use rb3-Wii to arbitrate where DC3 rewrote.
- **Spot-checks:** `CreateDefaults` diagnose: 242 instrs, **55.8% equal**, dominant uniform
  **+60 offset on 20 instrs** + 15 insert / 9 delete — looks like a stack-frame/local-array
  delta or one DC3-vs-retail block; tractable. `DrawTimers` diagnose: 270 instrs, **84.4%
  replace** (whole-body divergence) — our DC3-derived body is a different-era implementation;
  re-port from rb3-Wii line 457 (debug-timer drawing changed between eras). Both bodies same
  ballpark size, so NOT a §3i mispair.
- Unit history: vtable lever (+10 `30a4ae8`), Watcher drop (`261b66a`), PostProc shape
  (`94c2672`) all landed — class layout is now believed correct, which is what makes these
  bodies freshly portable. **Bodyport-viable.** Caveat: WordWrap/UpdateRate residue may be
  partly regalloc; defer those per §3d if 2-3 variants don't move them.

### 3. Gem — `src/band3/bandtrack/Gem.cpp` (unit `default/band3/bandtrack/Gem`) — **MEDIUM-HIGH, +2-3**
| fn | norm% | size |
|---|---|---|
| `?AddChordInstance@Gem@@QAAXVSymbol@@@Z` | 71.33 | 908 |
| `?CreateWidgetInstances@Gem@@QAAXVSymbol@@@Z` | 78.30 | 472 |
| `?AddInstance@Gem@@QAAXVSymbol@@H@Z` | 83.77 | 1704 |

- **Oracle:** rb3-Wii `../rb3/src/band3/bandtrack/Gem.cpp` (598 lines; AddInstance @138,
  AddChordInstance @240). Our file (596 lines) is already a near-complete port
  (`774423d`, `b228fef` Gem layout bools-not-bitfields +1).
- **Spot-check (AddChordInstance):** 242 instrs, 24.8% equal, **47 delete / 15 insert** =
  target has real logic blocks we lack (likely widget-instance branches or stripped-assert
  side-effect args), plus moderate regalloc (r28<->r31 etc.). Small +8 offset cluster (4
  instrs) — check `GemWidgetInfo`/track member offsets vs struct_db, but the dominant signal
  is missing logic. The 3 fns share the widget-instance creation path — one root cause may
  flip all three. **Bodyport-viable.**
- Same agent should also evaluate **GemManager.cpp from-scratch port** (see §fingerprint below).

### 4. GuitarController — `src/system/beatmatch/GuitarController.cpp` (unit `default/GuitarController`) — **MEDIUM-HIGH, +1-2 (cheap)**
| fn | norm% | size |
|---|---|---|
| `?IsDisabled@GuitarController@@UBA_NXZ` | 50.00 | 16 |
| `?Handle@GuitarController@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | 63.24 | 372 |
| `??1MessageTimer@@QAA@XZ` | 87.14 | 56 |

- **Oracle:** rb3-Wii `../rb3/src/system/beatmatch/GuitarController.cpp` (288 lines).
  Unit ported `f101f54` (+6).
- **Spot-checks:** `Handle`: 120 instrs, **27 insert** (missing HANDLE cases / handler body)
  + **uniform +8 on 18 instrs** (possible small member delta in GuitarController or its
  HANDLE-macro temp layout — run `--offsets` and gate-zero check first). `IsDisabled`
  (compare-asm): our 2 instrs (`lbz r3,0x48; blr`) match the target's first half exactly; the
  target symbol span contains TWO folded tiny fns (`lbz 0x48/blr` + `stb r4,0x49/blr` — an
  adjacent `SetDisabled(bool)`-shaped setter). This is a **target-span/pairing artifact**, not
  logic: check whether our header declares the setter inline (emit it out-of-line in the .cpp
  and see if dtk's span pairs clean) — if not resolvable in 20 min, defer as split-artifact.
  **Handle is the real body-port target.**

### 5. OvershellSlot — `src/band3/meta_band/OvershellSlot.cpp` (unit `default/OvershellSlot`) — **MEDIUM, +1-2 (big bodies)**
| fn | norm% | size |
|---|---|---|
| `?UpdateState@OvershellSlot@@QAAXXZ` | 76.41 | 2132 |
| `?UpdateView@OvershellSlot@@QAAXXZ` | 77.24 | 6852 |

- **Oracle:** rb3-Wii `../rb3/src/band3/meta_band/OvershellSlot.cpp` (2076 lines; UpdateState
  @1164, UpdateView @1286). Our file (2053 lines, ported `c5011b0` +22) already contains both
  bodies near-verbatim — so the residue is **retail-360-vs-Wii-dev divergence**, not an
  unported body.
- **Spot-check (UpdateState):** 615 instrs, **51.5% equal, 82 insert / 40 delete** in discrete
  clusters + small +8 offset cluster (18 instrs) — real missing/extra logic blocks (likely
  Wii-dev-only branches stripped in retail, platform overshell text paths, or
  suppressed-side-effect args in stripped MILO_* paths — try the per-TU
  `MILO_ASSERT/(void)(args)` override pattern, playbook §5.5). Diff-guided iteration, not
  verbatim porting. UpdateView is 6.8 KB — budget accordingly; UpdateState first.

### 6. VocalTrackDir — `src/system/bandobj/VocalTrackDir.cpp` (unit `default/VocalTrackDir`) — **MEDIUM, +1-2**
| fn | norm% | size |
|---|---|---|
| `?TutorialReset@VocalTrackDir@@UAAXXZ` | 46.53 | 292 |
| `?PreLoad@VocalTrackDir@@UAAXAAVBinStream@@@Z` | 60.50 | 152 |
| `?Deploy@VocalTrackDir@@UAAXXZ` | 72.90 | 80 |

- **Oracle:** rb3-Wii `../rb3/src/system/bandobj/VocalTrackDir.cpp` (1361 lines). Unit ported
  `a9a547f` (+29).
- **Spot-check (TutorialReset):** 73 instrs both sides, **89% replace** — yet our source is
  **character-identical to the oracle** (verified). Same length + all-replace means the
  divergence is in **inlined `Find<T>`/dispatch expansion shape**, not the statement list —
  possibly the 2-arg FindObject / MILO_FAIL inline form (cf. `a524ad0`) expanding differently
  here, or virtual `SetFrame` dispatch vs direct call. ⚠ Risky: time-box 30 min; if the
  Find<> expansion can't be aligned, defer as codegen-shape. `PreLoad` (60.5, BinStream arg)
  and `Deploy` (72.9, tiny) are the safer first targets — check PreLoad against rb3-Wii's
  body (likely a missing PreLoad step or rev gate; do NOT convert to gRev — refuted).

### 7. VocalTrack — `src/band3/bandtrack/VocalTrack.cpp` (unit `default/VocalTrack`) — **STRETCH, +1 (deep)**
- `?UpdateScrolling@VocalTrack@@QAAXM@Z` — **52.34%, 8948 bytes** — the single largest
  headroom item in the whole pool (2877 instrs).
- **Oracle:** rb3-Wii `../rb3/src/band3/bandtrack/VocalTrack.cpp:1107`. Our file (2764 lines,
  partial port `c5011b0`) already has the fn at line 1109 ~verbatim.
- **Spot-check:** 19.9% equal, **640 insert / 375 delete** (large real logic divergence —
  retail-360 vocal scrolling differs from the Wii-dev body) **plus** very heavy regalloc
  (1353 swap instrs / 182 pairs) that will remain after logic alignment. Max payoff is +1
  matched fn for multi-hour work with permuter-class residue risk. **Only as a stretch goal /
  dedicated 4th agent; do not block the wave on it.**

### 8. Spotlight + SpotlightDrawer_NG — `src/system/world/{Spotlight,SpotlightDrawer_NG}.cpp` — **MEDIUM-LOW, +1-2**
| fn | norm% | size | unit |
|---|---|---|---|
| `?BuildNGCone@Spotlight@@IAAXAAUBeamDef@1@H@Z` | 70.80 | 1692 | default/Spotlight |
| `?BuildNGQuad@Spotlight@@IAAXAAUBeamDef@1@W4Constraint@RndTransformable@@@Z` | 87.80 | 964 | default/Spotlight |
| `?RenderConeDefs@NgSpotlightDrawer@@IAAXPAVSpotlight@@ABVColor@Hmx@@@Z` | 68.36 | 1324 | default/SpotlightDrawer_NG |
| `??0NgSpotlightDrawer@@QAA@XZ` | 92.56 | 312 | default/SpotlightDrawer_NG |

- **Oracle:** DC3 `../dc3-decomp/src/system/world/Spotlight.cpp` (1521 lines) +
  `SpotlightDrawer_NG.cpp` (993 lines). No rb3-Wii NG variants (Wii uses GX path) — DC3 only.
- **Spot-check (BuildNGCone):** 486 instrs, 20% equal, **53.1% diff_arg** (FP-heavy cone
  math, mixed regalloc) + 63 insert / 50 delete (some real logic). Mixed verdict: there IS
  body divergence but the FPR scheduling residue (§3d) may cap it below 100. bodyport-batch5
  visited Spotlight before — remaining fns are likely the hard residue. Take only as a
  second TU for an engine agent; abandon fast on FPR walls.

### 9. TourDescPanel + CalibrationPanel (game panels) — **MEDIUM, +1-2 (bench)**
| fn | norm% | size | unit |
|---|---|---|---|
| `?LoadIcons@TourDescPanel@@QAAXXZ` | 86.09 | 360 | default/TourDescPanel |
| `?UpdateExtendedCustom@TourDescProvider@@UBAXHHPAVObject@Hmx@@@Z` | 90.56 | 1944 | default/TourDescPanel |
| `?HandlePreAndPostTestAnim@CalibrationPanel@@QAAMM@Z` | 49.14 | 348 | default/CalibrationPanel |
| `?UpdateAnimation@CalibrationPanel@@QAAXXZ` | 93.14 | 648 | default/CalibrationPanel |
- **Oracles:** rb3-Wii `../rb3/src/band3/tour/TourDescPanel.cpp`,
  `../rb3/src/band3/meta_band/CalibrationPanel.cpp`. Both TUs exist ported in our tree
  (`src/band3/tour/TourDescPanel.cpp` wired in objects.json line ~716). Not spot-checked
  this pass (budget) — playbook-recon first. CalibrationPanel was a grind-execute3
  recon target previously; check `~/tmp` notes before investing. Bench/4th-agent material.

### 10. Vendor/crypto cluster — **DEFER (classified, do not launch)**
- `default/SHA1` `?Transform@CSHA1@@AAAXPAIPBE@Z` 61.99/5856B: diagnose = 53.7% diff_arg,
  1771 swap-instrs/134 pairs — **regalloc-saturated unrolled rounds**, permuter-class (§3d).
  Sources `src/system/math/SHA1.cpp` == DC3's (both 230-237 lines). Max +1, low odds. Defer.
- `default/network/Plugins/Checksum/MD5/MD5` `?transform@MD5@Quazal@@QAAXPBE@Z` 89.48/6068B:
  **77.8% diff_arg** over 1572 instrs = same class. Also remember network/ units carry the
  `/Od /Oi-` flag question (`project_quazal_network_od_flag`) — if anything, check the unit's
  extra_cflags before touching. Defer.
- `default/FFT` (synth_xbox, DC3 oracle exists) 2 fns 57.6/86.0 — FP matrix loops, likely §3d. Defer.
- `default/mapping0` (3 fns 89-94) + `default/psy` (4 fns 87-94), vorbis `.c`: mapping0_forward
  diagnose = 51.2% diff_arg + 44 delete; plausibly a **vorbis version delta** (RB3-era libvorbis
  vs DC3's). A from-scratch source-version hunt is research, not a body-port. Defer; note as a
  possible future "match the vendor drop" experiment (cf. `project_c_libs_compiled_as_cpp`).
- `default/json_tokener` `json_tokener_parse_ex` 93.57/4872B: single giant switch fn; /TP
  already applied. Max +1; not spot-checked. Bench only.
- STLport template residue in `default/DepthBuffer3D` / `default/MeshDeform` vector internals
  (49-90%): per-TU instantiation codegen; sized-vector is REFUTED — leave alone.

---

## From-scratch game-TU candidates (`tools/fingerprint_pipeline.py candidates --min-fns 4`)

12 workable (source present, unported, ≥4 oracle fns). Highlights with rb3-Wii source:

```
9 fns  band3/game/GemPlayer.cpp     ← big player TU: per-FUNCTION only (playbook §2), no span pin
9 fns  band3/game/VocalPart.cpp     ← same caveat
6 fns  band3/bandtrack/GemManager.cpp   ← BEST: same area as Gem dossier; tight bandtrack TU
5 fns  band3/game/BandUser.cpp / BandUserMgr.cpp  ← player-TU caveat applies
4 fns  band3/game/Performer.cpp, meta_band/BandSongMgr.cpp, meta_band/ProfileMgr.cpp,
       tour/TourSavable.cpp
4 fns  network/net/{NetMessenger,NetSession,SessionSearcher}.cpp  (check /Od flag first)
```

**Recommendation:** hand `GemManager.cpp` to the Gem agent (same oracle dir
`../rb3/src/band3/bandtrack/`, 6 oracle fns, scaffold via
`python3 tools/fingerprint_pipeline.py scaffold band3/bandtrack/GemManager.cpp`). Beware the
coverage-stub mirage on trivial accessors (playbook §3) — port real-bodied fns only.

---

## RANKED LAUNCH LIST (3 Opus agents, per playbook §4-§9)

| # | Agent | TUs | Est. gain | Confidence | Key evidence |
|---|---|---|---|---|---|
| 1 | A (engine) | **BinStream** + **Rnd** | +4 to +6 | HIGH (BinStream) / MED-HIGH (Rnd) | BinStream root cause FOUND (DC3 AutoGlitchReport in Read/Write/WriteEndian; rb3-Wii form is retail). Rnd: CreateDefaults 55.8% equal w/ clean +60 cluster; DrawTimers re-port from rb3-Wii:457; layout pre-fixed by `30a4ae8`/`261b66a`. |
| 2 | B (game, bandtrack) | **Gem** + **GemManager (from-scratch)** + GuitarController if time | +3 to +6 | MED-HIGH | Gem 3 fns w/ delete-block logic divergence, oracle verbatim available; GemManager = top fingerprint candidate (6 oracle fns, same dir); GuitarController::Handle 27-insert body-port. |
| 3 | C (game, UI/vocal) | **OvershellSlot** + **VocalTrackDir** | +2 to +4 | MEDIUM | OvershellSlot::UpdateState 51.5% equal w/ discrete insert clusters (try (void)(args) overrides); VocalTrackDir PreLoad/Deploy first, TutorialReset time-boxed (identical-source all-replace = codegen-shape risk). |
| 4 | D (optional stretch) | **VocalTrack::UpdateScrolling** (single fn) or TourDescPanel+CalibrationPanel | +1 to +3 | LOW-MED | UpdateScrolling = biggest headroom but +1 max and 182 regswap pairs; panels unspot-checked, recon-first. |

**Do NOT launch:** SHA1, MD5, FFT (regalloc/permuter-class — classified above), vorbis
units (version-delta research, not body-port), all at-limit/refuted units in the exclusion
list. After landing, run `NINJA_JOBS=12 tools/refill_loop.sh --map global_fuzzy_pairs.json`
(playbook §9) and re-check `tools/inline_policy_finder.py` for n≥2 clusters.

**Tooling gaps hit this pass:** none blocking — diff_inspect `--diagnose`/`--compare-asm`
worked for every unit incl. game paths (the `f82ed7f` obj-path fix held). Ghidra avoided
per constraints. Note: `--diagnose` "Match estimate" disagrees wildly with report-normalized
(e.g. DrawTimers 1.5% vs report 49.46) — it's positional, fine for classification, never for
ranking.
