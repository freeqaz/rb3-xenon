# W16-EQ — the "vetted relocation-name" band is FIVE MAP MISLABELS AND ONE REAL SOURCE BUG, not a fold wall

Lane W16-EQ, 2026-09-16. Branch `w16-eq`, worktree `~/tmp/wt-w16-eq`, off main `0f93166c`.

## Brief, and what verification changed

Six sub-100 rows, 5,100 B, all with `mpn == fuzzy` to the digit. Every briefed
figure reproduced **exactly** against `build/45410914/report.json` on main and
again in the worktree after a full build. Two corrections to the brief, neither
material:

- `diff_score` is **not a field in `report.json`** — that column comes from
  `objdiff-cli diff`'s JSON (`diff_score.score`). Verified: TCRP::Handle = 5.
- `OnMsg@NetSession` is **eight** overloads with **four** already at
  `fuzzy == 100` (84/120/272/136 B), not "seven / three".

## The band is 100% relocation-name, confirmed

Enumerating charged sites per row with `objdiff-cli diff` (no `--build`) at the
grader's own map file: **10 charged sites across the 6 rows, ALL `diff_arg`,
ZERO instruction-level**. W16-EJ's characterisation of this band holds exactly.
No amount of ordinary source-logic work touches these rows.

## The discriminator that actually separates A from B

A name-shape test cannot do it (W16-EJ), and "byte-identical ⇒ folded" is false
(W16-EP). What worked here, non-circularly:

1. **Resolve the callee's identity from retail `.rdata` string bytes.** Milo Msg
   ctors call a `Type()` accessor that interns a literal; reading that literal
   out of `band.exe` names the class with no reference to the map.
2. **The MPNGAP-1 signature test** — does the named callee's signature match what
   the call site does with `r3..r7`?
3. **Caller-set fan-in**, censused by scanning retail `.text` for every `bl`
   whose resolved destination is the candidate address (opcode 18, AA=0, LK=1).

Fan-in turned out to be the clean separator, and it is stronger than byte
identity because it does not ask whether two bodies *look* alike:

| candidate address | map name | callers | verdict |
|---|---|---:|---|
| `0x8235BF00` | `__copy_ptrs<const int*,int*>` | **4** | MISLABEL |
| `0x826F1080` | `RndGroup::Draw()` | **2** | MISLABEL |
| `0x823E1B40` | `SpeechEnableMsg::ctor` | **3** | MISLABEL |
| `0x824416D0` | `push_back<vector<PressRec>>` | **15** | ICF fold |
| `0x823E4630` | `vector<Vector3>::vector(const&)` | **10** | ICF fold |
| `0x82788308` | `push_back<vector<GemInProgress>>` | **44** | ICF fold |
| `0x82B5F808` | `push_back<vector<ChatReceiver*>>` | **453** | ICF fold |

A mislabel has a **small, semantically coherent** caller set — every caller is a
site our own source spells with the *same* callee. A fold survivor has **large,
heterogeneous** fan-in: 453 call sites that our source spells as many different
`push_back<vector<T*>>` instantiations cannot all be one function unless the
linker folded them.

## Per-row adjudication

| row | size | charges | class | evidence |
|---|---:|---:|---|---|
| `?Handle@TourChallengeResultsPanel@@` | 1392 | 1 | **A** | `0x8235BF00` is not a copy loop; lives in `Tour.s`; 4-for-4 caller bijection with our `GetQuest()` sites |
| `?OnMsg@NetSession@@…UpdateUserDataMsg` | 296 | 1 | **A** | ctor names **swapped** in the map; proved on `.rdata` strings |
| `?OnMsg@NetSession@@…AddUserResponseMsg` | 292 | 1 | **A** | `Type()` interns `"add_user_result"`; **plus a real source bug** (int vs bool) |
| `?InitFretSteps@RGTrainerPanel@@` | 1220 | 2 | **A + B** | charge 1 mislabel (fixed); charge 2 is a 15-way fold ⇒ **row cannot cross** |
| `?OnMsg@NetSession@@…AddUserRequestMsg` | 664 | 2 | **B, B** | 44-way and 10-way folds |
| `?OnMsg@NetSession@@…JoinRequestMsg` | 1236 | 3 | **B, B, B** | 44-, 10- and 453-way folds |

Collectable from the briefed six: **1,980 B of 5,100**. The rest is fold noise,
and because `matched_code` is all-or-nothing per row, `InitFretSteps` pays **0 B**
even though one of its two charges was a genuine defect I fixed.

## The five map defects

### 1–2. A swapped pair: `0x823E1E20` ↔ `0x823E2020`

Both ctors live in `NetSession.s`, are shape-identical, and differ in exactly two
relocations — the `Type()` accessor and the vtable pointer. **Differing
relocations means MSVC ICF could not have folded them.**

Reading the interned literals straight out of retail `.rdata`:

- `0x82054AE8` = `"removing_remote_user"` ⇒ `fn_823E0CA8` is `RemovingRemoteUserMsg::Type()`
- `0x82054B88` = `"remote_user_updated"` ⇒ `fn_823E0DA8` is `RemoteUserUpdatedMsg::Type()`

The ctor at `0x823E1E20` calls the *removing* Type but was mapped
`??0RemoteUserUpdatedMsg`; `0x823E2020` calls the *updated* Type but was mapped
`??0RemovingRemoteUserMsg`. The two rows' charges are perfect mirror images of
each other — the self-evidencing signature of a swap.

Retail's `OnMsg(UpdateUserDataMsg)` calls `0x823E2020`, i.e. the **real**
`RemoteUserUpdatedMsg` ctor — exactly what our source already did. **Our source
was right and the map was wrong**, the MPNGAP-1 pattern.

### 3. `0x8235BF00`: `__copy_ptrs<const int*,int*>` → `?GetQuest@Tour@@QAAPAVQuest@@XZ`

68 B, lives in `Tour.s`, no copy loop — loads a member at `0x30`, null-checks it,
calls through a global. Its four retail callers are `Tour::CreateAndSubmitMusicLibraryTask`,
`TrackerManager::StartIntro`, `TrackerManager::ConfigureQuestGoal`, and
`TourChallengeResultsPanel::Handle`; our source calls `GetQuest()` in exactly
those four functions and nowhere else. `Tour.obj` emits `GetQuest` out of line,
so the row re-pairs with a real body rather than being orphaned.

### 4. `0x823E1B40`: `??0SpeechEnableMsg` → `??0AddUserResultMsg@@QAA@_N@Z`, **and a source bug**

Its `Type()` accessor interns `"add_user_result"`. Three retail callers, all in
NetSession's address range, **none in SpeechMgr** — matching our three
`AddUserResultMsg` constructions in `NetSession.cpp`.

Retail's body opens `clrlwi r11, r4, 24` — an 8-bit zero-extend. Extracting our
own compiled bodies from the COFF objects settled the parameter type without
guessing:

| our symbol | word at +0x18 |
|---|---|
| `??0AddUserResultMsg@@QAA@H@Z` (int) | `39600000` = `li r11, 0` |
| `??0SpeechEnableMsg@@QAA@_N@Z` (bool) | `548B063E` = `clrlwi r11, r4, 24` |

Retail has the `clrlwi`. ⇒ **retail's ctor takes a bool and our source declared
`int`** — a genuine source-level signature divergence that the wrong map name had
been masking. Fixed in `BandUser.h`; the map entry uses the honest `@_N@Z`
mangling rather than bending the name to fit our source.

### 5. `0x826F1080`: `?Draw@RndGroup@@QAAXXZ` → `?GetFinger@FretHand@@QBAXIAAH00@Z`

`mulli r11, r4, 0xc` then three loads written through **r5, r6, r7**. A no-argument
`Draw()` cannot write through three out-pointers. Two callers, both fret-display
functions. **This rename was pre-registered as worth 0 bytes** and landed purely
for accuracy.

## Why the un-pairing risk was zero

Every row renamed *away from* was already below 100 — `__copy_ptrs` 43.23%,
`??0SpeechEnableMsg` 99.85%, `?Draw@RndGroup` 33.33% — and `matched_code` is
all-or-nothing, so each contributed **0 bytes** before the edit and can lose
nothing. No new duplicate names were introduced (the 2 pre-existing duplicates
are untouched), and the ctor swap is a proper 2-cycle that keeps the map a
bijection.

## What I deliberately did NOT do

- **No aliases.** Four of the ten charges are genuine ICF folds. An alias would
  close them and lift `name_check` by construction; that is forgiveness, not
  evidence, and the lane brief's default is refuse. They stay charged.
- **No re-homing.** `0x823E1B40` is pinned into `default/SpeechMgr` and
  `0x826F1080` into `default/band3/meta_band/ViewSetting`, both almost certainly
  mis-pins. Re-homing an already-pinned address is *not* metric-neutral, and it
  is a separate question from naming. I corrected the names and left the pins.
- **Did not chase the 4th mislabel of the same shape.** `RegisterOnline` and
  `Disconnect` both charge `_M_allocate_and_copy<const unsigned long long*>`
  against our `MakeQuazalSessionJob(QuazalSession**, bool)` — the identical
  STL-name-over-game-function pattern. Left for a follow-up lane; noted here so
  the vein is not re-hunted from scratch.
- Did not attempt the three fold-bound rows (3,120 B of the briefed 5,100).

## Closing the "did my census miss a reference?" hole

The `bl` census only sees opcode-18 branches with LK=1. A tail-call `b` (LK=0) or
a **data** pointer to a renamed address would have been invisible to it, and
would have shown up as an unexplained regression. Re-run over the whole `.text`
accepting both LK values, plus a raw big-endian pointer scan of `.rdata`/`.data`:

```
AddUserResultCtor  bl 3      GetFinger bl 2      GetQuest bl 4
ctorA              bl 1      ctorB     bl 1
data/rdata pointer references to the renamed addresses:   (none)
```

Zero tail-calls, zero data references. The only references to all five renamed
addresses are the `bl` sites already enumerated, so the blast radius is fully
accounted for. (None of the five is virtual — `RndGroup::Draw` is `QAA`, not
`UAA` — so no vtable slot can hold them either.)

## Pre-registration vs measured

Pre-registered **before** the first A/B: **+3,428 B / +9 functions**, range
+3,428..+3,496 B. Named failure modes: (1) the bool change perturbs caller
codegen; (2) the 68 B GetQuest row's body does not match; (3) re-split /
`symbols.txt` convergence moves unrelated rows; (4) a data reference my `bl`-only
census missed; (5) an unseen second charge on a ctor row.

**Failure mode (1) fired, in a form I did not predict.** I expected perturbed
codegen; the actual failure was a **compile error**: `DECLARE_MESSAGE` also
generates an `AddUserResultMsg(DataArray*)` ctor, so with a `bool` overload the
literal `0` at `NetSession.cpp:250` became an ambiguous call (`C2668`). The first
A/B **refused at exit 2 with no deltas** — the tool behaving exactly as designed
rather than reporting a broken run. Fixed by spelling the three call sites
`false`/`true`, which is codegen-identical (`li r4, 0/1` either way).

Fixing it produced an unplanned but decisive confirmation: our recompiled
`??0AddUserResultMsg@@QAA@_N@Z` now emits `548B063E` (`clrlwi r11, r4, 24`) at
+0x18 — **byte-matching retail** where the `int` version had emitted `li r11, 0`.
The signature fix is correct on retail bytes, independent of the metric.

Failure mode (4) was closed by measurement (census above), not by assumption.


### Measured

`python3 tools/ab_measure.py --worktree /home/free/tmp/wt-w16-eq --from-dirty`,
run dir `20260916-051906-from-dirty-393156`, patch `03673a3ba7c2d597`,
kinds `['map','source']`, both legs settled and both at a `symbols.txt` split
fixed point after 0 extra re-splits, ruler `functionRelocDiffs=name_check`
(from `objdiff.json`, i.e. the grader's own):

```
leg A: matched=43958 masked=23224 honest=20734 code%=40.260880  (recompiles 0, settled)
leg B: matched=43967 masked=23224 honest=20743 code%=40.294340  (recompiles 256, split=1, renamer_patched=1830)
Δmatched=+9  Δhonest=+9  Δcode%=+0.033460pp  Δcode_bytes=+3428
```

**Pre-registered +3,428 B / +9 functions. Measured +3,428 B / +9 functions.**
The point prediction landed exactly, and so did the decomposition — the tool's
per-unit list is `NetSession +5, TrackerManager +2, Tour +1, TCRP +1`, which is
the pre-registered breakdown line for line.

The nine rows that crossed to `fuzzy == 100`, from the archived leg reports:

| unit | row | size | legA fuzzy | legB fuzzy |
|---|---|---:|---:|---:|
| TourChallengeResultsPanel | `?Handle@TourChallengeResultsPanel@@` | 1392 | 99.985634 | **100.0** |
| NetSession | `?AddLocalUser@NetSession@@` | 380 | 99.947365 | **100.0** |
| Tour | `?CreateAndSubmitMusicLibraryTask@Tour@@` | 344 | 99.941864 | **100.0** |
| NetSession | `?OnMsg@NetSession@@…UpdateUserDataMsg` | 296 | 99.932434 | **100.0** |
| NetSession | `?OnMsg@NetSession@@…AddUserResponseMsg` | 292 | 99.931500 | **100.0** |
| TrackerManager | `?ConfigureQuestGoal@TrackerManager@@` | 260 | 99.923080 | **100.0** |
| NetSession | `??0RemoteUserUpdatedMsg@@` | 164 | 99.878050 | **100.0** |
| NetSession | `??0RemovingRemoteUserMsg@@` | 164 | 99.878050 | **100.0** |
| TrackerManager | `?StartIntro@TrackerManager@@` | 136 | 99.852940 | **100.0** |

Sum = **3,428 B**, which is the whole delta: nothing else moved a byte.

★ **Only 1,980 B of the 3,428 came from the briefed rows. The other 1,448 B is
cascade into rows the brief never mentioned.** The brief offered 5,100 B across
six rows; I adjudicated 1,980 B of it as collectable (three rows class A) and
3,120 B as unreachable (three rows carrying at least one genuine fold). **Both
halves of that adjudication were confirmed exactly**:

- the three class-A briefed rows crossed — 1392 + 296 + 292 = **1,980 B**;
- the two pure-fold `OnMsg` rows (1,236 B and 664 B) **did not move at all**;
- `?InitFretSteps@RGTrainerPanel@@` (1,220 B), adjudicated A+B, went
  **99.96722 → 99.983604 and did not cross** — one charge closed by the
  `GetFinger` correction, the other a genuine fold. ⇒ **+0 bytes.** This is the
  all-or-nothing rule of `matched_code` demonstrated cleanly: a real, correct
  repair to a row that still carries one surviving charge is worth **exactly
  nothing** to the metric, and a lane pricing that row off its charge *count*
  would have booked 1,220 B that cannot be collected.

The cascade (out of brief) is the map fixes paying out at the other call sites
of the same five addresses: `AddLocalUser` 380, the two swapped ctors 164 + 164,
`Tour::CreateAndSubmitMusicLibraryTask` 344, `TrackerManager::ConfigureQuestGoal`
260 and `?StartIntro@TrackerManager@@` 136 = **1,448 B**.

**Failure mode (2) fired, and it is why I registered a RANGE.** The 68 B
`?GetQuest@Tour@@` row re-paired against a real body but scored **81.70588**,
not 100 — so the outcome landed at the bottom of the +3,428..+3,496 range, not
the top. The row is now identified and mostly-matching where it was previously a
43.24% `__copy_ptrs` mispairing; closing the remaining 18 pp is ordinary source
work on `Tour::GetQuest`, left for a later lane.

**Three rows un-paired, all worth 0 `matched_code`, exactly as pre-registered.**
Each was below 100 on the A leg, so none was contributing bytes:

| address | A-leg row | A fuzzy | B-leg row | B fuzzy |
|---|---|---:|---|---:|
| `0x8235BF00` | `__copy_ptrs<const int*,int*>` | 43.235294 | `?GetQuest@Tour@@` | 81.70588 |
| `0x823E1B40` | `??0SpeechEnableMsg@@` | 99.852940 | `??0AddUserResultMsg@@` | 0.0 |
| `0x826F1080` | `?Draw@RndGroup@@` | 33.333332 | `?GetFinger@FretHand@@` | 0.0 |

⚠ **The two rows that read 0.0 are a pinning artifact, not a regression, and the
next lane should not read them as one.** `0x823E1B40` is pinned into
`default/SpeechMgr` and `0x826F1080` into `default/band3/meta_band/ViewSetting`,
but our definitions of `AddUserResultMsg::ctor` and `FretHand::GetFinger` compile
into *different* translation units — and **objdiff pairs by name within a unit**,
so a correctly-named row whose base obj cannot define that name reads 0%. The
identification is right; the **pin is wrong**. That is the re-homing class
(PINHOME-1: re-homing is *not* metric-neutral), and it is worth 172 B if someone
moves those two addresses to the units that actually compile them. I did not do
it — re-homing is a different lever with a different risk profile, and folding it
into this measurement would have made the +3,428 unattributable.

This also fully explains the only negative reading in the run:
**Δfuzzy = −0.001155pp** aggregate. Renaming away three partially-matching rows
(43.24 / 99.85 / 33.33) forfeits their partial fuzzy credit; `matched_code` is
unaffected because none of them was at 100. The `none` control reads −136 B and
is **NOT_APPLICABLE by the tool's own classifier** — the patch contains source,
so movement on `none` is expected and the alias shape is only adjudicable on a
map-only patch. I am not citing it as a clearance.

One incidental row improved without crossing:
`?DisplayChord@ChordbookPanel@@` (3,436 B) 97.179276 → 97.185100, collateral from
the `GetFinger` correction. Worth 0 B.

## Handoff: a fourth instance of the same shape, located but NOT fixed

`0x823F2C98`, mapped
`??$_M_allocate_and_copy@PB_K@?$vector@_KV?$StlNodeAlloc@_K@…`, has **3 callers,
all inside NetSession's address range** — `??_ENetSession`,
`?UpdateSyncStore@NetSession@@`, `?RegisterOnline@NetSession@@`. Our source
spells that callee `??0MakeQuazalSessionJob@@QAA@PAPAVQuazalSession@@_N@Z`. A
genuine STL `_M_allocate_and_copy<const unsigned long long*>` would show broad
heterogeneous fan-in the way the four proven folds here do (15 / 44 / 10 / 453);
three callers inside one class is the mislabel signature.

`?RegisterOnline@NetSession@@QAAXXZ` (136 B) and `?Disconnect@NetSession@@QAAXXZ`
(136 B) each carry **exactly one charge**, and it is this one — so the repair is
worth roughly 272 B plus whatever else references it.

I did not fix it: it is outside this lane's six briefed rows, and folding it in
after pre-registering would have made the measured delta unattributable. Flagged
here so the next lane starts from the address, not from scratch.

## The instrument, stated for reuse

For a `diff_arg` relocation-name charge where our callee's name is **absent from
the map**, the question "fold or mislabel?" is answered by the **caller set of
the target address**, censused directly off retail `.text`:

- **few callers, all semantically the same function in our source ⇒ MISLABEL**
  (fixable, and the fix is a map edit, not a source edit)
- **many callers, spelled in our source as many different callees ⇒ ICF FOLD**
  (unfixable without an alias)

This does not depend on byte identity, so it is immune to the W16-EP refutation
("byte-identical ⇒ folded" is false — retail keeps eleven identical `erase`
bodies unfolded). And where the callee is a Milo `Message`, the interned type
string in retail `.rdata` names the class outright, with no reference to the map
at all — that is the non-circular anchor everything else here was checked against.
