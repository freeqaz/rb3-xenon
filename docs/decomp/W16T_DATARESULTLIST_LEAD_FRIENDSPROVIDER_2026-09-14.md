# W16-T — the `DataResultList` funclet lead, `FriendsProvider`, and the `Accelerometer` naming row

**Lane W16-T (Opus), 2026-09-14.** Worktree `~/tmp/wt-w16-t`, branch `w16-t`, off `main` at `94b0ddae`.
Takes the four source-shaped leftovers from W16-J's `W16D_BLOCKED_LIST_ESCALATION_2026-09-14.md`
(§"Deliberately NOT done"). Every number is read from `build/45410914/report.json` after a **full**
`./tools/ninja-locked` with `rc` tested from the log file, ruler **`name_check`** (`objdiff.json`
`options.functionRelocDiffs = name_check`, objdiff 4.2.8 `032122696555`), and every price is a
**set-diff of the `fuzzy == 100` row set**, never a Δcount and never a mismatch count.

**Lane baseline (build 1, full, rc=0):** `matched_functions` **43,272** / `matched_code` **3,960,812 B**
/ `matched_code_percent` 38.65732 / fuzzy 49.479626, `total_functions` 69,216, `total_code` 10,245,956.

---

## Item 1 — the two `DataResultList` funclets: **REFUTED. Our source is already correct; the charge is an objdiff funclet-PAIRING artifact**

**The lead as briefed.** W16-J's naming of `0x8250b510` = `??1DataResultList@@UAA@XZ` turned two funclet
call sites from forgiven into checked. Both read `fuzzy 99.5 / mpn 100`, 40 B:
`default/MetaPerformer` `fn_825800B0` (ours appeared to destroy a `Message` at frame `+0x84`) and
`default/RockCentral` `fn_824F9BAC` (ours appeared to destroy a `DataArrayPtr` at `+0x50`). The brief's
hypothesis: **a wrong local type in our port** — a real behavioural defect, the wrong destructor running
on unwind.

**That hypothesis is refuted on retail bytes. Our source declares the right type in both places.**

### The retail reading

An EH funclet unwinds one local of its parent, and MSVC emits it **inside the parent's own COMDAT**, so
the parent is the function contiguously preceding it. Both parents resolve exactly:

| retail funclet | retail parent (contiguous) | what the parent does with that slot |
|---|---|---|
| `fn_825800B0` (40 B) | `fn_82580068` (72 B) — a destructor | `stw r3, 0x84(r31)` stores `this`; then `addi r3,r3,0x1c; bl fn_8257FE60` destroys a sub-object at `+0x1c`; then `mr r3,r30; bl fn_8250B510` = `~DataResultList` on `this` |
| `fn_824F9BAC` (40 B) | `fn_824F9B68` (68 B) | `addi r3,r31,0x50; bl fn_8250B410` **constructs** a `DataResultList` local at frame `+0x50`, passes `&it` as arg 3 to `fn_824F9620`, then `addi r3,r31,0x50; bl fn_8250B510` destroys it |

The funclet reads **the very frame slot its parent writes** (`+0x84` written by `stw`, read by `lwz`;
`+0x50` constructed into, destroyed from) — so the parent attribution is not an inference, it is the
same slot on both sides.

### Our side — the matching funclets exist, with retail's exact bytes and retail's exact callee

Walking the COFF symbol tables of our own compiled objects (`__unwind$NNNNNN` symbols, each in the same
section as its parent's external symbol):

| our funclet | its section's parent symbol | its relocation | vs retail |
|---|---|---|---|
| `__unwind$366621` (MetaPerformer.obj) | `??1PendingDataInfo@MetaPerformer@@QAA@XZ` | `??1DataResultList@@UAA@XZ` | **9/9 words identical** to `fn_825800B0` |
| `__unwind$325531` (RockCentral.obj) | `?RecordDataPointNoRet@RockCentral@@SAXAAVDataPoint@@H@Z` | `??1DataResultList@@UAA@XZ` | **9/9 words identical** to `fn_824F9BAC` |

And **both parents already score `fuzzy 100.0 / mpn 100.0`** in `report.json`
(`??1PendingDataInfo@MetaPerformer@@QAA@XZ` 72 B = retail's 72 B; `?RecordDataPointNoRet@RockCentral@@…`
68 B = retail's 68 B). Under `name_check` a parent at 100 means its own `bl ??1DataResultList`
**relocation name** already agrees with retail's. There is no wrong local type to fix.

### What is actually charged: an ambiguous byte-signature equivalence class

objdiff pairs these anonymous funclets by **funclet byte signature** (the mechanism behind
`masked_equal`), not by name — our side has no `fn_<addr>` spelling for them. Within one signature the
pairing is a many-to-many assignment, and it is resolved arbitrarily:

- MetaPerformer, signature `subi r31,r12,0x70` + `lwz r3,0x84(r31)`: retail has **1** such funclet;
  **our object has 4** (two calling `??1Message`, one `??1_String_base`, one `??1DataResultList`).
  objdiff chose a `Message` one. `objdiff-cli diff` confirms the single charge:
  `bl ??1DataResultList@@UAA@XZ` (target) vs `bl ??1Message@@UAA@XZ` (base), `diff_arg`, **all 10
  instructions equal**.
- RockCentral, signature `subi r31,r12,0x90` + `addi r3,r31,0x50`: retail has **5**
  (`fn_823F0F5C`, `fn_824F8F24`, `fn_824F9BAC`, `fn_82508E28`, `fn_826D0F70`); our object has ≥5
  (three `??1DataArrayPtr`, one `??1String`, one `??1DataResultList`). **All five retail rows read
  99.5** — i.e. the assignment is wrong for **0/5 correct**, which is what an arbitrary assignment
  inside an equivalence class looks like, and is not a property of `DataResultList`.

### Size of the real class (measured, whole binary)

Rows at `mpn == 100` **and** `fuzzy < 100` — counted in `matched_functions`, withheld from
`matched_code`:

| class | rows | bytes | pp of `total_code` |
|---|---:|---:|---:|
| all `mpn==100 & fuzzy<100` | 3,366 | 204,884 | 2.0000 |
| of which **size 40 (EH funclets)** | 1,842 | 73,680 | 0.7191 |

(Next sizes: 44 B ×909, 48 B ×389 — the same funclet shape with one or two extra instructions.)

### Verdict and what was NOT done

**No source change is correct here, and two of the three obvious "fixes" would be harmful:**

- Changing our local's type to `Message`/`DataArrayPtr` to chase the pairing would **introduce** the
  behavioural defect the brief was trying to remove, and would break the two parents that are at 100.
- An alias `??1Message` ≡ `??1DataResultList` is forbidden by the brief and is alias fabrication: the
  two destructors are genuinely different code, so it would be forgiveness bought with a false claim.
- Un-naming `0x8250b510` would hide the charge without changing anything; W16-J named it deliberately.

The defect is in the **funclet pairing**, which lives in `../objdiff` (a tiebreaker on the relocation
target name within a byte-signature equivalence class would resolve it deterministically). That is a
shared-toolchain change and explicitly outside this lane's scope. Recorded here with its size so a
tooling lane can price it: **up to 73,680 B at 40 B alone, 204,884 B for the whole class**, unreachable
by any amount of source work.

**Δ measured: 0 (no change made).** The deliverable of this item is the refutation itself — it closes a
lead that would otherwise have cost an escalation lane a correctness regression.
