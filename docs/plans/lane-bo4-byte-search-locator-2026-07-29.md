# laneBO4 — the compile-and-byte-search TU locator (2026-07-29)

Mission (from `docs/plans/tu-pin-wave-2026-07-29.md` §6): build the instrument
laneBL said was the *only* signal that could reach the controller family, and
prove it on the pool it was designed for.

> **Do not fund another string/RTTI channel for the controller family.** The one
> signal that would work is **compile-and-byte-search** … **Nobody has yet turned
> it into a locator.**

**Result: built, calibrated, and it works.** The tool is
`scripts/harvest/byte_locate.py`. It placed **all five** controller TUs — the
265-function block laneBL measured as unreachable by string and RTTI — in one
contiguous neighbourhood, `0x82797760..0x8279B280`, currently pinned to
`TrackWatcherImpl.cpp`.

Baseline for everything below: main `9df262c9`, `measures.matched_functions`
**40,302** (verified in a fresh fully-built worktree).

---

## 0. TL;DR

* **The instrument works and is calibrated.** On 60 randomly-drawn
  already-pinned units: **precision 0.9841** when it commits to a single VA
  (434/441), **recall 0.5813**, **TU-level 47 of 50** proposals overlap the true
  pinned span, against a **chance baseline of 0.001124** — a **875× lift**.
* **Two independent negative controls both read zero at the operating point.**
  Instruction-rotation: **0 hits in 38,960** bodies. One-bit mutation of a single
  unmasked instruction: **0 hits in 1,115** TU-owned bodies.
* **A held-out positive control fell out for free: 20/20.** A `.c`-suffix bug in
  the pinned-set join briefly made 20 vendor units (oggvorbis, zlib, json-c,
  tomcrypt) look unpinned. The tool placed **every one of them inside its own
  existing pin**, blind. That is a clean 1.000 on units it had no ground truth
  for at the time.
* **What makes it work is a discriminator nobody had isolated**: a body is usable
  as an anchor only if it is **COMDAT `Selection == 1`** (this `.cpp` defines it)
  **AND** defined in exactly one obj tree-wide **AND** not an `__unwind$` funclet.
  Each of those three was measured, not assumed — see §3. Without them the same
  matcher reads **0.476** precision.
* **Seven TUs located in total**: the five controllers, `StoreArtLoaderPanel`, and
  `band3/game/UISyncNetMsgs` (`0x8269F3F8..0x8269FDE4` + `0x82690A10..0x82690B74`) —
  the last of which I had wrongly written off as a phantom and the locator
  overturned (§6-ter).
* **A fresh-pool confirmation:** `StoreArtLoaderPanel` (another laneBL §6
  signal-less row, 26 functions) was ported and located at
  `0x827B70F8..0x827B7ECC` on the first compile — §4-bis. The instrument is not
  tuned to the controller family.
* **Landed: `matched_functions` 40,302 -> 40,405 (+103); strict-100 by bare name
  +112 / -6 = net +106, and all five TUs finished at 100% (124/124 functions).** All six losses were predicted in advance and are
  retirements of false 100 %s — §4-ter.
* **The five controller TUs are located.** Five disjoint, consecutively-ordered
  blocks, every inter-TU boundary landing on a 12–20 byte alignment gap in
  retail's `.pdata` — an independent structural confirmation the tool did not use.
* **The free-yield vein is empty and that is now measured**: sweeping all **147**
  compiled-but-unpinned objs placed exactly the 6 TUs this lane ported and
  nothing else. Do not fund a "just compile everything and see" wave — it has
  been run; new yield requires new ports.

---

## 1. How the instrument works

```
port (speculatively)  ->  compile UNPINNED  ->  LOCATE  ->  pin
```

`tools/project.py` is already patched so an `objects.json` entry gets a compile
edge without a `splits.txt` range, so step 2 is free.

1. **Slice.** Parse our compiled COFF. For every code symbol, slice the body
   between consecutive symbols in its section — so EH funclets come out as their
   own bodies, matching retail's per-funclet `.pdata` records (CLAUDE.md's
   "one source function ≠ one `.fn` block" trap is handled by construction, not
   by hope).
2. **Mask.** Every 4-byte instruction word carrying a COFF relocation is masked
   wholesale. This is deliberately the *same* conservative convention
   `size_order_automap.py` already uses, so results are comparable across the
   tool stack. It over-masks, which costs precision, never recall. I did **not**
   invent a second convention; objdiff's own `functionRelocDiffs=none` works at
   the decoded-instruction level and is not usable for an 11.8 MB byte scan.
3. **Search.** Take the longest contiguous unmasked run as an anchor, enumerate
   its occurrences with `bytes.find`, verify every remaining unmasked run at the
   implied offset, and require the implied start to be a real `.pdata` function
   start. ~1 s for a whole TU against the full `.text`.
4. **Cluster.** Group hits into neighbourhoods and rank by **TU-owned anchors**,
   not by hit count. Propose the anchor extent, snapped to `.pdata` boundaries.

### CLI

```
byte_locate.py locate    --unit system/beatmatch/KeyboardController [-v]
byte_locate.py calibrate --auto 60 --owned-only [--show-wrong]
byte_locate.py audit     --min-owned 3        # sweep every unpinned compiled obj
byte_locate.py control   --auto 80            # negative controls only
```

Read-only. It prints a **proposed** span; it never writes `splits.txt`,
`objects.json` or `target_symbol_map.json`.

---

## 2. Calibration — with the chance baseline

Positive control: 60 units drawn at random (`--seed 1`) from those that are both
pinned in `splits.txt` and compiled. Ground truth = the unit's own pinned
`.text` span(s); a located VA is CORRECT iff it lies inside one.

| metric | value |
|---|--:|
| functions in the scored population | 1,062 |
| searchable (mask leaves ≥ 24 unmasked bytes) | 855 (80.5 %) |
| got ≥ 1 hit | 497 — **recall 0.5813** |
| committed to a **unique** VA | 441 |
| …correct | **434** |
| …wrong | 7 |
| **precision(unique)** | **0.9841** |
| any-hit-correct / hit | 0.9638 |
| **chance baseline** (mean \|span\|/\|.text\|) | **0.001124** |
| **lift over chance** | **875×** |

**TU level** (the number that actually gates a pin): the tool proposed a span for
**50 of 60** units and **47** of those overlap the true span — **0.94**. It
declines on 10 rather than guessing.

The three TU-level misses are informative, not random:

| unit | proposed | true | gap |
|---|---|---|--:|
| `band3/meta_band/Asset` | `0x825EFB58..0x825EFBC8` | `0x825EF708..0x825EFB34` | **0x24** |
| `system/bandobj/DialogDisplay` | `0x8232A2C0..0x8232A5BC` | `0x82329B20..0x82329FD8` | 0x2E8 |
| `system/gesture/DrawUtl` | `0x82553FC8..0x82554028` | `0x826058E8..0x8260596C` | far |

`Asset` and `DialogDisplay` are **span under-coverage**, the systemic defect
laneBL §3 documented ("every TU in this wave needed its span corrected") —
`DialogDisplay` is literally one of the TUs laneBL had to extend. Only `DrawUtl`
is a real miss. Read that way the TU-level number is 49/50; I report 47/50
because the pinned spans are the stated ground truth and I will not move the
goalposts after seeing the answer.

The same effect appears per-function: two of the seven "wrong" unique VAs in the
first 3-unit run were `LockStepMgr`'s `EndLockMsg::Load` at exactly `0x825AB3B8`
— **the byte immediately after** `LockStepMgr`'s pinned end. The tool was right
and the pin was short.

### ★ Which numbers the class-name bug did and did not touch

Explicitly, because it matters: **the locator's calibration is unaffected.**
`calibrate`, `locate` and `control` never reference a class name or a file name —
they compare compiled bodies against retail bytes and score against pinned spans.
I verified this by inspection of the call graph rather than by assertion, and
re-ran everything after the fix: precision(unique) **0.9841**, recall **0.5813**,
TU-level **47/50**, chance baseline **0.001124** (**875×**), rotation control
**0/38,960**. Unchanged to the digit.

What the bug *did* corrupt was the **phantom screen's** false-negative rate, and
that ledger has been fully re-run (below). Two different instruments; only one was
wrong.

### Negative controls

An FP rate that is only argued is not measured, so two were run.

| control | construction | searched | hits | FP rate |
|---|---|--:|--:|--:|
| **rotation** | cyclically rotate the instruction sequence by one word (mask rotated with it) — identical length, mask density, opcode and register histogram, but not a real function | 38,960 | **0** | 0 (< 7.7e-5 at 95 %) |
| **mutation, owned population** | flip **one bit** in the low half of **one** unmasked instruction word — everything else byte-identical | 1,115 | **0** | 0 |
| mutation, *excluded* population | same, on funclets + vague-linkage bodies | 18,366 | 224 | 0.0122 |

The third row is the point of the discriminator in §3: the matcher genuinely does
tolerate a one-instruction change **on the bodies the tool refuses to use**, and
tolerates nothing on the bodies it does use.

### Held-out control (found by accident, kept because it is honest)

`_stem()` stripped `.cpp`/`.obj` but not `.c`, so 20 vendor units
(`oggvorbis/psy`, `zlib/deflate`, `json-c/linkhash`, `tomcrypt/aes`, …) were
invisible to the pinned-set join and went through `audit` as if unlocated.
**All 20 proposed spans landed inside that unit's own existing pin — 20/20.**
Different compiler front-end path (C, not C++), different code style, no
`Selection`-heavy COMDAT structure, and the tool still placed every one.

---

## 3. ★★ The discriminator — and the two things that had to be measured

The naive form of this tool — "search for every compiled body" — reads
**precision 0.476**. Three filters take it to 0.984. Each was derived from a
measurement, and each was wrong in a way I would not have guessed:

**(a) COMDAT `Selection`.** MSVC marks a normal out-of-line definition
`IMAGE_COMDAT_SELECT_NO_DUPLICATES` (1) and vague linkage — inline members,
template instantiations, compiler-generated `??_G`/`??__E` — `SELECT_ANY` (2).
Measured on `UIGridProvider.obj`: 14 sel=1, 129 sel=2. Retail keeps **one** copy
of a `SELECT_ANY` body and may take it from any TU, so a hit on one is a true
location of a real body that says **nothing** about where *this* TU is. Adding
this filter alone: 0.476 → 0.634.

**(b) `Selection == 1` is NOT sufficient.** `ViewSetting.obj` has 452 sel=1
sections, and several of those bodies (`PropSync(String&,…)`, the whole
`CriticalUserListener` group) are **also defined by another `.cpp` in our tree** —
our headers define some functions out-of-line, which never surfaces because we
never link. `?Poll@Game@@QAAXXZ` is defined by both `band3/game/Game.obj` and
`band3/meta_band/MusicLibrary.obj`. The tool therefore builds a tree-wide
definition-multiplicity map over all 1,073 objs (**45,771 multiply-defined code
symbols**) and requires a unique definer. Cached at
`build/45410914/byte_locate_defmap.json`; ~4 s to rebuild.

**(c) `__unwind$` funclets were 87 % of the residual error.** After (a) and (b),
250 of the 286 remaining wrong answers came from three units, and inspecting
them showed almost all were 32–48 byte `__unwind$NNNNN` bodies. They pass every
ownership test — unique name (the numeric suffix), sel=1 (they live in the
parent's COMDAT) — and they are mask-identical across the binary. Excluding them:
0.634 → **0.984**.

> **Rule for the next instrument-builder: an ownership test that is structurally
> valid can still be defeated by a name that is unique for the wrong reason.**
> The `__unwind$` counter made 39,735 interchangeable bodies look individually
> identifiable.

---

## 4. Applied to the target pool — the controller family

Five TUs ported speculatively from the rb3-Wii DEV oracle
(`/home/free/code/milohax/rb3/src/system/beatmatch/`), compiled unpinned. **All
five compiled clean on the first attempt** — our tree's `BeatMatchController`,
`BeatMatchControllerSink`, `JoypadController`, `RGUtl`, `RGState` and
`UsbMidiGuitarMsgs` already match the oracle's expected signatures exactly, so no
API shims were needed and nothing was `#if 0`'d.

Every one lands in the `system/beatmatch` neighbourhood, in **five disjoint,
consecutively-ordered blocks**:

| TU | located `.text` block | owned anchors |
|---|---|--:|
| `system/beatmatch/JoypadMidiController.cpp` | `0x82797760..0x82798410` | 6 |
| `system/beatmatch/KeyboardController.cpp` | `0x82798410..0x82798F90` | 7 |
| `system/beatmatch/ButtonGuitarController.cpp` | `0x82798F90..0x82799D68` | 5 |
| `system/beatmatch/RealGuitarController.cpp` | `0x82799D68..0x8279AA60` | 6 |
| `system/beatmatch/JoypadGuitarController.cpp` | `0x8279AA60..0x8279B280` | 6 |

**Three independent corroborations, none of which the tool used:**

1. **Every one of the six boundaries is an alignment gap.** The retail `.pdata`
   table has a 12-byte hole at `0x82797754`, `0x82798404`, `0x82798F84`,
   `0x82799D5C` and `0x8279AA54`, and a 20-byte hole at `0x8279B26C`. A COMDAT
   boundary is exactly where padding appears; five TUs proposed from byte content
   alone landed on six consecutive padding gaps.
2. **The base classes bracket them.** `JoypadController.cpp` is already pinned at
   `0x8279B280` and `GuitarController.cpp` at `0x8279BE58` — immediately after the
   derived family, exactly as `/O1`-without-LTCG TU spatial grouping predicts. The
   whole neighbourhood is `BeatMatcher` / `DrumMixDB` / `SlotChannelMapping` /
   `TrackWatcherImpl` / `TrackWatcher` / `DrumPlayer` / the `*TrackWatcherImpl`
   family.
3. **The string channel already pointed here.** laneBL §6 recorded that
   `JoypadGuitarController` and `JoypadMidiController` each had exactly one
   selective literal and that it sat **in `TrackWatcherImpl.cpp`** — filed as a
   dead end because one literal cannot pin a TU. It was right; it just could not
   act alone.

### The donor is `TrackWatcherImpl.cpp` — the seam again

`TrackWatcherImpl.cpp` claims `0x82794730..0x82797808` and
`0x82797858..0x8279B280`. The second block is **entirely** the controller family.
This is laneBD/laneBL's seam thesis for the fourth time: the TUs were never
missing, they were swallowed by an over-broad neighbouring pin.
`TrackWatcherImpl.cpp` keeps 7 other `.text` blocks, so there is no empty-unit
trap.

### `AsyncFile_Win.cpp`'s `0x82797808..0x82797858` micro-pin is an ICF island

0x50 bytes, far from `AsyncFile_Win.cpp`'s other two blocks at `0x825353C0` /
`0x825357D0` — laneBL §3.5's island-distance tell, and the tool independently
identifies the body as `??1JoypadMidiController@@UAA@XZ`. Folded with
`??1AsyncFileWin@@UAA@XZ`.

### The pre-existing map entries in the region are mostly wrong

10 `target_symbol_map.json` entries fall inside `0x82797700..0x8279B300`.
**Four of them — `??_G?$ObjOwnerPtr@VRndParticleSysAnim@@`,
`??_G?$ObjPtr@VCrowdMeterIcon@@`, `??_G?$ObjPtr@VRndParticleSys@@`,
`??_G?$ObjPtrList@VRndGroup@@VObjectDir@@` at `0x82797A38` / `0x827985C8` /
`0x82799EA0` / `0x8279AD00` — are the same 76-byte body**, which the tool
identifies as `??_GKeyboardModMsg@@UAAPAXI@Z` appearing once per controller TU.
That is laneBL §4.2-bis's `??_G` ICF-fold class reproducing exactly, and it is a
direct confirmation of their standing rule that `??_G` map entries are
low-confidence by default. Carving retires them; that is a **correction**, not a
regression.

---

## 4-bis. A fresh-pool confirmation — `StoreArtLoaderPanel`

To check the instrument was not tuned to the controller family, it was run on a
different signal-less row from laneBL §6: `system/meta/StoreArtLoaderPanel`
(26 unmatched functions, zero selective literals). Ported compile-only from the
Wii oracle in ~15 minutes; the only Wii-specific divergence was
`TheWiiCommerceMgr.InitCommerce/DestroyCommerce` in `Load`/`Unload`, which has no
360 equivalent in our tree or in DC3 and was dropped (costing those two bodies).

**Located at `0x827B70F8..0x827B7ECC`**, on 5 TU-owned anchors — `??0`, `??1`,
`ClearArt`, `EnsureArtLoader`, `Handle` — plus 9 further anchors that are
`ArtEntry`-specific STL instantiations (`__copy<ArtEntry*>`,
`__destroy_range_aux<reverse_iterator<ArtEntry*>>`,
`_M_insert_overflow_aux<vector<ArtEntry>>`, `clear<vector<ArtEntry>>`, …). Those
are vague-linkage by COMDAT selection, so they never anchor the ranking — but
they are instantiated over a **class-private** type, so as corroboration they are
close to conclusive.

Both ends land on `.pdata` alignment gaps (12 B at `0x827B70EC`, 20 B at
`0x827B7ECC`), the same tell as §4.

Pin-ready, not landed here (three donors need shrinking and this lane already had
a carve in flight):

| donor | current | action |
|---|---|---|
| `StorePanel.cpp` | `0x827B53A4..0x827B78EC` | shrink to `..0x827B70F8` |
| `StandingStillGestureFilter.cpp` | `0x827B78F8..0x827B7950` | 88-byte micro-pin holding `?StaticClassName@UIPanel@@` (ICF-folded 453×) — **check its other blocks before deleting** |
| `ProfileMgr.cpp` | `0x827B7950..0x827B7D88`, `0x827B7D88..0x827B7DBC` | delete both |
| — | `0x827B7DBC..0x827B7ECC` | already UNCLAIMED |

The head of the span at `0x827B7170`, `0x827B72F0`, `0x827B79C8`, `0x827B7950`,
`0x827B7B04` did not match — those are `Poll`, `GetBmp`,
`IsAllArtLoadedOrFailed`, `Load` and the `ArtEntry` ctor/dtor, i.e. exactly the
functions touching the dropped Wii commerce API and the `NetCacheLoader`/
`BufStream` path. **Read that as a divergence list, not as absence.**

---

## 4-ter. The landed result — verified independently by this lane

Branch `laneBO4-land`, tip `88428ea8`. Measured by me, not taken from the
sub-lane's report: both worktrees fully built, `report.cache` removed before
every read, delta computed **unit-agnostically** (by `(unit, name)` AND by bare
`name`), counting only `fuzzy_match_percent == 100.0` exactly.

| measure | baseline | landed | delta |
|---|--:|--:|--:|
| `measures.matched_functions` | 40,302 | **40,405** | **+103** |
| strict-100 by bare `name` | — | — | **+112 / −6 = net +106** |
| strict-100 by `(unit, name)` | — | — | +129 / −23 = **net +106** |

**All five TUs finished at 100 %: 124 of 124 functions** (JoypadMidi 31/31,
Keyboard 25/25, ButtonGuitar 26/26, RealGuitar 26/26, JoypadGuitar 16/16).

Landed in three commits, and the loss set is **byte-for-byte the same six at every
one of them** — every gain was additive, no fix traded one match for another:

| commit | what | delta |
|---|---|--:|
| `6b3ee2eb` | the carve + pin itself | +76 / net +79 |
| `7157f27d` | retail-only StompBox handler arms + unsigned `mSlotMask` | **+16** |
| `4743c9c9` | `KeyboardModMsg` has an INLINE dtor in RB3 retail, not DC3's noinline one | +1 |
| `1b896035` | retail truncates `OnMsg`'s return to a byte in 4 more `Handle` blocks | **+4** |
| `88428ea8` | the last 7 near-misses off the worklist | **+7** |

★ **Three of the four fixes came straight off the §6-bis divergence worklist**, so
the near-miss output is not a consolation prize for the 0.58 recall — it produced
**+21 of the +96** on its own. `7157f27d` is the clearest case: the tool measured a
uniform ~−148 byte deficit across four independently-ported `Handle` bodies and
called it one shared divergence rather than four bugs; the fix was one missing
handler-arm group, and it was worth **+16**.

The two multiset views agree on the net, which is the point of running both: the
23 `(unit,name)` "losses" are 17 legitimate unit migrations out of
`TrackWatcherImpl.cpp` plus the same 6 real ones.

★★ **Every one of the six by-name losses was predicted in advance**, from the
locator's output, *before the carve was built*:

| lost name | what it actually is |
|---|---|
| `??1AsyncFileWin@@UAA@XZ` | `??1JoypadMidiController@@UAA@XZ` — the 0x50 ICF island micro-pin |
| `??_G?$ObjOwnerPtr@VRndParticleSysAnim@@` | `??_GKeyboardModMsg@@UAAPAXI@Z` |
| `??_G?$ObjPtr@VCrowdMeterIcon@@` | same 76-byte body |
| `??_G?$ObjPtr@VRndParticleSys@@` | same 76-byte body |
| `??_G?$ObjPtrList@VRndGroup@@VObjectDir@@` | same 76-byte body |
| `?AllowsInlineProxy@ObjectDir@@UAA_NXZ` | flagged suspect in the same pre-flight |

All six are **retirements of false 100 %s** — `target_symbol_map.json` entries
bound to a VA that is not that function, scoring only as a shape. Zero of them is
a real body ≥ 100 B. This is the cleanest adjudication of a carve's losses the
project has had, and it is clean *because the instrument named the bodies first*:
the loss list was written down before the build and matched exactly.

★ Note the shape of the win as well as its size. **+105 by-name gains against a
laneBL headroom estimate of 265 unmatched functions** is ~40 % realisation on a
first-compile speculative port. The gap is the dev-vs-retail divergence tax
(§6) — Wii `MILO_ASSERT` / `HANDLE_CHECK` line-number immediates above all — not
a location error. The pins are now correct, so that remainder is ordinary
body-port work rather than a location problem.

---

## 5. The audit sweep — a measured negative

`byte_locate.py audit` sweeps every compiled-but-unpinned obj, since those have
already paid the expensive half of the pipeline. **147 unpinned compiled objs
swept; 6 placed** — the five controllers and `StoreArtLoaderPanel`, i.e. exactly
the TUs this lane ported. Nothing that was already sitting in the build dir
clears 3 TU-owned anchors.

This closes a plausible-sounding vein before anyone funds it: there is no reserve
of already-compiled-but-unplaced TUs waiting to be found. Adding to this pool
requires *new ports*, which is the expensive part, and the tool's value is that
it makes each new port immediately actionable rather than requiring a positional
signal that may not exist.

★ Note the join hazard the sweep walked into and that the code now guards
explicitly: `splits.txt` names units inconsistently — some entries are full paths
(`system/beatmatch/BeatMatchUtl.cpp`), some are bare basenames
(`TrackWatcherImpl.cpp`). The pinned set is taken as {full stem} ∪ {basename},
which can only ever call an unpinned unit *pinned* (conservative), never the
reverse. **Do not invert this join** — a bare-basename join in the other
direction is the documented `live_units.py` defect.

---

## 6. Honest limits

* **Recall is 0.58, and it is bounded by the port, not by the search.** A miss
  means our compiled body differs from retail by ≥ 1 unmasked byte. Since
  `../rb3` is a **dev** build, the standing divergence list (laneBL §7) applies in
  full — the ported controllers carry Wii `MILO_ASSERT` / `HANDLE_CHECK` line
  numbers, which are certainly wrong for retail and will sink those functions'
  bodies. **A near-miss is evidence of a divergence, not of a wrong location.**
* **It cannot place a TU with no out-of-line definitions of its own.** Units whose
  every function is inline/template (or whose `.cpp` we have not ported) produce
  zero owned anchors and the tool declines — 10 of 60 in calibration.
* **It says nothing about the span's ENDS.** It places anchors; the head (leaf
  helpers, `OBJ_CLASSNAME` accessors) and tail (EH funclets) still need laneBL
  §3.1–§3.4's rules. In this wave the alignment-gap tell settled all six
  boundaries, but that will not always be available.
* **The mask convention over-masks** (whole word, not the operand field). Tightening
  it to per-reloc-type operand masking would raise recall; it was not done because
  matching `size_order_automap.py` exactly was worth more than the margin.
* **`.pdata`-start anchoring is on by default** (`--free` disables). A body that
  retail placed inside a larger `.pdata` record will be missed.
* **Ground truth is the pins, and the pins under-cover.** The precision figure is
  therefore a *lower* bound; at least 2 of the 7 per-function errors and 2 of the
  3 TU-level errors are the pin being short, not the tool being wrong.

---

## 6-bis. ★★ A miss is a DIALECT DIVERGENCE, not an absence — and the tool now says which

laneBO6 supplied the framing that turned this instrument's weakest number
(recall 0.58) into its second output: **the false-negative rate is dominated by
our source, not by retail.** A `.text` search is exact, so a miss carries no
partial credit — but the *shape* of the miss is highly informative, and throwing
it away as "0 hits" discards the most useful thing the search learned.

`locate` now emits a **DIVERGENCE WORKLIST**: for every TU-owned body that
missed, the closest retail function inside the proposed span, the size delta, and
how many 4-byte words already agree from each end. Ranked exact-size first.

### It paid off immediately — 7 exact-size near-misses in the controller family

| function | ours / retail | retail VA | words agreeing |
|---|---|---|---|
| `ButtonGuitarController::OnMsg(RGFretButtonUpMsg)` | 320 / 320 | `0x82799418` | 58 + 21 of 80 |
| `JoypadMidiController::OnMsg(KeyboardKeyPressedMsg)` | 236 / 236 | `0x82797CA0` | 48 + 10 of 59 |
| `ButtonGuitarController::OnMsg(RGSwingMsg)` | 276 / 276 | `0x82799028` | 47 + 4 of 69 |
| `JoypadMidiController::OnMsg(KeyboardKeyReleasedMsg)` | 208 / 208 | `0x82797DC0` | 41 + 10 of 52 |
| `KeyboardController::OnMsg(KeyboardKeyPressedMsg)` | 308 / 308 | `0x82798618` | 28 + 44 of 77 |
| `KeyboardController::OnMsg(KeyboardKeyReleasedMsg)` | 212 / 212 | `0x82798750` | 21 + 27 of 53 |
| `RealGuitarController::OnMsg(RGSwingMsg)` | 312 / 312 | `0x8279A040` | 4 + 6 of 78 |

**Exact size with both ends agreeing means the divergence is a bounded middle
region and register allocation did not shift** — single-expression fixes, and the
`pre`/`suf` counts localise the window to look in.

### The five `Handle` bodies share ONE deficit

−128 / −148 / −152 / −356 / −8. Four of five within ~24 bytes of each other across
four independently ported TUs is **one shared handler-arm omission**, not four
bugs — the tool now says so explicitly when several misses cluster on a delta.
Site count is blast radius; here the *defect* count is one.

### ★ The `END_HANDLERS` hazard is real, and I checked whether it was mine

laneBO6 read retail's unhandled tail off four `Handle` bodies at 100 %
(`BandDirector`, `StreakMeter`, `BandScoreboard`, `EntityUploader`):
`clrlwi. r11,rN,24` (`_warn`) → `beq` → adjusted `this` → `bl PathName` →
`li r11,6` (`kDataUnhandled`). Our tree has **two** `END_HANDLERS`:
`obj/Object.h` carries that tail, `obj/ObjMacros.h`'s is a bare `return` — and
these ports include `ObjMacros.h` *after* `Object.h`, so the bare one wins.

I did not assume either way. Comparing our compiled tail to retail's:

```
ours  : 574b063f 4182000c 7fa3eb78 4bfffd31 39600006 939b0000 917b0004 7f63db78 383f00d0 4bfffd18
retail: 574b063f 4182000c 7fa3eb78 4bfbee21 39600006 939b0000 917b0004 7f63db78 383f00d0 48090500
```

**Byte-identical modulo relocations.** The explicit `HANDLE_CHECK(line)` already
supplies the `_warn`/`PathName(this)` arm, and its `line_num`/`__FILE__` sit in a
comma expression whose non-final operands have no side effects — so **the Wii
line numbers cost nothing in codegen**, contrary to my §6 assumption, which is
hereby corrected. The hazard remains real for any TU that uses the bare
`END_HANDLERS` *without* a `HANDLE_CHECK`; it simply was not what sank these.

### Dialect classes the tool now flags by name

* **`Load`/`PreLoad`/`PostLoad` in the `d.`/`BinStreamRev` dialect** — laneBO6
  measured **0 matched, 58 failed, zero counterexamples**. Retail constructs plain
  `BinStream` in `Load` bodies; `BinStreamRev` is real but only for *nested*
  loaders (`PropKeys::Load`, `ObjVector`/`ObjList` `operator>>`). Normalise to
  `bs >>` / `gRev` / `gAltRev` before searching, or discount these bodies as
  evidence and locate on the TU's other functions.
* **`Save`** — a Wii `SAVE_OBJ` is frequently a real `SAVE_REVS` body in retail.
* **`SyncProperty`** — local-static `Symbol` spelling is codegen-load-bearing.
* **`Handle`** — the tail above, plus the handler-arm list.

### A free fingerprint, now guarded

**316 bytes is exclusive to `OBJ_SET_TYPE`** (91 distinct classes, no other family
shares that size). The worklist tags any counterpart of that size so a `SetType`
is never mistaken for something else — and it is a ready-made extra positive
control.

---

## 6-ter. ★★ The phantom pre-flight — port-then-locate only pays if the TU EXISTS

Inverting the pipeline moves the cost forward: you spend the port *before* you
learn anything. So the pre-flight matters more than it did for the string and
RTTI channels, and this lane paid for the lesson.

I assigned a sub-lane laneBL §5.3's three biggest never-located rows —
`StorePackedMetadata` (124 fns, the largest lead in the entire worklist),
`StoreOfferContentsProvider` (31), `UISyncNetMsgs` (25). The port compiled: 636
functions, 90 TU-owned. The locator then placed **0 of 75** searchable TU-owned
bodies — no cluster clearing even one anchor.

★ **A wholesale 0-of-N is a different signature from a dialect divergence.** A
dialect divergence produces near-misses with shared ends (§6-bis); absence
produces nothing at all. The instrument diagnosed the phantom before the
dedicated test was run.

The mechanism is visible in the function names: `UpdateOfferStateFromEc`,
`ECContentCatalogInfo`, `StoreLoadPackedFile` — the **Wii EC / NAND packed
metadata store**. RB3-360 uses Xbox Live Marketplace, a different implementation.
`BufStreamNAND` and `AsyncFileCNT` fail identically for the same reason.

### The two-channel test is NOT sufficient — measured

laneBL §2 established the phantom test on two channels (class-name byte string +
`.?AV` RTTI type descriptor) and noted in passing that RTTI alone is not decisive.
**That caveat needs promoting to a rule, because the two-channel test over-calls.**
Sweeping it over laneBD's 141 never-located TUs flagged **`ArpeggioShape`** as a
phantom — and laneBL *located* `ArpeggioShape` at `0x82356118` on 5/5 selective
literals. A non-polymorphic or never-name-registered class has no name string and
no COL and is perfectly real.

`byte_locate.py phantom` therefore runs a **third, decisive channel**: do the TU's
own source string literals appear in the binary? Only 0-of-3 is a `PHANTOM`
verdict; one channel is `SUSPECT` and explicitly not a verdict.

### ★ And the class scan must read the HEADER ONLY

My first version scanned class declarations from the `.cpp` as well and read
`StorePackedMetadata` as **PRESENT** — because `StorePackedMetadata.cpp` declares
`StorePanel`, which is real (`name=5`, `rtti=1`) and owned by `StorePanel.cpp`.
All **14** classes actually declared in `StorePackedMetadata.h` read 0/0. That
single sloppiness would have kept a dead 124-function lead alive; it is fixed and
commented in-source.

### The refutation ledger — RE-RUN after the class-name fix, and it moved

★ **The first version of this ledger was wrong and is superseded.** After making
the tool derive class names from the compiled obj / header and **refuse** a bare
file-name probe (below), I re-ran **all 141** rows. The ledger changed materially:

| bucket | before (buggy) | **after (fixed)** |
|---|---|---|
| PHANTOM — refuted, 0 of 3 channels | 27 TUs / 187 fns | **12 TUs / 147 fns** |
| REFUSED — no class name derivable, undecidable | (mis-bucketed as refuted) | **20 TUs / 63 fns** |
| SUSPECT — one channel only | 14 | 8 TUs / 235 fns |
| PRESENT — worth porting | — | **86 TUs / 1,925 fns** |

**The honest refutation is 12 TUs / 147 functions**, not 27/187: `BufStreamNAND`
(22), `VocalOverlay` (21), `StoreRootPanel` (17), `AsyncFileCNT` (15), `HeldNote`
(13), `RGTutor` (12), `IntPacker` (10), `MidiInstrumentMgr` (10), `Submix` (9),
`FretHand` (9), `IIRFilter` (5), `TrackTest` (4). Fifteen rows I had listed as
refuted were not — they were undecidable or present. **Anyone who read the first
ledger should re-read this one.**

The **REFUSED** bucket is the important new output: 20 TUs where no class name is
derivable at all (`rso_utl`, `dxt1compress`, `wav`, `chardeform`, `graphicsutl`,
`Symbols*`, `Messages*`, …). Previously these produced a confident-looking
refutation from a file-name probe. Now they produce an explicit "cannot decide".
* **14 more are `SUSPECT`** and need adjudication, headed by `StorePackedMetadata`
  (5 of 43 literals present, and all five — `by_artist`, `by_difficulty`,
  `by_review`, `offer_id` — are generic store vocabulary another TU owns) and
  `ArpeggioShape` (5 of 13, but *selective*, and independently located). **The
  generic-vs-selective distinction is the whole adjudication**, and it is a human
  judgement the tool deliberately does not make.
  `Symbols*.cpp` appears here with ~1,500 of ~1,900 literals present — the
  systematic false positive CLAUDE.md already warns about.

### ★★★ CORRECTION — the sharpest lesson in this lane, and it is a TOOL bug

I assigned all three of the sub-lane's TUs as phantoms on the strength of an
**ad-hoc probe I typed by hand**, searching the binary for the *file* names
`StorePackedMetadata`, `StoreOfferContentsProvider`, `UISyncNetMsgs`. All three
read zero, and I told the sub-lane to stop.

**One of those three was wrong.** `UISyncNetMsgs` does not declare a class called
`UISyncNetMsgs` — it declares `ComponentFocusNetMsg`, `ComponentScrollNetMsg` and
`ComponentSelectNetMsg`, and those read `name=6, rtti=3, lits=3/20`. `byte_locate.py
phantom`, which reads class names out of the header rather than trusting the file
name, called it **PRESENT** correctly. My hand probe called it PHANTOM.

**Probe class names, never file names.** A Milo TU's file name is frequently not
any class it declares — `*NetMsgs`, `*Msgs`, `*Utl`, `Symbols*` are all like this.

★ This is a **locator correctness bug, not a usage note**: a file-name probe
yields a *silent false negative*, and in a locator a silent false negative is
indistinguishable from "this TU is unlocatable" — precisely the conclusion this
lane exists to stop people drawing. So it is fixed in the tool, not in the
instructions. `phantom` now derives class names from the **compiled obj's mangled
symbols** first (authoritative — it is what the compiler emitted), falls back to
the header, and **refuses to run** when neither yields a class rather than
quietly probing the file name.

And the locator settled it outright. Run against the sub-lane's compiled obj,
`UISyncNetMsgs` **placed with 8 TU-owned anchors** in one cluster:

```
owned=8 anchors=12  0x8269F3F8..0x8269FDE4   claimer: Performer.cpp(0x9B0)
  ??0ComponentFocusNetMsg  ??0ComponentScrollNetMsg  ??0ComponentSelectNetMsg
  ?Dispatch@ComponentFocusNetMsg  ?Load@ComponentScrollNetMsg
  ?Load@ComponentSelectNetMsg  ?Save@ComponentScrollNetMsg  ?Save@ComponentSelectNetMsg
+ a second cluster 0x82690A10..0x82690B74 (claimer NetGameMsgs.cpp) carrying
  ?Load@ComponentFocusNetMsg and ?Save@ComponentFocusNetMsg
```

So `band3/game/UISyncNetMsgs` is **a new location, not a refutation** — pin-ready
out of `Performer.cpp`'s pin, with a second block in `NetGameMsgs.cpp`. It is the
lane's seventh placed TU.

★ **The ordering that follows is the real rule: the compile-and-byte-search is a
STRONGER existence test than all three phantom channels combined**, because it
looks for the code itself rather than for metadata *about* the code. The phantom
pre-flight is a cost-saving heuristic to run *before* paying for a port. **Once an
obj exists, the locator overrules it** — never discard a compiled obj on a phantom
verdict.

Applied back to the other two: `StorePackedMetadata` is `SUSPECT` on the tool's
channels *and* reads 0-of-75 owned bodies located — both agree on absent.
`StoreOfferContentsProvider` is `SUSPECT` and the locator declines it (1 owned hit,
no cluster clearing an anchor); its `mPackedData` member has no counterpart on
RB3-360's `StoreOffer`, which is DataArray-driven. Those two stand as refuted; the
third does not.

**Run `byte_locate.py phantom <tu>…` before funding any port** — it costs seconds
and it retired 27 TUs. But treat a PHANTOM verdict as a *budget* decision, not a
fact, and let the locator overrule it whenever an obj already exists.

---

## 7. What this refutes and what it opens

**Refuted:** laneBL §6's verdict that the controller family "sits in rows
unreachable by *any* refinement of the existing instruments" — true of the string
and RTTI channels, and the reason they wrote it stands, but the pool is reachable.
265 functions moved from *unreachable* to *located* without a single new
positional signal.

**Refuted:** that "just compile the unpinned objs and sweep" is untapped yield.
147 swept, 6 placed — and the 6 are exactly the TUs this lane ported.

**Refuted, with evidence:** two of laneBL §5.3's three biggest never-located leads
— `StorePackedMetadata` (124 fns) and `StoreOfferContentsProvider` (31) — plus 27
further TUs / 187 functions, are not in the RB3-360 binary at all (§6-ter). The
third, `UISyncNetMsgs` (25), I initially refuted **in error** and the locator
overturned it: it is now LOCATED at `0x8269F3F8..0x8269FDE4`. See §6-ter for the
correction and the rule it produced.

**Corrected:** the phantom test needs THREE channels, not two, and its class scan
must read the header only. Both corrections were forced by counterexamples inside
this lane's own results.

**Opened:** the ordering. Every previous locator required evidence to *precede*
the port. This one requires the port to precede the evidence, which means **any
Wii-oracle TU we are willing to port is now placeable**, regardless of whether it
has literals, RTTI, or unclaimed space. laneBL §5.3's twelve single-literal
leads (`StorePackedMetadata` 124 fns, `StoreOfferContentsProvider` 31,
`UISyncNetMsgs` 25, `VocalOverlay` 21, `ChordPreview` 20, `NowBar` 16,
`GameMic` 15) and laneBD's 65 never-placed TUs are all now port-then-locate
candidates. **That, not the 265 functions, is the result.**

---

## 9. ★★★ Three coordinator findings, verified and folded in

### 9.1 The `.text` VA→file-offset mapping — CHECKED, and it was already right

`.text` is RVA `0x00270000` but `PointerToRawData` `0x00264E00` — a **`0xB200`
delta**. Computing a file offset as `va - 0x82000000` is valid only for `.rdata`;
a sibling lane used it on `.text`, disassembled the wrong bytes and had to retract
a refutation.

This tool reads `.text` as `data[PointerToRawData : +size]` and indexes it by
`(va - VirtualAddress)` — the correct per-section mapping — everywhere. Verified
against the supplied anchor:

```
off(0x824DAAD0) = 0x004CF8D0   expected 0x004CF8D0   OK
naive va-0x82000000 would give 0x004DAAD0   (the bug)
tool bytes @VA : 7d8802a64834e77d3be1ff609421ff60
file bytes@off : 7d8802a64834e77d3be1ff609421ff60   IDENTICAL
```

It is now a **hard startup assert** so it cannot silently regress. There is also a
strong independent argument that it was never wrong: a 0xB200-shifted mapping
produces essentially *zero* matches, not 434 correct placements at 0.98 precision
with a 0/38,960 rotation control.

### 9.2 ★★ `.pdata` absence is not a "not a function" test — and it was blocking §9.3

Frameless leaves are systematically absent from the X360 `.pdata` table, so
anchoring candidate entry points on `.pdata` alone is **structurally blind** to
them. Measured: **4,963 `bl` targets in `.text` have no `.pdata` entry**
(57,733 `.pdata` entries; 26,954 distinct call targets).

Entry points are now `.pdata` ∪ `bl`-targets (`--no-call-entries` restores the
old behaviour). Honest A/B on the same 60-unit calibration:

| | `.pdata` only | **+ call targets** |
|---|--:|--:|
| recall | 0.6152 | **0.6152** |
| precision (unique) | 0.9841 | **0.9809** |
| TU-level | 47/50 | 47/50 |
| rotation control | 0/38,960 | 0/38,960 |

**+29 functions found for −0.3 pp precision**, and a structural blind spot closed.
Taken.

### 9.3 ★★★ `relocaudit` — the at-100% defect detector, and it passes the acid test

`functionRelocDiffs=none` masks the operand of a relocated branch, so a body that
calls the **wrong function** still scores a clean 100 % and **no sub-100 scanner
can ever surface it**. The fix is to compare what the masked relocations *point
at*: decode retail's actual branch target, resolve it through
`target_symbol_map.json`, and compare with the symbol our own relocation names.

Acid test on the supplied case — **passes**:

```
NAME CONTRADICTION  ??1NewAwardPanel@@UAA@XZ
   our VA 0x82630340 (NewAwardPanel.obj)
   +0x040  we call ??1TexLoadPanel@@UAA@XZ
           retail calls 0x8262F390 = ??1VoiceoverPanel@@UAA@XZ
```

★ **Two things had to be right, and the first attempt got both wrong** — worth
recording because they are the general traps for anyone rebuilding this:

1. **The defect is a TAIL CALL** (`b`, LK=0), not a `bl`. A `bl`-only filter
   silently misses the entire class the detector exists to find.
2. **Retail's `??1NewAwardPanel@@` at `0x82630340` is not in `.pdata`.** §9.2 was
   *blocking* §9.3 — the frameless-leaf blindness and the at-100% detector are the
   same bug wearing two hats.

Two tiers, because the confidence genuinely differs:

* **HARD** — both callees are mapped and the two candidate retail bodies **differ**.
  ICF folds are filtered by comparing those bodies directly (without that filter
  `??3@YAXPAX@Z` vs `??3BinStream@@SAXPAX@Z` and `??$Find@VRndMat@@` vs
  `??$Find@VRndCam@@` — laneBL §4.2's same-size template twins — both fire).
* **NAME** — our callee is unmapped, so only the names are comparable. This is the
  tier the motivating defect lives in. ICF aliasing can explain a hit here.

Tree-wide over functions **currently scoring exactly 100 %**, deduplicated per
symbol: **18,712 distinct functions audited, 72,458 branch sites, 1,738 HARD and
4,758 NAME**.

★ **Those are NOT defect counts and must not be quoted as such.** My first run
reported 11,348/14,858 because a symbol defined in N objs was audited and counted
N times — site count is blast radius, never yield (the standing rule, and I walked
straight into it). Even deduplicated, each hit needs adjudication into **(a) our
code calls the wrong function** or **(b) the symbol map names the callee wrongly**.
Both are worth fixing and they are different findings. What is established is that
the *detector* works: it names the known case exactly, with the ICF filter and the
tail-call and frameless-leaf paths all required to get there.

---

## 8. Artifacts

* `scripts/harvest/byte_locate.py` — the instrument (branch `laneBO4-loc`).
* `docs/plans/lane-bo4-byte-search-locator-2026-07-29.md` — this file.
* `src/system/meta/StoreArtLoaderPanel.cpp` + its `objects.json` entry — branch
  `laneBO4-loc` (pin-ready, span in §4-bis, NOT pinned).
* `src/system/beatmatch/{KeyboardController,ButtonGuitarController,RealGuitarController,JoypadGuitarController,JoypadMidiController}.{cpp,h}`
  + `config/45410914/objects.json` + `config/45410914/splits.txt` + map fragments
  — branch `laneBO4-land`.
* `build/45410914/byte_locate_defmap.json` — regenerable cache, gitignored.
* Un-landed, pin-ready, ports live in `~/tmp/laneBO4/wt-big` (branch `laneBO4-big`,
  uncommitted): `band3/game/UISyncNetMsgs` at `0x8269F3F8..0x8269FDE4` (donor
  `Performer.cpp`) + `0x82690A10..0x82690B74` (donor `NetGameMsgs.cpp`).

### ★ A fourth channel, visible from source alone — now `byte_locate.py design`

The sub-lane that ported the store TUs found the cleanest statement of why they
are absent, **without any binary probe**: RB3-360's `StoreOffer` is DataArray-driven
(`DataArray *mStoreOfferData`), while rb3-Wii's is a `#pragma pack(1)` view onto a
packed binary blob (`mPackedData`). Those are not two versions of one class, they
are two different store implementations — and `StoreOfferContentsProvider::BuildList`
reads `mOffer->mPackedData->mIsRBN`, which has no counterpart on our side, so it
could not be ported faithfully at all. **When the oracle's class and our class
share a name but not a data model, the TU around it is a different implementation.**
That is cheaper than any of the three binary channels and should be checked first —
so it is now a mode: `byte_locate.py design <Class>…` compares member sets and
`#pragma pack` between our header and the Wii header.

```
StoreOffer           DIVERGENT-DESIGN  ours=6 wii=4 shared=3 jaccard=0.43  PACK-MISMATCH
     ours: mReleaseDateStr, mSongsInOffer, mStoreOfferData
     wii : unk6c
KeyboardController   COMPATIBLE        jaccard=1.00
UIProxy              COMPATIBLE        jaccard=0.75
```

The point of the gate is not the compile it saves. It is that it **distinguishes
"our source is a different program" from "we have not found it yet"** — two
failures that are otherwise identical in the output and would both land in the
false-negative bucket.

### MWCC -> MSVC porting fixes worth carrying forward

* `Symbol::mStr` is private — use `Str()`.
* `ObjectDir::sMainDir` is protected — use `ObjectDir::Main()`.
* `sprintf` needs an explicit `#include <stdio.h>`; nothing on the engine PCH path
  pulls it in.
* The oracle contains ill-formed `else bool _cond = …;` that MWCC tolerated and
  MSVC rejects — rewrite as an else-if chain.
