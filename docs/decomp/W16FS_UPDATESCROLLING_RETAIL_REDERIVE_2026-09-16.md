# `?UpdateScrolling@VocalTrack@@QAAXM@Z` (8,948 B) re-derived from retail bytes: 72.28 → 91.69 fuzzy, size now identical, NOT crossed — lane W16-FS, 2026-09-16

Branch `w16-fs` off main `44213c04`. Worktree `~/tmp/wt-w16-fs`. Ruler `name_check` (graded), every
score read from `report.json`. Only `src/band3/bandtrack/VocalTrack.cpp` is touched.

## 0. Headline

| state | fuzzy | mpn | our size | retail | bytes banked |
|---|---:|---:|---:|---:|---:|
| main `44213c04` (brief's figure, re-measured) | 72.27805 | 72.27805 | 8,968 | 8,948 | 0 |
| C18 `86763444` (highest fuzzy) | **91.69379** | 92.64372 | 8,960 | 8,948 | 0 |
| C19 `4d47415c` (branch tip; retail-size, retail loop skeleton) | 91.32990 | 92.23514 | **8,948** | 8,948 | 0 |

- **Not crossed.** `matched_code` is all-or-nothing per row; 91.7 % of 8,948 B pays exactly zero. Whole-binary keys were
  unchanged at every one of 19 full builds: `44040 / 4150676 / 40.50599 / total_code 10247068 / total_functions 69240 /
  masked_equal 23246` (global fuzzy 50.380030 → 50.395966 — that is the row itself moving, nothing else).
- **BP4's wall (`docs/decomp/research/2026-06-11-bp4-vocaltrack.md`, 52.34, "161 clusters, frame −48") is stale on its own
  terms**: the frame is now byte-identical (`stwu r1,-0x370` both sides, since C15), the row is 19.4 pp higher than the wall,
  and the size is exactly retail's. It was a starting map, not a refusal, and it is now superseded by this doc.
- **Method that moved it**: read retail's assembly row-by-row (annotated side-by-side, §3), use the oracle only for names,
  pre-register every build, and refuse to keep an edit whose only evidence is "the oracle says so". **Three more instances
  of "the oracle is the defect"** were found in this one function (§2.2).

## 1. Trajectory (every number is a full `./tools/ninja-locked` build + `report.json`; pre-registrations in `~/tmp/w16fs_prereg.md`)

| exp | change | fuzzy | kept |
|---|---|---:|---|
| A | phrase loop `for(;;){if(!c)break;}` → `while (c)` | 72.27805 (identical) | yes (neutral) |
| B | inline the deploy-zone min-select at every use | 59.764 (−12.5) | reverted |
| B2 | per-REGION `int *curDeployPtr` (retail evaluates the select exactly 7×, census 7==7) | 74.038 | yes |
| C1 | phrase-loop header as `for(;;){idx=*p; if(idx>=size) break;}` | 73.417 | reverted |
| C3 | `tmpEndPos` loop-persistent | 73.723 | reverted |
| C4 | pass the iterator by reference, read `->mMs` after the call via a pre-call pointer copy | 75.007 | yes |
| C5 | drop cached `notesEnd`/`altEnd`, compare against `.end()` at every site (retail re-reads `lwz r10,4(r19)` per site) | 77.902 | yes |
| C6 | `2.0f*margin` → `margin*64.0f` | 77.642 | reverted |
| C8a | static-lyrics test respelled | 77.902 (bit-identical) | yes (neutral) |
| C11 | ternary loop-exit | 77.468 | reverted |
| C13 | lyricPhrases reference-ternary (`+0xc` per arm) | 74.515 (see prereg: batched with a regression later split out) | partially |
| C15 | hoist `plates.end()` into the for-init | 75.129 | yes |
| C15b | `if (plate->Empty()) break;` (Baked stays `continue`) | **88.697** (+13.6) | yes |
| C16 | batched fixes (prereg L403) | 90.562 | yes |
| C17 | `lbz 0x2e` bool + slot 0x10c | 90.561 | yes (correctness) |
| C18 | RangeShift fields under their real names; drain loop passes TO; `<= 0`; tambourine iterator loop; LyricShift local | **91.694** | yes |
| C19 | LyricShift drain loop: `Vector3 pos(v); xPos = shift.unk0; pos.x = xPos; SetLocalPos(pos); shiftedX = pos.x + nowbar` | 91.330, **size 8,948 == retail** | yes (own commit, revertable) |

Eight of the nineteen were reverted; each revert is recorded with its measured number and the falsifier that fired.

## 2. What was learned about this function

### 2.1 Loop forms F1/F2/F3 and why the phrase loop cannot be matched by spelling

F1 = `T: test; bcc EXIT; body; b T` (retail's phrase loop). F2 = `b TEST; BODY; TEST: test; bcc BODY`. F3 = peeled top
guard + duplicated bottom test (ours, for both big loops). A micro-TU harness (`~/tmp/w16fs_micro`, 40+ variants:
`continue`, body size, calls, `divw`, breaks, goto, `for(;;)`, ternaries, `#pragma optimize` legs) produced F3 for **every**
loop-local spelling. A binary-wide census of 318 `divw`-terminated `size()` latch loops finds **exactly three F1 loops in
retail RB3** — this function, a `/10` digit loop in Locale, and one inside the measured `/Od` Quazal region — and one in
all of DC3. ⇒ retail's shape here is the anomaly; its cause is function-level (something about the whole TU/function
state), not a loop-local construct, and no loop-local spelling can produce it. The RangeShift drain loop is the same story
in miniature: retail's peeled copy and latch copy use different scratch registers (r8/r10) so they do not tail-merge; ours
are regalloc-identical and do (F2) — a post-regalloc artifact, not directly controllable.

### 2.2 "The oracle is the defect", instances #9–#11 (all in `UpdateScrolling`, all fixed in C18)

- #9 `RangeShift` fields were read under the wrong member names (offsets right, names wrong — only visible in this
  function because it is the only reader of the struct through those names).
- #10 the RangeShift drain loop called `SetRange` with the FROM pair where retail passes the TO pair (`rs.unkc, rs.unk10`).
- #11 `rangeDelta < 0` where retail tests `<= 0`.
Each was found by reading retail bytes against the oracle text, and each is a behavioural bug the oracle (rb3-Wii DEV
build) carries.

### 2.3 The C19 mechanism — a store-forwarded copy is what `fmr f31,f0` means

Retail's LyricShift drain loop is `lfs f0,0(r10); 4× lwz v(r26); stfs f0,0(r25); lbz 0x9c(r26); fmr f31,f0; pos stores;
copy-back; bl SetDirty_Force; fadds f0,f0,f31; stfs f0,0(r27)`. Standalone `/FAcs` probes (`~/tmp/w16fs_real/var/`):

| variant | second read of the value | listing |
|---|---|---|
| V1/V14 `pos.x = shift.unk0` | reloads `lfs fr31,0(r10)` after the `xPos` store (may alias) | no `fmr` |
| V16 `shiftedX = xPos + …` | reloads `0(r27)` after the call, r25/r26/r27 reshuffled | no |
| **V15** `xPos = shift.unk0; pos.x = xPos; … shiftedX = pos.x + …` | store-forwarded into a fresh temp = `fmr fr31,fr0` | **retail skeleton, extent 0x22f4 = 8,948 B** |
| V17 `pos.x = xPos = shift.unk0` | identical to V15 | schedule not spelling-sensitive |

The `Vector3 pos` construction must precede the `xPos` store (MSVC cannot disambiguate `*r25` from `r26->v`, so source
order is preserved) — V14/V15 both reproduce retail's loads-before-store. The phi hypothesis for `fmr` was refuted first
(the interp block reloads `lfs f12,0(r25)` and defines f31 fresh).

**Why fuzzy still fell 0.36 pp**: inside that block retail issues the `lfs` first and stores `pos.z` before x/y, and the
copy-back is z-first; the aligner renders our x,y,z,w order as insert/delete pairs (149 → 162) which cost more than the
`diff_arg` rows they replaced (443 → 414). Non-equal rows fell 618 → 595. C19 is kept as its own commit because size
identity and the `fmr` skeleton are retail-byte witnesses of the source form; `git revert 4d47415c` returns to C18 if
fuzzy alone is the criterion.

### 2.4 The frame is a PERMUTATION of ours, not a shift (new, unexploited)

Both frames are `-0x370`, but slot assignment differs: retail `0xc0` ↔ ours `0xb4` (L1415–1449), retail `0xb0` ↔ ours
`0xd0` (L1559/L1890), retail `0x80` ↔ ours `0x170` (L1818), retail `0xd0..0xd8` ↔ ours `0xc0..0xc8` (deque temps, L1334/
1388/1910), retail `0x170` ↔ ours `0x160` (`pos`). Our listing's frame table (`~/tmp/w16fs_real/var/VocalTrack_v15.asm`
PROC head) shows the 0x10 displacements come from `staticFirst`/`beginPos` being allocated after the temps on our side.
`MSVC_X360_REGALLOC.md`'s corrected rule says declaration order controls **stack slots** — so reordering the declarations
of `staticFirst/staticLast/staticLeftX/staticY`, `beginPos`, `isolated`, `lyricPhrases` is the lever for the **86
frame-slot rows** in §3. Not attempted (§5).

Also new: retail row 26 spills `stfs f0,0x160(r1)` at function entry (L1229) and reads it at L1862 where we re-read the
member `lfs f11,0x2e8(r31)` — retail caches a `this` float at entry. Not attempted.

### 2.5 Other mechanisms recorded in the prereg file

Value-numbering-order artifact (`fsubs f31,f13,f0` vs ours `f0,f13` with the two loads swapped); deque `size()` computed
from a stack copy of `_M_start` on both sides; our `clrrwi r11,r7,0` copy-propagation where retail reloads `lwz 0(r30)`;
retail forms `&freestyles` late (right after the `tmpEndPos` store) into r15 while ours forms it at declaration and spills;
retail keeps `notes` dead after row 987 while ours keeps it live across the loop (source spells `notes->mNotes` at every
site). `~/tmp/w16fs_wii.cpp` is NOT the Wii oracle (an older copy of our own function) — do not cite it.

## 3. Charged-site inventory at C19 (from the graded `objdiff-cli diff` dump, agreeing with `report.json`'s 91.3299)

| class | rows | example |
|---|---:|---|
| regalloc (same shape, different register) | 204 | `fmr f17,f1` vs `fmr f16,f1` |
| other-arg (incl. relocation-name sites) | 115 | `lfs f16, lbl_82000D78@l` vs `lfs f17, __real@00000000@l` |
| frame-slot | 86 | `stfs f28,0x144(r1)` vs `0x130(r1)` |
| delete (retail-only) | 81 | `stfs f0,0x160(r1)` at entry |
| insert (ours-only) | 81 | `lwz r7,0x0(r10)` |
| replace | 16 | `fsubs f31,f13,f0` vs `subf r11,r11,r10` |
| branch-target | 9 | |
| diff_op | 3 | `bne` vs `beq` |
| **total charged** | **595** | of 2,318 aligned rows |

Regalloc is the plurality and is downstream of the structural rows (the phrase-loop F1/F3 divergence alone drives most of
the 204). The relocation-name class is small; no alias/map work is implicated.

## 4. What remains, priced

Crossing pays 8,948 B and requires all 595 rows. The structural residue is: the phrase loop F1-vs-F3 (function-level
cause, unsolved after 40+ micro-variants and a binary-wide census), the frame permutation (§2.4, a concrete lever, 86
rows), the entry-cached `0x2e8` member (§2.4), the RangeShift F3 latch, the intra-block schedule in the C19 loop, and
clusters K (L1878/L1943 bound checks, `lfsx` vs `lfs`), J (L1855–1857), I (L1781 memory-resident lyricX), the deploy loop
(L1282/1284/1303/1317), N, P. **This is not a priced refusal in the CUSTOMIZE sense** — two concrete, untried levers remain
(§2.4) — but the phrase-loop form is a wall of the CUSTOMIZE kind: measured inert across every loop-local spelling, with a
census showing retail's shape is unique in the binary. Evidence that would reopen it: any `/O1` TU in retail RB3 or DC3
with an F1 `size()` loop whose source is known.

## 5. What this lane deliberately did NOT do

- Did not run the permuter (directive: OFF). Did not try scheduler-perturbing spellings beyond V17 in the C19 block.
- Did not try the frame-permutation lever (§2.4) or the `0x2e8` entry cache — found in the last hour, unbudgeted.
- Did not edit `VocalTrack.h`, `BandSongMetadata.*` or `PracticeSection.*` (scope fence; lanes W16-FT/FU concurrent).
- Did not merge or push; branch `w16-fs` stays for the coordinator (`git merge --no-ff`).
- Did not report fuzzy movement as bytes: **0 B banked**.
- The native gate was run AFTER this document's commit, as the lane's final action; its `NATIVE_GATE_RESULT` line is in the
  lane report, not here.

## 6. Ledger (`git log --oneline main..w16-fs`)

    4d47415c VocalTrack::UpdateScrolling C19: LyricShift drain loop builds pos first, reads xPos back
    86763444 VocalTrack::UpdateScrolling C18: read RangeShift fields under their real names
    a3112408 VocalTrack::UpdateScrolling: retail tests mBends and calls IsScrolling (C17) -- two named sites closed, fuzzy flat
    e1ebb19e VocalTrack::UpdateScrolling: retail calls Player::IsNet, not InTambourinePhrase (C16) -- fuzzy 88.697 -> 90.562
    838f1912 VocalTrack::UpdateScrolling: plates bake loop -- hoist end(), Empty() is a break (74.515 -> 88.697)
    f425aece VocalTrack::UpdateScrolling: bind noteVec/freestyles at retail's points (C13) -- fuzzy 77.902 -> 74.515
    ee31d941 VocalTrack::UpdateScrolling: C8a spelling of the skip-loop condition (codegen-identical)
    29a1fc52 VocalTrack::UpdateScrolling: compare against mNotes.end() directly; block-local alt end only where retail holds it across calls (Exp C5)
    87c354b6 VocalTrack::UpdateScrolling: pass the iterators to CreateLyric directly; loop-persistent tmpEndPos (Exp C3+C4)
    eee7944c VocalTrack::UpdateScrolling: per-region block-local deploy-zone pointer (Exp A+B2)
