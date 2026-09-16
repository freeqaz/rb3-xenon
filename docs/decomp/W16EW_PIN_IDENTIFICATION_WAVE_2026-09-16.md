# W16-EW — three addresses, three diseases, and one lesson about invisible rows

Lane `w16-ew`, worktree `~/tmp/wt-w16-ew`, branch off main `2d9c4ead`.
Measured result: **+10 matched functions / +1,800 matched code bytes**, across
seven A/B runs, every one pre-registered before measurement.

| task | change | Δfns | Δbytes | predicted? |
|---|---|---:|---:|---|
| 1 | port missing TU `QuazalSession.cpp`, re-home ctor, name 3 addresses | +3 | +92 | yes, exactly |
| 2 | carve `Join`/`AssignLocalOwner` out of MeshAnim into NetSession, name them | +0 | +0 | yes, exactly |
| 2b | `NetSession::Join` tail-merge direction (early returns) | +1 | +544 | discovered work |
| 3 | name `OnMsg(JoinResponseMsg)` + `IsLocal` | +0 | +0 | yes, exactly |
| 3b | `IsLocal` enumerated bool, not a range trick | +1 | +208 | yes, exactly |
| 3c | `OnMsg` drop `HasOnlinePrivilege` loop | +4 | +200 | **no** — predicted +1/+756 |
| 3d | `OnMsg` drop `JoinVoiceChannel` call | +1 | +756 | yes, exactly |

Leg A of the Task-3 run independently reproduced the briefed baseline: it read
43,982 fns / 4,132,292 B, and subtracting this lane's own committed work
(+4 / +636 at that point) gives exactly the briefed main@`2d9c4ead` figures of
43,978 / 4,131,656. **Deltas compose; the brief's Task-3 numbers were sound.**

## The central finding: an unpaired row is indistinguishable from a correct one

The brief framed Task 3 as "pure identification" worth roughly nothing in bytes,
and the A/B agreed: naming `0x823e4b68` and `0x823e24d8` measured **+0 / +0**.
Read as a number, that is a null result.

It was the most valuable thing this lane did. Those two names are what made the
next three commits possible:

```
OnMsg(const JoinResponseMsg&), 756 B
  unnamed / unpaired   fuzzy 0          <- invisible to every instrument we own
  named                fuzzy 79.005295  <- Task 3,  +0 bytes
  loop removed         fuzzy 97.962960  <- Task 3c, +4 fns / +200 B
  call removed         fuzzy 100.0      <- Task 3d, +1 fn  / +756 B

IsLocal() const, 208 B
  unnamed / unpaired   fuzzy 0
  named                fuzzy 79.230770  <- Task 3,  +0 bytes
  bool test fixed      fuzzy 100.0      <- Task 3b, +1 fn  / +208 B
```

Both rows contained **real source defects the whole time** — two of them in
`OnMsg` alone. Neither was findable at fuzzy 0, because objdiff pairs by name:
with no name there is no counterpart, no diff, no charged-site list, no
candidacy. A row at 0% because it is unpaired looks exactly like a row at 0%
because our source is absent, and nothing distinguishes either from a row with
nothing wrong.

**Therefore a naming A/B systematically understates its own value, and the
understatement is not small.** Task 3 measured +0/+0 and directly produced
+6 functions / +1,164 bytes in the three commits that followed it. Pricing
identification work on its own delta is the wrong instrument; the payout is
realised later, in commits attributed to something else.

## Three FretHand-shaped negatives — the kill-check earning its keep

The brief's standing order is to ask, before anything else, whether a compiled
object actually *defines* the symbol, by reading COFF symbol tables rather than
expectations. Parsing section numbers (0 = UNDEF) killed three candidates:

| symbol | verdict |
|---|---|
| `??0MakeQuazalSessionJob@@QAA@PAPAVQuazalSession@@_N@Z` | UNDEF everywhere **before** Task 1 |
| `?New@SessionData@@SAPAV1@XZ` | UNDEF in `NetSearchResult.obj`, `NetSession.obj` |
| `??1QuazalSession@@QAA@XZ` | UNDEF in `NetSession.obj` |

The last two are unfixable by any pin move or map name and the correct action
was to do nothing. The first one is the interesting case, because the brief was
wrong about it in a way worth recording.

## Refuting the brief: Task 1 was not a homing problem

The brief said `0x823F2C98` was "correctly named but MIS-HOMED", worth 60 B, fix
it in `splits.txt`. The kill-check said the symbol was **UNDEF in the only object
referencing it** — so no pin move could ever have worked. The real disease was a
**missing translation unit**: `src/network/net/QuazalSession.cpp` did not exist
in the tree. Porting it, wiring it into `objects.json`, re-homing the ctor pin
out of `StorePurchaser`, and naming three addresses measured **+3 fns / +92 B**,
not the briefed 60 B from a pin move.

Identification rested on three independent instruments: the vtable at
`0x82058F7C` parsed out of retail `.rdata`; the 16-byte `Cancel`/`OnCompletion`
bodies both tail-branching to `?OnCreateSessionJobComplete@NetSession@@QAAX_N@Z`;
and the ctor's `stw r30,0x8` / `stb r29,0xc` confirming member offsets.
`IsFinished` was predicted in advance **not** to match (measured 65.66%) because
our `QuazalSession` is a two-int stub where retail's is a real Quazal NetZ
object — a prediction registered so the shortfall could not later be mistaken
for a defect.

## The oracle was wrong twice in one function

`NetSession::OnMsg(const JoinResponseMsg&)` in our tree was **verbatim identical**
to `rb3/src/network/net/NetSession.cpp` — character for character. There was no
porting slip to find. Retail nonetheless lacks two constructs the Wii build has:

1. a per-user `HasOnlinePrivilege()` loop with a recursive
   `return OnMsg(respMsg)` early-out;
2. the `TheVoiceChatMgr->JoinVoiceChannel()` call.

Both are plausibly platform-managed differently on Xbox Live (privileges at
sign-in; voice chat in the XBL session layer). `../rb3` is the Wii **development**
build, and retail bytes outrank the oracle. **A source file that matches the
oracle perfectly is not thereby correct.**

Each removal was corroborated by a second instrument before it was made:
- the loop, by a **16-byte frame excess** (`subi r31,r1,0xe0` vs retail `0xd0`)
  exactly accounting for the `JoinResponseMsg respMsg` local inside it — measured
  before the edit, so the edit could not have been fitted to it;
- the call, by 3 clean inserts naming `?JoinVoiceChannel@VoiceChatMgr@@QAAXXZ`
  with no counterpart.

## A register swap that was not a register swap

After the loop removal, `OnMsg` had 20 charged sites: 3 inserts and **17 sites
that were a pure `r26` <-> `r27` exchange**. The 17 are precisely the shape that
invites an `AT_LIMIT` / permuter-bound verdict on a 756-byte row.

Pre-registered instead: the swap is a *symptom*, registers follow liveness, and
deleting a call that consumes a register across the middle of the body is a
liveness change. **Deleting three instructions cleared all twenty sites** and the
row crossed to 100.

Had the 17 been read as the diagnosis rather than the symptom, the largest row
this lane closed would have been filed as unfixable with its actual
three-instruction cause sitting directly above it in the same table.

## Two source-construct lessons

**`IsLocal` — an enumerated bool, not a range trick.** Retail compares `mState`
against 3, 4, 5, 6 individually, funnels hits to a common `li r11,1`, then does
`clrlwi. r11,r11,24`. That last instruction is an 8-bit zero-extend-and-test: a
**bool materialized into a variable and re-tested**, which a bare
`if (a||b||c||d)` would not produce (MSVC would branch directly off each
comparison). Our ported `mState - 3U <= 3` was a decompiler's compression of
names it had lost. Recovering the enum names recovered the codegen:

```cpp
bool joining = mState == kCreatingJoinSession || mState == kConnectingToSession
    || mState == kRequestingJoin || mState == kRevertingToHost;
if (joining) { return false; } else if (...)
```

**`Join` — tail-merge direction is a source-layout lever.** `Join` paired at
98.38% with 4 of 137 instructions charged; the cause was two identical
`Handle(msg,false)` cleanup blocks merged in the opposite direction from retail.
Converting an `if / else if / else` chain into two early `return`s chose the
other survivor and took the row to 100 (+544 B).

## Task 4 — a negative, adjudicated on retail bytes

`SaveLoadMgrStatusUpdateMsg(int status)` is **correctly an `int`**. The bool tell
`clrlwi rN,r4,24` occurs **0 times** in the entire 132-byte body; `+0x18` is
`li r11,0`; and `stw r4,0x58(r31)` stores the argument at full 32-bit width,
which a bool cannot reach unmasked. Our row is already `fuzzy 100.0 / mpn 100.0`
with **no `masked_equal` flag**, so changing it would break a matching row, and
the map name (`...@@QAA@H@Z`) is already right.

With this adjudicated the int/bool message-ctor family is **closed**: of 13
single-param ctors, 3 are already bool, 8 take pointers, 1 takes `Symbol`, and
this last int is correct. There is no candidate left to sweep.

## Method notes worth carrying forward

- **A size instrument that agrees with your prior is the one to re-derive.** I
  predicted `Join` and `AssignLocalOwner` could never match, by comparing COFF
  **section raw sizes** (656 B, 192 B) against retail body sizes (544 B, 180 B).
  A `.text` COMDAT also carries an 8-byte EH prefix, EH funclets and alignment,
  so a section size is not a body size. `Join` reached 100. This is the STLPORT-1
  trap, and it read as confirming evidence precisely because it agreed with me.
- **`masked_equal` must be split out of any headline.** Task 3c's +4 fns / +200 B
  was *entirely* funclet byte-signature pairing (Δhonest +0) — five 40-byte EH
  funclets that began matching once the body's shape changed. Reported as a bare
  +4/+200 it would badly overstate the work.
- **Commit an inert file to price a new TU.** `ab_measure --from-dirty` refuses
  untracked files in build-relevant dirs, so a new `.cpp` cannot be measured that
  way. Committing it alone — absent from `objects.json` and `#include`d nowhere,
  hence no compile edge — leaves the wiring dirty and preserves the delta's sign.
- **`target_symbol_map.json` uses 1-space indent.** A `json.dumps(indent=2)`
  round-trip reformats all 61k lines. Insert map rows by surgical textual line
  insert, and note the file carries non-address metadata keys that blow up a
  naive `sorted(key=lambda x: int(x,16))`.
- **The `.s` address column is synthetic for multi-block units.** `SaveLoadManager.s`
  renders the ctor at `0x8254C8E0` when it lives at `0x8254CAC8`. Key every scan
  on the `.fn fn_<addr>` symbol.

## Handoff

- **Nothing left in this unit is a wall.** `NetSession` went 281 -> 287 matched
  rows this lane.
- `MakeQuazalSessionJob::IsFinished` sits at 65.66% and is blocked on
  `QuazalSession` being a two-int stub rather than a real Quazal NetZ object.
  That is a porting job, not a matching job.
- `fn_823F2D00`'s vtable is `lbl_82058F94`, immediately after
  MakeQuazalSessionJob's vtable+COL in `.rdata` — implying a **second job class
  in the same TU** that the rb3-Wii oracle does not carry. Deliberately not
  pursued; recorded so the next lane does not re-derive it.
- `??_ENetSession@@UAAPAXI@Z` at `0x823E46D0` is a UNDEF weak external and was
  deliberately left in MeshAnim. It is not a mis-pin to fix.
