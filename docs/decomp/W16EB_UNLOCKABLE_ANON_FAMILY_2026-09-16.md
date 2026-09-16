# W16-EB — the anonymous `Unlockable@?A0xf8e4b4b5@` row family, adjudicated

**Date:** 2026-09-16 · **Lane:** W16-EB · **Base:** main `c8101305` · **Branch:** `w16-eb`
**Ruler:** `name_check` (graded, the shipped default) — read from `report.json`'s
`provenance.diff_config`, not assumed. `total_code` **10,247,068**, `total_functions`
**69,240**, read from the keys (never hardcoded).

## Verdict in one line

**The coordinator's hypothesis — "address-pin attribution artifacts" — is REFUTED as a
general claim, and the premise behind it was wrong twice over.** All 11 rows are *real
retail functions* at real addresses inside real pinned blocks. `Unlockable` is not "a
band3 anonymous type" at all: it is a **Dance Central 3** type, and these are **ICF fold
survivors wearing the twin binary's spelling**. But the refutation is not uniform — the
family splits three ways, and 4 of the 11 rows are bodies we genuinely do not hold.

## 1. The population (verified, not inherited)

11 rows / **1,612 B**, every one at `fuzzy == 0` **and** `mpn == 0`, none `masked_equal`,
across 9 units. Sizes are `.pdata`-authoritative and agree with `report.json`.

| VA | unit | size |
|---|---|---|
| 0x822a25e8 | band3/bandtrack/Gem | 96 |
| 0x822a3010 | Character | 136 |
| 0x82346e50 | PracticeSection | 60 |
| 0x82348390 | MoveMgr | 96 |
| 0x823495e0 | Morph | 328 |
| 0x826d2418 | band3/game/Tracker | 108 |
| 0x826d2488 | band3/game/Tracker | 124 |
| 0x827070f0 | Sequence | 96 |
| 0x82ba3300 | VocalTrack | 96 |
| 0x82ba8e98 | VocalTrack | 136 |
| 0x82ba96e0 | VocalTrack | 336 |

Every VA resolves inside its unit's own `.text` block in `splits.txt` (keyed on full path,
never `basename()`), and every VA is a genuine function start: retail bytes begin
`7d8802a6` (`mflr r0`) and lane-au-4 independently recorded `va_status: LIVE_FN_START`.

## 2. Where the name came from — DC3, not RB3

- DC3's leaked map `orig/373307D9/ham_xbox_r.map` carries **68 `Unlockable` lines and 81
  `f8e4b4b5` lines, every one attributed to `meta_ham:MetagameRank.obj`.**
- `dc3-decomp/src/lazer/meta_ham/MetagameRank.cpp` declares, in an anonymous namespace,
  `struct Unlockable` (size 0x20) plus `std::vector<Unlockable> gUnlockables` and
  `std::vector<std::vector<Unlockable *>> gTiers` — *exactly* the instantiations seen here.
- `meta_ham` is Dance Central's metagame. RB3 has no such subsystem. `Unlockable` has
  **zero** references in `rb3-xenon/src/` and **zero** in the rb3-Wii oracle.
- The names entered via the DC3 content-matching naming pass (`ac7bb136`, "1377 globally
  unique names drawn from dc3_content_match"), and were re-keyed wholesale at the TU5 flip
  (`a320bc12`), which is why `git log -S` reports the flip commit.

Consequences that follow immediately, and that no amount of source work changes:

1. **No source edit can ever pair these rows.** The MSVC `?A0x<hash>` encodes the
   *declaring file's path on Harmonix's build host*; this one encodes a **DC3** path. We
   will never emit the symbol.
2. **`anon_ns` cannot close these rows** — it cannot rewrite anything *into* an `Unlockable`
   spelling, because we never instantiate the type. Across all **1,219** compiled objects we
   emit **zero** `Unlockable`. ⚠ But see 2.1: the pass is *not* idle here, and what it
   actually does runs in the harmful direction.
3. **`Unlockable`'s absence from `band.exe` is NOT evidence of anything.** The lane
   DC3-ONLY-PINS instrument (`OBJ_CLASSNAME(X)` emits the literal `"X"`) has **no recall for
   a POD struct in an anonymous namespace** — it never emits a class-name string. Do not
   cite the string absence as a finding; it is a vacuous test on this population.

### 2.1 ⚠ The imported DC3 name is NOT inert — it leaks into OUR OWN symbol table

**I wrote the opposite into this document's first draft and the measurement refuted it.**
Our compiled objects contain zero `Unlockable`, but the hash `f8e4b4b5` occurs **6 times,
in 2 of our 1,219 compiled objects**:

| our obj | our symbol stamped with the foreign hash | the real entity |
|---|---|---|
| `system/synth/Sequence.obj` | `?RandomVal@?A0xf8e4b4b5@@YAMMM@Z` | `Sequence.cpp:24` `namespace { float RandomVal(float,float) }` |
| `system/gesture/SkeletonClip.obj` | `??R<lambda0>@?A0xf8e4b4b5@@…` + 2 `lower_bound` instantiations | `SkeletonClip.cpp:125` lambda |

The mechanism is exactly the documented one, running on poisoned input: `anon_ns` assigns a
hash **per symbol, by asking the TARGET object which hashes belong in those positions**. Our
`Sequence.cpp` and `SkeletonClip.cpp` each declare a genuine anonymous-namespace entity that
needs a hash — and the **only** candidate hash present in their target objects is
`f8e4b4b5`, which is DC3's `MetagameRank.cpp` path hash, imported by the naming pass. So the
patcher borrows the foreign hash and stamps it on our unrelated entities.

**Exposure: 9 of 2,517 target objects carry `f8e4b4b5`** — `VocalTrack` (14), `auto_00_…_rdata`
(21), `SkeletonClip` (7), `Morph` (7), `MoveMgr` (6), `Character` (4), `HamCamTransform` (2),
`PracticeSection` (2), `Sequence` (2). (In units like `SkeletonClip` the occurrences are
*referenced callees*, not definitions — which is why a unit with no `Unlockable` row of its
own is still exposed.) Two of our objects were actually stamped; the rest have no
anonymous-namespace entity of their own to rename.

The hash is a function of build-host name + **declaring file path**, so RB3's `Sequence.cpp`
cannot share DC3's `MetagameRank.cpp` hash except by coincidence. ⇒ **this stamp is almost
certainly wrong**, and it is a contamination channel carrying the imported-name defect out of
the map and into our compiled symbol table.

⚠ **NOT ACTED ON, deliberately.** Changing the patcher or the map is a build-wide change that
must be priced by a whole-binary A/B, which is out of scope for an adjudication lane; and
under `name_check` a symbol rename is charged at every call site, so the sign is not
predictable from first principles. Filed as a follow-up with its mechanism proven, not a
suspicion. `HamCamTransform` is worth a look by whoever picks it up — `HamCam*` is itself
DC3 (`ham`) nomenclature, so that unit may belong to the same imported-name stratum.

### Control: the rest of the anonymous-namespace stratum

Across the whole binary there are **65 anon-ns hashes / 159 rows / 22,872 B**, of which only
**19 rows / 2,396 B** sit at `fuzzy 0`. `f8e4b4b5` alone supplies **11 of those 19 rows
(1,612 B = 67%)** and is **the only hash whose rows are 100% at zero**. Every other hash
mostly matches. ⇒ the machinery works; this hash fails *because it is foreign*, not because
the pipeline is broken. (A finding with no untreated-population control would not have
separated those two explanations.)

## 3. Do we hold the bodies? — `scripts/harvest/homing_reverse.py`

Reloc-masked byte-identical body scan of each retail VA against **1,219** compiled objects.

**Instrument control first** (a 0-hit that agrees with the prior is the trap this repo warns
about): four *known-matched* rows of comparable size — `0x82ba3360` (328 B),
`0x826d2280` (192 B), `0x82707190` (100 B), `0x82ba9038` (316 B) — were each **found**, 1–2
hits, in exactly the right unit. **The instrument discriminates**, so a 0-hit is a real
negative.

| VA | size | obj-hits | distinct units | units/1219 | owner's own obj defines an identical body |
|---|---|---|---|---|---|
| 0x826d2488 | 124 | **2** | 2 | **0.16%** | `resize<vector<TrackerPlayerDisplay>>` |
| 0x82ba8e98 | 136 | 8 | 8 | 0.66% | `~vector<deque<TubePlate*>>` |
| 0x82ba3300 | 96 | 16 | 16 | 1.3% | `__destroy_range_aux<reverse_iterator<deque<TubePlate*>*>>` |
| 0x826d2418 | 108 | 176 | 70 | 5.7% | `_M_fill_insert<vector<TrackerPlayerDisplay>>` |
| 0x822a25e8 | 96 | 456 | 401 | 32.9% | yes — but see below |
| 0x822a3010 | 136 | 453 | 400 | 32.8% | yes — but see below |
| 0x82346e50 | 60 | 1845 | 484 | 39.7% | yes — but see below |
| 0x82348390 | 96 | **0** | 0 | — | **no body held anywhere** |
| 0x823495e0 | 328 | **0** | 0 | — | **no body held anywhere** |
| 0x827070f0 | 96 | **0** | 0 | — | **no body held anywhere** |
| 0x82ba96e0 | 336 | **0** | 0 | — | **no body held anywhere** |

★ **"The owner's obj defines a byte-identical body" is only informative when the fold class
is SMALL.** At 400–484 of 1,219 units, an owner hit is ~33–40% likely *by chance* — the
statement is very nearly vacuous for Gem / Character / PracticeSection, and it must not be
briefed as evidence for them. At 2/1219 and 8/1219 it is near-decisive. This is the same
disease as any detector that confirms whatever it is pointed at; the denominator is the
control.

## 4. The three-way split of the 1,612 B

**(a) REAL membership, body held, identity recoverable — 4 rows / 464 B.**
`0x826d2488`, `0x826d2418` (Tracker), `0x82ba3300`, `0x82ba8e98` (VocalTrack). Small fold
class *and* the byte-identical body in the owning unit's own object is over a type that
belongs to that unit — `TrackerPlayerDisplay` for Tracker, `deque<TubePlate*>` for
VocalTrack. These are that unit's own COMDATs; the DC3 spelling is a proxy name for the
fold class. `0x82ba3300` and `0x82ba8e98` additionally carry installed alias groups (1482,
1593) whose folded members are the `deque<TubePlate*>` spellings — independent corroboration
from a different instrument.

**(b) Real function, identity UNDECIDABLE by byte identity — 3 rows / 292 B.**
`0x822a25e8`, `0x822a3010`, `0x82346e50`. The fold class spans a third of the binary's
objects, so bytes cannot say which name belongs. This is precisely the `_bijection_arbitrary`
doctrine: the match would be byte-true under *any* assignment, so the assignment is not
established. Membership here is neither proven nor refuted.

**(c) Real retail function whose body we do NOT hold — 4 rows / 856 B.**
`0x82348390` (MoveMgr), `0x823495e0` (Morph), `0x827070f0` (Sequence), `0x82ba96e0`
(VocalTrack). Zero byte-identical COMDATs anywhere in 1,219 objects, against a control that
proves the scan finds what it should. These are instantiations retail has and we never emit.

⇒ **"We do not hold the body" is semantically misleading for 7 of 11 rows and literally true
for 4.** The standing finding applies, but only partially — do not generalise it here.

## 5. Reconciling the prior art (two prior lanes disagree, and one was wrong)

- **`lane-ao-map-ownership-2026-07-26`** found `0x826d2488` byte-identical to a sibling at
  `0x822c87b0` and concluded "the Tracker instantiation is the one in Tracker's range".
  **Independently reproduced here by a different route**: our own `Tracker.obj` emits
  `resize<vector<TrackerPlayerDisplay>>` and the fold class is 2 objects wide. Confirmed.
- **`research/2026-06-11-bp4-vocaltrack.md`** walled this family as *"foreign COMDATs the
  retail linker interleaved"*, on the warrant that the types "appear in NEITHER our
  VocalTrack.cpp nor the oracle". ⛔ **That inference is invalid under ICF**: a fold survivor
  wears the *survivor's* spelling, so the absence of `Unlockable` from our source says
  nothing about whose COMDAT occupies the address. Measured here, 2 of the 3 VocalTrack rows
  are VocalTrack's own `deque<TubePlate*>` instantiations — i.e. **not foreign**. (Foreign
  interleave is real and does occur in that span — `_Copy_Construct<Merger@FileMerger>` is a
  genuine instance — but it was concluded from a warrant that cannot support it.)
- **`lane-au-4-wrongunit-map-worklist.json`** labels 8 of the 11 `WRONG-UNIT` with
  `n_definers: 0`. ⚠ **That label is TAUTOLOGICAL for this family**: `n_definers` counts our
  objects defining that exact mangled name, which is structurally impossible for a DC3-only
  anon-namespace spelling. It carries no information about membership, and must not be
  cited as evidence of one.
- **`comdat-fold-gate-2026-08-12.json`** REFUSED an alias at `0x822a3010` because "retail
  extent 136 B vs our COMDAT 128 B". ⚠ That is the exact shape of the **STLPORT-1 reader
  artifact** (a COMDAT span billing the successor's EH-funclet prefix). Flagged, not
  re-litigated — but the refusal should not be inherited as settled.

## 6. What this is worth, and what NOT to do

- **This is not a source-work vein.** 0 of 1,612 B is reachable by writing C++: 7 rows
  already have the body compiled under a different spelling, and the other 4 would still
  carry a name we can never emit.
- **Renaming is a bet, not a freebie.** Under `name_check`, repairing a *wrong existing*
  name can pay, but proving a name wrong does not make renaming safe — if the base object
  cannot define the replacement, the row is pinned at 0% permanently. The strongest
  candidate on the evidence is `0x826d2488` (fold class of 2, owner-type match); the
  VocalTrack pair merely corroborates alias groups that already exist.
- ⛔ **Do not mint aliases from this.** An alias is pure forgiveness under `name_check` and
  lifts the score *by construction*; the `none` control cannot detect a fabricated one.
- ⛔ **Do not re-home or add pins for score**, and do not stub a `Unlockable` type into
  existence — that is metric fitting, and it would buy pairable rows at 0% with no content.
- Under the standing directive that **accuracy beats headline %**, the correct disposition is
  to record these as an *identification* backlog of known cause, not a decomp backlog.
- **The one follow-up with real content is §2.1**, not the 1,612 B: a DC3 hash is being
  stamped onto our own symbols in `Sequence.obj` and `SkeletonClip.obj`. That is a
  correctness question about our symbol table, which outranks the byte count.

## 7. Pre-registrations and how they fared

1. "The 3 VocalTrack + 2 Tracker rows will find byte-identical bodies in their own objs" —
   **HELD for 4 of 5, MISSED on `0x82ba96e0`** (336 B), which has zero hits anywhere.
2. "MoveMgr and Character rows will hit in other units or not at all" — **MISSED for
   Character**: `0x822a3010` hits 400 units *including* `Character.obj`. Held for MoveMgr.
3. "At least 6 of 11 will have >= 1 hit" — **HELD** (7 of 11).

The two misses are why the verdict is a three-way split rather than the clean binary both
the coordinator's framing and my own priors expected.

## 8. Provenance of every number here

`report.json` at main `c8101305` (ruler `name_check`, `tool_commit a5f0ea903ec1`); retail
bytes read from `orig/45410914/band.exe` via PE section mapping in Python (never the shell
`grep`, which is a ugrep shim with `-I` and yields silent false negatives on binaries);
fold-class figures from `scripts/harvest/homing_reverse.py` over 1,219 objects with the
4-row discrimination control above; DC3 figures from `dc3-decomp/orig/373307D9/ham_xbox_r.map`
counted with `command grep -ac`.
