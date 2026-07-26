# laneAV — the `_bijection_arbitrary` re-decision, and a scope predicate that was wrong

**Merge-base:** main `71acd6e7` = **38,139** strict (the lane started at `620bfb21`
= 37,619; main advanced twice mid-lane).

> ## **+46 strict, 0 real losses — reproduced on two different bases**
> `71acd6e7` 38,139 → **38,185**, and after merging main forward,
> `ade6b32b` 38,259 → **38,305**. The identical +46 on two bases a full laneAT
> round apart is the cross-check that the delta is independent and the rebase
> faithful.
> unit+name 47 gained / 1 lost · **name-only 46 gained / 0 lost** (the single
> unit+name loss is `?SaveType@Object@Hmx@@` migrating DirLoader→Object with its
> span — the pin move working, which is exactly what the unit-agnostic leg
> exists to distinguish). `overlap_check` clean (`.text` 5,703 / `.pdata` 5,163,
> 0 overlaps); map 25,669 entries, **0 duplicate VAs** on the raw-line check;
> `_bijection_arbitrary` 1207 / `_icf_arbitrary` 25 / `_denylist` 3 intact.

| commit | content | strict |
|---|---|--:|
| `baedcd09` | 24 VA-closed identity repairs in byte-identical classes | **+0** |
| `f2473e8a` | 42 over-covering `.text` pins → 36 move+map (worker B, rebased) | **+17** |
| `9a1252b6` | 67 unpinned vtable VAs → 51 new pins + 49 map entries + 1 source fix (worker D, rebased) | **+29** |

**The two items the lane was chartered on both closed, but not symmetrically:**
the `_bijection_arbitrary` re-decision is **strict-neutral by construction and
we proved it** (ceiling +2, measured +0), while the *splits* items — the 42
over-covering pins and the 67 unpinned VAs — carried the entire +46. **Two of
five worker fragments (+26 and +8, both correctly measured) rebased to exactly
0 net-new** because a concurrent lane had taken every VA in the interval.

Also delivered: **83 VAs converted from 0.00 % structurally-unpairable to paired
and scored** (19 from the pins, 64 from the unpinned arm) — a ready body-port
worklist that never shows in the strict count — plus six refutations, four of
them of our own claims, and one wrong scope predicate found and priced.

---

## 1. The `_bijection_arbitrary` channel has a measured ceiling of **+2**

`scripts/target_symbol_map.json` carries a 1,207-element `_bijection_arbitrary`
array: VAs whose name was assigned by an arbitrary bijection over a
reloc-masked byte-identical equivalence class (laneAK). objdiff pairs by **name**
and then compares **bytes**, so any bijection within such a class scores 100 % on
every pair.

Measured on a clean full build at `620bfb21`:

| | n |
|---|--:|
| `_bijection_arbitrary` VAs | 1,207 |
| already strict-100 | **1,205** |
| sub-100 | **2** |
| unmapped / absent from report | 0 |

The only two sub-100 entries, both 12 bytes at 80.0 %:
`0x82741c28` `?EaseQuarterStairstep@@YAMMMM@Z` (`default/CharClip`) and
`0x82742c98` `?ResumeFunc@@YAXXZ` (`default/Splash`).

> **So re-deciding the set in place cannot move the counter by more than +2.**
> The brief predicted "modest headline movement"; the honest number is smaller
> than that, and it is knowable *before* any build. Anyone funding this channel
> for headline should stop.

**Size profile of the 1,207:** 0-16 B **838** · 17-32 B 18 · 45-68 B 37 ·
69+ B **314**. Top units: BandCharacter 49, Anim 28, GemTrackDir 26, BandUser 24,
Rnd_Xbox 23. 69 % of the set is ≤16 B — 2 to 4 instructions — which is exactly
where laneAR measured byte identity to be *vacuous* (at 12 bytes with 2 of 3
instructions relocated: 3,348 obj-hits / 556 names). **The decidable arm is the
314 at ≥69 B**, which is where laneAR's 88-byte `StaticClassName` class (453
members, each materialising a distinct legible class-name literal) lives.

### 1.1 ★ A measurement trap not on any standing list

`report.json` functions carry **two** percent fields and only one is the strict
set:

| predicate | whole-binary count |
|---|--:|
| `match_percent_normalized >= 100` | **37,619** = `measures.matched_functions` exactly |
| `fuzzy_match_percent >= 100` | 37,397 — **222 low** |

We produced and had to retract a wrong figure (a "+4 ceiling") from the fuzzy
field before catching this. **Cross-check every hand-computed strict count
against `measures.matched_functions`; if it is not exactly equal, the predicate
is wrong.** `/home/free/tmp/laneAS/strictdiff.py` already uses the right field.

## 2. ★★ The load-bearing lesson: a repair set is a PERMUTATION, and splitting it orphans names

laneAV worker A proposed **31 repairs + 26 additions** (rescued unmeasured as
`38d9c4fb` on branch `laneAV-A`).

**The 26 additions were 26/26 already claimed on main** by a concurrent lane —
7 name-identical, 19 *conflicting permutations of the same byte class*
(e.g. `0x8273ddc8`: ours `?SetType@DxCam@@`, main `?PreLoad@BandLabel@@`;
`0x8273d9e8`: ours `?PreLoad@BandLabel@@`, main `?SetType@DxLight@@`;
`0x8273e4d0`: ours `?SetType@DxLight@@`, main `?SetType@DxCam@@` — one 3-cycle).
This is `_bijection_arbitrary` observed empirically for the third time. All 26
dropped; **net-new contribution zero.**

Applying the remaining 31 repairs alone then measured **−1, with 5 losses**:

```
- ?Handle@GameplayOptions@@$4PPPPPPPM@A@AA?AVDataNode@@PAVDataArray@@_N@Z
- ?SetControllerType@BandUser@@QAAXVSymbol@@@Z
- ??_ECharForeTwist@@$4PPPPPPPM@A@AAPAXI@Z
- ?IsReady@GemPlayer@@UBA_NXZ
- ??_GActionElement@InlineHelp@@QAAPAXI@Z
```

Those five are exactly the names left with **no VA at all** — a pre-flight
name-conservation check had predicted 4 drops / 5 introductions before the build
and was right.

> ★**A proposed map delta is a permutation whose cycles can span the "added" and
> "changed" halves. You cannot apply half of it.** Dropping the double-counted
> additions silently orphaned the names their cycles were carrying.

**The fix — apply only the VA-closed, name-conserving subset.** A repair is safe
only if the name it *displaces* is re-homed within the applied set **and** the
name it *introduces* was displaced from within it. Closure is asserted
(displaced-name multiset == introduced-name multiset) before writing. That
subset is **24 of 31**, and it measures clean:

```
38,139 -> 38,139   +0 strict   gained 0   lost 0
name-only (unit-agnostic): 38,024 -> 38,024   +0 / 0 / 0
duplicate VAs 0 (checked on RAW LINES, not json.load)
unpaired(0%) -> SCORED conversions: 0
```

**The 7 excluded repairs** (cycles escaping into the double-counted additions):
`0x823c6520`, `0x8262c280`, `0x8262c530`, `0x8268bab0`, `0x8268bb68`,
`0x8268e478`, `0x826bc6a8`. They are *not* refuted — they are unlandable until
someone re-derives their cycle against current main.

### 2.3 ★★ Worker A's measured **+26** does not survive the rebase — it is **0** net-new

Worker A subsequently completed a full A/B at the **old** base and landed
`30f165b3` / `c95708b2`: **37,619 → 37,645 = +26**, 34 gained / 8 lost, with an
honest decomposition that confirmed this lane's central prediction —

| channel | applied | strict |
|---|--:|--:|
| re-decision of the bijection (repoint) | 40 VAs | **+0** *(strict-neutral, as predicted)* |
| **class completion** (spare class name → unmapped byte-identical VA, same unit) | 25 VAs | **+25** |
| a name **freed** by a re-decision finding a home | 1 VA | **+1** |

Precision was calibrated non-circularly (held-out pool of multi-member byte
classes containing **only non-arbitrary** entries; the alphabetical tie-break
scores **44.0 %** on that pool, so contamination would have shown as ~100 %):
all DECISIVE **83.4 %** (n=955), **≥69 B with content evidence 100 %** (n=70,
the only de-tagging gate), ≤16 B **89.5 %** (n=381). Dispositions: 79 DECIDED,
11 SINGLETON, **1,141 REFUSED** (483 TIE, 351 class too large to enumerate,
224 all-candidates-contradicted, 44 elimination-only, 39 not multi-member).

**Re-derived against current main `71acd6e7`, all 66 VA edits collapse:**

| | n |
|---|--:|
| adds identical to main already | 7 |
| adds where main placed a **different** name | 19 |
| adds genuinely clean | **0** |
| changes where the target name is already main's, elsewhere | 36 |
| changes genuinely clean | 4 |

And the closing check: **at all 19 VAs where main placed a different name, main's
incumbent already reads 100.0 %** — scoring-equivalent, nothing to win. laneAT's
concurrent +520 consumed the entire vein.

> ★**Worker A's net-new contribution to current main is 0 strict.** The +26 was
> real at its base and is double-counted now. This is the **third** independent
> instance in this one lane of *two lanes on one evidence channel must measure
> the union, never the sum* — and the first where a fully-measured, correctly
> A/B'd, honestly-decomposed result evaporated purely on timing.

Worker A also refuted two of its own tool assumptions (the free-name test must be
**per-unit**, since objdiff pairs by name inside one unit — `no_class` 186 → 30;
and the class-completion pool must exclude names by the **post-repoint** map, as
a TIE class keeps its incumbents — 16 of 26 entries had reused a still-mapped
name, caught by the applier's duplicate check rather than by reasoning) and
**refuted this lead's ≤16 B prior**: what is vacuous at 12 bytes is *byte*
identity, not *relocation* content — a 12-byte adjustor thunk's branch target
resolves to a specific function, and that band measured **best** (89.5 %).

### 2.1 The 24 landed repairs

Most are clean 2-cycles within a byte-identical class, several semantically
compelling (`?SetGridSpan@UIList@@` ↔ `?SetNumDisplay@UIList@@`;
`?OnEnableComponent@PanelDir@@` ↔ `?OnDisableComponent@PanelDir@@`;
`?UnisonStart`/`?UnisonEnd@TrackPanelDir@@`;
`?LoadFixed`/`?SaveFixed@StandIn@@`; `?DataDir`/`?PostSave@ObjectDir@@` ×2
adjustor pairs; four `VocalPlayer::Add*Stat` in a 4-cycle).

### 2.2 Method notes that should outlive this lane

* **Never re-serialise the map.** `scripts/harvest/map_line_splice.py` (new,
  committed) changes **exactly one line per repair** — 24 insertions / 24
  deletions. A `sort_keys` rewrite churns all ~25 k lines and makes the branch
  unmergeable against every concurrent map lane.
* The splicer requires a **colon after the closing quote** to identify an entry
  line. A bare `startswith('"0x')` also matches the bare `"0xVA",` **elements of
  the `_denylist` / `_icf_arbitrary` / `_bijection_arbitrary` arrays** and
  corrupts the JSON. It re-parses after writing and asserts all three arrays are
  still lists of bare strings, and that duplicate VAs are 0 on the raw lines.

## 3. ★★ A wrong scope predicate — found, priced, and then deflated by its own worker

Every identity lane defines "in scope" as *outside* `0x82800000..0x82D00000`,
hard-skipped as XDK + Quazal. **That window is a VA-range proxy for "vendor" and
it admits our own code.** Measured:

* **207 of our own units hold pinned `.text` inside the window, totalling
  570,988 B (0.54 MB)** — `VocalTrack.cpp` 44,320 · `GemManager.cpp` 26,368 ·
  `UIFontImporter.cpp` 21,768 · `synth_xbox/Synth.cpp` 16,572 ·
  `bandtrack/TrackPanel.cpp` 15,880 · `UI.cpp` · `Mic.cpp` · `bandtrack/Gem.cpp`
  · `GemTrack.cpp` · `FFT.cpp` …  `Track.cpp`, `FIRFilter.cpp`,
  `UIListArrow.cpp`, `ViewSetting.cpp` are all declared in `objects.json`.
* Anonymous `fn_` in **pinned** units inside the window: 1,538 — **939 already
  strict**, 485 at exactly 0.0 %, 114 scored.
* ★**Exact reconciliation:** anonymous-in-`auto_`-inside-window 10,826 +
  anonymous-in-pinned-inside-window **599** = **11,425**, which is precisely
  laneAS's independently published hard-skipped total. So exactly **599
  functions in our own pinned units were swept into the vendor bucket** by every
  identity lane to date.
* **598 of the 599 are in non-vendor source units** (79 units); exactly 1 is
  vendor-ish (`default/SoundTouch`, which we compile anyway).

**The exclusion is systemic, and inconsistent.** `perunit_funnel.py` has
`VENDOR = (0x82800000, 0x82D00000)` — **and an `--include-vendor` flag at line
125 that was never turned on.** Worker E found **12** committed files hardcode
the constant, with *mutually inconsistent* predicates: `signature_mismatch_scan.py`
and `tu_wiring_rank.py` use an **unbounded** `>= 0x82800000`, and
`joint_unblock.py` uses a different upper bound `0x82C00000`.

> **Recommended predicate: "the unit is an `auto_*` carve", not any VA range.**
> All 10,826 genuine vendor anonymous functions live in `auto_*` units, and every
> pinned in-window unit is by construction something we compile.

### 3.1 ★ …and the vein is POORER than the pool already mined — our own framing, refuted

Worker E ran the per-unit byte-identity funnel over the 599 and shipped the 8
EXACT_UNIQUE entries for a measured **+8 / 0 losses** (at the old base). Funnel:
EXACT_UNIQUE 8 · EXACT_AMBIG 3 · WD1 1 · WD2 10 · WD3 10 · **WD4+ 342** ·
NEARSIZE 152 · SOURCE_MISSING 68 · NO_BASE_OBJ 5.

**EXACT_UNIQUE rate in the excluded region is 1.34 % (8/599) versus 3.09 %
(295/9,557) in the region already worked.** The mis-skipped pool is *less* rich
than the mined one; 342 WD4+ / 152 NEARSIZE / 68 SOURCE_MISSING is body
divergence, not identity. **This is a correctness fix worth single digits, not a
vein — do not fund a follow-up round.** The lead's "un-picked pocket, unusually
high expected value" framing is refuted by its own worker.

And the credit is gone too: **all 8 of worker E's entries are now on main
character-for-character**, landed by laneAT's concurrent +520. Worker E's
net-new contribution to main is **0**. Two lanes on one evidence channel must
measure the **union**, never the sum — third instance in three days.

Worker E's held-out control, with **both** `_bijection_arbitrary` and
`_icf_arbitrary` truth rows excluded: EXACT_UNIQUE **12,924/12,971 = 99.64 %**,
flat across every band (0-16 B 99.52 % … 85+ B 99.71 %) — an independent
reconfirmation that uniqueness is self-calibrating and that no entropy gate
belongs here. **EXACT_AMBIG measured 36.93 %**, *worse* than the standing 44.95 %
figure; not shipped.

## 4. Item 3 — "overlapping data carves" — REFUTED (worker C, `248dd4cd`)

laneAS §13.2 reported *"2,036 `lbl_` VAs disagree in content between
`auto_06_82C34400_data` and `auto_06_82C64400_data` … a real splits defect."*

**The count is right (re-derived: 2,045 of 15,260 shared) and the interpretation
is wrong. These are not two carves of one binary; they are the same carve of two
different binaries.**

| | `auto_06_82C34400_data.s` | `auto_06_82C64400_data.s` |
|---|---|---|
| header span | `0x82C34400` +`0x1F35EC` | `0x82C64400` +`0x1F5EAC` |
| mtime | **2026-07-13** | 2026-07-26 |
| in current `config.json` | **NO** | yes |

Both reproduce their binary's PE `.data` section geometry exactly: the first is
the **TU0** image (`orig/45410914/tu0-archive/band.exe`), carved before the
2026-07-15 TU5 flip and never deleted. **0 splits defects, 0 strict yield.**

The real finding underneath: **a 282 MB reservoir of stale dtk carve artifacts —
8,638 orphans (8,548 `auto_*`, 90 named) against 4,174 live units — that ~14
asm-globbing tools silently read, at a measured 63.9 % wrong-file resolution for
the `locator.py` access shape.** ★**mtime is NOT a freshness proxy** (72 of the
90 named orphans are same-day). Guard: `scripts/harvest/stale_artifact_scan.py`.

## 5. The `n_definers` gate — right gate, and this pool passes it 109/109

A sister pool of 290 "WRONG-UNIT" rows was found to be **276/290 with no definer
anywhere** (XDK entry points, `__unwind$` funclets, unwired TUs) — unfixable by
any map or pin edit. We applied the same gate here, from the COFF symbol tables
of all 1,024 compiled objs (**411,517 defined symbols**, section number > 0 —
this independently reproduces laneAR's "411 k-symbol COFF index"):

| pool | NO_DEFINER | UNIQUE_DEFINER | MULTI_DEFINER |
|---|--:|--:|--:|
| 42 over-covering pins (`cross_unit_review`) | **0** | 29 | 13 |
| 67 `unpinned` vtable VAs | **0** | 37 | 30 |

**The calibration does not transfer**: laneAS worker C built these rows from
*vtable slots of classes we actually compile*, so every name has a definer. The
gate can still fail — it returns MULTI_DEFINER 43 times and NO_DEFINER 5,455
times across the whole map — so this is a real pass, not a vacuous one.

It also **independently corroborates** laneAS's two pre-verified pin targets from
a channel orthogonal to both the vtable evidence and the splits geometry:
`0x8275a898 ?Copy@Object@Hmx@@`, `0x8275a9d0 ?FindPathName@Object@Hmx@@` **and
`0x8275bd78 ?Handle@Object@Hmx@@` (a third in the same DirLoader interloper span
that laneAS did not call out)** all resolve to `system/obj/Object.obj`; and
`0x825d4608 ?IsActive@SortViewSetting@@` resolves to `meta_band/ViewSetting.obj`.

Tools: `/home/free/tmp/laneAV/definer_index.py`, cached index
`/home/free/tmp/laneAV/definers.json`, annotated evidence
`/home/free/tmp/laneAV/frag109_definers.json`.

## 5.5 The splits arms — where the +46 actually came from

### The 42 over-covering pins (worker B, `ebb363cd` → rebased `f2473e8a`, **+17**)
36 moved+mapped, 2 already correct, 4 refused. **40 of the 42 VAs were absent
from the map**, so a splits move alone converts nothing — every landed row is
move **and** map entry in one leg. `splits.txt` on main was unchanged since the
old base, so the delta applied verbatim; 35 of 36 map adds rebased clean (one
dropped: `0x82579f50 ?ContentDone@SongMgr@@`, already held by main at
`0x827a88c8`). The +17 reproduced *exactly* at the new base — the cross-check
that the rebase was faithful.

* ★**Spatial distance is not a rejection criterion.** 11 rows were deferred on
  claim-distance grounds; 5 were tested anyway and **all 5 flipped to 100 %** —
  at up to 1.9 MB. Adjustor thunks and small accessors are ICF-eligible and land
  wherever the surviving fold goes. Distance should **rank** multi-definer
  contests, never **reject** a unique-definer row. **Other lanes' distance-
  deferred rows are worth re-testing.**
* ★**A whole-island move can buy a fake match with a real one.** Leg 1 read +12
  with 2 losses, both named matches at map-known VAs inside wholesale-moved
  islands. One netted *exactly zero*: it destroyed a named match and created a
  new 100 % on an anonymous `fn_` that byte-paired in the destination obj — same
  count, less honest. Narrowing both moves to the single function recovered
  both. **`lane-ar-map-ownership-2026-07-26.md` §10 currently reads as though
  "sandwiched by the claimant on both sides" licenses a wholesale range move; it
  does not** — the sandwich establishes the span's *ownership*, not transfer of
  the named matches inside it. That wording is worth correcting.
* ★**4 laneAS rows refuted, all one shape:** the VA is already held at 100 % by a
  *different* name (`?GetProfile@Tour@@` `0x8235bb28`,
  `?Ranked@MatchmakingSettings@@` `0x823e2ae8`, plus duplicate-name cases at
  `0x82317a88`, `0x823af200`). **Byte identity of the incumbent beats vtable
  anchors**, and anchor count (3-5 here) does not rescue them.

### The 67 unpinned vtable VAs (worker D, `a917302a` → rebased `9a1252b6`, **+29**)
3 refused as stale (already pinned by a concurrent lane — laneAS's `route`
snapshot is out of date), **64 pinned** via 51 new `.text` ranges + 49 rebased
map entries; 44 strict, 20 scored. Plus one carried source fix:
`Accomplishment::InitializeTrackerDesc` store order (hoist `Symbol nm = mName`
above the `unkc` store) 99.90 % → **100 %** — laneAO's pin-then-read-the-diff
lesson reproducing exactly.

* ★★**Reloc-masked byte equality is near-vacuous below ~32 B — and worker D
  refuted its own tool for it.** It built the byte-equality upgrader, validated
  it (297/299 known-strict controls), got "40/64 byte-equal", then killed it:
  indexing every (size, masked-body) pair over all **154,948** compiled `.text`
  symbols shows the 12-byte MSVC adjustor-thunk body is shared by **1,673
  distinct symbols**, the `Handle` variant by 590, a 4-byte `blr` body by
  **2,309**. It confirms *shape*, not identity. **Anyone planning to "upgrade
  candidates with byte equality" must gate on pattern multiplicity first.**
* **What discriminated instead, at the same build cost:** decode the retail
  `b`/`bl` at the VA, resolve it through `target_symbol_map` + `symbols.txt`, and
  compare against the reloc symbol our obj points at. Measured flips:
  BR-MATCH ≤68 B **16/18** · leaf/no-branch ≤68 B **4/4** · BR-MISMATCH-but-ICF-
  explained ≤68 B **3/3** · BR-UNRESOLVED ≤68 B 8/18 · **>68 B any class 1/19**.
  The 68-byte rule held almost exactly; the one >68 B flip was an
  extent-vs-body sizing artefact (36 B body inside an 84 B extent).
* Two BR-MISMATCH rows were **ICF artefacts, not misassignments**
  (`?Main@ObjectDir@@` / `?Current@MetaPerformer@@` are byte-identical; `??_E`/
  `??_G` destructors fold likewise). `icf_aliases.map` carries only 7 entries, so
  branch-target comparison needs its own fold check.

### ★ Composition: the merge that `git apply --3way` could not do
B's delta *moves* ranges; D's delta only *adds* them (D did this deliberately so
a concurrent `splits_move.py` could not collide). A plain `git apply --3way` of
D onto B **conflicted**. `scripts/harvest/splits_additive_merge.py` (new) parses
both files structurally and copies only the `(unit, section, start, end)` ranges
the target lacks — never removes, never edits an endpoint, and **refuses on any
interval overlap**. 72 ranges added, 0 removed, D's 51 new ranges overlapping the
landing tree in **0** places. This is the safe primitive that `land.sh`'s
dict-union is not.

★**`.pdata` must be carried with `.text`.** A `.text` move silently orphans the
donor's `.pdata` for the moved functions — invisible in `matched_functions`, but
it leaves donors owning unwind records for code they no longer contain. The
landed delta carries it (60 `.pdata` lines added / 23 removed alongside 82/27 for
`.text`), and `overlap_check` is clean on **both** sections.

## 6. Handoff: 46 named methods retail has and our tree never emits

Applying the definer index to the map itself: **5,455 of 25,166 entries name a
symbol no compiled obj defines.** ★**We first reported that as a 4,520-row source
worklist and it was wrong** — ~96 % of it is vendor shader-compiler/XGRAPHICS
classes inside the hard-skipped band (`Compiler` 783, `XGRAPHICS` 606, `xWMA`
204, `D3DXShader` 166 …). With the vendor band excluded the residue collapses to
**167**, of which the actionable slice is **46 methods whose class we DO
compile** — the generalisation of laneAR §9 item 6 (`ObjRefConcrete::Replace`).

List: `/home/free/tmp/laneAV/source_missing_methods.json`. Concentrated in
`PlatformMgr` (9), `DxTex` (5), `DxMesh` (4), `DxRnd` (4), `AppLabel` (3).
45 of the 46 appear in the report as scored-but-not-strict, 1 is absent entirely.

**Caveat before anyone funds it:** many are private (`IAA`/`AAA` — `DxMesh::Fill`,
`RndParticleSys::InitParticle`, `CharIKFoot::DoFSM`), which is more likely an
`/Ob2` inline-policy difference than a genuinely absent body. The public `QAA`
ones (`PlatformMgr::IsInParty`, `::SetScreenSaver`, `::InviteParty`,
`BandUser::HasAsFriend`, `BandProfile::HasFinishedCampaign`) are the credible
absences.

## 7. Refuted this lane

* ★**Ours:** "the mis-skipped vendor-window pool is an unusually rich un-picked
  pocket." **1.34 % EXACT_UNIQUE vs 3.09 % in the already-mined region** — it is
  *poorer*. Correctness fix, not a vein (§3.1).
* ★**Ours:** a "+4 ceiling" for `_bijection_arbitrary`, computed from
  `fuzzy_match_percent`. The right field gives **+2** (§1.1).
* ★**Ours:** "4,520 map entries are a source-missing worklist." Vendor-band
  contamination; the honest number is **46** (§6).
* ★**Ours (strong prior, stated then measured):** "most UNPINNED VAs will have no
  definer." **0 of 109** have no definer (§5).
* ★**laneAS §13.2's "real splits defect"** — a TU0-era stale artifact, 0 defects
  (§4).
* ★**Ours:** "≤16 B `_bijection_arbitrary` entries should mostly be refused
  because byte identity is vacuous there." Refuted by worker A — the vacuous
  thing at 12 bytes is *byte* identity, not *relocation* content, and the ≤16 B
  band measured **best** (89.5 %) (§2.3).
* **The assumption that byte-identical-class members are freely interchangeable.**
  They are for scoring, but the *set* is not freely splittable: applying 31 of a
  34-cycle permutation cost −1 (§2). Consistent with the fleet finding that 113
  target symbols are reloc-masked byte-EQUAL to their mapped base symbol and
  still score below 100 % — **objdiff's normalised diff is strictly stronger than
  masked byte equality.**

## 8. Residue, named

Whole-binary composition on the clean merge-base tree, scored correctly:

| bucket | n |
|---|--:|
| STRICT | 37,619 |
| anonymous in `auto_*` units (cannot score — no `base_path`) | 14,336 |
| anonymous in pinned units, outside the window | 9,127 |
| named in `auto_*` units | 5,392 |
| named in pinned units, not strict (body divergence) | 2,324 |
| **anonymous in pinned units, inside the window** | **599** |

Anonymous non-strict in pinned in-scope units by size band — the funclet-pairing
cliff, reproduced: 0-16 B 710 zero / 6 scored · 17-32 B 633/144 · 33-44 B
499/2,003 · 45-68 B 386/125 · 69-84 B 418/4 · 85-128 B 1,225/**0** · 129-256 B
1,281/**0** · 257+ B 1,709/**0**. ★**4,215 anonymous VAs at ≥85 B read exactly
0.0 % and not one is scored** — the entire >84 B supply, reachable only through
body-identity evidence.

Also open:
1. **201 duplicate names across 481 VAs** (266 report rows scoring, **210 not**,
   5 VAs with no row). laneDUPNAME's channel; not worked here.
2. **The 7 orphaned repairs** of §2 — need their cycles re-derived against
   current main.
3. ★**The four `Track` accessors at 96.5-97.1 %** — `GetPlayerName`,
   `PlayerDisconnected`, `HasLocalPlayer`, `PlayerDisabled` all sit behind **one
   shared mismatch**: retail `cmplwi cr6, r11, 0x0` vs our `cmpwi` on the null
   test of `user->GetPlayer()`. `!= NULL` and a hoisted `Player *player` local
   both fail to move it (tried and reverted). **Four strict matches sit behind
   whichever source form makes MSVC pick the unsigned compare** — worth a
   `docs/decomp/patterns/` entry if someone cracks it.
4. **83 VAs converted 0.00 % -> scored**, a ready body-port worklist:
   `?Poll@CharMirror@@` 98.97 · `HasLocalPlayer@Track` 97.14 ·
   `PlayerDisconnected@Track` 96.67 · `GetPlayerName@Track` /
   `PlayerDisabled@Track` 96.47 · `?PollDeps@CharWeightSetter@@` 95.39 ·
   `Load@JoinRequestMsg` 95.12 · `?Poll@CharSleeve@@` 92.92 ·
   `?FindPathName@Object@Hmx@@` 90.56 · `SetPitchBend@BeatMatcher` 90.00 ·
   `setCoefficients@FIRFilter` 87.37 · `?Poll@CharForeTwist@@` 82.14 ·
   `?Handle@Object@Hmx@@` (2,660 B) 80.24. Tail down to `Save@Fader` 3.57 (our
   body is a 4-byte stub vs retail's 112 - a pure body-port lead). The three
   lowest from the pins arm (`?Handle@UIScreen@@` 1.95, `?ContentDone@SongMgr@@`
   0.98, `?Poll@SetlistToStorePanel@@` 0.33) cost nothing but are **not
   corroborated by their score** - treat their attribution as unproven.
5. **The 314 `_bijection_arbitrary` entries at ≥69 B** — the only decidable arm,
   and the reloc-content discriminator (`scripts/harvest/reloc_disc/`, imported
   from `laneAS-B` where it had never landed, now on branch `laneAV-A`) is the
   tool for it. Ceiling remains +2 strict; fund for correctness only.
6. **12 files hardcoding the vendor VA window**, two of them unbounded and one
   with a different upper bound (§3).

## 9. Reproducing

```bash
scripts/setup_worktree.sh ~/tmp/wt-X laneX
cd ~/tmp/wt-X && git checkout -- config/45410914/symbols.txt \
  && touch config/45410914/config.yml \
  && rm -f build/45410914/{report.cache,target_symbol_renames.stamp} \
  && ./tools/ninja-locked            # MANDATORY before any obj-reading scan
python3 scripts/harvest/map_line_splice.py scripts/target_symbol_map.json plan.json
python3 /home/free/tmp/laneAS/strictdiff.py snap build/45410914/report.json after.json
python3 /home/free/tmp/laneAS/strictdiff.py diff before.json after.json
```
