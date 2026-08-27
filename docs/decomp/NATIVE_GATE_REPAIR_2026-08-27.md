# Native gate repair — lane NATFIX, 2026-08-27

**What this lane did:** restored `tools/native_build_gate.sh` to a full pass after
four permuter-sweep shard merges broke it, **without reverting any of the sweep's
landed match work**.

**What it also found, and considers the more important half of the result:** the
sweep landed **five distinct semantic defects**, of which **only two were
compile errors**. The other three compile clean in both builds and would never
have been caught by any tool the project runs. One of them swaps two revision
words in a `.milo` parser; one kills two arms of the scene-graph transform
chain; one makes a validation function's early-out unreachable.

This is the standing accuracy directive demonstrating itself. From the sweep's
own commit messages:

> Every edit was found by search and judged solely by the compiler and objdiff.
> None is hand-written.

The compiler and objdiff cannot see any of the three silent defects. **A metric
that hides real bugs is worse than a lower metric.**

---

## 0. The verdict line

Before (main `469b3082`, reproduced in this lane's worktree):

```
NATIVE_GATE_RESULT verdict=FAIL expected=18 verified=1 skipped=0 partial=0 failed=17 rc=1
```

After: see §6.

Attribution was established by the coordinator before this lane opened: gating
the permuter merge point `1e091edf` in isolation, with none of the later vtable
lanes present, reproduces an identical FAIL across the same 17 targets. The four
implicated merges are `648d8154`, `79d23d0f`, `4f37cd30`, `1e091edf` (source
commits `0c36bc01`, `cd42a2d3`, `9d99331a`, `58172bae`). **They are local only —
`origin/main` does not have them.**

---

## 0.1 Two corrections to the incoming brief

Both were load-bearing enough to record.

**(a) The gating macro is `HX_NATIVE`, not `VERSION_SZBE69_B8`.** The brief
attributed the `ASSERT_REVS` asymmetry to `VERSION_SZBE69_B8` in
`obj/ObjMacros.h`. That is a *different* macro set — the rb3-Wii dialect, whose
`INIT_REVS` takes **one** argument (`INIT_REVS(objType)`). `CharBoneDir.cpp`
writes `INIT_REVS(4, 0)`, the **two**-argument form, so it cannot be seeing that
definition. There are three competing definitions in the tree:

| file | `ASSERT_REVS` gate | uses `d`? |
|---|---|---|
| `obj/ObjMacros.h:676` | `VERSION_SZBE69_B8` | no — uses `gRev`/`gAltRev` |
| `obj/Object.h:1707` | **`HX_NATIVE`** | **yes — `d.rev` / `d.altRev`** |
| `obj/dialect_object_push.h:70` | ungated (always live) | yes |

`CharBoneDir.cpp` includes `obj/Object.h` and does **not** include
`obj/dialect_object_push.h`, so it sees the middle row. That is confirmed
independently by the error text itself: only a definition that references `d`
can produce *"use of undeclared identifier `d`"*, which rules out the
`ObjMacros.h` row; and the dialect row is ungated, so a TU seeing it would fail
the **match** build too, which `CharBoneDir.cpp` does not.

The practical consequence is unchanged and is what makes the repair cheap:
`obj/Object.h:1739` defines `ASSERT_REVS(rev1, rev2)` as **empty** whenever
`HX_NATIVE` is undefined, which is every match build. Moving an empty macro
cannot change match codegen.

**(b) There were FIVE distinct diagnostics, not three.** The brief listed
`CharBoneDir.cpp`, `DirLoader.cpp` and `Trans.cpp`. The gate's own
`--- distinct diagnostics (5) ---` block also names **`BandWardrobe.cpp:421`**,
with two errors on that single line. The brief also gave `DirLoader.cpp`'s path
as `src/system/utl/DirLoader.cpp`; the real file is
**`src/system/obj/DirLoader.cpp`**.

---

## 0.2 The repair construction, and why it needs no measurement to be safe

Every edit in this lane is written so that **the match build's preprocessed token
stream is unchanged**. That is a stronger guarantee than a measured Δ0: it is a
property of the preprocessor, not an observation about a particular build.

Two forms are used.

1. **Move a macro that is empty in the match build.** Used once, in
   `CharBoneDir`. `ASSERT_REVS` expands to nothing when `HX_NATIVE` is
   undefined, so relocating it is a zero-token edit.
2. **`#ifdef HX_NATIVE` / `#else`, with the sweep's exact spelling preserved in
   the `#else` arm.** Used everywhere else. The match build takes `#else` and
   sees byte-identical tokens; the native build takes the correct code.

Form 2 is house style in these very files, not an invention of this lane —
`obj/Object.h:1707` (the `ASSERT_REVS` definition itself), `obj/Dir.cpp:1174`
(`ObjectDir::InlineProxy`) and `rndobj/Trans.cpp:16` (`sShadowPlane`) already do
exactly this, each with a comment explaining the divergence. Measured across the
engine: **388 files under `src/system` already carry an `#ifdef HX_NATIVE`
divergence block** (284 `.cpp`, 104 `.h`).

**This lane deliberately did not revert anything.** Reverting a landed hunk is
the project owner's call. Where the sweep's code is wrong, the match build keeps
it and this document records what it costs; only the native build is corrected.

---

## 1. `src/system/char/CharBoneDir.cpp` — swapped revision words

*Sweep shard 0 (`0c36bc01`), landed as `648d8154`.*

The hunk, as landed:

```diff
 void CharBoneDir::PreLoad(BinStream &bs) {
-    LOAD_REVS(bs)
     ASSERT_REVS(4, 0)
     ObjectDir::PreLoad(bs);
+    LOAD_REVS(bs)
     d.PushRev(this);
 }
```

**The compile error** is the shallow half: `LOAD_REVS(bs)` is what *declares*
`d` (`int revs; bs >> revs; BinStreamRev d(bs, revs);`), and it now sits after
`ASSERT_REVS(4, 0)`, which *uses* `d.rev`. Empty in the match build, live in
native ⇒ `CharBoneDir.cpp:85: use of undeclared identifier 'd'`.

**The real defect is the reorder itself, and it is not a compile error.**
`ObjectDir::PreLoad` *opens with its own* `LOAD_REVS(bs)` — `obj/Dir.cpp:1248`:

```cpp
void ObjectDir::PreLoad(BinStream &bs) {
    LOAD_REVS(bs)
    ASSERT_REVS(0x1C, 0)
    sObjectDirRev = d.rev;
```

So **both** orderings consume a revision word from the stream. They differ only
in *which* word each reader gets:

| | first 4 bytes read by | next 4 bytes read by |
|---|---|---|
| original | `CharBoneDir::PreLoad` | `ObjectDir::PreLoad` |
| as landed | `ObjectDir::PreLoad` | `CharBoneDir::PreLoad` |

The two revisions are **transposed**. `ObjectDir` then validates a CharBoneDir
rev against `0x1C` and stores it in `sObjectDirRev`, and `CharBoneDir` branches
its member parsing (`d.rev < 2`, `< 3`, `> 3`) on an ObjectDir rev. Every
`CharBoneDir` in every `.milo` mis-parses.

`Dir.cpp:1248-1249` is also the project's own ordering oracle: **`LOAD_REVS`,
then `ASSERT_REVS`, then use.** The sweep's arrangement is the only one in the
tree that inverts it.

**Fix.** Native gets the correct order; the match build keeps the sweep's, with
`ASSERT_REVS` moved below `LOAD_REVS` — a zero-token move, since the macro is
empty there.

**Match-neutrality evidence:** the `#else` arm preprocesses to
`ObjectDir::PreLoad(bs); LOAD_REVS(bs)`, and the pre-edit source preprocesses to
`ObjectDir::PreLoad(bs); LOAD_REVS(bs)` — `ASSERT_REVS(4, 0)` contributes no
tokens in either. Identical.

---

## 2. `src/system/obj/DirLoader.cpp` — a save/restore pair reduced to three no-ops

*Sweep shard 0 (`0c36bc01`), landed as `648d8154`.*

```diff
     if (mStream == nullptr) {
-        Archive *theArchive = TheArchive;
-        bool using_cd = UsingCD();
-        bool cache_mode = sCacheMode;
         const char *fileStr = mFile.c_str();
         bool matches = gHostFile && FileMatch(fileStr, gHostFile);
         if (matches) {
+            TheArchive = nullptr;
             SetCacheMode(gHostCached);
             SetUsingCD(false);
-            TheArchive = nullptr;
         }
         ...
         if (matches) {
+            bool cache_mode = sCacheMode;
+            bool using_cd = UsingCD();
             SetCacheMode(cache_mode);
             SetUsingCD(using_cd);
-            TheArchive = theArchive;
+            TheArchive = TheArchive;
         }
```

This was a **save/restore** around a host-file override. The sweep moved the
three saves from *above* the block that overwrites the globals to *below* it,
and deleted `Archive *theArchive` outright. The result is that **all three
restores are no-ops**:

- `cache_mode` now re-reads `sCacheMode` *after* `SetCacheMode(gHostCached)` ⇒
  it holds `gHostCached`, and `SetCacheMode(cache_mode)` restores nothing.
- `using_cd` now re-reads `UsingCD()` *after* `SetUsingCD(false)` ⇒ it holds
  `false`, and `SetUsingCD(using_cd)` restores nothing.
- `TheArchive = TheArchive;` cannot undo the `TheArchive = nullptr;` above.

Net effect: after a host-file load, cache mode, `UsingCD` and `TheArchive` are
**never restored**, and `TheArchive` is left permanently null.

**Only the self-assign is a compile error** (`-Werror,-Wself-assign`), and only
that one line is touched. Eliding a no-op is behaviourally identical in every
build, so this is the genuinely minimal edit.

**This lane did not reconstruct the save/restore, in either build.** Doing so in
native only would require re-adding `Archive *theArchive` and hoisting two
locals — a real behavioural divergence between the two builds, in a path that is
**dormant in the native port anyway**: `matches` requires `gHostFile`, which is
set only from the dev-only `-host_file` command-line option
(`os/System.cpp:678`). Repairing it properly is a revert of the hunk, and that
is the owner's call. It is recorded here so the decision is available.

---

## 3. `src/system/rndobj/Trans.cpp` — `!!()` against out-of-range enumerators

*Sweep shard 2 (`9d99331a`), landed as `4f37cd30`.* Two sites, and **the one
that does not error is the worse one.**

Relevant enumerators (`rndobj/Trans.h`):

```
kConstraintNone = 0,  kConstraintLocalRotate = 1,  kConstraintParentWorld = 2,
kConstraintShadowTarget = 4,  kConstraintFastBillboardXYZ = 8,
kConstraintTargetWorld = 9,   kConstraintNoParentRotation = 10,
```

### 3a. `ApplyDynamicConstraint()` — the reported error

```diff
-    if (mConstraint == kConstraintTargetWorld) {
+    if (!!(mConstraint) == kConstraintTargetWorld) {
```

`!!(mConstraint)` is `0` or `1`; `kConstraintTargetWorld` is `9`. Always false ⇒
the `kConstraintTargetWorld` arm is dead. clang flags it precisely because `9` is
*outside* the `bool` range
(`-Werror,-Wtautological-constant-out-of-range-compare`). This is the error that
opened the lane.

### 3b. `WorldXfm_Force()` — silent, and more damaging

```diff
-    } else if (mConstraint == kConstraintLocalRotate) {
+    } else if (!!(mConstraint) == kConstraintLocalRotate) {
```

`kConstraintLocalRotate` is **1**, which *is* in range — so **no compiler, in
either build, emits any diagnostic at all.** The expression reduces to
`mConstraint != 0`, and the chain has no `kConstraintNone` guard ahead of it:

```cpp
    if (!mParent) { ... }
    else if (mConstraint == kConstraintParentWorld) { ... }
    else if (!!(mConstraint) == kConstraintLocalRotate) {   // == (mConstraint != 0)
```

By this point `mConstraint` can still be anything except `kConstraintParentWorld`,
so the arm is taken for **every nonzero constraint**. Consequences:

- `kConstraintNoParentRotation` (10) — **dead code**.
- the trailing `else` (`Multiply(mLocalXfm, mParent->WorldXfm(), mWorldXfm)`) —
  **dead code**.
- shadow-target, billboard and target-world transformables are all silently
  routed through the **LocalRotate** arm.

That is a scene-graph-wide behavioural change in the function that composes every
world transform, every frame.

**Fix.** Both sites get the correct comparison under `#ifdef HX_NATIVE`; the
match build keeps the sweep's spelling verbatim in the `#else` arm. 3b was fixed
even though it is not a compile error, because the native port renders through
`WorldXfm_Force` continuously and the gate only proves compilation.

### 3c. Not fixed — an added conjunct, reported only

```diff
-    } else if (mConstraint == kConstraintShadowTarget) {
+    } else if (mTarget && mConstraint == kConstraintShadowTarget) {
```

This one is genuinely ambiguous. The original arm reads an **uninitialised**
`Transform tf` when `mTarget` is null (`tf` is only written inside
`if (mTarget)`, then unconditionally consumed by
`Multiply(sShadowPlane, tf, pl)`), so the sweep's guard suppresses real
undefined behaviour. But it is still a behaviour change relative to retail, and
"safer than retail" is not this lane's call to make. **Left as landed, in both
builds.**

---

## 4. `src/system/bandobj/BandWardrobe.cpp` — bitwise `&` turned into logical `&&`

*Sweep shard 3 (`58172bae`), landed as `1e091edf`.*

```diff
 bool BandWardrobe::ValidGenreGender(CamShot *shot) {
     int flags = shot->Flags();
-    if ((flags & 0xF03) == 0xF03)
+    if ((flags && 0xF03) == 0xF03)
         return true;
```

`flags && 0xF03` yields `0` or `1`, which can never equal `0xF03` (3843), so the
early `return true` is **unreachable** and every shot falls through to the
focus-flag path below. This is the site the brief missed entirely; the gate
reports two errors on it
(`-Wconstant-logical-operand` and `-Wtautological-constant-out-of-range-compare`).

**Fix.** Native gets `&`; the match build keeps `&&`.

Two further hunks in the same file from the same shard were checked and are
**algebraically equivalent** — `if (!(!(bestScore > 0)))` for `if (bestScore > 0)`,
and `if (bestScore > score)` for `if (score < bestScore)`. Ugly, harmless, left
alone.

---

## 5. Further semantically-wrong sweep edits (reported, not fixed)

Scope note: this was a **bounded** scan of the four shards' combined diff
(3,141 lines, reconstructed with `git diff <merge>^1 <merge>`), looking for the
same shapes — introduced `!!()`, bitwise→logical operator swaps, self-assignment,
statements moved across calls, and dropped save/restore pairs. It is not an
audit of the whole tree.

### 5a. Tree-wide shape scan (this lane)

The four defects above were found from the gate's diagnostics. To check whether
the same shapes exist *outside* the shards, the whole of `src/` (excluding
`stlport/` and `xdk/`) was scanned for three signatures. Results:

| shape | hits | verdict |
|---|---:|---|
| `!!(…) == <constant>` | 2 | both are §3, already handled |
| `(x && <mask>) == <mask>` | 1 | §4, already handled |
| self-assignment `x = x;` | 2 | §2, **plus one new — see below** |

**The scan found no *new* instance of the `!!()` or `&&`-for-`&` shapes anywhere
in the tree.** Those two families are confined to the sweep.

**`src/system/rndobj/MultiMesh.cpp:363` — `proxy = proxy;`** is a genuine
self-assign, but it is **NOT from these shards**: `git log` attributes it to
`0ac748fc` (lane AG2, a body-port campaign). It is a true no-op and therefore not
a behaviour change — the enclosing loop already assigns `proxy` on every
iteration — so it is reported, not fixed.

### 5b. ⚠ A gate-coverage gap, found incidentally and worth acting on

`MultiMesh.cpp:363` raises an obvious question: if `-Werror,-Wself-assign` broke
the build over `DirLoader.cpp`, why is this one silent?

**Because `MultiMesh.cpp` is not compiled by the native build at all.** Only
`MultiMeshProxy.cpp` appears in `native/build/build.ninja`; there is no
`MultiMesh.cpp.o` edge in any of the 18 targets.

⇒ **`NATIVE_GATE_RESULT verdict=PASS` does NOT mean "all of `src/system` is
clang-clean."** It means every TU the 18 native targets *link* is clean. TUs
outside that closure carry no such guarantee, and `MultiMesh.cpp` demonstrably
already contains a diagnostic that would break the build the day it is added.

This is the same family as the gate's other documented blind spots (the X360
match build compiles a *subset* of what native links, so it cannot catch
undefined symbols; here the relation runs the other way). It is **not** a defect
in the gate — the gate is honest about what it verifies — but it is a limit on
what a PASS licenses, and this lane could not find it written down anywhere.

### 5c. Adjudicated hunks — three more CONFIRMED behaviour changes

Ten "pure move" candidates (a statement removed and re-added inside one hunk,
i.e. reordered) were enumerated from the shard diff and read against the
surrounding source. Most are semantics-preserving if/else arm swaps. Three are
not. **None of these is a compile error in either build; all three are live in
`main` today and none is fixed by this lane.**

**(i) `src/system/utl/MemTrack.cpp:183` — output redirected to the wrong log.**

```diff
             StopLog();
+            if (gMemTracker)
+                gMemTracker->DiffDump(*gLog);
             StartLog("mem_diff");
-            gMemTracker->DiffDump(*gLog);
             StopLog();
```

`DiffDump` was moved from *after* `StartLog("mem_diff")` to *before* it. The
diff dump is what the `mem_diff` log exists to contain; it now runs while that
log has not been opened, and `StartLog`/`StopLog` then bracket nothing. This is
precisely the "moved across a call" class, and the call it moved across is the
one that gives the write its destination.

**(ii) `src/system/bandobj/BandDirector.cpp:525` — a guard restructure that
widens what it gates.**

```diff
-                if (mCurWorld) {
-                    if (TheCrowdAudio)
-                        TheCrowdAudio->SetBank(mCurWorld);
+                if (TheCrowdAudio && mCurWorld) {
+                    TheCrowdAudio->SetBank(mCurWorld);
```

The two guards were **nested**, and the inner one covered only `SetBank`.
Flattening them pulls the *rest* of the block — `GetWorld()->SetSphere(...)`,
`mCurWorld->Handle(setup_midi_parsers_msg, false)` and `ClearLighting()` — under
`TheCrowdAudio` as well. Worse, `mCurWorld = dir;` executes immediately above
inside `if (dir)`, so `mCurWorld` is provably non-null there and the effective
new condition is **just `TheCrowdAudio`**. ⇒ with no crowd audio, entering a
venue silently skips MIDI-parser setup and lighting reset.

**(iii) `src/system/world/CameraShot.cpp:1395,1429` — two null checks deleted.**

```diff
-        if (TheHamWardrobe) {
-            TheHamWardrobe->ForceCrowdAnimationStart(mCrowdStateOverride);
-        }
+        TheHamWardrobe->ForceCrowdAnimationStart(mCrowdStateOverride);
```
```diff
-    if (TheHamWardrobe) {
-        TheHamWardrobe->ForceCrowdAnimationEnd();
-    }
+    TheHamWardrobe->ForceCrowdAnimationEnd();
```

Both `TheHamWardrobe` null guards were **removed** — a null-dereference where
the original was defensive.

★ **Note the direction is inconsistent, which is the tell that none of this is
reasoned.** Within the same four shards the permuter *added* null guards it did
not need (`Console.cpp` `mInput &&`, `MemTrack.cpp` `gMemTracker &&`,
`BandSongMgr.cpp` `mUpgradeMgr &&`, `Trans.cpp` `mTarget &&`) and *deleted* null
guards that were load-bearing (`CameraShot.cpp`, twice). Guards are being added
and removed by whichever way the byte count moves, not by whether the pointer can
be null.

**Adjudicated SAFE**, recorded so they are not re-investigated:

| site | change | why safe |
|---|---|---|
| `rndobj/Shader.cpp:478,957` | `IsActive() & 1` → `&& 1` | `bool IsActive() const` (`HiResScreen.h:58`) ⇒ `bool&1` ≡ `bool&&1` ≡ the bool. The *only* reason this differs from §4 is the operand's type. |
| `meta_band/BandSongMgr.cpp` | `maxCount <= GetCurSongCount()` → `GetCurSongCount() >= maxCount` | algebraically identical |
| `meta_band/BandSongMgr.cpp` | `i < count` → `(int)(int)(int)i < count` | no-op casts |
| `bandobj/BandWardrobe.cpp:1062` | `bestScore > 0` → `!(!(bestScore > 0))` | double negation |
| `rndobj/Console.cpp` | `Max(mCursor - 1, 0)` → ternary | identical for all inputs |
| `obj/Dir.cpp:992` | `static DataArray *objects = SystemConfig(...); objects->FindArray(s2);` → `SystemConfig("objects")->FindArray(s2);` | the `FindArray` result is discarded in **both** spellings, so the statement is dead either way; the only real change is `static`-once → per-call `SystemConfig`. Low severity, noted not fixed. |

**UNDETERMINED here, resolved in §5d:**
`bandobj/BandDirector.cpp:1830`, `playmode == coop_bk` → `(int)playmode == coop_bk`.

### 5d. ⛔⛔ A delegated full audit of the shard diff: 23 confirmed defects

The §5c work above covered ten reordering candidates. A read-only audit of the
**entire** 3,141-line shard diff was run in parallel and came back far larger
than this lane expected. The highest-severity findings are tabulated below; the
audit also cleared a long list of hunks as provably SAFE (algebraic no-ops,
`_outline_*` wrappers, `bool`-typed `&`/`&&` swaps, dead-store reorders), which
is recorded so nobody re-hunts them.

**Provenance, stated honestly.** Three of its findings (`MemTrack.cpp`,
`BandDirector.cpp`, `CameraShot.cpp`) were derived independently in §5c *before*
that audit reported, and the two accounts agree in every particular — which is
the only cross-validation available here and is why the remainder is reported
rather than merely filed. Its `MemTrack` analysis is **better than §5c's** and
supersedes it: `StopLog()` is `if (gLog) { RELEASE(gLog); … }`
(`MemTrack.cpp:49`), so the moved `DiffDump(*gLog)` does not merely write to the
wrong log — **it dereferences a null `gLog`.** Findings not independently
re-derived by this lane are labelled as such below.

**The headline number: 23 confirmed behaviour changes, of which 2 were compile
errors.** The other 21 compile clean in both builds. Highest severity:

| # | site | defect |
|---|---|---|
| 1 | `rndobj/Tex.cpp` | `PlatformBppOrder`: a 3-label fall-through `switch` rewritten as if/else gave `kPlatformXBox` and `kPlatformPC` **empty bodies** ⇒ on our actual target platform the function returns without writing `bpp` or `order` at all |
| 2 | `bandtrack/TrackPanel.cpp` | `GetNumPlayers()` body hoisted to a file-scope `auto _tmp2 = TheBandUserMgr->GetNumParticipants();` ⇒ a **dynamic static initialiser dereferencing `TheBandUserMgr` before it exists**, and the count is then frozen forever |
| 3 | `bandobj/BandList.cpp:68` | `mBandListRev = gRev <= 0x11` — precedence: the member receives **0/1**, never the revision, neutering six downstream `mBandListRev >=` gates |
| 4 | `meta_band/SongStatusMgr.cpp` | `stream >> count` moved **after** the `for (i < count)` loop ⇒ loop bound read **uninitialised** |
| 5 | `game/VocalPlayer.cpp` | `UnpackFloats`: both stores moved past both `>>= 8` ⇒ elements 0,1,2 all extract **byte 2** |
| 6 | `meta_band/NextSongPanel.cpp` | `done:` label moved *before* the assignment it existed to skip ⇒ `Exiting()` **always returns true** |
| 7 | `synth_xbox/FFT.cpp` | the `malloc` pointer `temp` is now advanced in-place ⇒ **`free()` on a non-malloc pointer**; the twin function 200 lines below still has the correct `dst1` |
| 8 | `rndobj/Env.cpp:60` | `Save` field order no longer matches `Load` ⇒ two `bool`s and a `float` round-trip as garbage (byte total coincidentally re-aligns, so the stream does not desync) |
| 10 | `meta/PreloadPanel.cpp:100` | `mContentNames.clear(); mSongDoesNotExist = false;` moved **after** the block that populates them ⇒ every Load wipes its own result |
| 12 | `os/Debug.cpp` | `Modal(t, …)` takes `t` **by reference** and mutates it; the `if (t != kModalFail)` test was moved *before* the call ⇒ always false |
| 13 | `bandobj/BandIKEffector.cpp` | `QuatXfm &_ref0 = outQuat;` → `auto _val0 = outQuat;` ⇒ `auto` deduces **by value**, so the out-parameter is never written |
| 15 | `world/SpotlightDrawer_NG.cpp:637` | `Normalize(toCam,toCam)` → `Normalize(toCamOrig,toCamOrig)` — but `toCamOrig` is the deliberately *un-normalised* copy every downstream line uses |
| 16 | `bandobj/VocalTrackDir.cpp:971,987` | packed RGBA `int` → `unsigned char` ⇒ `>>8`, `>>16`, `>>24` all yield **0**; three of four colour channels become 0.0 |

Plus guard defects (18–23): an unsigned range-check idiom disarmed by a cast back
to `int` (`CharBonesSamples.cpp`), a guard made vacuous by
`(unsigned)idx >= 0` (`OvershellSlot.cpp`), and §5c's three.

**★ The single most actionable pattern.** Five of these are *the same defect
shape as §2's DirLoader bug* — a save, snapshot or initialiser moved across the
call that changes what it captures (items 4, 10, 12, 17 `BoxMap.cpp`, and
`Instance.cpp`'s `sPersistRev`). If the permuter's transform set contains
"hoist/sink an assignment across a call boundary", that one rule accounts for a
disproportionate share of the damage and disabling it is worth more than fixing
any individual site.

**★ Resolving §5c's UNDETERMINED, and a native-specific hazard.**
`(int)playmode == coop_bk` and `(int)s == real_guitar` are **fine on ILP32
X360** but on the **LP64 native target** truncate an interned `Symbol` to 32
bits, so two distinct symbols can compare equal. Same for the other
`Symbol`-to-`int` casts the sweep introduced. This is a defect class that is
invisible to the match build **by construction**.

**⚠ And one that lands inside a file this lane edited:**
`char/CharBoneDir.cpp` **`PostLoad`** carries a *second*, separate reorder —
`BinStreamRev d(bs, bs.PopRev(this))` split and moved after
`ObjectDir::PostLoad(bs)`, which recursively post-loads children that push and
pop revs. **This lane's §1 fix covers `PreLoad` only and does NOT cover this.**
It is deliberately left alone: establishing whether the `PopRev` ordering
actually changes requires adjudication this lane did not do, and an unverified
"fix" to a stream-ordering site is worse than a reported one.

### 5e. Recommendation — this is a revert decision, and it is the owner's

**This lane fixed four sites and is reporting nineteen more. It did not fix them
and does not think it should.** At 23 confirmed defects across 96 files, the
question is no longer "which hunks are wrong" but whether the four shards should
stand at all. That is explicitly the project owner's call, so it is put here as a
finding rather than acted on.

The case for reverting all four shards wholesale:

- **The stated yield does not survive contact with `report.json`.** The shards
  claim +956.4 / +771.0 / +987.3 / +1095.7 "matched bytes" — the permuter's own
  **fractional** score. Every target was **sub-75**, and `matched_code` is
  all-or-nothing per row at `fuzzy == 100`, so their contribution to the graded
  key is approximately **zero**. This lane's A/B is direct evidence of the
  mechanism: reverting four of their hunks in the native build moved
  `matched_code` by **exactly 0 bytes**.
- **The defect rate is not a tail.** 23 confirmed in 96 files, in a change set
  whose own commit message says nothing was hand-written or human-reviewed.
- **The two compile errors were luck.** 21 of 23 compile clean; the native gate
  found the set only because two of them happened to trip `-Werror`. Nothing in
  the pipeline was looking for the other 21, and nothing would have.

⇒ **The honest summary is that the sweep bought approximately zero graded bytes
and cost 23 behavioural defects.** If that trade is declined, the cleanest action
is `git revert` of the four merges, after which §1–§4's `#ifdef HX_NATIVE` blocks
should be removed as well — they exist only to preserve hunks that would no
longer be there.


---

## 6. Measurement

### 6a. The gate

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

with `build: rc=0, 0 error line(s) + 0 linker diagnostic(s) / 0 distinct, 0 failed edge(s), 0 warning(s)`.

**One round. No hidden errors appeared.** The lane expected at least a second
round — the gate stops after a bounded number of failed edges, so the 23 edges
it reported could have been masking more. They were not: all 23 failed edges
across 17 targets were downstream of the **five** diagnostics in **four** files.

The reason it took one round rather than two is worth recording, because it is a
reusable rule. The incoming brief named three sites; the gate's own
`--- distinct diagnostics (5) ---` block named five. Had the brief been trusted,
`BandWardrobe.cpp` would have been a second round. **Read the instrument's
enumeration, not the summary of it.**

### 6b. Whole-binary A/B

`tools/ab_measure.py --worktree /home/free/tmp/wt-natfix --from-dirty`, both legs
settled to a zero-work build, report cache wiped per leg, patch sha256/16
`afb2a7b7fa611ef4`, classified `kinds=['source']`.

| measure | leg A | leg B | Δ |
|---|---:|---:|---:|
| `matched_functions` | 42,256 | 42,256 | **+0** |
| `masked_equal` | 22,911 | 22,911 | **+0** |
| honest (`matched − masked_equal`) | 19,345 | 19,345 | **+0** |
| `matched_code_percent` | 36.814487 | 36.814487 | **+0.000000pp** |
| `matched_code` bytes | — | — | **+0 B** |
| fuzzy | 48.962635 | 48.962635 | **+0.000000pp** |
| units at 100% (`mpn`) | 150 | 150 | **+0** — 0 reached, **0 fell off** |
| units at 100% (all-rows-`fuzzy`) | 122 | 122 | **+0** — 0 reached, **0 fell off** |

Ruler: `functionRelocDiffs=name_check`, resolved from `objdiff.json` options by
the tool, i.e. the shipped graded ruler that `report.json` scores on.

**`none`-ruler control:** `matched=44491 code%=43.167587` on **both** legs ⇒
Δ0 there too. (The tool correctly labels the alias-shape check
`NOT_APPLICABLE` for a source patch — default-UP/none-FLAT is also the
wrong-callee-fix signature, so that guard is only adjudicable on a map-only
patch. It is reported here as a plain both-rulers-flat reading, not as an alias
clearance.)

**Not absent-vs-absent:** leg B performed **14 MSVC recompiles** (first
iteration, read before any report step), so the edits were genuinely compiled.
Leg A settled to 0.

**Why Δ0 was predicted rather than hoped for.** §0.2's construction makes the
match build's preprocessed token stream identical, so Δ0 is the only result
consistent with the edits being what they claim to be. The A/B is therefore
functioning here as a *falsification test of the construction*, not as a search
for a gain: a nonzero reading on any key would have meant an `#else` arm did not
reproduce the sweep's spelling exactly. Every key, on both rulers, read exactly
zero.

---

## 7. What this lane did NOT do

- **It reverted nothing.** Every defect above is still present in the match
  build exactly as the sweep landed it. Only the native build is corrected, and
  only via `#ifdef HX_NATIVE`.
- **It did not reconstruct DirLoader's save/restore** (§2) — dormant in native,
  and repairing it is a revert.
- **It did not touch Trans.cpp §3c**, the added `mTarget &&` conjunct, because
  "safer than retail" is a judgement call the owner should make.
- **It did not audit the sweep beyond the four shards' own diff.** The 255-target
  sweep attempted far more than it landed; nothing here says anything about the
  hunks it discarded.
- **It did not re-price the sweep's match gains.** Their commit messages quote
  the permuter's own **fractional** score (e.g. "+956.4 matched bytes"), which is
  not `report.json`'s `matched_code`. Every target was sub-75, and `matched_code`
  is all-or-nothing per row at `fuzzy == 100`, so their contribution to that key
  is approximately zero either way.
