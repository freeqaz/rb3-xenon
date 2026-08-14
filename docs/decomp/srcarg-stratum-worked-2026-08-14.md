# The `SOURCE_ARG` stratum, worked — lane SRCARG-1 (2026-08-14)

Tree `7ea4cfbd` + this lane. Shipped ruler `functionRelocDiffs=name_check`
(read from `report.json` `provenance.diff_config`, and re-confirmed in every
`run_diff_inspect` header line — never assumed).

Baseline in-worktree: `total_code` **10,320,664** · `matched_code`
**3,726,864 / 36.110700%** · `matched_functions` **44,406** ·
`masked_equal` **22,897** ⇒ honest **21,509**. (That is INSTR-1's
3,725,560 + its landed 1,304 B, so the worktree carries its work.)

## Row closed

**`?SetFrame@FloatKeys@@UAAXMM@Z`** (PropKeys, 620 B) → **100%, all 155
instructions equal**.

Whole-binary A/B, prediction **pre-registered and exact**:

| measure | predicted | measured |
|---|---:|---:|
| Δ`matched_functions` | +1 | **+1** |
| Δ`matched_code` | +620 B | **+620 B** |
| Δ`matched_code_percent` | +0.006007 pp | **+0.006007 pp** |

0 regressions · 0 units fell off 100% · unit net +1 == whole-binary +1 ·
`Δfuzzy` +0.000000 pp. Native gate **PASS 18/18, 0 SKIPs, rc=0**.

### The defect class: MSVC SHARES A STACK SLOT BETWEEN DISJOINT LEXICAL SCOPES

Retail keeps **four** user slots — `0x50=ref, 0x54=val, 0x58=prev, 0x5c=next`.
We merged `val` into `ref`'s slot `0x50`, because each was declared inside its
own `if`/`else` arm, i.e. **disjoint scopes**. The symptom is a uniform
`off:-4` on six `r31`-relative accesses.

⚠ **The frame sizes were ALREADY EQUAL (0xe0 both).** This is the trap: the
row looks like a layout or frame-size defect and is neither — it is purely
*which locals share a slot*. Declaring both at function scope, `ref` first to
match retail's slot order, closes it.

Two variants measured and rejected, both informative:

- **Hoisting `float ref = 0;` bodily** to function scope: 99.96 → **94.7**. It
  moves the zero-store above the branch (4 insert / 4 delete) **and the
  `off:-4` delta SURVIVED** — so it did not even break the merge. Splitting the
  declaration from the assignment (`float ref;` at top, `ref = 0;` in the arm)
  is what preserves the store site.
- **Hoisting only ONE of the pair** (`float ref;` at function scope, `val` left
  nested): **BYTE-IDENTICAL to baseline.** A half-hoist is inert; both locals
  must leave the nested scopes. MSVC's packing here is not decided by the
  lexical position of a single declaration.

## ★ The class is ASYMMETRIC — only one direction is source-addressable

| direction | example | outcome |
|---|---|---|
| retail keeps slots SEPARATE, we merge | `FloatKeys::SetFrame` | ✅ **closed, +620 B** |
| retail SHARES a slot, we over-allocate | `SampleData::Load` (440 B, frame Δ +0x10, two `tmp` slots 0x60/0x70 vs retail's one 0x60 across [21..57]) | ✗ resists |
| retail SHARES a slot, we over-allocate | `CharIKHand::Load` (980 B) | ✗ **already drained by lane MATCH-G** |

`CharIKHand::Load` carries an in-source note from MATCH-G recording that the
unnamed-temporary variant was **refuted** (MSVC elides the copy ctor, frame
shrinks 0x120→0x110, 99.41 → 97.35) and that declaring `b` before `s` **changed
nothing**. ⇒ **Do not re-open either row on the scoping lever.** The
"make them share" direction is 0-for-2 and should be treated as codegen.

## The class, sized honestly — and two instrument traps that inflated it

Detector: all charges `diff_arg`, same opcode both sides, differing arg is a
displacement off a stack base, deltas from a small set
(`/home/free/tmp/srcarg1/scan_stackslot.py`).

⛔ **Trap 1 — the base register is NOT always the last argument.**
`lwz rD, off, rA` puts the base last; **`addi rD, rA, imm` puts it in the
MIDDLE.** Requiring it last silently dropped every `addi` slot-address form —
4 of FloatKeys' own 6 charges. The first run reported **5 rows / 1,072 B**;
corrected, **14 rows / 3,568 B**. ★ The correction was caught by running the
detector against the **known positive with its fix reverted** — it must fire
`PURE_STACK_SLOT {4: 6}` there, and the uncorrected one did not.

⛔ **Trap 2 — `r31` is a coin flip, and it must be resolved PER CHARGE, not per
function.** `r31` is either a second frame pointer or a cached `this`.
`mr r31, r3` in the prologue does **not** settle it: `RndText::Save` caches
`this` in `r31` **and** charges accesses off `r1`. Resolved on the base
register of each charged instruction, the 14 rows split:

| true class | bytes |
|---|---:|
| `STACK(r31-frame)` | 1,556 |
| `STACK(r1)` | 1,260 |
| `MEMBER(this)` — a struct-layout defect, not a slot defect | 752 |

⛔ **Trap 3 — a delta set containing BOTH `+X` and `−X` in equal counts is a
PERMUTATION ARTIFACT, not an offset shift.** `RndParticleSys::UpdateRelativeXfm`
(412 B, `{64:2, -64:2}`) reads as two swapped 64-byte `Transform` members; it is
not. **Both sides reference the same offsets** {532,536,540,596,600,604} — the
loads are merely permuted, so pairing target[i] against base[i] manufactures a
symmetric delta. INSTR-1's commutative-order diagnosis of that row **stands**.
Same for `KerningTable::SetKerning` (352 B, `{-2:1, 2:1}`). Removing the two
artifacts leaves ≈ **2,804 B** of genuine class.

⇒ **Only single-signed delta sets are real shifts.**

## Sub-class that is NOT scope-addressable: compiler temps for `bs << x`

`RndText::Save` (328 B) and `PlayerDiffIcon::Save` (136 B) are `STACK(r1)` with
a clean uniform `+4`, and both are the inlined `bs << x` →
`WriteEndian(&temp, 4)` pattern. Retail uses **two** temp slots (0x54 for
insns [5..42], 0x58 for [45..78]); we reuse one. These are **compiler-generated
temps with no named local to scope**, so the FloatKeys lever has no handle on
them. `Text.cpp` already carries a prior lane's note that **chaining the
trailing writes to force the second slot REGRESSED** (frame grew +0x10).
⇒ The stack-slot class splits into **named source locals (addressable)** vs
**compiler temps (not)**.

## Force-multiplier verdict: NOT one — 2 rows / 732 B

INSTR-1 flagged, without chasing it, that *"retail rematerialises a
temporary's address where MSVC hands us the ctor's `this`"* — target
`addi rD, <frame>, imm` vs base `mr rD, rS`, two instances.

Scanned across all **1,137** divergence rows
(`/home/free/tmp/srcarg1/scan_remat.py`):

| measure | value |
|---|---:|
| rows carrying ≥1 site | **33** / 1,137 (28,164 B) |
| total sites | **62** |
| of those rows, `SOURCE_INSDEL` | **29** |
| rows the shape **FULLY explains** (`hits == n_charged`) | **2** (732 B) |

⇒ **The shape is common but almost never load-bearing.** 29 of 33 rows are
`SOURCE_INSDEL` where the site is one charge among hundreds
(`CharacterCreatorPanel::Handle`: 1 hit / **224** charged), so closing it buys
nothing — `matched_code` is all-or-nothing per row. Only
`BandCamShot::OnListAnimGroups` (660 B, 1 charge) and `MicNull::GetRecentBuf`
(72 B) are fully explained. **Do not fund this as a multiplier.**

★ **And the mechanism is not what it looks like.** The obvious reading — retail
has a *named* local, we wrote an unnamed temporary — is **REFUTED**: retail
destroys the temp immediately after the assignment (inlined dtor at insns
132-136), which is end-of-full-expression semantics, i.e. an **unnamed
temporary on both sides**. Writing the named local anyway
(`DataNode tmp(...); handled = tmp;`) measured **99.64 → 93.9**: the dtor moved
to end-of-block (5 deletes → 5 inserts) and **the `addi`/`mr` charge did not
even close.** Reverted.

⚠ `MetaPerformer::SyncSave` — INSTR-1's other named instance — carries the same
shape but the scan does **not** flag it, because its two charges are a
`replace` pair whose destination registers differ (`li r28,1` / `addi r4,r31,88`
against `mr r4,r3` / `li r28,1`, i.e. the remat is tangled with a scheduling
swap). Its ternary-with-temporary source line is already the right shape.

## Deferred, with the diagnosis recorded

- **`StandardStream::Init`** (672 B) — exactly **one** real defect,
  `stb r29, 233(r30)` vs our **232**: `mFloatSamples` is one byte low. The
  header comment says `// 0xe8` (232), which is what we emit. The other two
  charges are `vector<T*>` ICF fold-aliases (`_M_fill_insert`, `push_back`), so
  the row can reach `mpn` 100 (**+1 function**) but **never `fuzzy` 100 / 0
  bytes**. Not attempted: a 1-byte member shift in a synth base class cascades,
  and the payoff is bytes-free.
- **`GemManager::GemManager`** (896 B) — two adjacent `stb` stores swapped
  (retail 204 then 196; ours 196 then 204). Third charge is a
  `vector<T*>::reserve` fold ⇒ likewise 0 bytes.
- **`Character::CalcBoundingSphere`** (1,340 B) — the three component
  subtractions are computed in order y,z,x (retail) vs z,y,x (ours); operand
  semantics are identical (`local - world` both sides). Scheduling.
- **`Game::Reset`** (396 B) — charge at insn 90 is a **map defect, not a source
  defect**: retail names the callee `?Init@Movie@@SAXXZ` (static, `void(void)`)
  at a site that passes `f1` and consumes the returned `f1` — our
  `?TickToMs@@YAMM@Z` fits the bytes and `Movie::Init` cannot. Adjudicate on
  the map row, not the call site. (The row also carries an arg-address
  ordering charge, so repairing the map alone will not cross it.)

## ⚠ The stratum is LESS tractable than its median implies — the number

`SOURCE_ARG` is 251 rows / 46,356 B at a median of **2** charged instructions,
which reads as an order of magnitude cheaper per row than `SOURCE_INSDEL`'s 28.
**The median is real; the tractability it implies is not.** Worked from the top
down by size-if-it-crosses, the stratum decomposes roughly as:

- **fold-capped** — the row's remaining charges are ICF relocation-name
  aliases, so it can reach `mpn` 100 but **0 bytes** (StandardStream,
  GemManager, BandSongMgr).
- **codegen** — scheduling, operand permutation, temp-slot colouring
  (CalcBoundingSphere, RndText::Save, CharIKHand, the whole "make them share"
  direction). Permuter is OFF by directive.
- **map defects** — the charge is a wrong map name, not wrong source
  (Game::Reset).
- **genuinely source-fixable** — this lane found and closed **one** at 620 B.

A "median 2 charges" row is cheap **to diagnose** and frequently impossible
**to close**, because with so few charges the odds that *all* of them are
source-shaped are poor — and `matched_code` pays nothing until they all close.
⇒ **Price this stratum by defect signature, not by charge count.** The
signature that paid here was *uniform single-signed stack-slot delta with
matching frame size and a TGT_ONLY slot* — which is exactly what
`run_diff_inspect mode=stack-layout` reports, and it should be the first tool
reached for on any `SOURCE_ARG` row.

## Tooling (under `/home/free/tmp/srcarg1/`)

`show.py` (per-row charged-instruction dump at the shipped ruler — a
reconstruction, INSTR-1's copy did not survive), `scan_stackslot.py`
(stack-slot signature + its known-positive control), `split_r31.py`
(`r31` role discriminator), `scan_remat.py` (rematerialised-address census).
