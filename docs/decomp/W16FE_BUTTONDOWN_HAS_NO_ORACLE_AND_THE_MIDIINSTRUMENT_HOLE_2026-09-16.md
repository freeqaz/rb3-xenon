# W16-FE — `Game::OnMsg(ButtonDownMsg)` has NO oracle, and the hole next door was worth 340 B

**Branch** `w16-fe` (off main `3890b450`). **Ruler** `name_check` (graded), objdiff 4.2.9.
**Denominators read, not inherited:** `total_code` **10,247,068**, `total_functions` **69,240**.

**Headline: +4 functions / +340 B / +0.003314 pp, one unit completed, two real
behavioural bugs fixed — and the assigned 1,664 B row NOT closed, for a reason
that is structural rather than a matter of effort.**

---

## 0. Scorecard — pre-registered before the A/B (`PREREGISTRATION_W16FE.md`)

| measure | predicted Δ | measured Δ | |
|---|---:|---:|---|
| `matched_functions` | +4 | **+4** | ✅ |
| `matched_code` | +340 B | **+340 B** | ✅ |
| `matched_code_percent` | +0.003320 pp | **+0.003314 pp** | ✅ |
| `masked_equal_functions` | +0 | **+0** | ✅ |

Falsifiers, all required and all held:

- `?UpdatePausedState@Game@@QAAX_N00@Z` **improved without reaching 100**: 66.296 → **70.000**.
- `?OnMsg@Game@@...ButtonDownMsg@@@Z` unchanged at **8.010** (not touched).
- `?OnMsg@Game@@...ButtonUpMsg@@@Z` unchanged at **98.590 / mpn 100.0** (deliberately not attempted).

⚠ **My absolute predictions were off by +2 and every delta was exact** (predicted leg A
44,003 from main's `report.json`; measured leg A **44,005**). That is the house rule
*"deltas compose, absolutes do not"* demonstrating itself — a lane that had reported
absolutes here would have been wrong while being right about its change.

---

## 1. ⛔⛔ THE LOAD-BEARING FINDING: the 1,664 B body has NO SOURCE ORACLE IN EITHER SIBLING REPO

Neither the brief nor W16-EH stated this, and it is the actual reason the row is hard.
Measured, not assumed:

| probe | result |
|---|---|
| `OnMsg(ButtonDownMsg)` in rb3-Wii `band3/game/Game.cpp` | **ABSENT** — the file has no `ButtonDownMsg` handler at all |
| `OnMsg(ButtonUpMsg)` in rb3-Wii | **ABSENT** |
| `audition_cam_toggle` / `audition_jump_forward_ms` / `audition_jump_back_ms` / `audition_jump_end_buffer_ms` / `audition_keyboard_synth_volumes` anywhere in rb3-Wii | **ABSENT** (only `deploy_if_possible` exists, as an unrelated `Player` action) |
| any of the above, or `VocalGuidePitch`, in dc3-decomp | **ABSENT** |

⇒ This is **TU5-era code**. rb3-Wii is a vanilla-RB3 dev build and DC3 is a different
game; neither ever contained this feature. **Every one of the ~416 instructions must be
reverse-engineered from retail bytes alone.** Any successor who plans to "port it from
the oracle" is planning against a file that does not exist.

⚠ Note this does NOT make it RB3DX-specific: CLAUDE.md records the DX-lineage TU5 image
differing from clean TU5 by only 53 words / 10 byte-patch groups, far too small to hold a
1,664 B handler. It is official Harmonix TU5 code.

## 1.1 What this means for whether it can be closed at all

**I could not close it and I say so plainly.** Beyond the missing oracle:

- `matched_code` is all-or-nothing per row, so closing 400 of ~416 instructions banks
  **exactly zero bytes**. There is no partial credit to collect.
- The dispatch is MSVC **sparse-switch lowering** (value-mapping cascade → `mtctr` → a
  chain of `bdz`), which must be reproduced exactly.
- Several cases construct **local statics**, and `Game.obj` carries
  `/DRB3_HANDLE_LOCAL_STATIC`, so the guard/`??_B` shape is in play too.

W16-EH's structural decode of all eight switch cases
(`W16EH_BUTTONDOWNMSG_DECODE_AND_SHUTTLE_SETACTIVE_2026-09-16.md` §3) is **correct and
remains the map a successor should start from**. What I add is: the decode is not the
bottleneck, and no amount of oracle work will shorten the remaining job.

## 1.2 ✅ Two of W16-EH's three recorded blockers are now GONE — its refusal is partly stale

W16-EH declined to write the tail partly because of fold-name charges. Re-checked live:

| blocker | status now |
|---|---|
| `GetSongDurationMs@SongDB` ← `GetMaxValue@TourProperty` (`0x82368fc0`) | **aliased** (T2) |
| `GetGuideTrack@VocalGuidePitch` ← `size@ObjPtrList<Fader>` (`0x822e4500`) | **aliased** (our-side COMDAT identity) |
| `fn_826C9160` "the one genuine unknown" (§3.2) | **RESOLVED by this lane — see §3** |
| `Array`/`Int` at `fn_8274B0F8` | still open |
| `Shuttle::SetActive` (§4) | aliased by W16-EK; `?OnSetShuttle@Game@@` is now **100.0/100.0** |

⇒ The remaining obstacle is **writing 416 instructions with no oracle**, not name charges.

---

## 2. ⛔ The brief's ButtonUp claim is REFUTED: it is pure regalloc, not a relocation name

The brief priced `?OnMsg@Game@@...ButtonUpMsg@@@Z` (156 B, fuzzy 98.5897 / mpn 100.0) as
*"an arg-only shape … a relocation-NAME charge withholds all 156 B … 156 B for a one-line
fix"*. Measured on the graded ruler: **there is no name involved anywhere in the row.**

`canonical_match_percent` = **100.0**, `diff_score` 55/3900, and all **11** charged rows are
one 3-way callee-saved register rotation:

| | retail | ours |
|---|---|---|
| sret (`r3`) | `r31` | `r29` |
| `this` (`r4`) | `r30` | `r31` |
| `pad` | `r29` | `r30` |

Every opcode, offset, constant and branch is identical, in identical order. The one `bl`
resolves to `fn_825150C8` — a **placeholder**, which objdiff forgives, so it is not charged.
`diff_arg` covers register differences as well as relocation names; inferring "name charge"
from the arg-only shape is the trap.

⇒ **Evidenced negative: this row is permuter-class, and the permuter is OFF by standing
directive.** Recorded so the next lane does not re-derive it. Per rule 8 I did check for a
real source defect first — there is none to find; the source is semantically and
structurally identical to retail.

---

## 3. What I closed, and the retail-byte fact that proved each

All four crossings are in the `VocalGuidePitch ↔ MidiInstrument` interface, reached by
pulling on W16-EH's `fn_826C9160` thread.

### 3.1 `MidiInstrument::Pause(bool)` — 96 B — a WRONG MAP NAME, not a fold

`target_symbol_map.json` mapped `0x82714350` to
`?clear@?$ObjPtrList@VTask@@VObjectDir@@@@QAAXXZ`.

**That name is a `void()` and cannot be this body:** the body saves `r4` (`mr r30, r4`) and
forwards it into the per-node virtual call (`mr r4, r30`) — it consumes an argument the
named signature does not have. Signature-vs-call-site is the MPNGAP-1 adjudication rule and
it settles this outright.

Corroboration: the body lives in **`MidiInstrument.s`**, walks the list at `0x58(this)`
(`mActiveVoices` at 0x50, node pointer at +0x8), and vcalls slot `0x58/4 = 22`.

⚠ **Cascade measured BEFORE renaming** (the documented hazard is that proving a name wrong
does not make renaming safe): `fn_82714350` has **exactly one caller in all of retail asm**,
so the rename cannot charge anywhere else. The old row was fuzzy 54.458 → **0 bytes at
risk**.

### 3.2 `MidiInstrument::ReleaseNote(unsigned char)` — 104 B — anonymous, and an oracle defect

`0x827141D8` was **unmapped and fuzzy 0.000**. Body: `clrlwi r30, r4, 24` (⇒ `unsigned
char`), compared against `lbz r11, 0x34(r3)` (`NoteVoiceInst::mTriggerNote` at 0x34,
compiler-confirmed), then vcall slot 22 = `Stop()`.

★ **First port scored fuzzy 99.615 / mpn 100.0 and paid ZERO bytes**, on a single
instruction:

```
retail   cmplw cr6, r11, r30      ; TriggerNote() first
ours     cmplw cr6, r30, r11      ; uc first  <- rb3-Wii's `uc == (*it)->TriggerNote()`
```

Writing the comparison as `(*it)->TriggerNote() == uc` closed it. **One token, 104 B**, and
`mpn` was 100.0 on both sides — arg-blind `mpn` could not see it at all.

### 3.3 `MidiInstrument::Pause`'s loop body — the oracle is wrong a second time

rb3-Wii spells it `(*it)->Pause(b)`. Retail does `lwz r3, 0x28(r11)` **before** the vcall,
and `mSample` is at 0x28 — so the oracle's one-line `NoteVoiceInst::Pause` is **inlined
away** and the real spelling is `(*it)->Sample()->Pause(b)` (`SampleInst::Pause` is slot 22
= 0x58, compiler-reported). Writing the oracle literally emits a vcall on the wrong object.

⇒ Two independent oracle defects in one 200-byte pair. **A file that matches the oracle
perfectly is not thereby correct.**

### 3.4 `VocalGuidePitch::StopNote()` — 76 B — W16-EH's "one genuine unknown"

```cpp
mInstrument->ReleaseNote(mGuidePitch + mPitchModifier);
mGuidePitch = 0;   // 0xc
unk8 = 0;          // 0x8
```

Not guessed: the emitted sequence is **byte-identical to the block inside
`EnableGuideTrack` (`0x826C9098`)**, which already matched at 100% with exactly this source
spelling — so operand order is calibrated against a matching neighbour. Retail saves only
`r31` (frame 0x60) vs `EnableGuideTrack`'s `r30+r31` (0x70), consistent with one fewer live
value. Retail factored these three statements out; the Wii dev build inlines them into
`EnableGuideTrack`.

**Phantom check performed before naming** (a dtk mis-carve is indistinguishable from an
unidentified row): real `.pdata` entry `pdata@82234798` (unwind `0x40001304`), `symbols.txt`
size `0x4C`, complete prologue/epilogue, two `bl` callers. It exists.

**Anonymous pairing was tested first and refuted:** the unit's other anonymous row
`fn_826C96F8` reaches 100% only because it is `masked_equal` (an EH funclet paired by byte
signature). `fn_826C9160` is `masked_equal: false`, so a map name is genuinely required.

⚠⚠ **NAME PROVENANCE — do not cite `StopNote` as an identification.** The **body**, the
**class** and the **signature** (`void`, no args) are proven on retail bytes. The
**spelling** is a lane-assigned descriptive label with **no oracle backing** — rb3-Wii has
no such method and DC3 has no `VocalGuidePitch`. If a successor finds the true name,
changing it is free (one map row + one declaration).

⇒ `default/band3/game/VocalGuidePitch` is now **COMPLETE: 1728/1728 B, 16/16 fns**.

---

## 4. Two real behavioural bugs fixed, both worth 0 bytes

1. **`MidiInstrument::Pause` and `::ReleaseNote` were declared in the header and defined
   NOWHERE in the tree** — unresolved externals, the same hole W16-EH found for
   `Shuttle::SetActive`. Every `VocalGuidePitch` pause/note-release path was calling a
   function that did not exist.
2. **`Game::UpdatePausedState` never stopped the guide note.** Retail's tail at `0x8267AA48`
   runs `StopNote()` when **pausing** with movie-sync on (`bne` at `0x82361174` skips the
   spin-up loop; `beq` at `0x82361188` skips the block when not pausing). Ours omitted it,
   so the vocal guide-pitch note kept sounding across a pause. Measured 66.296 → **70.000**
   fuzzy, **+0 bytes** — the row is 972 B and cannot cross, exactly as pre-registered.

---

## 5. What a successor needs for the 1,664 B row

Start from W16-EH §3 (the case-by-case decode — it is good) plus this doc's §1, and accept
up front that:

1. **There is no oracle.** Budget for instruction-level reconstruction of ~416
   instructions, not for a port.
2. **Partial work banks nothing.** Do not price this row by fuzzy movement; 3.2 → 8.0 → 40
   are all worth 0 B. The only event that pays is `fuzzy == 100`.
3. **Remaining known unknown:** `fn_8274B0F8` is called **both** as `Array` and as `Int`
   (W16-EH §3.1) — unresolved, and it sits in the tail.
4. The `Symbol`s the body needs (`audition_jump_forward_ms`, `audition_jump_back_ms`,
   `audition_jump_end_buffer_ms`, `audition_cam_toggle`, `deploy_if_possible`,
   `audition_keyboard_synth_volumes`) exist in retail `.rdata` but must be **added to our
   Symbols tables** — only `deploy_if_possible` exists today.
5. **The highest-value work adjacent to this row is not the row.** Pulling one thread out of
   its decode (`fn_826C9160`) yielded +340 B and a completed unit in a single lane, because
   the `VocalGuidePitch`/`MidiInstrument` neighbourhood was full of declared-but-undefined
   methods and one wrong map name. There may be more of that class; there is very little
   more of the 1,664 B class.

## 6. What I did NOT do

- **Did not write any part of the 1,664 B switch tail.** With no oracle, all-or-nothing
  scoring, and an unresolved callee in the tail, a speculative body pays 0 B either way
  while adding a large untested body the native gate must carry.
- **Did not attempt the ButtonUp register rotation** (§2) — permuter is OFF by directive,
  and there is no source defect behind it.
- **Did not install any alias** and did not hand-edit `scripts/symbol_aliases.json`.
- **Did not resolve `fn_8274B0F8`** (`Array` vs `Int`).
- **Did not correct `ROADMAP_GAP_TO_TARGET_2026-09-01.md:2369`**, which W16-EH flagged as
  still repeating two refuted claims — another lane's dated record, outside my staged paths.
