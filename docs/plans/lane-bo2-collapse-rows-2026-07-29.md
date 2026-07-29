# laneBO2 — draining laneBL's §6.1 "rows that DO collapse" (2026-07-29)

Mission: convert laneBL's §6.1 worklist — **12 TUs / ~304 functions of headroom**,
each with a concrete anchor or unclaimed run already computed — plus the §6.0
provider cluster, into measured matches. Predecessor:
`docs/plans/tu-pin-wave-2026-07-29.md` (laneBL, +599/−38 across 31 TUs), itself
following `docs/plans/wii-oracle-tu-location-2026-07-29.md` (laneBD).

Baseline, verified by the lead in a freshly full-built worktree at main
`9df262c9` with `report.cache` cleared: `measures.matched_functions` **40,302**,
strict-100 by `(unit, name)` **40,087**, `fuzzy_match_percent` 40.420128.

---

## 0. TL;DR

* **Consolidated, MEASURED (not summed): `matched_functions` 40,302 → 40,439,
  strict-100 40,087 → 40,223 = net +136.** Double full build, `report.cache`
  cleared each read, one branch. By `(unit,name)` +167/−31; by bare-name multiset
  **+142/−6**; the two agree at **+136**. The 25-loss gap between the views is
  anonymous `fn_<VA>` symbols re-homing into the newly carved units — which is
  exactly why the gate demands both views.
* **13 of the 14 assigned rows landed; 1 declined on evidence
  (`AssetOffer` — the class exists in neither oracle); 1 refuted as stale
  (`DrumTrackWatcherImpl`, already pinned by laneBL's own lane C).** 14 new units.
* ★ **The lane sum was +136 and the measured consolidation is +136 only because
  the 8 map repoints are included; without them it is +129.** Both numbers were
  measured separately (§2bis) rather than asserted, on the coordinator's standing
  rule that totals are banked from measurement, never from adding up lane claims.
* **Two new TU-location instruments were built, calibrated on ground truth, and
  then partly refuted in the field by the lanes using them** — `td_order.py`
  (§1) and `str_scatter.py` (§1bis). The refutations (§6.0) are the more valuable
  half and are recorded in the tools themselves.
* **laneBL's deferred `BandFaceDeform`/`ReviewDisplay` reconciliation is
  resolved** (§3) — and the two rows resolve by *different* mechanisms and fail in
  *opposite* directions, which is the actual finding.
* **Five laneBL claims refuted or corrected**, including a **fifth phantom**
  (`HamScrollSpeedIndicator`) and the re-opening of the controller family that
  laneBL had priced as unreachable (§6).

---

## 0bis. ★★ CORRECTIONS — two of this lane's own claims, refuted after landing

Both were established by the coordinator and re-verified here against
`orig/45410914/band.exe`. They are recorded at the top because each invalidates a
*method*, not just a number, and the methods are still in circulation.

### 0bis.1 The absolute match count is WORKTREE-DEPENDENT — a delta is only valid within one worktree

Beyond the ~2-function **split-churn floor** (a second split of identical source
does not measure the same as the first; a control at `9df262c9` measures 40,304
where a virgin tree measures 40,302), there is a second, larger effect:

| | baseline | after | delta |
|---|--:|--:|--:|
| coordinator's worktree | 40,304 | **40,439** | **+135** |
| this lane's worktree | 40,304 | **40,441** | **+136** |

Byte-identical content — the coordinator verified `git diff` between the landed
tree and `laneBO2-final` is **empty**. A repeat full build inside one worktree
reproduces its own figure exactly, so this is **not run-to-run noise: it is stable
within a worktree, different between worktrees, and a full `ninja` does not
converge them.** Candidate mechanism, flagged but not diagnosed: objcache's
cross-root `/Fo` path string, long assumed match-irrelevant.

> ★★ **OPERATING RULE: a delta is valid only within a single worktree, with both
> legs in the same split state. NEVER compare absolute counts across worktrees,
> and never quote an absolute as portable.** The landed headline is published as
> a **band, 40,439–40,441**.

This supersedes every absolute in §2 and §2bis below; those figures are correct
*within this lane's worktree* and are left as measured rather than silently
edited. **The delta (+136 here / +135 there) is the transferable quantity.**

### 0bis.2 `.pdata`-absence does NOT prove something is not a function

This lane twice asserted that `0x82630340` "is not a `.pdata` function entry at
all", inferring from the `.pdata` table that no name should be bound there. **The
bracketing observation was right; the inference was wrong.** *Frameless leaf
functions are systematically absent from the X360 `.pdata` table*, so
`.pdata`-absence is not evidence of non-functionhood.

Disassembly settles it (re-verified here):
* `0x8263064C = 0x4BFFFCF5` → **`bl 0x82630340`** — a direct call to the address.
* `0x8263033C = 0x4E800020` (`blr`) — the previous function ends there.
* `0x82630340 = 0x3D60820D` (`lis r11, 0x820D`) — materialising a vtable address,
  the classic destructor opening; `0x82630344 = 0x81430004` (`lwz r10, 4(r3)`).
* `0x82630380 = 0x4BFFF010` → `b 0x8262F390`.

So `0x82630340` **is** a function, and sub-lane B's original identification was
right. Independent corroboration: our `??1NewAwardPanel@@` tail-calls
`??1TexLoadPanel@@` where retail's tail-calls `??1VoiceoverPanel@@` — a different
base class, so the existing pairing is definitively wrong. It reads a full
**100.0 %** only because `functionRelocDiffs=none` masks the differing tail-call
target: **the first full-100.0 instance of the at-100 % defect class in this
project.**

★★ **The root cause of the original refutation is the trap to internalise.** It
computed the `band.exe` file offset as `va − 0x82000000`. That is valid **only for
`.rdata`**; `.text` is RVA `0x00270000` / raw `0x00264E00`, a **`0xB200` delta**.
So string and typedesc reads — which land in `.rdata` — *accidentally work*, while
every `.text` read silently returns unrelated bytes. **An evidence set can be
half-correct and feel completely reliable.**

> ★ **Sanity anchor for any future `band.exe` read:
> `off(0x824DAAD0)` must equal `0x004CF8D0`** (the value in `Spotlight.s`).
> The naive mapping yields `0x004DAAD0` — off by exactly `0xB200`.

**Audit performed in response, so the blast radius is known rather than assumed:**
* `scripts/harvest/tu_locate/str_xref.py` — the shared tool generating the
  20,141 code→string edges that laneBD, laneBL and this wave all reason from —
  resolves addresses through the **section table** (`read()` computes
  `pr + (va − sva)` per section). **Not affected.**
* `td_order.py` and `str_scatter.py` (this wave) both parse the section table;
  verified through their real code path: `off(0x824DAAD0) = 0x004CF8D0` **PASS**,
  and the `f2v` round-trip returns `0x824DAAD0` **PASS**.

⇒ The string channel's accumulated evidence stands. The bug was confined to one
ad-hoc read, not to the shared instruments — but it produced a confident,
fully-argued, wrong refutation, which is why the anchor above is worth running
before trusting any new `.text` byte evidence.

---

## 1. ★★ The lead's contribution: a THIRD TU-location channel

laneBD built two instruments (RTTI class-owned-vtable-slot spans; string-literal
cross-reference) and laneBL sharpened the reduction of the second. Both read
`.text` and `.rdata` *content*. This wave adds a third that reads neither — it
reads **layout order** — and it is therefore genuinely independent of both.

Tool: **`scripts/harvest/tu_locate/td_order.py`** (committed this wave).

### 1.1 The idea

RB3-360 is `/O1 /Oi /GR /EHsc` with **no LTCG**, so the linker preserves TU
spatial grouping — and that holds in `.rdata` as well as `.text`. Every
`.?AV<Class>@@` RTTI type descriptor is emitted by the TU that *defines* the
class. So the `.rdata` typedesc sequence is a proxy for object-file order, and
object-file order is what determines `.text` order.

⇒ Given two classes whose TUs are already **pinned**, any class whose typedesc
sorts between theirs must have its `.text` between theirs. That is a two-sided
**bracket** on an unlocated TU, derived from the retail binary alone.

### 1.2 ★ Calibration — and why the obvious number is the wrong one

The tempting metric is pairwise: do two adjacent typedescs preserve their order
in `.text`? That gives 64.1 % unwindowed and **89.5 %** in the tight regime
(typedescs ≤ 0x40 apart, `.text` ≤ 32 KB apart) against a 50 % chance baseline,
over 514 class→unit joins. **That number flatters the instrument and should not
be quoted.** Preserving the order of two neighbours is a much easier test than
*containing a third TU between them*, which is what the tool is actually used for.

`--score` asks the real question: for a class already pinned in `splits.txt`,
does the bracket built from its **neighbours** contain that class's own `.text`?
(A TU's blocks never contribute to its own bracket, so there is no leakage; only
spatially-coherent pinned classes are scored, since "is it inside" is not
well-posed for a unit scattered over megabytes.)

| regime | n | bracket contains ≥ 99 % of the unit |
|---|--:|--:|
| all | 206 | **78.2 %** |
| zero skipped anchors | 88 | **83.0 %** |
| zero skips **and** bracket ≤ 8 KB | 26 | **69.2 %** |

★★ **NARROW BRACKETS ARE LESS RELIABLE, NOT MORE.** This is the opposite of the
intuition and it is the single most important property of the channel. It is a
selection effect: a narrow bracket means the two flanking neighbours sit close
together, which is exactly what happens when the target TU is **not** between
them at all. A tight bracket is not a confident bracket.

⇒ **Treat a bracket as roughly 4:1 evidence** — a prior that says where to look
and what to test. It cannot settle a pin, and it never overrides
`orig/45410914/band.exe`.

### 1.3 ★ Two failure modes, both found the hard way

1. **Name-vs-stem join.** `Rnd*` classes live in files that drop the prefix
   (class `RndMeshAnim` ↔ unit `MeshAnim.cpp`). A neighbour reported UNPINNED may
   simply be un-joined. Aliased.
2. ★★ **SCATTERED UNITS CANNOT ANCHOR.** A unit whose COMDATs spread over
   megabytes has no single `.text` position, so it cannot bound anything.
   `MeshAnim.cpp` spans **5,740,108 B**; used as an anchor it placed
   `BandFaceDeform` in a `Font`/`LitAnim` neighbourhood **2 MB** from its real
   one. Anchors are now rejected above `--max-spread` (default 64 KB) and the
   bracket reaches further out, printing every skip.

**This cost the lead a retraction mid-wave** — an inference sent to sub-lane C
about `BandFaceDeform` rested on `MeshAnim.cpp` as an anchor and had to be
withdrawn once the coherence test was added. Recorded because the failure is the
lesson: *an inverted or skip-dependent bracket is the tool DECLINING, and a
decline must never be reported as counter-evidence against another channel.*

### 1.4 What the channel settled, independently of the sub-lanes

* **Zero phantoms in this worklist.** All 22 classes touched by this wave (12
  targets, the provider cluster, and every donor) have ≥ 1 raw name hit and
  exactly one `.?AV<Class>@@` descriptor in `band.exe`. laneBL's §2 phantom
  hazard **does not fire here**, so every loss must be adjudicated by one of the
  other three mechanisms (ICF fold, adjustor thunk, reloc-masked byte twin).
* ★ **Twelve of the eighteen target classes have NO class-name string literal at
  all** — only the RTTI descriptor. `CurrentOutfitProvider`, `OutfitProvider`,
  `MakeupProvider`, `EyebrowsProvider`, `FaceTypeProvider`,
  `InstrumentFinishProvider`, `FaceHairProvider`, `AssetOffer`,
  `CharMeshCacheMgr`, `TourPropertyCollection`, `SetlistSortByLocation`,
  `DrumTrackWatcherImpl`. ⇒ **the `OBJ_CLASSNAME` head-marker recipe (laneBL
  §3.1) cannot fire on any of them**, and a lane hunting one is wasting budget.
  Only `BandFaceDeform` (`0x8200FEE0`), `ReviewDisplay` (`0x82031F30`),
  `MultiSelectListPanel` (`0x8209C088`), `SaveLoadStatusPanel` (`0x8209C880`),
  `RetryAudioPanel` (`0x8209C5E8`) and
  `CampaignGoalsLeaderboardChoicePanel` (`0x8209B410`) have one.
* **A structural (non-statistical) proof that `MakeupProvider.cpp` is mis-pinned.**
  It is pinned across `0x8266F730..0x826720B8` while `AssetProvider.cpp` is pinned
  at `0x82670B8C`, **inside** that footprint. One TU's pinned footprint strictly
  containing another's is impossible under preserved grouping with no LTCG ⇒ at
  least one `MakeupProvider` block is mis-attributed, *independent of any
  calibration*. This corroborates laneBL §6.0 by a route that does not use the
  order statistic at all.

---

## 2. The ledger

All deltas are strict-100 **by-name multiset**, double full build, `report.cache`
removed before every read, measured in each lane's own worktree against the
baseline above. Every lane verified the baseline exactly before its first edit.

| lane | TU | verdict | span as landed | gains | losses |
|---|---|---|---|--:|--:|
| A | `meta_band/EyebrowsProvider` | LANDED (carve ← `Leaderboard.cpp`) | `0x8266EC88..0x8266F088` | +5 | |
| A | `meta_band/FaceTypeProvider` | LANDED (carve ← `Leaderboard.cpp`) | `0x8266F088..0x8266F380` | +3 | |
| A | `meta_band/FaceHairProvider` | **EXTENDED** +0x78 | `0x8266F380..0x8266F7A8` | +1 | |
| A | `meta_band/OutfitProvider` | **LANDED (relabel ← `MakeupProvider.cpp`)** | `0x8266F7A8..0x8266FAC4` | +7 | |
| A | `meta_band/InstrumentFinishProvider` | LANDED (carve ← `Leaderboard.cpp`) | `0x8266FAC4..0x8266FDC8` | +4 | |
| A | `meta_band/MakeupProvider` | LANDED (carve + ADD) | `0x8266FDC8..0x8266FF30`, `0x82670090..0x826706F8` | +3 | |
| A | `meta_band/AssetProvider` (`PremiumAssetProvider`) | LANDED (pure ADD) | `0x82670700..0x82670A88` | +2 | |
| A | `meta_band/CurrentOutfitProvider` | **LANDED (relabel ← `MakeupProvider.cpp`)** | `0x82671AC8..0x826720B8` | +4 | |
| A | `meta_band/AssetOffer` | **DECLINED** — source-blocked | span confirmed, unlandable | 0 | |
| | *lane A total* | | | **+30** | **−5** |
| B | `meta_band/SetlistSortByLocation` | LANDED (pure ADD) | `0x825C4B10..0x825C54D8` | +24 | 0 |
| B | `meta_band/CampaignGoalsLeaderboardChoicePanel` | LANDED (2 ADDs) | `0x825F4130..0x825F4598`, `0x825F45F0..0x825F4948` | +13 | 0 |
| B | `meta_band/MultiSelectListPanel` | LANDED (pure ADD) | `0x82626A40..0x82626E50` | +13 | 0 |
| B | `meta_band/SaveLoadStatusPanel` | LANDED (ADD + 1 carve) | `0x82631A20..0x82631DFC` | +11 | 0 |
| B | `meta_band/RetryAudioPanel` | LANDED (2 ADDs + 1 carve) | `0x82630228..0x82630690`, `0x82630778..0x826308B4` | +12 | −1 |
| | *lane B total* | | | **+73** | **−1** |
| C | `system/char/CharMeshCacheMgr` | LANDED (3 ADDs) | `0x8239C380..C3C8`, `C3D0..C504`, `0x8239CCA8..D048` | +8 | 0 |
| C | `system/bandobj/BandFaceDeform` | LANDED (4 ADDs) | `0x822C7128..7680`, `76F0..783C`, `8290..83CC`, `8448..85A8` | +11 | 0 |
| C | `system/bandobj/ReviewDisplay` | LANDED (3 ADDs) | `0x8231E8F8..EA20`, `EAA8..EF28`, `F130..F674` | +13 | 0 |
| C | `band3/tour/TourPropertyCollection` | LANDED (3 ADDs) | `0x82365560..5668`, `5710..58F8`, `5978..5C20` | +7 | 0 |
| C | `system/beatmatch/DrumTrackWatcherImpl` | **REFUTED** — already pinned in main | — | — | — |
| | *lane C total* | | | **+39** | **0** |

Lane A: `matched_functions` 40,302 → **40,327**, strict-100 40,087 → **40,112**
(+30/−5 = **net +25**), identical by `(unit,name)` and by bare name.
Lane C: `matched_functions` 40,302 → **40,341**, strict-100 40,087 → **40,126**
(+39/−0), identical both ways.

★ **`AssetOffer` is the wave's honest shortfall.** Everything laneBL claimed about
it is *confirmed* — `0x8266B520..0x8266B7CC` is 100 % unclaimed and its vtable
slot 0 at `0x8266B780` is **unfolded**, so the span is correctly named. It is
still unlandable: `class AssetOffer` exists in **neither oracle** (rb3-Wii's
`AssetOffer.cpp` is a 3-line `DECOMP_FORCEDTOR(AssetOffer, StorePurchaseable)`
stub, and `StorePurchaseable` is absent too), and the retail vtable (21 slots,
only slot 0 overridden) does not disambiguate `: Hmx::Object` from
`: StorePurchaseable`. A pin without a base object scores 0; a pin with an
invented base is a guess. **Declined under "name it or decline it."**

★★ **Unit-level effect of the provider untangle**, which the function counts
understate: `Leaderboard.cpp` went **125 fns / 89 matched / 58.91 % fuzzy →
86 / 73 / 68.67 %**, and `MakeupProvider.cpp` **44 / 26 / 47.86 % → 22 / 17 /
63.21 %**. Two over-broad pins shed 61 foreign functions between them.

---

## 1bis. ★★ The scatter-block filter — rescuing the class-name anchor

Tool: **`scripts/harvest/tu_locate/str_scatter.py`** (committed this wave).

laneBD §6.3 and laneBL §5.3 both saw class-name anchors landing far from their
class and concluded such an anchor is *"an existence proof only"* — i.e. never a
locator, discard them all. **That over-corrects, and here is by how much.**

**Mechanism:** the linker groups the small `?StaticClassName@<Class>@@` COMDATs
of many unrelated classes into shared **scatter blocks**. A site inside one tells
you nothing about where its class lives; a site outside one is real.

| | |
|---|--:|
| classes with a `.?AV` descriptor | 1,127 |
| class-name literal **code** references | 513 (over 297 classes) |
| scatter blocks (≥ 3 distinct classes per 4 KB window) | 37 |
| references inside one — **discard** | 348 (**68 %**) |
| references **ISOLATED** — genuine locators | 165 (**32 %**) |

> ★★ **RULE: a class-name anchor locates a TU only if it is ISOLATED.** If ≥ 3
> distinct classes reference their own names from the same 4 KB window, that
> window is a scatter block and none of those sites locate anything.

The test is **self-contained** — it needs only the class-name reference table, no
RTTI, no pins, no oracle — and it is a **filter, not a veto**: it discards 68 %
and *certifies* the other 165 as usable. That is the correction to laneBL §5.3:
not "class-name anchors are worthless" but "worthless **inside a scatter block**,
and you can tell which in one pass."

The largest blocks are family-shaped, which is why the effect was mistaken for
noise: `0x8236A000` (32 `Char*`), `0x8227A000` (20 `Band*`), `0x8267F000` (16
`*Msg`), `0x8256E000`/`0x8256F000`/`0x82570000`/`0x82571000` (16/15/14/13, the
whole `*Panel` family), `0x826FC000` (15 `FxSend*`), `0x827F7000` (10 `UIList*`).

★ Immediate consequence for this wave: **all four of sub-lane B's panel classes
have exactly one class-name site and all four are scatter-block artifacts**
(`CampaignGoalsLeaderboardChoicePanel` `0x8256E37C`, `MultiSelectListPanel`
`0x8256FD54`, `RetryAudioPanel` `0x825707DC`, `SaveLoadStatusPanel`
`0x82570D54`). For those rows the string channel contributed **nothing**, and any
report implying three channels agreed would be over-counting evidence.

### 2bis. ★ The consolidation, measured — and why it is NOT the sum

The three sub-lanes claimed +25, +72, +39 = **+136**. That number is *not* the
result; it is three separate A/Bs each taken against the same baseline, and
adding them assumes the lanes do not interact. They were merged into one branch
and re-measured:

| | `matched_functions` | strict-100 | by `(unit,name)` | by bare name |
|---|--:|--:|---|---|
| baseline (main `9df262c9`) | 40,302 | 40,087 | — | — |
| consolidated, **without** lane A's 8 map repoints | 40,432 | 40,216 | +160 / −31 | +135 / −6 = **+129** |
| consolidated, **with** them | **40,439** | **40,223** | +167 / −31 | +142 / −6 = **+136** |

So the sum happened to be right — **but only because it was checked.** The
repoints are worth exactly **+7**, isolated by running the A/B both ways
(laneBL §4.1-bis's discipline), and they sit in their own commit so BO-6 can drop
them without touching the 14 new units.

★ **The merge itself needed care.** `splits.txt` was merged as
`merged[unit] = (main[unit] ∪ all lane additions) − all lane removals`. A naive
line-union — which is what the composition tooling does — would have **re-added
the blocks the carves removed** and manufactured overlaps, because a carve is a
*removal* from the donor as well as an addition to the new unit. Zero units were
touched by more than one lane, so no hand-resolution was needed. Post-merge
audit: **942 units / 5,864 `.text` blocks, 0 overlaps, 0 inversions, 0 duplicate
ranges**. `Sections` was drained of its last `.text` block and its whole entry was
deleted in the same edit (the empty-unit trap, laneBL §7ter).

---

## 3. The two-instrument reconciliation (`BandFaceDeform`, `ReviewDisplay`)

laneBL §6.1 deferred this pair: *"their string anchor lands outside the
RTTI-derived span … two instruments disagreeing is signal … reconcile before
pinning either."* It is resolved — and the two rows resolve by **different**
mechanisms, which is the actual finding.

| row | class-name sites | scatter filter | order bracket | verdict |
|---|---|---|---|---|
| `BandFaceDeform` | `0x8227A564`, `0x822C72D8` | **decisive**: `0x8227A564` is in the `0x8227A000` block (20 `Band*` classes) → discard; `0x822C72D8` **isolated** → in the TU | **declines** (`MeshAnim.cpp` spans 5.7 MB and cannot anchor) | the anchor laneBL quoted (`0x822C7298`) was **RIGHT**; their §5.3 `0x8227A528` was the wanderer |
| `ReviewDisplay` | `0x8231E48C`, `0x8231F5F4` | **silent** — *neither* is in a scatter block | **decisive**: bracket lo `0x8231E8EC` (coherent `StarDisplay` anchor) puts `0x8231E48C` below it, inside `StarDisplay`'s pin | `0x8231F5F4` (`Init()`, which *calls* `StaticClassName`) is the locator; `0x8231E48C` (`StaticClassName` itself) is the wanderer |

### 3.1 ★★ Sub-lane C's correction: hypothesis (a) is not a mechanism at all

The reconciliation was framed around three candidate mechanisms. Sub-lane C
collapsed it to two, from the binary, and the argument is worth stating exactly.

Define `foldN(v)` = the number of distinct retail vtables containing slot VA `v`.
* `foldN ≥ 2` — the slot is shared / inherited / ICF-folded. It is not a locator.
  **That single condition is both (a) and (b)** — they are not distinct causes.
* `foldN == 1` — a genuine own override, which **cannot** sit in a base's TU,
  because a base TU cannot define an override of itself.

⇒ **Hypothesis (a) can never coexist with a correctly-computed `owned_slots`.**
And **every endpoint-setting slot on both rows measured `foldN == 1`** — so the
RTTI side was innocent both times, and the disagreement was never RTTI-vs-string.

### 3.2 ★ A fourth mechanism, absent from laneBL's list — and a limit of my filter

`BandFaceDeform`'s `0x822C7298` is **not a class-name anchor at all.** It is
`DeltaArray::AppendDeltas`, and the `"BandFaceDeform"` literal is the
**`const char*` pool tag passed to `MemResizeElem`**, not an `OBJ_CLASSNAME`
`Symbol`. (Corroborated by four function-local statics matching the oracle's
`total`/`maxDelta`/`totalRuns`/`totalLength`, and a loop packing 3 bytes/vertex.)
It sits `0x4D0` **below** the RTTI lo — i.e. plain **head under-coverage**, which
is laneBL §3.1's own mechanism, not one of the three on the §6.1 list.

So laneBL §5.3's row **conflated two anchors of different shape**: an 88-byte
`StaticClassName` COMDAT in a scatter block, and a 584-byte real body carrying
the same literal for an unrelated reason. The instruments never disagreed.

★ **Limit of the scatter filter, worth carrying:** it tests **location, not
semantics.** It correctly certified `0x822C72D8` as *inside the TU* — but the
reference turned out to be a pool tag rather than a class name. **An isolated
class-name reference tells you the site is in the class's TU; it does not tell
you the site is `StaticClassName`.** Confirm the shape (an 88-byte COMDAT) before
reasoning about *what* the site is.

★ And the two rows fail in **opposite directions**: `BandFaceDeform`'s RTTI span
**under-covers at the head**, `ReviewDisplay`'s **over-covers at the head**. A
single "spans under-cover" or "spans over-cover" rule would have been wrong on
one of them.

> ★★ **The reusable rule.** Neither instrument is the tie-breaker in general.
> **Ask first which one is even applicable**, in this order:
> 1. Enumerate *all* class-name code sites — there is often more than one.
> 2. **Scatter filter** (`str_scatter.py`). One survivor ⇒ done. Zero survivors
>    ⇒ this channel *cannot* locate the TU; stop using it. ≥ 2 survivors ⇒ it
>    cannot *separate* them; go to 3.
> 3. **Order bracket** (`td_order.py`), remembering it is ~4:1 and that its
>    narrow regime is its weakest.
> 4. Fall back to the RTTI span with laneBL §3.0's ICF-fold check and §3.5's
>    island test.
> 5. **Record which step fired.** That is what makes the result transferable.

★ Note the asymmetry this exposes in the original framing. "Two instruments
disagree" was read as *one of them is wrong*. In fact, on both rows **both
instruments were partly right**: the class-name literal really is referenced from
the TU *and* from a scatter block, and the RTTI span really does cover the TU
*and* over-reach into a neighbour. The disagreement was never between the
instruments — it was between **two sites of the same instrument**, and the fix is
a filter, not an adjudication.

*(sub-lane C's per-row confirmation from `band.exe` follows)*

---

## 4. Losses, adjudicated by name

**Lanes B and C had ZERO losses, and structurally so** — every one of their pins
is a pure ADD into 100 %-unclaimed space with no donor `.text` edit, so a loss
was impossible by construction, not lucky. That is laneBL §1's finding
reproduced: the unclaimed-run rows are the best yield-per-unit-of-risk in the
worklist and should always be drained first.

**Lane A had 5**, all from the provider carves, and **none is a real-body
regression**. Adjudicated per name against the donor `.s`:

| # | name | VA | size | donor `.s` | mechanism | disposition |
|---|---|---|--:|---|---|---|
| 1 | `??_ELeaderboard@@W3AAPAXI@Z` | `0x8266EE88` | 8 | `Leaderboard.s` | §3.8/3 **adjustor thunk** — mask-identical, unresolvable by automap | that VA is `EyebrowsProvider`'s off=4 slot 0, **unfolded**. Repointed → `??_EEyebrowsProvider@@…`, **TRUE 100 %** |
| 2 | `??_GLeaderboardShortcutProvider@@UAAPAXI@Z` | `0x8266EF60` | 76 | `Leaderboard.s` | §4.2-bis **`??_G` ICF fold** | that class's real slot 0 is `0x8266D460` (**fold 6**), inside Leaderboard's genuine block. Repointed → `??_GEyebrowsProvider@@…`, **TRUE 100 %** |
| 3 | `??_Glength_error@stlpmtx_std@@UAAPAXI@Z` | `0x82670648` | 76 | `MakeupProvider.s` | §4.2-bis **`??_G` ICF fold** onto an STL template | that VA is MakeupProvider's own **unfolded** slot 0. Repointed → `??_GMakeupProvider@@…`, **TRUE 100 %** |
| 4 | `?DataSymbol@MakeupProvider@@UBA?AVSymbol@@H@Z` | `0x8266F730` | 116 | `MakeupProvider.s` | **ICF fold 2** — Makeup's and FaceHair's `DataSymbol` are identical source | the surviving copy sits at FaceHair's TU tail, one function past laneBL's pin. FaceHair **extended to `0x8266F7A8`**, repointed → `?DataSymbol@FaceHairProvider@@…`, **TRUE 100 %**. Makeup's own copy does not physically exist, so the name is correctly retired |
| 5 | `fn_8266FE78` | `0x8266FE78` | 32 | `Leaderboard.s` | **byte-pairing artifact** | a local-static guard thunk (`lis r11, lbl_82E02024@ha`). It **moved** with the carve into `MakeupProvider.s` — not deleted. Its masked signature paired against a funclet in `Leaderboard.obj` and finds no partner in `MakeupProvider.obj` |

★ **Four of the five are corrections that each convert a false 100 % into a true
one — they are −1/+1 renames, not lost program.** Item 5 is the lane's only
genuine loss, and it is a 32-byte funclet-shaped byte-pairing artifact, well
inside laneBL §4.3's threshold ("any loss that is a real body ≥ 100 B is a
genuine regression and blocks that TU"). **No TU was blocked.**

★★ Note the *shape* of item 4: it is the first case in the campaign where a loss
was resolved by **growing a neighbouring, already-landed pin**. laneBL's
`FaceHairProvider` pin was one function short, and the missing function was an
ICF-folded `DataSymbol` shared with the very unit being carved. A fold-2 slot at
a pin boundary should be read as "check whether the pin ends one function early",
not as "this name is wrong."

---

## 5. Honesty gate — size/shape distribution

Tree baseline **re-measured at this wave's baseline** (n=40,095 strict-100 in a
worktree at main `9df262c9`; read for *shape* only, so the exact n differs by 8
from the 40,087 A/B baseline because the worktree was mid-edit):

| | min | p25 | median | mean | p75 | p95 | max | funclet |
|---|--:|--:|--:|--:|--:|--:|--:|--:|
| tree, now | 4 | 32 | **40** | **88.4** | 92 | 288 | 6,900 | **53.1 %** |
| tree, laneBL era (n=39,521) | 4 | 32 | 40 | 88 | 88 | 288 | 6,900 | 53.4 % |

The tree baseline has **not** drifted across laneBL's +561 — median 40 B, mean
88 B, ~53 % funclet, unchanged. laneBL's own gate figures therefore remain the
right comparison, and laneBL achieved median 48 B / 34.0 % funclet against it.

★ One refinement worth carrying: **funclet-shaped and anonymous `fn_<8hex>` are
the same 53.1 %** of the tree's strict set — i.e. every funclet-shaped strict
match is anonymous, and no `__unwind$` / `??__F` *named* symbol scores strict
under its own name. So for this tree the "funclet share" and the "anonymous
share" are one statistic, not two, and a lane reporting both as if they were
independent evidence is double-counting.

Funclet regex:
`^(fn_[0-9a-fA-F]{8}|__unwind\$|__catch\$|\?\?__E|\?\?__F|__unwind__merged_)`

★ Reporting standard inherited from laneBL lane C and applied here: every row is
presented as **"N ported bodies + M byte-paired funclets"**, never as "N bodies";
and the **named vs anonymous `fn_<8hex>` split** is reported alongside the funclet
share, because "0 % funclet-shaped" is not "0 % boilerplate" when the names are
anonymous.

| lane / TU | n | min | median | mean | max | % funclet | named / anon |
|---|--:|--:|--:|--:|--:|--:|---|
| **A** `OutfitProvider` | 7 | 8 | 44 | 58.3 | 116 | 42.9 % | 4 / 3 |
| **A** `EyebrowsProvider` | 5 | 8 | 44 | 71.2 | 188 | 40.0 % | 3 / 2 |
| **A** `CurrentOutfitProvider` | 4 | 8 | 138 | 139.0 | 272 | 0 % | 4 / 0 |
| **A** `InstrumentFinishProvider` | 4 | 8 | 56 | 58.0 | 112 | 0 % | 4 / 0 |
| **A** `MakeupProvider` | 3 | 24 | 32 | 142.7 | 372 | 33.3 % | 2 / 1 |
| **A** `FaceTypeProvider` | 3 | 8 | 44 | 42.7 | 76 | 33.3 % | 2 / 1 |
| **A** `AssetProvider` | 2 | 32 | 32 | 32.0 | 32 | 100 % | 0 / 2 |
| **A** `FaceHairProvider` | 1 | 116 | 116 | 116.0 | 116 | 0 % | 1 / 0 |
| **A** `Leaderboard` | 1 | 44 | 44 | 44.0 | 44 | 100 % | 0 / 1 |
| **A total** | **30** | 8 | **44** | **77.7** | 372 | **33.3 %** | **20 / 10** |
| **C** `BandFaceDeform` | 11 | 32 | 84 | 106.9 | 352 | 36.4 % | 7 / 4 |
| **C** `ReviewDisplay` | 13 | 32 | 40 | 100.0 | 296 | 53.8 % | 6 / 7 |
| **C** `CharMeshCacheMgr` | 8 | 40 | 88 | 162.5 | 620 | 25.0 % | 6 / 2 |
| **C** `TourPropertyCollection` | 7 | 8 | 40 | 56.0 | 136 | 14.3 % | 6 / 1 |
| **C total** | **39** | 8 | **76** | **106.9** | 620 | **35.9 %** | **25 / 14** |
| *tree baseline* | 40,095 | 4 | *40* | *88.4* | 6,900 | *53.1 %* | — |

**Lane A framed honestly: 10 ported bodies + 10 named compiler-generated
dtor/thunk pairs + 10 byte-paired anonymous funclets.** The 10 bodies:
`?UpdateExtendedText@MakeupProvider` 372 B · `?UpdateExtendedText@CurrentOutfitProvider` 272 ·
`?Text@CurrentOutfitProvider` 200 · `?Mat@EyebrowsProvider` 188 ·
`?DataSymbol@FaceHairProvider` 116 · `?Text@OutfitProvider` 116 ·
`?DataSymbol@InstrumentFinishProvider` 112 · `?DataSymbol@OutfitProvider` 80 ·
`?NumData@InstrumentFinishProvider` 36 · `?NumData@MakeupProvider` 24.
★ The 10 named-boilerplate are five 8-byte `??_E…@@W3AAPAXI@Z` adjustor thunks and
five 76-byte `??_G…@@UAAPAXI@Z`, one pair each for Eyebrows / FaceType /
InstrumentFinish / Outfit / CurrentOutfit — **every one read off a retail vtable
slot, never guessed** (laneBL §4.1's "name it or decline it").

**Lane C framed by kind of gain: 12 byte-paired anonymous funclets (from the pins
alone) + 22 already-byte-correct ported bodies that only needed a name to score +
5 from real body fixes.** That middle bucket is worth noticing — it is the
"port → compile → the bytes already agree" effect laneBD saw on `UIProxy` (13
byte-identical on first compile) and laneBL on `UIGridProvider` (17 of 26), here
at 22 of 39.

★ **Both lanes beat the tree on both axes** — median 44 B and 76 B against 40 B,
funclet share 33.3 % and 35.9 % against 53.1 % — but neither is dramatically
body-weighted, and **two rows are labelled weak by their own lanes**:
`ReviewDisplay` (53.8 % funclet, *at* the tree baseline) and lane A's
`AssetProvider` (n=2, both 32-byte anonymous funclets). Reported as such rather
than averaged away.

---

## 5bis. ★★ Two transferable techniques the sub-lanes produced

### 5bis.1 The global vtable-slot FOLD COUNT (sub-lane A)
For every class, every vtable slot, count how many **distinct classes** reference
that slot VA. Then:

> **`foldN == 1` ⇒ that VA physically IS the class's TU. Full stop.**
> **`foldN > 1` ⇒ the slot may live in a sibling's TU and must NEVER set a span
> endpoint or a map name.**

This turns laneBL §3.0 from a *warning* ("check fold status before letting a slot
set an endpoint") into a **decision procedure**. One scan settled all five of lane
A's TUs and caught the `0x8266F730` fold-2 that span heuristics would have
mis-assigned (loss #4 above). Reproduce:
`TU_LOCATE_SCRATCH=… venv/bin/python scripts/harvest/tu_locate/vt_rtti_scan.py`,
then invert `vtables.json` slot → classes.

★ Sub-lane C reached the same instrument from the opposite direction and used it
to **collapse laneBL's hypothesis (a) entirely** (§3.1): `foldN ≥ 2` *is* both (a)
and (b), and `foldN == 1` cannot sit in a base's TU. **Two lanes independently
converging on fold count is the strongest methodological signal in this wave**, and
it should be promoted into the standard pre-flight alongside `size_order_automap`.

### 5bis.2 Local-static DECLARATION ORDER is load-bearing (sub-lane A)
A new addition to laneBL §7's divergence list. Retail's
`static Symbol none("none")` in `CurrentOutfitProvider::Text` initialises **after**
the `DataSymbol` virtual call. Declaring it first measured **worse than using the
global** (73.5 % → 45.9 %); declaring it after took the function to **TRUE 100 %**.

> **Place the local static where the guard-bit cluster actually appears in the
> target listing** — not where the oracle puts it, and not at the top of the
> function.

Companion tell, worth its own line: retail reloading the returned `Symbol` from
its **own stack slot** (`lwz r4, 0x50(r1)` where we emit `lwz r4, 0(r3)`) means
the call result is a **named local, not a temporary**. That single tell closed
`?Text@MakeupProvider@@` and `?UpdateExtendedText@CurrentOutfitProvider@@`.

---

## 6. Claims REFUTED or CORRECTED by this wave

★ Including **two refutations of the lead's own order channel** — recorded first,
because an instrument's author is the worst-placed person to score it.

### 6.0 ★★ The lead's typedesc-order channel — refuted THREE times by its own lanes

★ **The sharpest one first, because it overturned a refutation I had issued.** I
used the channel to refute laneBL's `SetlistSortByLocation` row, claiming the TU
was swallowed between `SongSortByDiff` and `SongSortByRank` at `0x8265D…` on a
degenerate 8-byte bracket with two coherent anchors — on paper the channel's
strongest possible statement. **Sub-lane B re-refuted that from `band.exe` and
was right**: it pinned laneBL's `0x825C4B10..0x825C54D8` and closed the unit at
**24/24 strict-100, 100.00 % fuzzy, zero losses**. Four independent binary
readings back it, the decisive one being that `SetlistSortByLocation`'s and
`LocationCmp`'s own vtable slots are *all* at `0x825C4…` and **zero** fall
anywhere near `0x8265D…`.

**Mechanism of my error: a class family sharing base headers pools its typedescs
in `.rdata` regardless of where its `.text` went.** The whole `SongSort*` family
sits together in `.rdata`, so typedesc adjacency *inside a family* carries no
positional information at all — and a family is exactly where the brackets look
tightest. That is the measured "narrow brackets are less reliable" property
(§1.2) showing up in its purest form.

> ★★ **GATE, contributed by sub-lane B and adopted into the tool: before this
> channel may CONTRADICT a placement, require that the class have its own vtable
> slots inside the bracket.** That test would have made the tool decline instead
> of mislead.

**Two levels of prior judgement were wrong about this row and the byte read was
right both times** — laneBL had demoted it to LOW, and I had refuted it outright.
It landed at 24/24. Recorded prominently rather than quietly dropped.

★ Sub-lane B also diagnosed *why* the string channel scored `str_ratio` 0.07 on
this row, and it is **a reduction defect, not weak signal**: in the Wii oracle the
three decisive symbols are bare `Symbols.h` **globals**, not quoted literals, so
`wii_lits()` never extracted them; retail localises them. **Teaching `wii_lits` to
emit `Symbols*.h` global names as candidate literals would have located this TU
on the first pass** — a concrete, cheap fix to laneBD's instrument.

### 6.0-bis Two further refutations, by sub-lane A
1. **A counter-example inside my own calibration cluster.** My order lists
   `PlayerCampaignCareerLeaderboard` **before** `PlayerCampaignGoalLeaderboard`.
   The binary is the opposite: Goal's unfolded slots are
   `0x8266B8D8`/`0x8266B9B0`/`0x8266BA80`, Career's `0x8266BB40`/`0x8266BC28`/
   `0x8266BD08` — **Goal precedes Career in `.text`.**
2. **My "four TUs swallowed by `Leaderboard.cpp`'s big pin" is wrong for at least
   `LeaderboardShortcutProvider`.** Its own `??_G` is at `0x8266D460` (**fold 6**)
   *inside* Leaderboard's genuine block, and `?Handle@` / `?UpdateIndices@` /
   `??0LeaderboardShortcutProvider@@` already match at 100 % **in
   `Leaderboard.obj`**. It is a class *defined in* `Leaderboard.cpp`, not a
   swallowed TU. ⇒ **A typedesc between two pinned anchors does not imply a
   separate TU — a `.cpp` may define several classes.** That is a failure mode I
   did not document and it belongs in `td_order.py`'s list.
3. **Where it was right, it was right inside its own weakest regime.**
   `CurrentOutfitProvider` = `0x82671AC8..0x826720B8` was confirmed — but on a
   1,520-byte bracket, i.e. the **69.2 %** band. Sub-lane A's framing is the
   correct one and I adopt it: *one datum, not a vindication of narrow brackets.*

### 6.1 `DrumTrackWatcherImpl`'s §6.1 row is STALE
laneBL §6.1 prices it at `0x82780298..0x827808C0` (1,576 B / 4 fns), **98 %
unclaimed**. But laneBL's *own* lane C landed it as a bonus, and committed main
`9df262c9` already carries `.text 0x827800B0..0x82780130` +
`.text 0x82780150..0x827808C0`. The §6.1 table was not updated after the bonus
landed. Confirmed by sub-lane C: the unit is **6/10 strict, 97.37 % fuzzy**, and
its real remaining headroom is **porting 4 near-misses** (99.93 / 99.84 / 94.7 /
91.2 / 84.1 %), **not pinning**. Re-pinning it would have produced exactly the
silent duplicate-range corruption of laneBL §7bis.

### 6.1-bis laneBL §9's `TourQuestGameRules` ADD is REFUTED (sub-lane C)
laneBL §9 lists `0x82365BC0..0x82365CCC` as one of four "clean ADDs with no loss
risk". It is not: that class's own `foldN == 1` slots are at `0x82365D60` /
`0x82365DF0`, **above** the range and inside `FileMergerOrganizer.cpp`'s pin. It
needs a **carve** at ≈ `0x82365CD8..0x82365E3C`. Sub-lane C stopped at
`0x82365C20` and **declined** `0x82365C20..0x82365CCC` rather than guess.

### 6.1-ter laneBL's `InstrumentFinishProvider ≈ 0x8266FAF0` hedge (sub-lane A)
The true lo is **`0x8266FAC4`**. `0x8266FAC8` is that class's unfolded slot 10
(`NumData`) — a 36-byte leaf with **no `.pdata` entry**, which is precisely why
the `.pdata`-based instruments missed it. Matched to TRUE 100 %.
★ General lesson: **a leaf with no `.pdata` entry is invisible to every
`.pdata`-driven instrument in the stack**, including `str_locate.fn_of`. Expect
spans to under-cover by exactly one such leaf at the head.

### 6.2 ★★ `HamScrollSpeedIndicator` is a PHANTOM — the fifth (sub-lane C)
Zero name-string hits, zero RTTI, zero vtables, zero own slots. Its 520-byte pin
at `0x8231EF28..0x8231F130` holds `ReviewDisplay`'s factory `0x8231F048`. This is
the **first phantom found by a lane other than the one that invented the test**,
which is the useful part: laneBL's §2 predictor generalises.
(Running total: `SongDifficultyDisplay`, `FlowEventListener`, `MoveMgr`,
`MoveGraph`, `HamScrollSpeedIndicator`.)

### 6.3 laneBL §5.3 partially refuted — in the useful direction
See §1bis. "Class-name anchors are an existence proof only" over-corrects: **165
of 513 are isolated and do locate.** Independently reproduced by sub-lane C from
`band.exe` (513 / 348 / 165 / 37 figures identical).

### 6.4 ★★ The controller family is NOT unreachable
laneBL §6 measured the five controller TUs (265 of the LOW bucket's 532
functions) as having **0 selective literals and 0 unclaimed space**, and
concluded: *"Do not fund another string/RTTI channel for the controller family."*

**The string/RTTI half of that verdict stands — the "therefore unreachable" half
does not.** The order channel places all five, because it needs neither:

```
ButtonGuitarController   (18 Wii fns) ┐
JoypadGuitarController   (11)         ├ bracket 0x8279BE54..0x8279BE58  (4 bytes)
KeyboardController       (16)         │ anchors: JoypadController .. GuitarController
RealGuitarController     (17)         ┘
JoypadMidiController     (12)   bracket 0x82794730..0x8279B280
                                anchors: SlotChannelMapping .. JoypadController
```
`JoypadController.cpp`'s last block ends `0x8279BE54`; `GuitarController.cpp`'s
first begins `0x8279BE58`. Four TUs bracketed into a 4-byte gap is the channel
saying **"these are swallowed by those two pins"** — and it names the donors,
which is what a carve needs and what the other two channels could not supply.

★ **Independent convergence.** laneBL recorded that `JoypadGuitarController` and
`JoypadMidiController` each have one selective literal, living **in
`TrackWatcherImpl.cpp`** — a datum they had and discounted. The order channel
independently brackets `JoypadMidiController` into `0x82794730..0x8279B280`,
whose largest claim **is `TrackWatcherImpl.cpp`'s block `0x82794730..0x82797808``.

**Honest caveats.** (a) A 4-byte bracket is the narrow regime — 69.2 %, and §6.0
shows that regime failing. (b) The channel orders the four but **cannot separate
them**. (c) `JoypadController.cpp` + `GuitarController.cpp` hold 8,364 B against
62 Wii functions, so expect the true span to reach beyond those two pins.
⇒ **Re-opened, not solved.** laneBL was right that a third *content* channel was
not the way in; the way in is a *layout* channel.

### 6.5 A caveat for BO-6's 316-byte fingerprint (sub-lane C)
BO-6 established 316 B as an **exclusive** fingerprint for `OBJ_SET_TYPE` (91
classes, no other family shares it). Sub-lane C found **zero** 316-byte functions
across all 13 of its ranges — but flagged that `BandFaceDeform::SetType`
(slot `[5]`, `foldN == 1`) is **252 B**. ⇒ **316 B is exclusive but NOT
universal**: it must not be used as a *completeness* check for `SetType`, only as
a positive mispair tell.

### 6.1 `DrumTrackWatcherImpl`'s §6.1 row is STALE
laneBL §6.1 prices it at `0x82780298..0x827808C0` (1,576 B / 4 fns), **98 %
unclaimed**. But laneBL's *own* lane C landed it as a bonus, and committed main
`9df262c9` already carries
`system/beatmatch/DrumTrackWatcherImpl.cpp: .text 0x827800B0..0x82780130` +
`.text 0x82780150..0x827808C0`. The §6.1 table was simply not updated after the
bonus landed. Its remaining headroom is **porting, not pinning**; re-pinning it
would have manufactured exactly the silent duplicate-range corruption laneBL
documents in §7bis.

### 6.2 ★★ The controller family is NOT unreachable — the order channel places it
laneBL §6 measured the five controller TUs (265 of the LOW bucket's 532
functions) as having **0 selective literals and 0 unclaimed space**, and
concluded: *"Do not fund another string/RTTI channel for the controller family."*

**The string/RTTI half of that verdict stands — the "therefore unreachable" half
does not.** The order channel places all five, because it needs neither strings
nor unclaimed space:

```
ButtonGuitarController   (18 Wii fns) ┐
JoypadGuitarController   (11)         ├ bracket 0x8279BE54..0x8279BE58  (4 bytes)
KeyboardController       (16)         │ anchors: JoypadController .. GuitarController
RealGuitarController     (17)         ┘
JoypadMidiController     (12)   bracket 0x82794730..0x8279B280
                                anchors: SlotChannelMapping .. JoypadController
```
`JoypadController.cpp`'s last block ends `0x8279BE54`; `GuitarController.cpp`'s
first begins `0x8279BE58`. Four TUs bracketed into a 4-byte gap is the channel
saying, unambiguously, **"these are swallowed by those two pins"** — and it names
the donors, which is precisely what a carve needs and what the other two channels
could not supply.

★ **Independent convergence.** laneBL recorded that `JoypadGuitarController` and
`JoypadMidiController` each have exactly one selective literal, and that it lives
**in `TrackWatcherImpl.cpp`** — a datum they had and discounted. The order channel
independently brackets `JoypadMidiController` into
`0x82794730..0x8279B280`, whose largest claim **is `TrackWatcherImpl.cpp`'s block
`0x82794730..0x82797808`**. Two channels, arrived at from different directions,
naming the same donor.

**Honest caveats.** (a) A 4-byte bracket is the *narrow* regime — 69.2 %, the
channel's least reliable. (b) The channel orders the four TUs but **cannot
separate them**; they share one bracket, so a carve needs a per-TU discriminator
the order channel does not provide. (c) `JoypadController.cpp` + 
`GuitarController.cpp` hold 8,364 B between them against 62 Wii functions, which
is tight — expect the true span to reach beyond those two pins.
This is therefore a **re-opened**, not a solved, row: laneBL's "do not fund a
third string/RTTI channel" was right, and the way in was never a third *content*
channel but a *layout* one.

---

## 7. What remains — the handoff, priced

Still-unlocated TUs from laneBD's 141-list that are **not yet pinned** and that
the order channel brackets with two coherent anchors. Sorted by bracket strength
(zero-skip first, then narrowest). `uncl` = unclaimed bytes inside the bracket,
i.e. the loss-proof ADD headroom; the rest is carve.

★ **The five DEGENERATE rows are not ADD space — they are carve targets with
named donors.** A bracket of 4 or 8 bytes cannot hold a 16-to-23-function TU;
it means the flanking pins abut and the TU is *inside* one of them. `td_order.py`
now reports these as SWALLOWED and refuses to describe the gap as unclaimed,
because the earlier output ("fully unclaimed; a clean ADD") invited exactly the
wrong action.

| class | Wii fns | verdict | donors / bracket |
|---|--:|---|---|
| `ButtonGuitarController` | 18 | **SWALLOWED** (4 B gap) | `JoypadController.cpp` \| `GuitarController.cpp` |
| `JoypadGuitarController` | 11 | **SWALLOWED** (4 B gap) | ″ |
| `KeyboardController` | 16 | **SWALLOWED** (4 B gap) | ″ |
| `RealGuitarController` | 17 | **SWALLOWED** (4 B gap) | ″ |
| `SetlistSortByLocation` | 23 | **SWALLOWED** (8 B gap) | `SongSortByDiff.cpp` \| `SongSortByRank.cpp` |
| `OutfitProvider` | — | **SWALLOWED** (0 B gap) | `FaceHairProvider.cpp` \| `MakeupProvider.cpp` |
| `InstrumentFinishProvider` | 7 | **SWALLOWED** (0 B gap) | ″ |
| `PremiumAssetProvider` | — | **CONTAINMENT** anomaly | `MakeupProvider` footprint contains `AssetProvider`'s ⇒ one is mis-pinned |

And the rows that do bracket to real space:

| class | Wii fns | bracket | width | claims | uncl | anchors |
|---|--:|---|--:|--:|--:|---|
| `CurrentOutfitProvider` | 10 | `0x82671AC8..0x826720B8` | 1,520 | 1 | 0 | AssetProvider .. NewAssetProvider |
| `Playback` | 10 | `0x82793908..0x82793F8C` | 1,668 | 4 | 856 | BeatMatcher .. SlotChannelMapping |
| `MultiSelectListPanel` | 19 | `0x82626A3C..0x82627118` | 1,756 | 3 | 1,056 | ManageBandPanel .. NewAwardPanel |
| `StandInProvider` | 6 | `0x82673334..0x82673B20` | 2,028 | 1 | 8 | MainHubMessageProvider .. StoreMenuProvider |
| `TourPropertyCollection` | 10 | `0x82365504..0x82365E68` | 2,404 | 6 | 1,664 | FixedSetlist .. TourGameRules |
| `TourQuestGameRules` | 4 | `0x82365504..0x82365E68` | 2,404 | 6 | 1,664 | ″ |
| `Quest` | 21 | `0x82364AA0..0x823663B8` | 6,424 | 16 | 1,676 | TourCondition .. TourPerformerLocal |
| `JoinInvitePanel` | 19 | `0x8263021C..0x82631E00` | 7,140 | 3 | 6,772 | VoiceoverPanel .. SelectDifficultyPanel |
| `RetryAudioPanel` | 21 | `0x8263021C..0x82631E00` | 7,140 | 3 | 6,772 | ″ |
| `SaveLoadStatusPanel` | 17 | `0x8263021C..0x82631E00` | 7,140 | 3 | 6,772 | ″ |
| `GameTimePanel` | 14 | `0x8261F938..0x82624748` | 19,984 | 13 | 920 | EventDialogPanel .. ManageBandPanel |
| `InterstitialPanel` | 32 | `0x8261F938..0x82624748` | 19,984 | 13 | 920 | ″ |
| `AssetOffer` | 1 | `0x82669844..0x8266F380` | 23,356 | 13 | 2,556 | CymbalSelectionProvider .. FaceHairProvider |
| `EyebrowsProvider` | 7 | `0x82669844..0x8266F380` | 23,356 | 13 | 2,556 | ″ |
| `FaceTypeProvider` | 7 | `0x82669844..0x8266F380` | 23,356 | 13 | 2,556 | ″ |
| `JoypadMidiController` | 12 | `0x82794730..0x8279B280` | 27,472 | 3 | 0 | SlotChannelMapping .. JoypadController |
| `BandNetGameData` | 19 | `0x82650B74..0x826556E0` | 19,308 | 16 | 4,288 | Instarank .. PerformanceData [1 skip] |
| `ProfileAssets` | 14 | `0x82650B74..0x826556E0` | 19,308 | 16 | 4,288 | ″ |
| `StoreArtLoaderPanel` | 18 | `0x82638D58..0x8263A718` | 6,592 | 1 | 12 | SongSelectPanel .. StoreMainPanel [2 skips] |
| `ReviewDisplay` | 53 | `0x8231E8EC..0x82321608` | 11,548 | 7 | 3,192 | StarDisplay .. ScrollbarDisplay [2 skips] |
| `CharTransCopy` | 20 | `0x823C3728..0x823C9540` | 24,088 | 33 | 3,488 | CharPosConstraint .. CharMirror [5 skips] |

38 unpinned TUs are bracketed in total; the 11 omitted have both skipped anchors
and > 32 KB width, which is too weak to act on. `AssetStore` is the extreme case
(a 1.87 MB bracket) and should be treated as unbracketed.

★ Reproduce any row with
`venv/bin/python scripts/harvest/tu_locate/td_order.py --class <Class>`; the tool
prints the bracket, every claim inside it, and each claim's **island distance**
(distance to that unit's own nearest other block), which is laneBL §3.5's
mis-attribution test computed for free.

---

## 8. Reproduction

```bash
cd /home/free/code/milohax/rb3-xenon          # main, read-only

# the third channel: calibration, then per-class brackets
venv/bin/python scripts/harvest/tu_locate/td_order.py --score        # 78.2 % (quote THIS)
venv/bin/python scripts/harvest/tu_locate/td_order.py --calibrate    # 89.5 % pairwise (flatters)
venv/bin/python scripts/harvest/tu_locate/td_order.py --class SetlistSortByLocation
venv/bin/python scripts/harvest/tu_locate/td_order.py --neighbourhood ReviewDisplay --radius 8

# laneBL's sharpened string reduction (unchanged, read-only, ~1 min)
cd scripts/harvest/tu_locate
TU_LOCATE_SCRATCH=~/tmp/laneBO2/tu_locate ../../../venv/bin/python str_xref.py
#   -> 20,141 code->string edges, 12,693 distinct strings

# honesty-gate calibration: see docs/plans/tu-pin-wave-2026-07-29.md §10
```

Operating brief every sub-lane followed: `~/tmp/laneBO2/BRIEF.md` (regenerable
scratch). Worktrees `~/tmp/laneBO2/wt-{a,b,c,doc}`.

### Re-run triggers
`td_order.py` reads only `band.exe` and `splits.txt`, so its brackets **tighten
automatically as more TUs get pinned** — every landed pin becomes a new anchor.
Re-run `--score` and the §7 table after any bulk `splits.txt` change; the
handoff table above is a snapshot at main `9df262c9`.
