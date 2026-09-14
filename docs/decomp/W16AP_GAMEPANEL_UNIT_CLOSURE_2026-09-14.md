# W16-AP — `default/GamePanel` unit closure (band3/game)

**Lane:** W16-AP · **Branch:** `w16-ap` · **Worktree:** `~/tmp/wt-w16-ap` · **Base:** main `424107a6`
**Date:** 2026-09-14 · **Ruler:** `name_check` (read from `report.json` `provenance.diff_config`, not assumed)
**objdiff:** 4.2.9 `5a51cd51fe0a353f` · **`total_code`:** 10,246,004 (read from the key, never hardcoded)

## Result

| measure | before | after | Δ |
|---|---:|---:|---:|
| `matched_functions` (lane-internal) | 43,428 | **43,433** | **+5** |
| `matched_code` (lane-internal) | 4,007,884 | **4,008,468** | **+584 B** |
| `matched_code_percent` | 39.116558 | 39.122257 | +0.005699 |
| `fuzzy_match_percent` | 49.621574 | 49.643856 | +0.022282 |
| `default/GamePanel` functions | 91/97 | **93/97** | +2 |
| `default/GamePanel` bytes | 7,684 / 11,104 | **7,992 / 11,104** | +308 B |

**Rows that fell out: ZERO, at every step.** Priced by set-diff of the `fuzzy==100` row set
(`tools/rowset_snapshot.py diff`) against a COPY of the lane baseline (`~/tmp/rows_w16ap_base.json`,
copied from `~/tmp/rows_w16an_main.json`; the original was never modified).

```
CROSSED IN : 6 rows, 584 B
   +    196 B  default/GamePanel::?CreateGame@GamePanel@@QAAXXZ
   +    172 B  default/NetGameMsgs::?Dispatch@RestartGameMsg@@UAAXXZ
   +     72 B  default/GamePanel::?SendRestartGameNetMsg@GamePanel@@QAAXXZ
   +     64 B  default/band3/game/Game::?NewNetMessage@RestartGameMsg@@SAPAVNetMessage@@XZ
   +     40 B  default/NetGameMsgs::fn_82691C0C
   +     40 B  default/GamePanel::fn_82694924
FELL OUT   : 0 rows, 0 B
```

⚠ **Six rows crossed `fuzzy==100` but `matched_functions` rose only +5.** That is the documented
two-ruler divergence, not an accounting error: one of the six already had `mpn == 100` (an arg-only
penalty) and was therefore *already* counted as a matched function while its bytes were withheld.
I cannot say retroactively **which** one — the baseline snapshot stores only the `fuzzy==100` row set,
not the baseline `report.json`, and that file has since been overwritten by later builds. Noted rather
than smoothed over.

**Commits** (three, all on `w16-ap`, tip **`ba76a34d`**):

| sha | what |
|---|---|
| `618ad133` | map: name GamePanel's four anonymous rows |
| `9b48f068` | RestartGameMsg carries no payload in retail: drop `mFromWin`, `SendRestartGameNetMsg` takes no arg |
| `ba76a34d` | CreateGame: `disable_pause_ms` is a FUNCTION-LOCAL static, not the `Symbols2.h` extern |

---

## Item 1 — build first

Done before reading a single retail symbol name. This is not ceremony: a fresh worktree's reflinked
target objs are **pre-renamer**, so every mangled name reads "absent" until the first build runs
`obj_target_symbol_renamer`, and the resulting false negative *agrees with whatever you expected*.
Build log `~/tmp/rb3_build_w16ap_1.log`, rc=0.

---

## Item 2 — the four anonymous rows

All four were identified on retail bytes and named in `scripts/target_symbol_map.json`
(29,458 → 29,462 entries; the file round-trips byte-exact at
`json.dumps(d, indent=1, separators=(',',': '), ensure_ascii=False) + '\n'`).

Before naming anything I checked the **caller population**, because a name is a bet: `name_check`
*forgives* a placeholder target (`fn_`…), so naming converts a forgiven call site into a checked one,
and a wrong name is charged to every caller. All references to these four addresses are confined to
`GamePanel.s`, and the four affected matched callers already spell the same names. So the downside was
bounded before the bet was placed.

| address | name | evidence |
|---|---|---|
| `0x82694860` | `?CreateGame@GamePanel@@QAAXXZ` | `RELEASE`/`operator new`/`??0Game@@` call sequence + the `disable_pause_ms` string; unique among GamePanel members |
| `0x82694b00` | `?SendRestartGameNetMsg@GamePanel@@QAAXXZ` | constructs a `RestartGameMsg` on the stack and hands it to the net-msg sender; 72 B |
| `0x82695178` | `?UpdateNowBar@GamePanel@@QAAXXZ` | `TheSongDB` record walk + a 4-arg vtable-`0xd4` call into `TrackPanelDirBase` |
| `0x82696b48` | `?Handle@GamePanel@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | `HANDLE_ACTION` dispatch chain, message-symbol order matches the source's macro order exactly |

⚠ **Signature correction made mid-lane, on evidence.** `0x82694b00` was first named
`?SendRestartGameNetMsg@GamePanel@@QAAX_N@Z` (taking a `bool`), copying the oracle. Retail's call site
passes **no argument at all**, so the mangled name was wrong; corrected to `…QAAXXZ` in the same commit
series. See the RestartGameMsg section — the wrong signature and the phantom payload are one finding.

**Map-only commit `618ad133` measured +0 fns / +0 B, exactly as predicted.** Naming pays in *pairing
and bug exposure*, not bytes. It is what made the next two commits legible.

### `CreateGame` — the function-local static (W16-AN's pattern, reused)

**Predicted +1 fn / +196 B. Measured +2 fns / +236 B.** Prediction failure, in the useful direction,
and worth recording: the extra +40 B is the EH funclet `fn_82694924`, which crossed once the frame
layout agreed. I had priced the named row and forgotten its funclet.

Retail materialises a **function-local `static Symbol`** at this exact declaration point — guard word
`lbl_82E0260C` bit `0x1`, instance `lbl_82E02608`, string `lbl_820E2608` = `"disable_pause_ms"`, plus a
`??__F` atexit thunk — emitted immediately after the `mGame` store and before the `Property` call. The
rb3-Wii oracle uses the file-scope extern from `Symbols2.h:1146`, which is the wrong spelling for this
image. This is precisely W16-AN's finding reproduced in a second unit, which is what promotes it from
an anecdote to a pattern: **retail materialises function-local statics where the Wii dev source uses
file-scope externs.**

```cpp
    mGame = new Game();
    static Symbol disable_pause_ms("disable_pause_ms");
    mGame->mDisablePauseMs = Property(disable_pause_ms, true)->Float();
```

### `RestartGameMsg` carries no payload — four independent witnesses

The oracle has `RestartGameMsg { int mFromWin; }` with a `bool` constructor. **This image does not.**
Established on retail bytes, not by preference:

1. **The vtable.** `lbl_820DBFAC` slots 1 and 2 (`Save`, `Load`) both point at `fn_826C3888`, a bare
   `blr`. A message with a payload cannot have an empty `Save` — it would have nothing to serialise.
2. **`Dispatch` never touches `this`.** `fn_82691B40` is exactly one function-local `static DataArrayPtr`
   (`"game_restart"`, `lbl_820DCFE4`) behind a single guard bit, then `Execute`. No member load, no
   branch on a flag, and **no store to any global** — so the oracle's `ThePlatformMgr.SetIsRestarting(true)`
   is absent from this image whether inlined or not.
3. **The call site passes nothing.** `Handle`'s `send_restart_game_net_msg` action calls with no argument.
4. **The constructor is trivial** — no member initialised.

Source changed accordingly (`NetGameMsgs.h`/`.cpp`, `GamePanel.h`/`.cpp`): member deleted, `Save`/`Load`
emptied, `Dispatch` reduced to the single static, `SendRestartGameNetMsg()` de-parameterised.

**Predicted +3 fns / +244 B. Measured +4 fns / +348 B.** Under-counted again, and for a related reason:
removing the member cascaded into `?NewNetMessage@RestartGameMsg@@` (64 B, a *different unit* —
`default/band3/game/Game`) and an EH funclet `fn_82691C0C` (40 B). **Both of my prediction misses were
cascades into rows I had not listed, both in the same direction.** The lesson for the next lane: when a
change alters a type's layout, price the type's *whole* row family — funclets and factory functions
included — not just the function you edited.

---

## Item 3 — the two near-100 rows, priced from the charged-site list

Both were priced from `report.json`'s charged sites, **not** from a mismatch count. That distinction is
load-bearing here: both rows read *all instructions equal* and are held below 100 purely by
relocation-**name** (`diff_arg`) charges, which an instruction-level mismatch count cannot see.

objdiff labels both `AT_LIMIT` / `ICF (cross-function merge)`. Per the standing rule, that label on a
`diff_arg`-only row **carries no information** — it is the detector restating its own input, since
"target calls A, we call B, A≠B" is bit-for-bit the definition of a wrong callee *and* of a fold. So all
three charged sites in this lane were adjudicated on retail bytes. Two are genuine folds; **one is not**,
and that one is the most useful finding in this document.

### `?Handle@GamePanel@@` — 1,812 B @ 99.9890 — genuine fold

One charged site (idx 280), `base size == target size == 1812`.

Our `?PrintStats@HitTracker@@QBAXXZ` is a 4-byte `.text` COMDAT, bytes `4e800020` — a bare `blr` — because
its body is entirely `MILO_LOG`, and the whole `MILO_*` family is `#ifdef HX_NATIVE`, which the match
build never defines. Retail's survivor `0x826c3888` is the same bare `blr`.

⚠ **Byte identity between two empty bodies proves almost nothing** — every empty body is identical to
every other. The discriminating evidence is **positional**: the immediately following `HANDLE_ACTION`
loads the **same member** (`lwz r3, -0x10(r27)`, identical on both sides) and calls
`?Reset@HitTracker@@QAAXXZ` — **a name both sides already agree on**. That proves the pointer at
`-0x10(r27)` is a `HitTracker*`, so the charged call at idx 280 is a `HitTracker` method on that pointer,
at the source position of `HANDLE_ACTION(print_hit_stats, mHitTracker->PrintStats())`.

Existing alias group index 1537. **Proposed as a membership addition, not applied** — `symbol_aliases.json`
is W16-AL's file.

### `?IsLoaded@GamePanel@@UBA_NXZ` — 124 B @ 99.8387 — genuine two-level fold, proved programmatically

I verified this by program rather than by eye, because eyeballing hex is exactly how a vacuous comparison
slips through:

```
ours   len 96
retail len 96
IDENTICAL: True
```

Our `?IsLoaded@?$ObjDirPtr@VObjectDir@@@@QBA_NXZ` COMDAT is **96 bytes with ZERO relocations** and
byte-for-byte identical to retail `fn_82823340` (= `?IsLoaded@?$ObjDirPtr@VUILabelDir@@@@QBA_NXZ`).
**Zero relocations means nothing is masked**, so this is a non-vacuous T1 proof rather than flat T1's
usual understatement. `ObjDirPtr<T>::IsLoaded`'s body does not depend on `T`, so all instantiations fold.

The outer thunks then follow: our `?IsLoaded@DirectInstrument@@QAA_NXZ` is 8 bytes
(`addi r3,r3,4; b <reloc>`) against retail's `0x826ccb48` (`addi r3,r3,4; b fn_82823340`) — first word
identical, second the same instruction with a different displacement, so they fold **iff** their
destinations are the same function, which is what the 96-byte proof establishes.

**Control:** the map contains exactly **one** `ObjDirPtr<T>::IsLoaded` row in the whole binary, so
nothing in the image contradicts the fold.

### `??0GamePanel@@QAA@XZ` — 468 B @ 99.9573 — **NOT a fold. A mis-identification.**

One charged site: target `bl ??0Splash@@QAA@XZ` where we emit `bl ??0DirectInstrument@@QAA@XZ`.

This is the row where the `AT_LIMIT` label had to be refused:

- Retail's `??0Splash@@QAA@XZ` is **328 B**; our `??0DirectInstrument@@QAA@XZ` is **~420 B**. MSVC folds
  only *identical* COMDATs — **different sizes cannot fold.**
- Independently, `default/Splash`'s **own** row for `??0Splash@@QAA@XZ` reads **6.89 %**. Our Splash
  source does not produce that body either.

Two spellings that neither fold nor self-match is the signature of a **mis-identified address**, not of
ICF. **Decision: report, do not guess, and do not alias.** Adding an alias here would lift `name_check`
*by construction* and bury a real identification defect — the exact integrity hazard the alias mechanism
is warned about. See the mis-pin section below.

⚠ **Caveat I am flagging against my own evidence:** the ~420 B figure comes from our COMDAT-span reader,
which lane STLPORT-1 measured billing a successor symbol's EH funclet prefix into the span. The 92 B gap
is far larger than that artifact, and the 6.89 % self-match is an independent second witness, so the
conclusion does not rest on the size alone — but **re-derive the size before acting on it.**

---

## Mis-pin / mis-identification report (no splits moved)

**`0x826ccc10` is named `??0Splash@@QAA@XZ` in `scripts/target_symbol_map.json`, and the evidence says
that name is wrong or the address is mis-pinned.**

| witness | value |
|---|---|
| size of the body at `0x826ccc10` | 328 B |
| size of our `??0DirectInstrument@@QAA@XZ` | ~420 B (see caveat above) |
| `default/Splash`'s own row for `??0Splash@@QAA@XZ` | **fuzzy 6.890244** |
| `??0GamePanel@@QAA@XZ`'s single charged site | target `??0Splash`, ours `??0DirectInstrument` |

`??0GamePanel` constructs a `DirectInstrument` member; if `0x826ccc10` really were `Splash`'s constructor,
GamePanel's constructor would be calling the wrong class outright. The coherent reading is that
`0x826ccc10` is **`??0DirectInstrument@@QAA@XZ`** and the map's `Splash` name is mis-assigned — which
would simultaneously explain `default/Splash`'s 6.89 % row (scored against a body that is not its own).

**I did not change it.** Renaming a map row is not safe merely because the current name is proved wrong:
if the base obj does not define the new name, the row pairs with nothing and reads a permanent 0 %.
That verification needs the `Splash` unit's own COFF and belongs to a map/identification lane with both
units in scope. Filed with evidence, not acted on. **No `splits.txt` edit was made** (lane bar, and
nothing here calls for one).

---

## Alias proposals (for W16-AL)

`docs/decomp/W16AP_ALIAS_PROPOSALS_FOR_W16AL.json` — three memberships, **proposed, not applied**:

| id | group | add | tier | unlocks |
|---|---|---|---|---|
| W16AP-1 | `0x826c3888` | `?PrintStats@HitTracker@@QBAXXZ` | FT-EMPTY + positional | `Handle` — 1,812 B |
| W16AP-2 | `0x82823340` | `?IsLoaded@?$ObjDirPtr@VObjectDir@@@@QBA_NXZ` | T1 (0 relocations ⇒ non-vacuous) | prerequisite for W16AP-3 |
| W16AP-3 | `0x826ccb48` | `?IsLoaded@DirectInstrument@@QAA_NXZ` | T1, conditional on W16AP-2 | `GamePanel::IsLoaded` — 124 B |

W16AP-2 and W16AP-3 must be applied **together or neither** — the thunk fold is contingent on the
destination fold. The JSON also records, explicitly, the `??0Splash` membership that must **not** be
created and why.

If all three land, they close `default/GamePanel` to **95/97** and **+1,936 B**, leaving only
`UpdateNowBar`.

---

## NOT done, with reasons

**`?UpdateNowBar@GamePanel@@QAAXXZ` — 628 B @ 5.9299. Deferred, with evidence.**

This is the one row I chose not to attempt, and the reasoning is specific rather than a shrug. Retail's
body is **RB3-360-specific: there is no oracle to port from.** The rb3-Wii oracle's `UpdateNowBar` writes
to an `mTime` overlay and shares essentially no structure with retail's body, which walks `TheSongDB`'s
record vector at `+0x20` (0x24-byte records) and ends in a **4-argument** vtable-`0xd4` call.

Closing it requires all three of:

1. Widening `TrackPanelDirBase::Unkd4()` (`src/system/bandobj/TrackPanelDirBase.h:73`, currently
   `virtual void Unkd4() {}` — **no parameters**) to 4 parameters, in a **shared `bandobj` header** whose
   blast radius across other units is unmeasured.
2. Identifying `fn_827C91A0` and `lbl_82C71838` — both unnamed in the map.
3. Modelling `TheSongDB`'s 0x24-byte record vector at `+0x20`.

Because `matched_code` is **all-or-nothing per row**, partial progress on any subset of these buys
**exactly zero bytes**. That is what makes deferral the correct call rather than a budget excuse: there
is no partial credit to bank, and a speculative header widening could regress other units.

⚠ **I also refuted an in-tree comment while investigating this row, and did not correct it.**
`src/band3/game/GamePanel.cpp` (~lines 464-484, the `#else` branch) carries a stub
`GetTrackPanelDir()->Unkd4();` whose comment claims fn_82695178's formatting "belongs to a different
symbol". **That is wrong on retail bytes** — the formatting is inside `fn_82695178`, which ends with the
4-argument vtable-`0xd4` call. I left the comment in place because correcting it without closing the row
would be a cosmetic edit to a file whose real fix is deferred; flagging it here instead so the next lane
does not trust it.

**Also not done, deliberately:**

- **Stretch item 5 (`PerfectOverdriveTracker`)** — not attempted. GamePanel did not drain early: three of
  its four remaining rows are live proposals in another lane's file, and the budget went to proving those
  three adjudications rather than opening a new unit. An unproven alias is worse than no alias.
- **Item 6 (`GuitarController` / `DataArraySongInfo`)** — not reached; the precondition ("GamePanel proves
  drained early") never held.
- **`scripts/symbol_aliases.json`, `icf_alias_*`, `config/45410914/splits.txt`** — untouched (lane bars).
  Alias work filed as a JSON proposal.
- **`src/network/net/Server.h` `GetPlayerID`** — untouched, per the standing bar.
- **W16-AO's `list<T>` map rows** — untouched.
- **No rebase** onto main `73f16555` (W16-AL's landing), per the coordinator's instruction. Numbers above
  are lane-internal against the snapshot this lane started with. Nothing W16-AL changed touches GamePanel.

---

## Gates

Run in order, in the worktree, native gate last (this lane touches `src/band3/**`).

| gate | result |
|---|---|
| full build | rc=0 (`~/tmp/rb3_build_w16ap_5.log`) |
| `scripts/verify_ruler_agreement.py --check` | PASS |
| `scripts/verify_objs_patched.py --verify-manifest` | PASS |
| `tools/native_build_gate.sh` | see `NATIVE_GATE_RESULT` below |

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

`skipped=0` as required. All four gates green, native last.

---

## Concurrency check (map rows), and a false alarm worth recording

The coordinator advanced main twice during this lane (W16-AL → `73f16555`, then W16-AO → `77db3f09`).
Per instruction I did **not** rebase; every number in this document is lane-internal against the
snapshot this lane started from (`424107a6`), and the coordinator reconciles at landing.

Verifying that my map edits do not collide with W16-AO's reserved addresses, I first ran
`git diff main..w16-ap` — which reported that this lane had touched **ten of AO's reserved rows.**
That was a **false alarm manufactured by the comparison itself**: `main` had moved forward to include
AO's work, so rows present in main and absent from my older base render as deletions attributable to me.
Against the true merge-base the picture is clean:

```
merge-base = 424107a6
rows this lane touched : 0x825f5c60  0x82694860  0x82694b00  0x82695178  0x82696b48
overlap with AO reserved: NONE
```

(`0x825f5c60`'s *value* is unchanged — it gained a trailing comma because entries were appended after it.)

⚠ **Generalisable trap:** in a fleet where the coordinator lands other lanes mid-flight, `main..branch`
silently stops meaning "what this lane changed" the moment main moves, and the resulting diff is shaped
exactly like a concurrency violation. **Diff against `git merge-base`, never against `main`, when
auditing what a lane touched.** Five files changed in total:
`scripts/target_symbol_map.json`, `src/band3/game/{GamePanel.cpp,GamePanel.h,NetGameMsgs.cpp,NetGameMsgs.h}`.
