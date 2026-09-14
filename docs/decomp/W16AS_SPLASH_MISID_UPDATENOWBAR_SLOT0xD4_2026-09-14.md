# W16-AS — the mis-identified `Splash` ctor, and `TrackPanelDirBase` vtable slot `0xd4`

**Lane:** W16-AS · **Branch:** `w16-as` · **Worktree:** `~/tmp/wt-w16-as` · **Base:** main `0040e815`
**Date:** 2026-09-14 · **Ruler:** `name_check` (read from `report.json` `provenance.diff_config`)
**objdiff:** 4.2.9 `5a51cd51fe0a353f` · **`total_code`:** 10,246,004 (read from the key, never hardcoded)
**Source of record consumed:** `docs/decomp/W16AP_GAMEPANEL_UNIT_CLOSURE_2026-09-14.md`

## Result

| measure | before | after | Δ |
|---|---:|---:|---:|
| `matched_functions` (lane-internal) | 43,455 | **43,456** | **+1** |
| `matched_code` (lane-internal) | 4,024,232 | **4,024,700** | **+468 B** |
| `matched_code_percent` | 39.27611 | 39.28068 | +0.00457 |
| `fuzzy_match_percent` | 49.64466 | 49.648354 | +0.003694 |
| `default/GamePanel` | 93/97 | **94/97** | +1 |

Priced by set-diff of the `fuzzy==100` row set (`tools/rowset_snapshot.py`), run inside the worktree,
against snapshots this lane took itself. **Rows that fell out: ZERO, at every step.**

```
CROSSED IN : 1 rows, 468 B
   +    468 B  default/GamePanel::??0GamePanel@@QAA@XZ
FELL OUT   : 0 rows, 0 B
```

| sha | what |
|---|---|
| `87785557` | map: `0x826ccc10` is `DirectInstrument`'s ctor; the real `??0Splash` is `0x827421b8` |
| `8a5f0c73` | vtable: prove slot `0xd4`'s 4-arg signature; remove two false comments |

---

## Item 1 — `0x826ccc10` was NOT `??0Splash@@QAA@XZ`

**Answers to the three questions the brief asked, in order.**

### (a) Which class's ctor lives at `0x826ccc10`? — `??0DirectInstrument@@QAA@XZ`

Six independent channels, all on retail bytes. Channel 6 is the one that could have refuted the other five.

1. **No polymorphic vptr at `+0`.** `Splash` declares `virtual ~Splash()`, so `??0Splash` *must* store
   `??_7Splash@@6B@` at offset 0. The body at `0x826ccc10` never does. It stores `li 0x7f` there
   instead. **A body that stores no vtable pointer cannot be a polymorphic class's constructor.**
2. **The pointer it *does* store is `ObjDirPtr`'s, at `+0x4`.** `lbl_82000A04` decodes from retail
   `.rdata` as `??_7?$ObjDirPtr@VObjectDir@@@@6B@` — slot 0 `??_G?$ObjDirPtr@VObjectDir@@@@UAAPAXI@Z`,
   slot 3 `?IsDirPtr@?$ObjDirPtr@VObjectDir@@@@UAA_NXZ`. `DirectInstrument`'s member at `+0x4` is
   `ObjDirPtr<ObjectDir> mDir`. Exact fit.
3. **The body is `DirectInstrument`'s, semantically.** `[this+0]` is set to `0x7f` (127, MIDI max
   volume) by the ctor-init list, then overwritten by
   `SystemConfig(Symbol("instruments"), …)->FindArray(Symbol("chamberlin"), true)->Int(…)` — i.e.
   `int mVolume`. It then builds a `FilePath` and calls a 5-arg loader on `this+4`. That is
   `DirectInstrument { int mVolume; ObjDirPtr<ObjectDir> mDir; MidiInstrument *; Fader *; }`.
   None of `Splash`'s members (`std::list mScreens`, `Timer`, `CriticalSection`,
   `SynchronizationEvent`) is constructed anywhere in it.
4. **Spatial bracketing.** There is no whole-program optimization, so TU grouping in `.text` is
   preserved. `0x826ccc10` sits between `?Enable@DirectInstrument@@QAAXXZ` (`0x826ccb60`) and
   `??1DirectInstrument@@QAA@XZ` (`0x826ccdb8`), inside an unbroken run of `DirectInstrument` members.
5. **Caller population.** `0x826ccc10` has exactly **one** code caller in the image — `GamePanel.s` —
   and `??0GamePanel` constructs a `DirectInstrument` member. (The three `.rdata` hits are EH
   IP-to-state map entries, not callers.)
6. **The falsifiable one.** With the *identical, unchanged* base source, re-pointing the row at
   `0x827421b8` moved `??0Splash@@QAA@XZ` from **fuzzy 6.890244 → 92.266670** (mpn 8.658537 →
   93.123810). Had the identification been wrong this would have stayed low. A 6.89 → 92.27 jump with
   zero source change is not something a mis-identification produces.

**AP's flagged size figure, tested literally as the brief required.** AP wrote "~420 B" for our
`??0DirectInstrument` and warned it came from a reader with the STLPORT-1 one-sided artifact.
Re-derived from the COFF: our `??0DirectInstrument@@QAA@XZ` `.text` COMDAT **rawsize is exactly 420**,
and retail's body at `0x826ccc10` is **exactly 328** (`(last − first + 4)` over the `.s` listing).
The figure is **confirmed, not artifact-inflated** — this is the COMDAT *section* size, not a span
billed against a successor symbol, so the STLPORT-1 failure mode does not apply here. AP's
"different sizes cannot fold ⇒ the `AT_LIMIT`/ICF label is wrong" reasoning therefore stands.

### (b) Which unit's span contains it, and does that unit's base obj define the correct name?

`config/45410914/splits.txt` line **6295**: `Splash.cpp: .text start:0x826ccc10 end:0x826ccd58`.
That 328 B pin is an **island sandwiched inside `band3/game/TrainerPanel.cpp`**:

```
0x826ccb00-0x826ccc10  band3/game/TrainerPanel.cpp
0x826ccc10-0x826ccd58  Splash.cpp          <-- the mis-pin
0x826ccd58-0x826cd438  band3/game/TrainerPanel.cpp
```

**No**, `Splash.obj` does not define `??0DirectInstrument@@QAA@XZ` — that symbol is compiled into
`build/45410914/src/band3/game/DirectInstrument.obj`. So the renamed row is now **unpaired at 0 %**.
That is the exact hazard AP declined to walk into, and it is why the rename is only safe *here*:
the row contributed **0 bytes and 0 matched functions at 6.89 % too**, so unpairing it costs
literally nothing on either ruler, while the rename buys `??0GamePanel`'s 468 B. I verified the base
obj's symbol table directly before betting, rather than assuming.

### (c) Where is retail's real `??0Splash`? — `0x827421b8`, 420 B, previously anonymous

Found exactly as the brief suggested, by looking for the body that loads `??_7Splash@@6B@`:

- It stores `lbl_8210345C` at `+0`, and that label decodes from retail `.rdata` as **`??_7Splash@@6B@`**:
  slot 0 = `??_GSplash@@UAAPAXI@Z` (`0x82742990`), slot 1 = `?Draw@Splash@@MAAXXZ` (`0x82741708`) —
  both already-mapped `Splash` members.
- It constructs `??0Timer@@QAA@XZ`, `??0CriticalSection@@QAA@XZ`, `??0SynchronizationEvent@@QAA@XZ` —
  `Splash`'s `mTimer` / `mStateLock` / `mWorkerEvent`.
- Its config symbol string is literally **`"splash_time"`** → `int mSplashDurationMs`.
- Its sole code caller is `App.s`, at `addi r3, r31, 0xf0` — an in-place subobject construction.

It lies inside `Splash.cpp`'s pinned span `0x827414e0-0x82742968`, and our base `Splash.obj` **does**
define `??0Splash@@QAA@XZ`, so the name pairs against a symbol that exists.

**Bet analysis before naming** (under `name_check`, naming an anonymous address converts a *forgiven*
placeholder call site into a *checked* one, so it is a bet): `0x827421b8`'s only code caller is `App`,
whose unit measures **0/1 functions, 0/1,864 bytes, fuzzy 0 — unpaired**. The downside was therefore
bounded at **exactly zero** before the edit. The two edits are **coupled**: applying only the second
would put two addresses on one name and break the map's bijection.

**Predicted +1 fn / +468 B, 0 rows out. Measured exactly that.** No prediction miss this time — which
is worth saying only because AP's two misses were both cascades into unlisted rows, and a map rename
has no such cascade: it changes one relocation *name*, recompiles nothing, and cannot move a funclet.

⚠ **Instrument note.** dtk's address column in `Splash.s` renders `fn_827421B8`'s body at `826CDBC0`
— the synthetic `first_block_start + cumulative offset` value CLAUDE.md warns about for multi-block
units. Every read in this lane was keyed on the `.fn fn_<addr>` symbol, never the address column.

---

## Item 2 — `TrackPanelDirBase` vtable slot `0xd4`

### (a) The signature is PROVED: `(const char *, const char *, const char *, Symbol)`

- **`TrackPanelDir`'s vtable is `.rdata 0x8202d464`.** Slot `0xd0` is
  `?GetGemTrackResourceManager@TrackPanelDir@@UBAPAVGemTrackResourceManager@@XZ` and slot `0xdc` is
  `0x00000000` — so `0xd4`/`0xd8` genuinely are the **last two slots**, confirming the header's
  existing placement.
- **`0x82303bb8` occurs as a `.rdata` word exactly ONCE in the whole image.** So `TrackPanelDir`
  supplies the only body; every other derivation inherits the empty base version, which folds into
  the shared bare-`blr` COMDAT.
- **Exactly one caller.** Of **13** `?GetTrackPanelDir@@YAPAVTrackPanelDirBase@@XZ` call sites, exactly
  one dispatches slot `0xd4`: GamePanel's `fn_82695178`. The receiver's type comes from the
  accessor's own return type, not from the offset — ⚠ **slot offset `0xd4` is NOT class-specific**;
  a naive offset scan finds 10 unrelated vtable calls at `0xd4` in other units, and keying on the
  offset alone would have produced a confident wrong answer.
- **Arity 4**: the override saves and uses `r4..r7` (→ r29/r28/r27/r26), all four.
- **Types, three independent ways.** The override does:

```
if (this->0x374) {
    this->0x344->vtbl[0x58](a, true);      // ?SetDisplayText@UILabel@@MAAXPBD_N@Z
    this->0x350->vtbl[0x58](b, true);
    this->0x35c->vtbl[0x58](c, true);
    SetTextToken((UILabel *)this->0x368, d);   // ?SetTextToken@UILabel@@QAAXVSymbol@@@Z
}
```

  `a`/`b`/`c` are `SetDisplayText`'s first parameter (`PBD` = `const char *`) **and** are produced by
  `MakeString` (return type `PBD`); `d` is `SetTextToken`'s parameter (`VSymbol@@`).

⚠ **ICF fold-artifact trap, recorded in the header.** The map spells the `MakeString` instantiation
`??$MakeString@VSymbol@@PBDPBD@@YAPBDPBDVSymbol@@00@Z`. Reading that as the true call signature is
**wrong**: every all-4-byte-argument instantiation of `MakeString` emits identical code and ICF
collapses them onto one arbitrary survivor name. The actual format strings are `"%d.%02d.%02d"` and
`"%d.%d.%03d"`, which take **three ints** each.

### The NAME is deliberately NOT changed, and that is a result, not an omission

The brief invited me to "name the slot properly if the callers tell you what it is". **They do not.**
- rb3-Wii's `TrackPanelDirBase.h` **ends at `GetGemTrackResourceManager`** — slots `0xd4`/`0xd8` are
  RB3-360-only and the oracle has nothing to transfer.
- The one suggestive string inside the override, `"audition_time_display_string_for"`, is a
  function-local static `Symbol` that is **constructed and never read**, and it does not exist in the
  Wii tree either.

Proving a signature is not proving a name. The slot keeps `Unkd4`, with the evidence recorded in the
header so the next lane does not re-hunt it.

### (b) Blast radius — MEASURED, before and after, not assumed

`TrackPanelDirBase.h` is a shared `bandobj` header and the change alters an inline virtual's mangled
name, so the risk was real. **73 TUs recompiled** (`band3/bandtrack`, `band3/game`, `band3/meta_band`
and more) — so the Δ0 below is a *live* measurement, not an absent-vs-absent one. I checked the
recompile count explicitly because a Δ0 whose build did nothing is the classic vacuous result.

```
CROSSED IN : 0 rows, 0 B
FELL OUT   : 0 rows, 0 B
matched_functions 43456 -> 43456 · matched_code 4024700 -> 4024700
fuzzy_match_percent 49.648200 -> 49.648354 (+0.000154)
```

`?UpdateNowBar@GamePanel@@QAAXXZ` itself moved **fuzzy 5.929900 → 8.191083** (mpn 5.929900 →
8.477707) from the corrected call shape alone.

**Landed at Δ0 on proof**, per the standing rule that a correct vtable signature is worth landing even
when it does not move the headline, and per "accuracy beats headline %".

### (c) The body was NOT ported — and two of AP's three blockers are now cheaper

`?UpdateNowBar@GamePanel@@QAAXXZ`, 628 B / 157 instructions, still cannot cross. `matched_code` is
all-or-nothing per row, so partial progress buys zero bytes — AP's reason for deferring stands. What
this lane changed is the *cost* of the remaining work:

| AP's blocker | status after this lane |
|---|---|
| widen slot `0xd4` | **DONE** — proved and landed |
| identify `fn_827C91A0` | **mostly solved.** It is a 9-instruction thunk: `r3 = *lbl_82C78F5C; f1 *= 1000.0f; tailcall r3->vtbl[0x8](f1)`. `lbl_820010B4` is the float **1000.0**, i.e. **seconds → milliseconds** forwarded to a global singleton's slot `0x8`. Consistent with the time-display reading. Only the singleton at `lbl_82C78F5C` remains unnamed. |
| identify `lbl_82C71838` | **NOT a blocker at all.** It is referenced by **464 units** and is consumed as `??0Symbol@@QAA@PBD@Z`'s `const char *` argument; the byte at its target `0x82000c55` is `\0`. It is the engine-wide **`gNullStr`** empty-string global — i.e. every use is just a default-constructed `Symbol()`. |
| model `TheSongDB`'s 0x24-byte record vector at `+0x20` | still open — the one genuine remaining unknown |

Callees of `fn_82695178`, for whoever picks this up: `MakeString`,
`?GetMaxValue@TourProperty@@QBAMXZ`, `?Seconds@TaskMgr@@QBAMW4TimeReference@1@@Z`, `??0Symbol@@QAA@PBD@Z`,
`fn_827C91A0`, `fmod`, `?GetTrackPanelDir@@YAPAVTrackPanelDirBase@@XZ`; data `?TheSongDB@@3PAVSongDB@@A`.
Float constants `lbl_820E27B8 = 4.47656` and `lbl_820E27C0 = 7.41553`.

### Two false in-tree comments removed (one of them newly found)

`src/band3/game/GamePanel.cpp` carried two claims, **both wrong on retail bytes**:

1. *"that text-formatting body belongs to a different symbol (Unkd4's true target, not
   UpdateNowBar)"* — AP refuted this; **re-verified independently here**. The three `MakeString`
   calls are inside `fn_82695178`; the slot body does no formatting whatsoever, it only fans finished
   strings out to four `UILabel`s.
2. *"formats the same `"MBT %d:%d:%03d [...]"` text seen in the debug-HUD variant above"* — **this one
   AP did not catch.** `fn_82695178` references **no `MBT` string at all**; its only format strings
   are `"%d.%02d.%02d"` (twice) and `"%d.%d.%03d"`, and with `TaskMgr::Seconds` and `fmod` beside them
   it is a **time display**, not the debug HUD.

Both were deleted rather than softened, and the correction records *why*, so the claim cannot return.

---

## Item 3 — `PerfectOverdriveTracker` (recon only, as scoped)

`default/band3/game/PerfectOverdriveTracker`: **41/43 functions, 4,540/6,744 bytes.**

| row | size | fuzzy |
|---|---:|---:|
| `?Poll_@PerfectOverdriveTracker@@UAAXM@Z` | 1,248 B | 99.599 |
| `?GetPlayerContributionString@PerfectOverdriveTracker@@UBA…` | 436 B | 99.817 |
| `fn_826E1740` | 344 B | 0.0 (unpaired, unnamed) |
| four funclets `fn_826E0FC4/0FF0/1148/1174` | 44 B each | 99.545 |

**`?Poll_@` is the biggest size-if-it-crosses here, and it is NOT cheap.** Priced from its charged-site
list (not from a mismatch count): 4 charges of 313 instructions, and **all four are
register-allocation / scheduling class** — `add r10,r11,r10` vs `add r10,r10,r11` (commutative operand
order, twice), a `clrlwi.` destination-register difference, and one extra `mr r10,r11`. No source-logic
defect is named by any charge. Permuter/liveness territory, which is deferred by standing directive, so
I did **not** open it. `fn_826E1740` (344 B, unpaired) is an identification target, not a source target.

---

## For the owning lanes — proposals, NOT applied

### For W16-AQ (`config/45410914/splits.txt`) — the `DirectInstrument` TU is unpinned and split across units

`band3/game/DirectInstrument.cpp` **is** declared in `config/45410914/objects.json` (line 1214,
`"NonMatching"`) and compiles to a real base obj defining all its methods — but it has **no
`splits.txt` heading at all**, so its retail bodies are scattered into neighbouring units' spans:

| retail | size | currently pinned to | correct owner |
|---|---:|---|---|
| `0x826cca88` `?Disable@DirectInstrument@@` | 100 B | `band3/game/TrainerPanel.cpp` | DirectInstrument.cpp |
| `0x826ccb00` `?NoteOn@` | 28 B | `band3/game/TrainerPanel.cpp` | " |
| `0x826ccb20` `?NoteOff@` | 12 B | `band3/game/TrainerPanel.cpp` | " |
| `0x826ccb30` `?PlayNote@` | 24 B | `band3/game/TrainerPanel.cpp` | " |
| `0x826ccb48` (8 B ICF thunk, AP's `?IsLoaded@DirectInstrument@@`) | 8 B | `band3/game/TrainerPanel.cpp` | " |
| `0x826ccb50` `?PostLoad@` | 12 B | `band3/game/TrainerPanel.cpp` | " |
| `0x826ccb60` `?Enable@` | 168 B | `band3/game/TrainerPanel.cpp` | " |
| **`0x826ccc10` `??0DirectInstrument@@`** | **328 B** | **`Splash.cpp` (line 6295)** | " |
| `0x826ccdb8` `??1DirectInstrument@@` | 68 B | `band3/game/TrainerPanel.cpp` | " |

⚠ `0x826ccaf0-0x826ccb00` (16 B) is pinned to `PracticePanel.cpp` and holds
`?InTransition@UIManager@@QAA_NXZ` — an ICF survivor from another TU that legitimately landed inside
the range. Do not assume the region is homogeneous.

**Quantified upside, so this is not a speculative tidy-up.** Six of the eight `DirectInstrument`
bodies are **size-identical to retail** (`Disable` 100, `Enable` 168, `NoteOn` 28, `NoteOff` 12,
`PlayNote` 24, `PostLoad` 12 = **344 B**); the ctor (ours 420 vs retail 328) and dtor (ours 120 vs
retail 68) differ. Size agreement is necessary, not sufficient — but **344 B is the size of the
prize** if `band3/game/DirectInstrument.cpp` gets a heading. Minimum surgical version: move the
`.text start:0x826ccc10 end:0x826ccd58` line off `Splash.cpp:`, which at least stops a 328 B row that
can never pair from sitting in `default/Splash`'s denominator.

⛔ **Do not hand-edit `.pdata`** — it is derived output, re-derived from `.text` on every split run.

### For the alias owner (W16-AL)

No new alias proposals from this lane. AP's three (`docs/decomp/W16AP_ALIAS_PROPOSALS_FOR_W16AL.json`)
are unaffected by anything here. ⚠ One correction for whoever applies them: AP's note that
`0x826ccb48` is `?IsLoaded@DirectInstrument@@QAA_NXZ` is consistent with what I see (the map currently
names it `??$_Destroy@UGrammar@SpeechMgr@@…`, an 8-byte thunk fold), but I did **not** re-prove it —
it was out of scope and is not load-bearing for anything I landed.

---

## NOT done, with reasons

- **`?UpdateNowBar@GamePanel@@QAAXXZ` body — not ported.** Needs `TheSongDB`'s 0x24-byte record vector
  at `+0x20` modelled, plus the singleton at `lbl_82C78F5C`. `matched_code` is all-or-nothing per row,
  so a partial port banks nothing. The signature was landed because it is *proved*, independently of
  whether the row ever crosses.
- **Slot `0xd4` not renamed** — no oracle and no retail evidence for a name (see above). Inventing one
  would be fabrication, and a wrong name under `name_check` is charged to every caller.
- **Slot `0xd8` (`0x82309b60`) not touched** — out of scope; its one caller (`TrackPanel.cpp:237`)
  already compiles against the existing no-arg signature and nothing I measured contradicts it.
- **`?Poll_@PerfectOverdriveTracker@@` not opened** — all four charges are regalloc/scheduling class;
  permuter is deferred by standing directive.
- **`config/45410914/splits.txt`, `scripts/symbol_aliases.json`, `icf_alias_*` — untouched** (lane
  bars). Splits work filed above as text with exact addresses.
- **No TourProgress / hashtable / CampaignSongInfoPanel / MainHubPanel files touched** (W16-AQ's).
  Note `?GetMaxValue@TourProperty@@QBAMXZ` appears only as a *callee* of `UpdateNowBar`; no
  `TourProperty` file was edited.
- **`src/network/net/Server.h` `GetPlayerID(int)` — untouched**, per the standing bar.
- **No `list<T>`/`ObjList` `resize` map rows and nothing in the `0x823c38e8` region** (W16-AR's).
- **No rebase onto main.** Every number here is lane-internal against the snapshot this lane started
  from (`0040e815`); the coordinator reconciles at landing.

## Escalation candidates

Neither item produced an "unfixable" verdict, so there is nothing to escalate on proof-of-impossibility
grounds. The honest status of the one row that did not cross, `?UpdateNowBar@GamePanel@@QAAXXZ`, is
**not-yet-done, not impossible**: it is RB3-360-specific with no oracle, and the remaining work is a
`SongDB` record-layout modelling job rather than a wall on retail bytes. If a future lane wants it, it
is a reasonable **Fable** candidate — a 628 B body with no oracle, one unnamed singleton and one
container layout to derive.

## Gates

Run in order in the worktree, native gate **last** (this lane edits a shared `bandobj` header — exactly
the class of change the native link catches and the match build structurally cannot).

| gate | result |
|---|---|
| full build | rc=0 (`~/tmp/rb3_build_w16as_3.log`) |
| `scripts/verify_ruler_agreement.py --check` | see below |
| `scripts/verify_objs_patched.py --verify-manifest` | see below |
| `tools/native_build_gate.sh` | `NATIVE_GATE_RESULT` line pasted verbatim below |

**Measured, in order:**

```
full build                                      rc=0   (~/tmp/rb3_build_w16as_4.log)
scripts/verify_ruler_agreement.py --check       rc=0   OK: both objdiff-cli entry points resolve the same ruler.
scripts/verify_objs_patched.py --verify-manifest rc=0  [patch-state] OK: 1215 decomp, 3115 target objects match
                                                       (tree_sha256=5c17119a6654cf11)
tools/native_build_gate.sh                      rc=0
```

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

`skipped=0` as required — full coverage, not an INCOMPLETE run. All four gates green, native last.
The native gate ran against the final state of every source and map file in this lane; the only tree
change after it is this Markdown file, which no build edge consumes.
