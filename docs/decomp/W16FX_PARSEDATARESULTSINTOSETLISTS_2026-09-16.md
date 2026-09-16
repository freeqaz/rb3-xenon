# W16-FX — `MusicLibraryNetSetlists::ParseDataResultsIntoSetlists` reconstructed from retail bytes

**Date:** 2026-09-16 · **Branch:** `w16-fx` · **Base:** `c31808c1` · **Commit:** `7b0d7239`
**Target:** `?ParseDataResultsIntoSetlists@MusicLibraryNetSetlists@@QAAX_N@Z`, retail `0x825D01B8`, 1,968 B
**Ruler:** `name_check` (graded/shipped), objdiff 4.2.9, `tool_commit a5f0ea903ec1`, read from `report.json`'s own `provenance` block.

---

## Headline

| | value |
|---|---|
| target row | **81.93293 → 99.522354** fuzzy (mpn 83.254 → 99.98984) |
| our base size | **1,784 → 1,968 B** = exactly the target size (184 B deficit closed) |
| structural residue | **0 insert / 0 delete / 0 replace** — 47 `diff_arg` only |
| whole-binary Δ | **+11 matched / +600 B / +0.005850 pp / Δfuzzy +0.003224 pp** |
| Δhonest | **+0** (Δmasked_equal is also +11) |

⛔ **The target row banks ZERO of its own 1,968 bytes.** `matched_code` keys on
`fuzzy == 100` and the row stands at 99.5224. Every byte reported above comes
from *neighbouring* rows that crossed. Saying "the row went 81.9 → 99.5" is a
statement about legibility, not about bytes.

---

## 1. The central hazard was real, and worse than briefed

Our source for this function was **verbatim the rb3-Wii oracle**, and rb3-Wii is
a **dev** build. So the oracle could not be the authority. Everything below was
read off retail bytes, keyed on `.fn fn_825D01B8` — never the `.s` address
column, which is synthetic for multi-block units.

★ **The strongest single finding: `owner_guid` exists in NEITHER oracle.**
`command grep -rn "owner_guid" ~/code/milohax/rb3/src ~/code/milohax/dc3-decomp/src`
returns **nothing**. This is TU5-retail-only code with *no source oracle at all*
— the "a big row can have NO ORACLE IN EITHER REPO" case. It had to be
reconstructed from machine code.

### How it was found

The retail prologue hoists a run of `.rdata` field-name pointers into
callee-saved registers. Reading those addresses out of `orig/45410914/band.exe`
(PE section table parsed in Python — `grep` is binary-blind in this shell):

| label | retail string |
|---|---|
| `lbl_8205EBF8` | `title` |
| `lbl_820A5040` | `desc` |
| `lbl_8203F864` | `type` |
| `lbl_820B2F9C` | `art_url` |
| `lbl_82070C54` | `owner` |
| **`lbl_820B2F78`** | **`owner_guid`** ← we never parsed this |
| `lbl_82084900` | `guid` |
| `lbl_8209F6D8` | `id` |
| `lbl_820B2F84` | `valid_instr` |
| `lbl_820B2FA4` | `seconds_left` |
| `lbl_820B2F90` / `lbl_820B2FB4` | `s_id%03i` / `s_name%03i` |

`owner_guid` is loaded into r16 and used at **two** sites (0x825D0528 and
0x825D0700) — the `case 0/1` and `case 1000/1001` branches. That is exactly the
briefed `+2 GetDataResultValue / +2 String ctor / +2 Str` gap, and it is **one
field read twice**, not two fields.

### What retail does with it

```
lbz  r11, 0x0(r3)          ; c = *p
extsb. r11, r11
beq  end
loop:
  subi r11, r11, 0x30      ; c - '0'
  lbzu r10, 0x1(r3)        ; c = *++p
  mulli r8, r23, 0xa       ; 64-bit multiply
  extsw r9, r11            ; digit is 32-bit signed, widened
  extsb. r11, r10
  add  r23, r8, r9
  bne  loop
end:
...
cmpldi cr6, r23, 0x0       ; unsigned 64-bit test
beq  skip
std  r23, 0x60(r31)        ; materialise a TEMPORARY
addi r4, r31, 0x60
addi r3, r29, 0x40         ; &setlist->mOID   (NetSavedSetlist::mOID @0x40)
bl   fn_82524510           ; OnlineID::SetXUID(const XUID&)
```

An inlined decimal-string → `unsigned long long` XUID parse, then
`SetXUID`. `addi r3, r29, 0x40` is what identifies the callee: 0x40 is
`NetSavedSetlist::mOID`.

⚠ Note `fn_82524510` here, while `src/system/os/OnlineID.h`'s comment cites
`OnlineID::SetXUID @0x82511030`. I did **not** resolve that discrepancy and it
is not load-bearing for this lane (the call site's shape is what identified it).
A map lane should check whether one of the two is stale.

---

## 2. Four defects, each with its retail-byte evidence

### (a) `owner_guid` unparsed — the entire 184-byte deficit
Adding the two reads + `SetXUID` took our base size **1,784 → 1,968 B**,
exactly the target size. Row 81.93293 → 92.19309.

### (b) The accumulator must not be address-taken
Passing the accumulator straight to `SetXUID(const XUID&)` makes it
address-taken, so MSVC pins it to memory. Measured consequence: both digit loops
gained `ld r10, 0x68(r31)` / `std r11, 0x68(r31)` per iteration, **and the extra
8-byte stack home shifted every later frame offset by 8** (`addi r3,r31,0x130`
vs `0x128`). Retail keeps the accumulator in **r23** and spills only a
temporary (`std r23,0x60(r31); addi r4,r31,0x60`). Binding the const ref to a
short-lived copy reproduces that. Row 92.19309 → **97.477646**, whole binary
**+440 B / +11 fns**.

★ One root cause explained three symptom classes at once: the loop spills, the
frame-offset shift, *and* a large slice of the "REGISTER_SWAP" noise. This is
the brief's warning demonstrating itself — a `REGISTER_SWAP` label on a sub-100
row is a **symptom**, not a diagnosis.

### (c) ⭐ The ctor parameter order was wrong — the biggest structural finding
`??0NetSavedSetlist@@QAA@W4SetlistType@SavedSetlist@@PBD1_N111@Z` was itself a
sub-100 row (160 B @ 98.5 fuzzy). **Its body is the decisive instrument**,
because each parameter is stored into a named member:

| retail | lands in | ⇒ parameter is |
|---|---|---|
| r5 → r28 → `addi r3,r30,0x60` | `mGuid` (0x60) | **guid** |
| r6 → r27 → `addi r3,r30,0x34` | `mOwner` (0x34) | **owner** |
| r7 → r26 → `stb r26,0x50(r30)` | `unk44` (0x50) | **validInstr** |
| r8 → r25 → `addi r3,r30,0x54` | `unk48` (0x54) | **artUrl** |
| r9, r10 → `bl ??0SavedSetlist@@QAA@PBD0@Z` | base(title, desc) | **title, desc** |

⇒ retail: `NetSavedSetlist(SetlistType, guid, owner, validInstr, artUrl, title, desc)`
⇒ ours (from the dev oracle): `(SetlistType, title, desc, validInstr, owner, artUrl, guid)`

The four call sites corroborated it independently *before* the ctor body was
read: retail's **last three arguments are byte-identical across all four switch
branches** (they are the three strings computed before the switch — title, desc,
artUrl), while the first two vary per branch (guid, owner). Ours had that
inverted. Also measured: **`case 2` passes literal `nullptr` for guid and owner
(`li r5,0` / `li r6,0`), not `gNullStr`** — `gNullStr` is a load from a global,
which is what we emitted.

Row 97.928860 → **99.522354**; the ctor row itself crossed to 100 (**+160 B**).

### (d) `mSongs` test is `size()`, not `empty()`
Retail: `lwz r11,0x14(r29); lwz r9,0x10(r29); subf r11,r9,r11; clrrwi. r11,r11,2`
— `(end − begin) & ~3`, i.e. `size() != 0`. `empty()` lowers to
`begin() == end()` (`cmplw`), which is what we emitted. Row 97.477646 →
97.928860.

### (e) `Print` is dev-build residue
`mDataResults.Print(TheDebug)` — the brief's claim reproduced: the log string
`"Setlists from net:"` is absent from the retail image while `art_url`,
`seconds_left`, `valid_instr` are each present exactly once. Guarded with the
house pattern `#if defined(MILO_DEBUG) && defined(HX_NATIVE)`, **never**
blanket-removed (the measured whole-binary control for blanket removal is −21).

---

## 3. Corrections to the brief

The brief was right about the shape of the problem and wrong about three of its
six ranked sub-targets. Stated loudly, as requested:

- ⛔ **Sub-target #2 (`??2CriticalSection@@SAPAXI@Z` at 4 sites) is a NON-ISSUE.**
  `??2@YAPAXI@Z` is **already a folded member of alias group 1546** (survivor
  `??2CriticalSection@@SAPAXI@Z`). Already forgiven; no source work exists.
- ⛔ **Sub-target #4 (`MakeString<const char*>` vs `MakeString<int>`) is a
  NON-ISSUE.** `??$MakeString@H@@YAPBDPBDH@Z` is **`folded[0]` of alias group 4**,
  survivor `??$MakeString@PBD@@YAPBDPBD0@Z`. Already forgiven.
  ★ **Why the brief was misled: `objdiff-cli --analyze`'s "Function Call Diff"
  section is ALIAS-BLIND.** It reports raw name inequality, so an
  already-forgiven fold appears there as if it were a live divergence. Anything
  read out of that section must be checked against `scripts/symbol_aliases.json`
  before it is briefed as work. (CLAUDE.md already says "grep
  `symbol_aliases.json` BEFORE believing a reloc-name find" — this is that rule
  firing.)
- ⚠ **Sub-target #3 is not a divergence at all.** `fn_8235B008` *is*
  `NetSavedSetlist::GetOwnerOnlineID` and `fn_8251C7D0` *is*
  `PlatformMgr::CanSeeUserCreatedContent` — they read as "target only" purely
  because those addresses are **unnamed in `target_symbol_map.json`**. The real
  difference was that we called `GetOwnerOnlineID()` **twice** (once for
  `IsInvalid()`, once as the `CanSeeUserCreatedContent` argument) where retail
  calls it once and reuses r3; MSVC cannot CSE across a call. Hoisting it into
  a local fixed it. ⇒ **This is a map-coverage gap masquerading as a semantic
  divergence**, and two anonymous addresses are available for a map lane to name.
- ✅ The brief's core hypothesis (+2 `GetDataResultValue`/`String`/`Str` ⇒ missing
  field parses) was **correct**, but it is **one field read in two branches**,
  not two fields.
- ✅ The brief's `Print` finding reproduced exactly.

---

## 4. What I deliberately did NOT do

- ⛔ **I did NOT install the `push_back<vector<NetSavedSetlist*>>` alias.**
  It is very likely a genuine fold: alias group 10's survivor is
  `push_back<vector<ChatReceiver*>>` and it already carries **12** members
  (`NetSearchResult*`, `OvershellSlot*`, `BandUser*`, `ExternalMic*`,
  `JsonObject*`, …), all on CHASED-T1 proof; `vector<T*>::push_back` is one body
  for every pointer `T`. **But it cannot cross this row** — the r28↔r29 swap
  still charges 46 instructions — so installing it would buy `mpn` 100.0
  (**+1 function, +0 bytes**) with *no crossing to justify it*. That is exactly
  the "an alias lifts the score BY CONSTRUCTION" integrity hazard, and
  `ab_measure` would fire ALIAS_SUSPECT on a map-only patch. Left for a lane
  that can also close the register pair, with
  `tools/icf_pair_adjudicate.py --chase` as the instrument.
- ⛔ **I did not run the permuter** (OFF by standing user directive).
- ⚠ I did not resolve the `SetXUID` address discrepancy noted in §1.
- ⚠ I did not touch `target_symbol_map.json`; naming 0x8235B008 / 0x8251C7D0 /
  0x82524510 is a separate map lane with its own re-split requirement.

---

## 5. The residue, and a surprise that contradicts CLAUDE.md

After all five fixes the row is **structurally identical to retail**: 492
instructions, **0 insert / 0 delete / 0 replace**, 445 equal. All 47 remaining
charges are:

- **46 instructions of a pure r28↔r29 swap** (42 of them the same pair): retail
  puts the `DataResult&` in r28 and `setlist` in r29; we do the reverse. Plus a
  dependent r8↔r9 pair in each digit loop.
- **1 `push_back` ICF fold-alias** (see §4).

To reach `fuzzy == 100` and bank the 1,968 B, **both** must close. The register
pair is the blocker.

### ⚠ Surprise: declaration order is NOT inert for these registers

`CLAUDE.md` states, citing `MSVC_X360_REGALLOC.md`, that declaration order
"controls **stack slots**; it is measured *inert* for register-only swaps (12+
byte-identical hand variants across 4 functions, two zero-gain beam sweeps)".

**On this function that is false.** Three declaration variants, one variable
changed at a time, full builds, `report.json` read each time:

| variant | row fuzzy |
|---|---|
| `DataNode node; DataResult &result = *it;` … `setlist` before `switch` (**kept**) | **99.522354** |
| `setlist` hoisted to the top of the loop body | 98.231705 |
| `DataResult &result` before `DataNode node` | 99.126015 |

The spread is **1.29 pp** and it is monotone in nothing obvious. So declaration
order **did** move register assignment here — it just never moved it in the
helpful direction. ⇒ The doc's claim should be read as "inert *in the cases
measured*", not as a general law; and a lane should not skip the experiment on
the strength of it. Equally: **a lane should not expect the experiment to pay** —
three variants, zero gain, which is consistent with the doc's *conclusion* even
though its stated *mechanism* is wrong.

---

## 6. Measurement provenance

Baseline read from `build/45410914/report.json` **in this worktree** (never
inherited): `matched_functions 44,098 · matched_code 4,162,044 ·
matched_code_percent 40.61693 · fuzzy 50.446686 · total_code 10,247,068`.

Pre-registered before measuring: Δmatched **+11**, Δcode_bytes **+600**,
Δcode% **+0.00585 pp**, Δfuzzy **+0.003224 pp**, with the attribution
`??0NetSavedSetlist` (160 B, already `mpn` 100 ⇒ +0 fns) + 11 × 40 B EH funclets
(+11 fns, +440 B). Falsifiers: any Δ differing; any negative Δ; any unit outside
`MusicLibraryNetSetlists` moving; a refusal.

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-fx --from-dirty`, both legs
settled to zero work (2 iterations each), report cache wiped per read:

```
leg A: matched=44098 masked=23289 honest=20809 code%=40.616930  (recompiles: 0, settled)
leg B: matched=44109 masked=23300 honest=20809 code%=40.622780  (recompiles: 140, settle iterations: 2)
Delta matched=+11  Delta masked_equal=+11  Delta honest=+0
Delta code%=+0.005850pp  Delta code_bytes=+600
Delta fuzzy=+0.003224pp   (legA 50.446686 -> legB 50.449910)
unit improvements: 1 unit(s), sum +11
   +11  default/band3/meta_band/MusicLibraryNetSetlists  (37->48)
unit net (ALL units) = +11   vs whole-binary Delta matched = +11
units at 100% [mpn]: 194 -> 194 (Delta +0)   units at 100% [all-rows-fuzzy]: 172 -> 172 (Delta +0)
```

**Every pre-registered figure hit exactly. Zero regressions.**

⚠ **Honest reading of the +11:** `Δmasked_equal` is **also +11**, so
`Δhonest = 0`. All eleven newly-matched rows are 40-byte **EH funclets** of
`ParseDataResultsIntoSetlists`, which objdiff pairs by byte signature. They
crossed because the function's EH structure now matches retail — a real
consequence of the fix — but the honest floor (`matched − masked_equal`) is
unmoved. The `+600 B` of `matched_code` is real and is the number to quote.

⚠ The `[control none]` leg is **NOT_APPLICABLE** here and the tool says so: with
`source` in the patch, default-UP/none-FLAT is also the wrong-callee-fix
signature, so it cannot adjudicate an alias. No alias was installed, so nothing
rests on it.
