# Permuter sweep revert — lane PERMREVERT, 2026-08-31

**What this lane did:** reverted the four permuter-sweep shard merges and lane
NATFIX's `#ifdef HX_NATIVE` repair blocks, executing the recommendation in
[`NATIVE_GATE_REPAIR_2026-08-27.md` §5e](NATIVE_GATE_REPAIR_2026-08-27.md).
The decision to revert was the project owner's; this lane executed it, verified
it, and priced it.

**The headline, measured rather than argued:**

> The four shards were worth **−4 bytes** of graded `matched_code` and
> **+1 matched function**. Reverting them costs **4,336.9 bytes of fractional
> `fuzzy` credit (−0.042328 pp)** and **gains one matched function**, while
> closing **23 confirmed behaviour defects**. Zero units fell off 100% on
> either ruler.

The audit's central claim — that the sweep "bought approximately zero graded
bytes and cost 23 behavioural defects" — is **confirmed, not contradicted.**
The measured graded cost is 4 bytes out of a 10,245,956-byte denominator.

---

## 1. What was reverted

Five commits, on branch `perm-revert` off main `05ff76aa`, newest-listed-first:

| commit | reverts | what |
|---|---|---|
| `a3134311` | `648d8154` (src `0c36bc01`) | permuter sweep shard 0 — 31 sub-75 targets, "+956.4 matched bytes" |
| `21daa110` | `79d23d0f` (src `cd42a2d3`) | permuter sweep shard 1 — 31 sub-75 targets, "+771.0 matched bytes" |
| `5780555a` | `4f37cd30` (src `9d99331a`) | permuter sweep shard 2 — 31 sub-75 targets, "+987.3 matched bytes" |
| `22579df5` | `1e091edf` (src `58172bae`) | permuter sweep shard 3 — 38 sub-75 targets, "+1095.7 matched bytes" |
| `2b5a51b1` | `a46a29f4` | lane NATFIX's four `#ifdef HX_NATIVE` repair blocks |

NATFIX was reverted **first**, deliberately. Its blocks exist only to give the
native build correct code while preserving the sweep's exact spelling in an
`#else` arm for the match build; with the shards gone they would preserve
nothing. §5e says so explicitly. Reverting it first also cleared the four files
it shared with the shards (`BandWardrobe.cpp`, `CharBoneDir.cpp`,
`DirLoader.cpp`, `Trans.cpp`), and the four shard reverts then applied with
**zero conflicts**.

Each shard is a merge commit, so `git revert -m 1` was used. All five reverts
were clean; nothing required manual conflict resolution.

### 1.1 Scope, established two independent ways

The shard set is **96 files**, computed as the union of `git diff <merge>^1
<merge>` over the four merges — i.e. each shard against **its own parent**.

⚠ A naive `git diff 24ef42e9 1e091edf` spans intervening lanes and is
**contaminated**: it falsely implicates `utl/Cache.h` and `utl/Cache_Xbox.cpp`,
which are not in any shard. Neither appears in the 96.

**Completeness was verified structurally, not by spot-check.** After the
reverts, **95 of the 96 files are byte-identical to `648d8154^1`** — the
pre-all-shards baseline. That is a stronger statement than any per-defect
inspection: every defect the sweep introduced into those 95 files is gone by
construction, because the files are the pre-sweep files.

The 96th is `src/system/os/ContentMgr_Xbox.cpp`, and its residual difference is
**exactly** lane PINHOME's fix (see §2).

A second, independent instrument agrees: across the whole range
`648d8154..05ff76aa`, exactly **one** non-shard, non-NATFIX commit touches any
of the 96 files — `bbdb7dc5`, PINHOME. Content-based and history-based checks
give the same answer.

---

## 2. The one collision — PINHOME's `IsCorrupt` survived

`src/system/os/ContentMgr_Xbox.cpp` is the only shard file also carrying recent
verified work: **`bbdb7dc5`, `XboxContentMgr::IsCorrupt`, worth +140 B**, taking
`0x82520668` to 100%.

The two regions are disjoint — PINHOME at lines ~265-290 (`IsMounted` /
`IsCorrupt`), the shard hunks at ~452-542 (`PollRefresh`) — so git auto-merged
the revert. **This was verified rather than assumed**, three ways:

1. The post-revert `IsCorrupt` body is the retail form PINHOME established:
   unconditional `dynamic_cast<XboxContent *>(*it)->IsCorrupt()`, **no null
   check**, **no `displayName` write**.
2. `git diff bbdb7dc5 HEAD -- <file>` has exactly three hunks, all headed
   `void XboxContentMgr::PollRefresh()` (lines 460, 468, 532), and mentions
   `IsCorrupt`/`IsMounted` **zero** times.
3. `git diff 648d8154^1 HEAD -- <file>` is precisely and only PINHOME's hunk.

✅ **PINHOME's +140 B is intact.**

---

## 3. Defects verified closed

All 23 confirmed defects from §5c/§5d live in shard files, and **every file
named in that audit is in the byte-identical-to-pre-shard set** (checked
individually: `Tex.cpp`, `TrackPanel.cpp`, `BandList.cpp`, `SongStatusMgr.cpp`,
`VocalPlayer.cpp`, `NextSongPanel.cpp`, `FFT.cpp`, `Env.cpp`,
`PreloadPanel.cpp`, `Debug.cpp`, `BandIKEffector.cpp`,
`SpotlightDrawer_NG.cpp`, `VocalTrackDir.cpp`, `CharBonesSamples.cpp`,
`OvershellSlot.cpp`, `MemTrack.cpp`, `BandDirector.cpp`, `CameraShot.cpp`,
`BoxMap.cpp`, `Instance.cpp`, `CharBoneDir.cpp`, `DirLoader.cpp`, `Trans.cpp`,
`BandWardrobe.cpp`, `Dir.cpp`). None is `ContentMgr_Xbox.cpp`.

⇒ **All 23 are closed by construction.** The spot-checks below are
*confirmation* of that structural argument, not the argument itself.

| # | site | post-revert state |
|---|---|---|
| 1 | `rndobj/Tex.cpp` `PlatformBppOrder` | ✅ back to the fall-through `switch`; `kPlatformXBox`/`kPlatformPC` share the `kPlatformPS3` body, so **both write `bpp` and `order`** |
| 2 | `bandtrack/TrackPanel.cpp` | ✅ `int TrackPanel::GetNumPlayers() const { return TheBandUserMgr->GetNumParticipants(); }` — a plain member fn; no file-scope `auto _tmp2` dynamic initialiser |
| 3 | `bandobj/BandList.cpp:68` | ✅ `mBandListRev = gRev;` with `if (mBandListRev <= 0x11)` as a separate test — the precedence bug that fed the member `0/1` is gone, and the six downstream `mBandListRev >=` gates are live again |
| 7 | `synth_xbox/FFT.cpp` | ✅ `temp` holds the `malloc` result and `free(temp)` receives it; `dst1` carries the advance. Both twins (`:453/:539/:652` and `:690/:781/:880`) have the correct shape |
| 8 | `rndobj/Env.cpp` | ✅ **positively round-trip-checked**, not merely "differs from the sweep": `Save` writes `mFogEnable, mAnimateFromPreset, mFadeOut, mFadeStart, mFadeEnd` and `BEGIN_LOADS` reads that exact order |
| — | `char/CharBoneDir.cpp` **`PreLoad`** | ✅ `LOAD_REVS(bs); ASSERT_REVS(4,0); ObjectDir::PreLoad(bs);` — the canonical order `obj/Dir.cpp:1248` establishes as the oracle; the revision transposition is gone |
| — | `char/CharBoneDir.cpp` **`PostLoad`** | ✅ `BinStreamRev d(bs, bs.PopRev(this));` is back **before** `ObjectDir::PostLoad(bs)` recurses into children that push/pop revs. **This is the site NATFIX's §1 fix did not cover** — only the revert closes it |

### 3.1 Tree-wide shape scan re-run

The audit's §5a signatures, re-run over `src/` (excluding `stlport/`, `xdk/`)
after the revert:

| shape | before | after |
|---|---:|---:|
| `!!(x) == <constant>` | 2 | **0** |
| `(x && <mask>) == <mask>` | 1 | **0** |
| self-assignment `x = x;` | 2 | **1** |

Both families the audit confined to the sweep are now **absent tree-wide**. The
surviving self-assign is `rndobj/MultiMesh.cpp:363` — **pre-existing, from lane
AG2 (`0ac748fc`), not the sweep** — left untouched as out of scope.

---

## 4. What the revert did NOT close, and what it re-introduced

Recorded so none of it is mistaken for sweep damage or re-hunted.

- **`rndobj/MultiMesh.cpp:363` — `proxy = proxy;`** survives. It is lane AG2's,
  not the sweep's, and it is a true no-op (the enclosing loop assigns `proxy`
  every iteration). Deliberately not fixed. ⚠ It still carries the
  gate-coverage consequence §5b describes: `MultiMesh.cpp` is compiled by **no**
  native target, so a gate PASS does not prove it is clang-clean, and this line
  will break the native build the day the TU is linked.

- **⚠ `rndobj/Trans.cpp` §3c is re-introduced, deliberately.** NATFIX kept the
  sweep's added `mTarget &&` conjunct in *both* builds because it suppressed
  real UB. The revert removes it, so `ApplyDynamicConstraint`'s
  `kConstraintShadowTarget` arm again declares `Transform tf;`, writes it only
  inside `if (mTarget)`, and then unconditionally consumes it via
  `Multiply(sShadowPlane, tf, pl)` — an **uninitialised read when `mTarget` is
  null**.

  This is the correct outcome for a matching decomp: that read is what retail
  does, and "safer than retail" is a divergence. It is recorded here because it
  is a real latent hazard for the **native** port, where it is a live
  uninitialised read rather than a byte-matching goal. If the native build ever
  needs it guarded, the house pattern is an `#ifdef HX_NATIVE` block — not a
  shared edit.

- **The sweep's discarded attempts are not addressed.** The 255-target sweep
  attempted far more than the 131 targets it landed. Nothing here says anything
  about the hunks it never landed.

- **No claim is made about permuter policy.** §5d's actionable pattern — "hoist
  or sink an assignment across a call boundary" accounts for a disproportionate
  share of the damage (items 4, 10, 12, 17 and `Instance.cpp`'s `sPersistRev`) —
  is a change to the permuter's transform set, in a different repo, and is out
  of this lane's scope.

---

## 5. Measurement

`python3 tools/ab_measure.py --worktree /home/free/tmp/wt-permrevert --patch
<revert.diff>`, patch sha256/16 `634af0dd82c4032d`, classified `kinds=['source']`.

The worktree was parked at main `05ff76aa` (detached) with branch `perm-revert`
preserved, so **leg A is main and leg B is the reverted tree** — the signs below
read as the cost of reverting, with no mental inversion. `--patch` rather than
repeated `--revert`: the tool refuses repeated `--revert` flags (lane EE2-C got a
confident number for one of three), and five reverts cannot be expressed as one.

Ruler: `functionRelocDiffs=name_check`, resolved by the tool from
`objdiff.json` options — the shipped graded ruler `report.json` scores on.
Denominator `total_code = 10,245,956` / `total_functions = 69,219`, read from
the run's own report, not inherited.

| measure | leg A (main) | leg B (reverted) | Δ |
|---|---:|---:|---:|
| `matched_functions` | 42,256 | 42,257 | **+1** |
| `masked_equal` | 22,911 | 22,912 | +1 |
| honest (`matched − masked_equal`) | 19,345 | 19,345 | **+0** |
| `matched_code` bytes | 3,771,996 | 3,771,992 | **−4 B** |
| `matched_code_percent` | 36.814487 | 36.814445 | **−0.000042 pp** |
| `fuzzy_match_percent` | 48.962635 | 48.920307 | **−0.042328 pp** |
| units at 100% (`mpn`) | 150 | 150 | +0 — 0 reached, **0 fell off** |
| units at 100% (all-rows-`fuzzy`) | 122 | 122 | +0 — 0 reached, **0 fell off** |

**`none`-ruler control:** leg A `matched=44,491 code%=43.167587`, leg B
`matched=44,492 code%=43.167550` ⇒ **Δ`matched_code` = −4 B, Δcode% =
−0.000037**. Both rulers move together and by the same 4 bytes, which is the
correct signature for a real instruction-level change rather than a naming or
alias effect. (The tool labels the alias-shape check `NOT_APPLICABLE` for a
source patch — with source present, default-UP/none-FLAT is also the
wrong-callee-fix signature, so that guard is only adjudicable on a map-only
patch.)

**Not absent-vs-absent:** leg B performed **142 MSVC recompiles** on its first
iteration, read from the build log before any report step. Both legs settled to
a zero-work build (leg A 0 recompiles, leg B settled in 2 iterations). No
headers are in the 96, so there was no PCH cascade.

**Per-unit movement** — one improvement, one regression, net +1, reconciling
exactly with the whole-binary Δ:

- `+2` `default/band3/tour/GigFilter` (10 → 12)
- `−1` `default/CharNeckTwist` (18 → 17)

### 5.1 The fuzzy delta is the permuter's own score, and that is the whole story

Converted to bytes against this run's denominator:

```
Δfuzzy = −0.042328 pp  ×  10,245,956 B / 100  =  −4,336.9 B
```

The four shards claimed `956.4 + 771.0 + 987.3 + 1095.7 = ` **+3,810.4**
"matched bytes". **The measured fuzzy loss is 1.14× that figure** — the same
quantity, to the accuracy one should expect once neighbouring rows are
perturbed too.

⇒ **This is a direct measurement of the mechanism the audit asserted.** The
sweep's entire yield was *fractional* `fuzzy` credit on rows that were all
sub-75 and stayed sub-75. Because `matched_code` is all-or-nothing per row at
`fuzzy == 100`, essentially none of it ever reached the graded key: the whole
3,810-byte headline converts to **−4 bytes** of `matched_code`.

**Prediction and result, stated in that order** (the lane pre-registered this
before running): Δ`matched_code` ≈ 0 and Δ`fuzzy` ≈ −0.03 to −0.04 pp, derived
from the shards' claimed fractional bytes over `total_code`. Measured: −4 B and
−0.042328 pp. In-band and correct in sign, so the audit's model of *why* the
yield was illusory is not merely consistent with the data — it predicted it.

⚠ The brief instructed this lane to stop and report loudly if Δ`matched_code`
came back **large and negative**, since that would mean the shards bought real
graded bytes and the trade deserved re-examination. **It did not.** −4 B is not
a contradiction of the audit; it is the strongest available confirmation of it.

### 5.2 The revert is net-positive on the count ruler

`matched_functions` went **up** by one. This is worth stating plainly because it
inverts the intuitive framing: the sweep did not merely fail to buy graded
bytes, it was **holding one matched function hostage**. `Δhonest` is +0 (the
gained row is `masked_equal`), so the honest floor is unmoved.

---

## 6. Gate and patch state

```
[patch-state] OK: tree is a fixed point of 6 post-compile passes
[patch-state] relpath-paired passes (guard/bool_mangle/atexit) see 347/1048 declared target-base pairs (33.1%); 701 invisible, 3 paired against a different target obj than objdiff.json names
```

The 33.1% coverage line is the **pre-existing documented limit** recorded in
CLAUDE.md (three of the six passes pair target↔base by relpath), not a finding
of this lane.

Native gate, run as this lane's **last** action, after the final commit:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

Note that the gate passing here is *expected and slightly uninformative*: the
revert removes the two compile errors' source lines entirely, and NATFIX's
blocks — which were what made the gate pass on main — go with them. The gate
confirms the combination is coherent, which is exactly what it is for.

⚠ Per §5b, a PASS does **not** license "all of `src/system` is clang-clean" —
`rndobj/MultiMesh.cpp` is in no native target's link closure and still carries
its own self-assign (§4).

---

## 7. What this lane did not do

- **It did not touch main.** All work is on `perm-revert`; nothing was pushed.
- **It did not fix `MultiMesh.cpp`** (§4) — out of scope, not the sweep's.
- **It did not re-guard `Trans.cpp`'s shadow-target arm** (§4). Restoring
  retail's uninitialised read is the correct matching outcome; the native-side
  hazard is recorded, not patched.
- **It did not audit the sweep's unlanded attempts**, or change permuter policy.
- **It did not re-derive the 23 defects.** They were taken from
  `NATIVE_GATE_REPAIR_2026-08-27.md` §5c/§5d and verified *closed*; this lane
  independently re-confirmed six of them plus the tree-wide shape scan, and
  established the remainder structurally via byte-identity to the pre-shard
  baseline.
