# Lane BQ-1 · Job A — carving MiniLeaderboardDisplay out of MetaPanel's 44-span mega-unit

Date: 2026-07-30 · branch `laneBQ1` · worktree `~/tmp/wt-bq1` (from `19e27a75`)
Sole owner of `config/45410914/splits.txt` and `scripts/target_symbol_map.json`
for this lane.

Baseline in this worktree (full build, `report.cache` removed, `symbols.txt`
restored, post-split): **matched 40880 / masked_equal 1509 / honest 39371** —
identical to main's headline, so a clean A-leg.

| | predicted | measured |
|---|---|---|
| matched | +5 | **+6** (40886) |
| masked_equal | 0 | **0** (1509) |
| honest | +5 | **+6** (39377) |

---

## 1. What the inherited evidence said, and where it was incomplete

BP-3 measured `0x82319C48` (then mapped `?SetType@BandStoreUIPanel@@`) and
inferred a 328-byte this-adjustor. BP-4 §9 re-read it as a **map mispair** whose
real owner is `MiniLeaderboardDisplay`, identified via the `StaticClassName`
callee at `0x82319950` building the literal `"MiniLeaderboardDisplay"`; BP-7's
literal channel confirmed the CONTRADICT. Both lanes **held** the repoint on the
correct grounds that `MetaPanel.obj` supplies no MiniLeaderboardDisplay COMDAT,
so a bare repoint would read 0%.

Two things had changed or were missing by the time this lane picked it up:

1. **Two thirds of the "triplet" was already gone.** BP-7's Part C phantom drain
   had already deleted `0x82319A50` and `0x82319C48` (both mapped to the phantom
   `BandStoreUIPanel`). So the advertised "3 false-100s drain" was really **one**
   surviving false 100 on `0x82319950` — the other two had been priced by BP-7.
2. **The bandobj-vs-hamobj question is not a real fork.** BP-6 flagged that
   `class MiniLeaderboardDisplay` is defined twice in our tree. But only
   `src/system/hamobj/MiniLeaderboardDisplay.cpp` **exists as a `.cpp`**
   (`src/system/bandobj/MiniLeaderboardDisplay.h` is a header with no
   translation unit), and only the hamobj file is declared in `objects.json`
   (as `NonMatching`, compile-only, **unpinned**). So there is exactly one
   candidate obj to carve into.

## 2. The decisive new evidence: adjustor thunks name the base-class bodies

`AppMiniLeaderboardDisplay` derives from `MiniLeaderboardDisplay`, and its
already-correctly-mapped `$4PPPPPPPM@DE@` vtordisp thunks each branch to the
**base class's** body. That makes the thunk an oracle-free name oracle for its
target, and it named an entire TU's worth of bodies at once:

| App thunk (already mapped, correct) | branches to | ⇒ identity of target |
|---|---|---|
| `0x8264c158 ?SyncProperty@MiniLeaderboardDisplay@@$4…DE@` | `0x82319a80` | `?SyncProperty@MiniLeaderboardDisplay@@` |
| `0x8264c0e8 ?Copy@…$4…DE@` | `0x82319af0` | `?Copy@MiniLeaderboardDisplay@@` |
| `0x8264c118 ?PostLoad@…$4…DE@` | `0x82319b68` | `?PostLoad@MiniLeaderboardDisplay@@` |
| `0x8264c138 ?Save@…$4…DE@` | `0x82319fe8` | `?Save@MiniLeaderboardDisplay@@` |
| `0x8264c108 ?Load@…$4…DE@` | `0x8231a058` | `?Load@MiniLeaderboardDisplay@@` |
| `0x8264c128 ?PreLoad@…$4…DE@` | `0x8231a350` | `?PreLoad@MiniLeaderboardDisplay@@` |

A second, **`$4PPPPPPPM@A@`** thunk set sits in the same region
(`0x82319bb0/bd0/bf0/c00`, `0x8231a138/148/158/488`) branching to those same
bodies — i.e. MiniLeaderboardDisplay's own zero-displacement thunks. That
displacement encoding is exactly the one our hamobj obj emits, and all eight
12-byte bodies are **byte-identical to retail under branch masking** (verified
before the build, which is what made the delta predictable).

### The `??_E<Panel>` names on three of those thunks were impossible

`0x82319bb0/bd0/bf0` were mapped `??_EMetaPanel@@$4…A@`,
`??_ENextSongPanel@@$4…A@`, `??_ESetlistToStorePanel@@$4…A@` and all three read
**100.0%**. A `??_E` scalar-deleting-destructor thunk cannot branch to
`SyncProperty` / `Copy` / `PostLoad`. Their 100% was reloc-masked false credit:
every 12-byte vtordisp thunk in the binary is identical machine code apart from
the branch relocation, and objdiff runs `functionRelocDiffs=None`. Repointed.

## 3. Refutation: `0x8264bce8` was a mispair, and our App header has a source bug

BP-6 landed `0x8264bce8 → ?StaticClassName@MiniLeaderboardDisplay@@` as a
knowingly-unresolved **dupname**, accepted because it was reveal-only. It is
resolvable, and it was wrong. Two callers settle it:

- `0x8264bee0` = `?ClassName@AppMiniLeaderboardDisplay@@` → `bl 0x8264bce8`
- `0x8264c178` = `?SetType@AppMiniLeaderboardDisplay@@` (matching at 100%, with
  the 316-byte `types`/`objects` OBJ_SET_TYPE fingerprint) → `bl 0x8264bce8`

A class's `ClassName()`/`SetType()` call their **own** `StaticClassName()`, so
`0x8264bce8` is App's. It nonetheless builds the literal `"MiniLeaderboardDisplay"`
— which means **retail's `AppMiniLeaderboardDisplay` declares
`OBJ_CLASSNAME(MiniLeaderboardDisplay)`**: the DTA-visible type name carries no
`App` prefix. Our `src/band3/meta_band/AppMiniLeaderboardDisplay.h:38` writes
`OBJ_CLASSNAME(AppMiniLeaderboardDisplay)`.

**Filed, not fixed by this lane:** that header literal is a genuine source
defect. It is *invisible to the metric* (the class-name string reaches the body
only through a relocation, which objdiff masks), so fixing it is a correctness
change with an expected delta of 0 — exactly the at-100% defect class. It also
belongs to a `band3/` game unit, outside this lane's splits/map remit.

This also explains why ICF did not fold `0x82319950` and `0x8264bce8` despite
both building the same string: they are two *different classes'* COMDATs with
different static-Symbol caches (`0x82CBDD04` vs `0x82E019A4`), so their
relocations differ and they are not identical COMDATs.

## 4. The region is a COMDAT scatter pool, not a TU cluster

Worth recording because it changes how the next lane should read this address
space. `0x82319810–0x8231A500` interleaves at least six units' small COMDATs:
MiniLeaderboardDisplay's bodies and thunks, `?Init@MicInputArrow@@`, two
`ObjectDir::Find<T>` instantiations, `RndTransformable` `$4…GM@` thunks (owned by
`PhysicsVolume.cpp`), a `ScoreDisplay` `SetType` thunk, `AppMiniLeaderboardDisplay`
`DrawShowing`, and `MeterDisplay::StaticClassName`. Earlier lanes had already cut
surgical single-thunk pins into the gaps (PhysicsVolume owns
`0x82319BC0–0x82319BD0` and `0x82319C10–0x82319C20`; ScoreDisplay owns
`0x82319C30–0x82319C40`). The carve therefore had to be **seven separate
`.text` blocks**, not one span.

The `$4…GM@`-vs-our-`$4…GI@` displacement difference on the `RndTransformable`
thunks (108 vs 104) is a real 4-byte layout delta between retail's derived class
and ours — those thunks are *not* suppliable by name from our obj and were left
with PhysicsVolume.

## 5. The carve

`MetaPanel.cpp` (44 `.text` blocks → 43; three edited, one deleted):

| before | after |
|---|---|
| `0x82319810–0x82319BC0` | `0x82319810–0x82319950` |
| `0x82319BD0–0x82319C10` | *(deleted — moved wholesale)* |
| `0x82319C20–0x82319C30` | unchanged (foreign `UIComponent $4…3` thunk, 0%) |
| `0x82319C40–0x82319F74` | `0x82319D84–0x82319F74` |

New unit `system/hamobj/MiniLeaderboardDisplay.cpp` (was in `objects.json` as
`NonMatching` but unpinned; four of its seven blocks come from previously
**unpinned** address space, i.e. free upside):

```
.text 0x82319950–0x82319BC0   StaticClassName, ClassName, SyncProperty, Copy, PostLoad, +SyncProperty thunk
.text 0x82319BD0–0x82319C10   Copy / PostLoad / ClassName thunks
.text 0x82319C40–0x82319D84   SetType (316B)
.text 0x82319FE8–0x8231A0D8   Save, Load                      (was unpinned)
.text 0x8231A138–0x8231A170   SetType / Load / Save thunks    (was unpinned)
.text 0x8231A350–0x8231A458   PreLoad                         (was unpinned)
.text 0x8231A488–0x8231A498   PreLoad thunk                   (was unpinned)
```

`.pdata` was not touched — it is derived output, re-derived from `.text` on every
split run.

Map: 5 repoints (`lane-bq1-jobA-minileaderboard-repoints-2026-07-30.json`) and
13 inserts (`…-inserts-2026-07-30.json`). The repoint fragment vacates
`?StaticClassName@MiniLeaderboardDisplay@@` from `0x8264bce8` and claims it on
`0x82319950` **in the same fragment**, so the applier's post-condition
no-duplicate-name assert passes.

## 6. Measured composition, including what I got wrong

New unit ends at **11 functions matching at 100%**: the 9 I predicted
(`ClassName` + 8 `$4…A@` thunks), plus two I did not:

- **`?StaticClassName@MiniLeaderboardDisplay@@` matched.** I predicted it would
  not, from an obj symbol size of 128 B vs retail's 88 B. That size reading was
  **my error**: my dumper reported the COMDAT *section* size (including
  alignment padding and associated data), not the function body extent. Recorded
  because the same mistake would mis-predict any future carve.
- **`fn_823199A8`** (a 32-byte body I deliberately left unnamed for lack of
  evidence) paired at 100% via objdiff's byte fallback.

MetaPanel's four false 100s are gone (the region now shows only 0.00% rows
there), and the neighbouring surgical pins (PhysicsVolume ×2, ScoreDisplay,
AppMiniLeaderboardDisplay, MeterDisplay) all still match — checked explicitly.

Arithmetic: `+11 − 4 = +7` against a measured `+6`. The missing 1 is **not**
attributed to a named regression; re-splitting changes `MetaPanel.obj`'s shape,
which perturbs objdiff's fuzzy byte-fallback pairings inside it. That is the
known **split-churn floor of ~2 functions** (memory:
`project_bandexe_read_traps_2026-07-29`), so a ±1 wobble here is expected noise
rather than a lost match. The banked number is the measured **+6**.
