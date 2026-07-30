# Lane BS-2 · the ScoreDisplay 3-cycle, and what the attribution-carve channel actually pays

Date: 2026-07-30 · branch `laneBS2` · worktree `~/tmp/laneBS2/wt` (from `51e61cf7`)
Sole owner of `config/45410914/splits.txt` + `scripts/target_symbol_map.json` for this lane.

A-leg in this worktree (full build, `report.cache` removed, `symbols.txt` restored,
post-split): **matched 40898 / masked_equal 1509 / honest 39389**. Main's reference
was 40896/1509 — inside the ±2 split-churn floor, so a clean A-leg.

| | predicted | measured |
|---|---|---|
| matched | +4 | **+5** (40903) |
| masked_equal | 0 | **0** (1509) |
| honest | +4 | **+5** (39394) |

---

## 1. The assigned target was real, and it pays nothing

The BQ-1 handoff named `0x8231f680 → ScoreDisplay.cpp`. The identification is
correct and I confirmed it three independent ways:

1. **String.** `fn_8231F680` builds the literal at `0x82032328` = `"ScoreDisplay"`.
2. **Callers.** A class's `ClassName()`/`SetType()` call their *own*
   `StaticClassName()` — BQ-1 job A §3's method. `?ClassName@ScoreDisplay@@`
   `bl`s it at +0x14, `?SetType@ScoreDisplay@@` (the 316-byte `types`/`objects`
   OBJ_SET_TYPE fingerprint) at +0x4c, `?Init@ScoreDisplay@@` at +0x10.
3. **Spatial.** `/O1` without LTCG preserves per-obj contribution grouping, and
   `0x8231F6F8` — the very next byte — is already ScoreDisplay.cpp.

It is also a **3-cycle**, exactly as advertised, but not the one I expected:

| addr | true identity | was mapped as | hosted by |
|---|---|---|---|
| `0x8231f680` | `?StaticClassName@ScoreDisplay@@` | `…@RndTex@@` | LiveCameraInput.cpp @ **100%** |
| `0x8256e7a8` | `?StaticClassName@AppScoreDisplay@@` | `…@ScoreDisplay@@` | ScoreDisplay.cpp @ **100%** |
| `0x8256e828` | `?StaticClassName@AuditionSessionPanel@@` | `…@AppScoreDisplay@@` | MetaPanel.cpp @ **100%** |

`0x8256e7a8` builds the *same* `"ScoreDisplay"` literal because retail's
`AppScoreDisplay` declares `OBJ_CLASSNAME(ScoreDisplay)` with no `App` prefix —
the identical finding BQ-1 made for `AppMiniLeaderboardDisplay`. Its
`ClassName`/`SetType` call it at the *same* +0x14/+0x4c offsets. `0x8256e828`
builds `"AuditionSessionPanel"` (`0x8209B660`) and is neither.

**All three were already at 100%.** Closing the cycle correctly is therefore
**metric-neutral by construction** — the false credit merely relocates onto the
right body. Measured per-unit: ScoreDisplay 0, MetaPanel 0, LiveCameraInput −1.
The −1 is real drained false credit (`RndTex` was never here; the true
`RndTex::StaticClassName` is `0x82273860`, proven by `?ClassName@RndTex@@`
`bl`ing it at +0x14).

**So the assigned carve, done correctly, is a net −1.** That is the honest
result and the main thing to carry forward: in *this* channel, correctness and
yield are orthogonal.

## 2. Why: these bodies are one rigid template, and the metric cannot see names

`OBJ_CLASSNAME(X)` defines `X::StaticClassName()` **inline in the class body**, so
its COMDAT is emitted by every TU including the header; the linker keeps one and
places it in that obj's contribution. Retail compiles it to a rigid
22-instruction template whose only varying fields are three relocations (guard
word, cached `Symbol`, class-name string). objdiff runs `functionRelocDiffs=None`.

⇒ **All 453 such bodies in the binary are byte-identical under masking.** A body
scores 100.0% iff the unit it is pinned in supplies a COMDAT with the *mapped*
name — whether or not that name is correct. Byte-similarity is worthless here;
only the string + caller + spatial evidence identifies them.

The corollary is the whole lever: **yield is not in fixing names, it is in bodies
that currently earn ZERO.** Two populations qualify — bodies in unpinned address
space, and bodies pinned in a unit that cannot supply their mapped name. I built
`scripts/harvest/staticclassname_census.py` to enumerate both (it recovers each
body's class-name string from the template and joins map × splits × per-obj
COMDAT supply). It found 9 unpinned and 10 dead-pinned; four were clean.

## 3. The four that paid

| addr | name | from → to | Δ |
|---|---|---|---|
| `0x8264bce8` | `?StaticClassName@AppMiniLeaderboardDisplay@@` | NextSongPanel.cpp → AppMiniLeaderboardDisplay.cpp | **+1** |
| `0x827b78f8` | `?StaticClassName@StoreArtLoaderPanel@@` | StandingStillGestureFilter.cpp → StoreArtLoaderPanel.cpp | **+1** |
| `0x82324ca0` | `?StaticClassName@PlayerDiffIcon@@` | *unpinned* → PlayerDiffIcon.cpp | **+2** |
| `0x824a8ec8` | `?StaticClassName@NgMat@@` (**inserted**) | *unpinned* → Mat_NG.cpp | **+2** |

`0x8264bce8` is BQ-1 job A's own leftover: §3 proved it is App's `StaticClassName`
and repointed it, but could not carve, so it sat at **0.0%** in NextSongPanel —
which supplies no such COMDAT. `AppMiniLeaderboardDisplay.cpp`'s own span begins
at `0x8264bd68`, 0x28 bytes later, so spatial and caller evidence agree.

`0x827b78f8` was `StandingStillGestureFilter.cpp`'s *only* `.text` block and read
**0.0%** (unit total=1 matched=0) — a whole splits entry earning nothing.

`0x82324ca0` and `0x824a8ec8` were unpinned and immediately contiguous with
PlayerDiffIcon.cpp's and Mat_NG.cpp's existing spans, so both were block-start
extensions rather than new blocks. `0x824a8ec8` is `NgMat` (proven by
`?ClassName@NgMat@@` at +0x14), a name absent from the map — a clean insert.
Each paid **+2**, not +1: the 32-byte `??__F` atexit stub that trails every
`StaticClassName` came in with the extension and byte-fallback paired.

## 4. Decisions I made deliberately, and what I did NOT do

- **Atexit stubs stay with their current unit.** Every retail `StaticClassName`
  is trailed by a 32-byte `??__F` that clears its guard bit. **We emit none** —
  our `Symbol` has no user-declared destructor, so no `??__F` COMDAT exists in
  any of our 1094 objs (verified). They only ever pair by byte-fallback, so
  hosting one gains nothing while moving one costs a real paired row. I
  therefore carved the 88-byte bodies **only**, leaving `0x8231F6D8` with
  LiveCameraInput and `0x8264BD40` with NextSongPanel. This is knowingly
  imperfect attribution, traded for 2 matched — the same call BQ-1 job B made
  when it split SkeletonClip's span around the RndText body.
- **`0x82802530` "UIProxy" left alone.** Unpinned, and `?StaticClassName@UIProxy@@`
  is supplied by exactly one obj — but the body sits *between two UI.cpp blocks*
  (`…24b0` and `82802588…`), so retail's contributor is **UI.cpp**, not
  UIProxy.cpp. Claiming it for UIProxy.cpp would buy +1 by putting the pin in a
  unit the evidence says did not contribute it. Declined. The legitimate route is
  a source change (`#include "ui/UIProxy.h"` in UI.cpp), which has codegen blast
  radius and belongs to a source lane, not a splits lane.
- **The Tex/Mat permutation left alone.** `0x82273860` is mapped `DxTex` but
  `?ClassName@RndTex@@` `bl`s it at +0x14, so it is `RndTex`; `0x827347d0`
  (string `"Tex"`, in ShaderMgr.cpp) is mapped `RndMat`. That is a multi-link
  chain in which every link is metric-neutral (each body already earns 100% and
  every candidate host already supplies its current name). Correctness-positive,
  yield-zero, and long — deliberately deferred rather than risk a net-negative
  lane. See the dossier.
- **The three MeshDeform bodies left alone** (`0x8240dcb8` "Light", `0x8240dd38`
  "Fur", `0x8240ddc0` "ParticleSys"). They read 0% and each has 3–13 possible
  supplier units, so they *look* like +3. But they sit inside MeshDeform.cpp's
  own span with no caller or spatial evidence pointing anywhere else, and for a
  header-inline COMDAT the contributing TU is exactly what placement tells you.
  Moving them to whichever unit happens to supply the name would be metric-gaming
  with no identification behind it. Declined.
- **Orphan `Rnd.cpp:` noted, not removed.** It still emits
  `Missing configuration for Rnd.cpp` from `configure.py`. It is *not* empty — it
  pins `.text 0x8251BF98–0x8251BFA8` — but `Rnd.cpp` is absent from
  `objects.json` (only `system/rndobj/Rnd.cpp` exists), so it gets no compile
  edge and can never match. It was not in my way. Deleting it is safe-but-zero;
  the interesting question is whether that 16-byte block belongs to
  `system/rndobj/Rnd.cpp`, which needs its own identification.

## 5. A mistake worth recording

I created a **duplicate `system/meta/StoreArtLoaderPanel.cpp:` heading**: I
grepped `objects.json` for the unit but never grepped `splits.txt`, and an entry
already existed further down. The split run did not fail — it silently
**unioned** both blocks into *both* headings. Caught it by reading the
post-split diff. Deduped, re-split, re-measured. **Lesson: before adding a new
unit heading, grep `splits.txt` for it, not just `objects.json`** — the failure
mode is silent duplication, not an error.

The dedupe re-measured 40903 where the pre-dedupe run read 40905. Every one of
my eight touched units was **byte-for-byte identical between the two runs**
(checked row by row), so the 2 is split-churn elsewhere — changing the unit list
reshuffles objdiff's global fuzzy byte-fallback pairings. Banked the
conservative, reconciled **+5**, which sums exactly over the per-unit deltas
(−1 +1 +1 +2 +2).

## 6. Dossier for the next cycle

Run `python3 scripts/harvest/staticclassname_census.py --project-dir <wt>` — it
reprints the live pool. As of this landing, 7 unpinned + 8 dead-pinned remain.
Ranked by tractability:

1. **The Tex/Mat/NgMat chain** (correctness, yield ~0, but it unblocks names).
   `0x82273860` = `RndTex` not `DxTex` (proven: `?ClassName@RndTex@@` +0x14).
   `0x827347d0` (string "Tex", ShaderMgr.cpp) is then the `DxTex` candidate.
   `?StaticClassName@RndTex@@` is currently **unused in the map** — this lane
   vacated it — so the first link is a free repoint with no delete needed.
2. **`0x8227a728` / `0x8227a7a8` both string `"Song"`, unmapped, unpinned.** Two
   distinct bodies with the same class string ⇒ a base/subclass pair exactly like
   ScoreDisplay/AppScoreDisplay. The caller test at +0x14 will separate them.
   Needs a supplier check before it is worth anything.
3. **`0x82570428` "BandLabel"** and **`0x82593fa8` "acc_secretdesc"**, unmapped +
   unpinned. Same recipe: caller test → name → supplier → pin.
4. **`0x82802530` "UIProxy"** — only reachable via the UI.cpp include change
   described in §4. Hand to a source lane, not a splits lane.
5. **Do NOT re-hunt** the three MeshDeform bodies or `0x8236a8a8`
   "CharTransCopy" / `0x826d87e0` / `0x827a6c58` / `0x82b5e040` / `0x82b8ff80` —
   all have **zero suppliers** in our tree, so no pin can make them pay until the
   owning class is actually compiled.

Method reminder for whoever picks this up: **check what the current owner is
earning before you cut.** Three of the five candidates I inherited or found were
already at 100% on the wrong body, and moving those is a wash. The ones that pay
are the ones reading 0.0%.
