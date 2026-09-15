# W16-CB — `Handle@Game` adjudication, 8 anonymous-row identifications, `LoadSong`

**Lane:** W16-CB · **Branch:** `w16-cb` (off main `11f5b142a61e`) · **Date:** 2026-09-15
**Worktree:** `/home/free/tmp/wt-w16-cb` · **Ruler:** shipped `name_check`, read from
`build/45410914/report.json` (`provenance.diff_config`) on every figure below.

> Every number here was measured by a full `./tools/ninja-locked` build followed by a
> read of `report.json`. No figure comes from `run_objdiff`, `objdiff-cli --build`, or a
> single-`.obj` ninja invocation — those skip the six obj patchers, which are part of the
> ruler.

---

## 0. Headline

| | matched_functions | matched_code | matched_code_percent |
|---|---:|---:|---:|
| baseline (`11f5b142a61e`) | 43,682 | 4,068,676 | 39.705757 |
| after this lane | **43,686** | **4,075,956** | **39.776802** |
| delta | **+4** | **+7,280 B** | **+0.071045** |

`fuzzy_match_percent` 49.86114 -> 49.89027.

**CROSSED IN — 5 rows, 7,420 B**

| bytes | row |
|---:|---|
| 5,428 | `default/band3/game/Game::?Handle@Game@@UAA?AVDataNode@@PAVDataArray@@_N@Z` |
| 1,036 | `default/band3/game/Game::?Poll@Game@@QAAXXZ` |
| 408 | `default/band3/game/Game::?LoadSong@Game@@QAAXXZ` |
| 408 | `default/band3/game/Game::?PostLoad@Game@@QAAXXZ` |
| 140 | `default/ContentMgr_Xbox::?IsCorrupt@XboxContentMgr@@UAA_NVSymbol@@@Z` |

**FELL OUT — 1 row, 140 B**

| bytes | row |
|---:|---|
| 140 | `default/ContentMgr_Xbox::?IsCorrupt@XboxContentMgr@@UAA_NVSymbol@@AAPBD@Z` |

⚠ **The one fell-out row is rename bookkeeping, not a regression.** It is the *same
function* under its old spelling: it scored 100% before and scores 100% after, and its
replacement is the crossed-in 140 B row directly above. Net for that change is **Delta 0**
on every key, which is what I predicted. **Genuine regressions: 0.**

Commits on `w16-cb`:

| sha | what |
|---|---|
| `aef4471d` | map: `0x82684e90` -> `?TotalBasePoints@SongDB@@QAAHXZ` (section 1) |
| `5ef98992` | map: 8 anonymous Game rows named (section 2) |
| `a0df0ae5` | `Game::Poll` calls `VocalGuidePitch::Poll` (section 3) |
| `6a0b05b5` | `Shuttle::~Shuttle` defined (section 4) |
| `a67a9ff5` | `ContentMgr::IsCorrupt` signature + map row (section 5.2) |
| `b3f7c466` | `Game::LoadSong` restructured (section 5) |

**Predictions vs measurements** — the record, including the miss:

| change | predicted | measured |
|---|---|---|
| `Handle` map repair | +2 rows / +5,836 B | +2 rows / +5,836 B ✅ |
| 8 namings | Delta 0 | Delta 0 ✅ |
| `Poll` fix | +1,036 B / +1 fn | +1,036 B / +1 fn ✅ |
| `~Shuttle` definition | Delta 0 | Delta 0 ✅ |
| `IsCorrupt` signature | Delta 0 | Delta 0 ✅ |
| `LoadSong` attempt 1 | **+408 B** | **+0 B** ❌ (54.49 -> 97.598; sub-100 pays nothing) |
| `LoadSong` attempt 2 | +408 B / +1 fn | +408 B / +1 fn ✅ |

---

## 1. The adjudication: `?Handle@Game@@` — WRONG MAP NAME, not an ICF fold

The brief posed this as the lane's first task: a 5,428 B row sitting on a single
relocation-name charge is **either** a wrong map name (which pays) **or** a genuine ICF
fold (which does not) — and under `name_check` the two are bit-for-bit the same
observation. *Target calls A, we call B, A != B* is simultaneously the definition of a
wrong callee and of a fold whose survivor kept the twin's spelling. The metric cannot
separate them; only retail bytes can.

The charge: target `?TypeDef@Object@Hmx@@QBAPAVDataArray@@XZ` vs our
`?TotalBasePoints@SongDB@@QAAHXZ`, at map address `0x82684e90`.

**Verdict: WRONG MAP NAME.** The map row was repaired to
`?TotalBasePoints@SongDB@@QAAHXZ` (commit `aef4471d`). Four independent lines of
retail-byte evidence, no two of which share a failure mode:

1. **The body says so.** `0x82684e90` is 8 bytes: `lwz r3,0x1c(r3) ; b 0x826cd8e0`,
   and `0x826cd8e0` is `?TotalBasePoints@MultiplayerAnalyzer@@QBAHXZ`. That is
   literally `SongDB::TotalBasePoints() { return mMultiplayerAnalyzer->TotalBasePoints(); }`
   (`src/band3/game/SongDB.cpp:129`). The compiler places `SongDB::mMultiplayerAnalyzer`
   at `0x1c`, confirming the offset. Two independent disassemblers agree on the body.

2. **ICF is excluded by construction, not by argument.** MSVC folds only COMDATs
   identical *including relocations*. Our `Object::TypeDef` COMDAT is
   `806300104e800020` (`lwz r3,0x10(r3) ; blr`) — 8 bytes, **zero relocations**.
   Retail's body is `8063001c48048a4c` — a different offset, a different second
   opcode, and it **carries a branch relocation the other lacks**. These cannot fold.
   (`Object::mTypeDef` is at `0x10`; `0x1c` is `mDir`.) Neither spelling appears in
   `scripts/symbol_aliases.json`.

3. **Our bytes match retail's.** Our compiled `?TotalBasePoints@SongDB@@QAAHXZ` is
   `size=8 raw=8063001c4bfffffc` with `reloc@4 -> ?TotalBasePoints@MultiplayerAnalyzer@@QBAHXZ`
   — the same offset, the same shape, the same relocation target as retail.

4. **The call context pins the slot.** Exactly two retail callers reach `0x82684e90`:
   `?PostLoad@Game@@` and `?Handle@Game@@` — precisely the two places our source calls
   `TheSongDB->TotalBasePoints()` through the inlined `Game::PrintBasePoints()`
   (`Game.cpp:806-809`, reached from `HANDLE_ACTION(print_base_points, ...)` at
   `Game.cpp:1198`). A wrong name would not reproduce the caller set.

**Control against "the name is just an arbitrary ICF survivor".** A tree-wide sweep of
all 1,219 compiled objects found **exactly one** function with that COMDAT shape — there
is no twin for it to have folded with, so the survivor-name story has nothing to stand on.
The sweep is **proved non-vacuous**: the same instrument found a genuine **12-member**
fold family at the adjacent shape `8063001c4e800020`. An instrument that finds folds
where they exist and none here is evidence; one that finds none anywhere is a bug.

**Spatial corroboration** (weak, but free): the neighbouring map rows are
`GetNumTracks@SongDB`, `GetBaseMaxPoints@SongDB`, `GetBaseMaxStreakPoints@SongDB`.

### What would overturn this

Retail-byte evidence that some **other** class has a member pointer at `0x1c` whose
accessor tail-calls `MultiplayerAnalyzer::TotalBasePoints`, **and** that better explains
the `PostLoad`/`Handle` call sites than `SongDB` does. Because we already compile a body
that reproduces retail's bytes exactly, such a rival would have to come from a body we do
**not** compile. Finding one would not merely rename the row — it would mean our
`SongDB::TotalBasePoints` is coincidentally byte-identical to an unrelated accessor, which
the caller-set evidence (point 4) independently argues against.

**Measured:** the repair crossed `?Handle@Game@@` (5,428 B) and `?PostLoad@Game@@`
(408 B). Predicted **+2 rows / +5,836 B** before building; measured exactly that, with
**0 rows falling out**.

---

## 2. The 11 anonymous rows — 8 identified, 2 deliberately refused, 1 deferred

All 11 rows were first checked for the W16-BY phantom hazard: a dtk mis-carve produces a
row that is **indistinguishable from an unidentified function** and must never be named.
All 11 are exact `.pdata` BeginAddresses with exactly matching lengths, so **zero phantoms
/ zero mis-carves** — the hazard does not apply here. That check came first because naming
a phantom is unrecoverable by any later measurement.

**8 named** (commit `5ef98992`, appended to `scripts/target_symbol_map.json`,
29,510 -> 29,518 rows):

| address | name |
|---|---|
| `0x8267bf30` | `??0Game@@QAA@XZ` |
| `0x8267b808` | `?OnMsg@Game@@QAA?AVDataNode@@ABVButtonDownMsg@@@Z` |
| `0x8267aa48` | `?UpdatePausedState@Game@@QAAX_N00@Z` |
| `0x8267b670` | `?AddPlayer@Game@@QAAXPAVBandUser@@@Z` |
| `0x8267b000` | `?ResetVoiceChatState@Game@@QAAXXZ` |
| `0x8267a4c0` | `?OnMsg@Game@@QAA?AVDataNode@@ABVRemoteLeaderLeftMsg@@@Z` |
| `0x82678bb8` | `?SetRealtime@Game@@QAAX_N@Z` |
| `0x82679900` | `?OnMsg@Game@@QAA?AVDataNode@@ABVButtonUpMsg@@@Z` |

Identified by dispatch structure, **caller-set matching** (3-for-3, 2-for-2, 3-for-3 —
map-independent, and cannot be satisfied by a coincidence of size), callee sequence, and
argument count.

**Post-hoc corroboration, not the basis of the call:** the newly-named rows score
91.27 / 86.47 / 80.80 / 66.30 / 53.47 / 49.14 / 19.82 / 3.24. A wrong identification lands
near 0, because objdiff pairs by name and a misnamed row is compared against an unrelated
body.

**Economics, stated plainly:** naming an anonymous address has **no byte upside** —
`name_check` already *forgives* placeholder targets (`fn_`, `lbl_`, ...), so an unnamed
callee is uncharged. Naming converts a **forgiven** call site into a **checked** one:
right name = still 0, wrong name = a new charge at every caller. I predicted **Delta 0**
for the eight namings before building and measured exactly that. Naming pays in **pairing
and bug exposure, not bytes**, and that is the whole reason to do it.

**2 deliberately NOT named:**

- `fn_8267B4E8` (164 B) — its only caller is itself unnamed (`DropUser+0xe04`). Size alone
  is not evidence, and there is nothing else to triangulate on.
- `fn_8235F858` (308 B) — its sole caller is `??0Tour@@`. This is a **Tour body mis-homed
  into the Game unit by a splits pin**, exactly as the brief suspected. Re-homing an
  already-pinned address is **not** metric-neutral (measured elsewhere at +3 fns / +428 B),
  and `Tour` belongs to lane W16-BU. Naming it here would be filing someone else's row
  under my unit.

---

## 3. `Game::Poll` — the placeholder that was already named

`?Poll@Game@@QAAXXZ` (1,036 B) sat at fuzzy 99.9807 behind exactly one `diff_arg`.

The movie-sync branch called a placeholder:

```cpp
extern void fn_826C91C8(void *, float);
void *syncObj = *(void **)(*(char **)((char *)this + 0x48) + 0x14);
fn_826C91C8(syncObj, songMs);
```

`scripts/target_symbol_map.json` **already** maps `0x826c91c8` to
`?Poll@VocalGuidePitch@@QAAXM@Z`, and `VocalGuidePitch::Poll(float)` was **already**
declared at `src/band3/game/VocalGuidePitch.h:14` with an identical ABI. So this was not a
new identification — it was our source declining to spell a name the map already held, and
under `name_check` that declining is charged.

```cpp
VocalGuidePitch *syncObj = *(VocalGuidePitch **)(*(char **)((char *)this + 0x48) + 0x14);
syncObj->Poll(songMs);
```

Predicted **+1,036 B / +1 function**; measured exactly that, 0 fell out (commit `a0df0ae5`).

---

## 4. `Game::~Game` — one real defect, one proven fold, and what is left

`??1Game@@UAA@XZ` — target 792 B, ours 796 B (4 B larger), fuzzy 99.28283, 8 charges.
**It did not cross, and I want to be precise about why, because 6 of the 8 charges are
symptoms rather than causes.**

```
[diff_arg] TGT bl ??$?0H@?$StlNodeAlloc@V?$_List_node@H@...@Z  || BASE bl ??1Shuttle@@QAA@XZ
[diff_arg] TGT lis  r11, lbl_82077CFC@h       || BASE lis  r10, ??_7Interpolator@@6B@@h
[diff_arg] TGT addi r10, r30, 0xec            || BASE addi r11, r30, 0xec
[diff_arg] TGT addi r11, r11, lbl_82077CFC@l  || BASE addi r10, r10, ??_7Interpolator@@6B@@l
[insert  ] TGT (none)                         || BASE addi r11, r11, 0x14
[diff_arg] TGT stw  r10, 0x50(r31)            || BASE stw  r10, 0x100(r30)
[diff_arg] TGT stw  r11, 0x100(r30)           || BASE stw  r11, 0x50(r31)
[diff_arg] TGT stw  r11, 0xec(r30)            || BASE stw  r10, 0xec(r30)
```

### The interpolator cluster is ONE defect wearing seven charges

`Game::mInterpolator` is an `ATanInterpolator` at `0xec`; `ATanInterpolator` embeds
`LinearInterpolator mXMapping` at `+0x14`, so `0x100 == 0xec + 0x14` (compiler-verified
layout, not header comments). All three destructors in the chain are inline-empty and
virtual, so the successive vptr assignments collapse to the base's — which is why **both
sides write the same vtable to both `0xec` and `0x100`**, and why `lbl_82077CFC` is
`??_7Interpolator@@6B@`.

That matters for reading the charge list correctly: **the four `lis`/`addi`/`stw` charges
are register-argument differences (r10 vs r11), not relocation-name differences.** The
only substantive difference is that retail stores `&mInterpolator` (`r30+0xec`) into the
frame slot `0x50(r31)` while we store `&mInterpolator.mXMapping` (`r30+0x100`) — our
surplus `addi r11, r11, 0x14`. Those 4 bytes are the entire size delta, and they force the
r10/r11 renumber that accounts for six further charges.

`0x50(r31)` is the slot the unwind funclet reads (the same slot is used by the two vector
deallocations at indices 171 and 187). So retail records the **outer** member as the
in-flight subobject and we record the **nested** one — a difference in how MSVC assigns EH
states to an inlined empty-destructor chain.

**I found no source lever for it and did not spend a build guessing at one.** `Interp.h`
reaches only 6 TUs so a header experiment is cheap to run, but I could not name a change
that *ought* to move EH-state assignment: removing the explicit `virtual ~ATanInterpolator() {}`
still yields an implicitly-generated destructor with the same nesting. I am recording it
rather than reporting a speculative attempt as a result. Note `Tail` (`Tail.h:53`) carries
the same `ATanInterpolator` member, so this is not Game-specific and a lever found here
would pay twice.

### The remaining charge is a PROVEN fold, and it caps the row

TGT `??$?0H@?$StlNodeAlloc@...@Z` vs BASE `??1Shuttle@@QAA@XZ`, at `RELEASE(mShuttle)`.
Adjudicated the same way as section 1:

- The map puts the StlNodeAlloc copy-ctor at `0x826c3888`. **`0x826c3888` disassembles to
  a single `blr`** — 4 bytes, no relocations, the most-folded shape that exists.
- **The call site refutes the map's spelling**: `mr r3,r28 ; bl 0x826c3888 ; mr r3,r28 ;
  bl <operator delete>` is a one-argument destructor-then-delete. A two-argument copy-ctor
  cannot be it. (This is the MPNGAP-1 method that killed `Handle@GemPlayer`.)
- `scripts/symbol_aliases.json` **already carries the group** at `0x826c3888` (survivor =
  the StlNodeAlloc ctor, tier `FT-EMPTY`, 10 folded spellings). `??1Shuttle@@QAA@XZ` is
  simply not yet a member.

⇒ **our source is right and the charge is fold noise**, so even a perfect fix of the
interpolator defect leaves this row below `fuzzy == 100`. The realisable prize is `mpn`
(+1 function), not the 792 bytes — which is exactly the RESIDUAL-1 hazard: a row's headline
prize can be uncollectable by source work in principle.

### Why the alias is not installed in this lane

The `FT-EMPTY` tier compares **our compiled COMDAT** against the retail body. We had none:
`Shuttle::~Shuttle()` was **declared in `Shuttle.h` and defined nowhere in the tree** — an
unresolved external (the match build only compiles, so nothing complained). A prior lane
restored `Shuttle::Shuttle()` from retail bytes and left the destructor behind.

This lane **defines it** — `Shuttle::~Shuttle() {}` — on two grounds: retail's body at the
fold survivor is an empty `blr`, and the class is four PODs with no owned resources, so an
empty body is the only one consistent with both. That closes a real gap and makes the
evidence producible. Installing the membership through `tools/fold_thunk_gate.py` is the
correct next step and is deliberately left to be done with the gate re-run rather than by
hand-editing the group — an alias is *forgiveness*, so an unproven one lifts the score by
construction and is an integrity hazard, not a win.

---

## 5. `Game::LoadSong` — the oracle was wrong and retail was right

`?LoadSong@Game@@QAAXXZ` — 408 B, **54.49020 -> 100.0**. Three divergences, all decided
against an oracle that disagreed.

### 5.1 The oracle was wrong and retail was right

`../rb3` (rb3-Wii dev decomp) carries the exact 2-way ternary our source inherited, and the
exact `SetPracticeMode` line. Retail Xbox TU5 has neither. This is the standing rule doing
real work: **retail bytes outrank the oracle**, and the oracle being *our own source's
provenance* is not a reason to trust it over the target.

Retail computes a **three-way** `SongDataValidate`. Our own codegen supplied the numbering
before I read any header: the old base emitted `li r8,0x2` then `and r26,r11,r8`, so
`kSongData_Validate == 2` and `kSongData_NoValidation == 0` — meaning retail's `li r27,0x1`
path is a **third value we never emitted**. `SongData.h:35` confirms
`{ NoValidation=0, ValidateUsingNameOnly=1, Validate=2 }`.

The middle predicate is `TheContentMgr.IsCorrupt(Symbol(TheSongMgr.ContentName(songSym, true)))`.
That it is gated on the **content name** is what `ValidateUsingNameOnly` means — the
corroboration I wanted before believing a predicate reconstructed from a bare vtable slot.

Both predicates are evaluated **unconditionally**: retail issues the `IsCorrupt` call
*before* the `IsOnDisc` result is ever tested, so it cannot be a short-circuiting ternary
chain. Two local bools, then a select.

### 5.2 `IsCorrupt` takes only the Symbol — settled on the CALLER, not the body

Our header had `virtual bool IsCorrupt(Symbol, const char *&)`, inherited from DC3.

`ContentMgr_Xbox.cpp` already recorded half the answer — *"r5 ... is never read or written
anywhere in the 140 bytes"* — and that lane kept the 3-arg signature anyway, **correctly**:
an unused reference parameter emits no code either, so **the body cannot distinguish a
2-arg function from a 3-arg one that ignores its third argument.**

The caller can. Scanning all of `.text` for `lwz rX,0x88(rY) ; mtctr ; bctrl` yields 22
slot-0x88 indirect calls, of which **exactly one** dispatches on `TheContentMgr`
(`0x82C71A14`): `0x82679da4`, inside `LoadSong`. It sets `r3` and `r4` and nothing else.
A caller must materialise every parameter even when the callee ignores it ⇒ **no third
parameter exists.**

That slot 0x88 really is `IsCorrupt` comes from the retail `XboxContentMgr` vtable at
`0x82089aa4`:

| off | symbol |
|---|---|
| 0x70 | `MountContent` |
| 0x74 | `IsMounted` |
| 0x78 | `DeleteContent` |
| 0x7c | `IsDeleteDone` |
| 0x80 | `GetLicenseBits` |
| 0x84 | *(folded with `TrackPanelDirBase::GetCrowdMeter`)* — `GetCreationDate`, both trivial `return 0` |
| 0x88 | **`IsCorrupt`** |
| 0x8c | `NotifyMounted` |

⇒ the slot assignment `ContentMgr.h` reasoned its way to is **CONFIRMED**, `IsCorrupt`
included.

> ⚠ **Instrument trap, recorded because it nearly produced a wrong answer.** My first read
> of this vtable was **off by one slot** — it put `IsCorrupt` at 0x8c and reported that the
> header was wrong — because the vtable *start* was located by walking backwards over
> words that "looked like `.text` pointers". Re-anchoring on `IsMounted` at 0x74, an offset
> retail had **already proven** via `SongMgr::IsSongMounted`, fixed it. **Anchor a vtable
> on a known offset, never on a guessed start**: the off-by-one version was internally
> consistent and would have justified inserting a phantom virtual.

rb3-Wii has no `IsCorrupt` at all, so nothing outside DC3 ever attested the `const char *&`;
the map's mangled name was hand-authored from the DC3 signature, i.e. **inherited
assumption restating itself, not independent evidence.** It is renamed in lockstep —
proving a name wrong does not by itself make renaming safe, but leaving it stale while our
emitted symbol changes would UNPAIR the row and pin it at 0% permanently.

### 5.3 Branchy, not branchless — and a register swap that dissolved

My first attempt used a nested ternary and reached **97.598%**: right structure, wrong
shape. `corrupt ? 1 : 0` compiles to a branchless bool normalise (`subic`/`subfe`); retail
pre-initialises the 0 case and branches twice (`li r27,0` precedes the `beq`). An
`if`/`else if` chain over a pre-initialised variable reproduces it exactly.

At 97.598% the residual **also** included an r26/r27 inversion between `cfgList` and the
validate value, charged three times. **It dissolved on its own once the control flow
matched.** Opening it as a regalloc problem would have been wasted effort — the standing
rule that a `REGISTER_SWAP` is a symptom, not a diagnosis, demonstrating itself.

### 5.4 The failed prediction, which is the most useful line here

I pre-registered **+408 B** for the first attempt and measured **+0**. The restructure was
correct and moved the row 54.49 -> 97.598, but `matched_code` is **all-or-nothing per row**:
a row below `fuzzy == 100` pays nothing. Pricing a structural fix as though partial credit
existed is the error. The bytes arrive only on the crossing — which they did, on the second
attempt, at exactly +408 B.

---

## 6. What I did NOT do

- **I did not name `fn_8235F858`** (the mis-homed Tour body) or `fn_8267B4E8`. Reasons in
  section 2. Both are live veins for another lane, not drained ones.
- **I did not chase `Reset@Game`.** Its charge is a plain r4/r6 register swap — regalloc,
  not a name issue, and the permuter is off by standing directive.
- **I did not adjudicate `OnSetShuttle`.** Its charge is TGT `?Enable@Metronome@@QAAX_N@Z`
  vs BASE `?SetActive@Shuttle@@QAAX_N@Z` — the same wrong-name-or-fold class as section 1,
  and it deserves the same retail-byte treatment rather than a guess. Recorded, unadjudicated.
- **I did not touch `??0GemTrainerLoopPanel@@`** (140 B @ 76.03) — the brief's section 4
  item, reached only if time allowed. It did not.
- **I did not add an alias for the `~Shuttle` fold.** See section 4: the evidence tier
  requires our compiled COMDAT, which did not exist until this lane defined the destructor.
  Installing the membership is the correct next step and is left with its evidence written
  down rather than half-applied.

---

## 7. Gate chain

Run as the last actions in the worktree, in the brief's order, on the tree these commits
produce.

| # | gate | result |
|---|---|---|
| 1 | full `./tools/ninja-locked` | **rc=0** |
| 2 | `scripts/verify_ruler_agreement.py --check` | **rc=0** — both objdiff-cli entry points resolve the same ruler |
| 3 | `scripts/verify_objs_patched.py --verify-manifest` | **rc=0** — 1,219 decomp + 3,105 target objects match, `tree_sha256=764fa00bf861b6eb` |
| 4 | `tools/icf_alias_finder.py --validate` | **rc=0** — PASS: 1,407 map-consistent, 249 tolerated, **0 contradicted**, 1,657 total |
| 5 | `tools/funclet_homing.py --validate` | **rc=0** — PASS: 25,052 HOMED / 1,226 ORPHAN / 1 MIS-PINNED / 42 UNPINNED-FUNCLET |
| 6 | `tools/native_build_gate.sh` | see line below |

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```


`skipped=0` as required — the 0-SKIP rule is the one with the track record, and it is
applied here rather than trusting `verdict=PASS` alone.

---

## 8. For the next lane

1. **Install the `~Shuttle` fold membership** into the `0x826c3888` group via
   `tools/fold_thunk_gate.py` (do not hand-edit the group). The COMDAT it needs now exists.
   This alone will not cross `??1Game@@` — see item 2 — but it removes the row's one
   relocation-name charge.
2. **`??1Game@@` needs an EH-state lever**, not a regalloc one: our surplus
   `addi r11, r11, 0x14` makes the unwind funclet record `&mInterpolator.mXMapping` where
   retail records `&mInterpolator`. `Tail` (`Tail.h:53`) has the same `ATanInterpolator`
   member, so a lever found here pays twice. `Interp.h` reaches only 6 TUs, so experiments
   are cheap.
3. **`OnSetShuttle`** carries TGT `?Enable@Metronome@@QAAX_N@Z` vs BASE
   `?SetActive@Shuttle@@QAAX_N@Z` — the section-1 class, unadjudicated. Decide it on retail
   bytes (disassemble the mapped address; check the call site's argument count), not on the
   metric, which cannot separate the two hypotheses.
4. **`fn_8235F858`** (308 B, sole caller `??0Tour@@`) is a Tour body mis-homed into the
   Game unit by a splits pin. Re-homing is **not** metric-neutral, so it belongs to whoever
   owns `Tour`.
5. **Where does retail set practice mode?** Section 5 removed the `SetPracticeMode` call
   from `LoadSong` on retail bytes without finding its real home. That is an open question,
   not a closed one.
