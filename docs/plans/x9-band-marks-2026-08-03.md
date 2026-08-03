# X9 — the band renders on its shipped marks; the wall was a guard copied from a container it doesn't describe

**Date:** 2026-08-03
**Predecessor:** [X8](x8-band-render-2026-08-03.md) "the band's wall was `InlineProxy` bypassing a virtual, and the next wall is 248 dead dispatch keys"
**Branch:** `x9-band-marks`, rebased onto `main` @ `671a94d5`
**Engine:** `milo-native-engine` pinned at **`138e1606…`**, **zero engine edits**
**Change surface:** one shared TU (`obj/ObjPtr_p.h`, hunk inside `#ifdef HX_NATIVE`) and two native driver files (`main_render.cpp`, `milo_object_factories.cpp`).

---

## Verdict

★★★ **THE BAND RENDERS ON ITS SHIPPED MARKS. Four members, four distinct world
positions, every one matching the venue's authored slot transform to every
printed digit — in two different venues.** Not one position was computed,
interpolated or hand-picked. §2

★★ **X8's wall took one line, and the reason it stood is that its justifying
comment described a different container.** `obj/ObjPtr_p.h`'s `gInReplaceList`
erase-suppression was copied into `ObjPtrList` from `ObjPtrVec`, where it is
real. In `ObjPtrList` neither stated hazard can occur. Restoring rb3-Wii's
unconditional erase turned `rc=139` into `rc=0` and the shipped `enter_venue`
path now runs end to end. §1

★★ **BOTH of X8's flagged-not-asserted items were artifacts of the blocked
path, and both are now gone.** The "four members share ONE 140-mesh set" is
false (160/169/162/160, each with its own `outfit`/`instrument` subdir), and
`skinned=0` is false (19/27/22/18). §3

⚠ **THE BAND IS PLACED, NOT YET FULLY DRAWN — stated plainly, because that is
the honest state.** Each member draws ~9 meshes; one member reads as a complete
outfitted figure in the frame, the others contribute instruments. The remaining
gap is named to a file and a class: nine **SHOWN-BUT-EMPTY** meshes per member,
all head/hands, gated by an unregistered `OutfitConfig`. §4

★★ **The cross-repo dead-key question is answered: NO.** rb3-Wii does not share
X8's defect class — verified with an instrument that first reproduces X8's
xenon population exactly (139 + 109 = 248) and a positive control that finds
**227** live dead keys in xenon and **zero** in rb3-Wii. §6

★ **X6's SHA table is vindicated a THIRD time**, independently: this lane's
default frame `cmp`-matches X6's, X7's *and* X8's artifacts, and both
instruments agree on `5282bd275159f10b`. §5

---

## 1. ★★ The wall: an erase-suppression copied from a container it does not describe

### 1.1 What X8 handed over

> `obj/ObjPtr_p.h:777-789` suppresses an `ObjPtrList` erase while
> `gInReplaceList`, leaving a NULL in a `kObjListNoNull` list.
> `BandCharacter::SyncObjects`'s shipped loop dereferences it, and the entry
> never leaves, so a naive null-skip would spin forever.

Every part of that is correct as an *observation*. Reproduced first, before
touching anything (`x9-run-place-baseline.log`): `rc=139`, and the suppression
warning fires exactly twice — on `player0` and on `FileMerger.fm` — immediately
before the SIGSEGV.

### 1.2 The charter's rule, applied: read the oracle before unblocking

The three trees do **not** agree about what an `ObjPtrList` *is*, and that is
the whole finding.

| tree | `ObjPtrList::Node` | is the Node a ring member? |
|---|---|---|
| rb3-Wii (`rb3/src/system/obj/ObjPtr_p.h:230-279`) | `{obj, next}`, plain | **No** — the LIST is the single ring-ref |
| rb3-xenon X360 arm (`Object.h:1039-1050`) | `{mObject, next, prev}` = 0xc, no vtable | **No** — same, list-as-ref |
| rb3-xenon **native** (`Object.h:1000-1031`) | `: public ObjRefConcrete<T1,T2>` | **Yes** — each Node is its own ring entry |

rb3-Wii's `Replace` erases **unconditionally** (`:266-268`):

```cpp
if (mMode == kObjListNoNull && !to) { it = erase(it).mNode; continue; }
```

There is no guard in the oracle, and there is nothing in the oracle for the
guard to be protecting — its nodes never touch a ring.

### 1.3 ⛔ Both stated hazards are false for `ObjPtrList`

The suppression's own comment gave its provenance in its last line: *"Matches
the guard in `ObjPtrVec::ReplaceNode`."* It was reasoned by analogy, and the
analogy does not hold.

**Hazard 1 — "erasing shifts nodes and corrupts the ring walk."** This is the
`ObjPtrVec` comment (`:538-549`), and there it is **REAL**: that container's
Nodes live *inline in a `std::vector`*, so `erase()` memmoves survivors via
`CopyRef`, which rewrites live ring `prev`/`next` under an in-progress walk.
`ObjPtrList`'s Nodes are individually heap-allocated (`Node::operator new` →
`PoolAlloc`) and are never moved. The mechanism cannot occur.
**`ObjPtrVec`'s guard is left untouched.**

**Hazard 2 — "erasing frees a node other ObjRefs still point to."** Also false
*here*, and the disproof is three lines away in the same header. `old` is
`SetObj`'s return, and `SetObj` returns the **new** referent
(`ObjPtr_p.h:143-153`, `return mObject;`). So reaching the erase arm means
`SetObjConcrete(nullptr)` has **already** run `mObject->Release(this)` and
unlinked this node from the ring. By the time `erase()` runs the node holds no
ring membership at all; `delete node` reaches `~ObjRefConcrete` with
`mObject == nullptr`, which performs **no ring operation** (`:34-43`).

### 1.4 Result

Restored the unconditional erase. **`rc=139` → `rc=0`**, and
`BandWardrobe::OnEnterVenue`'s two shipped statements (`LoadCharacters` +
`SetVenueDir`) run end to end for the first time.

| | X8 default | X8 forced-show | **X9 `RB3_BAND_PLACE=1`** |
|---|---|---|---|
| scene meshes | 320 | 320 | **411** |
| skinned | — | — | **134** |
| textured | — | — | **341** |
| draws | 114 | 148 | **203** |
| coverage | — | — | **38.92 %** |

---

## 2. ★★★ The acceptance evidence: members, distinct positions, and the shipped slots

`small_club_01`, `coop_bg` (`--dump-tree`, `x9-run-place-dumptree.log`). The
driver prints the shipped `BandConfiguration` rows and the measured world
positions **from two independent passes over the scene**, so they can be put
side by side:

| slot (mode 0) | shipped transform | member | **measured world pos** |
|---|---|---|---|
| 0 `player_bass0` | (-103.728…) → (-70.003, 80.657, 13.495) | `player0` | **(-70.00, 80.66, 13.50)** |
| 1 `player_drum0` | (14.429, 146.133, 13.182) | `player1` | **(14.43, 146.13, 13.18)** |
| 2 `player_guitar0` | (68.770, 51.436, 13.248) | `player3` | **(68.77, 51.44, 13.25)** |
| 3 `player_vocals0` | (-10.026, 31.389, 13.218) | `player2` | **(-10.03, 31.39, 13.22)** |

**4 members, 4 DISTINCT world positions, 4/4 matching.** All four z within
13.18–13.50 — coplanar on the stage floor, exactly X7's census. Each member's
`outfit` and `instrument` subdirs report the member's own position, i.e. they
are correctly parented.

### 2.1 ★ `arena_01`: the drummer is on the riser, at the height X7 measured independently

Second venue, same run shape (`x9-run-arena01.log`), and this is the strongest
single piece of evidence in the lane because the number was predicted by a
different lane from the asset alone:

| member | measured z |
|---|---|
| `player0` bass | 255.83 |
| `player2` vocals | 255.79 |
| `player3` guitar | 256.12 |
| **`player1` drums** | **320.90** |

**320.90 − 255.83 = +65.07** — X7 measured the shipped `arena_01` drum riser at
**+65** from the asset, months before anything could render it. Three members
coplanar to 0.33; the drummer 65 units up. All four match `arena_01`'s own slot
rows to every printed digit (`x9-run-arena01.log:5373-5384`).

**Eight slot matches across two venues.** ⛔ **NOT SUBSTITUTED — PLACEMENT.**
No position in this lane was computed, interpolated or hand-picked.

---

## 3. ★★ X8's two flagged items: both were artifacts of the blocked path

X8 flagged these rather than asserting them, which was right — and unblocking
§1 resolved both without any further change.

| X8 flag | X8 measurement | **X9 measurement** | verdict |
|---|---|---|---|
| "the four members SHARE one 140-mesh set" (320 collected, not 180+4×140) | one shared set | **160 / 169 / 162 / 160** — different per member, each with its own `outfit`/`instrument` subdir at its own position | ⛔ **REFUTED** |
| "`skinned=0` on band meshes vs `skinned=6` on crowd" | 0 | **19 / 27 / 22 / 18** per member; 134 scene-wide | ⛔ **REFUTED** |

Both were consequences of the outfit/LOD recompose never running.

---

## 4. ⚠ What did NOT land: the band is placed, not fully drawn

Each member reports `showing=19–29`, `verts>0=39–44`, **`DRAWABLE=5–9`**. In the
frame, **one** member reads as a complete outfitted figure (jacket, jeans, hair
— textured, not white); the others contribute instruments (two guitars and a
drum kit are visible). The frame-to-frame diff vs the default is confined to
(708,444)–(942,603).

**An aggregate cannot say why**, which is the charter's standing warning, so
this lane added a diagnostic that names the two disagreeing populations rather
than counting them (`main_render.cpp`, `--verbose`):

```
player0  SHOWN-BUT-EMPTY (9): fingernails_resource.mesh tongue.mesh head.mesh
                              upperteeth.mesh eyebrows1_resource.mesh
                              youngozzie_resource.mesh hands_naked.mesh
                              eyes.mesh lowerteeth.mesh
         FULL-BUT-HIDDEN (34): male_neck_ao.mesh male_tattoo_torso.mesh
                              female_tattoo_legs.mesh male_placement_*.mesh
                              female_placement_*.mesh female_ao_seams.mesh …
```

- **`SHOWN-BUT-EMPTY` is the defect.** These are exactly the head/hands set. The
  recompose **selects them correctly** — `Showing()` is true — but they carry
  zero vertices.
- **`FULL-BUT-HIDDEN` is believed CORRECT and is flagged, not asserted.** It is
  entirely `male_*`/`female_*` tattoo, ambient-occlusion and placement decals; a
  male member should not show `female_tattoo_legs`.

### 4.1 ⛔ The next wall, named — and a negative result worth more than the change

Alongside those nine meshes the log emits **`Can't make OutfitConfig` 40 times**
— once per head / hands / hair / facehair / eyebrows resource milo — because
`OutfitConfig::Init()` (`bandobj/OutfitConfig.cpp:404-409`) is never called, so
its `Register()` never runs. That is the **FIFTH** instance of the factory-list
drift `milo_object_factories.cpp`'s own header warns about (X7 found the third,
X8 the fourth).

**Adding the call does not link (rc=1), and that is the finding.** Referencing
`Init()` retains `OutfitConfig.cpp` sections needing `BandPatchMesh::{Render,
ReProject, PreRender, PostRender, Compress, ListDrawChildren}`, its
copy-ctor / `operator=` / `operator>>`, plus `gRB3OutfitComposeActive` and the
`Symbol recompose` — none defined in this target.

★ **Why, and it is cross-cutting:** `BandPatchMesh.cpp` is **not compiled
standalone**. It is **scatter-included into `src/system/world/LightPreset.cpp:1503`**
— an X360 **TU-packing decision made for objdiff SCORING** — and
`LightPreset.cpp` is not in `rb3-render`'s source list (only
`LightPresetManager.cpp` is). **An X360 match-build packing choice silently
decides what the NATIVE link contains.** This is X7's *"a stub's blast radius is
the source list it sits in"*, **inverted**: here the TU you need sits in a list
your target is not built from. Recorded in the file as a comment; the change was
reverted.

---

## 5. Gate results

| # | Gate | Result | Evidence |
|---|---|---|---|
| a | Full native gate, **fresh** (`rm -rf native/build`), rc=0, **0 SKIPs** | ✅ **PASS — 18/18** | `NATIVE GATE: PASS (rc=0, 0 errors, 0 warnings, 18/18 target(s) verified)`; all 18 `relinked this run`; zero SKIPs. Run **twice** — once fresh, once after the rebase. Cache seeded first with explicit `-DMILO_ENGINE_PATH=` + `-DDawn_DIR=` (§5.1). `x9-native-gate.log`, `x9-native-gate-rebased.log` |
| b | Zero `milo-native-engine` edits | ✅ **PASS** | engine `HEAD` == pin `138e1606a202f2b3226e38a8f28010b096f3d441`. ⚠ The foreign uncommitted edit to `src/platform/FxSendNative.cpp` disclosed by X4d, X5, X6, X7 **and X8** is **still there, still not mine**, still off the load/render path. Left untouched — **sixth lane running.** |
| c | Shared-`src/` edit `HX_NATIVE`-gated, X360 arm faithful | ✅ **PASS** | Full X360 build `rc=0`. Direct A/B (§5.2) — not inference. |
| d | PNG determinism ×2 on every cited image | ✅ **PASS** | both frames rendered ×2 → `cmp` identical; and re-rendered again **after the fresh gate rebuild** → still `cmp`-identical to the cited artifacts |
| e | Prior lanes' evidence non-regressed | ✅ **PASS — byte-identical** | `cmp` against X6's, X7's and X8's **artifacts**. §5.3 |
| f | Was `main` broken by a decomp lane? | ✅ **NO** | `main` @ `db8f09fc` then `671a94d5`; native build `rc=0` and X360 `rc=0` at both. |

### 5.1 ⚠ The gate's cache trap has a THIRD ingredient, and it cost this lane a build

X4c warned that the gate's own `cmake` line omits `MILO_ENGINE_PATH` and
`Dawn_DIR`, letting three targets silently SKIP while still reporting PASS. True
— but it also **sets the compiler** (`native_build_gate.sh:228`,
`-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++`), and a hand-rolled seed
configure that supplies only the two paths gets the system `g++`. That failed
with 104 errors that had nothing to do with this lane
(`unrecognized command-line option '-fms-compatibility'`) and looked exactly
like a broken `main`. **A correct seed needs all four flags.** Recorded because
the next lane will otherwise lose the same twenty minutes.

### 5.2 X360 blast radius — a header every target includes

`obj/ObjPtr_p.h` is a **header, not a scoreable unit**: it has no `splits.txt`
entry and **cannot be scored at all**. Saying so is not the same as implying a
match. What *is* measurable is its blast radius — every unit that includes it,
i.e. the whole build — and that was measured directly, twice:

- **Pre-rebase, against `db8f09fc`:** full X360 build `rc=0`, then a per-unit
  comparison of `report.json` across **all** units on
  `matched_code_percent` / `total_code` bytes / `matched_functions_percent`:
  **0 units changed.** Overall identical at **39.153187 % code /
  62.959976 % funcs**.
- **Post-rebase A/B:** rebuilt with `main`'s `ObjPtr_p.h` swapped in, then with
  mine, comparing the two `report.json`s directly. Result in §5.4.

### 5.3 Determinism and non-regression

Both cited frames rendered ×2 (`cmp` identical), then re-rendered a third time
**after the fresh gate rebuild** and still `cmp`-identical to the cited copies.

★ The default frame is **byte-identical to X6's, X7's AND X8's shipped
artifacts**, verified by `cmp` against the PNGs on disk (not against transcribed
hashes):

| frame | vs X6 artifact | vs X7 artifact | vs X8 artifact | sha256 | X6's recorded value |
|---|---|---|---|---|---|
| default (E0) | **IDENTICAL** | **IDENTICAL** | **IDENTICAL** | `5282bd275159f10b` | `5282bd275159f10b` ✅ |

⚠ This matters more than a routine non-regression check: my change is in the
**default** path too (the erase now runs unconditionally in every build), so a
perturbed default frame was a live possibility. It is unperturbed. **X6's SHA
table is now vindicated a third time**, and as X8 insisted, both my instruments
(`cmp` and `sha256sum`) agree with each other *and* with the document.

### 5.4 A/B result — direct, not inferred

Same tree, same `main` @ `671a94d5`, only `obj/ObjPtr_p.h` swapped, full X360
build each way (`rc=0` both):

| build | overall code | overall funcs | units differing |
|---|---|---|---|
| `main`'s `ObjPtr_p.h` | 39.154087 % | 62.961410 % | — |
| **this branch's `ObjPtr_p.h`** | **39.154087 %** | **62.961410 %** | **0** |

**Zero units changed**, compared per unit on `matched_code_percent`,
`total_code` **bytes** and `matched_functions_percent`. The X360 arm is
untouched — as it must be, since the hunk is inside `#ifdef HX_NATIVE` — and
this is a measurement of that, not an argument for it.

⚠ The earlier pre-rebase figure (39.153187 % / 62.959976 %) differs from these
only because `main` itself moved: `671a94d5` landed a decomp lane between the
two measurements. The A/B above is same-`main`-both-sides, which is the only
form that can answer the question.

---

## 6. ★★ The cross-repo question: does rb3-Wii share the dead-dispatch-key class?

**Answer: NO** — not in `src/`, not in `native/`, not in `milo-native-engine`.
A well-supported negative, not an absence of evidence.

The coordinator's own attempt failed instructively ("169 in rb3-Wii vs 223 in
xenon" — but the matches were struct *members* and function *locals*, never
file-scope dispatch keys). The fix was to **start from X8's own enumeration**
rather than invent a pattern.

**The instrument, and proof it reproduces X8's population.** X8's 248 are
textually `^Symbol <ident>;` at **column 0**:

```bash
grep -cE '^Symbol[[:space:]]+[A-Za-z_][A-Za-z0-9_]*[[:space:]]*;[[:space:]]*$' \
  rb3-xenon/native/src/milo_link_stubs.cpp   # -> 139
grep -cE '…same…' rb3-xenon/native/src/m6_symbols.cpp   # -> 109      = 248 ✓
```

The indent-tolerant variant reproduces the failed attempt exactly (164 in
`src/`) and sampling shows why: all members/locals. **Column 0 is the whole
discriminator.**

**Positive control** (the step that makes the negative trustworthy): intersecting
those 248 names against every bare-identifier first argument of a
`HANDLE*`/`SYNC_PROP*` macro in `rb3-xenon/src/` yields **227 live dead keys**
(`deploy`, `band_energy`, `crowd_rating`, `change_difficulty`, …). The
instrument demonstrably finds the defect where it exists.

**rb3-Wii, same instrument:**

| tree | file-scope default-ctor `Symbol` globals | any used as a dispatch key? |
|---|---|---|
| `rb3/src/` | **2** | no |
| `rb3/native/` | **1** | no |
| `milo-native-engine/` | **0** | — |

All three sampled and classified individually — `DataFile.cpp:57 gFile`,
`os/System.cpp:71 gSystemLanguage`, `native/src/rb3_game_input.cpp:280
gLastScreen` — each mutable runtime state assigned during execution, never a
`sym ==` comparand.

**Audited from the other direction too:** all **4002** distinct bare-identifier
dispatch keys in `rb3/src/`; **3997** resolve to an interned definition, and all
5 residuals are instrument artifacts individually run to ground (`sym`, `_s` are
`ObjMacros.h`'s own macro *parameters*; `exit`, `printf`, `remove` use the
`_STATIC` variants, which intern by stringification and never touch a global).
**Net: zero dead dispatch keys in rb3-Wii.**

**The structural reason.** rb3-Wii is equally susceptible *in principle* —
`rb3/src/system/obj/ObjMacros.h:120` has the byte-identical macro. What differs
is that rb3-Wii **has the definitions xenon lacks**: `src/system/utl/Symbols{,2,3,4}.cpp`
define **7146** interned symbols with **zero** default-constructed lines, their
headers are 100 % `extern Symbol` (no tentative definitions), and
`native/CMakeLists.txt:306` globs them into **every** native target. xenon ships
the `Symbols*.h` headers and **no** `.cpp`, which is why its 248 had to be
hand-defined at all.

⚠ **Scope limits, stated:** the key extractor captures only bare identifiers as
first macro argument — keys written `Symbol("foo")` or as member expressions are
out of scope, but those cannot exhibit this defect since they do not route
through a global.

---

## 7. Per-subsystem verdict table

| subsystem | verdict | evidence |
|---|---|---|
| **Band members on their SHIPPED MARKS** | ★★★ **VERIFIED — 4/4, two venues** | 8 slot matches to every printed digit; `arena_01` riser +65.07. §2 |
| **`ObjPtrList` NULL-under-`gInReplaceList` (X8's wall)** | ★★ **FIXED — one line** | `rc=139` → `rc=0`; guard was a false analogy from `ObjPtrVec`. §1 |
| **Shipped `enter_venue` path end-to-end** | ✅ **VERIFIED** | `LoadCharacters` + `SetVenueDir` run; 411 meshes / 203 draws / 38.92 % coverage. §1.4 |
| **Band members TEXTURED + OUTFITTED** | ✅ **YES, partially** | real recompose (militaryjacket, jeans, cowboyboots, youngozzy hair, lemmy facehair); one member reads as a complete figure. §4 |
| **Band members FULLY drawn** | ⚠ **PARTIAL — ~9 meshes/member** | 9 SHOWN-BUT-EMPTY head/hands meshes per member. §4 |
| **"four members share ONE mesh set" (X8 flag)** | ⛔ **REFUTED** | 160/169/162/160, own `outfit`+`instrument` subdirs. §3 |
| **"`skinned=0` on band meshes" (X8 flag)** | ⛔ **REFUTED** | 19/27/22/18; 134 scene-wide. §3 |
| **`OutfitConfig` registration** | ⛔ **BLOCKED — named to a line** | `Init()` does not link; `BandPatchMesh.cpp` scatter-included into `LightPreset.cpp:1503`, absent from this target. §4.1 |
| **rb3-Wii dead dispatch keys** | ★★ **ANSWERED — NO** | validated instrument + 227-key positive control in xenon, 0 in rb3-Wii. §6 |
| **`ObjPtrVec` erase suppression** | ✅ **CORRECTLY LEFT ALONE** | vector-inline nodes; `CopyRef` memmove hazard is real there. §1.3 |
| **`obj/ObjPtr_p.h` objdiff score** | ⛔ **NOT SCOREABLE AT ALL** | a header; no `splits.txt` entry. Blast radius measured instead. §5.2 |
| **Venue geometry / crowd / lighting / skinning** | ✅ **ALIVE, non-regressed** | byte-identical to X6/X7/X8 artifacts. §5.3 |
| **`BandConfiguration` objdiff score** | ⛔ **STILL NOT SCOREABLE** | no `splits.txt` entry. Carried from X6/X7/X8. |
| **109 dead keys in 7 other targets** | ⚠ **CARRIED** | `InternSymbolGlobals_M6Symbols()` still defined-and-uncalled. |
| **`BandCamShot` (611 misses); deform/skin stubs; impostor RTT; `video_05`; foreign `FxSendNative.cpp` edit** | ⚠ **CARRIED, untouched** | as before |

---

## 8. Retracted hypotheses, with evidence

1. ⛔ **X8's framing that the `ObjPtrList` guard protected against "real heap
   corruption" that had to be *reconciled* with the no-null invariant** — i.e.
   that this was a hard trade needing "its own lane, with an A/B on every prior
   frame." **Retracted.** There was nothing to reconcile: neither stated hazard
   can occur in `ObjPtrList` (§1.3), the oracle has no guard at all, and the
   removal perturbs **zero** prior frames (§5.3) and **zero** X360 units (§5.2).
   X8 was right to flag it and right not to guess; the premise it inherited from
   the comment was simply false.
2. ⛔ **My own first hypothesis: "the native `ObjPtrList` needs a null-skip, or a
   deferred-cleanup pass, because the entry never leaves."** **Retracted before
   any code was written**, by reading `SetObj`'s return semantics
   (`ObjPtr_p.h:143-153`). The entry never left *because the erase was
   suppressed*; nothing else was needed.
3. ⛔ **My own second hypothesis: "`OutfitConfig::Init()` is a one-line fifth
   instance of the factory drift, like X8's `BandCharDesc::Init()`."**
   **Retracted by measurement** — it does not link, for a reason that is a
   finding in its own right (§4.1). Recorded in the source rather than silently
   dropped.
4. ⛔ **The coordinator's own cross-repo grep ("169 in rb3-Wii vs 223 in
   xenon").** **Retracted and superseded** — those matches are struct members and
   function locals. The discriminator is **column 0** (§6).
5. ⚠ **Explicitly NOT claimed:** that unblocking `OutfitConfig` alone will make
   the band fully draw. It addresses the 9 SHOWN-BUT-EMPTY head/hands meshes and
   says nothing about the stubbed deform passes, `CharKeyHandMidi`, or
   `BandCamShot`.
6. ⚠ **Explicitly NOT resolved:** whether `FULL-BUT-HIDDEN (34)` is entirely
   correct. It *looks* correct (male/female tattoo + AO + placement decals) and
   is flagged, not asserted.
7. ⚠ **Explicitly NOT resolved (carried from X7/X8):** the band-vs-crowd z gap.
   `small_club_01` band z≈13.2–13.5 against crowd bboxes at z=69.5/74.5, band y
   positive vs crowd y negative. Untouched.

---

## 9. Owed work / handoff

| item | why | owner |
|---|---|---|
| ⛔ **`OutfitConfig` cannot be registered: `BandPatchMesh` is scatter-included into a TU this target does not build** | `LightPreset.cpp:1503`. An X360 objdiff **packing** decision determines the **native** link surface. Either add `LightPreset.cpp` to the target, or split the scatter, or stub `BandPatchMesh`'s 9 referenced members. This is what stands between a placed band and a fully-drawn one. | X10 |
| ⚠ **`FULL-BUT-HIDDEN (34)` unconfirmed** | Believed correct (male/female decals). One cheap check: confirm the shown/hidden split flips with member gender. | X10 |
| ⚠ **Generalize the scatter-include hazard** | If one X360 packing choice silently removed a class from the native link, others may too. A sweep of `#include "…​.cpp"` sites against each native target's source list would be cheap and is probably not a one-off. | build-system |
| ⚠ **109 dead dispatch keys in 7 other targets** | `InternSymbolGlobals_M6Symbols()` defined and uncalled. Unchanged from X8. **rb3-Wii is confirmed clear (§6) — do not re-audit it.** | build-system |
| ⚠ **`band.play_mode` has no shipped source natively** | Still read from `config/macros.dta` element 0 (`coop_bg`) and disclosed; `RB3_BAND_PLAY_MODE` reaches the other two. Unchanged from X8. | X10 |
| ⚠ **`RB3_BAND_PLACE` is still opt-in** | It no longer crashes. Per the ack rule (native render fixes default-ON with an opt-out once ON-vs-OFF evidence exists), this lane now HAS that evidence on both sides — but flipping the default changes every prior lane's baseline frame, so it is proposed, not taken. | X10 |
| ⛔ **`BandConfiguration` / `ObjPtr_p.h` not scoreable** | No `splits.txt` entry (a header cannot have one). Carried. | match lanes |
| **`BandCamShot` (611); 18 stubbed bodies; `ObjOwnerPtr` recurrence trap; impostor RTT; `video_05`; foreign `FxSendNative.cpp` edit** | All carried, untouched. | as before |

---

## 10. Recommended X10 shape

1. ★ **A comment that explains a guard is a hypothesis, not a citation — and
   "matches the guard in X" is the tell.** X8 said to grep the comment as well
   as the code. This lane's headline is the next step: when a comment says a
   guard *matches* another one, go read the other one, because the two
   containers may not be the same shape. Two of the three trees here disagree
   about what an `ObjPtrList` even is.
2. ★ **The disproof of an inherited wall is often three lines away in the same
   file.** `SetObj`'s `return mObject;` — 130 lines above the guard — was enough
   to retire both stated hazards without a build.
3. ★ **A build-system decision made for SCORING can silently determine RUNTIME
   linkage.** `BandPatchMesh` is absent from the native link because of an X360
   TU-packing choice made to help objdiff. Nothing in either build reports this;
   it surfaces only as an undefined reference when something finally references
   the class. Assume there are more.
4. **The band is placed. What remains is geometry, not placement.** For the
   first time in six lanes the open question is not "where do they go" — it is
   "why do nine correctly-selected meshes carry no vertices", and that has a
   named class and a named line.
