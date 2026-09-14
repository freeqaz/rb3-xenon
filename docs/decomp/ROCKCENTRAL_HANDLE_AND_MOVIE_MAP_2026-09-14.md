# RockCentral::Handle, the Movie::Init map error, and 0x825df840 — lane W15-E, 2026-09-14

Branch `w15-e`, worktree `~/tmp/wt-w15-e`, base main **`b9e32547`**.
Ruler **`name_check` (graded)**, read at runtime from `report.json`'s
`provenance.diff_config` — never assumed.

Baseline, this worktree's own full build, patch fixed point verified first
(`scripts/verify_objs_patched.py --verify-manifest` → `OK: 1205 decomp, 3083
target objects match`, `tree_sha256=f7e4d3087cb918a4`):

```
matched_functions   42,860      matched_code    3,898,632 B
matched_code_percent 38.050446  fuzzy           49.200085
masked_equal        22,997      total_code     10,245,956
```

⚠ **Correction I made to myself mid-lane, recorded because the wrong version
nearly shipped.** My first `git log` put main at `c90f107c`, where `w15-c` was
an unmerged branch, and I wrote this section warning that W15-C's four commits
were absent and that landing order mattered. **Main advanced to `b9e32547` —
the `w15-c` merge — in the minutes between that `git log` and
`setup_worktree.sh`.** Verified with `git merge-base --is-ancestor`: all four
of `674de886`, `cae9a76e`, `7d867293`, `3b879bbe` are ancestors of my base, and
`RockCentral.cpp` already carries W15-C's `SystemLocale()` repairs. So this
lane's baseline **includes** W15-C's +6 fns / +4,252 B, there is no
merge-ordering concern, and `docs/decomp/GAME_CROSSING_GRIND_2026-09-14.md` is
present in-tree.

⇒ **A base SHA read before the worktree exists is not the worktree's base.**
Read it from the worktree (`git -C <wt> rev-parse HEAD`) after creation; on a
shared tree with concurrent lanes, main moves under you.

---

## 0. Headline

**+11 matched functions / +4,256 B / +0.041538 pp**, in three separately
measured, separately committed changes. Final state
**42,871 / 3,902,888 B / 38.091984%**.

| # | change | predicted | measured |
|---|---|---|---|
| 2 | `0x827c9110` is `TickToMs`, not `Movie::Init` (+ define the missing `TickToMs`) | +4 fns / +2,136 B | **+9 fns / +4,000 B** |
| 3 | `0x825df840` is `OvershellSlot::RemoveUser`, not a `_List_base` clear (+ drop two oracle-only statements) | +2 fns / +256 B | **+2 fns / +256 B** |
| 1 | `RockCentral::ForceLogout` tests `mState == 2` first | +0 / +0 | **+0 / +0** |

Three things worth carrying past this lane:

- ★★ **A wrong map name scored a FALSE 100%, and the ruler is structurally
  incapable of catching it.** `?Init@Movie@@SAXXZ` at `0x827c9110` read
  `fuzzy = mpn = 100.0`. Our `Movie::Init` is not that function — it merely
  compiles to the same six instructions, because `extern MovieSys &TheMovieSys`
  is a **reference** (a pointer variable), so a global-reference + virtual-slot-1
  tail call is byte-identical to a global-pointer + virtual-slot-1 tail call.
  The only difference is the **data relocation**, and `name_check` **forgives**
  it because retail's is a placeholder `lbl_`. Extend the standing rule: *never
  read a 100% row as evidence that a callee is right* — **it is not evidence
  that a DATA global is right either.**
- ★★ **A declared-but-never-defined function is a live defect class, not a
  one-off.** `float TickToMs(float)` was declared in `TimeConversion.h` and
  defined **nowhere in the tree**; every `TickToMs()` call site in `src/`
  referenced an undefined symbol. It survives only because the match build never
  links. This is the second instance in two lanes (W15-C found
  `PlayableBy__9VocalNoteCFi`). `TickToSeconds(float)` and `BeatToTick(float)`
  are still in that state — see §4.
- ⚠ **The bare-vs-nested `splits.txt` heading trap has now broken a FIFTH
  consecutive lane's scan — mine.** See §2c.

---

## 1. Item 2 — `0x827c9110` is `?TickToMs@@YAMM@Z` (commit `4d4c3c2f`)

### 1a. The body, and why it is ambiguous by construction

```
lis  r11, lbl_82C78F5C@ha
lwz  r3,  lbl_82C78F5C@l(r11)   ; r3 = *<global POINTER variable>
lwz  r11, 0x0(r3)               ; vptr
lwz  r11, 0x4(r11)              ; vtable slot 0x4
mtctr r11
bctr                            ; tail call; fp1 passes straight through
```

24 B / 6 instructions. `bctr` leaves `fp1` untouched, so a float argument passes
through and a float return comes straight back — the shape of a one-line
forwarding wrapper.

**Both candidate names produce exactly these bytes.** `Movie::Init(){
TheMovieSys.Init(); }` over a `MovieSys&` reference, and `TickToMs(f){ return
TheTempoMap->TickToTime(f); }` over a `TempoMap*`, differ only in which global
the relocation names. Under `name_check` a placeholder target (`lbl_…`) is
forgiven, so the row scored **100.0** under the wrong name. **No amount of
match-% work could ever have settled this.**

### 1b. Adjudication on retail bytes

Four independent lines, all pointing the same way:

1. **`fn_827C9128` identifies the global.** It loads **both** `lbl_82C78F5C`
   and `lbl_82C78FA0`, calls `fn_827D2878` on the latter, then the former's
   `vt[0x4]`. That is our
   `BeatToMs(b) = TheTempoMap->TickToTime(TheBeatMap->BeatToTick(b))`
   exactly ⇒ **`lbl_82C78F5C` = TheTempoMap**, `lbl_82C78FA0` = TheBeatMap,
   and **TempoMap `vt[0x4]` = `TickToTime`**. (`TempoMap.h` agrees: with a
   virtual dtor at slot 0, `TickToTime` is slot 1 = `0x4` and `TimeToTick`
   slot 2 = `0x8`.)
2. **A users-census of the two globals.** `lbl_82C78F5C` is referenced from
   `TempoMap.s`, `Task.s`, `SongInfoCopy.s`, `GemPlayer.s`, `VocalPlayer.s`
   — and from **no Movie unit at all**. `lbl_82C78FA0` is referenced from
   `BeatMap.s`. Tempo/beat code, not movie code.
3. **`fn_827C90B8` uses the SAME global at `vt[0x8]`** and is already named
   `?MsToTick@@YAMM@Z` at 100%, with an in-source note from a prior lane
   identifying it as `TheTempoMap->TimeToTick`. One global cannot be both
   `TheTempoMap` and `TheMovieSys`.
4. **The whole cluster is `TimeConversion.cpp`, in `TimeConversion.h`'s
   declaration order:**

   | retail | body | source |
   |---|---|---|
   | `fn_827C90B8` | TempoMap `vt[0x8]`, tail | `MsToTick` |
   | `fn_827C90D0` | `vt[0x8]` then BeatMap | `MsToBeat` |
   | **`fn_827C9110`** | **TempoMap `vt[0x4]`, tail** | **`TickToMs`** |
   | `fn_827C9128` | BeatMap `BeatToTick` then `vt[0x4]` | `BeatToMs` |
   | `fn_827C9190` | BeatMap, tail | `TickToBeat` |
   | `fn_827C91C8` | `×1000` then `vt[0x8]` then BeatMap | `SecondsToBeat` |
   | `fn_827C9218` | `vt[0x4]` then `×0.001` | `TickToSeconds` |
   | `fn_827C9258` | `bl fn_827C9128` then `×0.001` | `BeatToSeconds` |

### 1c. Why the repair had to be COUPLED

A rename alone would have left the row **unpairable** — the hazard CLAUDE.md
warns about ("proving a name wrong ≠ renaming is SAFE: the base obj may not
define it ⇒ permanently 0%"). Here it is avoided rather than risked:
`src/system/utl/StringTable.cpp:130` already scatter-includes
`utl/TimeConversion.cpp` into `default/StringTable`, **the same unit that owns
`0x827c9110`** — so defining `TickToMs` there makes the renamed row pairable in
the one place it can be. It compiled to the retail thunk exactly: **24 B,
fuzzy = mpn = 100.0**.

### 1d. Pricing before editing

Of **29** `bl fn_827C9110` sites in 15 callers, every *named* caller was
**already charged** and **none paired correctly** against `?Init@Movie@@SAXXZ`
— so the caller side carried **no un-pairing risk** whatsoever. Nine named rows
carried 18 charged sites; four of them had it as their **only** charge.

| row | size | sites | outcome |
|---|---:|---:|---|
| `?OnGemEnd@SongParser@@` | 1,008 | 1 | **crossed** |
| `?Rollback@GemPlayer@@` | 756 | 3 | **crossed** |
| `?Poll@Metronome@@` | 240 | 1 | **crossed** |
| `?GetSectionBounds@GameConfig@@` | 132 | 2 | **crossed** |
| `?UpdateScrolling@VocalTrack@@` | 8,948 | 6 of 1,378 | no |
| `?RebuildHUD@VocalTrack@@` | 2,188 | 1 of 139 | no |
| `?DrawTrackElements@GemTrack@@` | 1,432 | 2 of 30 | no |
| `?StartVocalNote@SongParser@@` | 1,104 | 1 of 16 | no |
| `?UpdateArpeggios@GemManager@@` | 444 | 1 of 2 (other is a proven fold) | no |

### 1e. Result

```
predicted  +4 fns / +2,136 B / +0.020847 pp
measured   +9 fns / +4,000 B / +0.039039 pp
```

Per-unit: `band3/game/Game` **+3**, `GameConfig` +1, `GemPlayer` +1,
`Metronome` +1, `SongParser` +1, `band3/game/GemTrainerPanel` +1,
`band3/game/RGTrainerPanel` +1. Units at 100% unchanged (165/137).
Δfuzzy +0.000049 pp. The `none` control read +0 B against +4,000 B on the
graded ruler; with `source` in the patch that shape is the
**wrong-callee-fix** signature, not the alias-suspect one, and `ab_measure`
correctly declined to adjudicate it as an alias.

---

## 2. ⚠ Why I underpredicted — a FIFTH lane lost to bare-vs-nested headings

My call-site census globbed **`build/45410914/asm/*.s`**. That matches only
**bare** `splits.txt` headings. The real population is recursive:

| scan | sites | units |
|---|---:|---|
| `asm/*.s` (what I ran) | 29 | 9 |
| `asm/**/*.s` (correct) | **41** | **11** |

The three missing units were `band3/game/Game.s`,
`band3/game/GemTrainerPanel.s`, `band3/game/RGTrainerPanel.s` — **nested**
headings — and they supplied exactly the unpredicted **+3 / +1 / +1**.

CLAUDE.md records this trap breaking four consecutive lanes and prescribes
"key on FULL PATH, never `basename()`". Mine is the mirror image of that
failure and the fifth instance: **a one-level glob silently sees only the bare
population.** Here the error was conservative and surfaced as a pleasant
surprise — but **a regression in those three units would have been concealed
exactly as effectively.** Use `grep -r` on the directory, or
`glob('…/asm/**/*.s', recursive=True)`.

(Incidentally `find … | xargs grep -l` returned nothing where `grep -rl` on the
directory returned 11 files; prefer `grep -r`.)

---

## 3. Item 3 — `0x825df840` is `?RemoveUser@OvershellSlot@@QAAXXZ` (commit `9fdd3641`)

W15-C proved the map name `?clear@?$_List_base@PAVPassiveMessage@@…` wrong and
**declined to rename**, on the correct principle that proving a name wrong is
not knowing the right one. Its stated blocker was *"our RemoveUser body does not
match it"*. **That blocker was itself the defect.**

Retail body (0x58 B, 22 instructions, **straight-line, no branch anywhere**):

```
r4 = this->[0x40] ; r3 = this->[0x38] ; bl fn_82682B60
r3 = ret->vptr[0x1c]() ; r4 = ret
r3 = this->[0x3c] ; bl fn_82587890
r3 = this           ; bl fn_825DAF18
```

Positive identification, from the compiler and the map — not the oracle:

- `fn_825DAF18` = `?ResetSlotCamera@OvershellSlot@@QAAXXZ`, called with
  `mr r3, r31` where r31 is the **incoming `this`** ⇒ **`this` is an
  OvershellSlot.** This is the one fact shape-matching cannot give.
- `class_layout_report.py` (i.e. `cl.exe /d1reportSingleClassLayout`, the
  compiler — not the header comments) puts **`mBandUserMgr` at 0x38,
  `mSessionMgr` at 0x3c, `mSlotNum` at 0x40** — exactly the three members the
  body reads, in exactly those roles.
- `fn_82682B60` = `BandUserMgr::GetUserFromSlot(int) const`;
  `fn_82587890` = `SessionMgr::RemoveLocalUser(LocalBandUser*)`. So the body
  reads *"get the user in this slot, take its LocalBandUser through vtable
  `+0x1c`, remove it from the session, reset the slot camera"*.
- Its three named callers are `AttemptRemoveUser`, `Handle@OvershellSlot` and
  `OvershellPanel::RemoveUsersRequiringSongOptions` — three independent
  remove-a-user paths.
- It is 0x58 B and sits exactly 0x58 **before** `AttemptRemoveUser`, matching
  our own source adjacency.

### 3a. The body defect — a real divergence, stated as such

Our `RemoveUser` carried two statements retail X360 does not have:

```cpp
if (TheSaveLoadMgr) TheSaveLoadMgr->AutoSaveNow();      // removed
TheWiiProfileMgr.RemovePad(pLocUser->GetPadNum());       // removed
```

The first **cannot** be present: it would emit a branch, and the retail body has
none. The second is `TheWiiProfileMgr` — **Wii-only**, the
Wii-oracle-correct / Xbox-wrong class already named in `RockCentral.cpp`'s own
handler comment. Both came from the rb3-Wii **dev** oracle. Retail bytes
outrank the oracle.

**Behaviourally**, removing a user on Xbox 360 retail does *not* force an
autosave and does *not* touch a Wii profile pad table; our code did both.

### 3b. Pricing and result

The row was **88 B at fuzzy 40.41 / mpn 42.00** under the wrong name — i.e.
contributing **zero** to `matched_code` and **zero** to `matched_functions`
— so the rename could not cost anything on the defining row.

```
predicted  +2 fns / +256 B / +0.002499 pp
measured   +2 fns / +256 B / +0.002499 pp
```

All four pre-registered row outcomes hit exactly:

| row | size | before → after |
|---|---:|---|
| `?RemoveUser@OvershellSlot@@` | 88 | 40.41 → **100.00** |
| `?RemoveUsersRequiringSongOptions@OvershellPanel@@` | 168 | 99.88 → **100.00** (sole charge) |
| `?AttemptRemoveUser@OvershellSlot@@` | 300 | 14.84 → 14.91, no cross |
| `?Handle@OvershellSlot@@` | 9,276 | 99.9957 → 99.9978, no cross — the remaining charge is W15-C's proven ICF fold, which charges **both** rulers |

★ **That `RemoveUser` reached exactly 100.00 is the identification's own
strongest witness**: a wrongly-named function does not match our body
byte-for-byte. This is the clean way out of the "proven wrong, replacement
unknown" stalemate — *repair the body you believe belongs there and let the
byte match adjudicate.*

---

## 4. Item 1 — `?Handle@RockCentral@@`: 28 of 30 charges are SCHEDULING

W15-C ranked this the best remaining pure-source candidate in the game units
(1,296 B, **zero** register and **zero** symbol charges). Decomposing all 30 on
retail bytes:

| charges | class | reachable from source? |
|---:|---|---|
| 22 | two twin clusters (idx 53–63, 149–159) | **no** — see below |
| 6 | one cluster (idx 86–96) | **no** |
| 2 | `cmpwi` constant order (idx 239/241) | **yes — fixed, commit `b29d778f`** |

### 4a. The 28 are a reordering, not a difference

Both sides contain the **identical 8-instruction multiset** in each twin
cluster. Retail's canonical order for a `HANDLE_MESSAGE` block is: build the
message temporary (`mData` at `0x5c`, vptr at `0x58`, refcount `sth` into the
DataArray at `+0xa`), **then** the three sret-call argument setups
(`subi r4,r27,0xcc` / `addi r5,r31,0x58` / `addi r3,r31,<slot>`). Ours hoists
those three *into* the construction. Same instructions, different schedule.

**No charge names a source construct**, so this fails the SOURCE_INSDEL
charge-screen in CLAUDE.md. And because `matched_code` is all-or-nothing per
row, closing the 2 fixable charges buys **exactly zero bytes** — measured.

### 4b. ★ ESCALATION: a named, testable mechanism for the 28

Three of six `HANDLE_MESSAGE` blocks diverge, and the split is **6/6
correlated with whether our `OnMsg` overload is a stub**:

| block | our `OnMsg` | retail callee | Handle block |
|---|---|---|---|
| #1 `ServerStatusChangedMsg` | real, substantial | named, paired | **matches** |
| #4 `ConnectionStatusChangedMsg` | real | named, paired | **matches** |
| #6 `RockCentralOpCompleteMsg` | real, substantial | `fn_824FA138` | **matches** |
| #2 `UserLoginMsg` | **stub** `UpdateOnlineStatus(); return 1;` | `fn_824F7C98` | **diverges** |
| #3 `FriendsListChangedMsg` | **stub** `return 1;` | `fn_824F7C98` | **diverges** |
| #5 `ProfileChangedMsg` | **stub** `return 1;` | `fn_824F7D48` | **diverges** |

Under a null of random assignment of 3 stubs to 6 slots, a perfect split has
probability 1/20 = 5%. **Suggestive, not proven** — but it is a mechanism a
follow-up can *test*, which "scheduling, unfixable" is not.

### 4c. ⛔ The in-source note above those three handlers is FALSE

`src/band3/net_band/RockCentral.cpp:256` says of the UserLogin /
FriendsListChanged / ProfileChanged handlers:

> *"None are in the pinned retail-Xbox range; stubbed for compilation."*

They are all three **in** the pinned range — `fn_824F7C98` and `fn_824F7D48`
are both defined in `build/45410914/asm/RockCentral.s`, and neither is a stub:
`fn_824F7C98` allocates a 0x90 frame, saves r26–r31, and makes four calls
including a vtable dispatch. Retail calls `fn_824F7C98` for **both**
`UserLoginMsg` and `FriendsListChangedMsg` — one ICF-folded body, so in retail
those two handlers are **byte-identical including relocations**, i.e. the same
source text. `ProfileChangedMsg` is a third, different body.

⇒ Our three stubs are wrong, **and so is our grouping**: we pair
FriendsListChanged with ProfileChanged (both `return 1;`), retail pairs
UserLogin with FriendsListChanged. **The retail bodies are available and
pinned.** This is a body-port job, not a crossing job — handed on.

---

## 5. Still-open defects recorded, not repaired

- **`float TickToSeconds(float)` and `float BeatToTick(float)` are declared in
  `utl/TimeConversion.h` and defined nowhere in the tree.** Same class as
  `TickToMs`. Deliberately not touched: no map row needs them today, so each is
  an unpriced risk with no measurable upside. Retail has them at `fn_827C9218`
  (`TickToMs(t) × 0.001`) and — for `BeatToTick` — somewhere in the same
  cluster.
- **`?AttemptRemoveUser@OvershellSlot@@` sits at fuzzy 14.91 / 300 B.** Now that
  `RemoveUser` is proven and matching, this is the obvious next row in that TU;
  its body is the Wii-heavy one (`TheWiiProfileMgr` loops) and is very likely
  the same oracle-contamination class.
- **`?Handle@OvershellSlot@@` (9,276 B) remains uncollectable**, confirmed
  independently here: after the `RemoveUser` repair it reads 99.9978 and its
  remaining charge is W15-C's proven ICF fold.
- **`?CountOrCreateExpandedDetails@NextSongPanel@@`** (12,220 B, one
  ARITH_COMMUTE charge) and **`?RecordAccomplishmentData@RockCentral@@`**
  (4,676 B, all-register) are unchanged from W15-C's escalation list.

## 6. What I did NOT do

- Did not run the permuter (standing directive); §4a's 28 charges are exactly
  permuter-shaped.
- Did not port the three retail `OnMsg` bodies (§4c) — a body-port of two
  substantial functions with unnamed callees, well beyond a crossing lane.
- Did not touch `TickToSeconds` / `BeatToTick` (§5), and did not attempt
  `AttemptRemoveUser`.
- Did not rebase onto or merge `w15-c`; this branch is based on `main`
  (`b9e32547`) exactly as briefed.
