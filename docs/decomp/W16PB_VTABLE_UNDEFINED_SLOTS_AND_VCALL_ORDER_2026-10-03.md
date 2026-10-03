# W16-PB — undefined vtable slots, call-site slot order, and the lever-3 remainder (2026-10-03)

Lane W16-PB, branch `w16-pb`. Brief: finish the class-layout and vtable audit
that `CAMPAIGN_STATE_2026-10-03.md` §6 lever 3 lists as left over from W16-OP
and W16-OR — secondary vtables, vtable order on the engine dirs, the
`no_base` / `parent_no_primary` / unreadable tables, and the RB3-only
synth-effect and dsp classes. `src/network` and `src/xdk` out of scope.

## 1. The lever-3 list was already mostly drained

`CAMPAIGN_STATE_2026-10-03.md` was written at 05:49; **W16-OT landed at
05:57** (`W16OT_ALL_TABLES_VTABLE_AND_SIZEOF_AUDIT_2026-10-03.md`) and did
three of the four items: secondary tables and the `no_base` /
`parent_no_primary` tables via `vtable_override_pattern.py --all-tables`, and
the name-keyed order sweep over the engine dirs. So this lane re-ran OT's
instruments as a reproduction and then built the two instruments OT's own
write-up says it lacked:

* OT's BODY check reaches only slots whose **retail** body is a tiny leaf. It
  found three virtuals defined nowhere that way (`BeatMatchController` 31/32,
  `User` 28). Nothing checked every slot.
* `vtable_order_sweep.py` judges order by **map name** and leaves 748 tables
  UNRESOLVED; its docstring names the call site as the authoritative
  instrument, which nobody had built.

## 2. Instruments

### 2.1 `tools/vtable_override_pattern.py --all-tables` (reproduction, +1 change)

Re-run on the fresh worktree: `tables 1812 · compared_primary 811 ·
compared_secondary 842 · compared_via_ancestor 28 · body_checked 8923 ·
OURS_OVERRIDES 78 · OURS_NOT_EMITTED 19 · body_ours_missing 32`. The
`OURS_OVERRIDES` / not-emitted populations are OT's and OP's, already
adjudicated there; nothing new.

Change: `BODY_OURS_MISSING` slots are now **emitted as rows**, not only
counted. The 32 were: **14 = SongInfoCopy / DataArraySongInfo slots 9, 10,
14–18** (the defect fixed in §3.1) and **18 = Quazal** `RB*Client` protocol
slots (out of scope).

### 2.2 `tools/vtable_undefined_slots.py` (new) — every slot of every table

The match build compiles and never links, so a virtual that an emitted `??_7`
table references and no object defines is an undefined external nothing
reports. The census walks every `??_7` in every compiled obj and requires every
slot target to be defined by **some** obj (`secnum > 0`, or storage class 105:
MSVC emits `??_E` as a **weak external** alias of `??_G` — the first run
missed this and read **1,287** false rows). Each miss is classified by a
source grep: `HX_NATIVE_ONLY` / `NOT_COMPILED` / `NOWHERE` / `OTHER`.

Control: `--selftest` removes a known-defined probe
(`?Handle@Object@Hmx@@…`, referenced by 411 tables) from the defined set and
requires the census to report every reference — **PASS**; vacuity floor
≥900 objs / ≥2,000 tables (actual 1,266 / 11,664, 89,965 slots).

| run | NOWHERE | HX_NATIVE_ONLY | OTHER | rows |
|---|---:|---:|---:|---:|
| before fixes | 32 | 25 | 14 | 71 |
| after fixes | 31 | 18 | 14 | 63 |

The 8 rows removed are exactly the 7 SongInfoCopy getters and
`BandUser::IsInSession` (§3.1, §3.2). The 63 that remain, by family:

| family | rows | kind | disposition |
|---|---:|---|---|
| `BinkMovieImpl` | 17 | HX_NATIVE_ONLY | **absent from retail** (0 name strings in `band.exe`); DC3-only class |
| `DepthBuffer3D` | 1 | HX_NATIVE_ONLY | absent from retail; DC3-only |
| `SingleItemEnumJob` (via `PostPurchaseEnumJob`) | 3 | NOWHERE | absent from retail; DC3-only |
| `ATG::CSampleXAPOBase<BitCrushEffect>::OnSetParameters(const void*, unsigned)` | 14 | OTHER | **real, out of scope** — see §5 |
| XDK `CXAPOBase` / `CXAPOParametersBase` library virtuals | 14 | NOWHERE | XDK library, out of scope |
| Quazal | 14 | NOWHERE | out of scope |

(`BitCrushEffect` / `FxSendBitCrush` themselves have 0 name strings in
retail — DC3-only, which also makes the `CSampleXAPOBase` row moot for
matching; it is still a native-link hazard.)

### 2.3 `tools/vcall_slot_census.py` (new) — vtable ORDER from call sites

A virtual call on X360 is `lwz rV,M(obj); lwz rF,D(rV); mtctr rF; bctr[l]`.
M and D are **immediates** in retail's own code — no relocation, so no ICF fold
or map name can poison them, and a header with a virtual declared in the wrong
place shifts D at every caller of every later slot. The tool extracts the
ordered (M, D) sequence from the target and base body of every paired row at
`0 < fuzzy < 100` (rows at 100 agree on every immediate by construction), aligns
with difflib, and aggregates disagreements by (M, D_retail, D_ours), so a
systematic header shift is one line with a large count.

Control: `--selftest` plants a slot shift that must be reported and asserts a
≥1,000-row population (actual 1,839) — **PASS**. A second control came from the
data: `BandRetargetVignette::Poll` first read M=0x20 vs M=0, which was the same
vfptr reached as `addi r3,r3,0x20; lwz r11,0(r3)`; the extractor now folds a
preceding `addi` into M and that row agrees.

Result (src/network + src/xdk excluded): **944 rows, 820 retail vcalls, 813
equal, 1 slot disagreement, 2 other, 7 unaligned.**

* the single slot disagreement is `Rnd::DrawPreClear` (80.8): retail deletes
  the object at struct+0 via slot 0 with r4=1; ours reads +8 and calls slot 1.
  That is a divergent hand-decompiled body, not a header order defect (no other
  caller of the class moves).
* the 2 "other" were `InlineHelp` — the defect in §3.3 (different receiver
  offset: BandLabel's `UITransitionHandler` vfptr at +0x214).
* the 7 unaligned sites sit in `BandCharacter::Filter`,
  `BandLeadMeter::SyncScores`, `CharClip::LockAndDelete`,
  `ReadSingleXinputJoypad` and `StorePanel::CheckOut` — read by hand; each is a different call
  sequence in the body (source divergence), not a slot shift.

⇒ **No vtable order defect anywhere in scope.** 813/820 call sites agree on
slot immediates, and none of the 7 residual sites is a header shift.

### 2.4 RB3-only synth-effect and dsp classes

`scratch: ~/tmp/w16pb_dispset.py` compares, per sub-100 row, the **multiset of
load/store displacements** in retail vs ours — a layout defect changes the set,
scheduling/addressing-mode does not. dsp/synth effects: **15 of 18** sub-100
rows have identical displacement multisets; `Wah`, `EQ` and `VoiceBeat` differ
only in addressing mode (indexed vs displacement form of the same field).
Constructor callers are all at 100 except the Synapse ctor, `AddRemoteMic` and
`SetupHeadsetSubmixes` — each read by hand, none a layout defect. **0
defects.**

### 2.5 The unreadable tables

The 8 anonymous-namespace tables W16-OR could not read: slot counts match
retail, and all **34** leaf-slot bodies agree (e.g.
`OpenWaitingGateMsg::GetLockData` = `addi r3,r3,0x14; blr`, matches
`0x827E6180`). OP's 13 not-emitted tables were already compiler-checked by OP.
**0 defects.**

## 3. Defects fixed (all on retail bytes)

### 3.1 `SongInfoCopy` — 7 virtual getters existed only under `#ifdef HX_NATIVE`

`src/system/utl/SongInfoCopy.cpp` carried `GetNumVocalParts`,
`GetHopoThreshold`, `GetCrowdChannels`, `GetDrumSoloSamples`,
`GetDrumFreestyleSamples`, `GetMuteVolume`, `GetVocalMuteVolume` inside an
`#ifdef HX_NATIVE` block whose comment claimed retail emits them elsewhere. It
does not: retail's `SongInfoCopy` and `DataArraySongInfo` vtables point slots
9/10/14–18 at `0x8252E038`, `0x8235AFA0` (ICF folds) and `0x827D10F8`,
`0x827D1100`, `0x827D1108`, `0x827D1110`, `0x827D1118` (this TU's own bodies,
which sit right after `GetCores`). The getters are now unconditional and in
retail order; every body is byte-identical to retail.

### 3.2 `BandUser::IsInSession` — declared, in 4 vtables, defined nowhere

Retail body at `0x8268ADE0` (72 B), right after `UnkTU5Virtual`:
`if (mgr) return mgr->HasUser(this); return false;`. Added to
`src/band3/game/BandUser.cpp`; mapped as
`?IsInSession@BandUser@@UBA_NPAVSessionMgr@@@Z`.

### 3.3 `InlineHelp::mTextLabels` holds `BandLabel*`, not `UILabel*`

Retail's dtor and `SyncLabelsToConfig` delete each element through the vfptr
at **+0x214** — `BandLabel`'s `UITransitionHandler` base, which `UILabel` does
not have. `std::vector<BandLabel *> mTextLabels; // 0x158` and
`Hmx::Object::New<BandLabel>()`. `mTemplateLabel` is left `UILabel*` (no byte
evidence either way).

## 4. Measurement — `tools/ab_measure.py`

**Prediction (written before run 1):** +3 fns / +420 B — `??1InlineHelp`
94.98→100 (+340 B), `IsInSession` 0→100 (+72 B), `GetDrumSoloSamples`
`0x827D1100` 0→100 (+8 B); `SyncLabelsToConfig` → 99.94 (0 B).

**Run 1 (patch incl. a map name for `0x827D1100`), base `b2d214cb5`:**
Δmatched **+2**, Δcode_bytes **+124**, Δhonest +2. Prediction failed.
Row-level comparison of the archived leg reports (keyed (unit, symbol), rows
missing from B counted as down, renamed rows re-keyed by address): the three
predicted rows rose exactly as predicted (+420 B), and
**`AccomplishmentManager::IsAvailable` (296 B) fell 100 → 99.93** — 420 − 296 =
124, exact. Cause: `0x827D1100` is itself an **ICF fold survivor** that
`IsAvailable` reaches by direct `bl` where our source calls
`Accomplishment::GetDynamicPrereqsSongs`. Naming the address turned a forgiven
placeholder site into a charged name site. I had already left `0x827D10E8`,
`10F8` and `1108` anonymous for exactly this reason (10 direct `bl` callers
between them) and missed that `1100` has one too. **Fix: the `0x827D1100` map
entry is removed** (`e34b648f6`); the getter stays anonymous.

**Run 2 (final patch), base `89dfeac60`:** re-predicted after run 1 as **+2 fns / +412 B,
0 rows down**. Measured (run dir
`~/tmp/wt-w16-pb-ab/.ab_measure_runs/20261003-091658-w16pb_branch3-719518`):

```
leg A: matched=53349 masked=25155 honest=28194 code%=57.492424  (recompiles: 0, settled)
leg B: matched=53351 masked=25155 honest=28196 code%=57.496445  (recompiles: 284, split=1)
Δmatched=+2  Δmasked_equal=+0  Δhonest=+2  Δcode%=+0.004021pp  Δcode_bytes=+412
units at 100% [mpn]: 533 -> 533 · [all-rows-fuzzy]: 470 -> 470
```

Row-level check over both archived leg reports (68,909 rows each): **0 rows
down.** Movers: `??1InlineHelp` 94.98→100 (+340 B), `fn_8268ADE0` →
`?IsInSession@BandUser@@…` 0→100 (+72 B, same address, re-keyed),
`SyncLabelsToConfig` 94.39→99.94 (+0 B). Prediction met exactly. The getter
bodies at `0x827D10F8..1118` are byte-identical to retail but stay unpaired
(anonymous rows), so the SongInfoCopy fix is worth 0 B on the metric — its
value is that the vtables now resolve (native link + the census).

## 5. Found, deliberately not fixed

* **`ATG::CSampleXAPOBase<…>::OnSetParameters(const void*, unsigned)`** —
  undefined, 14 census rows. Belongs in `src/xdk/xaudio2/xapobase.h` (out of
  scope). One-line fix for whoever owns that tree:
  `{ OnSetParameters(*(const Params *)p); }`; retail's shared body is
  `0x82B5AC08` (16 B row in `system/synth_xbox/Synth`, currently 0%). Moot for
  matching because its only instantiation (`BitCrushEffect`) is DC3-only; a
  native-link hazard only.
* **DC3-only classes compiled into the match build** — `BinkMovieImpl`,
  `DepthBuffer3D`, `PostPurchaseEnumJob`/`SingleItemEnumJob`,
  `BitCrushEffect`/`FxSendBitCrush`: 0 name strings in `band.exe`. Their
  undefined slots are a symptom of the classes not existing in RB3, not a
  layout defect.
* **`Rnd::DrawPreClear`** — body divergence (§2.3), not a vtable defect.
* **SongInfoCopy fold survivors `0x827D10E8/10F8/1100/1108`** — left anonymous
  (fold bait, §4).
* **`BandRetargetVignette::Poll`** — retail evaluates `PathName(this)` inside
  `MILO_NOTIFY_ONCE` (the `RB3_NOTIFY_ONCE_EVAL` class); source, not layout.
* **Possible overlap with W16-PC**: the `InlineHelp` rows may also be in
  W16-PC's I1/I3/I4 lists. The fix here is on retail bytes (+0x214 receiver);
  whichever lane lands second should rebase over it, not redo it.
* No permuter, no regalloc grinding, nothing under `src/network` or `src/xdk`.
