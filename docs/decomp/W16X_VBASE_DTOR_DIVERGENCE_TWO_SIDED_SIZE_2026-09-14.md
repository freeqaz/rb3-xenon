# W16-X — the vbase-dtor divergence, and a size reader that reads both sides

Lane W16-X, branch `w16-x`, worktree `/home/free/tmp/wt-w16-x`, based on main
`11fc1234`.

**Ruler note up front.** objdiff-cli was swapped 4.2.8 → 4.2.9 *during* this lane
(funclet reloc-target-NAME tiebreaker, lane W16-W). Item 1's A/B is entirely on
4.2.8 (`provenance.tool_binary_hash = 14ac591a0814e6c9` on both legs). Everything
after it is entirely on 4.2.9 (`5a51cd51fe0a353f` on both legs). **No before/after
in this document chains the two.** The tool change alone is worth about
+1 function / +24,352 B in anonymous funclet rows, so a cross-swap delta would
have fabricated exactly that.

---

## Item 1 — the 80 B `??_G` bodies: a user-declared destructor, not the override set

W16-U found `??_GGemTrainerLoopPanel` and `??_GTourChallengeResultsPanel` at 80 B
against retail's 68 B `??_GUIPanel`-shaped survivor they are aliased to, called
them REAL our-side divergences, and deliberately did **not** act — because the
argument for acting was a *size* argument, and the STLPORT-1 trap is exactly a
size argument made with a one-sided reader. That caution was correct and this lane
did not resolve it by asserting the size; it read retail's bytes.

### Retail's body

The thunk at `0x82b908d8` branches to `0x82b908e8`, a **68 B** body calling
`??1UIPanel` (`0x82814358`), `??1Object@Hmx@@` (`0x8275cbf0`) and operator delete
(`0x8240ddb0`). There are **no derived-class vptr stores** in it.

### The hypothesis that was refuted first

"These classes override nothing, so their `??_G` is `??_GUIPanel`." **False.**
Retail RTTI gives four *distinct* vtables — `0x820dc5ec`, `0x8219a7c4`,
`0x8219d63c`, `0x821263d4` for GemTrainerLoopPanel / TourChallengeResultsPanel /
TrackPanelInterface / UIPanel — all holding slot 0 = `0x82b908d8`. A slot-by-slot
diff shows they **do** override `ClassName`(4), `SetType`(5), `Handle`(6) and
`Poll`(29), and that slot 21 is the per-class RTTI COL pointer, which is why the
vtables do not ICF-fold even where the function pointers agree. So the override
set is not the variable.

### The compiler probe (the decisive instrument)

Five variants compiled with the exact project cflags (`/O1 /Oi /GR /EHsc /TP`),
each a UIPanel subclass:

| probe | shape | `??_G` | `??_D` | `??1` |
|---|---|---:|---:|---:|
| A | GemTrainerLoopPanel's exact shape, inline `~A(){}` | **80** | 112 | 60 |
| B | identical but **no user-declared destructor** | **68** | 56 | 4 |
| C | destructor declared in class, defined out of line | 80 | 112 | 60 |
| D | no ClassName/SetType overrides, keeps Poll + inline dtor | 80 | 112 | 60 |
| E | overrides nothing at all, inline dtor | 80 | 112 | 60 |

⇒ **The single variable is the presence of a user-declared destructor, not the
override set.** A user-declared dtor — inline, out of line, with or without
overrides — forces the vptr-restore prologue into `??1X` (4 B → 60 B), which
inlines into `??_DX` (56 B → 112 B), which is then too big to inline into `??_GX`
(68 B → 80 B).

### Two-sided census, with a control that fires

All 54 UIPanel subclasses, both sides: **33 resolve on both sides, 28 agree, 5
diverge — all the same way.** `ConnectionStatusPanel` is the discriminating
control: retail is **80 B** there, via a real `??_DConnectionStatusPanel` at
`0x827ba378` that is named in the map. Ours is 80 B. It **agrees**, and it was
deliberately left untouched. Retail demonstrably emits *both* shapes, so the rule
is not firing on everything — which is what makes the 5 divergences a finding
rather than a blanket rewrite.

### What changed

Five headers, each `virtual ~X() {}` removed with the reasoning recorded in place
(`b5ed6f8d`): `src/band3/game/GemTrainerPanel.h`,
`src/band3/tour/TourChallengeResultsPanel.h`,
`src/band3/meta_band/CampaignSongInfoPanel.h`, `src/band3/game/FadePanel.h`,
`src/band3/meta_band/MultiSelectListPanel.h`.

**5/5 shape predictions exact**: GemTrainer 68, TourChallenge 68,
CampaignSongInfo 88, FadePanel 88, MultiSelectList 88, every `??1` down to 4/8 B
and every `??_D` to 56 B. GemTrainer's and TourChallenge's relocations are now
identical to `??_GUIPanel` **including target names**, so alias gi=1627's two
flagged memberships are now real folds rather than forgiven divergences.

**Measured Δ 0 functions / 0 B** by set-diff of the `fuzzy == 100` row set,
same-ruler 4.2.8 on both legs: 0 rows crossed in, 0 fell out. Landed on merit —
the alias was already forgiving the divergence, so correcting it was never going
to pay in bytes.

**Predicted and missed:** `??_GMultiSelectListPanel@@UAAPAXI@Z` was pre-registered
at +1 function / +88 B and measured +0/+0. The row is named and paired at 88 B but
sits at `fuzzy 87.954544`; its only relocation-name difference
(`??3@YAXPAX@Z` vs `??3BinStream@@SAXPAX@Z`) is *already* a folded member of alias
gi=1539 @`0x8240ddb0`, so why the forgiveness does not apply is **unresolved**. It
has not been re-checked since the 4.2.9 swap.

---

## Item 2 — `tools/twosided_extent.py`

Our side reads the COMDAT `.text` span with EH funclets **excluded and reported
separately** (`funclet_bytes`); retail's side reads `.pdata` BeginAddress..end,
which never included a funclet to begin with. Before this, the two sides were not
the same measurement — which is precisely how STLPORT-1's phantom "+8 B STLport
source bug" survived a two-sided control: **a size test cancels a one-sided reader
artifact on both sides, so it cannot catch one.**

`selftest()` has three checks and a control that can fail:

```
(1) POPULATION over 17293 mapped symbols with both extents:
      naive  our-side reader agrees with retail .pdata:  10825
      NORMALIZED our-side reader agrees               :  16582
      repaired by normalization                       :   5758 (5758 of them funclet-bearing)
      BROKEN by normalization                         :      1
(2) KNOWN ANSWER _Copy_Construct @0x823d3ac8 (W16-S):
      our=60 retail=60 naive=104  OK
(3) GENUINE mismatch must STAY a mismatch (??_GUIPanel 68 B vs retail 0x827ba3f8 = ??_GConnectionStatusPanel 80 B):
      NE_SIZE our=68 retail=80  OK
SELFTEST PASS
```

It **refuses to pass** if `norm_eq <= naive_eq` or if `repaired == 0`, so a
normalization that does nothing cannot report success.

### (a) `0x827f42a8` — alias group 1621

`0x827f42a8` is a **12 B vtordisp thunk and not a `.pdata` BeginAddress** ⇒
`NO_PDATA`: **no size claim was ever licensed at that address.** The fold
destination `0x827f5348` is 80 B, and our `??_GUIButton` and `??_GUILabel` are
both 80 B with `funclet_bytes == 0` ⇒ **EQ_SIZE under the naive reader and the
normalized one alike.** The SIZE argument evaporates.

Underneath it sits a real divergence. Masked bodies are identical (18 of 20 words
survive masking — strong anti-vacuity), but relocation **names** differ at +32 and
+48. Closure condition (a) holds: `??_DUIButton` / `??_DUILabel` are 108 B,
masked-identical, relocations identical **including names**, already alias gi=1135
@`0x827f4e58`. Condition (b) fails **on our side only**: we spell
`??3UIButton@@SAXPAX@Z` (a 4 B `b ?MemFree@@YAXPAX@Z`, alias gi=1539) where
retail's single body calls `?MemFree@@YAXPAX@Z` (`0x827bc430`) **directly** at +48.

Cause: `src/system/ui/UIButton.h` used `OBJ_MEM_OVERLOAD`, which is
`__declspec(noinline)`. Switched to `OBJ_MEM_OVERLOAD_INLINE_DEL` — the per-class
call `src/system/utl/MemMgr.h` explicitly asks to be decided on retail bytes.
`??_GUIButton@@UAAPAXI@Z` has **no report row at all**, so this was pre-registered
at **Δ0 and measured at Δ0**. This is the MAPID-1 shape: the payout is bug
exposure, not bytes.

### (b) the 68 `survivor_vs_retail == SIZE` rows — 0 artifacts

| class | rows |
|---|---:|
| **REAL size mismatch (survives normalization)** | **44** |
| **NO_PDATA — not a `.pdata` BeginAddress, no size claim licensed** | **24** |
| **READER ARTIFACT (repaired by normalization)** | **0** |

**W16-S's SIZE verdicts are not a repeat of the STLPORT-1 trap.** Normalization
did move four distinct addresses' our-side readings — `_M_allocate_and_copy`
152→100, `__uninitialized_copy` 160→96, `Automator` 304→132, `_Copy_Construct`
104→60 — it simply moved none of them *into* agreement.

The honest correction is a different one: **24 of 68 rows (35%) carry a SIZE
verdict at an address that has no `.pdata` record**, i.e. the census made a size
claim it had no ground to make. Not wrong-by-artifact; unlicensed.

Three of the 44 are our-side **stubs**, not fold questions, and want bodies rather
than aliases: `?Terminate@RndMat@@SAXXZ` (our 4 / retail 96),
`?CancelOutstandingCalls@RockCentral@@QAAXPAVObject@Hmx@@@Z` (12 / 128),
`?deallocate@?$StlNodeAlloc@VString@@@stlpmtx_std@@QBAXPAVString@@I@Z` (8 / 128).

The brief licenses action only where the normalized reading yields a **named**
correct spelling the same base obj defines. With 0 artifacts, **Item 2(b) lands
nothing** — that is the result, not a shortfall.

---

## Item 3 — `ourside_fold_sweep.py` re-run after the `comdat_bytes` fix

Re-ran on the built tree: **686 rows, 652 ADMIT, 34 REFUSE.**

**The premise did not hold, and that is the finding.** Of the 652 ADMIT pairs,
**0 are funclet-bearing on either side** — so the `comdat_bytes` funclet-billing
fix is a **no-op on this instrument's population**, and there is no
"new admissions caused by the fix" set to prove. The sweep's candidates are short,
funclet-free bodies by construction. The re-run was still the only way to know
that.

What the re-run *does* show is coverage growth: 374 ADMIT pairs are outside the 53
groups whose `evidence` begins `tools/ourside_fold_sweep.py`, and **80** have a
folded spelling in **no alias group at all**.

### The screen

652 ADMITs is far too many to install, and the tool's own docstring warns that
our-side identity on a short body is cheap. Applied: **body ≥ 32 B and ≥ 2
relocations on the folded side.** That kills **78 of the 80** and keeps **2**.

### The 2 installed, each T1-proven

| address | survivor ← folded | our S/F | retail `.pdata` | bytes eq | reloc names eq |
|---|---|---:|---:|---|---|
| `0x82296728` | `_M_clear_after_move` ← `_M_clear`, `vector<CameraManager::PropertyFilter>` | 116 / 116 | **116** | yes | **yes** |
| `0x82387a28` | `_M_clear` ← `_M_clear_after_move`, `vector<CharEyes::CharInterestState>` | 112 / 112 | **112** | yes | **yes** |

Both carry two *named* relocation targets (the per-`T` `__destroy_range_aux` and
`?MemOrPoolFreeSTL@@YAXHPAX@Z`), compared by name and equal. Anti-vacuity holds:
these are 116/112 B bodies, not relocation-free thunks where masking proves
nothing.

### Refusals

- **78 of the 80** uncovered admissions: 4–8 B bodies, or fewer than 2
  relocations. Our-side identity is cheap there and proves nothing. Includes the
  88 B `?clear@_List_base<PassiveMessage*>` ← `_List_base<SynthPollable*>` pair,
  which is large enough but carries fewer than 2 relocations *and* is a
  cross-type pair — refused on both counts.
- **The 34 REFUSEs the sweep emitted itself**, with its own reasons: `map places
  F at 0x…` (the map already contradicts the fold) and `F already aliased at
  [...]`.

### Predicted vs measured

Pre-registered **Δ0 / Δ0 for both groups** — neither row is paired in
`report.json`, so the only channel that could pay is a *caller* whose sole
remaining charge is that relocation name, which I named and then judged unlikely.

**Measured +1 function / +324 B.** A miss, recorded as one. The mechanism is
exactly that channel: `?_M_insert_overflow_aux@?$vector@UCharInterestState@CharEyes@@…`
(324 B, `default/CharEyes`) crossed because it calls `_M_clear_after_move` and
that name was its last charged site. The **sign** prediction held — forgiveness
only removes charges, so Δ ≥ 0 structurally.

---

## Measures

Set-diff of the `fuzzy == 100` row set, **4.2.9 on both legs**
(`provenance.tool_binary_hash = 5a51cd51fe0a353f`):

| | matched_functions | matched_code |
|---|---:|---:|
| before (baseline on unchanged tree, 4.2.9) | 43,291 | 3,986,960 |
| after | **43,292** | **3,987,284** |
| Δ | **+1** | **+324** |

Crossed in: 1 row / 324 B. Fell out: **0 rows / 0 B**.

Item 1's own A/B, **4.2.8 on both legs** (`14ac591a0814e6c9`): **Δ 0 / 0**, 0 in,
0 out.

## Gates

```
BUILD rc=0
RULER rc=0        OK: both objdiff-cli entry points resolve the same ruler.
OBJS  rc=0        [patch-state] OK: 1212 decomp, 3101 target objects match (tree_sha256=edc1e4b6e90de659)
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

## NOT done

- **Item 4 (`UNDECIDED_MASKED`, 643 memberships / 41,360 B, top 10 by forgiven
  bytes)** — not started. Declined for budget. It is a MAPID-1-lever batch and
  wants a lane of its own; nothing here blocks it.
- **`??_GMultiSelectListPanel` at `fuzzy 87.954544`** — byte-identical to retail's
  88 B body in every non-relocated word, its one relocation-name difference
  already a folded member of gi=1539, and still not forgiven. Undiagnosed, and not
  re-checked since the 4.2.9 swap.
- **The 44 REAL size mismatches from Item 2(b)** — adjudicated as real, none
  acted on. The brief licensed action only where normalization produced a named
  correct spelling, and it produced none. Three are our-side stubs needing bodies.
- **The 24 NO_PDATA rows** — flagged as carrying an unlicensed size claim, not
  re-adjudicated on another instrument.
- **650 of 652 sweep ADMITs** — not installed, and deliberately so. The screen
  above is the reason; a T1 proof per row at that volume was not affordable, and
  installing unproven aliases lifts the score by construction.
- **`ConnectionStatusPanel`** — left untouched on purpose. It is the control that
  makes Item 1's rule falsifiable; "fixing" it would have destroyed the evidence.
