# W40-ACCURACY — W33's five queued findings landed: **+2 fns / +180 B, and one of them was a real logic bug the score could never see**

**2026-08-17.** Lane W40. Base `1172faa3` on `grounded2-restoration`. Baseline
re-derived in-worktree before any edit, never inherited: `matched_functions`
**44,511** · `matched_code` **3,761,152 B** · **36.442930%** · honest **21,601** ·
`total_code` **10,320,664** · `total_functions` **69,226**. Ruler **`name_check`**,
read from `report.json`'s own `provenance.diff_config` (22 keys,
`ppc.calculatePoolRelocations=false`), not assumed.

Lane W33-FOLDPROOF adjudicated the NAME-BLOCKED stratum, proved it worth **zero
bytes at any source quality**, and queued five correctness findings it
deliberately did not land — each needing its own settled A/B, none of them worth
bytes on W33's own §4 analysis. This lane landed all five under the standing
directive that **accuracy beats headline %**.

**Four of the five are cosmetic. One is a genuine behavioural bug. And the net is
positive anyway** — which was not the reason for doing any of them.

| # | finding | Δmatched | Δbytes | class |
|---|---|---:|---:|---|
| **B** | `String::substr` one-arg → two-arg | +0 | **+0** | cosmetic (call spelling) |
| **D** | `PastFinalNote`/`AtFirstPhrase` transposed | **+1** | **+36** | ⚠ **REAL LOGIC BUG** |
| **F** | `FindObject` → `Find<T>` ×5 | +0 | **+0** | cosmetic (call spelling) |
| **A** | drop `inline` on `PrintTick` | **+1** | **+40** | cosmetic (inline policy) |
| **E** | `SetClipType` out-of-line | +0 | **+104** | cosmetic (inline policy) |
| | **NET** | **+2** | **+180 B** | +0.001740pp, honest +1 |

**0 units fell off either ruler in any of the six runs.** That was the acceptance
test, not the byte count — W27's rule, applied here because `Debug.h`-class macro
flips are known to cause funclet-pairing *losses* elsewhere.

---

## 0. The anti-vacuity gate, run first

A fresh worktree's reflinked target objs are **pre-renamer**, so every retail
mangled name reads ABSENT until you build — and the resulting unanimous
"refuted" agrees with your prior. Built first, then asserted against main:

| tree | target objs | content sha256/16 | mangled-name occurrences |
|---|---:|---|---:|
| main | 3,091 | `45a98666bbcd262d` | 80,060 |
| this worktree | **3,091** | **`45a98666bbcd262d`** | **80,060** |

Byte-identical. Every negative below is a real negative.

⚠ **A second vacuity was caught mid-lane and is worth recording**: dtk's split
`.s` writes addresses in **UPPERCASE** hex (`.fn fn_827BE7E0`). A lowercase
`grep fn_827be7e0` returns nothing and looks exactly like "this function is not
in the split" — a false negative shaped like a finding. Key on the `.fn` symbol
*and* match case.

---

## 1. B — `substr`: the dc3-newer contamination, confirmed and landed at Δ0

**Re-derived, not inherited.** Relocations in the split target obj, attributed to
their containing function:

| | retail `RecursePatternInternal` | ours before | ours after |
|---|---|---|---|
| `?substr@String@@QBA?AV1@II@Z` (two-arg) | **3** | 2 | **3** |
| `?substr@String@@QBA?AV1@I@Z` (one-arg) | **0** | 1 | **0** |

The two overloads adjudicated on retail bytes, on **two independent extractors
that agree byte-for-byte** (`tools/va_disasm.py` and dtk's split `Str.s`):
`0x827be7e0` does `add r9,r5,r6` / `mr r30,r6` / `mr r5,r6` — it **uses r6** (the
count); `0x827be7a8` never touches `r6`.

Provenance is the documented trap: **dc3 is NEWER than RB3**, uses the one-arg
form at `File.cpp:688`, and we copied it; the RB3-era **rb3-Wii** oracle uses the
two-arg form at `File.cpp:599`, and **retail agrees with rb3-Wii**. Same class as
W27's `MidiReader` `MILO_WARN`/`MILO_NOTIFY`.

⚠ **Correction to W33's figure, and to my own first pass.** W33 recorded "ours
2/1". My first measurement said 3/1 — because I counted the **aggregate** over
the TU, which silently folds in an unrelated `substr` call in
`OnFrameRateRecurseCB`. Attributed per function, W33's 2/1 is exactly right.
**The aggregate is the wrong instrument here; only per-function attribution
answers the question.**

★ **NEGATIVE RESULT, recorded rather than hidden: the row moves DOWN.**
`?RecursePatternInternal@@` goes **fuzzy 85.9417 → 85.8655**. Setting up `r6`
costs an instruction that shifts following code inside a function that is only
86% matched, so a *correct* instruction locally raises the diff count. The
relocation profile — not the score — is what settles this: after the fix our call
profile **is** retail's. This is the campaign's own rule biting in miniature
(*getting a size right collapsed the score ⇒ suspect the instrument before
reverting*).

**Behaviourally identical**: `pttnLen` is `length()-1`, so the count
`(pttnLen+1)-forwardPos` is exactly `length()-forwardPos`, i.e. "to the end" —
which is what the one-arg form already computed.

---

## 2. D — ★★★ the one real bug, and **W33's proposed fix would have REGRESSED**

W33: *"MAP_ERROR — the two names are TRANSPOSED; **our source is right**"*, with a
queued **map-only** swap. The map half is correct and re-derived three ways. The
"our source is right" half is **wrong**, and this lane measured the cost of
believing it.

### The map is transposed — three independent instruments

1. **Vtable membership (decisive alone).** `0x826e5688` has **ZERO** aligned word
   references anywhere in `band.exe`; `0x826e5ae8` has exactly one, at
   `0x820f1a34` in `.rdata`. The map spelled `0x826e5688`
   `?PastFinalNote@VocalPlayer@@UBA_NXZ` — **`U` = virtual**, and a virtual
   function's address *must* appear in some vtable.
2. **Slot alignment.** `0x820f1a34` is VocalPlayer's vtable; `GetStarRating` sits
   at `0x820f1a24`, so the occupant is at **Δ+0x10** — exactly where our header
   declares `virtual bool PastFinalNote() const` after `GetStarRating`.
3. **Body semantics.** The two bodies are identical but for **one** instruction:

```
0x826e5688   lwz r10, 0(r10)    -> mPhrases._M_start  == begin()  -> AtFirstPhrase
0x826e5ae8   lwz r10, 4(r10)    -> mPhrases._M_finish == end()    -> PastFinalNote
```

### But our source was transposed too — a REAL behavioural bug

`AtFirstPhrase()` compared `mThisPhrase` against **`.end()`**. An in-source
comment had read retail's `_M_finish` load off `0x826e5ae8` and concluded "this
is end()" — i.e. **the wrong map name had been reasoned backwards INTO our
source.**

That is a genuine logic defect, not a codegen one. `VocalTrack.cpp:746` takes the
`else` branch to hunt for the **previous** phrase, which only makes sense when we
are *not* at the first one. With `.end()`, at the actual first phrase the
predicate returned **false**, the search for a predecessor found nothing, and
`mPhraseEndMs = 0; BuildPhrase(cur, next)` never ran.

⇒ Landed as **map swap + `AtFirstPhrase` → `.begin()` + `AtLastPhrase` →
`.end()`** (the last behaviour-preserving: `data()+size()` is the same *value* but
compiles to a subtract/shift instead of retail's single `lwz r10,4(r10)`).

**Rename safety checked BEFORE moving anything**: both names are DEFINED by the
same obj, `src/band3/game/VocalPlayer.obj`, so neither row can be stranded at a
permanent 0%.

### ⛔ The negative control: W33's map-only swap measured **−2 / −72 B**

Measured deliberately, because the correction is the point:

| variant | matched | Δ vs baseline |
|---|---:|---:|
| baseline | 44,511 | — |
| **map-only (W33's queued edit)** | **44,510** | **−1 fn / −36 B** |
| map + source (landed) | **44,512** | **+1 fn / +36 B** |

A map-only swap re-pairs our `.end()` body against the begin-reader and drops
`AtFirstPhrase` from 100%, while `PastFinalNote` still cannot match. **Landing
the queued edit as written would have been worse than doing nothing, and would
have left the logic bug in place.** This is the whole case for *re-derive, never
inherit*.

### ★★★ Why the score could never have found this

`AtFirstPhrase` read **100% before** (our `.end()` body vs the end-reader under
the wrong name) and **100% after** (our `.begin()` body vs the begin-reader under
the right name). **Both transpositions score identically.** The metric was
structurally blind to a real bug; only retail-byte adjudication could see it —
the standing directive demonstrating itself.

---

## 3. F — five call spellings, Δ0, profile now identical to retail

| function | retail | ours before | ours after |
|---|---|---|---|
| `?Filter@BandCharacter@@` | 3× `Find<Hmx::Object>` | 1× `Find<…>` + 2× `FindObject` | **3× = retail** |
| `?OnInstallFilter@BandCharacter@@` | 2× `Find<Hmx::Object>` + 1× `Find<RndTransformable>`, 3× `__RTDynamicCast` | 3× `FindObject`, 4× `__RTDynamicCast` | **identical to retail** |

W33's independent check (`__RTDynamicCast` 4→3) reproduces exactly: rewriting
`dynamic_cast<RndTransformable*>(FindObject(n,false))` as
`Find<RndTransformable>(n,false)` moves the cast inside the template COMDAT.

Not a fold: `0x82270438` (`??$Find@VObject@Hmx@@@ObjectDir@@`) stashes `fail` in
`r30`, forces `li r5,0`, then `bl 0x82750188` = `?FindObject@ObjectDir@@` — a
template wrapper calling the implementation. **Behaviourally identical**; for
`T = Hmx::Object` the cast is an identity conversion resolved statically.

Rows rose but could not cross (904 B @ 85.10 → 86.78; 1220 B @ 71.54 → 71.56),
exactly as W33 §4 predicts. **+0 B.**

---

## 4. A — `/Ob2` does **not** re-inline, and there is no header blast radius

W33 flagged two risks. **Both are wrong**, which is precisely why it was measured:

* **Header blast radius: none.** `VocalNote.h:124` only *declares* `PrintTick`;
  the definition is in `VocalNoteList.cpp` and no other TU calls it.
* **`/Ob2` re-inlining: refuted by measurement.** Dropping the keyword converted
  all 8 call sites to real calls. (Had it been inert, the sure lever was
  `__declspec(noinline)`, the tree's house pattern — not needed.)

| | retail `.text` | ours before | ours after |
|---|---|---|---|
| `PrintTick` call sites | **8** (NotesDone 4, fn_82780CF0 2, fn_82781B00 1, fn_82781D58 1) | **0** (all inlined; the 8 sites called `TickFormat` directly) | **8** (NotesDone 4, EndPlayerPhrase 2, StartPlayerPhrase 1, AddNote 1) |

The correspondence is now legible: retail's `fn_82780CF0` is our
`EndPlayerPhrase`, `fn_82781B00`/`fn_82781D58` are `StartPlayerPhrase`/`AddNote`.

⚠ **Instrument note:** the raw aggregate says ours has **9** references vs
retail's 8. The 9th is a **`.pdata` unwind relocation, not a call**. Only the
section-attributed count is the right instrument.

Result **+1 fn / +40 B** (`NotesDone` 83.6754 → **91.3660**, +7.69pp on 1,836 B,
still short of crossing; `fn_8278291C` 99.9 → **100.0**). ⚠ `Δhonest` is **+0**,
because the crossing row is `masked_equal` — reported, not banked as honest
progress. My pre-registration said Δ0, so the prediction was **conservative**;
the load-bearing half (rows move ⇒ not inert) held.

---

## 5. E — the queued edit whose real risk was 17× its upside

W33 rated E "same as A". **It is not**, and pricing it first is the whole lesson.

| function | retail | ours before |
|---|---|---|
| `?OnEnterVignette@BandWardrobe@@` | **calls SetClipType** | **inlined it away** |
| `?SetClipTypes@BandCharacter@@` | calls SetClipType directly | called it via a `__declspec(noinline) _outline_SetClipType<>` wrapper |

⛔ **The risk, priced BEFORE editing:** `?SyncProperty@CharDriver@@` is **1,512 B
at fuzzy 100.0000** and `?Copy@CharDriver@@` is **220 B at 100.0000**, and both
inline `SetClipType` today. If `/Ob2` stopped inlining inside the TU they would
fall off for **−1,732 B**, against an upside of **+104 B**. Decision rule set in
advance: *if either falls off, reject and revert.*

**Mechanism that makes it safe:** there is **no LTCG**, so a body defined in
`CharDriver.cpp` cannot be inlined into `BandWardrobe.cpp` or
`BandCharacter.cpp` — reproducing retail's two out-of-line calls — while placing
it immediately after `SyncInternalBones` and well before `SyncProperty`/`Copy`
keeps it inlinable *within* the TU, which retail also does.

**Measured: both at-risk rows held at exactly 100.0000.** `SetClipTypes`
99.8077 → **100.0000** (**+104 B**), `OnEnterVignette` 89.0118 → 89.4043.

★ Note the shape: **Δbytes +104 with Δmatched +0** — the documented
two-different-rulers case, not an anomaly. `SetClipTypes` was already `mpn` 100
(a matched *function*) while `fuzzy` withheld its bytes at 99.8077.

⚠ **Instrument note:** the post-fix scan appears to show ~12 objs calling
`SetClipType`. That is **not** a regression — `src/system/obj/Dir.cpp:1699` and
friends `#include "bandobj/BandWardrobe.cpp"` (sw2 scatter-includes), so one
source function compiles into many TUs. The distinct **caller** set is exactly
retail's two. Verified before trusting the scan, because the raw count looked
alarming.

---

## 6. ⛔ `??__FsLoadedFile` — NOT renamed, and the refusal is re-derived

`0x82682668` is mapped `??__FsLoadedFile@@YAXXZ` (an atexit destructor thunk for
a static). Its retail body loads two `.rdata` pointers into `r5`/`r6`, sets
`r4 = 0` and `r7 = 0`, and **tail-branches to `0x8282a0c8` = `__RTDynamicCast`** —
bit-for-bit the `__RTDynamicCast(p, off, srcType, targetType, isRef)` signature.
**An atexit destructor cannot have that body.** The map name is wrong.

**But renaming it is still unsafe, and this was checked rather than assumed:**
the address is pinned to unit **`CharLipSync.cpp`**, while the plausible correct
name `?GetBandUser@BandUserMgr@@QBAPAVBandUser@@ABVUserGuid@@_N@Z` is DEFINED
**only** by `BandUserMgr.obj`. objdiff pairs by name, so moving that name into
CharLipSync's target obj leaves a base obj that cannot define it ⇒ the row reads
**permanently 0%**.

⇒ **Decision: do not rename. Reason recorded.** This is the standing rule —
*proving a name wrong does not make renaming it safe* — and it is the exact
mirror of D, where the same check **passed** (both names defined by the same obj)
and the rename was therefore correct to make. **The check is what separates the
two, not the strength of the evidence that the name is wrong.**

⚠ Also noted: W33 described the candidate as `GetBandUser(User*)`, but the map's
only `GetBandUser` spelling takes `const UserGuid&` and a `bool`. The
identification is therefore *weaker* than W33 stated; the refusal stands
regardless, and is independent of it.

---

## 7. What this lane deliberately did NOT do

* **Did not touch W33's finding C** (`PreloadPanel::Load`, a missing `String`
  construction + `push_back`). W33 explicitly withheld the container element type
  — retail also references `vector<Symbol>::erase`, so its `vector<String>`
  `push_back` is a different container — and guessing it is exactly the error
  that record warns against. **It remains the one genuinely undecided candidate.**
* **Did not rename `??__FsLoadedFile`** (§6).
* **Did not generalise any of these.** W27's `MILO_WARN` asymmetry measured **−20
  strict** when globalised; B and F are the same *class* of call-spelling fix, and
  a tree-wide sweep of either would need its own whole-binary control. None was
  run, so none is claimed.
* **Did not re-open the NAME-BLOCKED stratum.** W33 §4 stands: 0 of 42 rows can
  cross, and all five findings here collect their bytes (where any) from *other*
  rows entirely.
* **Ran no permuter** (OFF by standing directive).

## 8. For the next lane

* ★ **Price a queued edit before landing it.** E's downside (−1,732 B) was **17×**
  its upside and was invisible in W33's one-line risk note; B's row moves the
  *wrong way*; A's two stated risks were both false. All four facts came from
  ten minutes of reading `report.json` and relocations, before any build.
* ★ **Per-function relocation attribution is the instrument.** The aggregate was
  misleading in B (folded in an unrelated caller) and in A (counted a `.pdata`
  unwind entry as a call). Both would have produced a confident wrong number.
* ⛔ **A queued finding's *verdict* can be right while its *fix* is wrong.** D's
  map diagnosis was correct and its prescribed edit was a measured **−1**.
* ★ **The zero-regression half of a prediction is the half that can fail.** It was
  pre-registered and checked on both rulers in all six runs; it never failed here,
  but E is the case where it very nearly did.
