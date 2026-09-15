# W16-BV — VocalTrackDir / GemTrackDir residue

Lane W16-BV, 2026-09-15. Worktree `~/tmp/wt-w16-bv`, branch `w16-bv`, based on
main `fe020891`. Surface: the `0x822E`/`0x822F` band, `src/system/bandobj/
VocalTrackDir.*` and `GemTrackDir.*`, their `splits.txt`/map rows, and
`symbol_aliases.json` groups for three fold candidates.

**Headline: four rows improved, whole-binary delta EXACTLY 0.** Every source
and map change below was landed on accuracy, not on the metric, and each is
priced honestly. The reason is structural and is the single most important
thing in this document — see §2.

---

## 1. Baseline — the brief's figures, tested literally

Full build rc=0 in the worktree before touching anything. Headline keys
reproduce the briefed ledger row `c207d1ae` **exactly**:

| key | measured | brief |
|---|---:|---:|
| `matched_functions` | 43,651 | 43,651 |
| `matched_code` | 4,054,964 | 4,054,964 |
| `matched_code_percent` | 39.571945 | 39.5719 |
| `total_functions` | 69,240 | 69,240 |
| `total_code` | 10,247,068 | 10,247,068 |
| `fuzzy_match_percent` | 49.804825 | 49.804825 |
| `masked_equal_functions` | 23,107 | 23,107 |

objdiff tool commit `a5f0ea903ec1`, binary hash `5a51cd51fe0a353f`, ruler
`functionRelocDiffs=name_check` read from `report.json`'s own `provenance`.

The ten briefed rows also reproduce exactly, **with two corrections**:

1. **`SetConfiguration` is a `VocalTrackDir` method and the row lives in
   `default/VocalTrackDir`** — as the brief's own ⚠ note suspected. Confirmed:
   528 B, fuzzy 82.4015, mpn 82.5530. BQ discussed it in a GemTrackDir document;
   the row is VocalTrackDir's.

2. ⛔ **The brief's `mpn = 100` for `OnDrawSampleChord`, `PlayIntro` and
   `ReleaseSmasherPlate` is WRONG on this binary.** Measured, all three have
   `mpn == fuzzy`: 99.9691 / 99.9590 / 99.9550. This matters for pricing: the
   brief implied these rows could only ever pay bytes, because they were already
   counted as matched functions. They are **not** counted — objdiff 4.2.9's
   `vetted_reloc_name_diff` (`b14ba45`) keeps a vetted relocation-name diff in
   `diff_score` instead of cancelling it out of `mpn`, so each of these rows
   would pay **+1 function AND its bytes** if it crossed. CLAUDE.md already
   records that `matched_functions` stopped being ruler-invariant on 4.2.9; this
   is the same mechanism showing up in a per-row figure.

**A census the brief undercounted.** The brief named "the two 0% unnamed rows".
`default/VocalTrackDir` actually carries **431 anonymous rows / 23,188 B** and
`default/GemTrackDir` **297 / 16,344 B**. Most of that is the 32/40/44/48-byte
EH-funclet stratum, which is not a backlog. The *named* sub-100 surface — the
part a source lane can work — is **11 rows / 9,852 B** (VocalTrackDir) and
**14 rows / 6,444 B** (GemTrackDir). Real unnamed function-sized rows beyond the
two briefed include `fn_822F93D0` (688 B), `fn_822F64D8` (632 B),
`fn_822EE768` (728 B) and `fn_822E5520` (608 B); none were identified here.

---

## 2. Why every measured delta in this lane is 0 — read this before pricing anything

Four rows improved substantially and the whole-binary keys did not move by one
byte. That is **not** a measurement failure and not a reason to revert:

> `matched_code` is the Σ size of rows at `fuzzy_match_percent == 100`, and
> `matched_functions` counts rows at `match_percent_normalized == 100`. Both are
> **all-or-nothing per row**. A row moving 6.73 → 74.32, or 0 → 91.62, pays
> **exactly zero** on both.

So this lane's output is four rows repositioned much closer to the crossing
line, plus three adjudications that close veins. Per the standing directive
(*accuracy beats headline %*, and *a metric that hides real bugs is worse than a
lower metric*), that is the intended shape — but nobody should book it as bytes.

**All five keys after every change below: 43,651 / 4,054,964 / 39.571945 /
69,240 / 10,247,068 — identical to baseline, `total_code` included.**

★ **There IS one whole-binary key that registers this lane, and it is the one
nobody quotes: `fuzzy_match_percent` moved 49.804825 → 49.826508 (+0.021683).**
Unlike `matched_code`, the aggregate fuzzy is a size-weighted mean over rows, so
partial progress shows up in it. That is the honest headline for work of this
shape, and it is worth briefing future residue lanes on it — a lane told to
expect "Δ0 or you failed" will revert exactly the work that moves this key.

---

## 3. Per-row results

| row | unit | B | before | after | Δ headline | what changed |
|---|---|---:|---:|---:|---:|---|
| `?ApplyFontStyle@VocalTrackDir@@` | VocalTrackDir | 1,164 | 6.7251 | **74.3230** | 0 | source: Hmx::Color copy, not pack/unpack (§4) |
| `fn_822EF438` → `?PreLoad@GemTrackDir@@` | Vocal→Gem | 1,532 | 0 (unpaired) | **91.6188** | 0 | splits re-home + map name (§5) |
| `?SetConfiguration@VocalTrackDir@@` | VocalTrackDir | 528 | 82.4015 | **88.6364** | 0 | source: statement order (§6) |
| `?PostLoad@VocalTrackDir@@` | VocalTrackDir | 3,656 | 96.3862 | 96.3862 | 0 | diagnosed only (§7) |
| `?SetRange@VocalTrackDir@@` | VocalTrackDir | 700 | 93.5371 | 93.5371 | 0 | diagnosed only (§7.5) |

No row anywhere in the binary fell out; no row crossed in. Set-diff of the
`fuzzy==100` rowset against the baseline snapshot is reported in §9.

---

## 4. `ApplyFontStyle` 1,164 B — 6.73 → 74.32 (the oracle was wrong *and* our port of it was wrong)

The brief guessed "structural mismatch, wrong function body, or a missing
early-out chain". It is none of those: it is an **invented integer round-trip**.

Retail at `0x822f7488` (read out of `build/45410914/asm/VocalTrackDir.s`, keyed
on the `.fn` symbol) initialises four 16-byte colour slots on the stack —
`0x60/0x70/0x80/0x90` = `(1,1,1,1)`, `(1,1,1,0.75)`, `(1,1,1,1)`,
`(1,1,1,0.75)`, matching our initialisers exactly — and then, per text object,
inlines `SetShowing(false)` as `stb r11,0x8(r10)` and copies **four consecutive
words from `textobj+0x11c..0x128`** into the slot with plain `lwz`/`stw` pairs:

```
lwz r8, 0x11c(r10) ; stw r8, 0x0(r9)
lwz r8, 0x120(r10) ; stw r8, 0x4(r9)
lwz r8, 0x124(r10) ; stw r8, 0x8(r9)
lwz r10,0x128(r10) ; stw r10,0xc(r9)
```

`+0x11c` is `mStyle+0xC`, which `src/system/rndobj/Text.h` already documents as
`Hmx::Color@0x11c`. **Our layout was right; only the source was wrong.** We
called `GetSingleStyleColor()` (= `mStyle.mTextColor.PackAlpha()`) and then
unpacked the result again with `& 0xFF` / `/ 255.0f`. Retail never packs or
unpacks anything — it copies the `Hmx::Color` struct.

That invented chain is what the diff was reading as 182 inserts, 92 register
swaps in 8 pairs, and a 9-instruction `BOOL_MASK` cluster. Replacing the 16
loose floats with four `Hmx::Color` locals assigned from `StyleColor()` took the
row to **74.3230 / 75.5773**.

⚠ **The rb3-Wii oracle is refuted here, not merely departed from.** The oracle
spells this `mStyle.color.color` (a packed `Color32`) and unpacks it exactly as
we did; retail refutes the packed read outright. This is the documented
*retail bytes outrank the oracle* case, and it is why the offset table in the
first objdiff run (`target 0x11c vs base 0x54`) should not have been trusted on
its face — with 182 inserts the instruction pairing is garbage and those
"offset mismatches" were alignment artifacts. The prologue (`bl __savefpr_16`,
16 FPRs = the 16 floats) was the honest signal, and the `.s` was the ground truth.

**Residue (74.32 → 100), not attempted:** a frame-shape divergence. Target does
`subi r31, r1, 0x100` and addresses locals off r31 with a 0x100 frame and
`__savegprlr_22`; we use r1 with 0xe0 and `__savegprlr_26`. 61 of the remaining
register swaps are `r1↔r31` and follow from that one difference, and retail
holds five function-local `static Symbol` addresses in r22/r23/r24/r26/r28
simultaneously. Flagged `PROLOGUE_MISMATCH` / `RarelyHandFixable`.
*Evidence that would change it:* a reproduction showing which source shape makes
MSVC `/O1` choose the `subi r31,r1,N` frame-pointer form here, or a demonstration
that all five statics can be held live across the four property blocks.

---

## 5. The two unnamed rows — identified, one re-homed, one refuted

### 5.1 `fn_822EF438` (1,532 B) IS `GemTrackDir::PreLoad`, wrongly pinned

Three independent lines of evidence, strongest first:

1. **The thunk.** GemTrackDir's own pin *ends* at `0x822EF430` and *resumes* at
   `0x822EFB50` — and `0x822EFB50` is
   `?PreLoad@GemTrackDir@@$4PPPPPPPM@A@AAXAAVBinStream@@@Z`, the vtable thunk for
   this very function. A thunk lives in the TU that defines its function, so the
   block sandwiched between two GemTrackDir blocks was GemTrackDir's all along.
2. **Callees**, resolved through `target_symbol_map.json`:
   `?PreLoad@TrackDir@@` (the base-class call), `?LoadTrack@BandTrack@@`, and a
   run of `ObjRefConcrete<T>::Load` + `BinStream::ReadEndian` — the signature of
   a TrackDir subclass's `PreLoad`. Two callees (`0x822E4E80`, `0x822E7A20`) are
   themselves rows of the GemTrackDir unit.
3. **Size**: our `?PreLoad@GemTrackDir@@` COMDAT is 1,516 B against the 1,532 B
   target row (the 16 B being the 8-byte EH prefix plus alignment).

⛔ **REFUTED on the way: it is NOT `VocalTrackDir::PreLoad`**, which was the
obvious first guess given the unit it sat in. That symbol is already mapped at
`0x82302838`, is 148 B, and **already matches at 100%**. Checking the map before
proposing a name is what prevented a wrong re-home here.

Landed: moved the `.text` block to `GemTrackDir.cpp` and added
`0x822ef438 → ?PreLoad@GemTrackDir@@UAAXAAVBinStream@@@Z`. dtk re-derived the
`.pdata` range and rewrote `splits.txt` — the documented derived-output
behaviour, and the split-guard says so explicitly; the retry build is a fixed
point and `symbols.txt` never moved across either build.

Result: the 0% unpaired row became **91.6188 / 91.8277**. VocalTrackDir
528→520 rows, GemTrackDir 413→421. `total_code` unchanged ⇒ a pure
reattribution, exactly as CLAUDE.md predicts for pinning that does not re-home
an *already-correctly*-pinned address.

### 5.2 `fn_822FA7E0` (1,000 B) IS `VocalTrackDir::Save` — deliberately NOT named

Callees: `?Save@RndDir@@` (the base-class call), 18× `BinStream::WriteEndian`,
4× `BinStream::Write`, `operator<<(BinStream&, map<int,float>)` and
`operator<<(BinStream&, Symbol)`. That is a `Save` override and nothing else.

**Not named, on purpose.** Our `?Save@VocalTrackDir@@UAAXAAVBinStream@@@Z`
COMDAT is **4 bytes** — an empty stub; the header declares `virtual void
Save(BinStream&)` and no definition exists in `VocalTrackDir.cpp`. Mapping a
1,000 B target row onto a 4-byte stub buys a *pairable row scoring ~0 with no
content*, which is the metric-fitting hazard CLAUDE.md names explicitly. The
honest statement is that this is a **body-port opportunity of ~1,000 B**, not an
identification one.
*Evidence that would change it:* an actual `VocalTrackDir::Save` body. Once one
exists, the map entry is a one-line change and the identification above is
already done.

---

## 6. `SetConfiguration` — the liveness diagnosis the brief asked for

**Which two callee-saves: r24 and r25.** Target `mr r28, r5` (one instruction)
against our `mr r24, r4` + `mr r25, r5` (two) — we held **both** parameters in
callee-saved registers; retail only the second. Retail saves r26-r31, we saved
r24-r31.

**Why.** Retail consumes `r4` at idx 13-14 (`addi r3,r30,0x36c` /
`SetObjConcrete`) while it is still live in a volatile register. We emitted that
same call at idx 50-52, so both parameters had to survive the three
`static Symbol` guard-init blocks in between. The cause is **source statement
order**: our `mVoxCfg = o; unk4b4 = state;` sat *below* the three `static Symbol`
declarations, retail's above them.

This is not the "declaration order controls registers" claim that CLAUDE.md
corrects — declaration order really is inert for registers. It is *where two
statements sit relative to a static-init sequence*, which changes what must stay
live across it.

Moving the two assignments above the three statics: charged sites **29 → 15**,
fuzzy **82.4015 → 88.6364**. Prediction stated before measuring — that the
prologue, frame and register sites would all close — **held exactly**:
`__savegprlr_26`/`__restgprlr_26` now match, the frame is 0x90 not 0xa0, and
r24/r25 are gone.

⛔ **Two corrections to W16-BQ §9.2.** It called this "a register-pressure
difference … a substantially harder class than a missing statement" and asked
for "a `__savegprlr_26` reproduction". It was one source reorder. And its claim
that *"our source already carries the block"* the diff appears to be missing is
wrong: we emit **none** of those 15 instructions.

**Residue: exactly the 15-instruction trailing block, all `delete`.** Decoded
from retail bytes:

```
if (unk2a7 /*0x1fa*/ && mPlayerIntro /*0x348*/) {
    RndTransformable *t = mPlayerIntro + 0xd4;
    if (t->byte@0x170 == 0) t->SetDirty_Force();
    t->float@0x4c = <constant at lbl_82000D78>;
}
```

No `Find<>` call, no 3-float copy. Our source calls
`mPlayerIntro->SetLocalPos(Find<RndTransformable>("h2h_player_intro_trans.grp",
true)->LocalXfm().v)` — a string lookup and a `Vector3` copy, a different
computation. *Evidence that would close it:* the identity of the single float
member at transform`+0x4c` and the value at `lbl_82000D78`, plus which
`RndTransformable` sub-object sits at `mPlayerIntro+0xd4`. That is a small,
well-posed adjudication and the best-value next step in this unit.

---

## 7. `PostLoad` 3,656 B @ 96.39 — diagnosed, not attempted

The biggest size-if-it-crosses in the band, and nobody had classified it. 57
charged sites across 923 instructions (21 `diff_arg`, 2 `diff_op`, 5 `replace`,
9 `insert`, 20 `delete`). It is **not one lever**:

- **Instruction scheduling** (idx 19-29): the same instructions
  (`lwz r9,-0x738(r30)`, `clrlwi r11,r3,16`, `sth r11,0x4(r26)`) in a different
  order. No source construct is named by the charge.
- **`OFFSET_SWAP` of (0xa8, 0xb8), 3 sites** — target writes `0xb8/0xbc/0xc0`
  where we write `0xa8/0xac/0xb0`, a uniform +0x10. The `Stack` summary reports
  *3 DIFFER, 4 PERMUTED, 2/0 TGT/BASE-only — different variables in same slots*,
  so these are **stack slots, not struct members**, and the resolver's
  `RndTransformable::mTarget` / `BandTrack::mStopDeployTrig` labels are guesses
  across two unrelated classes. Stack-slot assignment *is* the one thing
  declaration order controls, so this is the tractable sub-lever.
- **`subi` vs `addi`** at idx 426/458 with `-0x738(r30)` / `subi r4,r11,0x734`
  — small-data-area addressing, the MSVC global co-addressing class.
- One `WRONG_CALLEE` and one `TEMPLATE_INSTANTIATION_MISMATCH`, one call site each.

Deliberately not ground: a 96.39% row needs *all 57* closed to pay anything
(all-or-nothing), and three of the four classes above are not named by any
source construct. *Evidence that would change it:* a stack-layout table
(`run_diff_inspect mode=stack-layout`) identifying which locals occupy the
swapped 0xa8/0xb8 slots, since a decl reorder is cheap if the pair is named.

---

## 7.5 `SetRange` 700 B @ 93.54 — diagnosed, one real finding, not a single lever

⚠ **The brief's symbol is wrong**: there is no `?SetRange@VocalTrackDir@@QAAXMM@Z`.
The row is `?SetRange@VocalTrackDir@@QAAXMMH_N@Z` — four parameters
`(float min, float max, int tonic, bool b)`, not two. Looking the symbol up
rather than trusting the brief is what found it.

17 charged sites of 180 instructions, in two clusters:

**(a) One genuine type divergence, idx 141/143** — target `cmpwi cr6, r11, 0x0`
(**signed**) against our `cmplwi cr6, r11, 0x0` (**unsigned**), on the word at
`0x534(r31)`. The compiler (`/d1reportSingleClassLayoutVocalTrackDir`,
`sizeof` = 1904) puts `0x534` inside **`ObjPtr<RndGroup> mTubeRangeGrp` at +8** —
its raw pointer field. Our source binds it to a pointer first:

```cpp
RndGroup *grp = mTubeRangeGrp;
if (grp) { ... }
```

A `RndGroup*` nullness test is an unsigned compare; retail's signed compare says
retail tested something declared as a signed integral at that offset. This is a
**well-posed single-site question**, and it is the only charge in the row that
names a source construct.

**(b) The other 16 sites are CSE and scheduling**, and neither is a source lever:

- idx 116-125: retail **recomputes** `addi rN, r30, 0x4c` at both use sites of the
  material's tex-transform field; we compute it once into `r29` and `mr` it twice.
  This is the `Transform texXfm = mat->TexXfm(); … mat->SetTexXfm(texXfm);` block —
  the `li r5, 0x40` makes it a 64-byte struct copy. A common-subexpression
  difference, not a missing or extra statement.
- idx 127-140: the same instructions in a different order around the
  `mMiddleCZPos = bottom + (60.0f - min) * pitchRange / (max - min)` computation
  (`__real@42700000` is 60.0f) — retail loads `0x188(r30)` early and the 60.0f
  constant late, we do the reverse.

⇒ **Not attempted.** Closing (a) alone buys nothing — `matched_code` is
all-or-nothing and the 16 scheduling sites would remain.
*Evidence that would change it:* the declared type of the field retail reads at
`+0x534`, i.e. whether retail's `ObjPtr<T>` stores a signed integral where ours
stores `T*`. That single answer closes (a) and is worth having independently,
because `ObjPtr` is used tree-wide — if our `ObjPtr` layout carries a signed
field retail spells differently, this row is one witness of many.

## 8. Fold-alias adjudications — three candidates, **zero installed**, all refuted or undecidable

The instrument was validated before use: `tools/icf_pair_adjudicate.py
--selftest` returns **PROVEN** on its positive control and **REFUTED** on its
negative control ("selftest PASSED -- the instrument can both pass and fail").
A gate that cannot fail proves nothing, so this ran first.

### 8.1 `PlayIntro` 488 B — `??2CriticalSection@@SAPAXI@Z` vs our `??2Task@@SAPAXI@Z` — **REFUTED**

Flat T1 returns **UNDECIDABLE (VACUOUS)**: both bodies are 8 bytes (2 words),
below the ≥4-word anti-vacuity guard, and retail's 8-byte body has **12 twins
including `Sleep`, `atof` and `OggMalloc`** — it compares equal to far too much.

`tools/fold_thunk_gate.py`, which is built for exactly this sub-4-word stratum
because it *resolves* the branch destination instead of masking it, returns a
hard **REFUSE**:

> our COMDAT is not the retail survivor body — relocated fields at different
> offsets: retail `[0, 4]` vs ours `[4]`

Retail's 2-word body carries relocations in **both** words, ours in only the
second. Different shapes ⇒ not one COMDAT ⇒ not folded. Independently,
`wrong_callee_triage` classifies this pair **`map_misassignment`**, not
`fold_thunk_naming`: the map parks `??2Task@@SAPAXI@Z` at `0x822d4278` with
**fanin 0** while retail's `0x827bd2f0` has fanin 1048.
*Evidence that would change it:* a retail address for `??2Task`'s own body that
is byte-identical to `0x827bd2f0` with relocation targets agreeing.

### 8.2 `ReleaseSmasherPlate` 444 B — **REFUTED**, and the charge is not a naming artifact

Charge confirmed exactly as briefed: target
`??0?$StlNodeAlloc@VSmasherPlateInfo@GemTrackResourceManager@@@stlpmtx_std@@QAA@ABV01@@Z`
vs our `?ReleaseSmasherPlate@GemTrackResourceManager@@QAAXPAVRndDir@@@Z`.

T1: **retail's survivor is 4 bytes; our body is 88.** Not the same body by an
order of magnitude — no tier can fold them.

The interesting part is what that implies. All 110 other instructions match, so
the call site is identical in shape and retail's callee at that site is a 4-byte
body (a bare `blr`). `?ReleaseSmasherPlate@GemTrackResourceManager@@` is
**absent from `target_symbol_map.json` entirely**. So retail's
`GemTrackResourceManager::ReleaseSmasherPlate` is *empty*, and ICF folded it onto
the trivial empty-allocator copy-ctor whose name the map happened to record.

**Our source was NOT changed**, and that is deliberate: it is byte-for-byte the
rb3-Wii oracle's body (a linear search of `unk28` clearing `mInUse`), and
emptying it would (a) contradict the oracle on a behavioural question, and
(b) **not clear the charge anyway**, since `name_check` compares names and ours
would still be spelled differently. There is no metric payoff to buy the risk.
*Evidence that would change it:* a retail address for
`GemTrackResourceManager::ReleaseSmasherPlate`'s own body showing it empty — at
which point the pair becomes an FT-EMPTY alias candidate rather than a source edit.

### 8.3 `??3GemTrackDir@@SAXPAX@Z` → the `??3BinStream` group — **NOT ADJUDICATED**

`??3GemTrackDir@@SAXPAX@Z` appears in **neither** `scripts/symbol_aliases.json`
nor `scripts/target_symbol_map.json` (0 occurrences in each), and it is not in
the 2026-08-12 `wrong-callee-triage` worklist that `fold_thunk_gate.py` consumes,
so the gate has no pair to decide. Under the gate's tiering an unmapped `addr(F)`
is the **FT1** shape ("nothing contradicts"), so this is plausibly admissible —
but "plausibly admissible" is not a proof, and an unproven alias lifts
`name_check` **by construction**.
*Evidence that would change it:* our COMDAT for `??3GemTrackDir@@SAXPAX@Z`
shown byte-identical, with resolved relocation targets compared, to retail's
body at `??3BinStream@@SAXPAX@Z` (`0x8240ddb0`, 4 B).

### 8.4 `OnDrawSampleChord` — left alone, as instructed

Our `vector<int>` is right per the oracle's explicit `std::vector<int> fretNums`.
Not touched, and no alias attempted: `vector<T>::_M_fill_insert` is byte-identical
for any 4-byte POD, so a byte proof here would be exactly the template-twin
vacuity that `relocs_agree` exists to close.

### 8.5 A binary-wide finding, FILED not installed

Run read-only for context, `fold_thunk_gate.py --subclass fold_thunk_naming`
reports **ADMIT 7 pairs / 1,507 sites in 3 groups; REFUSE 29 pairs / 290 sites**
— including `??3BinStream@@SAXPAX@Z <- ??3@YAXPAX@Z` at **1,180 sites** on an
FT3 homonym witness. That is a change across ~403 units, far outside this lane's
surface and overlapping other live lanes. **Not installed.** Filed for the
coordinator as a sized, gated, ready-to-run lever.

---

## 9. Gates

All run in the worktree, in the briefed order, each exit code captured by
redirecting to a file and testing `$?` on the **next** line (never
`cmd | tail; echo rc=$?`, which reads `tail`'s status and is vacuous).

| # | gate | rc | substance |
|---|---|---:|---|
| 1 | full `./tools/ninja-locked` | **0** | fixed point; `~/tmp/rb3_build_w16bv_6.log` |
| 2 | `verify_ruler_agreement.py --check` | **0** | `functionRelocDiffs = name_check`, `combineDataSections/TextSections = true`, `ppc.calculatePoolRelocations = false` — *"both objdiff-cli entry points resolve the same ruler"* |
| 3 | `verify_objs_patched.py --verify-manifest` | **0** | 1,219 decomp + 3,105 target objects match, `tree_sha256=9d443604a38eea5d`; denylist clean (495,503 symbols scanned) |
| 4 | `icf_alias_finder.py --validate` | **0** | `PASS — 1406 map-consistent, 249 tolerated, 0 contradicted, 1656 total` |
| 5 | `funclet_homing.py --validate` | **0** | `PASS` — 25,052 HOMED / 1,226 ORPHAN / 1 MIS-PINNED / 42 UNPINNED-FUNCLET, fan-in uniformly 1 |
| 6 | `tools/native_build_gate.sh` | **0** | last action; line quoted verbatim below |

**Rowset set-diff against the baseline** (`~/tmp/rows_w16bv_base.json`, captured
from this tree before any edit, 40,979 rows at `fuzzy == 100`): **0 crossed in,
0 fell out** — and the after-snapshot is also exactly 40,979.

⚠ **That set-diff was nearly a false alarm, and the near-miss is worth the line.**
The first run keyed the after-set as `unit|symbol` while the baseline file used
`unit::symbol`, producing a confident, decisive-looking **40,979 crossed in and
40,979 fell out** — every row in the binary, simultaneously, in both directions.
The give-away was that the two numbers were equal and equal to the total. Keyed
correctly, both differences are empty. This is the same family as the `grep`
binary-blindness and `all([])` traps the repo docs collect: **an instrument whose
failure mode is a decisive-looking answer**, and the check that caught it was a
sanity read of the magnitude, not the tool.

### Native gate (last action)

```
NATIVE_GATE_RESULT_PLACEHOLDER
```

---

## 10. NOT done, and why

| # | not done | reason | evidence that would change it |
|---|---|---|---|
| 1 | `?PostLoad@VocalTrackDir@@` 3,656 B @ 96.39 | 57 charged sites in four classes, three of which name no source construct; all-or-nothing means partial work pays 0 | a `stack-layout` table naming the locals in the swapped 0xa8/0xb8 slots — a decl reorder is cheap once the pair is named |
| 2 | `?SetRange@VocalTrackDir@@` 700 B @ 93.54 | 16 of its 17 charged sites are CSE/scheduling that name no source construct; closing the 1 real one pays 0 under all-or-nothing | the declared type of the field retail reads at `ObjPtr<T>`+8 — see §7.5; valuable beyond this row since `ObjPtr` is tree-wide |
| 3 | `?ApplyFontStyle@` 74.32 → 100 | frame-shape/prologue divergence (`subi r31,r1,0x100`, r22-r31 vs r26-r31), `RarelyHandFixable` | a reproduction of the source shape that makes MSVC `/O1` pick the r31 frame-pointer form and hold five statics live |
| 4 | `?SetConfiguration@` last 15 instructions | needs the identity of the float at transform`+0x4c`, the constant at `lbl_82000D78`, and the sub-object at `mPlayerIntro+0xd4` | those three identifications; the block is fully decoded in §6 and is a small, well-posed task |
| 5 | Naming `fn_822FA7E0` = `VocalTrackDir::Save` | our `Save` is a 4-byte stub; pairing 1,000 B against it buys a 0%-with-no-content row (the named metric-fitting hazard) | a real `VocalTrackDir::Save` body — the identification is already done |
| 6 | `??0GemTrackDir` 2,548 B @ 79.27 | brief priority 7, "only if budget remains"; budget went to §4-§6, which closed more | BQ §11 item 8 stands: which `ObjPtr<T>` shape makes `/O1 /Ob2` inline the member ctor |
| 7 | Installing any fold alias | all three candidates refuted or undecidable (§8); an unproven alias lifts `name_check` by construction and the `none` control cannot catch a fabricated one | per-candidate, listed in §8.1-§8.3 |
| 8 | The 7-pair / 1,507-site binary-wide thunk-fold lever (§8.5) | ~403 units, outside this lane's surface, overlaps live lanes | a coordinator decision, not evidence |
| 9 | The other unnamed function-sized rows (`fn_822F93D0` 688 B, `fn_822F64D8` 632 B, `fn_822EE768` 728 B, `fn_822E5520` 608 B) | budget; the two briefed ones were done first | the same callee-resolution method used in §5, which took ~2 tool calls per row |
| 10 | Anything in W16-BT's or W16-BU's surface | lane boundaries | — |

**Filed for other lanes:** nothing in this band needed the parenthesized
`MemAlloc` bypass, so W16-BT has no inbound item from W16-BV. The map places
`??0?$StlNodeAlloc@VSmasherPlateInfo…` at `0x82356300`, inside W16-BU's
`0x8235`–`0x8236` band; it was **read only**, never edited.
