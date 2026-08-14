# WRONGCALL-3's 100 "fold-shaped" pairs, adjudicated

**Lane FOLDPROVE-2, 2026-08-14, on `efebb9c6`.** Lane WRONGCALL-3 (`1ad913dc`)
re-tested NOGROUP-1's 313 "wrong-callee source defects" on the **masked body alone**
and found **100 pairs / 21,912 B (54% of the queue's bytes)** whose masked body is
BYTE-IDENTICAL — differing only in relocation **target names**, which is true **by
construction** for two template instantiations over layout-compatible types. It
explicitly excluded them from source work and left them for a fold lane.

Reproduced here exactly: **100 pairs / 21,912 B.**

## Result

| class | pairs | solo_B |
|---|---:|---:|
| REFUTED — residency (`addr(F) != addr(S)`) | **37** | 4,428 |
| REFUTED — shape not unique in retail | **31** | 2,612 |
| REFUTED — recursive relocation inconsistency | **19** | 11,836 |
| **FOLD PROVEN + installed** | **7** | **2,468** |
| UNDECIDED | 5 | 568 |
| MAP_REPAIR_CANDIDATE (reported, not repaired) | 1 | 0 |

**87 of 100 refuted. Measured +2,564 B.** Per-pair queue with the reason each was
rejected: **`docs/decomp/foldprove2-fold-shaped-100-adjudicated.tsv`**.

## ⛔ The cheap kill first — 37 die before any body work

FOLDPROVE-1's lesson, applied as the lane's first action
(`tools/foldprove2_cheapkill.py`, all gates evaluated **non-short-circuited**):

**37 pairs / 4,428 B have `addr(F) != addr(S)`.** Retail keeping two addresses **is**
the definition of not folded, so those can never be aliased — an alias there would
forgive a genuinely wrong callee. They keep the wrong-callee reading and belong to the
source/map queue, not here.

⚠ **Uniqueness on the FULL signature killed ZERO, and that is a real reading rather
than a dead gate** — the control says **2.65% of retail body signatures (1,588 of
60,009) are held by >1 address**, so it *can* fire. It is simply near-tautological on
a post-ICF binary: a signature that includes relocation **names** describes exactly
what ICF already folded away. Checking that the gate could fail is what made "0" worth
reporting.

⛔ **Instrument trap, caught only by a count that disagreed.** The lane's first
cheap-kill run reported **100/100 killed, 0 survivors** — precisely the answer it was
primed to expect. It was entirely an artifact: **a fresh worktree's reflinked target
objs are PRE-RENAMER**, so every mangled retail name reads as absent and every pair
reports `absent_body`. The tell was the symbol count (69,438 vs main's 69,415); after
building the worktree so `obj_target_symbol_renamer` ran, it matched main exactly.
**Any COFF-symbol-name analysis in a freshly-created worktree is silently vacuous, and
its vacuity is shaped exactly like a decisive kill.**

## ★ The gate this population needed: uniqueness on the MASKED body

The full-signature gate is the wrong ruler here. The right one is CD-7's
**shape-identical** axis — index retail by `(size, masked bytes)`, *ignoring*
relocation names:

| retail addresses holding this masked shape | pairs | solo_B |
|---|---:|---:|
| **1** | **20** | 13,324 |
| 2 | 4 | 356 |
| 3–4 | 2 | 92 |
| 5+ | 74 | 8,140 |

**Only 20 of 100 have retail keeping one body of their shape.** For the other 80,
retail demonstrably kept several — and CLAUDE.md already says why that is *expected*
and does **not** indicate folding: `_List_base<T>::clear` has **42 addresses** with
reloc-identical surplus **0**, because its members differ in per-`T` node
deallocators. Shape-identical bodies at distinct addresses are the norm, not evidence.

## ★★ This refutes WRONGCALL-3's own headline proof case

The queue's **#1 row, 8,212 B** — `map<CRC,float>::operator[]` ← `map<int,float>::operator[]`
— is **not a fold**. WRONGCALL-3 argued at length:

> `Hmx::CRC` is a lone `int mCRC` whose `operator<` is bit-identical to `less<int>`, so
> `map<CRC,float>` and `map<int,float>` **cannot differ in a single instruction**, and
> measured they do not.

That is correct **about the masked body** and does not reach the fold criterion, which
requires identity **including relocations**. Measured: retail keeps **six distinct
addresses** for masked-identical `_Rb_tree::insert_unique` bodies —

| spelling | addr | size |
|---|---|---:|
| `_Rb_tree<CRC,…>::insert_unique` | `0x82271e70` | 232 |
| `_Rb_tree<CRC,…>::insert_unique` | `0x82271f58` | 488 |
| `_Rb_tree<int,…>::insert_unique` | `0x82594990` | 232 |
| `_Rb_tree<int,…>::insert_unique` | `0x8233c668` | 488 |
| `_Rb_tree<int,…>::insert_unique` | `0x826da558` | 232 |
| `_Rb_tree<int,…>::insert_unique` | `0x826da9a8` | 488 |

— same size, masked bytes equal, relocation names differing. **That family was not
folded**, so the parent `operator[]` pair cannot be one either. The 8,212 B was never
collectable as an alias.

⇒ **Both of the queue's readings of this row were wrong in opposite directions.**
NOGROUP-1 called it a source defect (it is not — do not "fix" `SongData::mRangeShifts`
to `map<CRC,float>`; that would break working code, as WRONGCALL-3 correctly warned);
WRONGCALL-3 called it fold-shaped (it is not a fold either). It is a **map-or-source
question about two genuinely distinct retail bodies**, and it stays open.

## The channel this population needed: RECURSIVE relocation consistency

`tools/fold_recursive_probe.py`. These pairs' **entire evidentiary gap is the
differing relocation slots**, so no body comparator can ever decide them. But
`/OPT:ICF` folds only COMDATs identical *including relocations*, so a parent fold is
real only if every differing slot resolves to one address — **the fold is recursive**.
That makes each slot decidable and, crucially, **refutable**:

* both slot targets map-resident at **different** addresses ⇒ REFUTE (FOLDPROVE-1's
  residency kill applied one level down, where it had never been run) — 8 slots;
* our slot target not map-resident ⇒ recurse on the callee bodies; different size or
  different masked bytes ⇒ REFUTE — 13 slots;
* retail slot target is a `fn_`/`lbl_` placeholder ⇒ UNRESOLVED (35 slots), never
  counted as support.

Verdicts over the 63 cheap-kill survivors: **37 SUPPORTED / 19 REFUTED / 7 UNDECIDED.**

## The two installed, each on ≥2 independent channels

**`vector<T>::operator=` @ `0x82bab758`, 260 B ← 6 spellings (+2,456 B).** Element
types `float`, `Tail*`, `UIScreen*`, `RemoteBandUser*`, `RndAnimatable*`,
`DummySample*` — all 4-byte trivially-copyable, the class `/OPT:ICF` folds.

1. **Masked-body uniqueness = 1** — retail holds exactly one 260-B body of this shape
   while our source emits **seven** instantiations.
2. **Recursive relocation consistency** — the single differing slot
   (`_M_allocate_and_copy`) resolves to retail's one body at `0x82b670a0`
   (masked-dupes 1); our spelling is not map-resident.
3. ★ **Heterogeneous fan-in — 57 call sites, and the callers NAME THE ELEMENT TYPE IN
   THEIR OWN MANGLED SIGNATURE:**
   `TalkyMatcher::LoadEvents(const vector<float>&)`,
   `SessionMgr::GetWaitingUsers(vector<RemoteBandUser*>&)`,
   `NonDestructiveTransitionEvent::ctor(…, const vector<UIScreen*>&)`,
   `SavedSetlist::SetSongs(const vector<int>&)`. One address cannot be four source
   functions.

★ Channel 3 does something the other two cannot: it **pins membership for individual
spellings**. Byte identity says "something folded here"; a caller whose own signature
says `vector<float>` says *which*. This is FOLDPROVE-1's finding #6 (the `LiteralSym`
3==3 count match) arriving through a different instrument. The rows that paid are the
very callers the probe named — `LoadEvents@TalkyMatcher`, `GetWaitingUsers@SessionMgr`,
`NonDestructiveTransitionEvent`.

**`vector<T*>::_M_fill_insert_aux` @ `0x82272798`, 328 B ← `vector<Object*>` (+108 B).**
Masked-body uniqueness = 1, plus retail's own
`_M_fill_insert@vector<Object*>` calls the address the map labels
`vector<RndDrawable*>` — impossible without folding, and it names exactly the spelling
being aliased.
⚠ **The recursive probe returns SUPPORTED here and was deliberately NOT banked as a
third channel:** its only differing slot is a **self-recursive tail call**, so the test
degenerates to re-comparing the parent against itself and carries zero independent
information. Excluded from the witness rather than counted.

## Reported, not repaired — for the map lane

`0x826f9540` — map says `?resize@?$vector@UDepthBuffer3DAttachment@@…`, we say
`?resize@?$vector@VVocalScoreCache@@…`. Its **sole** retail caller is
`?PostLoad@Singer@@QAAXXZ`. A vocals class resizing a vector of *depth-buffer
attachments* fits nothing; `vector<VocalScoreCache>` fits it exactly. But the fan-in is
**homogeneous** (one caller), so it yields no fold evidence, and both "fold" and "wrong
map name" explain the observation. Worth **0 B** either way. **Left charged rather than
guessed at** — handed to lane WRONGCALL-4, which owns `scripts/target_symbol_map.json`.

## Measured

Pre-registered before the run, priced **jointly** from `report.json` under the
all-or-nothing rule (12 rows / 2,564 B) — *not* by summing the queue's
`solo_closable_B` column, which gives 2,468 B because rows carrying **two** of the
installed pairs are missing from a solo sum:

```
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.024842pp  Δcode_bytes=+2564
```

Hit exactly. Both legs at the `symbols.txt` split **fixed point after 0 extra
re-splits**; `renamer_patched=1821`. Units at 100% [fuzzy ruler] **127 → 128**, 0 fell
off. **Composition verified**: `--revert 634c72dc` measured exactly the negation
(`−2,564 B`, `−0.024842pp`, `FlowMultiSetProperty` falls off).

`VALIDATE: PASS` — **1,322 map-consistent, 202 tolerated, 0 CONTRADICTED**, 1,528
total; **no `contradiction_exempt` used** (the 4 shown are pre-existing).

`ALIAS_SUSPECT` fired (default ruler up, `none` flat) — the documented shape of *every*
map-only patch. The `none` control **cannot** clear an alias (flatness is the signature,
not a clearance), so it is answered per group on retail bytes in the `witness` field.

**No `src/` movement, so the native gate is not applicable to this lane** (stated rather
than skipped silently). Lane WRONGCALL-4 owns `scripts/target_symbol_map.json`
concurrently and was not touched.

## Reusable findings

1. **The cheap kill paid again, and a lane should run it before pricing itself.** 37 of
   100 died on a map lookup that costs nothing. Combined with the shape gate, **87 of
   100 were refuted without any adjudication of the bodies.**
2. **A gate that fires zero times needs a base-rate control before you report the
   zero.** Full-signature uniqueness killed 0 here; measuring that 2.65% of retail
   signatures *are* multiply-held is what converts "0" from a suspected dead gate into
   a finding.
3. ★ **Match the uniqueness ruler to the fold question.** Post-ICF, uniqueness on a
   signature *including* relocation names is near-tautological. Uniqueness on the
   **masked** body is the gate with teeth — and it refuted **80** of these 100.
4. ★★ **A fold criterion is RECURSIVE, and that makes instantiation pairs decidable
   after all.** "The bodies differ only in per-instantiation callee names" is not the
   end of the analysis; it is a pointer to the next level, where the question is
   answerable and frequently *refutable*.
5. **A correction can itself need correcting, in the other direction.** WRONGCALL-3 was
   right that the 8,212 B row is not a source defect and right to forbid "fixing" it —
   and wrong that it is fold-shaped. Both readings of the same row failed; the row is
   still open.
6. **A freshly-created worktree cannot answer any question keyed on retail symbol
   NAMES until it has been built.** Pre-renamer target objs make every lookup miss, and
   the resulting unanimous "kill" looks exactly like a strong result.
