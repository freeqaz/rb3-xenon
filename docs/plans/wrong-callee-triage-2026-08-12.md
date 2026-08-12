# The `name_check` "wrong callee" lane is symbol naming, not source

2026-08-12. Companion to `wrong-callee-triage-2026-08-12.json` (the ranked
worklist) and `scripts/wrong_callee_triage.py` (the instrument).

## What was being acted on

`functionRelocDiffs=name_check` exposes 8,955 rb3-xenon functions; 8,473 of them
are charged by the `different_function` lane alone. A previous triage split that
lane by residency in `scripts/target_symbol_map.json` and read "both spellings
map-resident at DIFFERENT addresses" as a **source defect** — a wrong callee —
with two named sub-classes to repair: missing `POOL_OVERLOAD` on classes that
retail gave a pooled `operator delete`, and 82 two-cycles where two same-class
method bodies had been transposed.

Re-derived against the current tree (`main` @ `2b12cbd7`): 4,188 sites over
1,048 pairs classify as "both mapped, different VAs".

**Both named sub-classes dissolve under measurement.** The split rule is unsound.

## Why the rule is unsound

`target_symbol_map.json` is a VA→name *function*: exactly one name per address.
The retail link ICF-folded thousands of identical COMDATs, so a call to any fold
member resolves to the survivor's single, arbitrary name. Meanwhile our callee's
spelling is often parked on some *other* address — sometimes a real function,
sometimes padding. "Both map-resident at different VAs" is then satisfied with
nothing wrong at the call site.

The map says so itself. `_bijection_arbitrary` lists **1,109 VAs** whose name was
assigned by a bijection over a reloc-masked byte-identical class, with the
comment: *"WHICH name belongs on WHICH VA is NOT established … an oracle may
refine the assignment later without changing any score."* `_icf_arbitrary` adds
27 more.

`scripts/wrong_callee_triage.py` re-derives the split from `orig/45410914/band.exe`
instead — reading both retail bodies, masking relocation-carrying fields, and
measuring each target VA's fan-in over the whole `.text`:

| sub-class | pairs | sites |
|---|--:|--:|
| `fold_thunk_naming` | 36 | **1,797** |
| `residual` | 535 | 1,108 |
| `bijection_class` | 239 | 503 |
| `map_name_unresolved` | 137 | 449 |
| `transposition` | 70 | 203 |
| `map_misassignment` | 27 | 84 |
| `wrapper_not_inlined` / `wrapper_inlined_by_us` | 4 | 44 |
| **total** | **1,048** | **4,188** |

Roughly two thirds of the lane — 2,793 of 4,188 sites — is naming.

## The allocator pairs are one folded thunk, and the POOL_OVERLOAD roster is already complete

The headline pair was `??3BinStream@@SAXPAX@Z` (retail) against `??3@YAXPAX@Z`
(ours) at 1,180 sites, read as a missing pooled `operator delete`. Four
independent measurements say otherwise.

**1. The body is not pooled.** `??3BinStream@@SAXPAX@Z` @ `0x8240DDB0` is *four
bytes*: `b MemFree`. That is exactly `{ MemFree(v); }` — the body of both our
global `::operator delete` (`MemMgr.cpp:60`) and every `MEM_OVERLOAD` class
delete. A retail pooled delete looks different and is unmistakable:

    ??3DataArray@@SAXPAX@Z  0x82270288  12B: mr r4,r3 ; li r3,16 ; b PoolFree

**2. The fan-in is impossible for one class.** `0x8240DDB0` is reached by
**2,308** direct branches spread over every region of `.text` (0x822–0x82c), and
our side of the charge covers 1,283 distinct calling functions in 433 units.
Those are not BinStream deletions; they are the whole game's deletions arriving
at one ICF fold survivor that the map can only name once. The same shape sits on
the `new` side: `??2CriticalSection@@SAPAXI@Z` @ `0x827BD2F0` is `li r4,0 ;
b MemAlloc` with **1,048** callers.

**3. We already emit the name.** `class BinStream` has carried
`MEM_OVERLOAD(BinStream, 0x55)` all along, so `??3BinStream@@SAXPAX@Z` is already
in our tree with the right body. There was never a missing overload to restore.

**4. Retail's pooled roster is 8 classes, and we have all 8.** Disassembling
every `??3…@@SAXPAX@Z` in the map, exactly eight have the
`mr r4,r3 ; li r3,<size> ; b PoolFree` shape — AnimTask (108), DataArray (16),
DataFuncObj (44), DirLoader (168), DxMesh (428), NullLoader (32),
SerialGroupSeqInst (84), WaitSeqInst (72) — and all eight are already
`POOL_OVERLOAD` in our headers and already score **100% at `none`** with the same
12-byte body. Most of the remaining `??3…` names in the map are 4-byte thunks
whose branch destination is something unrelated (`??3TypeProps@@SAXPAX@Z` is
`b UIPanel::Enter`; `??3ADSR@@SAXPAX@Z` is `b _List_base<Note>::clear`) or, in
four cases, a word of **padding** — more of the same arbitrary fold naming.

### Measured refutation

Swapping `MEM_OVERLOAD(BinStream, 0x55)` → `POOL_OVERLOAD(BinStream, 0x55)`,
which is the repair the previous triage proposed, and rebuilding:

| ruler | before | after | complete fns |
|---|---|---|---|
| `none` | 42.220000% | 42.207752% | **+0 / −10** |
| `name_check` | 31.278600% | 31.252516% | +0 / −14 |

The ten functions lost at the control ruler are the scalar deleting destructors
of the BinStream subclasses — `??_GMemStream`, `??_GBufStream`, `??_GNetStream`,
`??_GBinStreamRev`, `??_GWaveFileData`, `??_GIDataChunk`,
`??_GFixedSizeSaveableStream` — plus `AppChild::AppChild`, `SongData::Poll` and
`ChatReceiver::ChatReceiver`, i.e. precisely the code that inlines
`operator delete`. Reverted; baseline restored byte-for-byte (42.220000 /
31.278600, ±0 functions on both rulers).

**A pooled `operator delete` on BinStream is refuted.** Do not retry it, and do
not mint the class-scoped-delete repair on the strength of a folded thunk name.

The 1,797 `fold_thunk_naming` sites are **not reachable from source at all** — no
spelling of a callee in our tree can make the emitted relocation say
`??3BinStream@@SAXPAX@Z` at a site that deletes something else. Their only repair
is an alias group on the fold survivor, which the previous triage explicitly
forbade ("no alias should ever be minted for it") on the strength of the pooled-
delete theory that is now refuted. That call belongs to the alias lane.

## The transpositions are in the MAP, and the metric cannot adjudicate them

70 pairs / 203 sites form 2-cycles. Swapping either side clears both charges, so
`name_check` moving is not evidence — the metric cannot tell a source
transposition from a map transposition. Where semantic evidence exists, it says
**our source is right and the map's two names are swapped**:

| our call site | we call | retail's VA is named |
|---|---|---|
| `TrackPanelDir::UnisonStart` | `BandTrack::UnisonStart` | `BandTrack::UnisonEnd` |
| `TrackPanelDir::UnisonEnd` | `BandTrack::UnisonEnd` | `BandTrack::UnisonStart` |
| `AccomplishmentCategoryCmp::operator()` | `…GetAccomplishmentCategory` | `…GetAccomplishmentGroup` |
| `AccomplishmentGroupCmp::operator()` | `…GetAccomplishmentGroup` | `…GetAccomplishmentCategory` |
| `Game::AdjustForVocalPhrases`, `TrackerUtils::CountVocalPhrasesInSong`, `VocalPart::PostLoad` | `SongDB::GetVocalNoteList` | `SongDB::GetDrumFillInfo` |

`?UnisonStart@BandTrack@@QAAXXZ` @ `0x8234DD30` and `?UnisonEnd@BandTrack@@QAAXXZ`
@ `0x8234DD18` are **both** in `_bijection_arbitrary`; so is
`?GetDrumFillInfo@SongDB@@…` @ `0x82684FA8`. A `TrackPanelDir::UnisonStart` that
calls `BandTrack::UnisonEnd` is not a plausible source defect, and the map
already declares those names unresolved.

So the call-site oracle this lane produces is exactly the "oracle may refine the
assignment later" the map's own comment anticipates — but the repair is an edit
to `target_symbol_map.json`, not to `src/`, and it only takes effect on a
re-split (`obj_target_symbol_renamer.py` is documented idempotent and will not
rewrite an already-mangled name). **Left unlanded deliberately**: it is a
measurement-surface change on shared build state, it is adjacent to the alias
lane's re-derivation, and 34 sha256 pins ride on the eval roster.

## What was deliberately left on the list

* **1,797 sites of `fold_thunk_naming`** — alias-lane work; unreachable from source.
* **952 sites of `bijection_class` + `map_name_unresolved`** — the map's own
  arbitrary assignments; needs the same call-site oracle applied at scale.
* **203 sites of `transposition`** — map edits, evidence gathered above.
* **1,108 sites of `residual`** — genuinely unclassified, and *still* mostly
  naming on inspection. The biggest entries are `~ObjPtr<T>` vs
  `~ObjRefConcrete<T>` (270 sites over four instantiations), which is a
  per-call-site inlining difference on a trivial wrapper rather than a wrong
  callee, and a long tail of destructor pairs whose callers are still `fn_<addr>`
  in our tree. Neither is mechanical.

  The 114-site `list<CharClip*>::insert` vs `list<Hmx::Object*>::insert` pair
  shows how far the naming reaches. Every one of its call sites is inside a
  `PollDeps(list<Hmx::Object*>&, list<Hmx::Object*>&)` override — 45 units,
  `CharIKFingers`, `CharIKHand`, `CharEyes`, `CharWeightSetter`, … — whose
  signature is fixed by the vtable. The C++ type system *forbids* those bodies
  from calling any other instantiation, so the charge cannot be a source defect
  under any spelling; the map simply hung the `CharClip` name on the surviving
  COMDAT. Both spellings are 100 bytes and both score 100% at `none` here.

**No source fix was landed from this lane, because none of the candidates
survived its own evidence.** The instrument, the worklist and the measured
refutation are the deliverable.

## Reproduce

```sh
python3 <decomp-synth>/tools/namecheck_triage.py --repo . --out /tmp/tri
python3 scripts/wrong_callee_triage.py --sites /tmp/tri/sites.jsonl \
    --none-report /tmp/tri/report_none.json \
    -o docs/plans/wrong-callee-triage-2026-08-12.json
```
