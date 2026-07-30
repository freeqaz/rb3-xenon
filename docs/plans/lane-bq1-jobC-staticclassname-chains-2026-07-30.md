# Lane BQ-1 · Job C — BP-7's 19 open StaticClassName chains, adjudicated

Date: 2026-07-30 · branch `laneBQ1` · worktree `~/tmp/wt-bq1`

BP-7 opened the `StaticClassName`-literal channel, applied the 5 rows that formed
**closed permutation cycles** (`DxMesh/DxEnviron/DxCam`, `DxMovie/DxCubeTex`), and
handed on **19 open chains** with the correct warning that "their proposed name
currently sits on a VA outside the contradict set, so moving it could break a
correct pairing."

Re-running `staticclassname_literal_scan.py` against the current map (BP-7's and
BP-6's rows have since landed, and Job A of this lane fixed one) gives
**300 AGREE / 18 CONTRADICT / 3 NO_SOURCE_LITERAL**; Job A resolved `0x82319950`,
leaving **17** live.

**Applied 2. Reclassified 2 as not-a-map-defect. Blocked 13, each with its
blocker and, where identifiable, its carve target.** Plus the BQ-2 fold-in (§5).

| | predicted | measured |
|---|---|---|
| matched | +1 | **+1** (40888) |
| masked_equal | 0 | **0** (1509) |
| honest | +1 | **+1** (39379) |

Per-row outcome: `0x8240e0c0`→`RndMovie` **100%** and `0x8240e240`→`RndMultiMesh`
**100%** (both metric-neutral, as designed — the VAs keep their credit under the
correct name); `0x82606008` thunk **100%** (the +1); `0x826059e8`
`?SetType@BandStorePanel@@` **99.85%**.

That 99.85% is better than predicted and worth flagging: I forecast a clear
partial from a 356-vs-316 byte gap, but 99.85% over a 316-byte body is roughly a
**single differing instruction**. The 356 was again my dumper reporting the COMDAT
*section* size rather than the body extent (same error as Job A — noted twice
now, do not size-predict from that field). This is a prime S≤2 near-miss and the
best available probe for the `OBJ_SET_TYPE` double-definition defect
(`Object.h:948` vs `ObjMacros.h:30/47`), since it is now pinned, named and
one instruction from matching.

---

## 1. The gate that actually binds is COMDAT availability, not name occupancy

Measured first, before proposing anything: **all 17 are at 100.0%, and in all 17
the incumbent name is a real COMDAT in the obj of the unit that owns the VA.**
They match because every `OBJ_CLASSNAME` body is identical machine code apart
from the relocation supplying the string, and objdiff runs
`functionRelocDiffs=None`.

The consequence is sharp: repointing a VA to a name that is **not** a COMDAT in
that same obj does not "correct" it — it unpairs it and trades a false 100% for
an honest 0%, at −1 each. So the operative test is not "is the target name free"
but "does the owning unit's obj supply that COMDAT". Of the 13 blocked rows,
**11 fail on COMDAT availability**, not on occupancy.

That is the answer to the brief's question *"pin or carve?"* — for this channel
it is almost always **carve**: the VA is physically sitting inside the wrong
unit's span, and no map edit alone can fix it.

## 2. A false-positive mode in BP-7's channel: sometimes OUR SOURCE is wrong

BP-7's channel assumes a CONTRADICT means the *map* is wrong. It can equally mean
**our header's `OBJ_CLASSNAME` literal is wrong** — the map row is correct and the
scan is comparing against a bad expectation.

Discriminator (oracle-free): a class's `ClassName()`, `SetType()` and `Init()`
call **their own** `StaticClassName()`. Read those callers out of the linked
band.exe. If they name the *same* class as the incumbent, the map is right.

| VA | incumbent (map) | callers say | literal retail builds | our header |
|---|---|---|---|---|
| `0x824d1848` | `NgSpotlightDrawer` | `ClassName@NgSpotlightDrawer`, `SetType@NgSpotlightDrawer`, `Init@NgSpotlightDrawer` | `"SpotlightDrawer"` | `src/system/world/SpotlightDrawer_NG.h:34` `OBJ_CLASSNAME(NgSpotlightDrawer)` |
| `0x8256e688` | `AppInlineHelp` | `ClassName@AppInlineHelp`, `SetType@AppInlineHelp` | `"InlineHelp"` | `src/band3/meta_band/AppInlineHelp.h:10` `OBJ_CLASSNAME(AppInlineHelp)` |

Job A found the identical pattern a third time
(`src/band3/meta_band/AppMiniLeaderboardDisplay.h:38` declares
`OBJ_CLASSNAME(AppMiniLeaderboardDisplay)` where retail builds
`"MiniLeaderboardDisplay"`).

**The rule, stated generally: retail's DTA-visible class name drops the
platform/app prefix** (`Ng…`, `App…`); the prefix is a C++-side convention only.
Our headers wrote the C++ class name into `OBJ_CLASSNAME`. This also explains why
two distinct VAs can build the same literal without ICF folding them — they are
two different classes' COMDATs with different static-`Symbol` caches, so their
relocations differ.

**No map edit for these two.** The fix is a one-token source change per header,
and it is **metric-invisible** (the literal reaches the body only through a
relocation objdiff masks), so it prices at 0 and belongs to a source lane, not
here. Filed with exact file:line above.

## 3. Applied (2) — both in `system/rndobj/Rnd.cpp`, both metric-neutral

| VA | old (BP-7 phantom class) | new | why it is uniquely determined |
|---|---|---|---|
| `0x8240e0c0` | `BaseMaterial` | `RndMovie` | builds `"Movie"`; the only two declarers are `DxMovie` and `RndMovie`; `DxMovie` is already pinned at `0x82739288` in the `0x827xxxxx` **rnddx9** layer (BP-7's own 2-cycle), this VA is at `0x8240xxxx` in the **rndobj** layer owned by `Rnd.cpp`, and `?StaticClassName@RndMovie@@` is a real COMDAT there and FREE |
| `0x8240e240` | `MetaMaterial` | `RndMultiMesh` | builds `"MultiMesh"`; `DxMultiMesh` already at `0x8273f110` in the rnddx9 layer; `?StaticClassName@RndMultiMesh@@` is a real COMDAT in `Rnd.obj` and FREE |

Both incumbents (`BaseMaterial`, `MetaMaterial`) are BP-7 Part C confirmed
**DC3-only phantom classes**, so neither can be correct. Both VAs keep their
100% under the new name — the change is metric-neutral by construction and buys
correctness only.

`RndMultiMesh` did not appear as a candidate in the scan's own index at all:
`build_lit_index`'s regex only captures classes whose body it can parse, so it
silently drops some declarers. Verified directly against `Rnd.obj`'s symbol table
instead. **Anyone re-using that index should treat "no candidate" as "unknown",
not as "no such class".**

### Compliance with lane BQ-2's refutation

BQ-2 refuted BP-6's `RndScreenMask` remap and generalised: OBJ_SET_TYPE bodies
are byte-twins modulo relocations and the discriminating `StaticClassName` callee
*is* a relocation, so byte-locate/homing hits on them are unreliable — check the
`StaticClassName` callee in band.exe.

Both applied rows comply, and deliberately do **not** rest on a caller's map
label. `?SetType@RndMultiMesh@@` calling `0x8240e240` is recorded as
*corroboration only*, because that name is itself a byte-twin-class label of
exactly the kind BQ-2 warns about. The load-bearing evidence is the **literal read
out of the linked band.exe** (a resolved relocation), plus COMDAT availability in
the owning unit, plus renderer-layer spatial consistency.

## 4. Blocked (13), with blockers

| VA | true owner (literal + callers) | owning unit | blocker / what would close it |
|---|---|---|---|
| `0x8231f680` | **ScoreDisplay** (`ClassName@`, `SetType@`, `Init@ScoreDisplay` all call it; builds `"ScoreDisplay"`) | `LiveCameraInput.cpp` | no ScoreDisplay COMDAT in that obj. **CARVE it into `ScoreDisplay.cpp`.** Then a clean 3-cycle exists: `0x8231f680→ScoreDisplay`, `0x8256e7a8→AppScoreDisplay`, `0x8256e828→AuditionSessionPanel`. Best next target in this channel. |
| `0x8256e828` | `"AuditionSessionPanel"` | `MetaPanel.cpp` | tail of the same 3-cycle; no declarer found for the literal in our tree |
| `0x8227a1a8` | **BandCamShot** (`ClassName@`/`SetType@BandCamShot` call it) | `BandCamShot.cpp` | COMDAT present, but the name is held by `0x8229ce38`, whose own chain dead-ends (below) ⇒ correcting the whole chain costs −1 |
| `0x8229ce38` | `"Mat"` | `OutfitConfig.cpp` | `RndMat` COMDAT present here but occupied by `0x827347d0`, which cannot vacate |
| `0x827347d0` | `"Tex"` | `system/rnddx9/ShaderMgr.cpp` | **hard dead end** — `DxTex` and `RndTex` are both occupied *and* neither has a COMDAT in `ShaderMgr.obj` |
| `0x82369ba8` | **CharBone** (`ClassName@`/`SetType@CharBone` call it) | `Char.cpp` | COMDAT present; name held by `0x8227acc8` |
| `0x8227acc8` | `"Label3d"` | `CharBoneDir.cpp` | **no class in our tree declares `OBJ_CLASSNAME(Label3d)`** — open-ended; needs the class to exist before the chain can close |
| `0x8236ac28` | `"CharPollGroup"` | `Char.cpp` | name held by `0x824089d0` |
| `0x824089d0` | `"Environ"` | `ContentMgr_Xbox.cpp` | `DxEnviron`/`RndEnviron` both occupied, neither has a COMDAT here |
| `0x822dc7b0` | `"Mesh"` (caller `ClassName@DxMesh`) | `FlowOnStop.cpp` | `DxMesh`/`RndMesh` both occupied, no COMDAT in `FlowOnStop.obj` |
| `0x82741040` | `"Mat"` (caller `ClassName@DxCubeTex`) | `system/rnddx9/CubeTex.cpp` | `RndMat` occupied; `DxMat`/`NgMat` free but have no COMDAT here |
| `0x82b86e70` | `"Fur"` (caller `ClassName@NgFur`) | `Env_NG.cpp` | `NgFur`/`RndFur` both occupied |
| `0x82b86f78` | `"DOFProc"` (caller `ClassName@NgDOFProc`) | `Env_NG.cpp` | `DOFProc`/`NgDOFProc` free but neither has a COMDAT in `Env_NG.obj` |

Note the recurring shape in the last four: the caller names class *X* while the
body builds the literal of class *Y* one step along the chain. That is the
signature of a **whole cluster shifted by one slot**, not of independent
one-off mispairs — consistent with BP-4 §4's "cluster shift" diagnosis for
PropAnim. Closing them wants a cluster-level fixed point, not row-by-row edits.

## 5. Folded in from lane BQ-2 (coordinator hand-off)

BQ-2 identified `0x826059e8` as `?SetType@BandStorePanel@@` and noted its `??_G`
/ `??_D` rows were blocked on carves — this lane's remit.

Re-verified with BQ-2's own prescribed check rather than accepted: `0x826059e8`
is a 316-byte OBJ_SET_TYPE body (`types`/`objects` fingerprint) that `bl`s
`0x8256ea58`, and `0x8256ea58`'s body **itself builds the literal
`"BandStorePanel"`** — so the identification terminates on a literal, not on
another byte-twin's map label.

BQ-2 recorded the VA as pinned; it is **not** — `BandStorePanel.cpp`'s spans stop
at `0x82605720` and resume at `0x82606260`, leaving `0x826059e8` in an unpinned
gap. A map row alone would therefore have scored nothing. Carved both the body
and its adjustor thunk in (free upside — no other unit owned them):

```
.text 0x826059E8–0x82605B24   ?SetType@BandStorePanel@@              (retail 316B vs our 356B -> partial)
.text 0x82606008–0x82606014   ?SetType@BandStorePanel@@$4PPPPPPPM@A@ (12B, masked-equal -> +1)
```

Our 356-vs-316 byte gap on the main body is the known **OBJ_SET_TYPE
double-definition** defect (`Object.h:948` vs `ObjMacros.h:30/47`), so it lands
as honest partial credit rather than a match.

**Declined, with reason:** BQ-2's three `??_G`/`??_D` proposed-holds
(`0x824816a8` in `ScreenMask.cpp`, `0x8248b930` in `MeterDisplay.cpp`, and the
`??_DMeterDisplay`/`NewObject@BandPreloadPanel` rows). None intersects any span
this lane touched, and each needs its own carve **plus** the COMDAT-availability
check before the rename can score rather than trade a false 100% for a 0%. They
are a coherent next lane, not a fold-in.
