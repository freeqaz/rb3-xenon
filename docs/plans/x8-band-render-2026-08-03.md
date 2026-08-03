# X8 — the band's wall was never `SetupDir`: it was `InlineProxy` bypassing a virtual, and the next wall is 248 dead dispatch keys

**Date:** 2026-08-03
**Predecessor:** [X7](x7-band-on-stage-2026-08-03.md) "the band's stage positions were baked in the venue all along"
**Branch:** `x8-band-render`, from `main` @ `254e80bd`
**Engine:** `milo-native-engine` pinned at **`138e1606…`**, **zero engine edits**
**Change surface:** three shared TUs (`obj/Dir.cpp`, `obj/DirLoader.cpp`,
`bandobj/BandWardrobe.cpp` — every hunk inside `#ifdef HX_NATIVE`) and four
driver/native files (`main_render.cpp`, `milo_object_factories.cpp`,
`milo_link_stubs.cpp`, `m6_symbols.cpp`).

---

## Verdict

⛔ **BAND MEMBERS DO NOT RENDER IN THE DEFAULT BUILD. Stated first, because
that was the milestone.** The default frame is byte-identical to X6's. What
changed is that the reason is now a *named, measured, per-mesh* fact instead of
a wall.

★★★ **X7'S WALL IS RETRACTED, AND THE REAL DEFECT IS A ONE-LINE VIRTUAL-DISPATCH
BUG.** X7 named the blocker "a proxy class conversion desync": `chars.milo`'s
`player0` is declared `BandCharacter` but its proxied `char/main/main.milo`
"declares its root as `RndDir`", so `DirLoader::SetupDir:712-748` `ReplaceObject`s
a half-loaded dir mid-stream. **The premise is false and the mechanism is a
symptom.** `main.milo` declares **`BandCharacter`** (read from the asset bytes).
The real defect is that `ObjectDir::InlineProxy`'s `HX_NATIVE` arm reads the
`mInlineProxyType` *field* instead of dispatching through the **virtual**
`AllowsInlineProxy()`, which `BandCharacter` overrides to `false` in both trees.
`rc=139` → `rc=0`; **all four members instantiate.** §2

★★ **248 SYMBOL GLOBALS WERE DEAD DISPATCH KEYS — a whole silent failure class,
and it is why the venue's authored band transforms were never applied.**
`HANDLE_ACTION` compares against the *global itself*; 139 + 109 of them are
default-constructed to the NULL symbol, so every handler keyed on one reported
"unhandled msg" and did nothing — **with `rc=0` and no warning.** §4

⛔ **The band's own wall is now `obj/ObjPtr_p.h:777-789`.** The shipped
`enter_venue` path crashes because a native erase-suppression guard leaves a
NULL entry in a `kObjListNoNull` list, and `BandCharacter::SyncObjects`'s
shipped loop dereferences it. Opt-in behind `RB3_BAND_PLACE=1` rather than
shipped broken. §5

★ **X6's SHA table is vindicated a second time, independently.** This lane's
default frame hashes `5282bd275159f10b` — *exactly* X6's recorded E1 — and is
`cmp`-identical to X6's artifact. §7

---

## 1. Gate results

| # | Gate | Result | Evidence |
|---|---|---|---|
| a | Full native gate, **fresh** (`rm -rf native/build`), rc=0, 0 SKIPs | ✅ **PASS — 18/18, 0 SKIPs** | `NATIVE GATE: PASS (rc=0, 0 errors, 0 warnings, 18/18 target(s) verified)`; every target `relinked this run`; **zero SKIPs**. Cache seeded first with an explicit `-DMILO_ENGINE_PATH=` + `-DDawn_DIR=` configure, per X4c's warning that the gate's own `cmake` line omits them and lets three targets silently SKIP while still reporting PASS. §1.1 |
| b | Zero `milo-native-engine` edits | ✅ **PASS** | engine `HEAD` == pin `138e1606a202f2b3226e38a8f28010b096f3d441`. ⚠ The foreign uncommitted edit to `src/platform/FxSendNative.cpp` that X4d, X5, X6 **and X7** each disclosed is **still there, still not mine**, still off the load/render path. Left untouched — **fifth lane running**. |
| c | Shared-`src/` edits `HX_NATIVE`-gated, X360 arm faithful | ✅ **PASS** | Full X360 build `rc=0`; all three touched units' objdiff measures **identical to `main`'s to every digit, including `total_code` bytes**. §6 |
| d | PNG determinism ×2 on every cited image | ✅ **PASS** | both cited frames rendered ×2, `cmp` identical, and each matches the copy cited in §8 |
| e | Prior lanes' evidence non-regressed | ✅ **PASS — byte-identical** | `cmp` against X6's **artifact**. §7 |
| f | Was `main` broken by a decomp lane? | ✅ **NO** | `main` @ `254e80bd`; `rb3-render` configured and built `rc=0` first try in a fresh worktree. |

### 1.1 Native gate

**✅ PASS — 18/18, rc=0, 0 errors, 0 warnings, every target `relinked this run`,
ZERO SKIPs**, run fresh after `rm -rf native/build`
(`/home/free/tmp/laneX8/evidence/x8-native-gate.log`). It passed on the first
attempt — unlike X7's, whose first-run failure was the more instructive result.

⚠ Note this lane deliberately kept its riskiest change out of the shared source
list: `InternSymbolGlobals_M6Symbols()` is defined but **uncalled**, because
`m6_symbols.cpp` is linked by seven targets `rb3-render` is not. That is X7's
"a stub's blast radius is the source list it sits in" applied *before* the gate
rather than after it — the gate would otherwise have been the thing that found it.

---

## 2. ★★★ The wall: `InlineProxy` never dispatched through the virtual

### 2.1 Why I measured the asset before touching `SetupDir`

The charter's standing rule — *ask what the missing code would DO before
planning to write it* — has now been the finding in five consecutive lanes.
Applied here it said: **before diffing `SetupDir` against the oracle, check
whether its premise is true.** It is not, and the check cost one script.

`SetupDir`'s bodies in rb3-xenon (`obj/DirLoader.cpp:712-762`) and rb3-Wii
(`rb3/src/system/obj/DirLoader.cpp:446-476`) are **already equivalent** — same
conversion arm, same `TransferLoaderState`/`ReplaceObject`/`mDir = newDir`. There
was no oracle diff to find.

### 2.2 ⛔ `char/main/main.milo` declares `BandCharacter`, not `RndDir`

Decompressed the shipped X360 asset directly (`0xCDBEDEAF`, 3 chunks, header
`mChunkInfoSize` `0x810`):

```
rev              = 0x1C
root class len   = 13   ->  "BandCharacter"
name len         = 4    ->  "main"
```

There is **no class disagreement in the file.** X7's `RndDir` had to come from
somewhere else.

### 2.3 It came from a garbage rev, and X7's own log already ruled out the alternative

`RndDir` can reach `SetupDir` from exactly two places in `LoadHeader`:

- the `mRev > 0xD` arm — but only after logging **`"%s: %s not registered,
  defaulting to %s"`**. That line appears **zero times** in X7's crash log
  (`grep -c` → 0) and zero times in my reproduction.
- the **`mRev <= 0xC`** arm, `SetupDir(dirSym)` with `dirSym == "RndDir"`,
  which logs nothing.

So the rev itself was the measurement. Added `RB3_TRACE_DIRHEADER`
(`HX_NATIVE`, env-gated):

```
char/crowd/crowd_male03.milo   rev=28(0x1c)  stream=PARENT  tell=2593341  proxy='crowd_male03'
char/crowd/crowd_male04.milo   rev=28(0x1c)  stream=PARENT  tell=3018190  proxy='crowd_male04'
char/main/main.milo            rev=8(0x8)    stream=PARENT  tell=3469454  proxy='player0'
```

**Every one of the eight crowd proxies reads rev 28. `player0` reads 8.** The
stream was already off the rails *before* the header — so the conversion, the
`Can't copy type "main"` failure and the `String chars 290146 > 128` SIGSEGV are
all downstream of one bad read.

Those four bytes, read straight out of the decompressed `chars.milo` at inner
offset 3469450:

```
… 0000000a "naked_girl" 00000004 "none"  ad de ad de │ 00 00 00 08 │ 00000011 …
                                          ^^^^^^^^^^   ^^^^^^^^^^^
                                   object terminator    read as mRev
```

`0xADDEADDE` — the object terminator — sits immediately before the read. The
loader was reading a dir header at a position that holds no dir header.

### 2.4 ★★★ Root cause, against the oracle

rb3-Wii (`rb3/src/system/obj/Dir.cpp:613-619`):

```cpp
bool ObjectDir::InlineProxy(BinStream &bs) {
    return AllowsInlineProxy() && bs.Cached();
}
```

`AllowsInlineProxy()` is **virtual** (rb3-Wii `Dir.h:244`, rb3-xenon `Dir.h:495`)
and **`BandCharacter` overrides it to a hard `false` in BOTH trees** (rb3-Wii
`BandCharacter.h:64`, rb3-xenon `BandCharacter.h:71`) — a band member is never
inlined into its parent milo.

rb3-xenon's `HX_NATIVE` arm read the **field**:

```cpp
return (mInlineProxyType == kInlineCached && bs.Cached())
    || mInlineProxyType == kInlineAlways;      // never dispatches the override
```

So `player0` answered `true`, was handed the **parent** stream
(`Dir.cpp:1588` `InlineProxy(bs) ? &bs : nullptr`) instead of opening
`char/main/main.milo` from disk. ⚠ Note the non-native arm **already had the
retail form** — this was native-only divergence, introduced with the DC3-era
`InlineDirType` enum.

Corroboration that the body genuinely is not inlined: the four
`"char/main/main.milo"` strings in `chars.milo` sit at 3468690 / 3469841 /
3470975 / 3472130 — **~1.1 KB apart**, for a file whose decompressed body is
**114705 bytes**.

**Fix:** honour the virtual first, exactly as retail does, then keep the native
enum semantics for whatever the override does not veto. The whole function was
already `#ifdef`-split, so `default/system/obj/Dir` cannot move.

**Result: `rc=139` → `rc=0`.** `main.milo` loads `OWN-FILE` at rev 28, and:

```
player0  [BandCharacter] objs=50 mesh=0 char=2 sub=3 PROXY
player1  [BandCharacter] objs=50 mesh=0 char=2 sub=3 PROXY
player2  [BandCharacter] objs=50 mesh=0 char=2 sub=3 PROXY
player3  [BandCharacter] objs=50 mesh=0 char=2 sub=3 PROXY
```

---

## 3. ⛔ `player_mic0` → `player_vocals0`, confirmed from X360 assets

Not carried over from rb3-Wii's note about the *Wii* milo. Decompressed **all
eleven** shipped X360 `small_club` venue roots:

| venue | `player_vocals0` | `player_mic0` |
|---|---|---|
| `small_club_01` … `small_club_15` (11 roots) | **1322 – 1360 each** | **0 in every one** |

The name `LoadMainCharacters` builds is not present anywhere in any venue.
rb3-Wii carries the identical `HX_NATIVE` remap at `BandWardrobe.cpp:695-703`;
the surrounding lines in the two trees are otherwise token-for-token identical.
Applied, `HX_NATIVE`-gated.

**Measured effect:** with the mode sink wired (§4), **all four slots resolve** —
X7's `SyncPlayMode` unresolved-slot warning fires zero times.

---

## 4. ★★ 248 dead dispatch keys — the silent class

`obj/ObjMacros.h:184`:

```cpp
#define HANDLE_ACTION(symbol, action)  if (sym == symbol) { (action); return 0; }
```

This arm compares the incoming message against **the global variable itself**,
not a string literal. `native/src/milo_link_stubs.cpp` (139) and
`native/src/m6_symbols.cpp` (109) define those globals as:

```cpp
Symbol sync_play_mode;      // default-constructed == gNullStr == the NULL symbol
```

under a comment asserting they are *"dispatch keys only … never reached on the
load path"* and *"Default-constructed rather than interned, so nothing here
depends on `Symbol::Init` ordering."* **The first clause is false**, and the
second describes a real hazard whose fix was to disable the feature.

**Measured**, and it is the whole reason the venue's band transforms were never
applied — `BandWardrobe::SetVenueDir` → `SyncPlayMode()` →
`mModeSink->Handle(sync_play_mode_msg)`:

```
BandConfiguration (world/venue/small_club/small_club_01/small_club_01_base.milo):
    unhandled msg: sync_play_mode
```

`rc=0`, no warning, members silently left at their asset defaults. **A dead
dispatch key is indistinguishable from a message that was never sent** — which
is precisely why it survived four lanes.

Retail defines these in `src/system/utl/Symbols*.cpp`
(`Symbol sync_play_mode("sync_play_mode");`, rb3-Wii `Symbols.cpp:960`);
**rb3-xenon ships the `Symbols*.h` headers and no corresponding `.cpp`**, which
is why they had to be hand-defined at all.

**Fix:** intern them in a **function** called after `Symbol::Init()`, never at
static-init — the `Symbol` ctor dereferences `gStringTable`, which
`Symbol::Init` → `PreInit` creates, so static-init construction is a null deref
or an ordering lottery. The original comment's concern was correct; its remedy
was not.

⚠ **Only rb3-render's copy is called.** `m6_symbols.cpp` is not in rb3-render's
source list (`native/CMakeLists.txt:1235` gives this target `milo_link_stubs.cpp`;
`m6_symbols.cpp` goes to seven *other* targets). Its twin is **defined but
deliberately left uncalled** — enabling it changes behaviour in seven targets
this lane does not exercise. Filed as owed work. *(X7's lesson applied: a stub's
blast radius is the source list it sits in.)*

### 4.1 Two more defects the same path exposed

⛔ **`BandCharDesc::Init()` was never called → SIGSEGV.** Retail reaches it via
`BandInit()` (`bandobj/Band.cpp:98-113`), which the hand-rolled factory list
replaces. Without it `gInstNames[6]` is all NULL symbols, every member gets a
null instrument, `InstrumentOutfit::GetPiece` returns 0 for anything outside
{guitar,bass,drum,mic,keyboard}, and `BandWardrobe.cpp:620` dereferences it.
Replaced the bare `REGISTER_OBJ_FACTORY` lines with the engine's own `Init()`s
(which also restore `Register()` and two `DataFunc`s). **Fourth instance in that
file of the drift its own header warns about**; X7 found the third (`Waypoint`).

⛔ **The venue's `BandConfiguration` never becomes the wardrobe's mode sink.**
`BandConfiguration::Load`'s last statement (`:116-118`) is
`if (TheBandWardrobe) TheBandWardrobe->SetModeSink(this);` — but it runs while
the *venue root* deserializes, which is **before** `world/shared/world_chars.milo`
(where `TheBandWardrobe` is instanced) has loaded, so the guard silently
declines and `mModeSink` stays null forever. The driver re-executes that one
shipped statement, once, later.

### 4.2 ⚠ The play mode is read from shipped data, not chosen

`band.play_mode` is unset in this harness (retail sets it from
`config/band_keep.dta`, the SystemInit half X3 documented as unreadable from the
shipped archive). Both consumers hard-fail on that. The value is taken from
**`config/macros.dta`**, which ships `#define BAND_PLAY_MODES (coop_bg coop_bk
coop_gk)` — the same macro `BandConfiguration::ConfigIndex` itself indexes —
element **0**, `coop_bg`. Which mode a session is in is *game state*, not
placement data; the venue ships an authored row for all three.
`RB3_BAND_PLAY_MODE` reaches the other two. **Disclosed as a selection.**

---

## 5. ⛔ What does not work, and the defect that stops it

The shipped bridge is **not something I invented**: `BandWardrobe::OnEnterVenue`
(`bandobj/BandWardrobe.cpp:911-919`) is

```cpp
MILO_ASSERT(!TheBandDirector, 0x750);   // <- the no-director case rb3-render is in
LoadCharacters(dir->Name(), false);
SetVenueDir(dir);
```

i.e. the `enter_venue` DTA handler, whose first line asserts *exactly* the
situation this harness is in. Nothing dispatches `enter_venue` natively, so the
driver calls those two.

**It crashes**, and the defect is named rather than papered over:

```
SIGSEGV in ObjRefConcrete<RndMesh,ObjectDir>::operator RndMesh*()
   <- RndMeshDeform::Mesh()
   <- BandCharacter::SyncObjects()
   <- BandCharacter::OnPostMerge()      (fired by the FileMerger during LoadCharacters)
```

`BandCharacter::SyncObjects` runs the shipped loop (`BandCharacter.cpp:148-153`,
**token-identical to rb3-Wii `:186-192`**):

```cpp
while (!unk610.empty()) { RndMeshDeform *df = unk610.front(); … df->Mesh()…; delete df; }
```

which assumes a `kObjListNoNull` list really contains no nulls. **Natively it
can.** `obj/ObjPtr_p.h:777-789` (and the `ObjPtrVec` twin at `:538-549`)
suppresses the erase whenever `gInReplaceList` is set — a guard an earlier lane
added against real heap corruption ("corrupted double-linked list") — and leaves
a **NULL entry** in the list instead. A merge *is* a `ReplaceList`, so `front()`
hands back null. Worse: **the entry never leaves**, so even a null-skip would
spin forever on `!empty()`.

⚠ Not `RB3_NO_DEFORM`-avoidable — measured, same crash with deform disabled;
the loop is after `SetDeformation()`, not inside it.

Reconciling that suppression with the no-null invariant is **its own lane**: it
touches a header every target includes, and every prior lane's frame is downstream
of it. **A crash is not a frame.** Off by default (`RB3_BAND_PLACE=1`), exactly
as X7 left the registrations it could not make safe.

### 5.1 ★ Why they still do not draw — the per-mesh number

The default build now reports, per member:

```
player0  world=(55.67 28.85 0.00)  meshes=140  skinned=0  showing=0  verts>0=34  DRAWABLE=0
```

**All 140 of a member's meshes have `Showing()` false**; only **34** carry
geometry. `SetShowing(true)` on the *characters* (the one statement
`BandWardrobe::SetDir` opens with) does **not** propagate to meshes — measured,
draw count unchanged at 114. Which meshes a member shows is chosen by the
outfit/LOD recompose **inside the wardrobe path**, i.e. behind §5's defect.

⚠ **The four members SHARE one 140-mesh set.** Total collected is 320
(180 venue + 140), not 180 + 4×140. So a forced frame shows **one** figure, not
four.

⚠ **`skinned=0`** for band members against `skinned=6` for each crowd member —
unexplained, not investigated, flagged rather than asserted.

### 5.2 ⚠ The one diagnostic, and exactly what it is not

`RB3_BAND_FORCE_SHOW=1` un-hides the 136 meshes with geometry. It answers **one**
question — *is this geometry renderable at all* — and answers **nothing** about
which meshes a real member shows, what it wears, or where it stands.
**114 → 148 draws; the geometry renders.** Same disclosure class as X6's
crowd-draw substitution: a **mechanism** stand-in, never a placement one.

⛔ **NOT SUBSTITUTED — PLACEMENT. I did not compute, interpolate or hand-pick a
single position anywhere in this lane.** The members' four distinct positions are
`char/main/main.milo`'s own authored defaults (y=28.85, z=0), **not** the venue's
slots (`bass(-70.0,80.7,13.5) drum(14.4,146.1,13.2) guitar(68.8,51.4,13.2)
vocals(-10.0,31.4,13.2)` for `coop_bg`). **A frame from this path must never be
described as "the band on its marks."**

---

## 6. Shared-`src/` edits — X360 verification

Three TUs, **every hunk inside `#ifdef HX_NATIVE`**, so no hunk can reach the
X360 arm by construction. All three **are** scoreable units (unlike
`BandConfiguration`, which still has no `splits.txt` entry — carried from X6/X7
unchanged, and saying so is not the same as implying a match).

Baseline, from `main`'s `report.json`:

| unit | `matched_code_percent` | `total_code` | `matched_functions_percent` |
|---|---|---|---|
| `default/system/obj/Dir` | 53.22487 | 27536 | 83.5 |
| `default/DirLoader` | 47.168865 | 23948 | 64.42308 |
| `default/BandWardrobe` | 67.23399 | 33608 | 88.51852 |

**Post-change, measured from this branch's own `report.json` after a full X360
build (`rc=0`):**

| unit | this branch | `main` | |
|---|---|---|---|
| `default/system/obj/Dir` | 53.22487 / 27536 B / 83.5 | 53.22487 / 27536 B / 83.5 | **SAME** |
| `default/DirLoader` | 47.168865 / 23948 B / 64.42308 | 47.168865 / 23948 B / 64.42308 | **SAME** |
| `default/BandWardrobe` | 67.23399 / 33608 B / 88.51852 | 67.23399 / 33608 B / 88.51852 | **SAME** |

Identical `matched_code_percent`, identical `total_code` **bytes**, identical
`matched_functions_percent`. Nothing moved. Repo overall unchanged at
**39.153187% code / 62.959976% functions**.

⚠ A whole-file `md5` comparison would be worthless here — `setup_worktree.sh`
re-runs `configure.py` with absolute paths, so objects differ by embedded path
alone (X7's finding, re-applied).

---

## 7. Determinism and non-regression

Both cited frames rendered **×2**, `cmp`-identical each time, and each matches
the copy cited in §8.

★ **The default frame is BYTE-IDENTICAL to X6's shipped artifact**, verified by
`cmp` against the PNG (not against a transcribed hash):

| frame | vs X6's PNG on disk | sha256 | X6's recorded value |
|---|---|---|---|
| default (X6 "E1") | **`cmp` IDENTICAL** | `5282bd275159f10b` | `5282bd275159f10b` ✅ |

⚠ **This independently vindicates X6's SHA table a second time**, and confirms
the coordinator's correction to X7's retraction 4 without relying on it: I
measured the artifact first and my two instruments (`cmp` and `sha256sum`) agree
with each other *and* with X6's document. X7's own table asserted `cmp`-identity
**and** a different hash for the same file — two claims that cannot both be true,
which is the check that should have caught it in-lane.

---

## 8. Per-subsystem verdict table

| subsystem | verdict | evidence |
|---|---|---|
| **Band members rendering on stage** | ⬜ **UNREACHED — the milestone did not land** | default frame byte-identical to X6's. §5.1 |
| **Band members INSTANTIATE** | ★★ **FIXED — 4/4** | `player0..3`, `objs=50` each, 4 distinct positions. `rc=139` → `rc=0`. §2 |
| **The "proxy class conversion" wall** | ★★★ **RETRACTED and FIXED** | `main.milo` declares `BandCharacter`; `InlineProxy` bypassed a virtual. §2 |
| **Band member GEOMETRY renders** | ✅ **YES, under a disclosed diagnostic** | 114 → 148 draws, `RB3_BAND_FORCE_SHOW`. §5.2 |
| **Band members at the venue's SLOTS** | ⛔ **NO — asset defaults only** | `SyncPlayMode` blocked by §5. Never hand-placed. §5.1 |
| **`player_mic0` → `player_vocals0`** | ✅ **FIXED, confirmed from X360 assets** | 11 venues, 1322-1360 vs **0**. All 4 slots resolve. §3 |
| **Symbol dispatch keys** | ★★ **FIXED (rb3-render); 109 still dead in 7 other targets** | 248 null globals; `unhandled msg: sync_play_mode`. §4 |
| **`BandCharDesc::Init()`** | ✅ **FIXED** | `gInstNames` all-null → SIGSEGV. 4th factory-list drift. §4.1 |
| **`BandConfiguration` mode sink** | ✅ **FIXED (load-order)** | `Load`'s guard runs before `TheBandWardrobe` exists. §4.1 |
| **Band mesh visibility / outfit recompose** | ⛔ **BLOCKED** | 140 meshes, `showing=0`, 34 with geometry. §5.1 |
| **`ObjPtrList` NULL entry under `gInReplaceList`** | ⛔ **THE NEW WALL** | `ObjPtr_p.h:777-789`; SIGSEGV in `SyncObjects`. §5 |
| **`BandConfiguration` objdiff score** | ⛔ **STILL NOT SCOREABLE** | no `splits.txt` entry. Unchanged from X6/X7. §6 |
| **Venue geometry / crowd / lighting / skinning** | ✅ **ALIVE, non-regressed** | byte-identical to X6's artifact. §7 |
| **Deform / skin refinement** | ⚠ **STUBBED, disclosed (carried from X7)** | unchanged |
| **`BandCamShot` / camera shots** | ⬜ **UNREACHED** | 611 misses, carried |
| **`video_05` empty frame; impostor RTT** | ⚠ **CARRIED** | untouched |

---

## 9. Retracted hypotheses, with evidence

1. ⛔ **X7's "the proxy class conversion desync — THE wall": `chars.milo`'s
   `player0` is a proxy declared `BandCharacter` whose proxied
   `char/main/main.milo` declares its root as `RndDir`, so
   `DirLoader::SetupDir:712-748` `ReplaceObject`s a half-loaded dir mid-stream.**
   **Retracted on both counts.** `main.milo` declares **`BandCharacter`** (rev
   0x1C, symbol length 13, read from the decompressed asset). The `RndDir` came
   from `LoadHeader`'s `mRev<=0xC` arm after a garbage rev of **8** was read out
   of the four bytes following an `0xADDEADDE` terminator — the alternative arm
   logs "not registered, defaulting to", which appears **zero** times in X7's own
   log. `SetupDir` is equivalent to rb3-Wii's and was never at fault. §2.
2. ⛔ **X7's handoff: "Diff `SetupDir`/`ReplaceObject`/`TransferLoaderState`
   against the rb3-Wii oracle **before** changing either."**
   **Retracted as a plan.** The diff is empty — the two bodies already agree.
   Following it would have consumed the lane and found nothing. What settled it
   was checking the *premise* (one script over the asset), not the code.
3. ⛔ **`milo_link_stubs.cpp`'s own comment: "They are dispatch keys only:
   `SyncPlayMode` is reached natively by a DIRECT call from the driver, not
   through `Handle()`, so none of these needs to be interned for placement to
   work."**
   **Retracted by measurement.** The shipped path *is* `Handle()`
   (`BandWardrobe::SyncPlayMode` → `mModeSink->Handle(sync_play_mode_msg)`), and
   it printed `unhandled msg: sync_play_mode`. 248 globals were affected. §4.
4. ⛔ **My own first hypothesis: "`SetupDir`'s conversion arm needs a guard, like
   X4d's `Dir.cpp:475` fix."**
   **Retracted before any code was written**, by reading the asset instead of the
   function. Recorded because it was the charter's own suggested starting point.
5. ⛔ **My own second hypothesis: "the deform path is stubbed, so
   `RB3_NO_DEFORM=1` will get the wardrobe through."**
   **Retracted by A/B.** Identical backtrace with deform disabled — the crashing
   loop is *after* `SetDeformation()`, not inside it. §5.
6. ⚠ **Explicitly NOT claimed:** that fixing §5's `ObjPtrList` defect would make
   the band render correctly. It would un-block outfit/LOD selection; it says
   nothing about the 5 stubbed deform passes, `CharKeyHandMidi`, the `skinned=0`
   anomaly, or the shared-mesh-set question (§5.1).
7. ⚠ **Explicitly NOT resolved:** why the four members share ONE 140-mesh set,
   and why band meshes report `skinned=0` while crowd members report `skinned=6`.
   Both flagged, neither investigated.
8. ⚠ **Explicitly NOT resolved (carried from X7):** the ~56-unit z gap between
   band and crowd positions. Untouched.

---

## 10. Owed work / handoff

| item | why | owner |
|---|---|---|
| ⛔ **`ObjPtrList`/`ObjPtrVec` NULL entry under `gInReplaceList` — THE wall** | `obj/ObjPtr_p.h:777-789` + `:538-549`. Leaves a NULL in a `kObjListNoNull` list; the shipped `while(!empty()) front()->…` loop in `BandCharacter::SyncObjects` derefs it, and the entry never leaves so a null-skip spins. Needs the suppression reconciled with the no-null invariant — a header every target includes, so an A/B on every prior frame. | X9 |
| ⛔ **Band mesh visibility (outfit/LOD recompose)** | 140 meshes/member, `showing=0`, 34 with geometry. Downstream of the row above. This is what stands between here and a rendered band. | X9 |
| ⚠ **109 dead dispatch keys in 7 other targets** | `InternSymbolGlobals_M6Symbols()` is **defined and uncalled**; `m6_symbols.cpp` is linked by the seven targets at `native/CMakeLists.txt:451/492/535/578/632/684/730`. One call each, but it is a behaviour change in targets this lane cannot gate. | build-system |
| ⚠ **Why four members share ONE 140-mesh set** | 320 collected, not 180+560. Decides whether four members can ever draw at four positions through this driver's flat-mesh-vector draw loop. | X9 |
| ⚠ **`skinned=0` on band meshes vs `skinned=6` on crowd** | Unexplained. Likely bone-binding, adjacent to X7's `mNativeBonesRebound` seam. | X9 |
| ⚠ **`band.play_mode` has no shipped source natively** | Read from `config/macros.dta` element 0 (`coop_bg`) and disclosed. A real fix restores `config/band_keep.dta` or its play-mode section. | X9 |
| ⛔ **`BandConfiguration` has no `splits.txt` entry** | Carried from X6/X7 unchanged. | match lanes |
| **`BandCamShot` (611 misses, also gates crowd visibility); 18 stubbed bodies; `ObjOwnerPtr` recurrence trap; impostor RTT; `video_05`; foreign `FxSendNative.cpp` edit** | All carried, untouched. | as before |

---

## 11. Recommended X9 shape

1. ★ **Check the PREMISE of an inherited wall, not just its mechanism.** Five
   lanes running. X7's wall was stated with a file, a line range and a
   reproduction, and every part of the *mechanism* was correctly observed — the
   *premise* ("main.milo declares RndDir") was never checked and was false. One
   script over the asset retired it. When you inherit "the blocker is X at
   file:line", first ask what would have to be true of the DATA for that to be
   the blocker.
2. ★ **A silent failure is worse than a crash, and dead dispatch keys are the
   canonical case.** 248 handlers did nothing, with `rc=0` and no warning, for
   four lanes. When a shipped path "runs" and nothing happens, suspect that the
   key it dispatches on is null before suspecting the path.
3. ★ **`grep` the comment as well as the code.** Both this lane's biggest finds
   were sitting under comments that asserted the opposite ("never reached on the
   load path"; "not a lane"). An assertion in a comment is a hypothesis with no
   test attached.
4. **The band is now one defect away — and this time the defect is one
   function.** `ObjPtr_p.h`'s erase suppression is all that stands between this
   tree and the wardrobe path running end to end.
