# W16-FI — `ContentMgr_Xbox`: the array cardinality was wrong, not the field offsets

Base `b3171b99`. Ruler `name_check` (graded), read from `report.json`.
Authoritative measurement: `tools/ab_measure.py --from-dirty`.

## Result

```
Δmatched_code      +296 B      (4,139,696 -> 4,139,992)
Δmatched_functions +5
Δhonest            +3          (Δmasked_equal +2 -- do NOT quote +5 as honest)
Δcode%             +0.002892pp
unit net (ALL units) = +5, in exactly ONE unit: default/ContentMgr_Xbox (64->69 of 75)
units at 100%: 191 -> 191 (0 reached, 0 fell off)
```

Pre-registered before the run: `+296 B / +5 fns / exactly one unit moving`, with
five falsifiers. **All five held and the per-row attribution summed exactly**, so
the total is right for the reasons claimed and not by cancellation.

## The finding: `kNumberOfBuffers` is 6, not DC3's 7

The brief framed `Init`'s fuzzy 0.0 as "our two scratch members sit +4 from DC3's
(`unk938/unk93c` vs `unk934/unk938`)". That +4 is a **red herring** — it is a
consequence, not a cause. The cause is one character:

```c
-#define kNumberOfBuffers 7      // DC3
+#define kNumberOfBuffers 6      // RB3 retail
```

With `mEnumHandles` at `0x78` and `sizeof(XCONTENT_CROSS_TITLE_DATA) == 0x138`,
`0x78 + 4n + 0x138n == 0x7e0` has the **single integer solution n == 6**; at n == 7
`(0x7e0 - 0x94)/7` is not an integer, so no struct size rescues it.

**Six independent retail witnesses**, none of them arithmetic-only:

| witness | site | says |
|---|---|---|
| `li r11, 0x6` | `Init` clear-loop trip count | n = 6 |
| `li r28, 0x6` | `StartRefresh` cancel-loop | n = 6 |
| `cmpwi cr6, r28, 0x6; blt` | `StartRefresh` enumerator loop bound | n = 6 |
| `addi r10, r3, 0x7e0` / `addi r29, r30, 0x7e0` | `mOverlappeds` base | 0x7e0 |
| `mulli r11,r28,0x138; addi r4, r11, 0x90` | `&mXDatas[i]` | mXDatas at **0x90** |
| `addi r11, r28, 0x1e; slwi r11,r11,2` | `&mEnumHandles[i]` | 0x78 + 4i |

A seventh, structural: DC3's enumerator dispatch has **four** arms
(`i==4`, `i==5`, `i==6`, default); retail's has **three**. `i == 6` is
unreachable when n == 6, which is exactly what the image shows.

### This also CORROBORATES the previous lane's `unk70` filler
At DC3's `0x74` base the array would land at `0x7dc`, not retail's `0x7e0`. The
0x70 region and the 0x74/0x75 pair are both confirmed, from a constraint that
lane never used.

## Correction to the header: `0x70` is a BYTE, and it gates `StartRefresh`

The slot was declared `unsigned int unk70` as inert filler. It is neither inert
nor 4 bytes:

* `StartRefresh` opens `lbz r11,0x70(r3) / cmplwi r11,0x0 / beq <epilogue>` —
  a bool gating the **entire** body, evaluated before `mDirty` at `0x44`.
* the constructor (`fn_825213D0`) closes `li r9,0x1 ; stb r9,0x70(r30)` — it is
  initialised **true**.
* a 4-byte load would be `lwz`. It is `lbz`.

`0x71`-`0x73` are **unobserved** anywhere in this TU. They are spelled as bools
only to reproduce the measured `0x74`; that is not a claim that three more flags
exist, and the header says so.

⚠ **Trap for the next lane:** `fn_825208E0` does `lwz r3,0x70(r31)` and
`lwz r11,0x74(r31)`, which reads like a 4-byte member access contradicting all of
the above. It is not — there `r31` is the **frame pointer** (`subi r31, r1, 0xc0`),
so those are stack slots. In this codegen `N(r31)` is usually stack, and reading it
as `this` manufactures a layout conflict that does not exist.

## Four identifications (704 B of anonymous rows)

Added to `scripts/target_symbol_map.json`; all four bodies verified against retail
before naming, and no name collided with an existing entry.

| addr | size | name | evidence | result |
|---|---:|---|---|---|
| `0x8251f8f0` | 20 B | `?IsCorrupt@XboxContent@@UAA_NXZ` | `return field_0xc == 1` — matches the header's own retail note | **-> 100** |
| `0x825216e0` | 76 B | `??_GXboxContentMgr@@UAAPAXI@Z` | dtor call, `if (f&1) delete`, returns `this` | **-> 100** |
| `0x8251fdc8` | 424 B | `?Poll@XboxContent@@UAAXXZ` | reads `0x160` (mState) vs 1, `0x164` (mPadNum) vs 4/5 | 0 -> 89.65 |
| `0x825213d0` | 184 B | `??0XboxContentMgr@@QAA@XZ` | vtable store, 3 list inits, `stb 1,0x70` | 0 -> 23.52 |

This is the documented naming economics running in both directions at once: the two
rows whose bodies already matched **banked 96 B**, and the two that did not turned
an invisible `fuzzy 0.0` into an **adjudicable divergence** — which is where the
remaining work in this unit now is.

## Bodies corrected (source, not map)

* **`Init`** — retail's 120 bytes contain no `SystemConfig`/`FindData`/`FindArray`/
  `push_back` at all; DC3's `enumerate_save_game_exports` + `ignored_content` block
  is 50 base-side inserts of DC3-era code. Dropped. `Init` **crossed to 100**.
  Neither half sufficed alone: the layout fix by itself banked **0**, because
  `fuzzy` was already at the floor.
* **`StartRefresh`** — added the `0x70` gate; rebuilt the dispatch to retail's three
  arms; removed the `mEnumerateSaveGameExports` test (retail emits nothing there,
  consistent with `Init` never assigning it). **69.3605 -> 99.9571.**
* **`XEnumerateCrossTitle` returns `DWORD`, not `long`** — retail compares
  `cmplwi` (unsigned); a signed prototype gave `cmpwi`. Real defect.
* **`XboxContent::Poll`** — retail RELEASEs `mOverlapped` **before** branching on
  the result, and its failure arm is a bare `mState = kContentDeleting`. Our
  `XGetOverlappedExtendedError` -> `mCorrupt = err == 0x570` block does not exist in
  retail; the header's own note already recorded that **nothing in this TU touches
  `0x169`**, and our own code was the counterexample to it.
  Also `mState = (State)(res == 0)` was wrong: retail computes
  `xori 1; addi 7` = `kBackingUp(7)` / `kContentDeleting(8)`, which is also the
  semantically right pair under an `mState == kNeedsBackup(6)` guard.
  **0 (unpaired) -> 89.6509.**

### Measured NEGATIVE, kept in-source so it is not re-tried
Retail materialises the `ULARGE_INTEGER` argument on the stack
(`stw 0,0x60(r1)`, `stw 0,0x64(r1)`, `ld r10,0x60(r1)`) while `QuadPart = 0` keeps
it in a register. Assigning `HighPart`/`LowPart` separately to force the stack form
scored **WORSE — 87.764 -> 85.189 fuzzy** — and was reverted. The spill is driven by
something other than the shape of that initialisation.

## `StartRefresh` banks ZERO, and that is the honest outcome

It sits at **99.9571 with exactly two charged sites**, both `bl`:
retail names `list<Hmx::Object*>::insert`, we name `list<Content*>::insert`.
`matched_code` is all-or-nothing per row, so **932 B stay uncollected**.

There is **no source fix**: an alias group already exists at `0x823d14c0` with
survivor `list<Hmx::Object*>::insert` and four folded pointer-element siblings
(`void(*)()`, `char*`, `Dep<CharPollableSorter>*`, `CharClip*`). Ours is a fifth.
Retail's call site is literally `bl fn_823D14C0`, the survivor address.

**`tools/comdat_fold_gate.py` REFUSED it, and I did not install the alias anyway.**
The refusal is *chained*: our `insert` body is byte-equal to retail's except that its
inner `bl _M_create_node` resolves to a differently-spelled `_M_create_node`, itself
an unestablished fold (its own group exists at `0x82520150` with the same element-type
family, and `list<Content*>` is likewise absent). Gating the inner one:

> stage 1 (the substantive body compare) **PASSED** — "identical: 15/16 words compared
> as FULL 32-bit values, 1 relocated branch destination resolved through the map and
> name-equal". Stage 2 REFUSED.

### ⇒ Tooling gap, and it is structural
`comdat_fold_gate.py` does `fa = int(r["base_addr"], 16)` **unconditionally**. A
folded spelling that is **absent from `target_symbol_map.json`** — which is the
*strongest* CF1 case, "retail's map places F nowhere" — has **no representable
input**. Forced to supply a placeholder, the natural choice `base_addr = survivor`
then trips the documented `same_function(A, A)` vacuum that the tool's own selftest
measures at **43.8% of readable bodies**, producing a REFUSE that is an artifact of
the fabricated input rather than evidence.

I stopped there deliberately. Hand-installing an alias a gate refused is exactly the
integrity hazard that now underwrites ~22% of everything counted as matched, and the
`none`-ruler control cannot catch a fabricated alias (it reads flat by construction).
**The right fix is to give the gate a map-absent path, then re-run — the body
evidence is already sitting there.** Until then the 932 B are honestly uncollected.

## What I did NOT do

* **`PollRefresh` (824 B) — untouched, still 0.0.** Its DC3 oracle is itself unsolved
  (95.17) and ours is 105 lines vs DC3's 139, so it cannot be closed by transcription.
  It is the largest remaining row in the unit and needs its own lane.
* **`??0XboxContentMgr` (184 B, now 23.52)** — the 31 deletes say retail **inlined**
  `ContentMgr`'s constructor body while we emit a call to it. Fixing that means making
  `ContentMgr::ContentMgr()` inline, which ripples into the `ContentMgr` unit's own
  rows. Out of scope for a lane that had already changed a shared header; a
  cross-unit A/B should price it.
* **`Poll` (424 B, now 89.65)** — the residual is one instruction cluster around the
  `ULARGE_INTEGER` stack materialisation, whose obvious source form measured worse
  (above).
* **The six 44 B `fuzzy 99.5455 / mpn 100.0` rows** — genuinely the drained arg-only
  class; not worked, correctly.

## ⚠ Correction to the brief's own row classification
The brief called nine rows (384 B) "the DRAINED arg-only class -- DO NOT WORK THEM".
Only **six** (264 B) have `mpn == 100`. The three 40 B rows read `mpn == fuzzy` at
99.8/99.9 — sub-100 on **both** rulers, so they are not that class by its own
definition. **Two of the three then crossed to 100 as collateral of the layout fix,
paying 80 B** — 27% of this lane's total. A row misfiled into a closed class is
invisible to the lane told to skip it.
