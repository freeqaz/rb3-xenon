# W16-AR — W16-AO's filed-but-not-acted-on rows, and the ruler-invariance re-derivation

**Lane:** W16-AR · **Date:** 2026-09-14 · **Branch:** `w16-ar`, based on main `77db3f09`
**Predecessor:** [`W16AO_LIST_T_BIJECTION_ROWS_2026-09-14.md`](W16AO_LIST_T_BIJECTION_ROWS_2026-09-14.md)
**Scope:** `scripts/target_symbol_map.json` only. No `src/` edit, no splits edit, no alias edit.

---

## 0. Headline

| item | predicted | measured | verdict |
|---|---|---|---|
| 1 — repair AO's three filed rows | 3 rows wrong; MAPDEF-3 shape | 3 wrong **plus a fourth defect AO did not see**; 6 rows changed | **+3 fns / +364 B** |
| 2 — find the displaced spellings | 3 spellings unplaced | all 3 found in the EventTrigger cluster; a 5-row rotation | **+4 fns / +696 B** |
| 3 — ruler invariance | unstated | **the claim is FALSE on objdiff 4.2.9** and was deliberately removed by a tool change | see §3 |
| 4 — seven anonymous addresses | budget-dependent | **NOT DONE** | see §6 |

**Lane total: 43,450 / 4,023,648 B → 43,457 / 4,024,708 B = +7 functions / +1,060 B, 7 rows crossed in, 0 fell out.**

The most valuable output of this lane is §3, not the bytes.

---

## 1. Item 1 — AO's three filed rows

AO adjudicated three rows and did not touch them. The brief required each be re-checked on
bytes rather than inheriting AO's verdict. All three were wrong, and re-checking found a
**fourth** defect AO did not see.

### 1A — `0x822b6ee8`

Spelled `?resize@?$ObjList@UProxyCall@EventTrigger@@@@QAAXI@Z`, forwards to `0x822b6798`
which AO proved is `list<EventCall@EventAnim>::resize`.

Confirmed on retail bytes. Repaired to `?resize@?$ObjList@VEventCall@EventAnim@@@@QAAXI@Z`.

> **Prediction stated before measuring: "+68 B with zero downside", because the old pairing
> sat at 99.41 (<100) so nothing could fall out.**
> **Measured: +68 B crossed AND −108 B fell out in another unit. The prediction was half
> wrong.** I investigated instead of reverting, and that is what uncovered the rest of the
> item — the `ObjList<T>` wrapper family is rotated, so repairing one row displaces a name
> that another row is wrongly holding. The fall-out was not noise; it was the next defect
> announcing itself.

### 1B — `0x823c38e8`

Spelled `?erase@?$list@USink@MsgSinks@@…`.

**AO's stated premise is wrong.** AO wrote that `MsgSinks::Sink` is `{Hmx::Object*, SinkMode}`
= 8 bytes with no destructor. `src/system/obj/Msg.h` contains **two** structs named `Sink`:
`MsgSource::Sink` at line 215 (which is what AO read) and `MsgSinks::Sink` at line 293, which
is `{ObjOwnerPtr<Hmx::Object> obj; SinkMode mode;}` with a user-declared `~Sink(){}`.

I then formed my own wrong belief — I read the header's `// 0x14` comment as implying
`sizeof(MsgSinks::Sink)` = 24 and predicted node `0x20`. The compiler refuted it:
`class_layout_report.py` gives `_List_node<Sink@MsgSinks>` sizeof = 24, so
`sizeof(MsgSinks::Sink)` = **16 — identical to `ConstraintSystem`**. A live instance of
CLAUDE.md's *ask the compiler, not the comments*.

⇒ **AO's node-size channel for this row was vacuous** (both candidates are 16 B); only the
destructor relocation discriminates, and it names `ObjRefConcrete<RndTransformable,ObjectDir>`,
which is `ConstraintSystem`'s member. **AO's conclusion stands and AO's reason does not** —
the *count right, cause wrong* pattern, again.

Repaired to `?erase@?$list@UConstraintSystem@CharBlendBone@@…`.

### 1C — `0x822a8bd0`

Spelled `?resize@?$ObjVector@UEyeDesc@CharEyes@@@@QAAXI@Z` yet forwards to a **list** resize,
which an `ObjVector<T>::resize` cannot do. The wrapper's name is the wrong half: the body's
own relocations name `OldMatOption` (`OutfitConfig.h:197`, `ObjList<OldMatOption> mMatOptions`).
Repaired to `?resize@?$ObjList@VOldMatOption@@@@QAAXI@Z`. AO's row #9 rests on a direct
`??1OldMatOption@@` relocation and was not disturbed.

### 1D — the defect AO did not see

The `ObjList<T>` wrapper rows are **rotated**, so three further rows were holding names that
belong to other addresses. Six rows changed in total (all names lifted verbatim from a compiled
obj's COFF symbol table — never hand-mangled, per AO §3):

| address | was | now |
|---|---|---|
| `0x822b6ee8` | `ObjList<ProxyCall@EventTrigger>::resize` | `ObjList<EventCall@EventAnim>::resize` |
| `0x823c38e8` | `erase<list<Sink@MsgSinks>>` | `erase<list<ConstraintSystem@CharBlendBone>>` |
| `0x822a8bd0` | `ObjVector<EyeDesc@CharEyes>::resize` | `ObjList<OldMatOption>::resize` |
| `0x824c9ab8` | `ObjList<ProxyCall@EventTrigger>::operator=` | `ObjList<EventCall@EventAnim>::operator=` |
| `0x824c99f8` | `list<ProxyCall@EventTrigger>::operator=` | `list<EventCall@EventAnim>::operator=` |
| `0x824cab50` | `ObjList<EventCall@EventAnim>::operator=` | `ObjList<KeyFrame@EventAnim>::operator=` |

**Measured (set-diff, full build, ruler `name_check`): 43,450 / 4,023,648 B → 43,453 /
4,024,012 B = +3 fns / +364 B.** Commit `4315f364`.

### 1E — why item 1 paid less than it should have: a coupled map+splits defect

Two of the repairs are **not fully collectable by a map edit alone**, and this is the reason
item 1 reads +364 B rather than ~+836 B:

- `0x823c38e8` is pinned to `Msg.cpp`, but the true name is defined in `CharBlendBone.obj`.
- `0x822a8bd0` is pinned to `HamCamTransform.cpp`, but the true name is defined in `OutfitConfig.obj`.

objdiff pairs target↔base **by name**, so a row pinned to a unit whose base obj cannot define
its name reads permanently 0% however correct the source is. And the `Msg.cpp` pin is
**circular** — it exists *because of* the old wrong map name, so repairing the name leaves the
pin wrong.

`config/45410914/splits.txt` belongs to W16-AQ, so this is filed, not fixed:
[`W16AR_ALIAS_PROPOSALS_FOR_W16AQ.json`](W16AR_ALIAS_PROPOSALS_FOR_W16AQ.json), **472 B**
(144 B measured + 328 B ceiling). Both are **re-homes, not reattributions**, so per
CLAUDE.md/PINHOME-1 each needs its own A/B — neither is metric-neutral.

---

## 2. Item 2 — the displaced spellings

Every repair in item 1 leaves a true spelling unplaced. The brief required finding them by
caller channel and node immediate, and — emphatically — **naming only what is proven**, since
an unnamed address is already forgiven by `name_check` while a wrong name is a new charge.

### 2A — all three displaced EventTrigger spellings, found

They are not orphans. All three landed in the EventTrigger cluster, which is the strongest
available confirmation that item 1's repairs were right.

The EventTrigger `operator=`/`resize` family is rotated by exactly **one function** — the same
defect shape as item 1:

| address | map before | true identity |
|---|---|---|
| `0x824a2008` | *(absent)* | `?resize@?$ObjList@UProxyCall@EventTrigger@@@@QAAXI@Z` |
| `0x824a4518` | `null` | `??4?$list@UAnim@EventTrigger@@…` |
| `0x824a45c8` | `??4?$list@UAnim@EventTrigger@@…` | `??4?$list@UProxyCall@EventTrigger@@…` |
| `0x824a4790` | `null` | `??4?$ObjList@UAnim@EventTrigger@@@@QAAXABV0@@Z` |
| `0x824a4800` | `??4?$ObjList@UAnim@EventTrigger@@@@` | `??4?$ObjList@UProxyCall@EventTrigger@@@@QAAXABV0@@Z` |

**Two independent channels, both on retail bytes:**

*Relocation channel* — `0x824a2008` calls `??1ProxyCall@EventTrigger@@` (`0x8249bac8`) and
`resize<list<ProxyCall@EventTrigger>>` (`0x824a1b40`), **both named in the map by earlier lanes,
not by this one**. `0x824a45c8` calls `??4ProxyCall@EventTrigger@@` and
`erase<list<ProxyCall>>`. `0x824a4518` calls `??4Anim@EventTrigger@@` and `erase<list<Anim>>`.

> ⚠ **An instrument error I made and caught.** My first read used a fixed `0xa0` byte window
> instead of the `.pdata` extent, ran past the end of `0x824a45c8` (real size 176 B), and mixed
> in the *neighbour's* relocations — which made the function look self-contradictory (ProxyCall
> and HideDelay in the same body). **Read relocations within `.pdata` extents.** A fixed window
> does not fail loudly; it produces a confident contradiction.

*Caller channel* — `?Copy@EventTrigger@@` calls `0x824a4790`, `0x824a4800`, `0x824a4870` in
that order, and `EventTrigger.h` declares `mAnims` (0x10), `mProxyCalls` (0x30), `mHideDelays`
(0x78) in that order. `Copy` assigns members in declaration order, so the middle call is
`mProxyCalls` — it cannot be a second `Anim`.

*Pairability control* — the known hazard is that **proving a name wrong does not make renaming
safe**. Our compiled `src/system/rndobj/EventTrigger.obj` defines **all five** names, and all
five addresses are pinned to `EventTrigger.cpp`. That is exactly what item 1's two coupled rows
lacked, and it is why this item paid cleanly and that one did not.

**Measured: 43,453 / 4,024,012 B → 43,457 / 4,024,708 B = +4 fns / +696 B. CROSSED IN 4 rows /
696 B, FELL OUT 0 rows / 0 B.** Commit `74547f4b`.

```
+    328 B  default/EventTrigger::?Copy@EventTrigger@@UAAXPBVObject@Hmx@@W4CopyType@23@@Z
+    176 B  default/EventTrigger::??4?$list@UAnim@EventTrigger@@...
+    108 B  default/EventTrigger::??4?$ObjList@UProxyCall@EventTrigger@@@@QAAXABV0@@Z
+     84 B  default/EventTrigger::?resize@?$ObjList@UProxyCall@EventTrigger@@@@QAAXI@Z
```

**`?Copy@EventTrigger@@` crossing in is the confirmation the brief predicted.** It is also the
answer to the alias-suspect screen: this is a map-only patch, which CLAUDE.md flags as the
shape a fabricated alias would produce, but a fabricated alias **cannot make a caller's
instruction stream agree with retail**. The caller crossed because its `bl` relocation names
now match retail's, which is only possible if the names are right.

`0x824a4518` and `0x824a4790` were explicit `null` rows, i.e. **known**-anonymous. Naming them
converts two forgiven call sites into checked ones — a bet, per CLAUDE.md — and the bet paid,
measured, with zero fall-out.

### 2B — `list<Sink@MsgSinks>::erase`: a bounded NEGATIVE

**There is no `list<Sink@MsgSinks>::erase` in retail.** Recorded so nobody re-hunts it.

The brief guessed the erase anchor would be degenerate here because `T` is trivially
destructible. **That premise is wrong**, and so was my own first search. `MsgSinks::Sink` holds
an `ObjOwnerPtr<Hmx::Object>`, so `~Sink` is **not** trivial — meaning an erase would carry a
**direct** `bl` to `??1Sink@MsgSinks@@`, which is already in the map at `0x82767fa8`.

My first pass scanned the 112 retail node-free erase sites for an `ObjOwnerPtr` destructor and
came up empty. **That was the wrong channel and the negative was worthless** — it is exactly the
"decisive-looking false negative" failure class. Re-run on the right channel:

- No erase free-site in the image carries a `bl 0x82767fa8`.
- `0x82767fa8` has **exactly one** caller in the whole image, `0x8276821c`, and that is an **EH
  unwind funclet** (`addi r31,r12,-0xa0` / `bl 0x82767fa8` / `blr`) — a cleanup path, not an erase.

The channel discriminates (it is the same direct-`??1T` channel that carried AO's row #9), so
this is a real bounded negative rather than a vacuous one. The spelling stays unplaced and
**nothing is named**.

### 2C — `ObjVector<EyeDesc@CharEyes>::resize`

The spelling displaced by 1C is **not placed**. Item 1C settled which half of that row was
wrong (the wrapper's name), which does not by itself locate a genuine
`ObjVector<EyeDesc@CharEyes>::resize`, and I did not establish that one exists. **Nothing named.**

### 2D — proven but deliberately NOT landed

`??$_M_splice_insert_dispatch@…@EventTrigger` is rotated the same way: `0x824a2190` (unnamed)
is the `Anim` instantiation, `0x824a2230` (spelled `Anim`) is called by the ProxyCall list
`operator=` so it is the ProxyCall one, and `0x824c9878` (spelled `ProxyCall`) sits in the same
`0x824c9xxx` displaced-name cluster as the two rows item 1 repaired. Closing it is a three-row
permutation needing its own injectivity pass. **I would rather hand over a proven, unlanded
finding than land an unmeasured one.** Filed as AR-3 in the JSON.

---

## 3. Item 3 — the ruler-invariance claim is FALSE, and was removed on purpose

### 3.1 The claim under test

CLAUDE.md (lane RULER-SWEEP, 2026-08-13) and the memory index both state that
`matched_functions` is ruler-invariant — *"bit-identical"* 44,252 across `none`/`name_check` —
on the reasoning that `mpn` excludes arg-only penalties and `none`→`name_check` changes only
relocation-name arg comparison. W16-AO measured **43,436 vs 45,105** and flagged a 1,669-row
gap without asserting a cause.

### 3.2 What I measured

Two `report.json`s from **one built worktree**, differing in **one config key**, with
`report.json` **and** `report.cache` wiped between legs (both legs verified cache-cold:
**0 hits / 3,115 misses**).

| measure | `-c functionRelocDiffs=name_check` | `-c functionRelocDiffs=none` | Δ |
|---|---:|---:|---:|
| `matched_functions` | **43,453** | **45,104** | **+1,651** |
| `matched_code` | 4,024,012 | 4,511,496 | +487,484 |
| `masked_equal_functions` | 23,047 | 23,047 | 0 |

**Controls, each of which could have failed:**
- The `name_check` leg reproduces the **shipped canonical `report.json` on every key** — so the
  leg is not an artefact of driving the CLI by hand.
- `provenance.diff_config` differs in **exactly one** of its 22 keys.
- `tool_commit a5f0ea903ec1` and `tool_binary_hash 5a51cd51fe0a353f` identical on both legs, so
  this is not a tool swap (the failure mode that was mis-attributed once already on 2026-08-13).
- Row-level: rows at `mpn == 100` number 43,453 vs 45,104, with **B−A = 1,651 and A−B = 0**.
  The movement is strictly one-directional, which is what a forgiveness carve-out looks like and
  is *not* what cache noise looks like.

⇒ **`matched_functions` is NOT ruler-invariant.** Of the brief's three candidate readings,
**(b) is correct**: the claim was true on 2026-08-13's objdiff and is false on 4.2.9. It is not
(a) — my legs are provably one key apart on one binary — and not (c) — the caches were cold and
the `name_check` leg reproduces the shipped report exactly.

### 3.3 The mechanism

`objdiff-core/src/diff/code.rs`:

```rust
// line 288
let normalized_diff_score = diff_score.saturating_sub(diff_state.arg_diff_score).min(max_score);

// lines ~1781-1793
let vetted_reloc_name_diff = diff_config.function_reloc_diffs == FunctionRelocDiffs::NameCheck
    && matches!(a, InstructionArg::Reloc) && matches!(b, InstructionArg::Reloc)
    && !is_regalloc_save_helper(left_resolved.relocation)
    && !is_regalloc_save_helper(right_resolved.relocation)
    && !is_placeholder_symbol_name(&left_resolved.symbol.name)
    && !is_placeholder_symbol_name(&right_resolved.symbol.name)
    && !local_static_ordinal_only_diff(left_resolved.relocation, right_resolved.relocation);
if !is_immediate && !vetted_reloc_name_diff { state.arg_diff_score += penalty; }
```

`mpn = diff_score − arg_diff_score`. RULER-SWEEP's reasoning was right **for its binary**: under
`none` a relocation-name difference is not charged at all, and under the `name_check` of
2026-08-13 it was charged into `arg_diff_score`, which `mpn` subtracts back out — so `mpn`, and
therefore `matched_functions`, came out the same on both rulers.

`vetted_reloc_name_diff` breaks that. Under `name_check` **only**, a relocation-name difference
that survives three vetting screens (not a regalloc save helper, not a placeholder name, not a
local-static ordinal) is **excluded from `arg_diff_score`** and left in `diff_score` instead. It
therefore **no longer cancels**, and it drags `mpn` below 100. Under `none` the same site is
never charged anywhere, so `mpn` stays 100. The two rulers now disagree on `mpn` by exactly the
population of vetted wrong-callee sites: **1,651 rows.**

```
$ git -C ../objdiff log -S'vetted_reloc_name_diff' -- objdiff-core/src/diff/code.rs
b14ba45  2026-08-20  NameCheck: let a vetted wrong-callee reach match_percent_normalized
```

**The carve-out landed 2026-08-20 — seven days AFTER RULER-SWEEP measured on 2026-08-13.** The
invariance was not silently broken and was never a coincidence: it was **deliberately removed by
a tool change**, and the commit subject says so. RULER-SWEEP's measurement was correct on the
day it was taken; the house rule simply outlived its binary.

### 3.4 A per-row example

`?AddInfo@PhraseAnalyzer@@QAAXHW4TrackType@@HH_N@Z`, 100 B, unit
`default/system/beatmatch/PhraseAnalyzer`. **Zero instruction-byte differences.** One `diff_arg`
at instruction index 21: target `bl ?push_back@?$vector@VSongSection@@…` vs base
`bl ?push_back@?$vector@URawPhrase@@…`.

| ruler | `mpn` | `fuzzy` |
|---|---:|---:|
| `name_check` | 99.8 | 99.8 |
| `none` | **100.0** | **100.0** |

One vetted wrong-callee site, one row, +1 `matched_functions` under `none`. Multiply by 1,651.

### 3.5 Proposed correction to CLAUDE.md — **PROPOSED ONLY, NOT APPLIED**

I did not edit `CLAUDE.md`. Replace the sentence in the RULER-SWEEP bullet that reads
*"while `matched_functions` (44,252) and `masked_equal` (22,886) are **bit-identical**, because
`mpn` excludes arg-only penalties and `none`→`name_check` changes *only* relocation-name arg
comparison"* with:

> ⛔⛔ **`matched_functions` IS NO LONGER RULER-INVARIANT, AND THE INVARIANCE WAS REMOVED ON
> PURPOSE — DO NOT USE A `none` LEG AS A CONTROL FOR A FUNCTION COUNT.** RULER-SWEEP's
> bit-identical 44,252 was correct **on 2026-08-13's binary** and is false on objdiff 4.2.9.
> `objdiff-core` commit **`b14ba45` (2026-08-20), "NameCheck: let a vetted wrong-callee reach
> match_percent_normalized"**, added `vetted_reloc_name_diff` to `diff/code.rs`: under
> `name_check` *only*, a relocation-name difference that passes three screens (not a regalloc
> save helper, not a placeholder name, not a local-static-ordinal-only diff) is **excluded from
> `arg_diff_score`** and left in `diff_score`. Since `mpn = diff_score − arg_diff_score`, such a
> site no longer cancels and pushes `mpn` below 100, whereas under `none` it is never charged at
> all. Re-measured 2026-09-14 (lane W16-AR) on one built tree, one config key apart, both caches
> cold, the `name_check` leg reproducing the shipped `report.json` on every key:
> **`matched_functions` 43,453 (`name_check`) vs 45,104 (`none`), Δ+1,651**, with 1,651 rows
> moving `mpn <100 → 100` and **zero** moving the other way; `masked_equal_functions` 23,047 on
> both. Example: `?AddInfo@PhraseAnalyzer@@QAAXHW4TrackType@@HH_N@Z` (100 B) has **zero**
> instruction-byte differences and one `diff_arg` — target `push_back<vector<SongSection>>` vs
> base `push_back<vector<RawPhrase>>` — and reads `mpn` 99.8 under `name_check`, 100.0 under
> `none`. ⇒ **`matched_functions` and `matched_code` are now BOTH ruler-dependent**, so a
> function-count absolute is incomparable across rulers exactly like a byte absolute, and the
> `none` control for a map change is measuring the ruler, not the change. ⚠ The adjacent rule
> that `matched_code` differs by ~817 kB / 7.9 pp across rulers is **unaffected** and still
> holds.

**Why this matters more than §1–§2's bytes:** the `none` leg is the standing control for every
map lane, on the stated ground that it holds `matched_functions` fixed. It does not. A map lane
that reads "+N functions under `none`, flat under `name_check`" and concludes anything about its
own change is reading the carve-out. And the 1,651-row population is precisely the
**vetted-wrong-callee** stratum — the class this lane has spent its whole budget repairing — so
the control is at its least trustworthy exactly where map lanes work.

---

## 4. Commits

| sha | contents |
|---|---|
| `4315f364` | item 1 — 6 map rows, +3 fns / +364 B |
| `74547f4b` | item 2 — 5 map rows, +4 fns / +696 B |
| *(this doc)* | the record, the JSON proposals for W16-AQ |

Every replacement name was lifted **verbatim** from a compiled obj's COFF symbol table; none was
hand-mangled. The map was round-tripped with `json.dumps(d, indent=1, ensure_ascii=False)+'\n'`
and proven byte-identical on a no-op pass (2,412,774 B in = out), `_bijection_arbitrary`
preserved. After both commits the only duplicate names in the map are the **two pre-existing**
ones (`?NodeCmp@@YAHPBX0@Z`, `__destroy_aux<LevelData>`) — this lane introduced none.

---

## 5. Gates

Run in the mandated order, in the worktree, native gate last. See §7 for the verbatim line.

---

## 6. NOT done, and why

- **Item 4 — the seven anonymous addresses** (`0x82400b98`, `0x824ce0c8`, `0x824ce060`,
  `0x822a6670`, `0x82326fe8`, `0x8249b660`, `0x8249b5b8`). Budget went to item 3, which the
  brief itself priced above items 1–2. Item 4 is explicitly *"only if budget remains"*. I saw
  `0x82400b98` in passing — it is called from `?resize@?$ObjList@UAnim@EventTrigger@@@@`'s
  cleanup path, consistent with AO's `~ObjOwnerPtr<RndAnimatable>` reading — but *consistent
  with* is not *proven*, and naming an anonymous address is a bet that can un-pair callers, so
  **nothing was named**.
- **The `none` control leg for items 1 and 2.** Not run. Two reasons, and the second is the real
  one: (i) budget; (ii) **§3 shows the control is not the instrument it is advertised as.** Both
  patches are map-only and therefore purely relocation-name, so a `none` leg is flat *by
  construction* and carries no information about whether the names are right. The evidence that
  does discriminate is the caller crossing in (§2A), and I have it.
- **The splits re-homes for `0x823c38e8` and `0x822a8bd0`** (472 B). `config/45410914/splits.txt`
  is W16-AQ's file. Filed as AR-1/AR-2 in the JSON with the emptied-unit hazard called out.
- **The `_M_splice_insert_dispatch` rotation** (§2D). Proven, not landed — needs a three-row
  injectivity pass I did not have budget to measure.
- **`ObjVector<EyeDesc@CharEyes>::resize`** (§2C) — not located, nothing named.
- **No alias proposed.** The empty set is recorded deliberately in the JSON. Every
  identification here was settled on retail bytes and landed as a map name; an alias would have
  forgiven the charge instead of fixing it, and an unproven alias lifts the score by
  construction.
- **No `src/` edit, no `symbol_aliases.json` / `icf_alias_*` / `splits.txt` edit, no GamePanel or
  `src/band3/ui/` file touched.** AO's eleven landed addresses and AQ's three were not touched.
- **MCP `run_objdiff` / `run_diff_inspect` were deliberately not used** for any verdict: they do
  a one-`.obj` incremental build that skips the six obj patchers and manufactures phantom
  regressions. Every number here is from a full `./tools/ninja-locked` and `report.json`.

---

## 7. Method notes worth keeping

1. **Read relocations within `.pdata` extents, never a fixed byte window.** A fixed window runs
   into the neighbour and yields a confident self-contradiction rather than an error (§2A).
2. **When a channel comes up empty, check you were on the right channel before believing it.**
   The `Sink@MsgSinks` negative was worthless the first time because the struct's destructor is
   non-trivial and the erase would use a *direct* `??1T` relocation, not the `ObjOwnerPtr` one I
   scanned for (§2B).
3. **Two same-named structs in one header is a live trap.** `Msg.h` has `MsgSource::Sink` and
   `MsgSinks::Sink`; AO read the wrong one and still reached the right conclusion — *count right,
   cause wrong* (§1B).
4. **Ask the compiler, not the comments.** The `// 0x14` comment led me to predict a 24-byte
   `Sink` and node `0x20`; `class_layout_report.py` says 16 (§1B).
5. **A wrong map name is financed by its callers, and repairing it is confirmed by a caller
   crossing in.** That is also the one signal a fabricated alias cannot counterfeit (§2A).
6. **Proving a name wrong does not make renaming safe.** Check the paired base obj actually
   defines the replacement name before predicting a sign — that single check is the whole
   difference between item 1 (+364 B, two rows stranded) and item 2 (+696 B, clean) (§1E, §2A).
