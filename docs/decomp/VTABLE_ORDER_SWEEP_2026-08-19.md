# Retail-vs-ours vtable order sweep — 2026-08-19

> **STATUS (2026-08-19): CURRENT.** Builds the instrument the killed V1-VTORDER
> lane specified but never committed (`LANE_SALVAGE_VTABLE_STRUCT_2026-08-18.md`
> F1/F2). Tool: `tools/vtable_order_sweep.py`.
> **RESULT: 0 confirmed declaration-order bugs** over the 369 adjudicable
> vtables, with 305 agreeing — a negative result on a *validated* instrument.
> ⛔ It covers **369 of 2,220** retail vtables. Read §4 before quoting it as a
> clean bill.

## 0. ★ The instrument mostly already existed — `tools/retail_rtti.py`

The salvage doc presented "identify the vtable by RTTI (`??_R4` COL →
TypeDescriptor → class-name string)" as a construction to be built. **It was
already in the tree**, unified and controlled, and its own docstring says why:

> *"At least three lanes in one session independently re-wrote a retail RTTI
> resolver, and two of them baked in the wrong address arithmetic. Import it,
> don't re-derive it."*

Its 8/8 controls pass, including the one that matters — `va - 0x82000000` is
valid **only** for `.rdata`, and a `.data` TypeDescriptor read that way yields a
*false absence*. This lane wrote no address arithmetic of its own.

★ **The reusable lesson is the standing directive demonstrating itself: read the
in-tree record before building the thing a brief says to build.**

## 1. What the sweep does

| side | source |
|---|---|
| retail | a `??_R4` COL sits at `vtable[-1]`; the COL names the class. One linear pass over `.rdata` finds **2,220** vtables — matching CLAUDE.md's independently-measured COL count exactly. |
| ours | the `??_7<Class>@@6B@` symbol's relocations in our compiled COFF (**1,319** indexed). |

Verdicts: `PERMUTED` (same name multiset, different order — the prize),
`SET_DIFFER`, `SAME`, `UNRESOLVED`, `AMBIGUOUS_MULTI_VTABLE`.

## 2. Result

| verdict | primary only | **+ multi-vtable join** |
|---|---:|---:|
| **PERMUTED** | 2 | **2 — both REFUTED (§3)** |
| SET_DIFFER | 62 | 85 |
| SAME | 305 | **400** |
| UNRESOLVED | 545 | 1,323 |
| ⛔ AMBIGUOUS_MULTI_VTABLE | 1,306 | **410** |
| **adjudicated** | 369 | **487** |

★ **The multi-vtable join is principled, not a guess.** `COL.offset` is the
vftable's offset within the complete object, and the ClassHierarchyDescriptor's
BaseClassDescriptors carry each base's `mdisp` — so matching `offset → mdisp`
names the subobject, which is exactly what our COFF encodes in
`??_7Class@@6B<Base>@@@` (509 of 1,736 of our classes emit more than one).
That moved 896 vtables out of the refused bucket. It did **not** find new bugs.

## 3. Four instrument defects, each caught by a control that could fail

★ **`SAME = 0` was the tell.** The first full run reported `PERMUTED 1 /
SET_DIFFER 472 / SAME 0`. **An instrument that never returns agreement is
reporting difference by construction** — the numbers looked like a huge finding
and were an artifact.

1. **Off-by-one.** Our COFF vtable symbol includes the `??_R4` COL as entry 0;
   the retail table is read from *after* the COL. `WinSockSocket` showed it
   exactly (`ours=18, retail=17`, every slot shifted). Fixed ⇒ `SAME 0 → 327`.
   The single `PERMUTED` from this run (`MetaMusicLoader`) **vanished** — it was
   pure artifact.
2. **Vtable over-read.** "Keep going while the word is a function VA" runs past
   the end, because `.rdata` is full of function-pointer tables — it read
   `FilePath`'s vtable into a `String::operator+=`. Since every vtable start is
   enumerated, the honest bound is the next start minus its COL slot.
3. **ICF fold, across vtables.** One address is a slot in **hundreds** of
   distinct vtables (top address: 1,433), and the map can name it with only one
   arbitrary survivor spelling. Comparing such a slot by name conflates *folded*
   with *wrong* — the same disease as objdiff's `LINKER_MERGED` verdict.
4. **ICF fold, WITHIN one vtable — the subtle one.** The filter in (3) counts
   *distinct vtables* per address, so it misses two slots of the SAME table
   holding one address. `MCContainerXbox::Format()` and `Unformat()` are both
   `{ return (MCResult)0xD; }` ⇒ byte-identical ⇒ folded, so retail slots 9 and
   10 hold one address and the map calls it `Format`. It was reported as a
   `Format vs Unformat` defect **that does not exist**. Fixed ⇒ SET_DIFFER
   65 → 62.

### Both PERMUTED candidates are refuted — by different controls

- **`UIFontImporter`** claimed a 5-slot rotation of `Hmx::Object`'s virtuals.
  **Refuted by a control**: `RndSet` (verdict SAME, zero mismatches) pins the
  true order — `4=ClassName, 5=SetType, 6=Handle, 7=SyncProperty, 8=Save,
  9=Copy, 10=Load` — and **our `UIFontImporter` matches that exactly**. It is
  retail's `0x821279e4` that differs, so that table is a different subobject's
  adjustor vtable. The claim was also impossible on its face: a wrong
  `Hmx::Object` order would break essentially every match, and 44,514 match.
- **`StreamReceiver360`** (`GetPlayCursor`/`PlayImpl` at slots 13/14).
  ⛔ **Refuted by the in-tree record, which is better evidence than the sweep.**
  `src/system/synth/StreamReceiver.h` already carries a prior lane's comment:
  retail order is `PauseImpl @0x30, PlayImpl @0x34, GetPlayCursor @0x38`,
  *verified from call-site dispatch* (retail `StreamReceiver::Play` @
  `0x8272A3C0` dispatches 0x30 then 0x34), with rb3-Wii corroborating and DC3
  identified as the one that moved it.
  The sweep's names come from `target_symbol_map.json`, which was generated by
  matching against the oracle — so if those entries were assigned **positionally
  under DC3's order**, the "permutation" is circular. Adjudication was attempted
  and **failed cleanly**: both slots are 16-byte *adjustor thunks* with identical
  bodies differing only in the tail-branch target, and per the project's own
  thunk rule the identity is the branch target — but `0x82b65160` is **unnamed**
  and `0x82b66c48` maps to an `operator new` (a fold survivor). **No source
  change was made.**

## 4. ⛔ Coverage — do NOT read this as "vtable order is fine"

**1,851 of 2,220 vtables were not adjudicated.**

- **1,306 AMBIGUOUS_MULTI_VTABLE.** 440 of 1,354 retail classes have more than
  one vtable (multiple/virtual inheritance), while our COFF exposes a single
  `??_7X@@6B@`. Comparing them aligns two *different* tables and manufactures
  permutations, so they are refused rather than scored. **Closing this needs our
  secondary vtable symbols (`??_7X@@6BY@@@`) matched to the right retail table.**
- **545 UNRESOLVED** — fewer than 2 slots carry a comparable name.
- **ICF is the binding constraint, not name coverage.** Of 27,905 retail slots,
  **90.2% have a map name** but only **24.3% are unfolded**, leaving **20.4%
  comparable**. For folded slots retail itself destroyed the distinction and no
  instrument can recover it — the same irreducibility as the 129,360 alias
  pair-bytes.
- The 62 SET_DIFFER are dominated by fold-alias naming (e.g. `ObjectKeys` slot
  14 reads `Save@FloatKeys` vs our `Save@ObjectKeys` — one folded `Keys<T>::Save`
  instantiation). Not triaged individually.

## 4b. ★★ A BY-PRODUCT WORTH MORE THAN THE SWEEP: a map-defect detector

**Vtable membership PROVES virtuality.** A slot's function must be declared
`virtual`, so an MSVC access class of `Q`/`A`/`I` (non-virtual) or `S` (static)
on a vtable slot is impossible. `--map-audit`:

| plain named unfolded vtable slots | 2,929 |
|---|---:|
| virtual (`E`/`M`/`U`) — **the control** | **2,829 (96.6%)** |
| **non-virtual/static ⇒ MAP DEFECT** | **47 (1.6%)** |

The 96.6% is what makes the residue meaningful rather than detector noise.
Examples: `?StaticByteCode@NetPushScreenMsg@@SAEXZ` (**static**) in slot 6 of
four different message classes; `?DisplayName@MemcardXbox@@QAAPA_WXZ` (public
non-virtual) in slot 27; `?MoveBeat@MoveDir@@QBAHXZ` in slot 3.

Mechanism is usually an **ICF fold where the map recorded the non-virtual twin**
(`StaticByteCode` and a virtual `ByteCode` returning the same constant are
byte-identical). Under `name_check` our source then spells the *other* member of
the fold and is charged as a wrong callee — so these are actionable, not
cosmetic.

⚠ **This number was 1,379 (29.3%) before excluding adjustor thunks** — their
`@@$4PPPPPPPM@A@` displacement encoding sits between the `@@` and the real
access class, so a naive scan reads a letter out of the *thunk* and reports
every one as non-virtual. A 29.3% "defect rate" that is 96% artifact is exactly
the shape of a finding that should not be shipped.

## 5. Leads left open

- ★ **`target_symbol_map.json` is a suspect for `StreamReceiver360`.** Its
  `GetPlayCursor`/`PlayImpl` entries may be stale, assigned under DC3's vtable
  order before the header was corrected. Both thunks' branch targets
  (`0x82b65160`, `0x82b66c48`) are unidentified — identifying them settles it,
  and a stale map entry corrupts any analysis keyed on those names.
- The multi-vtable class (1,306) is the single biggest coverage win available.
- No source, header or map was changed by this lane.
