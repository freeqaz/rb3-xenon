# laneBA — the attribution frontier: census of the dtk auto-carve pool (2026-07-29)

Source of truth: `build/45410914/report.json` (Jul 27 05:36), `config/45410914/splits.txt`,
`config/45410914/scope_map.json` (rebuilt on main by the coordinator on 2026-07-29 —
**not** the stale Jul-25 cache), `scripts/target_symbol_map.json`.

Working files (regenerable, gitignored scratch): `/home/free/tmp/laneBA/`.

## 0. TL;DR

The pool this lane was funded to map is **4.63 MB / 18,883 functions**. After a
scope guard it is **≈0.67 MB / ≈5,561 functions**. Its **honest evidenced ceiling
is ≈+25 to +85 matches** — not thousands.

**Verdict: do NOT fund an attribution lane here. Fund the 11,113-function
source-attributed queue, whose 2,200 near-misses are already pinned and already
name-paired.**

The load-bearing reason is structural and was found this lane: **`auto_03_*`
units have a `target_path` but no `base_path`** — objdiff never attempts pairing
on them at all (`mod.rs:702` → `:1672`; `pair_funclets_by_bytes` is behind
`:771`'s `Some(right)` guard). No identification, naming, or map work can change
that. Only claiming the VA in `splits.txt` can. And **98.3% of the in-scope
> 68 B functions have no reloc-masked byte twin anywhere in our 1,034 compiled
objs** — this is a body-divergence backlog wearing an attribution costume.

Three claims are corrected by measurement here — one of mine, one of the
project's, and one from the immediately-preceding lane:

* **Mine (retracted):** "4,461 already-identified mangled-named functions sit
  unpinned in the pool" — true as a raw count, but only **167** are in scope, and
  laneAL's `165` reproduces. There is no hidden identified population.
* **laneAL's (refuted):** its "+1,900 remaining in the different-unit class" was
  collected the same day it was published, by **laneAM** (+1,602). Today's residue
  is **96 fundable functions ≈ +25**. laneAL's 26%-argmax sample was also refuted
  at source: laneAM's held-out calibration measured whole-gap argmax at **28.0%
  fake attributions**.
* **The project's (corroborated, not discovered):** the `0x82800000–0x82D00000`
  blanket hard-skip window contains **3,702 conf-1.0 pinned functions across 167
  source files, 2,711 already MATCHED** — it is genuinely over-broad. **But
  laneGAPFILL already killed that guard on 2026-07-26** (`dead67a0`). My
  measurement corroborates; it is not new. See §2b.

## 1. The pool, exactly

`build/45410914/report.json` has 3,967 units. 3,071 are `auto_generated`:

| | units | fns | code |
|---|--:|--:|--:|
| `auto_03_<VA>_text` | 2,395 | 18,742 | 4.561 MB |
| other auto (BINK/rdata/pdata/…) | 676 | 141 | 0.065 MB |
| **auto total** | 3,071 | **18,883** | **4.626 MB** |
| source-attributed | 896 | 50,495 (39,382 matched) | 5.954 MB |

The 141 non-`.text` auto functions are **all BINK** (RAD middleware) — out of
scope, ignore them. The pool is the 18,742.

**"0 matched by construction" is structurally confirmed, not inferred.** Auto
units carry *no* `matched_functions` / `matched_code` key at all in their
`measures` — objdiff never diffs them, because there is no base `.obj` to
compare against. They are pure denominator.

Name classes and sizes of the 18,742:

| name class | n | | size band | n | cum % |
|---|--:|---|---|--:|--:|
| anonymous `fn_<hex>` | 13,365 | | ≤ 16 B | 2,208 | 11.8% |
| mangled (`?`/`??`) | 4,461 | | ≤ 32 B | 3,926 | 20.9% |
| `__savegprlr`/`__restgprlr`/misc | 915 | | ≤ 44 B | 6,462 | 34.5% |
| `??__E`/`??__F` | 1 | | ≤ 68 B | 7,908 | 42.2% |

median 88 B, mean 243 B, modal 40 B (×1,870) / 28 B (×828) / 12 B (×728).
**>68 B = 10,834 fns / 4.301 MB** is the raw plausibly-real-code envelope before
any scope guard.

### ★ Retraction of my own headline

I initially flagged the 4,461 mangled-named entries as a large hidden population
of *already-identified but unpinned* functions, apparently contradicting laneAL's
count of 165. **That was wrong, and the error was mine: I had not applied a scope
guard.** With one applied:

| population | mangled-named |
|---|--:|
| all 18,742 | 4,461 |
| in-scope (game/engine neighbourhood, 8 KB guard) | **167** |
| strictest (ORACLE-pure) | 57 |
| outside the legacy `0x828–0x82D` window | 84 |

laneAL's **165 reproduces essentially exactly (167)**. The other ~4,294 sit in
vendor/XDK/thirdparty territory — precisely laneAL's own bucket-C class list
(NUISPEECH, DxMovie/DxMesh/DxRnd, XAUDIO2, CXLrcTransport, Quazal). **There is no
hidden identified population. laneAL stands.**

## 2. Scope: how big is the in-scope residue really?

Scope is the whole ballgame here and it is **radius-sensitive**, so I report the
curve rather than one number. Each auto function is labelled by the source path
of the nearest `pinned:` (conf 1.0) function in `scope_map.json` within radius R:

| radius | game+engine fns | MB | >68 B fns | >68 B MB | no neighbour |
|--:|--:|--:|--:|--:|--:|
| 4 KB | 4,655 | 0.550 | 2,047 | 0.462 | 13,376 |
| **8 KB** | **5,561** | **0.666** | **2,373** | **0.564** | 12,093 |
| 16 KB | 6,595 | 0.814 | 2,856 | 0.695 | 10,378 |
| 64 KB | 9,645 | 1.430 | 4,415 | 1.264 | 5,050 |

8 KB is the defensible choice (no LTCG ⇒ a TU's `.text` stays contiguous; the
`MasterAudio.cpp` cluster of 46 functions packs into 8 KB).

**Headline: the in-scope auto-carve residue is ≈5,561 fns / 0.67 MB, of which
≈2,373 fns / 0.56 MB are >68 B plausibly-real code** — against a nominal pool of
18,883 / 4.63 MB. The scope guard removes ~85% of it.

Bucketed by nearest pinned source path at a 64 KB radius (upper bound, to show
where the mass is):

| bucket | fns | MB | >68 B |
|---|--:|--:|--:|
| ENGINE `src/system` | 8,012 | 1.206 | 3,637 |
| **no pinned fn within 64 KB** | 5,050 | 2.481 | 4,094 |
| network / Quazal (LOW VALUE per directive) | 3,117 | 0.480 | 1,747 |
| GAME `src/band3` (highest priority) | 1,633 | 0.224 | 778 |
| xdk (hard skip) | 629 | 0.119 | 376 |
| thirdparty (zlib/ogg/vorbis/curl/…) | 300 | 0.052 | 201 |

Note the game tier — the project's stated highest priority — is only **1,633
functions / 0.22 MB even at the loosest radius**.

### 2b. The legacy VA window — corroboration, not discovery

I measured that the blanket `0x82800000–0x82D00000` "XDK + Quazal hard-skip"
window contains **3,702 conf-1.0 pinned functions across 167 source files, of
which 2,711 are already objdiff-MATCHED** (`VocalTrack.cpp`, `TrackPanel.cpp`,
`Synth.cpp`, the whole `src/system/ui` cluster, `GemManager.cpp`, `Gem.cpp`,
`TourDescPanel.cpp`). You cannot obtain 2,711 byte-matches from a vendor blob, so
the window is genuinely over-broad — it hard-skips ~1 MB of real code.

**But this was already found and fixed.** laneGAPFILL (`dead67a0`, 2026-07-26)
killed the VA-window guard in `diffunit_gap_funnel.py` and replaced it with a
per-unit source-path test, reporting that 150 of its 235 matches came from inside
the window. My measurement **corroborates** that; it is not new. Do not re-fund
it. The real vendor mass is the sub-bands `0x82840000–0x82A4FFFF` and
`0x82BC0000–0x82C1FFFF` (evidenced vendor, zero pins), not the whole 5 MB span.

## 3. The `unknown` tier, and a genuine scope_map blind spot

Answering the coordinator's sub-question directly:

* **`unknown` is essentially this pool.** 5,317 of scope_map's 6,167 `unknown`
  functions (86%) are auto-carve entries, all `provenance=residual`,
  `confidence=0.0`, 0.801 MB. So "what is the unknown bucket" = "the anonymous
  auto-carve residue". It does not change the in-scope residue materially: it is
  already counted inside the numbers in §2.

* **★ NEW DEFECT — scope_map cannot see 28% of the pool at all.** 5,326 of the
  18,742 auto-carve functions (**2.335 MB — over half the pool's bytes**) have
  **no entry in `scope_map.json` whatsoever**. Cause: `tools/scope_map.py`
  derives addresses with `FN_ADDR_RE = re.compile(r"fn_([0-9A-Fa-f]{8})")`, so
  every auto-carve function that `obj_target_symbol_renamer.py` has renamed to a
  mangled name becomes **address-less and is silently dropped**. The
  not-in-map set is 5,118 `tmap`-resolved + 207 `symbols.txt`-resolved + 1 — i.e.
  exactly the renamed population. Consequence: **the tier table's denominator is
  short by 5,326 fns / 2.3 MB, concentrated entirely in the auto-carve pool**, and
  any scope reasoning about this pool from scope_map alone is blind to half its
  bytes. Cheap fix: fall back to `scripts/target_symbol_map.json`
  (name→VA) when the `fn_` regex misses.

* **★ Tier confidence for this pool is near-zero.** Only **470 of 18,742 (2.5%)**
  carry an *evidenced* tier (conf > 0.5). The rest is 5,326 absent + 5,317
  `unknown` + 7,629 layer-7 **spatial propagation at conf 0.4–0.5**. So the tier
  labels on the auto-carve pool are overwhelmingly a guess by construction — which
  is expected (scope_map derives scope from *pinned* provenance, and these VAs are
  unpinned by definition), but it means **tier percentages must not be quoted as
  evidence for this pool**. That is why §2 uses nearest-pinned-neighbour distance
  with an explicit radius instead.

## 4. Identification coverage available today

Measured, not speculated. Using the legacy VA window as the scope cut (so this
table is directly comparable to laneAL's):

| stage | fns | MB |
|---|--:|--:|
| RAW `auto_03_*` | 18,742 | 4.561 |
| ├ vendor window `0x828–0x82D` | 15,588 | 4.186 |
| └ **in-scope** | **3,154** | **0.375** |
| ─ already NAMED | 86 | 0.010 |
| ─ ANONYMOUS `fn_` | 3,068 | 0.365 |
| ├ ≤ 68 B crumbs (not source work) | 1,760 | 0.058 |
| └ **> 68 B plausibly-real code** | **1,308** | **0.306** |
| ─ with ≥1 referenced string literal | 359 | 0.154 |
| ─ with ≥3 strings (string-anchor ceiling) | 91 | 0.080 |
| ─ **CONFIDENT proposal today (autoid ≥3)** | **18** | **0.016** |
| ─ high confidence (autoid ≥5) | 6 | 0.006 |
| ─ **NOTHING from any tool** | **1,187** | **0.228** |

The 86 in-scope named entries are **not new identity**: 85 of 86 are already in
`target_symbol_map.json`. Naming is what put them there — that join is a
tautology.

### Per-tool contribution

| tool | status | contribution | note |
|---|---|--:|---|
| `fingerprint_match.py` / `autoid.json` | valid, Jul 19, TU5-valid (835/837 = 99.8% size agreement) | **18** | the only tool with any yield |
| `scope_map.json` | fresh Jul 29 | **0 identity** | 2,933/3,154 in-scope are layer-7 `spatial:*` conf 0.40 — not evidence |
| Ghidra TU5 bank `:8002` | running, analysis complete | **0** | 0/30 sampled anon VAs have a real name; all default `FUN_`/`Function_<addr>` |
| `comdat_scatter_scan.py` | current | **0 by construction** | `:198` `continue`s on `metadata.auto_generated` |
| `homing_scan.py` | current | **0** | needs a compiled counterpart obj; this pool has none |
| BinDiff | saturated, do not re-fund | **0 usable** | carrier file is VA-dead, below |

### ★★ NEW: the whole `unified_id*` stack is TU0-keyed and VA-DEAD

`unified_id.json` (11,582 records, May 26): only 201 of its VAs exist in the
current anonymous set and **only 5 agree on size (2.5% ≈ chance)**. Same for
`unified_id_rb3wii.json` (213 present, 5 agree) and `ghidriff_identities.json`
(27 present, 0 sized). These predate the 2026-07-12 TU5 flip, which re-keyed
**only** `target_symbol_map.json`. **Any VA read out of these files today is a
coincidental collision.** BinDiff's recorded 263 IDs live in this file, so its
"saturated" ceiling is worse than recorded — the addresses are unusable without a
re-key.

Two further staleness findings: `tools/ghidra/rb3_symbol_map.full.json` (Jul 18)
holds 38 names (15 in-scope) that `target_symbol_map.json` has since
**retracted** — negative-value evictions, not proposals; and the Ghidra bank was
named *from* that snapshot, so it is a stale mirror, not an independent oracle.
An unresolved 2.2% discrepancy (68 VAs labelled `pinned:` at conf 1.0 by
scope_map yet outside all 5,813 splits ranges) suggests scope_map was generated
against a worktree's newer report; it does not change any conclusion but the
in-scope count may be ~68 high.

**Precision calibration — honest answer: there is none for autoid.** No held-out
measurement exists. The documented `Symbols*.cpp` systematic FP (45% of proposals
historically) is **absent from this pool: 0 of 433 hits**.

### ★ The proposals are crumbs, not TU discovery

All 18 are **singletons** — the largest same-source group at score ≥3 is n=2. So
CLAUDE.md's splits-bootstrap recipe (≥3 corroborating strings **and** a ≥3-function
tight cluster) is **unsatisfiable anywhere in this pool**. And **17 of 18 name a
source file already pinned in `splits.txt`; 15 of 16 distinct files are already
wired and compiled.** Only `ChordShapeGenerator.cpp` is genuinely absent from the
tree. The actionable output is at most one new TU plus ~17 gap-fill entries.

## 5. Can these actually flip? The objdiff pairing gate

### ★★ The decisive structural fact: auto units have no base object

Every one of the 2,395 `default/auto_03_*_text` units in `objdiff.json` has a
`target_path` but **no `base_path`** (measured: *auto units with base_path = 0*;
a real unit like `default/MasterAudio` carries both). That short-circuits the
entire pairing discussion:

* `matching_symbols` (`objdiff-core/src/diff/mod.rs:702`) calls
  `find_symbol(right, …)`; with `right = None` it returns at **`mod.rs:1672`
  (`let obj = obj?;`)** — `None`, always.
* `pair_funclets_by_bytes` is only reached inside **`mod.rs:771`
  `if let (Some(left), Some(right)) = (left, right)`** — never entered.

**All 18,742 auto-carve functions read 0.0% because no pairing is ever
attempted.** No name, no byte signature, and no `target_symbol_map.json` entry
can change that while the VA lives in an auto unit. The only thing that can is
**claiming the VA in `splits.txt` for a unit that has a compiled obj.**

### Necessary and sufficient conditions, from the source

| | anonymous `fn_<8hex>` target | mangled-named target |
|---|---|---|
| `target_symbol_map.json` entry | **neither necessary nor sufficient** (and see the trap below) | **necessary, not sufficient** |
| VA claimed by a `splits.txt` range of a unit with a compiled base obj | **necessary** | **necessary** |
| sufficient condition | base obj holds an unmatched, funclet-like, Code symbol with equal reloc-masked bytes | base obj **defines that exact mangled name** |

`is_funclet_like` (`mod.rs:815`) **gates both sides** — confirmed at the target
loop (`:1419-1432`) and the base loop (`:1434-1447`). Accepted shapes:
`fn_<8hex>`, `__unwind$N`, `__catch$N`, `__unwind__merged_*`, `??__E*`, `??__F*`.
**So an anonymous target can never byte-pair with a mangled base name — the
brief's claim is exactly right.** Pass 1 (`:1470`) is unique-on-both-sides; pass
2 (`:1507`) greedy; pass 2b (`:1533`) many-to-one onto an already-consumed base
funclet; pass 3 (`:1579`) same-size fuzzy at ≥50% masked equality.

### ★ NEW TRAP — map entries on unpinned VAs are NET-NEGATIVE

`reconcile_global_byte_matches` (`mod.rs:1082`) indexes every target obj,
*including auto units*, as retail supply for case-B promotion. Its VA key is
`sym.virtual_address.or_else(|| parse_fn_va(&sym.name))` (`mod.rs:1193`) — and
carved objs have no per-symbol VA, so the VA is recoverable **only from the
`fn_<8hex>` name**. Renaming an unpinned auto-carve VA via
`target_symbol_map.json` therefore **silently deletes it from the case-B retail
index**. Map entries must FOLLOW pins, never precede them.

### The A/B/C test, reproduced

On the 168 in-scope mangled-named auto functions (8 KB guard):

| bucket | meaning | laneAL (165) | today (168) | bytes |
|---|---|--:|--:|--:|
| **A** — defined by exactly one compiled obj | free splits attribution | 0 | **1** | 88 |
| **B** — several objs | ambiguous owner | 2 | **0** | 0 |
| **C** — no compiled obj | needs SOURCE | 163 | **167** | 33,076 |

**Total actionable yield from name-evidence attribution across the entire pool: 1
function, 88 bytes** — `?StaticClassName@PlayerDiffIcon@@SA?AVSymbol@@XZ` @
`0x82324ca0`, a pure end-extension 0 bytes past `PlayerDiffIcon.obj`'s existing
pin (`0x82324820–0x82324ca0`). Re-run against main's objs: identical A=1/B=0/C=167.

*Falsifiability (the test can fail, and does):* positive control — 300 random
100%-matching mangled functions from real units → 299/300 found in the expected
obj (the miss is an `anon_ns`-rewritten name). Differential control —
`?AddRef@DataArray@@QAAXXZ` FOUND vs `?Release@DataArray@@QAAXXZ` (bucket C,
same class) ABSENT; `PlatformMgr.obj` defines 83 PlatformMgr symbols but not
`IsEthernetCableConnected`.

### The byte-twin supply — the real ceiling

Reloc-masked signature of every in-scope anonymous auto function vs every
funclet-like symbol in our 1,034 compiled objs:

| in-scope anonymous class | count | bytes | has exact masked-byte twin in our tree |
|---|--:|--:|--:|
| ≤ 68 B (crumbs) | 3,419 | 120,504 | **2,021 (59.1%) / 76,012 B** |
| **> 68 B (real code)** | 2,635 | 665,928 | **46 (1.7%)** |

**≤ 2,021 functions / ~76 KB is the entire free-crumb ceiling, and it is an upper
bound** — the supply test is tree-wide, whereas objdiff pairs *within a unit*, so
each crumb must land in the specific unit whose obj carries the shape.

**The 1.7% is the sharpest number in this document: 98.3% of the 2,635 in-scope
real-code functions (666 KB) have no byte twin anywhere in our compiled tree.**
laneAL's "a source problem wearing an attribution costume" is now quantitatively
confirmed.

### Q: does crumb collection need the parent TU ported, or merely pinned?

**Merely pinned.** EH-cleanup shapes are generic and pass 2b permits many-to-one,
so one base funclet can absorb unlimited identical target crumbs. Confirmed
empirically: **17 of 276 pinned units score more 100% anonymous target functions
than their base obj has funclet-like symbols in total** (`SaveLoadManager` 330 vs
284; `CustomizePanel` 161 vs 110).

### ★ Contamination hazard found

`build/45410914/obj/` holds **7,013** `auto_03_*_text.obj` files but only 2,395
are live units — **4,618 stale objs** from earlier split configurations, never
cleaned. Globbing that directory inflated the first crumb measurement by 36%
(4,646 → 3,419). **Any obj-derived scan there must intersect with
`objdiff.json`'s `target_path` set.** Add this to the worktree-contamination list
alongside the dirty-obj reflink hazard.

## 6. Unfixable classes and the honest ceiling

### The subtraction

Starting from the nominal pool and removing each class **in a fixed order so
nothing is double-subtracted**:

| step | remaining fns | remaining MB |
|---|--:|--:|
| nominal `auto_03_*_text` pool | 18,742 | 4.561 |
| − out of scope (vendor / xdk / thirdparty / network-Quazal by nearest pin @ 8 KB) | **5,561** | **0.666** |
| − ≤ 68 B boilerplate crumbs (EH funclets, guard clears, adjustor thunks) | ~3,188 | ~0.61 |
| − > 68 B with **no reloc-masked byte twin** in our 1,034 compiled objs (98.3%) | **~46** | ~0.01 |

The dominant term is not boilerplate — it is **body divergence**. Even the crumb
class is capped: of 3,419 in-scope anonymous crumbs, only **2,021 (59.1%) have a
byte twin anywhere in our tree**; the other 1,398 need the parent TU genuinely
ported.

### The honest ceiling

| channel | mechanism | measured ceiling |
|---|---|--:|
| evidenced geometric (laneAM T=1) | claim gap for exclusive-signature owner | **+25** |
| same at T=0 (argmax) | — | +85, at **28% fake** |
| name-evidence attribution (bucket A) | end-extend a pin | **+1** |
| identification (autoid ≥3) | 18 proposals, all > 68 B | **≈0** without body ports |
| free-crumb collection, absolute upper bound | byte-twin supply, tree-wide | **≤ 2,021 / 76 KB** |
| free-crumb, *actually claimable* today | must sit in a claimable gap | **96 evidenced + 548 coin-flip** |
| laneGAPFILL 104 tight in-window gaps | **unmeasured** | "hundreds, not thousands" |

**Ceiling verdict: ~+25 to +85 evidenced today, ~+120 including the free bucket-A
and identification items, and at most ~+600 if you are willing to plant coin-flip
attributions (which the project's own `_splits_fill` doctrine says must be
recorded as UNRESOLVED).** The ≤2,021 crumb figure is a *supply* bound, not an
achievable one — the crumbs must be pinned into the specific unit whose obj
carries the shape, and the gap inventory that could claim them is 96 evidenced +
548 ambiguous, not 2,021.

### On the two claims in the brief

* **"17,771 retail coverage-breadcrumb stubs, 13.7% of carved `.fn` symbols,
  unfixable in source."** Not independently re-derived by this lane (that
  sub-analysis did not return in time). It does **not** change the ceiling above,
  because the ceiling is already set by the byte-twin measurement, which is
  agnostic to *why* a body diverges: a breadcrumb stub is simply one more
  > 68 B function with no twin, already inside the 98.3%. Treat the breadcrumb
  count as **unverified by this lane** and do not add it as a separate subtraction
  — that would double-count.
* **"16,821 EH funclets ≈ 24% of all fns."** Consistent with what was measured
  here: the pool's modal sizes are 40 B (×1,870) and 28 B (×828), 42.2% of the
  pool is ≤ 68 B, and laneAL's `classify_funclets.py` agreement was 4,197/4,197.
  Directionally confirmed; exact fleet count not re-derived.

## 7. Ranked targets

The brief asked for a top-20 by expected yield **in functions that would plausibly
flip**. Honest answer: **there is no top-20 worth a lane.** The whole evidenced
inventory fits in one table, and its total is under +120.

### 7a. The evidenced geometric channel — 96 functions, ≈+25 to +85

laneAM's exclusive-signature residue, grouped by claimant. **All are EH-funclet
crumbs** (60 × 40 B, 20 × 32 B, 10 × 44 B); only 2 exceed 68 B; 3,876 bytes total.

| fns | VA span | B | class | claimant unit (prev / next fence) |
|--:|---|--:|---|---|
| 7 | `0x826039F4-0x826046C8` | 264 | LEFT_ONLY | `AppInlineHelp.cpp` (… / CharServoBone.cpp) |
| 7 | `0x825AB540-0x825AC7DC` | 280 | RIGHT_ONLY | `RockCentral.cpp` (NetGameMsgs.cpp / …) |
| 6 | `0x827AD038-0x827AD1C8` | 240 | LEFT_ONLY | `MemcardMgr_Xbox.cpp` (… / ButtonHolder.cpp) |
| 6 | `0x823ED408-0x823EECF8` | 240 | LEFT_ONLY | `Server.cpp` (… / ContextChecker.cpp) |
| 4 | `0x823F2D78-0x823F2EFC` | 168 | RIGHT_ONLY | `band3/meta_band/PrefabMgr.cpp` (StorePurchaser.cpp / …) |
| 3 | `0x8276A6F4-0x8276A754` | 96 | LEFT_ONLY | `PropSync.cpp` (… / DataFile.cpp) |
| 3 | `0x82641D1C-0x82641DCC` | 96 | RIGHT_ONLY | `UploadErrorMgr.cpp` (FlowSlider.cpp / …) |
| 3 | `0x826315C0-0x82631A14` | 96 | RIGHT_ONLY | `FlowRun.cpp` (FlowValueCase.cpp / …) |
| 3 | `0x825BE870-0x825BEBB8` | 96 | LEFT_ONLY | `band3/meta_band/SongSort.cpp` (… / MusicLibraryStore.cpp) |
| 3 | `0x8230AD08-0x8230B1E4` | 112 | LEFT_ONLY | `OvershellDir.cpp` (… / CharClipSet.cpp) |
| 2×7 | — | 544 | mixed | TrainingPanel, NetGameMsgs, ContextChecker, CharClipDriver, HamCamTransform, FlowNode, Waypoint |
| 1×37 | — | ~1.5 K | mixed | 37 singleton gaps |

**Yield: ≈+25 at laneAM's recommended margin T=1 (10 gaps, 18 evidenced + 7
riders); ≈+85 at T=0 (argmax).** laneAM measured whole-gap argmax at **28.0% fake
attributions** (165/589), so T=0 buys ~+60 extra at the cost of planting fakes.
Conversion at T=1 is ~1.00 strict per claimed function (laneAM predicted 1,605,
measured 1,604 gains / 2 losses).

### 7b. The name-evidence channel — 1 function

`?StaticClassName@PlayerDiffIcon@@SA?AVSymbol@@XZ` @ `0x82324ca0`, 88 B, a pure
end-extension 0 bytes past `PlayerDiffIcon.obj`'s pin. **+1.** That is the entire
bucket-A inventory of the pool.

### 7c. The identification channel — 18 proposals, expected flip ≈0

The 18 autoid string-anchor proposals (§4). Confidence HIGH ×4, MED-HIGH ×2,
MED ×8, LOW-MED ×4. Highest-value rows:

| VA | size | score | proposed source | conf |
|---|--:|--:|---|---|
| `0x826C8470` | 2704 | 26/29 | `rb3/band3/game/GemPlayer.cpp` | HIGH |
| `0x822EADE0` | 980 | 11/17 | `rb3/system/bandobj/GemTrackDir.cpp` | HIGH |
| `0x825BF0C0` | 316 | 8/9 | `rb3/band3/meta_band/Utl.cpp` | HIGH |
| `0x822BB4E8` | 676 | 8/10 | `rb3/band3/meta_band/BandSongMgr.cpp` | HIGH |
| `0x822DD480` | 484 | 6/6 | `rb3/system/bandobj/ChordShapeGenerator.cpp` | MED-HIGH ★only unwired source |
| `0x823028E8` | 3656 | 4/6 | `rb3/system/bandobj/CrowdAudio.cpp` | MED |

**But expected flip ≈ 0**, because all 18 are > 68 B and **98.3% of in-scope
> 68 B functions have no byte twin in our compiled tree** (§5). Identifying them
tells you *which TU to body-port*; it does not make them score. Treat these as
**body-port worklist entries, not attribution targets.** `ChordShapeGenerator.cpp`
is the one genuinely-missing TU and is the single best item in this section.

### 7d. The one genuinely unmeasured pool

laneGAPFILL's **104 tight (≤4 KB) different-unit gaps inside the legacy window,
1,011 functions / 83 KB**. Conversion rate **unmeasured**; laneGAPFILL's own
guidance is "hundreds, not thousands". Pricing it is free — laneAM's static
predictor needs no build. **This is the only item in this document that could
change the verdict, and it should be priced before any lane is funded.**

## 8. Verdict

**Do not fund an attribution lane on the auto-carve pool. Fund the
source-attributed queue instead.**

Side by side:

| | auto-carve pool | source-attributed queue |
|---|--:|--:|
| functions | 18,883 | 11,113 |
| bytes | 4.63 MB | 2.43 MB |
| **in-scope** | **≈5,561 fns / 0.67 MB** | 11,113 / 2.43 MB |
| already name-paired with a base symbol | 0 | **all of them** |
| measured evidenced yield available now | **≈+25 to +85, +1, +0** | 2,200 near-misses at 96–100% / 0.30 MB |
| structural blocker | no `base_path`; needs pin + identity + body | body/codegen only |

The reasons are structural, and each was measured this lane:

1. **Auto units have no base object.** Pairing is never attempted (§5). Every one
   of the 18,742 reads 0.0% for a reason no amount of identification touches.
2. **The cheap geometry is drained.** INTERIOR: 29 in-scope functions in 8 gaps.
   DIFFERENT-UNIT: 3,122 sites but **96 fundable**. laneAL's "+1,900 remaining"
   **does not reproduce** — laneAM collected it (+1,602) the same day laneAL
   published, and laneAL's 26%-argmax sample was refuted at source (laneAM's
   held-out calibration measured whole-gap argmax at 28.0% *fake*).
3. **Name-evidence attribution yields exactly 1 function.** A=1, B=0, C=167.
4. **Identification yields 18 singletons**, 17 of which sit in already-wired TUs.
   The bootstrap recipe (≥3 strings ∧ ≥3-function cluster) is unsatisfiable
   anywhere in this pool.
5. **The residue is a body-divergence backlog, not an attribution problem.**
   98.3% of the 2,635 in-scope > 68 B functions have no reloc-masked byte twin
   anywhere in our 1,034 compiled objs.

By contrast the source-attributed queue's 2,200 near-misses (96–100%, 0.30 MB) are
already pinned, already name-paired, and concentrated (VocalTrackDir 105,
RockCentral 76, Mesh 51, Trans 34, VocalPlayer 34) — every one needs only body or
codegen work, with no identity or attribution risk and no fake-match honesty cost.

**Recommended, in order:**
1. Body-port the near-miss band and the 0% band of the source-attributed queue.
2. **Free, do first:** re-run laneAM's static predictor (no build) over
   laneGAPFILL's 104 tight in-window gaps / 1,011 fns to price the only unmeasured
   pool; and refresh laneAM's NEITHER labels, which are unmeasured since 07-26 and
   may have flipped as laneAT/AV/AW/AY landed body ports.
3. Take the free +1 (`PlayerDiffIcon`) and, if a lane is already touching splits,
   the T=1 subset (+25) — never T=0.
4. Port `ChordShapeGenerator.cpp`, the one genuinely missing TU.

### Tool defects to fix (each cheap, each will otherwise mislead the next reader)

* **`scripts/harvest/autocarve_funnel.py` still derives geometry from
  `report.json` auto-unit boundaries, not `splits.txt`** — the exact trap laneAL
  retracted. Measured inflation today: 8,481,248 B of reported auto span vs
  4,270,260 B truly unowned (**1.99×**), and 814 of 2,395 auto units overlap ≥1
  pinned range. Fix it or retire it in favour of
  `scripts/harvest/diffunit_gap_funnel.py`, which does it correctly.
* **`tools/scope_map.py` silently drops every renamed auto-carve function**
  (address regex is `fn_([0-9A-Fa-f]{8})` only) — 5,326 fns / 2.34 MB invisible.
* **`unified_id*.json` / `ghidriff_identities.json` are TU0-keyed and VA-dead.**
  Delete, re-key, or banner them; today any VA read from them is a coincidence.
* **`build/45410914/obj/` holds 4,618 stale `auto_03_*` objs** (7,013 files vs
  2,395 live units). Any obj-derived scan must intersect with `objdiff.json`'s
  `target_path` set — globbing inflated a measurement by 36% this lane.
* **Never add a `target_symbol_map.json` entry to an unpinned auto-carve VA** — it
  renames the symbol away from `fn_<hex>`, which removes it from
  `reconcile_global_byte_matches`'s case-B retail index (`mod.rs:1193`). Net-negative.

## 9. Reproduction

```bash
cd /home/free/code/milohax/rb3-xenon      # main, read-only

# pool census, VA resolution, scope join, radius sweep, attributability
#   (joins report.json x symbols.txt x target_symbol_map.json x scope_map.json x splits.txt)
#   scratch: /home/free/tmp/laneBA/{auto_recs,auto_scoped8k,inscope_runs,window_runs}.json

# splits.txt-derived gap geometry (authoritative; NOT autocarve_funnel.py)
python3 scripts/harvest/diffunit_gap_funnel.py --worktree $PWD
python3 scripts/harvest/diffunit_gap_funnel.py --worktree $PWD --legacy-window
python3 /home/free/tmp/laneBA-geom/gapcensus.py --json-out /home/free/tmp/laneBA-geom/gaps.json

# the structural gate, in one line
python3 -c "import json;u=json.load(open('/home/free/tmp/wt-laneBA-1/objdiff.json'))['units'];\
a=[x for x in u if 'auto_03' in x['name']];print(len(a),sum(1 for x in a if x.get('base_path')))"
# -> 2395 0

# A/B/C definer test + reloc-masked byte-twin supply (worktree must have built first)
python3 /home/free/tmp/laneBA-pair/coff_syms.py /home/free/tmp/wt-laneBA-1/build/45410914/src syms_wt.json
python3 /home/free/tmp/laneBA-pair/analyze.py
python3 /home/free/tmp/laneBA-pair/sigs.py base /home/free/tmp/wt-laneBA-1/build/45410914/src base_sigs.json
```

Scratch artifacts (regenerable, not committed): `/home/free/tmp/laneBA/`,
`/home/free/tmp/laneBA-geom/` (incl. `residue_exclusive_96.json`),
`/home/free/tmp/laneBA-pair/`, `/home/free/tmp/laneBA-ident/autoid_pool.json`.
Worktree `/home/free/tmp/wt-laneBA-1` (branch `laneBA-1`). Main tree was never
built in, never modified, never stashed.
