# The 8 MIS-PIN SUSPECTS + `DuplicatedObject`, adjudicated on retail bytes — **3 real, 2 refuted, 3 undecidable** (lane L2-BODYTRIAGE, 2026-09-10)

> **VERDICT SUMMARY.** P1-BODYTRIAGE handed off "8 units / 43,092 B MIS-PIN
> SUSPECT" and `DuplicatedObject` (1% matched, +49 deficit) as the vein's
> *accuracy* payoff. Adjudicating all nine on retail bytes:
>
> | verdict | units | suspect B |
> |---|---|---:|
> | **MIS-PIN CONFIRMED** — pin carries a foreign TU | **AccomplishmentSongConditional**, **RockCentral**, **RetryAudioPanel** | **19,520** |
> | **REFUTED** — pin is correct, our SOURCE diverges | **VocalTrackDir**, **Font** | 11,204 |
> | **UNDECIDABLE** — no rare evidence exists either way | **Joypad**, **Debug**, **rnddx9/Rnd** | 12,368 |
> | **STRUCTURAL** — neither pin nor source is wrong | **DuplicatedObject** | (25,264 pinned) |
>
> ⇒ the detector's precision on this population is **3/8 = 37.5%** with **2/8
> actively wrong**, and `DuplicatedObject` is **not an accuracy target at all**:
> **100.0% of its 25,264 B of pins (12/12 blocks) lie inside the measured `/Od`
> band**, and our own source already carries a prior lane's note that retail's
> frameless single-word store *cannot be produced from C++*. There is nothing
> to adjudicate there.

Read-only lane. No `src/`, `splits.txt` or map edits; **no `ab_measure` run** —
see §6 for why that is a deliberate stop, not an omission.

## 0. Provenance

Worktree `~/tmp/wt-l2triage` off `dd4dd0ae`, rebased onto main `1b45fc5b`.
Full `./tools/ninja-locked` (rc=0) **before** any name-keyed analysis — a fresh
worktree's reflinked target objs are pre-renamer. Asserted against main:
`check_target_objs_renamed.py` reads **25462/28965 = 87.9% in both trees**.
`freshness.py::ensure_measurable` **PASS** (manifest 4,293 objects, split
current, tool identity OK, alias map not newer than the report).
`total_code` **10,245,956 B / 69,219 fns**, `matched_code` **3,774,924 B =
36.843%**, ruler `name_check` from `provenance.diff_config`.

The 8 flags reproduce p1's figure exactly: re-running
`nobody_contamination.py` on this tree gives **MIS-PIN SUSPECT 8 units /
43,092 B / 3.66%**, `UNDECIDABLE 44.24%`, and the flagged `outB` column sums
to **43,092 B** to the byte.

## 1. ⛔ The instrument p1 handed me is VACUOUS as used — two channels that look like TU evidence and are not

This is the load-bearing methodological finding, and it changed three verdicts.
The obvious way to adjudicate a suspect block is "what do its functions
reference?" — strings and callee names out of `fingerprints.json` +
`scripts/target_symbol_map.json`. Measured on this binary, **both channels are
dominated by artifacts**:

| artifact | measured | why it is not evidence |
|---|---:|---|
| `?OnHit@KeyboardTrackWatcherImpl@@...` @ `0x827a0108` | **644 callers** binary-wide | an **ICF fold survivor** wearing whichever spelling the linker's coin-flip kept. Reading it as "this block is track-watcher code" reads the linker's arbitrary choice as a fact about the TU |
| `ui/startup/eng/startup_autosave_esrb_keep.milo` | **1,740 functions** | common-pool string, not a per-function reference |
| `Assertion failed: %s (%s:%u)` | 456 | ditto |
| `timer_script` | 406 | ditto |
| `entersandman.mid` | 236 | ditto |

**Unfiltered, these produce confident mis-pin verdicts on three units.** My
first pass read `Rnd`, `Debug` and `RetryAudioPanel` as foreign-TU mis-pins
almost entirely off `KeyboardTrackWatcherImpl::OnHit` and the `esrb_keep.milo`
string. Both are noise; two of those three verdicts do not survive.

**The fix**: count a string only if it appears in **< 20** functions, and a
callee only if its target address has **fan-in < 20** *and* the name is
TU-owned (defined in exactly one of our base objs, no STL/template
instantiations). 99.4% of *distinct* strings are rare — the contamination is
that a handful of ubiquitous ones sit in almost every function's list.
Suppressed items are **printed with their frequency**, so the suppression is
auditable rather than silent.

**The control that makes the filtered reading legible** is the unit's own
TU-owned named rows put through the identical filter. It discriminates:

| unit | CONTROL votes (own named rows) | BLOCK votes (flagged rows) | overlap |
|---|---|---|---|
| AccomplishmentSongConditional | SongStatusMgr 6, **AccomplishmentSongConditional 6** | OvershellPanel 2, BandUserMgr 2, BandUser 1, OvershellSlot 1 | **none** |
| RetryAudioPanel | **RetryAudioPanel 6**, VoiceoverPanel 4, UIPanel 2 | NetSession 1, OvershellPanel 1 | **none** |
| VocalTrackDir | **VocalTrackDir 23**, BandTrack 6 | GemTrackDir 1, TrackDir 1 | TrackDir family |
| RockCentral | **RockCentral 10**, KeyboardController 3 | EntityUploader 1 | none |

A control that produced the same votes everywhere would prove nothing; these
are unit-specific, which is what licenses reading the block votes at all.

## 2. ★ 2 of 8 flags are DETECTOR ARTIFACTS — the hull test cannot be trusted on a FRAGMENTED pin

`nobody_contamination.py` flags a class-3 row that sits outside the address
hull of the unit's TU-owned **named** rows. Its own docstring is careful that
"outside the hull" is not sufficient. The measured failure mode is sharper than
that caveat:

**These pins are not contiguous TU spans. `splits.txt` gives RockCentral
**74 separate `.text` blocks spanning 0x82276CD0–0x826DA0E0 (4.6 MB)**,
VocalTrackDir 37, Font 32.** For such a unit the named-row hull is a 2.3 MB
interval whose endpoints carry no information, and "inside/outside" is decided
by whichever scattered block happens to hold a named row.

⇒ **`VocalTrackDir`'s flagged block IS its own dedicated `splits.txt` line**:

```
VocalTrackDir.cpp:   .text start:0x822ECC48 end:0x822EE498      <- the flagged block, exactly
```

A "mis-pin" verdict on a block that has its own dedicated pin entry is a
contradiction in terms. The block was flagged only because VocalTrackDir's
*named* rows all live in the 0x822EFB50–0x82303890 cluster and none reaches
down into this one.

**And the retail bytes agree with the pin, not the detector.** The 2,548 B
function at `0x822ECC48` references `front1.grp`, `lead_lyric_scroll.grp`,
`phoneme0.grp`, `phoneme2.grp`, `scroller.trans` — traced by string to
**`src/system/bandobj/VocalTrackDir.cpp` in our tree AND in rb3-Wii**. Same
story for `Font`: the flagged 2,140 B function at `0x82475A20` carries the
printable-ASCII charset literal
`` !"#$%&'()*+,-./0123456789:;<=>?@A–Z[\]^_`a–z{|}~ `` — present in
`src/system/rndobj/Font.cpp` in our tree and in dc3's — and calls
`KerningTable::GetKerning` and `RndFont::KernInfo` vector ops, with Font itself
the top block vote (11).

⇒ **REFUTED: 11,204 B of the 43,092 B is not mis-pinned.** These two are
ordinary body divergence in code we hold, i.e. they belong to the class p1
already priced at ~0 conversion.

## 3. MIS-PIN CONFIRMED — 3 units / 19,520 B

### 3a. `AccomplishmentSongConditional` — the cleanest case (4,700 B)

```
splits:  AccomplishmentSongConditional.cpp  .text 0x825EB0AC-0x825EC508   <- COVERS the block
hull  :  0x825EC4F8-0x8266A5F0  (14 TU-owned named rows)
block :  0x825EB0D0-0x825EBD50  (19 anon rows / 3,152 B of the 4,700 B outside)
```

The pin starts at `0x825EB0AC`; the unit's own named code starts at
`0x825EC4F8` — **the pin's lower ~5.2 kB precedes every identified
accomplishment function.** What lives there, on rare evidence:

| addr | B | rare evidence |
|---|---:|---|
| `0x825EBB08` | 584 | strings `bass`, `drum`, `keys`, `real_keys`, `vocals` |
| `0x825EB820` | 504 | string `modifier_changed_msg` |
| `0x825EB3F8` | 244 | `BandUserMgr::SetSlot`(fan9), `BandUser::SetControllerType`(fan3) |
| `0x825EB4F0` | 220 | `BandUserMgr::ClearSlot`(fan2) |
| `0x825EB0D0` | 208 | `OvershellPanel::GetSlot`(fan4) |
| `0x825EB1F8` | 192 | `BandUserMgr::GetLocalBandUsers` |

Instrument symbols + slot/user assignment + controller types. The control votes
(SongStatusMgr / AccomplishmentSongConditional — `CheckStarsCondition` →
`SongStatusMgr::GetBestStars`, etc.) and the block votes (OvershellPanel /
BandUserMgr / BandUser / OvershellSlot) are **disjoint**. This is
user/slot-management code, not accomplishment-condition code.

**Actionable shape:** trim the `.text` start from `0x825EB0AC` to the unit's
real first function (~`0x825EC4F8`), ceding the lower block to whichever
BandUserMgr/Overshell TU owns it. ⚠ Not metric-neutral — see §6.

### 3b. `RockCentral` — confirmed, but the block is MIXED (11,260 B flagged)

```
splits:  RockCentral.cpp  .text 0x82507634-0x8250B138  (15.3 kB)  <- COVERS the block
block :  0x82509530-0x8250A0C8
```

The decisive row is the **1,736 B function at `0x82509938`**, whose rare
strings are `file_get_base` and `file_get_path`. Traced to source, those are
not RockCentral strings at all — they are `DataRegisterFunc` registrations in
**`src/system/os/File.cpp`**:

```
File.cpp:420   DataRegisterFunc("file_get_path", OnFileGetPath);
File.cpp:422   DataRegisterFunc("file_get_base", OnFileGetBase);
```

So RockCentral's pin block reaches into `os/`-cluster code. ⚠ **Honest caveat:
the block is mixed, not wholly foreign** — the small 40 B thunks in the same
block call `RockCentralOpCompleteMsg`'s ctor/dtor (fan33, suppressed by the
rarity filter but class-specific), and `0x82509530` calls
`EntityUploader::BuildUpdateCharOp` (fan1). The most defensible reading is that
this 15.3 kB pin block **straddles a TU boundary** between RockCentral's tail
and `os/File.cpp`. **I did not isolate the exact boundary address**, which a
pin move would require.

### 3c. `RetryAudioPanel` — confirmed foreign, true owner not named (3,560 B)

```
splits:  band3/meta_band/RetryAudioPanel.cpp  .text 0x82630778-0x826319F4  <- COVERS the block
hull  :  0x82630228-0x82630630   (12 TU-owned named rows)
block :  0x82631130-0x826319D4   (15 rows / 2,176 B largest contiguous)
```

Rare strings in the block: `enable_retry`, `mod_auto_vocals`, `join_invite`,
`set_joining_user`, `check_disconnect`, `join_result`; rare callees
`NetSession::IsBusy`(fan3), `OvershellPanel::AttemptToAddUser`(fan2).

**`RetryAudioPanel.cpp` contains none of those message names, in our tree or in
rb3-Wii** (grepped; the names live only in `utl/Messages.h` / `utl/Symbols.h`,
i.e. the shared symbol declarations, so a source grep cannot name the handler).
The control is unambiguous the other way: `RetryAudioPanel` is a
`VoiceoverPanel` subclass — `??0RetryAudioPanel@@` → `??0VoiceoverPanel@@`,
`?RandomVOContextItem@` with string `vo_retry_context`. A panel that answers
`join_invite` / `join_result` / `check_disconnect` and adds users to an
Overshell on join is a **session/join panel TU**, not the retry-audio VO panel.
Candidate owners (unresolved): `JoinInvitePanel`, `SessionMgr`,
`OvershellPanel`.

## 4. UNDECIDABLE — 3 units / 12,368 B, and the honest reason

These are the units whose *entire* apparent evidence was the two artifact
channels of §1. After rarity filtering, nothing survives:

- **`Joypad`** (4,672 B). The flagged block is **one** 2,468 B function at
  `0x82526A00` with **zero** strings and **zero** named callees; the unit's own
  7 named rows also carry **no rare evidence**, so there is no control either.
  Pin `0x82524A08-0x825276EC` covers it. Undecidable on this instrument —
  needs Ghidra/decompiler work, not a string scan.
- **`Debug`** (3,556 B). Pre-filter it looked foreign (`band3`,
  `esrb_keep.milo`, `KeyboardTrackWatcherImpl::OnHit`). Post-filter only the
  `band3` string on a 492 B row survives, and the control has no rare evidence
  at all. **My own first-pass mis-pin verdict here was wrong.**
- **`rnddx9/Rnd`** (4,140 B). The only surviving rare callee is
  `StreamReceiver::New`(fan8) — audio streaming, not DirectX, which is
  *suggestive* of a foreign TU. But the control is a single named row
  (`DxRnd::D3DFormatForBitmap`) whose own "evidence" is the pool string
  `vocal_parts`. One weak signal against no control is not a verdict. ⚠ Note
  this unit is the tool's own motivating case — the 24,260 B `Validator.cpp`
  mis-pin it was built from has **already been fixed**; only 5,060 B of class-3
  remains here.

## 5. `DuplicatedObject` — NOT an accuracy target. It is `/Od`, and we already knew.

p1 handed this over as "1% matched with a +49 function deficit". Measured:

```
/Od band (CLAUDE.md lane CF-4):   0x82A6D168 - 0x82B54190
DuplicatedObject.cpp pins:        12 blocks, 25,264 B total
inside the band:                  25,264 B = 100.0%   (12/12 blocks)
```

Every byte of the pin is inside the region measured to be compiled `/Od` in
retail while our build is `/O1`. p1's §5 already established that **no source
is byte-faithful across that boundary** (with the controls that make it
readable: band3 0.0%, engine 0.2% in-band). And the refutation is *already
written in our own source* — `src/network/ObjDup/DuplicatedObject.cpp:58-67`:

> `…the retail /Od TU stores just the code-address word (one 4-byte store at
> this+4) … We cannot reproduce that exact frameless single-word store from the
> [C++ source]`

⇒ **the +49 deficit is not a body deficit to fill, and the 1% is not a source
defect.** Neither the pin nor the source is wrong. This unit should be recorded
as structurally unmatchable and removed from every worklist — it is the single
largest class-3 unit in the binary (24,188 B asm) and will keep re-surfacing at
the top of any size-ranked list that does not know this.

## 6. What I did NOT do, and why

- ⛔ **No `ab_measure`, no pin move, no `splits.txt` edit.** Three of the nine
  units have an actionable pin-trim shape, but **re-homing an already-pinned
  address is NOT metric-neutral** (measured +3 functions / +428 B, lane
  PINHOME-1) because objdiff pairs by name and re-homing changes *which base
  obj is consulted*. A trim also cedes bytes to a unit that may have no pin at
  all, and for `RockCentral` I could not isolate the boundary address. Landing
  a pin move on a boundary I had not isolated would be exactly the
  "confident number from an unverified premise" failure this lane exists to
  avoid. The three trims are specified above as *proposals with their evidence*;
  each needs its own A/B leg.
- **Did not name RetryAudioPanel's true owner**, only refuted RetryAudioPanel.
- **Did not isolate RockCentral's TU boundary** inside the 15.3 kB block.
- **Did not re-derive the `/Od` band boundaries** (inherited from CF-4, as p1
  also flagged); I tested pin membership only.
- **Did not adjudicate the 44.24% UNDECIDABLE population** the detector cannot
  reach — 520,492 B, still a lower-bound problem, and §2 now suggests the
  fragmented-pin geometry makes that bucket structurally hard rather than
  merely unmeasured.
- **Did not run objdiff per row**, so no claim here is that any specific row
  *would* cross after a pin fix.
- **Did not run `tools/native_build_gate.sh`** — correctly: no `src/` file moved.

## 7. Corollary for the detector

`nobody_contamination.py` is not wrong, but its output should not be read as a
unit-level verdict. Two cheap improvements, both implied by §2:

1. **Suppress the flag when the flagged block is itself a dedicated
   `splits.txt` `.text` entry.** That single check removes the `VocalTrackDir`
   false positive outright, and is a pure lookup.
2. **Report the unit's pin FRAGMENTATION** (block count and span) beside the
   hull. A unit with 74 blocks over 4.6 MB has no meaningful hull, and the
   verdict should say so rather than computing one.

And the general rule this lane re-earned: **a string or callee name is evidence
only in proportion to its rarity.** An ICF fold survivor with 644 callers and a
pool string in 1,740 functions are both *shaped exactly like* decisive TU
evidence, and both point wherever you were already looking.
