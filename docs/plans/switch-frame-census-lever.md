# The switch-frame lever, automated — `scripts/harvest/switch_frame_census.py`

**Session:** lane J, 2026-07-25. Base main `ca711730` = **27,223** strict.
**Result: 27,226 (+3, 0 lost)**, plus `SetState` 90.3 → 91.69% fuzzy.
Worktree `~/tmp/wt-laneJ-switch`, branch `laneJ-switch`.

Prior context: `docs/plans/slm-setstate-reconstruction.md` (§7 states the lever,
§11 lists `SetState`'s +0x10 as an open item — this session closed it).

---

## 1. The lever

MSVC X360 at `/O1` gives **every `case` body its own stack slots and never
reuses them across arms of a switch.** A switch function's frame is therefore a
*census of its arms*, and a frame delta is a direct read-out of extra or missing
case bodies.

This matters because the frame is what flips a function's **EH funclets**. A
funclet's prologue is literally `subi r31, r12, <parent frame>` — it encodes the
parent's frame size, and the unwind data encodes the parent's saved-register
range. So:

> **funclet trigger = (frame immediate) AND (`__savegprlr_N` range)** — and,
> for funclets that clean up a specific local, **the slot offset** of that local.

Session 2 flipped **145 funclets in one build** on `GetDialogMsg` from a 6-line
diff, with the body still at 0%. That is the payoff shape: the parent can stay
broken and you still bank every funclet it owns.

Until now the analysis was manual asm archaeology. This script does it.

---

## 2. The tool

```
scripts/harvest/switch_frame_census.py --project-dir <worktree> <mode>

  census   --va 0x82550880 [--symbol '?Foo@@...']   retail-vs-ours slot census
  find     [--limit N] [--max-percent P]            scan for switch candidates
  funclets --va 0x… [--apply]                       harvest funclet map entries
```

**Retail side** is read straight out of the extracted PE `orig/45410914/band.exe`
— PE section table → VA↔file-offset map, prologue decode, jump-table walk. No
Ghidra, no objdiff, no `.s` files.

**Our side** is read out of the compiled COFF object
`build/45410914/src/<unit>.obj` with the same decoder, so both sides go through
one code path.

`census` prints: frame / `__savegprlr_N` / `subi r31` deltas; the jump-table arm
census (labels → blocks, shared blocks); an **arm-order** diff; the **slot
ladder** with per-slot arm attribution; and a **ranked list of concrete source
edits** ("MERGE arms 2A 3A into one body", "DELETE case 0x15", "case 0x33: our
arm touches +0x10 more frame than retail").

---

## 3. What the slot ladder actually looks like — MSVC X360 `/O1`

Empirical, from `SetState` (107 arms), `GetDialogMsg` (98), and
`FactoryCreateAccomplishment` (12).

### 3.1 Frame regions

```
0x000 .. ~0x50   linkage + outgoing-argument area   (shared by all arms)
~0x50 .. frame   per-arm locals, one region per arm, NEVER reused
```

`r31` is the frame pointer and equals the *new* `r1`: the prologue is
`subi r31, r1, N` **before** `stwu r1, -N(r1)`, so `0x54(r31)` and `0x54(r1)`
name the same byte. Slot offsets are directly comparable between the two sides.

### 3.2 The ladder is per-arm and ordered

Retail `SetState`, locals region, with the arm that owns each slot:

| slot | size | owning arm(s) |
|---|---|---|
| `0x50` | 4 | case `0x57` |
| `0x54` | 4 | cases `0xB`, `0x64` |
| `0x58` | 8 | case `0x53` |
| `0x60` | 8 | case `0x56` |
| `0x68` | 4 | case `0x56` |
| `0x6c` | 4 | case `0x13` |
| `0x70` | 8 | case `0x38` |
| `0x78` | 8 | case `0x38` |
| `0x80` | 48 | case `0x21` (`BufStream`) |
| `0xb0` | 192 | cases `0x33`, `0x3E` (`FixedSizeSaveableStream`) |

Two arms sharing a slot (`0xB`/`0x64`, `0x33`/`0x3E`) are arms whose *bodies are
the same block* — see 3.4.

### 3.3 Slot order follows **physical block order**, which is **source order**

MSVC emits case bodies in source order, and hands out slots in that order. So a
slot-ladder *permutation* (same sizes, different owners) is a **source ordering**
bug, not a missing/extra body. `census` reports this as its own ARM ORDER
section:

```
  2 of 62 shared arms are out of order.
    slot   6: retail 9  vs ours 8
    slot   7: retail 8  vs ours 9
```

Swapping `case 0x9` above `case 0x8` in `SaveLoadManager::SetState` took it from
90.3% to 91.69%. This is a mechanical, behaviour-neutral edit.

### 3.4 Shared arm blocks are a real source pattern

When the jump table points **two or more labels at one block**, retail's source
wrote `case A: case B:` — one body, one slot set. Splitting them in our source
duplicates the slot set and inflates the frame. `census` lists every retail
shared block; `GetDialogMsg`'s +0x30 was exactly this (`0x2A`/`0x3A`).

`SetState` has 13 shared blocks, the largest being 29 labels → one
`TriggerEvent(saveload_dialog_event, NULL)` block.

### 3.5 Jump-table forms seen

| form | dispatch | note |
|---|---|---|
| 2-byte offsets, ×1 | `slwi r0, idx, 1 ; lhzx r0, tbl, r0 ; add r12, base, r0` | `SetState`, `GetDialogMsg` |
| 1-byte offsets, ×4 | `lbzx r0, tbl, idx ; rlwinm r0, r0, 2, … ; add r12, base, r0` | `FactoryCreateAccomplishment`, `UIStats` — the byte stores an *instruction* count |

Both are preceded by `cmplwi crN, idx, BOUND ; bgt default`, optionally with a
`subi idx, idx, BIAS` first (`GetDialogMsg`'s state space is 6..0x67). `base` is
materialised by a `lis/addi` pair and is the instruction right after `bctr`.

Arm *count* changes the lowering shape, not just the frame: adding
`GetDialogMsg`'s missing 30th label is what tipped MSVC from a binary-search
chain to a jump table.

### 3.6 Local-static `Symbol`s widen the saved-register range

Confirmed again this session on `FactoryCreateAccomplishment`. Retail declares
`static Symbol accomplishment_type("accomplishment_type")` **inside** the
function (storage `0x82E0DABC`, guard `0x82E0DAC0`) instead of referencing the
`Symbols.h` global. The local static's storage address gets pinned in a
callee-saved register (`r28`), so the function saves from `__savegprlr_28`;
the global-Symbol form only needs `_29`. Frame identical, everything else
identical — one register of save range.

---

## 4. What `find` found, and the honest scope

28 switch functions in pinned in-scope units (`band3/`, `network/`, `system/`,
root game TUs; `auto_03_*` XDK/Quazal excluded per the project owner's hard
skip), `--min-size 0x100`. Ranked by **open** EH funclets (see §5.1):

| VA | unit / symbol | frame | sgpr | arms | open/tot funclets | % |
|---|---|---|---|---|---|---|
| `0x82563428` | `band3/meta_band/UIStats.cpp` (unmapped) | 0x70 | 29 | 14 | 4/7 | – |
| `0x82550880` | `SaveLoadManager::SetState` | 0x170 | 24 | 107 | 3/5 | 91.7 |
| `0x82553490` | `SaveLoadManager` `Poll` (unmapped) | 0x130 | 29 | 107 | 2/2 | – |
| `0x8274b980` | `DataNode::Load` | 0x90 | 28 | 38 | 2/6 | 100 |
| `0x825965e8` | `SongSortMgr.cpp` (unmapped) | 0xc0 | 26 | 11 | 1/1 | – |
| `0x82551d80` | `SaveLoadManager` (unmapped) | 0x90 | 26 | 108 | 1/5 | – |
| `0x8254cc98` | `SaveLoadManager::GetDialogMsg` | 0x4c0 | 27 | 98 | 0/146 | 100 |
| `0x82556038` | `AccomplishmentManager::FactoryCreateAccomplishment` | 0x80 | 28 | 12 | 0/47 | 100 |

**The pool is small and mostly already banked.** That is the honest headline:
the two giants that carried this lever (`GetDialogMsg` 146 funclets,
`FactoryCreateAccomplishment` 47) are now closed, and the residue is single-digit
funclet counts. The lever is not exhausted — it will refill as more game TUs get
pinned — but it is not a large standing vein today.

`0x82563428` (UIStats) is the top remaining candidate and is **not implemented in
our source at all** (no switch in `src/band3/meta_band/UIStats.cpp`, no map entry
in that range) — it is a body-port job, not a frame-census job.

---

## 5. Failure modes hit this session (read before reusing)

### 5.1 ★ Naming an already-matched funclet is a REGRESSION (measured −13)

objdiff **already pairs EH funclets positionally while they are anonymous**. A
funclet whose body matches is banked as `fn_XXXXXXXX` with no map entry at all.
Adding a `target_symbol_map.json` entry for it breaks that pairing.

`funclets --apply` on `FactoryCreateAccomplishment` emitted 13 verified
byte-identical entries and cost **−13** (27,226 → 27,213, LOST 13, all of them
exactly the funclets named). Reverted.

Both modes now consult `report.json`: `funclets` refuses to emit an entry for
anything already at strict 100% anonymously, and `find` ranks by **open**
funclets rather than total. Only the OPEN column is available work.

### 5.2 Funclet attribution in `find` is by frame size within the unit

There is no parent pointer in `.pdata` we decode, so `find` attributes a funclet
to a parent by matching `subi r31, r12, N` against the parent's frame within the
unit's pinned spans. Where two switch parents in a unit share a frame size the
row is flagged `~`. `FactoryCreateAccomplishment`'s "47" is over-attributed —
its COMDAT actually owns 13.

### 5.3 COFF function size must come from `.pdata`, not the next symbol

MSVC puts the function, its EH funclets, `$M` line markers and `__unwind$` data
in **one COMDAT section**, so "distance to the next symbol" gives nonsense
(0x10 for a 0x107c function). The correct source is the associated `.pdata`
`RUNTIME_FUNCTION`: `BeginAddress`, then **LSB-first** bitfields packed in a
big-endian dword — `PrologLen:8, FunctionLen:22, ThirtyTwoBit:1, ExceptionFlag:1`
— and the entry belonging to the function's own COMDAT is the one whose raw
`BeginAddress` (the reloc addend) is 0. `FunctionLen` is in instructions.

### 5.4 `IMAGE_REL_PPC_PAIR` must be skipped

The `lis`/`addi` pair that materialises a jump table carries reloc types
`0x10` (HI) and `0x11` (LO), **each followed by a `0x12` PAIR record at the same
VirtualAddress** pointing at `@comp.id`. Keying relocs by address without
skipping PAIR silently resolves every jump table to `@comp.id`.

### 5.5 Retail call names are anonymous

Per-arm call *names* cannot be compared (retail is `fn_XXXXXXXX`, ours is
mangled), so `census` compares call **counts** and prints retail's callee VAs for
you to identify. Do not read "calls retail makes we don't" into name diffs.

### 5.6 The usual

`rm -f build/45410914/report.cache` before **every** report read; full
`./tools/ninja-locked`; `touch config/45410914/config.yml` after editing
`target_symbol_map.json` so the target-symbol renamer re-runs.

---

## 6. Ledger

| commit | change | fn | whole-binary | lost |
|---|---|---|---|---|
| `14c6b5dc` | tool + `SetState` case `0x15`/`0x16` → `SetState(0x19)` | frame **exact** 0x170 | 27,225 | 0 |
| `b191a001` | `FactoryCreateAccomplishment` local-static Symbol + reveal; `funclets` mode + regression guards | **byte-identical** | 27,226 | 0 |
| *(this)* | `SetState` case 9 before case 8 (retail source order) | 90.3 → 91.69% | 27,226 | 0 |

`SetState`'s +0x10, open since session 1, was **case `0x15`/`0x16`** — a shared
arm in both builds where retail's whole body is `li r4, 0x19 ; b <shared SetState
tail>` (i.e. `case 0x15: case 0x16: SetState((State)0x19); break;`) while ours
called `TheCacheMgr->AddCacheID(mCacheID, unk4c.c_str())` first, costing a
16-byte temp. §11 of the SLM doc guessed the case-`0x38`/`0x56`/`0x57` vector
arms; **that guess was wrong** and the tool named the right arm in one run.

## 7. Next

1. `SetState`'s last 3 funclets need slots `0x54` (cases `0xB`/`0x64`) and
   `0x60` (case `0x56`) to line up — a further arm/local ordering problem, see
   §3.3. The ladder in `census` is the map.
2. `0x82553490` (`SaveLoadManager` `Poll`, 107 arms, unmapped) — apply the same
   recipe: reveal, census, close the frame.
3. `0x82563428` (`UIStats`) needs the body written first.
4. Re-run `find` after every wave of new game-TU pins; the pool grows with them.
