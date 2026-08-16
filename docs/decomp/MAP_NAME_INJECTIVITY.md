# The target symbol map must be injective on NAME

> **STATUS (2026-08-13):** current. Records an invariant, the gate that enforces
> it, the repair landed with it, and the two addresses that are still
> unadjudicated. Enforcement is live in `ninja`.
>
> **Revised 2026-08-13 by lane J3**, from an adversarial review of the landed
> work. Four claims here were wrong and are corrected in place, each marked
> where it sits: the gate's reach (it did **not** run on `report.json`, now it
> does — `tools/project.py`), the per-row account (**11** rows move at
> `name_check`, not 10, and the mechanism is that the ruler stops checking a
> placeholder target), the count of cleared floor certifications (**three**),
> and the repair SHA. Added: the `gated_map_write.py` prior art this doc
> omitted, and a forward worklist.
>
> **Revised 2026-08-16 by lane R**, which re-derived the forward worklist's two
> nulled-name re-homes from the retail image before applying either.
> `0x8249d5b0` survived on four independent witnesses and is APPLIED (+1
> matched / +204 B on both rulers); **`?Null@Symbol@@QBA_NXZ` at `0x8227c70c`
> is REFUTED** — that address is interior code of
> `??8Symbol@@QBA_NPBD@Z`, and naming it would have minted a byte-exact 100%
> against a non-function. Both write-ups are in the final section; the worklist
> entries are corrected in place.
>
> **Corrected after adversarial review, same day.** The identification held and
> got *stronger* (69 clean controls, not 6; the tail-callee partition is exact).
> Four numbers/claims in lane R's first pass were wrong and are fixed in place:
> the inbound edge is **13** instructions above, not four; the COMDAT is
> **252 B = 8 + 204 + 40**, not 244, and its body equals retail's extent
> exactly; the `name_check` **code%** delta is below its own ~0.05pp noise
> floor; and — the one that matters — **the +204 B measurement carries no
> information about which `T` is correct.**

## The invariant

`scripts/target_symbol_map.json` maps a retail VA to the MSVC-mangled name we
claim lives there. **No mangled name may appear at more than one address.** A
linked image resolves every external / COMDAT symbol to exactly one definition,
so a name at two VAs is a claim the image itself disproves — at most one of them
is real.

The one legitimate exception is **internal linkage**. A `static` free function
is not a COMDAT and is not deduped by the linker, so the same mangled name
honestly sits at one VA per defining TU. Exceptions are enumerated by name in
the map's `_internal_linkage_allow`, and the measured signature an entry must
meet is in `_internal_linkage_allow_comment`. There is exactly one today:
`?NodeCmp@@YAHPBX0@Z`, a file-static qsort comparator in both `DataArray.cpp`
and `BandWardrobe.cpp`. It is why a blanket "same name, two addresses, refuse"
rule would be **wrong**.

## Why a violation is dangerous rather than merely untidy

objdiff pairs symbols **by name** inside a unit, and compares bytes with a ruler
that does not see relocation targets. Adjustor thunks, deleting dtors and
template bodies are byte-identical modulo their relocations, so **a wrong name
on a byte-twin VA reads a clean 100%.**

That is not a metric problem. Byte-exactness is the *admission* gate — for
declaring a crack, for adopting machine-minted source as repo truth, and for a
training label. A duplicate name is therefore a live path to minting a
byte-exact witness against the **wrong target body**, and the witness is
indistinguishable from a real one after the fact.

The near-100 case is the more dangerous one, not the less. A false 100 is
already committed and can be audited; a duplicate sitting at 99.5% is an open
invitation to close it with one edit, on a coin-flip over which of the two
targets the edit is closing.

## The gate

`tools/map_name_injectivity.py`, wired into ninja as
`map_name_injectivity_check` beside `icf_alias_map_checked`, with `always` as an
input so no output mtime can make it look satisfied. `--selftest` exercises the
verdict function in memory; `tools/test_map_name_injectivity.py` does the same
under pytest plus one assertion that the checked-in map is clean.

**What "every build" means, exactly** (corrected by lane J3, 2026-08-13). As
first landed, the stamp `build/45410914/map_name_injectivity_checked.stamp` was
an implicit of the `progress` edge — the default target — and of *nothing else*.
`always` guarantees the edge is dirty; it does not put the edge in a graph that
does not reach it. So `ninja build/45410914/report.json` traversed 2,442 inputs,
none of them this gate, and exited **0 with a live collision in the map** — and
that is precisely the target `scripts/sync_match_percent.py --build` invokes
(the path objdiff's numbers take into `decomp.db`) and the one
`scripts/harvest/extent_sweep/eb1_measure.sh` runs per pass. Gating the summary
and not the measurement is the wrong way round.

Fixed in `tools/project.py` by adding the stamp to `report_implicit`: the report
target now has 2,444 inputs and the gate is two of them. Verify with ninja's own
graph rather than by reading the source —

```
ninja -t inputs build/45410914/report.json | grep map_name_injectivity
```

`build.ninja` is generated from `tools/project.py`; never edit it directly.
Still outside the gate's reach, and deliberately named here so nobody assumes
otherwise: `ninja baseline` / `build/45410914/baseline.json`, and any narrowly
scoped target such as a single `.obj`. `ninja` with no target, and the two
report-producing paths above, all run it.

Three properties, each of which is a specific past failure:

- **SET-based, and it prints the offending set.** Commit `2eb6307a` records a
  map plan that left the collision *count* unchanged (8 → 8) while retiring one
  duplicate and introducing another. A count comparison passes that plan clean.
- **It reads the MAP, not the objects.** An object-level audit only sees a name
  two compiled objs both emit.
  `?DataDir@UIPanel@@$4PPPPPPPM@EM@AAPAVObjectDir@@XZ` sat at two VAs inside one
  unit with only one of them pairing — invisible to any differ.
- **It scores the APPLIED map, through the renamer's own `load_address_map`.**
  Null rows are "deliberately unclaimed" and `_denylist` rows are refused, so
  the gate imports that filter rather than re-deriving it. `_denylist` was
  declared in the map and *ignored by the loader* until `f3fe9ab1`; a gate
  carrying a private copy of the filter would be the second safeguard here that
  silently did nothing.

Consequence, stated so nobody has to discover it: a duplicate confined to denied
or nulled rows does not reach an obj and does not fail the gate. It is printed
as an informational line. Denying is a deliberate escape hatch that
`_denylist_comment` requires a per-address rationale for — not a way to hide a
live collision.

There is deliberately **no** second, softer allow-list of "known open
collisions". An entry there would be a name we have disproved or cannot
adjudicate, left applied and left scoring, which is the hazard the gate exists
to stop, re-admitted under a comment.

## Why the per-unit checks were not enough

`scripts/harvest/icf_class_bijection.py` and
`scripts/harvest/tu5_map_apply_fragment.py` both enforce injectivity — **within
one unit / one fragment.** Cross-unit duplicates pass both. That gap is how the
debt returned twice (738 surplus VAs before `e7b8ba85`, 533 again two days later
from new fragments). A fragment applier being locally correct is not evidence
that the map is globally correct.

## Prior art already in the tree: `tools/gated_map_write.py`

Omitted when this doc landed, and it is the closest thing to a sibling this gate
has. `gated_map_write.py` is the guarded writer for this same JSON, and its
header section **"INJECTIVITY IS TWO INVARIANTS, NOT ONE (doc 66, 2026-08-06)"**
already names the axes:

- **P5 — map-side name injectivity.** "Is this name already a value in the MAP."
  That is the invariant this gate enforces, globally and after the fact.
- **P7 — object-side name injectivity.** Is the name already among the
  post-rename function names of the split **object** the address lands in.
  Doc 65 §4.2 found four split objects already carrying a name the map assigns
  to a second address; that sweep found a fifth. The reader every strict verdict
  passes through — decomp-synth `tools/il_witness/coff_ppc.parse` — keys its
  `funcs` dict by NAME, so one of the two functions simply becomes unreachable.
  `--audit-objects` runs P7 over the whole map with no write.

**They are complementary, not competing.** P5/P7 are *pre-write* gates on the
rows a writer is ADDING; this gate is a *whole-file, post-hoc* assertion that
runs in the build, so it also sees collisions introduced by a hand edit, by a
fragment applier that never went through `gated_map_write`, or by a change to
the `_denylist`/null filter that alters which rows are applied. Neither
subsumes the other: P7 sees a class of defect this gate cannot (the map is
injective, the OBJECT is not), and this gate sees a class P5/P7 cannot (rows
already in the file, and any writer that bypassed the gated path).

Two things to carry across when reading that header:

- Its sentence that map-side injectivity **"is not the invariant that matters"**
  is now contradicted and should be read as **scoped to its own context** — the
  argument there is "P5 alone is insufficient for a renamer that writes into an
  object that already has names", which is true. As a claim about map-side
  injectivity in general it is false: the seven names at fifteen addresses
  repaired below were a pure map-side defect, and one of them
  (`?DataDir@UIPanel@@$4...`, two VAs inside one unit) is invisible to any
  object-level or differ-level check.
- **Q4 is a COUNT check** — "duplicate-value count unchanged"
  (`gated_map_write.py`, POST gate list and the `dup_vals` comparison in its
  implementation). That is the exact anti-pattern this gate was built to
  replace: `2eb6307a` records a plan that held the duplicate count at 8 → 8
  while retiring one duplicate and introducing another, which a count
  comparison passes clean. Q4 is still live in the tree. It is not *wrong* as a
  post-write regression tripwire on a small batch, but it must not be read as
  an injectivity guarantee, and a future consolidation should make it set-based.

Also worth carrying across: `gated_map_write`'s **case-variant address-key
trap** (its "WHY P4 FOLDS CASE" section). The renamer reads a key as
`int(k.lower().removeprefix("0x"), 16)`, so `0x82357DD0` and `0x82357dd0` are
one address to the loader and two keys to JSON; the live map shipped 7
uppercase keys, and an exact-string duplicate-key check was structurally
incapable of seeing a second row for the same address. Because this gate scores
the **applied** map through `load_address_map`, it inherits that folding rather
than re-deriving it — which is the same reason it imports the `_denylist`
filter instead of copying it.

## Repairing a violation

In order of preference:

1. **Disprove the surplus copies and `null` them.** For `$4` adjustor thunks the
   name is fully determined by the body it tail-jumps to
   (`scripts/harvest/thunk_name_consistency_scan.py`) — a self-contained proof
   needing no oracle. For byte-twin template bodies the **relocations** separate
   what the bytes cannot.
2. **Re-home onto a free, byte-class-identical name**
   (`scripts/harvest/dupname_rebijection.py`). Never *guess* a re-home target:
   an unfree target just moves the collision, which is the trap `2eb6307a`
   documents in detail.
3. **If identity cannot be established at all**, add the addresses to
   `_denylist` with a rationale in `_denylist_comment`. That unclaims them; it
   does not assert they are wrong.
4. **Only** for a proven file-static, add the name to
   `_internal_linkage_allow`.

Nulling or denying a surplus copy is normally metric-free, because the surplus
copy is sub-100 and contributes no matched function and no matched byte. Measure
it anyway.

## What lane J2 repaired (2026-08-13, `03946970` + `202a7859`, merged `bc9c6bd3`)

> `6092f524` was cited here as the repair commit. That SHA is the pre-rebase
> lane commit and is **not an ancestor of `main`** — it will not survive gc.
> The landed pair is `03946970` (map repair) and `202a7859` (gate), under merge
> `bc9c6bd3`, whose message carries the corrected per-row account below.


Seven names sat at fifteen addresses. Extraction was not at fault — every
colliding VA falls inside exactly one source file's `.text` range in
`splits.txt`. dc3 has zero collisions because it reads its address→name
assignment from a retail `/MAP`; rb3-xenon has no map and infers them, and
collisions are the price of inference.

**Nulled, eight rows, each disproved:**

| VA | unit | was | disproved by |
|---|---|---|---|
| `0x82729d04` | ADSR.cpp | `?Null@Symbol@@QBA_NXZ` (48.57%) | store-only body, no load and no compare |
| `0x827bea98` | FilePath.cpp | `?Null@Symbol@@QBA_NXZ` (91.29%) | loads `0x48(r3)`; compares an `addi` address |
| `0x8270ff68` | MetaMusic.cpp | `?Null@Symbol@@QBA_NXZ` (91.29%) | loads `0x2c(r3)`; compares an `addi` address |
| `0x822c93e0` | CharCollide.cpp | `PropSync<RndAnimatable>` over `ObjOwnerPtr` (0%) | tail-calls `SetObjConcrete` |
| `0x8249d4b0` | EventTrigger.cpp | `PropSync<EventTrigger>` over `ObjPtr` (0%) | tail-calls `SetOwnerObj` |
| `0x8249d5b0` | EventTrigger.cpp | `PropSync<RndDrawable>` over `ObjPtr` (0%) | tail-calls `SetOwnerObj` |
| `0x82604a30` | StorePanel.cpp | `?DataDir@UIPanel@@$4...` (99.75%) | tail-jumps to `?Load@UIPanel@@...` |
| `0x827b6c78` | StorePanel.cpp | `?DataDir@UIPanel@@$4...` (0%) | tail-jumps to `??_GStorePanel@@...` |

`Symbol` is one `const char* mStr` at offset 0 and `Null()` is
`mStr == gNullStr`, so the body must load `0(r3)` and compare against the
*contents* of `gNullStr`; all three bodies fail on both counts. `PropSync<T>`
over `ObjOwnerPtr` tail-calls the `SetOwnerObj` family and over `ObjPtr` calls
`SetObjConcrete`, and the mangled name states which — that test adjudicates all
three pairs, and in every one the copy scoring 100% is the consistent one.
`DataDir`'s `$4` static displacement `EM@` decodes to `0x4c` and matches
neither body (`-0x44` and `0`), independently of the callee test.

**Measured** (`ab_measure --from-dirty`, both legs settled, forced re-split):
ΔMatched +0, Δmasked_equal +0, Δhonest +0, ΔCode **+360 B** (+0.003490pp),
Δfuzzy −0.000942pp, units at 100% 253 → 253 on both rulers. The
`functionRelocDiffs=none` control is **unmoved**, so the +360 B — all of it in
`default/EventTrigger` — is a relocation-name effect and not a code change.

**Per-row count, corrected.** The lane reported "exactly ten rows changed, and
all ten are the intended ones". That is true at the `functionRelocDiffs=none`
control and **false at the shipped `name_check` ruler**, where an **eleventh**
row moves and carries the whole +360 B: `default/EventTrigger`'s
`?PropSync@@YA_NAAUHideDelay@EventTrigger@@` goes **99.94444 → 100.0**.

The mechanism matters more than the count, because the count is what makes it
look benign. `name_check` did not start resolving that call site **correctly**;
it **stopped checking it**. The call site now targets a placeholder
`fn_8249D5B0`, which is unverifiable by objdiff's own definition and therefore
uncharged — where before it was charged against a name we now know was false.
Strictly better than the previous state, and still a 100.0 minted on an
unverifiable site rather than an evidenced one. The evidenced re-home that
would turn it into an evidenced 100.0 is in the worklist below, deliberately
not taken by lane J2: guessing a re-home target is the trap `2eb6307a`
documents.

## STILL UNADJUDICATED — do not close these blind

`??$__destroy_aux@ULevelData@@@stlpmtx_std@@YAXPAULevelData@@ABU__false_type@0@@Z`
is **denied**, not nulled, at two addresses:

- `0x82b5b1d0` — `system/synth_xbox/Synth.cpp` — `addi r3,r3,0x78; b 0x82b69220`
- `0x82b63ec8` — `system/synth_xbox/FxSendMeterEffect.cpp` — `addi r3,r3,0x64; b 0x82b69220`

Both compiled from the **same** base body, so a single struct-offset edit moves
both identically and takes exactly one of them to byte-exact 100 — with a
coin-flip on which. They can never both reach it. Both read 99.5% before the
repair, and `FxSendMeterEffect` sat at 99.31% with 18/21 matched, so the
pressure to close precisely that row was live.

The relocation test that settled `PropSync` **cannot** separate these two: both
target bodies branch to the same unnamed `fn_82B69220`, and our base branches to
`??1String@@UAA@XZ`.

Before reinstating either name, settle on retail bytes **what owns the
`LevelData` member at `+0x78` versus `+0x64`**. Until then the addresses stay in
`_denylist` with their rationale in `_denylist_comment`, and neither row is
scored — which is the point: it is no longer possible to mint a byte-exact
witness against whichever of the two is not the real one.

**Three** floor certifications in `decomp.db` were signed off against bodies
this work refutes or cannot adjudicate — these two `__destroy_aux` bodies,
`Symbol::Null`, and `?DataDir@UIPanel@@$4…`, which surfaced only after the `$4`
thunk-callee test disproved both its VAs. All three are cleared; see
[handoff/laneJ2-at-limit-clearance-2026-08-13.md](handoff/laneJ2-at-limit-clearance-2026-08-13.md).

## Forward worklist (evidenced, not this lane's work)

Follow-ups an independent review evidenced but did not take. Each is recorded
with the evidence that makes it a re-home rather than a guess — the distinction
`2eb6307a` exists to enforce.

- **Three `PropSync` VAs have evidenced re-homes.** Re-homing converts the
  unverifiable `100.0` above into an evidenced one, because the call site
  regains a real name to be checked against:
  - ~~`0x8249d5b0` → the `ObjOwnerPtr<RndDrawable>` instantiation.~~
    **DONE, lane R 2026-08-16** — see "The RTTI channel" below. Measured
    **Δmatched +1 / +204 B on BOTH rulers**.
  - `0x8249d4b0` → `ObjOwnerPtr<EventTrigger>`. **Independently corroborated**
    by lane R's RTTI channel (its `??_R0` reads `.?AVEventTrigger@@`) but not
    applied here — it was outside that lane's brief. ⚠ It is **not** an
    off-by-one against `0x8249d3b0`: `EventTrigger::ProxyCall` has TWO obj
    members (`ObjOwnerPtr<ObjectDir> mProxy`, `ObjOwnerPtr<EventTrigger>
    mEvent`), so one caller legitimately owns both VAs, and `d4b0` reading
    EventTrigger while still null is correct.
  - `0x822c93e0` → `ObjPtr<RndAnimatable>`.
- **One nulled name has a located true home; the other claim is REFUTED.**
  `?DataDir@UIPanel@@$4PPPPPPPM@EM@…` → `0x826412e0` was taken by a later lane
  and is live in the map. **`?Null@Symbol@@QBA_NXZ` does NOT belong at
  `0x8227c70c`** — refuted by lane R, below.
- **A `$4` displacement audit is outstanding, repo-wide.** 17 `$4` rows carry a
  mangled static displacement that contradicts their body. Of the 10 whose jump
  target is named, **10/10 also fail the independent callee-name test** — two
  unrelated tests agreeing on the same rows, which is why this is a finding and
  not decode noise. Example: `0x82604a40`, `?Load@UIPanel@@$4…EM@`, whose name
  says `0x4c` and whose body says `-8`.

## Lane R (2026-08-16): one re-home applied, one REFUTED

Both rows in the worklist above were re-derived from the retail image before
either was touched. One survived on four independent witnesses; the other did
not survive at all. **A lane that re-homes zero rows because the evidence did
not hold is a success** — and half of this lane was exactly that.

### `0x8249d5b0` = `PropSync<ObjOwnerPtr<RndDrawable>>` — APPLIED

The prior review's two witnesses were confirmed and two more were added. The
decisive one is new and is the cheapest of the four:

- **The RTTI channel (new, and it names `T` outright).** Every `PropSync<T>`
  over an obj-pointer ends in `__RTDynamicCast(obj, 0, srcType, targetType, 0)`,
  and `r6` (`targetType`) is the **`??_R0` Type Descriptor for `T`**, whose
  name string sits at `+8`. Reading it needs no map, no oracle and no build:
  `0x8249d5b0` → `.?AVRndDrawable@@`. `r5` is `.?AVObject@Hmx@@` on every row,
  as `dynamic_cast<T*>(Hmx::Object*)` requires.
  This lane validated it on **six** controls whose names were derived by
  unrelated means — `RndAnimatable`, `ObjectDir`, `RndDrawable`(ObjPtr),
  `RndMesh`, `RndTransformable`, `CharLookAt` — 6/6 agree, and it returns
  distinct names per row, so the agreement is not vacuous.
  ★ **Adversarial review (2026-08-16) replaced that sample with the whole
  population and the channel got stronger, not weaker** — figures inherited
  from that review, not measured here: all **146** mapped `??$PropSync@`
  symbols, **88 adjudicable, 0 disagreements, 62 distinct `T` values**, and
  **69 of the 88 are not `_bijection_arbitrary`-flagged**, i.e. **69** clean
  independent controls rather than 6.
- **The callee channel** (the prior lane's test) reproduces, and the review
  measured the partition as **exact rather than merely suggestive**:
  `{0x82383328, 0x8245cc78}` → **14/14 ObjOwnerPtr, 0 ObjPtr**;
  `{0x8227ce58, 0x8238b130}` → **44/44 ObjPtr, 0 ObjOwnerPtr**. It never
  crosses. `0x8249d5b0` calls `SetOwnerObj` ⇒ ObjOwnerPtr.
  Note both callee names are themselves ICF-arbitrary picks, so this channel
  adjudicates the *family*, never `T` — which is why the RTTI channel matters.
- **Caller side:** retail's ONLY `bl` to `0x8249d5b0` is from
  `?PropSync@@YA_NAAUHideDelay@EventTrigger@@…`.
- **Source side:** `EventTrigger::HideDelay`'s only obj member is
  `ObjOwnerPtr<RndDrawable> mHide` (`src/system/rndobj/EventTrigger.h:73`).

`0x8249d5b0` was also removed from `_bijection_arbitrary` — its identity is now
established, and that list means the opposite. Precedent: lane MAPDEF-3
(`db9eb318`) removed the eight VAs it evidenced.

**Measured** (`ab_measure --from-dirty --name-check`, both legs settled, map
kind ⇒ forced re-split, renamer_patched=1824, split fixed point on both legs,
objdiff-cli sha `6a4d96e3b7ecb6e4`):

| | leg A | leg B | Δ |
|---|---|---|---|
| `matched_functions` | 44,443 | 44,444 | **+1** |
| `masked_equal` | 22,898 | 22,898 | +0 |
| honest | 21,545 | 21,546 | **+1** |
| code% `name_check` (shipped) | 36.061363 | 36.063340 | **+0.001977pp / +204 B** |
| code% `none` (control) | 42.729694 | 42.731670 | **+0.001976pp / +204 B** |
| fuzzy | 48.589966 | 48.591938 | +0.001972pp |

One unit moved — `default/EventTrigger` 287 → 288. Units at 100% unchanged on
both rulers, nothing fell off.

⚠ `Δmatched` and `Δcode_bytes` are **exact integers** and are unaffected by
ruler noise. The `name_check` **aggregate code%** is build-unstable at ~0.05pp
(`ab_measure` prints this itself), so the `+0.001977pp` figure is **25× below
its own noise floor** and must not be read as a precise quantity. Quote the
`+1` and the `+204 B`.

⚠ **Read the `none` leg correctly.** It moved by the same +204 B, and that is
*expected and required* here: this is a **first-naming** of a previously-null
address, so a body that never paired now pairs. The fabricated-alias shape is
the opposite one (`name_check` UP while `none` FLAT), and `ab_measure`'s own
classifier returned `REAL_PAIRING`. But the `none` leg is a **sanity check, not
the evidence** — an alias/name tier cannot be validated by the `none` control.
The evidence is the four retail-byte witnesses above.

⛔⛔ **AND THE MEASUREMENT CARRIES ZERO INFORMATION ABOUT WHICH `T` IS CORRECT —
do not cite the +204 B as if it confirmed the name.** Every `ObjOwnerPtr<T>`
body in this family is **masked-identical**, so **any** of the 14 ObjOwnerPtr
candidates placed at this VA would produce *exactly* +1 matched / +204 B on
both rulers, with the same `REAL_PAIRING` verdict. The A/B confirms that a real
body now pairs; it cannot distinguish `RndDrawable` from any sibling
instantiation. **All evidential weight rests on the RTTI / tail-callee
identification above**, and none of it on the bytes moved.

This is the project's standing doctrine — *an alias/name tier cannot be
validated by the `none` control* — extended to **first-namings**: there the
`none` leg does move, which makes the reading look safe, and the movement is
still name-blind. A `REAL_PAIRING` verdict certifies "a body paired", never
"the right name paired".

⚠ **A prediction this lane got WRONG, recorded because it looks authoritative.**
The lane compared our COMDAT against retail's `.pdata` extent, read a 40 B
shortfall, and predicted Δmatched **+0** — byte-exactness impossible. The
measurement returned **+1 / +204 B**.

The comparison was not like-for-like. The COMDAT **section is 252 B**, and it
decomposes as

```
  +0x00     8 B   EH prefix   (ptr to __CxxFrameHandler)
  +0x08   204 B   BODY        <- equals retail's .pdata extent EXACTLY
  +0xd4    40 B   __unwind$<n> EH cleanup funclet
```

so the body never differed from retail at all; the "244 B" the lane quoted is
the symbol size, i.e. **body + funclet**, measured against a **function-only**
retail extent. That is the documented EH-prefix/funclet units error — see
`tools/comdat_retail_verify.py`'s header (lane STL-104), the same class the
STLPORT-1 fix addressed, where a phantom "+8 B STLport bug" turned out to be
the reader billing the successor's EH prefix.

⇒ **Never compare a COMDAT symbol size against a `.pdata` extent.** Subtract
the prefix and the funclet first, or use a reader that reports the body span.
The lesson about not trusting the prediction stands unchanged; only the numbers
were wrong.

### `0x8227c70c` is NOT `?Null@Symbol@@QBA_NXZ` — REFUTED, do not retry

The recorded argument was that `Symbol` is one `const char *mStr` at offset 0,
`gNullStr` is a pointer VARIABLE, so `Null()` must load `0(r3)` and compare
against the *contents* of `gNullStr` — and that `0x8227c70c` was the only body
in `.text` with that shape. **Every one of those sub-claims is true. The
conclusion is still false**, and that is exactly why it is worth recording.

Confirmed first: `gNullStr` is at `0x82c71838` in `.data`, holding `0x82000c55`,
which points at an empty string in `.rdata`. The 28 bytes at `0x8227c70c` are

```
lis    r11, 0x82c7
lwz    r10, 0(r3)          ; mStr, at offset 0
lwz    r11, 0x1838(r11)    ; CONTENTS of gNullStr
subf   r11, r10, r11
cntlzw r11, r11
rlwinm r3,  r11, 27, 31, 31 ; -> 1 iff equal
blr
```

— precisely `return mStr == gNullStr;`, and that 28-byte string occurs **exactly
once** in `.text` (the body is position-independent, so an ICF twin would be
byte-identical; there is none).

**It is nevertheless not a function.** It is interior code of
`??8Symbol@@QBA_NPBD@Z` (`Symbol::operator==(char const*) const`) at
`0x8227c6d0`, which is `{ if (cc) return strcmp(mStr,cc)==0; else return Null(); }`
with `Null()` inlined into the `else` and the `cntlzw/rlwinm/blr` tail **shared**
between the two paths:

- `0x8227c6d8  beq cr6, 0x8227c70c` — the `cc == 0` branch, into "the body".
- `0x8227c708  b   0x8227c720` — the strcmp path jumping into the **interior**
  (6th of 7 instructions) of that same body. **A compiler cannot branch into the
  middle of a different COMDAT**; with `/Gy` on, that alone proves the two are
  one section.
- Branch census over all of `.text`: `0x8227c6d0` has **297 inbound `bl`**;
  `0x8227c70c` has **zero `bl`** and exactly one inbound edge — the `bc` at
  `0x8227c6d8`, **13 instructions** above it, inside the same function. An
  out-of-line COMDAT with no callers would have been removed by `/OPT:REF`, so
  it cannot be one.

⛔ **`.pdata` cannot settle this and must not be quoted as if it had.** Both
`0x8227c6d0` and `0x8227c70c` fall in a `.pdata` **gap** — they are leaf
functions touching neither stack nor LR, so neither gets an unwind record.
`tools/pdata_map_audit.py`'s `interior_of()` returns `None` for the candidate,
and `None` there means *undecidable*, never *valid* (its own docstring says so).
The branch census is what settles it.

★ **What the row would have cost.** Our build **does** emit a standalone
`?Null@Symbol@@QBA_NXZ` COMDAT (`SELECT_ANY`, in 676 objs including
`BandCharacter.obj`, which owns `0x8227c70c`'s split range), and its bytes are
**identical to the retail fragment modulo the two link-time-relocated fields**
(`lis` imm and `lwz` disp, both patched by the `gNullStr` relocation).
So the re-home would have paired and scored a clean **byte-exact 100%** against
a target that is not a function — the precise hazard the top of this document
describes, arriving through the front door, carrying a correct-looking body
argument and a uniqueness proof.

⚠ **Method note for the next lane.** The prior review searched for a *body
shape* and found a *code fragment*. A shape match, even a unique one, is not a
function-identity proof. Before re-homing onto an address that is not a `.pdata`
start, run the branch census: **a real out-of-line COMDAT has `bl` callers.**

Two instrument defects were found and worked around while doing this, both
worth knowing:

- **`tools/pdata_map_audit.py --selftest --sabotage shift` PASSES in a fresh
  worktree**, i.e. the anti-vacuity control is dead exactly where lanes work.
  `fingerprints.json` is gitignored and never travels, so the one leg that
  catches a wrong shift reports `[SKIP]` and the run still says `OK`. Symlink
  it from the main checkout first; the control then reads 55,999/55,999 PASS
  and the sabotage leg correctly FAILs 0/55,999.
- **`tools/gated_map_write.py` cannot perform a re-home.** It refuses an
  existing key (`P4 … would be a PHANTOM EDIT`), which is right for an
  *insert* and means a null→name repair needs a reviewed single-line textual
  edit instead. Its `--selftest` (15/15) is still worth running for the
  writer's other invariants.
