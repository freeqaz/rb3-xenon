# Lane EC-2 — the COMPLETABLE bucket's misattribution contamination, SIZED

Tree: `5555db76` → landed at `361a2565`. Successor to lane EB-3
(`EB3_COMPLETABLE_FRONTIER_FINDINGS_2026-08-03.md`), which confirmed four
misattributed rows and ran out of budget before adjudicating them.

**Landed:** +1 unit at 100% (254 → 255), Δmatched **+0**, Δcode% **+0.000000pp**.
**Sized:** ~15% of the COMPLETABLE units and ~7% of its blocker rows are
misattribution artifacts, not source residue — and the published ceiling was
over-stated by **at least 3 units that could never have been completed at all**.

## Baseline (re-measured, leg A of the landed A/B)

| measure | value |
|---|---:|
| `matched_functions` | 43,852 |
| `total_functions` | 69,298 |
| `matched_code` | 4,234,020 |
| `total_code` | **10,689,000** |
| `matched_code_percent` | 39.611004 |
| `masked_equal_functions` | 22,724 |
| honest | 21,128 |
| AT_100 / COMPLETABLE / ceiling | 254 / 39 / 293 |

Reproduces EB-3's census bucket-for-bucket at `2e589b9b` + the three lanes since
(EB-2 +1, EB-1 +3 = the +4 on `matched`). ⚠ `total_code` moved again,
10,688,948 → **10,689,000**: read the key, never a memorised constant.

## ★★★ FINDING 1 — misattribution is CONFINED TO SLIVER PINS. Zero MID blockers are foreign.

The single most useful number in this lane. Cross-tabulating the neighbourhood
oracle (below) against **where the blocker sits in its unit's pinned `.text`
block**:

| block position of the blocker | foreign | native | rate |
|---|---:|---:|---:|
| **WHOLE** — the block is exactly this one function | **8** | 2 | **80.0%** |
| START — first function of a multi-function block | 3 | 4 | 42.9% |
| **MID** — inside a multi-function span | **0** | **31** | **0.0%** |
| END | 0 | 1 | 0.0% |
| untreated control (rows already at 100%) | 8 | 288 | 2.7% |

⇒ **A blocker sitting inside a multi-function span is genuine source residue —
0 of 31, no exceptions.** All contamination lives in *sliver pins*: a whole
`.cpp` pinned to a single 8–172-byte function, which is the shape a speculative
carve leaves behind. START is the predicted boundary artifact (a TU's first
function legitimately neighbours the *previous* TU), which is why it reads
in between rather than at either extreme.

**Operational rule:** rank misattribution suspicion by `WHOLE`-block pins.
Do not spend a minute testing a MID blocker for foreignness.

## The two instruments, and why both were needed

Both are ICF-immune by construction. That mattered: objdiff loads **784 ICF
equivalence entries**, so a tail-call callee name proves only which body you
EQUAL, never whose you ARE.

### A. Incoming-argument register set (`tools/ec2_misattribution_scan.py`)

The registers a body reads *before ever writing them* are its ABI incoming
arguments, fixed by the **signature**. A folded alias shares them exactly, and
no amount of body divergence in our source can invent an incoming FPR argument.
Deliberately **asymmetric**: "target consumes an incoming FPR arg and base does
not" is decisive; the converse is not (retail may ignore a parameter).

Measured on the 39 COMPLETABLE units, with an untreated control of 150 sub-100
named rows sampled from units outside the bucket:

| flag | charged | untreated | enr |
|---|---:|---:|---:|
| `FPR_ARG_TARGET_ONLY` | 1/82 = 1.22% | 0/27 = 0.00% | ∞ |
| `GPR_ARITY_TARGET_HIGHER` | 1/82 = 1.22% | 0/27 = 0.00% | ∞ |
| `THIS_OFFSET_FAR_BEYOND_BASE` | 1/82 = 1.22% | 0/27 = 0.00% | ∞ |
| `FLOAT_TARGET_ONLY` | 2/82 = 2.44% | 2/27 = 7.41% | **0.33× — ANTI-enriched** |

⛔ **`FLOAT_TARGET_ONLY` does NOT discriminate.** The untreated population trips
it at 3× the charged rate. EB-3 used "our `CancelJob` has no float stores at
all" as a witness; the per-instance reasoning survives (see FilterQueue below,
where the *offsets* carry it), but **do not promote float-store presence to a
population classifier.**

⚠ Two vacuities were found in this instrument and fixed before any number was
quoted, and both are the kind that manufacture a clean-looking result:

- **Rows where our obj defines no such symbol (`base_size == 0`) fire every
  asymmetric flag trivially** — the base has no instructions, so it "lacks"
  everything. That is a MISSING IMPLEMENTATION, a different class. **82% of the
  first control sample was this**, so the honest control denominator is 27, not
  150.
- **A control drawn from `mpn == 100` rows is STRUCTURALLY INCAPABLE of
  firing** — matching rows have identical instruction streams on both sides, so
  no asymmetric flag can ever trip. The first run read a beautiful 0/88 and
  meant nothing.

### B. Address-neighbourhood oracle (`tools/ec2_neighbourhood_attribution.py`)

RB3 retail has **no LTCG**, so TU spatial grouping in `.text` is preserved. A
blocker VA whose map-named neighbours share no class with the unit's own
identified classes is attribution-suspect. It never reads the body, so folding
cannot fool it — ICF does not move a function's neighbours.

| stratum | adjudicable | foreign | rate |
|---|---:|---:|---:|
| CHARGED (blockers) | 48 | 10 | **20.83%** |
| UNTREATED (rows at 100%, same units) | 276 | 6 | **2.17%** |
| — | | | **9.58×** |
| template/STL COMDAT stratum, charged | 3 | 1 | 33.3% |
| template/STL COMDAT stratum, untreated | 20 | 2 | 10.0% |

⚠ Two vacuity guards, both of which **inflated** the figure before they were
applied (ungrounded it read 38.46% vs 5.26% = 7.31× over a partly meaningless
denominator):

1. **A unit whose symbols are all FREE functions has an empty own-class set**,
   so "no neighbour shares our class" is true by construction. `FFT`, `Main` and
   `Rnd_NG` are *entirely* this class — 7 rows removed.
2. **Template/STL COMDATs are packed into shared linker pools**, so foreign
   neighbours are expected. Reported as a separate stratum, never pooled.

⚠ **Known false negative, and it is the lane's headline row:**
`FlowQueueable::Deactivate` reads *native* to this oracle, because a
`FlowQueueable` **vbase-adjustor thunk** sits in the neighbourhood — and such
thunks are emitted in the **derived** class's TU (`RndEnvAnim`), not the base's.
Instrument A caught it. **The two instruments are complementary, not
redundant; neither alone sizes this class.**

### C. The third witness, discovered by a near-miss: fuzzy% itself

`HamPhotoDisplay::Save` is `WHOLE`-pinned *and* neighbourhood-foreign
(`BandStarDisplay`, `TourDescPanel`) *and* its unit is 4 rows / 1 blocker — i.e.
excising it would have been a clean **+1 unit**. It is **NOT misattributed.**
Retail writes a `li r11,1` revision, calls `BinStream::WriteEndian`, then
`RndDir::Save` — exactly our shape. The residue is statement order plus a
**uniform 40-byte vbase-displacement gap** (retail `-548`/`-552` where we emit
`-508`/`-512`), which is a real and separately-actionable layout defect.

⇒ **Confirmed misattributions read fuzzy 0.0 / 0.95 / 12.7 / 29.2 / 46.7%;
this one reads 82.72%.** For a body large enough to carry signal, a high fuzzy%
is itself evidence *against* misattribution. The exception is tiny functions —
`SkeletonDir::TestClip` is a genuinely foreign 8-byte body scoring 99.5%,
because a two-instruction `lwz`/`blr` matches on everything but the
displacement.

**Predicate that selects exactly the right set:**
`WHOLE`-block pin **AND** neighbourhood-foreign **AND** (`fuzzy < 50` **OR**
`size <= 16`). It selects 7 of the 8 WHOLE+foreign rows and excludes
`HamPhotoDisplay`, which is the one proven native.

## ★★★ FINDING 2 — the sized census

Over the 39 COMPLETABLE units / 90 blocker rows at baseline:

| class | rows | units |
|---|---:|---:|
| **CONFIRMED misattributed** (≥2 independent axes, adjudicated on retail bytes) | **6 / 90 = 6.7%** | **6 / 39 = 15.4%** |
| flagged by one axis only, not adjudicated | 15 | 12 |
| proven NATIVE despite being flagged | 1 | 1 |
| genuine source residue (incl. all 31 MID blockers) | 68 | — |

**Of the 6 contaminated units, only 1 could ever have converted to a unit
completion.** Three (`FilterQueue`, `HamDriver`, `system/gesture/SkeletonDir`)
have their *entire* pinned extent inside the foreign function, so excision makes
the unit **VANISH** (DG-2), not complete. Two (`PropertyEventProvider`,
`StorePreviewMgr`) carry other blockers.

⇒ **The realisable COMPLETABLE headroom is ~4 units smaller than the 39
advertised, and ~85% of the bucket is genuine source residue.** The frontier is
real; it is just not as cheap at the top as the ranking suggests — which
independently corroborates EB-3's FINDING 3.

## The six confirmed rows, with the witness for each

| unit | row | retail body says | second axis |
|---|---|---|---|
| `FlowQueueable` | `Deactivate(bool)` @`0x82487880`, 168 B | `fmr f31, f1` — **consumes an incoming FPR argument**; `(this,bool)` is r3,r4 only. Calls `Keys<Vector3>::Add` / `Keys<Vector2>::Add` | neighbourhood is wall-to-wall `RndEnvAnim` (dtor, `Copy`, `??_G`, `Load`, `SetType`) ⇒ environment-animation keyframing |
| `system/gesture/SkeletonDir` | `TestClip` @`0x8258A190`, 8 B | `lwz r3, 31860(r3)` vs our `lwz r3, 584(r3)` — a member at **0x7C74** in a ~0x250-byte class | sits between `BandProfile::HasCheated` (`0x8258a180`) and `BandProfile::SetUploadFriendsToken` (`0x8258a198`) inside ~20 `BandProfile` methods; 0x7C74 is plausible for that save object |
| `FilterQueue` | `CancelJob` @`0x826F8F68`, 52 B | stores `1.0f`/`0.0f` into `this+60..72` and erases a vector at `this+640`; ours has the vector at `this+12` | bracketed by `Singer::Jump` and `Singer::HandlePhraseEnd` ⇒ `Singer` code |
| `HamDriver` | `Clear` @`0x824F6EB8`, 12 B | **discards `this`**, loads a global into r3, tail-calls `ContextWrapperPool::FailAllContexts`; DC3's `HamDriver::Clear` is `mLayers.Clear()`, which is what we emit | `??_GContextWrapperPool` is in the same neighbourhood |
| `PropertyEventProvider` | `BandProfile::HasFinishedCampaign` @`0x826F0F18`, 8 B | a **game** class pinned inside an **engine flow** TU; an 8-byte `addi r3,r3,4; b fn_827690D0` thunk we never emit | neighbourhood is `TrainerProgressMeter` |
| `StorePreviewMgr` | `PreviewDownloadCompleteMsg::PreviewDownloadCompleteMsg(bool,bool)` @`0x825C21D0`, 172 B | body calls **`RemoteMachineUpdatedMsg::Type()`** — a different message class. A static `Type()` returns a distinct Symbol, so its relocation differs and ICF cannot fold it ⇒ the callee is trustworthy *here* | neighbourhood is `BandMachineMgr` / `SyncLocalMachineMsg` |

## What was LANDED (`361a2565`)

Deleted 5 map rows and 5 splits blocks (three of them whole entries):

```
splits: FilterQueue.cpp, HamDriver.cpp, system/gesture/SkeletonDir.cpp   (whole entries)
        FlowQueueable.cpp        .text 0x8248787C..0x82487928
        PropertyEventProvider.cpp .text 0x826F0F18..0x826F0F20
map:    0x82487880 0x826f8f68 0x824f6eb8 0x8258a190 0x826f0f18
```

MEASURED by `ab_measure --from-dirty`, forced re-split on **both** legs
(`renamer_patched=1046`), both legs settled to zero work:

```
Δmatched +0 · Δmasked_equal +0 · Δhonest +0 · Δcode% +0.000000pp · Δcode_bytes +0
Δfuzzy −0.000283pp
units at 100%: 254 → 255  (Δ+1; 1 reached 100, 0 fell off)
   +100%  default/FlowQueueable  rows 2→1   DENOMINATOR_SHRANK
```

The zero is **expected**: a name/pin is a masked relocation argument under
`functionRelocDiffs=none`, so a repoint or excision pays exactly nothing on
either headline ruler. Landed for correctness and unit membership.

★ **The check that separates this from metric-fitting**: it is
**denominator-neutral globally**. `total_functions` (69,298) and `total_code`
(10,689,000) are **identical on both legs** — nothing left the denominator. The
five rows moved into six `auto_*` units (`auto_03_8248787C_text`,
`auto_03_824F6EB8_text`, `auto_03_8258A190_text`, `auto_03_826F0F18_text`,
`auto_03_826F8F68_text`, plus a re-derived `auto_01_82210938_pdata`). They were
re-attributed from a TU that does not own them to *unidentified retail code* —
which is the honest state.

Post-landing census: **AT_100 255 · COMPLETABLE 35 · ceiling 290** (was
254/39/293). The ceiling fell by exactly the 3 vanished units.

## ★★ FINDING 3 — the published ceiling was over-stated, and closing units cannot fix that

EB-3 published a **293** ceiling with **39 units of headroom**. Three of those
39 were units whose *entire* pinned extent is another TU's code: they were never
completable by any amount of source work, only deletable. The ceiling is
therefore **290**, and it moved **downward** for the first time in this
campaign's history — every previous re-census moved it up as identification
coverage grew.

⇒ **A ceiling figure is not just perishable (EB-3's lesson), it is not
monotonic.** Carve work inflates it with artifacts; adjudication deflates it
back. Quote it with the date and the tree, and expect both signs.

## Declines, with the witness for each

| unit | decline | is the witness capable of discriminating? |
|---|---|---|
| `HamPhotoDisplay::Save` | **NOT misattributed** despite firing WHOLE + neighbourhood-foreign. Genuine residue: statement order + a uniform **40-byte vbase-displacement gap**. Excising it would have been a fabricated +1. | Yes — 82.72% fuzzy over 128 B, and the retail body has our exact `WriteEndian` → `RndDir::Save` shape. Both sides have real bodies, so nothing here is vacuous. |
| `AccomplishmentPlayerConditional` | flagged, **not landed**: the neighbourhood scan keyed on a map name (`GetType` returning `Symbol`) that differs from the row `report.json` actually presents (`GetType` returning `AccomplishmentType`). That is a *map* defect, not an adjudicated misattribution. Separately, its blocker sits at `0x82b790b8` while the rest of the unit lives at `0x8235Exxx`/`0x82451Axx` — **8 MB away**. | Partly — spatial incoherence within a unit is a strong independent signal, but I did not adjudicate the body. Recorded as a lead. |
| `StorePreviewMgr`, `PropertyEventProvider` residue | confirmed rows removed/landed, but both units carry **other** blockers, so neither converts | n/a |
| the 15 one-axis suspects | not adjudicated — budget | n/a; explicitly a **lower bound**, see below |

## What I did NOT do

- **Did not re-pin** `FilterQueue.cpp`, `HamDriver.cpp` or
  `system/gesture/SkeletonDir.cpp` to their true retail extents. Their homes are
  now **unknown**, not wrong-but-known. That is identification work.
- **Did not adjudicate the 15 one-axis suspects** on retail bytes. The 6/90
  confirmed figure is therefore a **LOWER BOUND**; the upper bound implied by
  the neighbourhood oracle's enrichment is ~10/48 non-template charged rows.
- **Did not run `tools/native_build_gate.sh`** — the change touches only
  `config/` and `scripts/`, no `src/` file, so the gate does not apply.
- **Did not touch `symbols.txt`.**
- Did not re-fund EB-3's refuted statement-order lever, `MoggClip`, or the
  `??_E` lever.

## Reusable output

- `tools/ec2_misattribution_scan.py` — incoming-argument-register witness, with
  the untreated-control machinery and both vacuity guards.
- `tools/ec2_neighbourhood_attribution.py` — neighbourhood oracle, with the
  free-function and template-COMDAT guards and a built-in
  "NOT DISCRIMINATING" refusal when enrichment < 1.5×.
