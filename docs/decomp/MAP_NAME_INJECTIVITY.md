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
  - `0x8249d5b0` → the `ObjOwnerPtr<RndDrawable>` instantiation. Two
    independent witnesses: its own tail call is `SetOwnerObj`, **and** the
    caller-side relocation in base `EventTrigger.obj` names that exact
    instantiation at the same offset.
  - `0x8249d4b0` → `ObjOwnerPtr<EventTrigger>`.
  - `0x822c93e0` → `ObjPtr<RndAnimatable>`.
- **Two nulled names have located true homes.** `?Null@Symbol@@QBA_NXZ` belongs
  at `0x8227c70c`; `?DataDir@UIPanel@@$4PPPPPPPM@EM@…` at `0x826412e0`. Both
  were left unclaimed by lane J2 rather than moved.
- **A `$4` displacement audit is outstanding, repo-wide.** 17 `$4` rows carry a
  mangled static displacement that contradicts their body. Of the 10 whose jump
  target is named, **10/10 also fail the independent callee-name test** — two
  unrelated tests agreeing on the same rows, which is why this is a finding and
  not decode noise. Example: `0x82604a40`, `?Load@UIPanel@@$4…EM@`, whose name
  says `0x4c` and whose body says `-8`.
