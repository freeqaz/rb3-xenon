# The `MILO_DEBUG` force-define trap

> **Status: current (2026-07-31, lane CB-10).** Written against *this* tree, not
> ported from dc3-decomp — no compiler-conditional caveat applies. The census
> counts below are a dated snapshot; the *mechanism* and the adjudication
> procedure are evergreen. Re-run the census (below) before trusting the counts.

## TL;DR

`src/macros.h:3` unconditionally does `#define MILO_DEBUG`, tree-wide, with no
build-flag escape. Source ported from the **rb3-Wii dev decomp** guards real
behaviour with `#ifdef MILO_DEBUG`, and **retail took the false branch**. So an
inherited guard body is silently compiled *in*, and nothing in the build notices.

**But most such guards turn out to be harmless, and at least one is actively
correct — a blanket removal is measured NET NEGATIVE.** Adjudicate per site.

## ★ The premise that is usually stated, and is wrong

You will see this claim in commit messages and in header comments across the
tree:

> "`src/macros.h` force-defines `MILO_DEBUG` to keep `MILO_ASSERT` live."

**That is false.** `MILO_DEBUG` appears nowhere in `src/system/os/Debug.h`. The
entire `MILO_*` diagnostic family is gated on **`HX_NATIVE`**, which the matching
build does not define. Ground truth, by preprocessing a probe TU with the real
build flags (`cl.exe /P`, cflags carry **no `/D` at all**):

| written                          | expands to (match build)   |
|----------------------------------|----------------------------|
| `MILO_ASSERT(cond, line)`        | `((void)(cond))`           |
| `MILO_ASSERT_FMT(cond, ...)`     | `((void)sizeof(!(cond)))`  |
| `MILO_FAIL(...)`                 | `((void)(__VA_ARGS__))`    |
| `MILO_WARN(...)`                 | `MiloStripEval(__VA_ARGS__)` |
| `MILO_LOG(...)`                  | `((void)(__VA_ARGS__))`    |
| `#ifdef HX_NATIVE`               | **not taken** (0 occurrences) |
| `#ifdef MILO_DEBUG`              | **taken**                  |

⇒ **`MILO_DEBUG` buys the assert family nothing.** Turning it off would not cost
a single assert. Its only effect is to switch on rb3-Wii **dev-build** code.

⚠ **The tree contains both correct and incorrect statements of this**, so do not
trust a nearby comment. `src/system/ui/UI.h:25-26` states it *correctly* —
*"force-defines MILO_DEBUG tree-wide (src/macros.h), so MILO_DEBUG canNOT be part
of the strip condition — the gate is HX_NATIVE only"* — while several other
comments and commit messages assert the false version. Preprocess if in doubt:

```bash
# from a worktree root; prints what the MILO_* family actually expands to
wibo build/compilers/X360/16.00.10224.00/cl.exe <the cflags from build.ninja> \
     /P /Fi/home/free/tmp/probe.i src/probe.cpp
```

This also resolves an apparent contradiction in the tree: the comment in
`src/system/obj/Data.h` that says *"in the retail match build MILO_ASSERT is a
no-op, so the body reduces to `return mNodes[i]`"* is **correct**, and is not in
conflict with the force-define — the two statements are about two different
macros. (Note the one subtlety: `((void)(cond))` still *evaluates* the condition,
so side-effecting conditions survive and pure ones are DCE'd. That is deliberate
and A/B-verified; see the comment block at `Debug.h:159-171`.)

## The house fix pattern

Never delete a guard body — the native port needs it, and native correctness
matters independently of the match metric. Keep it alive under `HX_NATIVE`:

```c
// <why: what retail did, and the asm evidence that says so>
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
    ... the real dev-build behaviour (native port keeps this) ...
#else
    ... the retail / matching behaviour ...
#endif
```

Existing instances to copy from:

- `src/system/os/Timer.h:334-346` — `START_AUTO_TIMER_CALLBACK` (the original).
- `src/system/obj/ObjMacros.h:85-89` — `BEGIN_HANDLERS` message-timer arm.
- `src/system/utl/Loader.h:127-135` — `LOADMGR_EDITMODE` (lane CB-7, `a9cba70d`).

## ⚠ Do NOT blanket-remove — this is measured, not theorised

Lane CB-10 ran the control directly: comment out `#define MILO_DEBUG` in
`src/macros.h`, full build, full `report.json`, strict `(unit,name)` diff.

- The tree **compiles cleanly** with it off (rc=0, **0** errors) — so "it won't
  build without it" is not a reason either.
- Whole-binary result: **matched 41,267 → 41,246 (−21)**, honest floor
  39,759 → 39,738 (−21), `matched_code_percent` 35.506874 → 35.503925
  (**−0.002949 pp**). **GAINED 1, LOST 22.**
- Units affected by the *entire tree-wide* flip: **2 of 3,917**
  (`default/BandCharacter`, `default/SongData`). 0 key-set changes across
  69,366 shared keys.

So the vein is tiny and two-sided:

| site | verdict | effect |
|---|---|---|
| `src/system/beatmatch/SongData.cpp:165` (`DataVariable("log_midi_file_load")` probe) | **(a) retail EXCLUDES** | `SongData::Load` 86.671 → **100** |
| `src/system/bandobj/BandCharacter.cpp:2014` (`toggle_interests_overlay` HANDLE arm) | **retail has a *different* arm in that slot** — keep ours as a placeholder (see below) | removing it costs **−22** (19 funclets 100→99.9/99.8, two 100→93.9, one 100→92.5, `BandCharacter::Handle` 98.899→95.448) |
| the other 11 bare sites | **(a) correctness-only, or (c) inert** | 0 metric movement |

Final adjudication of the **13 bare-and-active** sites: **9 (a) retail EXCLUDES**
(1 paying, 8 correctness-only), **3 (c) inert**, **1 special** (BandCharacter,
above). **0 (d) unknown** — every site had target coverage. The composed union of
all fixes measured **+1 honest / +0.005296 pp, LOST 0**.

**The surgical patch beats the blanket one on both currencies.** Fixing the sites
individually, after adjudication, measured (lane CB-10, composed union, verified
as a strict set rather than by summing per-sub-lane claims):

```
matched_functions   41,267 -> 41,268   (+1)
HONEST FLOOR        39,759 -> 39,760   (+1)
matched_code       3,756,640 -> 3,757,200  (+560 B)
matched_code_percent 35.506874 -> 35.512170  (+0.005296 pp)
GAINED 1  LOST 0   |   MOVED of 69,366 shared keys: UP 1, DOWN 0
```

Blanket: **−21 / −0.002949 pp**. Surgical: **+1 / +0.005296 pp**. Same vein,
opposite sign — the difference is entirely per-site adjudication.

### ★★ `BandCharacter.cpp:2014` — the case worth reading twice

This site produced a two-instrument **conflict**, and the resolution was a third
explanation that neither instrument suggested. It is the best worked example in
the codebase of why you adjudicate on the conjunction rather than on whichever
instrument answered first.

- **Instrument 1 (metric):** removing the `toggle_interests_overlay` HANDLE arm
  costs **−22** — 19 `fn_8228C*` funclets 100→99.9/99.8, two 100→93.9, one
  100→92.5, and `BandCharacter::Handle` 98.899→95.448. Isolated to this one arm
  (`ObjMacros.h` untouched), so the loss is real and attributable.
  ⇒ suggests *retail includes it*.
- **Instrument 2 (binary absence):** a calibrated census of the
  `BEGIN_HANDLERS(BandCharacter)` block found **28 of 29 handler names present in
  retail, `toggle_interests_overlay` the only absent one** (0 in ascii/utf16le/
  utf16be, and 0 for every substring variant).
  ⇒ suggests *retail excludes it*.

**Both were correct.** Decoding the `.rdata` labels that retail's `Handle`
(`fn_8228B380`) actually references, in order, shows retail has an arm **in that
slot** — but it is **`hack_fix_clips_pre_merge`** (`0x820137FC`), not
`toggle_interests_overlay`:

```
restore_categories        0x82013818
game_over                 0x820118B8
hack_fix_clips_pre_merge  0x820137FC   <-- our toggle_interests_overlay slot
list_drum_venues          0x820137E8
portrait_begin            0x820137D8
portrait_end              0x820137C8
```

`0x820137FC` sits inside the contiguous descending-address BandCharacter string
pool, so the position is corroborated by pool ordering and not by the diff alone.
`hack_fix_clips_pre_merge` appears in **no oracle** — not rb3-Wii, not DC3, not
our tree. It is RB3-360-retail-exclusive.

⇒ **Leave the guard in place.** Our arm is a structurally-correct *placeholder*
for a retail arm whose body we do not yet have; deleting it removes a real slot
and costs 22 functions. The genuine fix is to port
`hack_fix_clips_pre_merge` — a body-port target, not a guard question.

**The transferable lesson:** "our name is absent from the binary" does **not**
imply "retail has no such handler". A dev build and a retail build can differ by
a *renamed or replaced* arm in the same slot, and a name-based absence test
cannot see that. When absence and codegen disagree, decode what the target
actually references before believing either.

## How to adjudicate a site

The **target asm is the only ground truth.** In particular:

- ⚠ **Oracle difference is NOT defect evidence.** rb3-Wii is a Wii *dev* build;
  it will *always* show the guard body. That tells you nothing about retail.
- ⚠ **Assert strings do survive in retail — but not Milo's.** `Object.h:1053-1058`
  says *"Retail keeps MILO_ASSERT strings (so the build is not a clean
  MILO_DEBUG-off build)"*. That **over-generalizes** (lane CB-10): of **499**
  `MILO_ASSERT` condition strings sampled from the Milo engine
  (`obj/ rndobj/ char/ utl/ os/`), exactly **1 of 499** appears in retail — and
  that one (`player_name`) is a bare identifier, i.e. a false positive. The
  ~105 assert-condition strings that *do* survive belong to the **UGC/RBN
  song-validation module and XDK/vendor middleware** (`nTrackNumber <=
  kUGCAlbumTrackNumberMax`, `size <= maxSize`, `iStr < max` — all confirmed
  present as positive controls). Milo's own assert *format* string
  (`"File: %s Line: %d Error: %s"`, `Debug.cpp:53`) is present once, consistent
  with the vendor path reusing the shared failer.
  ⇒ Retail is **per-TU heterogeneous**: Milo engine TUs are asserts-off; some
  game/vendor modules were built with asserts on. So "retail is a release build,
  therefore debug code is absent" is still not a valid inference — **check the
  specific string**, and pick your positive controls from the *same module*.
- Prefer the **conjunction of two instruments** (e.g. target asm *and*
  string-presence in the retail binary). Absence of a string in the binary is
  map-independent proof and makes a good second instrument.
- For anything touching a class member or a vtable, ask the **compiler**:
  `scripts/harvest/class_layout_report.py <Class>`. The `// 0xHEX` header
  comments — and `struct_db.sqlite` / `lookup_struct_offset`, which are *derived
  from* those comments — are measurably wrong in places.

Classify each site as:

- **(a) retail EXCLUDES** — guard body is compiled in and must not be. Fix it.
- **(b) retail INCLUDES** — the guard is correct here. Leave it alone.
- **(c) inert** — no codegen either way.
- **(d) unknown** — no target coverage to adjudicate against. Report as unknown;
  do **not** guess.

Note (a) and "metric-inert" are **not** mutually exclusive: a guard body can be
genuinely wrong for retail while the affected function is unpaired or unmatched,
so nothing moves. That is a legitimate **correctness-only** fix — worth making,
because the native port is the project's real goal — but it must be labelled as
such and never presented as a match win.

## ★★ The guard is an unreliable marker — the real defect class is larger

The most useful thing lane CB-10 learned is that **`#ifdef MILO_DEBUG` catches
only a fraction of the dev-only code that leaked in from the rb3-Wii dev decomp.**
Plenty of it arrived with **no guard at all**, so a census of the macro
systematically under-counts the problem.

Measured instance — the `CharDeferHighlight` overlay body. Four TUs contain the
identical dev-only `Highlight()` shape, and **only 1 of 4 is `MILO_DEBUG`-guarded**:

| file | guarded? |
|---|---|
| `src/system/char/CharDriver.cpp:101` | ✅ `#ifdef MILO_DEBUG` |
| `src/system/char/CharIKMidi.cpp:172` | ❌ unguarded |
| `src/system/char/CharLipSyncDriver.cpp:204` | ❌ unguarded |
| `src/system/hamobj/HamDriver.cpp:128` | ❌ unguarded |

All four are dev-only, proven by one map-independent instrument: the RTTI type
name **`.?AVCharDebug@@` is absent from retail (0 hits)** while `.?AVCharDriver@@`
and `.?AVCharEyes@@` each hit once. `CharDebug` is polymorphic and `/GR` is on, so
the probe is valid and its controls fire — **class `CharDebug` does not exist in
retail RB3**, therefore `CharDeferHighlight()` cannot either.

Two further unguarded instances found the same way:
`AnySignMercurySwitchFilter::Poll` (`beatmatch/MercurySwitchFilter.h:79` — the
guarded `LowPassMercurySwitchFilter::Poll` sits 40 lines above it), and a
`TheDebug << MakeString("transition from %s to %s\n", …)` at `ui/UI.cpp:451`
that is absent from retail asm **and from the rb3-Wii oracle entirely** — a
DC3-only line that leaked in via the engine-provenance path.

⇒ **Do not treat "no `#ifdef MILO_DEBUG`" as evidence that a body is retail's.**
Search for the *callee* (`CharDeferHighlight`, `gGuitarOverlay`, `TheDebug`,
`RndOverlay`), not for the guard. A **binary-absence check on a distinctive
callee, class RTTI name, or string literal — always with positive controls drawn
from the same TU and the same idiom** — is the instrument that actually finds
these, and it works whether or not a guard is present.

## ⚠ The secondary hazard: TU-local `#undef MILO_DEBUG`

Four TUs work around the force-define by `#undef`-ing it at the top of the `.cpp`:

- `src/band3/bandtrack/VocalTrack.cpp:2`
- `src/band3/game/GamePanel.cpp:2`
- `src/system/meta/CreditsPanel.cpp:8`
- `src/system/meta/StreamPlayer.cpp:10`

**This is an ODR violation whenever the guarded thing lives in a header.**
Confirmed instance: `src/system/meta/CreditsPanel.h:50` declares
`bool mCheatOn;` under a bare `#ifdef MILO_DEBUG`. `CreditsPanel.cpp` `#undef`s
the macro, so *in that TU the member does not exist* — but
`src/band3/meta_band/MetaPanel.cpp` includes the same header with `MILO_DEBUG`
still defined, so *there it does*. Two different `sizeof(CreditsPanel)` and two
different member offsets for one class in one binary; the linker silently picks
one.

**Prefer the `HX_NATIVE` gate in the header over a TU-local `#undef`** — it makes
the layout uniform tree-wide and removes the hazard at its root, rather than
papering over it in one TU. When you remove an `#undef`, remember it also
un-suppresses every *other* `#ifdef MILO_DEBUG` in that `.cpp`, so convert those
in the same edit and A/B the result.

## Related: other force-defines in the tree

`MILO_DEBUG` is the **only tree-wide** force-define with this wrong polarity.
A full scan of first-party `src/` for macros that are both `#ifdef`-tested and
bare-`#define`d (excluding include guards and vendored trees) returns **5**:

| macro | site | assessment |
|---|---|---|
| `MILO_DEBUG` | `src/macros.h:3` | ⚠ tree-wide, wrong polarity — this document |
| `RB3_GAME_SCATTER_COPY` | `src/band3/meta_band/MusicLibrary.cpp:2301` | benign — `#define`/`#undef` bracketed around a scatter-include |
| `SW_SCATTER_OWNER_INCLUDE` | `BandCharDesc.cpp:1130`, `PatchDir.cpp:847` | benign — same bracketed scatter-include idiom |
| `STL_NODE_ALLOC_DEBUG` | `src/band3/meta_band/BandProfile.cpp:1` | deliberate but fragile — see below |

`STL_NODE_ALLOC_DEBUG` is TU-local (no `#undef`) and gates a `typeid(pointer)`
inside the inline template `MemAllocator<T>::allocate` in
`src/network/Platform/qMemAllocator.h:70`. Because that template is COMDAT-folded
across TUs, `BandProfile.cpp` instantiates it *with* the `typeid` while every
other TU instantiates it *without* — an ODR violation on the template that
happens to select the retail-correct body. It is evidence-backed and intentional,
but it works by exploiting "ODR violation picks one arbitrarily", i.e. the same
fragility as the `#undef` pattern above. **Do not replicate this idiom**; if you
touch it, replace it with an explicit gate rather than relying on fold order.

The correct-polarity patterns to imitate instead — both already in the tree:

- **Default-off feature macro**: `MILO_MESSAGE_TIMERS` (`obj/Object.h:1056`),
  `WORLDDIR_DC3_TAIL`, `RB3_DC3_MAT`, `RB3_WORLDCROWD_DC3_REV`,
  `RB3_HAS_HUE_CONVERGE` — *never defined*, so "off" is the retail shape and the
  dev/DC3 shape is opt-in.
- **`HX_NATIVE`-gated**: `OBJREF_VIRTUAL` (`obj/Object.h:87-90`) — native gets
  the polymorphic form, the match build gets retail's layout.
- **`HX_NATIVE`-derived feature macro**: `RB3_UI_DEBUG_MEMBERS`
  (`ui/UI.h:27-29`) — defined *only* under `HX_NATIVE`, used to strip
  `UIManager`'s debug-only members (`mOverlay`, `mLoadTimer`, `mAutomator`,
  `mShowDevMenu`) from the match-build layout. Compiler-verified: `UIManager`
  is `sizeof` 172 (0xac) with vtordisp@0x80 and the `Object` vbase vfptr@0x84,
  agreeing with retail's ctor at `0x827DF040`. This is the right way to handle a
  debug-only **member**; note the stripped statements that still referenced those
  members are kept compiling by same-named file-scope statics at `UI.cpp:157-160`.

## Re-running the census

```bash
# every conditional site (24 at time of writing; 5 already HX-gated,
# 6 inside a TU-local #undef, 13 bare-and-active)
grep -rnE '^\s*#\s*(ifdef|if)\s.*MILO_DEBUG' src/ --include='*.h' --include='*.cpp'

# the TU-local workarounds
grep -rn 'undef MILO_DEBUG' src/ --include='*.h' --include='*.cpp'
```

⚠ The Bash tool runs **zsh**: quote the `--include` globs or the command dies and
then prints a misleading zero.

## See also

- `src/system/os/Debug.h:159-300` — the authoritative, heavily-A/B'd comment
  block on what each `MILO_*` macro strips and why the family is deliberately
  *asymmetric* (`MILO_WARN` uses `MiloStripEval`; `MILO_NOTIFY`/`MILO_LOG` keep
  the comma form, because MSVC evaluates function args right-to-left but the
  comma operator left-to-right). **"Finishing the job" by making them uniform is
  measured negative — do not.**
- `src/system/obj/ObjMacros.h:29` — `OBJ_SET_TYPE`'s `#ifdef MILO_DEBUG` is
  **mis-named rather than load-bearing**: the two arms are codegen-equivalent in
  the current match build (the tree-wide flip moved no `SetType` body). Treat it
  as a naming cleanup, not a behaviour change — this family is heavily tuned and
  its left-to-right argument ordering is load-bearing.
