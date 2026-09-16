# W16-EU — one pin was wrong, one name was wrong, one name was wrong AND unnamed; and a briefed row that does not exist

Lane W16-EU, 2026-09-16. Branch `w16-eu`, worktree `~/tmp/wt-w16-eu`, off
`fa93fb76` (W16-EQ landed). Three tasks, each priced in its own A/B so each
delta stays attributable.

## Summary

| task | what | pre-registered | measured |
|---|---|---:|---:|
| 1 | re-home `0x823E1B40` SpeechMgr -> NetSession | +136 B / +1 fn | **+136 B / +1 fn** |
| 2 | map: `0x823F2C98` is `MakeQuazalSessionJob::ctor` | +136 B / +1 fn | **+136 B / +1 fn** |
| 3 | map+source: `0x823E1808`/`0x823E1938` ctor swap + int/bool | +732 B / +3 fn (range +596..+1088) | **+1,088 B / +4 fn** |

**Total +1,360 B / +6 functions**, `matched_code_percent` 40.305344 ->
40.318620. Task 3 was **not in the brief** — it was found by testing a briefed
figure that turned out to be false, and it is worth more than both briefed
tasks combined.

## What the brief got wrong

Every figure was tested literally before being built on. Four were wrong, and
one of those errors is what produced Task 3.

1. **`?Disconnect@NetSession@@QAAXXZ` does not exist.** The brief (inheriting
   EQ's handoff) priced Task 2 at ~272 B = `RegisterOnline` 136 B +
   `Disconnect` 136 B. There is **no such row anywhere in `report.json`**. The
   136 B / 99.85294 row that does exist, and that was almost certainly meant,
   is `??0ProcessedJoinRequestMsg@@QAA@_N@Z` — whose single charge is an
   entirely different defect. Chasing that difference **is** Task 3.
   ⇒ **Task 2 is worth 136 B, not 272 B.**
2. **`0x826F1080` cannot be re-homed at all, and its ~36 B is uncollectable by
   any pin move.** `?GetFinger@FretHand@@QBAXIAAH00@Z` is an **UNDEF extern**
   in all three objects that reference it (`ChordbookPanel.obj`,
   `PracticePanel.obj`, `RGTrainerPanel.obj`) and **there is no `FretHand.cpp`
   in the tree** — `FretHand.h` declares eight methods and defines none. No
   object anywhere defines the symbol, so no pin can make the row pair.
   ⇒ **Task 1 is worth 136 B, not the briefed 172 B.**
3. **The brief's pin range for `0x826F1080` is wrong.** It says
   `.text 0x826F1080–0x826F1188`; the actual ViewSetting block is
   `0x826F0F98–0x826F10A8` (272 B).
4. **EQ's caller names for `0x823F2C98` are not supported.** EQ named them
   `??_ENetSession` and `?UpdateSyncStore@NetSession@@`; they resolve to the
   unmapped `fn_823E4808` (544 B — itself mis-pinned into `default/MeshAnim`)
   and `fn_823E4B68` (756 B). EQ's *conclusion* (3 callers, all NetSession)
   reproduced exactly; its caller *names* did not.

The brief's core claim — "the identification is right and the pin is wrong" —
is **half right**: true for `0x823E1B40`, false for `0x826F1080`, where the
identification is right, the pin is wrong, **and the source does not exist.**

## Task 1 — the re-homing lever

`0x823E1B40` was pinned into `SpeechMgr.cpp` as a 224 B block punched straight
through NetSession's span (NetSession ran `0x823E16A8–0x823E1B40` and resumed
at `0x823E1C20`). EQ's rename to `??0AddUserResultMsg@@QAA@_N@Z` was correct
and the row still read 0.0, because objdiff pairs by name **within a unit**.

**The pairing was measured before anything was moved.** COFF symbol tables put
`??0AddUserResultMsg@@QAA@_N@Z` in `network/net/NetSession.obj` (sec=492) and
nowhere else except as an UNDEF extern; diffing the SpeechMgr *target* obj
against the NetSession *base* obj at the graded ruler reported **fuzzy 100.0,
136/136 B, 0 charged sites**. That is the post-re-home score, obtained without
editing a pin — which is what made the +136 B prediction exact rather than a
guess.

### The whole block moved, and why that was the safer choice as well as the truer one

The 224 B block is ctor (136) + `fn_823E1BC8` (40) + `fn_823E1BF0` (40) + an
8 B EH prefix. **Both `fn_` rows were already at fuzzy 100.0** — 80 B of
counted bytes that a careless whole-block move could forfeit, PINHOME-1's
non-neutrality pointing the other way. The ctor ends exactly where
`fn_823E1BC8` begins, so a surgical 136 B move was available and risk-free.

I moved the whole block anyway, because the funclets are provably the ctor's
own: the ctor builds its frame as `r31 = r1 - 0x80`, stores its bool at
`0x58(r31)` and `this` at `0x94(r31)`; funclet A does `addi r3, r31, 0x58` and
calls a destructor, funclet B does `lwz r3, 0x94(r31)`. They clean up **that**
frame. Leaving them in SpeechMgr would have kept 80 B scoring by coincidence
in a unit that does not compile the function they unwind.

The metric risk was checked rather than assumed: `NetSession.obj` carries
**11** and **15** byte-compatible funclet bodies against `SpeechMgr.obj`'s
**2** and **4**, so byte-signature re-pairing had ample candidates on the
destination side. Measured: both re-paired at 100.0. They now pair correctly
instead of luckily.

SpeechMgr keeps two other `.text` blocks, so no unit drained empty (the
`Invalid COFF/PE section headers` hazard did not arise).

### `.pdata` is derived output — the first A/B refused, correctly

The first A/B **refused at exit 2 with no deltas**: `ninja: error: rebuilding
'build.ninja'`. My hand edit moved `.text` but left `.pdata` naming the old
TU, so dtk re-derived it and **rewrote `splits.txt` mid-build**, leaving the
edge dirty. This is the documented contract, not a bug. dtk merged
NetSession's two `.pdata` ranges into `0x82207A18–0x82207AB8` and deleted
SpeechMgr's `0x82207A88–0x82207AA0`. Two consecutive builds then agreed —
i.e. the file was a split fixed point — and the re-run measured cleanly.
**Only `.text` was edited by hand.** A lane doing this must let dtk re-derive
*before* the A/B, or it burns a leg.

### Measured

```
leg A matched=43969 code%=40.305344
leg B matched=43970 code%=40.306670   split=1 renamer_patched=1830
Δmatched=+1 Δhonest=+1 Δcode%=+0.001326pp Δcode_bytes=+136
```
Both legs settled and at a `symbols.txt` split fixed point; ruler
`functionRelocDiffs=name_check`, read from `objdiff.json`.
**Pre-registered +136 B / +1 fn. Measured +136 B / +1 fn.**

## Task 2 — `0x823F2C98` is a constructor, not an STL copy helper

Adjudicated on retail bytes **before** measuring, three independent strands:

1. **Body shape settles it alone.** `0x823F2C98` calls a base-class ctor
   (`bl fn_827CBAD8`), stores a pointer at `this+0x8`, stores a single **byte**
   at `this+0xc`, writes a **vtable pointer** to `this+0x0`, and returns
   `this`. `_M_allocate_and_copy` allocates and copy-loops; it has no vtable
   and no base ctor. The byte store independently confirms the `_N` (bool) in
   the mangling.
2. **Fan-in 3**, censused off retail `.text` (opcode 18, both LK values), all
   inside NetSession's span. EQ's proven folds were 15 / 44 / 10 / 453.
3. **3-for-3 caller bijection**: our source constructs `MakeQuazalSessionJob`
   only in `NetSession::RegisterOnline` (:142), `NetSession::Join` (:365) and
   `NetSession::OnMsg(JoinResponseMsg)` (:520).

Blast radius closed by measurement: 3 `bl`, 0 tail calls, and the only raw
big-endian pointer to the address in the whole image is its own `.pdata`
unwind record. The old name was on exactly one address, the new name on none,
and the map's duplicate-name count is 4 before and 4 after.

### ALIAS_SUSPECT fired, and why it is cleared

`ab_measure` flagged **ALIAS_SUSPECT**: `name_check` +136 B while the `none`
control stayed flat, on a map-only patch. That shape is shared by a fabricated
alias and by a genuine wrong-name repair — `none` ignores relocation names
**by construction**, so its flatness is not evidence in either direction and
the control *cannot* discriminate here. Cleared on the retail-byte evidence
above, plus: neither the old nor the new name appears in
`scripts/symbol_aliases.json` or `scripts/icf_alias_groups.json` (**0
occurrences each**), so no alias group was created or extended and no
forgiveness is involved. This is the "repair a WRONG existing name" class,
which legitimately pays.

### Measured

```
leg A matched=43970 code%=40.306670
leg B matched=43971 code%=40.308000   split=1 renamer_patched=1830
Δmatched=+1 Δhonest=+1 Δcode%=+0.001330pp Δcode_bytes=+136
```
**Pre-registered +136 B / +1 fn. Measured +136 B / +1 fn.** Exactly 3 rows
moved, all pre-registered:

| unit | row | A | B |
|---|---|---:|---:|
| NetSession | `?RegisterOnline@NetSession@@QAAXXZ` (136 B) | 99.85294 | **100.0** |
| StorePurchaser | `??$_M_allocate_and_copy@PB_K@…` (60 B) | 31.0 | *(gone)* |
| StorePurchaser | `??0MakeQuazalSessionJob@@…` (60 B) | *(none)* | 0.0 |

The callee row un-pairs to 0.0 and **that costs nothing** — it was at 31.0,
and `matched_code` is all-or-nothing per row. It can never pair anywhere: no
object DEFINES `MakeQuazalSessionJob`'s ctor (UNDEF extern in
`NetSession.obj`). `Δfuzzy = −0.000187pp` is exactly that forfeited partial
credit — a truer denominator, not a regression.

## Task 3 — the fifth mislabel: two constructor names transposed, one never mapped

Not briefed. Found by chasing the brief's phantom `Disconnect` row to the row
that actually carries 136 B at 99.85294, and asking what its single charge
really was.

### Evidence chain, all off retail bytes

The cheap explanation was tested first and **refuted**: I suspected the two
`Type()` symbols were transposed, since that is one map edit and explains a
ctor calling the wrong `Type()`. Disassembling both and reading the interned
`.rdata` strings:

```
0x823E0928 interns "session_ready"            -> Type() map name CORRECT
0x823E0A28 interns "processed_join_request"   -> Type() map name CORRECT
```

Both correct, which **redirects suspicion to the ctors** — and that is exactly
where it belonged:

```
0x823E1808  (mapped ??0ProcessedJoinRequestMsg) calls Type@SessionReadyMsg
fn_823E1938 (unmapped, fuzzy 0.0)              calls Type@ProcessedJoinRequestMsg
```

The two constructor names are **transposed**, and the second was **never
mapped at all**. Caller-set fan-in (EQ's discriminator, censused off retail
`.text`) is **2 and 2**, with a **2-for-2 caller bijection in both
directions** — the mislabel signature, nowhere near the folds' 15 / 44 / 10 /
453.

### The signature was wrong too — the second instance of EQ's int/bool bug

Both ctors carry `0x548B063E` (`clrlwi r11, r4, 24`) at +0x18, the
bool-parameter marker. Our `SessionReadyMsg(int i)` was therefore the wrong
signature, and **the map fix alone would not have paid**: the caller could not
construct the right overload. `SessionMgr.h:39` becomes `SessionReadyMsg(bool
b)`, and `NetSession.cpp:165`'s `static SessionReadyMsg msg(0)` becomes
`msg(false)` — required, because `DECLARE_MESSAGE` also generates a
`DataArray*` ctor, so a literal `0` is ambiguous against a `bool` overload
(C2668).

### Measured

```
leg A matched=43971 code%=40.308000
leg B matched=43975 code%=40.318620   split=1 renamer_patched=1830
Δmatched=+4 Δhonest=+4 Δcode%=+0.010620pp Δcode_bytes=+1088
```
**Pre-registered +732 B / +3 fns, range +596..+1088. Measured +1,088 B / +4
fns — the exact top of the range**, and a miss on the point estimate in the
favourable direction. The reason is worth recording: I priced
`OnRegisterSessionJobComplete`'s two register-allocation diffs as *surviving*
the fix, and they **dissolved** once the signature was corrected.
**`REGISTER_SWAP` is a symptom, not a diagnosis** — a thirteenth instance.

All five affected rows are now at fuzzy 100.0:

| row | size | note |
|---|---:|---|
| `??0SessionReadyMsg@@QAA@_N@Z` | 136 | renamed |
| `??0ProcessedJoinRequestMsg@@QAA@_N@Z` | 136 | newly mapped |
| `?DenyRequest@?A0x055cc49d@@…` | 184 | 100 before AND after, as predicted |
| `?OnCreateSessionJobComplete@NetSession@@` | 460 | |
| `?OnRegisterSessionJobComplete@NetSession@@` | 356 | the regalloc diffs dissolved |

`ALIAS_SUSPECT` did not fire — with `source` in the patch the control is
`NOT_APPLICABLE` by construction, and movement on `none` (+492 B) is expected
because the patch moves real code.

## Negative results — things that cost time and bought nothing

- **`?GetFinger@FretHand@@QBAXIAAH00@Z` is not re-homable** (see above). The
  whole ViewSetting block — `fn_826F0F98` (68 B), `fn_826F0FE0` (156 B),
  `GetFinger` (36 B) — sits at fuzzy 0.0 and contributes zero either way, so
  moving the pin would have been measurable churn for a guaranteed +0. **The
  real fix is writing `FretHand.cpp`**, which is source work worth ~36 B and
  is a handoff, not this lane's job.
- **The `Type()`-transposition hypothesis was refuted** by reading the interned
  strings. Recording it because the refutation is what pointed at the ctors;
  had I "fixed" the `Type()` names on suspicion I would have broken two correct
  map entries and buried the real defect.
- **EQ's doc says "2 pre-existing duplicates" in the map; the tree has 4.** I
  introduced zero (4 before, 4 after, verified per task). Not chased — flagged
  below.
- **A vacuous charge filter nearly produced a clean false negative.** My first
  charge-enumeration script filtered instruction rows on `diff_kind`, **a field
  that does not exist in objdiff's JSON schema**, and reported a confident
  `CHARGED SITES: 0` on a row scoring 99.85. It was caught only because that
  contradicted the fuzzy score. The real field is `match_type` (`equal`,
  `diff_arg`, …). Exactly the vacuity class this repo's docs warn about: the
  wrong answer was clean, decisive, and agreed with nothing.
- **An enclosing-function resolver keyed on `^[A-Za-z_].*::.*\(` misses
  anonymous-namespace functions** and wrongly attributed `NetSession.cpp:377`
  to `NetSession::Join`; it is really inside
  `void DenyRequest(unsigned int, JoinResponseError, int)` in an anonymous
  namespace. Replaced with a brace-aware matcher.
- **Writing a deliverable into the worktree DURING an `ab_measure` run destroys
  it.** The restore removes untracked files created during the run — it says so
  in a banner, and it deleted the first draft of this document. Write run-time
  deliverables to `~/tmp` and copy them in.

## What I deliberately did NOT do

- **No aliases**, in either direction, on any task. `ALIAS_SUSPECT` on Task 2
  was cleared on retail bytes, not by adding forgiveness.
- **Did not name any anonymous address for its own sake.** `fn_823E1938` was
  named because its name was *transposed with another row's*, i.e. the
  wrong-existing-name class that pays — not to convert a forgiven placeholder
  into a checked site.
- **Did not move the `0x826F1080` pin**, having proved the move is worth
  exactly 0 (no object defines the symbol). Doing it anyway would have looked
  like progress in the diff and measured nothing.
- **Did not generalise Task 1 into a re-homing sweep.** The vein is 59 rows /
  2,972 B, and each row needs its own COFF-defined-symbol check; a sweep
  without that check re-homes rows whose source does not exist.
- **Did not touch the 4 duplicate map names** or `fn_823E4808`'s mis-pin into
  `default/MeshAnim` — both are real and both are out of this lane's scope.
- **Did not run the permuter** (OFF by standing directive).

## Handoffs

1. **`FretHand.cpp` does not exist.** `src/band3/game/FretHand.h` declares
   eight methods, including `GetFinger`, and the tree defines none of them.
   Three panels reference `GetFinger` as an UNDEF extern. Worth ~36 B for
   `GetFinger` alone, more for the other seven; needs a new TU in
   `objects.json` and a `.text` pin. **This is source work, not a pin move** —
   the brief's framing of it as a re-homing target is wrong.
2. **`fn_823E4808` (544 B) is pinned into `default/MeshAnim`** and is really a
   NetSession function (it calls `MakeQuazalSessionJob`'s ctor). `fn_823E4B68`
   (756 B) is unmapped and in NetSession's range. Both read 0.0. A re-home
   candidate pair with the Task 1 shape — but check COFF for a defining object
   first, which is the check that killed `GetFinger`.
3. **4 duplicate names in `target_symbol_map.json`** (EQ's doc says 2). Not
   introduced by this lane; worth an audit since a duplicate means one of the
   two addresses is certainly wrong.
4. **The int/bool ctor bug has now appeared twice** (EQ's, and
   `SessionReadyMsg` here). `clrlwi rN, r4, 24` at +0x18 in a
   `DECLARE_MESSAGE` ctor is a cheap, mechanical screen for the rest of the
   family; the C2668 guard (`msg(0)` -> `msg(false)`) is a known, small cost.
