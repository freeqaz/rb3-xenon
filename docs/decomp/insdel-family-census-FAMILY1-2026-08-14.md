# `SOURCE_INSDEL` re-censused BY UNIT AND TYPE, ignoring charge bands (lane FAMILY-1, 2026-08-14)

Tree `f75f08fb` (INSDEL-5 merged) + this lane, worktree `~/tmp/wt-family1`.
Ruler **`functionRelocDiffs=name_check`**, read from `report.json`'s
`provenance.diff_config` (`tool_commit 6bf7ba700ce5`, `tool_binary_hash
69e6f63d2b725e33`), not inherited.

Baseline built in-worktree rather than read from main (main carries uncommitted
`symbols.txt`/`src` changes, and a shared-tree read is wrong even when settled).
It reproduces INSDEL-5's leg B **exactly** — `matched_functions` **44,434** ·
`masked_equal` **22,897** ⇒ honest **21,537** · `matched_code` **3,733,044 B** ·
**36.170580%** · `total_code` **10,320,664** — which is the provenance check that
nothing drifted between lanes.

This lane executes INSDEL-5's own recommendation #2: re-census all 804
`SOURCE_INSDEL` rows by unit and type, **ignoring charge bands**, to find the
16+ tail's families without working the tail row by row.

---

## 0. Census self-validation and staleness

The stratum reproduces from `docs/decomp/mpn-lt-100-census-INSTR1.tsv` **exactly**:
**804 rows / 446,724 B**, zero dropped.

Staleness was re-run, not assumed (INSDEL-5's "0 of 160 closed" was band-scoped):

| | rows | bytes |
|---|---:|---:|
| census as written | 804 | 446,724 |
| missing from a fresh `report.json` | **0** | 0 |
| **CLOSED** since the census (`mpn >= 100`) | **27** | 5,864 |
| **LIVE** — this lane's population | **777** | **440,860** |

The 27 closures include INSDEL-5's seven `MemDiffEntry` templates and
`FilterTypeToSym`, which is how the join was verified to be wired correctly.

---

## 1. THE CENSUS — how 777 rows / 440,860 B cluster

### By unit

| | units | rows | bytes | % live |
|---|---:|---:|---:|---:|
| singleton units | 173 | 173 | 111,400 | 25.3% |
| **multi-row units** | **165** | **604** | **329,460** | **74.7%** |

★ **74.7% vs the 41% INSDEL-5 measured inside the 7–15 band.** That is its
"the band boundary cuts through families, not around them" hypothesis confirmed
structurally: the charge-band framing was hiding most of the co-location.

### By unit AND type (532 unit-type clusters)

Clusters are connected components over "same unit AND ≥1 shared distinctive
identifier token" (STL/`Hmx`/`Symbol`/`Object` boilerplate stopworded).

| | clusters | rows | bytes | % live |
|---|---:|---:|---:|---:|
| clusters of ≥2 rows | 120 | 365 | 235,000 | 53.3% |
| singleton clusters | 412 | 412 | 205,860 | 46.7% |
| **of which BAND-MIXED** (rows from ≥2 charge bands) | **61** | — | **118,108** | **26.8%** |

The 61 band-mixed clusters are the direct measurement of the defect INSDEL-5
found by accident: **118 kB of the stratum sits in clusters that a charge-band
sweep would have split across two or more lanes.**

### ★★★ The decisive split: WHICH KIND of "type" joins the cluster

INSDEL-5 established that "multi-row unit" is *not* "shared cause" — `MeshAnim`
has the highest row count in its band and is six unrelated causes. Separating the
join predicate into the form it **refuted** and the form it **proved**:

| join form | clusters | rows | bytes | % live |
|---|---:|---:|---:|---:|
| **shared TEMPLATE-ARGUMENT type** — the `MemDiffEntry` form, PROVEN to pay | 26 | 60 | **14,448** | **3.3%** |
| **same OWNING CLASS only** — the `MeshAnim` form, REFUTED by INSDEL-5 | 117 | 355 | 233,676 | 53.0% |

⇒ **The family form that has actually paid addresses 3.3% of the live stratum.**
The 53% is the form already shown not to imply shared cause. Its largest members
(`kdTree<Triangle>` in `AmbientOcclusion`, `VocalTrack`, `Spotlight`) are large
and register-dirty, i.e. exactly the population no lane has cracked.

⚠ **But this table is a screen, not a verdict — and this lane's own win refutes
its top line.** `StorePanel` is joined only by owning class (the "unproven" form)
and turned out to be a *genuine* two-row shared cause (§3). **The owner-class form
is unreliable, not worthless**; the template-arg form is merely the one that is
reliably right, and it is nearly exhausted.

---

## 2. CONTROL AVAILABILITY — the number the brief asked for, and it is a NULL RESULT

The brief's screen is *"does a 100%-matching neighbour encode the rival shape?"*
Mechanised over the live stratum (self-matches excluded — including a row's own
now-100% self inflated three screens to a fake 100%, caught before use):

| screen | fires on live rows | bytes |
|---|---:|---:|
| unit contains **any** 100%-matching row | **772 / 777 = 99.4%** | 91.6% |
| 100% row sharing a type token ("Tier A") | 671 / 777 = 86.4% | 91.6% |
| no 100%-matching row in the unit at all | 5 / 777 = 0.6% | 0.5% |

⛔ **As a mechanical screen this is VACUOUS — it cannot fail, so it carries no
information.** 296 of 338 units contain a Tier-A row. This is the same hazard as
the single-candidate gate: a witness that fires on everything blocks nothing and
selects nothing.

### Discrimination against a positive control

Enrichment of each screen on the 27 rows that **closed** (things proven yieldable)
versus the 777 still open:

| screen | closed (27) | open (777) | enrichment |
|---|---:|---:|---:|
| unit has any 100% row | 100.0% | 99.4% | **1.01×** |
| same owning class at 100% | 85.2% | 78.0% | 1.09× |
| shared template-arg with a 100% row | 25.9% | 12.6% | 2.06× |
| same function name at 100% elsewhere | 51.9% | 25.2% | 2.06× |
| a 100% row within 8 B of the same size | 59.3% | 43.8% | 1.35× |
| **register-clean (0 register charges)** | **81.5%** | **12.7%** | **6.40×** |
| register-clean AND same-class 100% sibling | 70.4% | 8.8% | **8.04×** |

### ⛔⛔ …and the 6.40× is a SIMPSON'S-PARADOX ARTIFACT. Stratified, it is FLAT or DEPLETED.

Register-cleanliness is **confounded with charge count**, which is what selects
the bands the closures came from:

| charge band | open rows | register-clean | live bytes | closed rows | closed regclean |
|---|---:|---:|---:|---:|---:|
| ≤3 | 33 | 29 (**88%**) | 9,836 | 12 | 9 (**75%**) |
| 4–6 | 51 | 27 (53%) | 10,336 | 3 | 2 (67%) |
| 7–15 | 150 | 29 (19%) | 43,120 | 10 | 10 (100%) |
| **16+** | **543** | **14 (3%)** | **377,568** | 2 | 1 |

In the ≤3 band the screen is **0.85× — depleted**. The whole-stratum 6.40× exists
only because closures concentrate in low bands where register-cleanliness is
common (88%) and open rows concentrate in 16+ where it is rare (3%). The 7–15 cell
is additionally circular: INSDEL-5 *selected* on register-cleanliness there.

⇒ **The conjunction screen INSDEL-5 recommended mostly re-measures charge count.**
It is still the best available targeting rule — it found this lane's win — but its
enrichment must not be quoted as 6–8×.

### The honest control-availability answer

**Control availability is not the binding constraint and never was — it is ~99%
and unfalsifiable at the unit level.** What is scarce is a control that encodes
*the specific rival shape*, and that is only decidable by reading the charged
instructions. It cannot be screened from symbol names. The real cost driver is
triage, and the real scarcity is **register-clean rows: 99 of 777, 6.7% of bytes.**

---

## 3. What was closed — `StorePanel`, +2 functions / +280 B

INSDEL-5's recommended target #1, reached by the register-clean rule. Three
defects, two of them one shared cause. Full derivation is in the commit message
and as in-source notes; summary:

| row | was | now | bytes |
|---|---:|---:|---:|
| `IsLoaded` — dropped retail's third clause | 62.0% | **100.0%** | +112 |
| `Enter` — wrong virtual (`StoreProfile` → `StoreUser`) | 61.4% | **100.0%** | +168 |
| `Load` — same wrong virtual | 91.0% | 95.3% | +0 (stays sub-100) |

**Controls read BEFORE editing, in every case:**

1. `IsLoaded`: retail dereferences `0x84`/`0x54`; `/d1reportSingleClassLayout`
   puts `mPostPurchaseState` at 0x84 and `mLoadOk` at 0x54 ⇒ **identity, not
   shape**. The rb3-Wii oracle independently spells the same three-clause
   expression, and `Enter()` already writes the literal `2`.
2. `Load`/`Enter`: `StorePanel.h:71` already carried a **prior lane's in-source
   note** predicting slot 17 = `StoreUser` from `band.exe`. The oracle declares
   `StoreUser()` in that slot and no `StoreProfile` at all.
3. `Enter` is decisive **on retail bytes alone**: its callee is the NAMED symbol
   `?IsUserSignedIntoLive@PlatformMgr@@QBA_NPBVLocalUser@@@Z`, whose signature
   takes `const LocalUser*`. Signature adjudication at the call site, per MPNGAP-1.

⚠ The `!mLoadOk` polarity reads backwards for a member of that name. Both retail
and the oracle say it. **It was not "corrected"** — noted in source instead.

**A/B pre-registered `+2 / +280 B` and measured `+2 / +280 B / +0.002714 pp`.**
`0 units fell off 100% on EITHER ruler` — the `volatile` trap did not fire.

### Declined, with the reason

- **`Load`'s last 6 charges**: retail dispatches `GetPadNum` through a
  **vtordisp'd virtual base** (vbptr@4, slot 25) where our `LocalUser::GetPadNum`
  is own-vtable slot 0. Closing it means restructuring `User`/`LocalUser`'s
  virtual-base topology — tree-wide blast radius, and it buys **0 bytes** either
  way since the row stays sub-100.
- **`EnumerateOffers` (692 B)** carries the *same* `StoreProfile`/`StoreUser`
  defect (visible as `diff_arg lwz [off:+44]`) but needs a full body port
  (20 inserts / 38 deletes / register swaps). Half-porting it would depress fuzzy
  without crossing. Left named for a dedicated lane.

---

## 4. HANDOFF — the remaining actionable surface is 9,616 B in 15 units

Units with ≥2 register-clean rows — the only screen that has ever paid, with its
enrichment corrected to "roughly charge-count, stratify before quoting":

| bytes | unit | note |
|---:|---|---|
| 1,644 | `default/StorePanel` | **worked by this lane** (280 B taken; `Handle` 1,192 B remains, unrelated `ProfileSwappedMsg`/frame cause) |
| 1,292 | `default/CameraShot` | `CamShot::Copy` is INSDEL-5's **recorded open contradiction** — do not fix cheaply |
| **1,256** | `default/band3/meta_band/StoreMainPanel` | **best remaining row in the tree** — see below |
| 1,196 | `default/FocusTracker` | **do-not-reopen list** |
| 876 | `default/MoveMgr` | INSDEL-5 already judged it not a family |
| 700 | `default/GemTrack` | `DrawBeatLine` 684 B / 13 charges |
| 560 | `default/Archive` | `GetFileInfo` 432 B / 4 charges, fuzzy 97.9 |
| 504 | `default/LightPreset` | |
| 472 | `default/MusicLibraryStore` | ctor + `ClearPreview`, 5 charges each |
| 388 | `default/BandCharacter` | three identical 112 B `NewObject` stubs — likely one cause |
| ≤264 | `CharClip`, `SongLayout`, `MeshAnim`, `ModifierMgr`, `MatAnim` | |

### ★ `?FinishLoad@StoreMainPanel@@UAAXXZ` — 592 B behind ONE instruction

The single highest-value characterized row found: `fuzzy 99.32`, **1 charge**.

```
25 | stwu r3, 0x4, r27          mCoverArtMats[i] = mat
26 | mr   r29, r3
27 | lwz  r4, 0x78, r30         mNoneTex
28 | addi r3, r3, 0x8c
29 | stw  r29, 0x50, r31        <-- TARGET ONLY
30 | bl   SetObjConcrete<...>
31 | lwz  r10, 0x188, r29       reads r29, NOT the stack slot
```

Retail spills `mat` into `0x50(r31)` and **never reloads it** — a dead spill into
the slot otherwise used for by-value `Symbol` temporaries (idx 82/89/123/135, all
matching). No source lever is visible; the permuter is off by directive.

⚠ **Priced before briefing, per RESIDUAL-1.** The two relocation-name divergences
(`MakeString<const char*>` vs `MakeString<int>`; `ObjRefConcrete<Hmx::Object>` vs
`<RndTex>`) are **already forgiven as folds** and are *not* additional charges —
so unlike `CustomizePanel::Handle`, closing the one instruction really does
collect all 592 B. Note the argument register `mr r4, r28` is **identical on both
sides and holds an int**, so the `MakeString` template argument is unknowable from
these bytes and is not the lever.

---

## 5. Verdict for the next lane

**Do not fund another `SOURCE_INSDEL` sweep of any shape as a byte lever.**

- **85.6% of the remaining bytes (543 rows / 377,568 B) are the 16+ tail, and 97%
  of it is register-dirty** — averaging 38.6 register charges and 607 B per row.
  Every screen that has paid across six lanes selects **6.7% of the stratum**.
- **The family surface under the proven join form is 14,448 B total**, most of it
  already flagged do-not-reopen or already worked.
- INSDEL-5's hypothesis was **right about the mechanism** (61 band-mixed clusters
  / 118 kB) and it is nonetheless **not a lever**, because the tail halves of those
  clusters are register-dirty. The `MemDiffEntry` case — where the out-of-band
  members fell to the same one-line edit — was the favourable case, not the modal
  one, and its out-of-band members **have already been collected** (they are in
  this lane's closed-27).
- The correct residual instruction is **row-level and byte-priced**: work
  `StoreMainPanel::FinishLoad` (592 B / 1 charge) and the 4–13-charge entries in
  §4, then stop. That is ~9.6 kB of surface, not 440 kB.

Six lanes of evidence now say the same thing from three directions: charge band,
family, and control availability all select the same small register-clean
sub-population, and it is nearly drained.

## Tooling

`docs/decomp/insdel-family-census-FAMILY1.tsv` — every live row with its
cluster id, unit, shared type, band, charge and register counts.
Scripts: `~/tmp/family1_census*.py`, `~/tmp/family1_ctrl.py`,
`~/tmp/family1_disc.py`, `~/tmp/family1_final.py`, `~/tmp/family1_parse.py`.
