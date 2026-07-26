# laneAL — the `auto_03_*` unowned-address pool: funnel and verdict (2026-07-26)

Baseline at lane start: **32,182** strict (`match_percent_normalized == 100.0`),
main HEAD `4aa944e9`.

## What the pool is

`config/45410914/splits.txt` pins per-object `.text` ranges. Any `.text` no unit
claims is auto-carved by dtk into synthetic units named
`default/auto_03_<VA>_text`, which surface in `build/45410914/report.json` with
`metadata.auto_generated = true`. An identification lane's funnel ended by
routing **"11,496 in-scope VAs in `auto_` carve spans with no compiled TU"** to
this lane as the single biggest remaining in-scope bucket.

Reproduced exactly:

| | units | functions |
|---|--:|--:|
| all `auto_03_*` `.text` spans | 3,597 | 27,454 |
| minus the 0x828–0x82C vendor window (XDK + Quazal, owner hard-skip) | 3,260 | **11,514** |

The vendor window alone is 15,940 functions — 58% of the raw pool — confirming
the scope guard was load-bearing.

## The funnel

```
27,454   all auto_03 .text functions
11,514   in-scope (outside 0x82800000–0x82D00000)
   165   have a real mangled name (identity known)
     0   ...attributable to a compiled TU            <-- bucket A empty
11,349   anonymous fn_8XXXXXXX (identity unknown)
     0   ...present in scripts/target_symbol_map.json
```

Both branches terminate at zero. The reasons are independent and both are
structural.

### Branch 1 — the 165 named: attribution is already drained

Cross-referencing every mangled name against a COFF symbol-table parse of all
923 compiled `.obj` files under `build/45410914/src/`:

| bucket | count | meaning |
|---|--:|---|
| A — emitted by exactly one compiled obj | **0** | would be a free splits attribution |
| B — emitted by several objs (shared STL COMDAT) | 2 | ambiguous owner, and both are 400 KB–2 MB from any candidate pin |
| C — emitted by **no** compiled obj | **163** | needs SOURCE, not attribution |

163 of 165 decompose as: 61 STL/`stlpmtx_std` template instantiations whose
template argument is an unported game/UI class (`RCJob`, `ChallengeRow`,
`HamSpecialOffer`, anon-namespace `Label`/`Unlockable`); 23 `??_E`/`??_G`
deleting-destructor vtable thunks; 56 plain member/free functions from classes
absent from the tree (NUISPEECH, `DxMovie`/`DxMesh`/`DxRnd`, `XAUDIO2`,
`CXLrcTransport`) **plus a genuine sub-class of partially-ported classes** —
e.g. `PlatformMgr` is compiled with 30+ methods but `IsEthernetCableConnected`,
`GetName(int)`, `IsInParty`, `IsInPartyWithOthers`, `SetScreenSaver`,
`SetPadContext/Presence/Property` are declared in `PlatformMgr.h` with no
compiled definition; 4 `StaticClassName<T>()`; 2 `MakeString<...>`; 1 `lbl_`
mid-function label that is not a symbol at all.

This independently corroborates the splits-move lane's own drain state
(`docs/plans/splits-move-lane-2026-07-26.md` §6), which ended with **208
UNPORTED** residual records — "VAs whose mangled name no obj of ours defines…
they are not a splits problem and no splits work can move them."

### Branch 2 — the 11,349 anonymous: attribution *cannot* score them

The premise this lane was funded on is that pinning an unowned span to a real TU
lets its functions pair. **That premise is false for anonymous functions**, and
the reason is in the objdiff fork itself.

`objdiff-core/src/diff/mod.rs` pairs symbols in `matching_symbols` almost
entirely **by name** (`symbol_name_matches`: exact `name` or `normalized_name`
equality, plus matching `SectionKind`). There is **no generic ordinal /
positional / section-offset fallback** for Code symbols. The only non-name
mechanisms are narrow and gated:

* `pair_funclets_by_bytes` (mod.rs:1410-1627) — a masked-byte-signature
  fallback restricted by `is_funclet_like()` to names matching `fn_<8hex>`,
  `__unwind$NNN`, `__catch$NNN`, `__unwind__merged_<addr>`, `??__E…`, `??__F…`.
  Both sides must be `SectionKind::Code` and the reloc-masked signature must be
  byte-identical **and unique on both sides**.
* `reconcile_global_byte_matches` — explicitly **requires a real, non-anonymous
  mangled name** (`is_anonymous_or_funclet` gate, "Rule 3"), size > 44 bytes,
  oracle attribution and signature uniqueness. It never touches anonymous
  `fn_` items.
* `CompilerGenerated` literal symbols (`@251`-style) and manual
  `MappingConfig.mappings`. Neither applies.

> **Correction to `scripts/harvest/splits_move.py`'s docstring.** It states
> "objdiff pairs anonymous (`fn_8XXXXXXX`) target functions against our compiled
> COMDATs *positionally*". There is no positional pairing. The real mechanism is
> the uniqueness-gated funclet **byte-signature** fallback above. The tool's
> fake-match hazard is therefore real but *confined to funclet-shaped symbols*,
> not general. Worth fixing in the docstring before someone re-derives a risk
> model from it.

Verified empirically on `default/RockCentral`, the unit with the most
100%-scoring anonymous entries (538): the target obj carries 805 `fn_<addr>`
symbols, our base obj carries 705 `__unwind$NNN` + 4 `__catch$NNN` + 40 `??__F`
+ 2 `??__E`. None of the sampled scoring addresses appear in
`target_symbol_map.json`. **All 538 scoring entries are 12–68 bytes** (mean ~37,
dominated by 40 B ×307 and 32 B ×222 — MSVC PPC EH-cleanup funclet shapes). The
137 `fn_` entries that did *not* reach 100% run up to 1,736 bytes (mean ~147) —
the real functions, unmatched precisely because they have no identity.

## What the 11,349 actually are

The pool's own size distribution says the same thing:

| | count | share |
|---|--:|--:|
| ≤ 16 B | 1,313 | 11.6% |
| ≤ 32 B | 3,867 | 34.1% |
| ≤ 44 B | 7,625 | 67.2% |
| **≤ 68 B** | **8,459** | **74.5%** |
| ≤ 128 B | 9,844 | 86.7% |
| ≤ 512 B | 11,127 | 98.0% |

median 40 B, mean 87 B, total 989 KB. The modal sizes are **40 B ×2,808** and
**32 B ×2,346** — the exact EH-cleanup funclet shapes measured in RockCentral.

So the headline "11,496 in-scope VAs" is **not 11,496 decompilable functions**.

This was verified adversarially, not assumed: a **full census of all 11,349**
entries, disassembled from `orig/45410914/band.exe` with `llvm-objdump
--triple=powerpc-unknown-unknown` and cross-referenced against the binary's own
`.pdata` `RUNTIME_FUNCTION` table (57,733 entries at file offset `0x1F1600`).

| class | count | shape |
|---|--:|---|
| EH cleanup funclet | 4,197 | `addi rN,r12,-K / mflr r12 / … / bl <dtor> / blr` — parent frame via r12 |
| static-init guard clear | 2,287 | `stwu / lis+lwz <guard> / rlwinm <clear 1 bit> / lis+stw / blr` |
| this-adjustor thunk | 730 | `lwz r,-4(this) / sub / b <impl>` — tail-branch, no `blr` |
| scalar-deleting-dtor wrapper | 57 | `bl <real dtor> / mr 3,31 / blr` |
| trivial accessor | 356 | `lwz/lfs reg,off(r3) / blr` — real but one-line |
| tiny other (≤5 insn, no `bl`) | 354 | mostly real |
| other (dominated by the >68 B tail) | 3,368 | |

Confirmed boilerplate is **7,271 / 11,349 = 64.1%** on a conservative
classification, and **7,252 / 8,459 = 85.7%** of the ≤68 B bucket — a floor,
since hand-inspection showed residual 68 B "other" entries (`0x822702A8`,
`0x822716F8`) are deleting-destructor bodies of the same synthesized family,
pushing the ≤68 B share past **90%**.

The `.pdata` cross-reference is stronger evidence than shape-reading: **9,488 of
11,349 (83.6%) carry their own `RUNTIME_FUNCTION` record, and `FuncLen` matches
the report.json size exactly in all 9,488 — zero mismatches.** These split points
are the compiler/linker's own function table, not a dtk auto-carve artifact.
The 1,861 without a `.pdata` record are frame-less leaf code MSVC needs no
unwind info for — near-uniformly tiny thunks and one-line accessors.
Independently corroborated by the pre-existing `scripts/grind/classify_funclets.py`,
whose `subi r31,r12,imm`+`mflr` signature agrees on **4,197/4,197** of the EH
funclets (31,066 fleet-wide, modal size 0x28 = 40 B ×21,302).

Those crumbs pair **for free, by byte signature, once their parent TU is ported
and pinned** — a *derived* yield of TU porting, never an independent attribution
lever.

That leaves ≈**2,890** anonymous entries above 68 bytes (~693 KB) as the only
part of the pool that is plausibly real code — and it **is** real: 2,871/2,890
(99.3%) carry no boilerplate signature, and 10 hand-disassembled samples
(`0x823507B8`, `0x825AB930`, `0x826574E8`, `0x826BDD50`, `0x826D0BB8`,
`0x823486B8`, `0x823662C8`, `0x823F2898`, `0x8242E580`, `0x8257EFA8`) all show
genuine control flow: `bdnz`/`cmplw` loops, `mtctr`/`bctrl` vtable dispatch,
`__EH_prolog`-guarded try regions. Only 19 (0.7%) are large array-destructor
funclets. Every one of these 2,890 needs (a) an identity in
`target_symbol_map.json` — a different lane's single-owner file — and (b) source
that compiles to the right bytes.

## Geometry, and the "cheap attribution" mirage

The 3,260 in-scope spans merge into **1,718 contiguous runs** totalling 1.61 MB.
Only **45 runs are ≥ 8 KB**, holding 4,501 functions (40% of the pool); the
remaining 1,673 runs are small interleaved fragments. Largest runs by function
count: `0x822FC4F8` (44,712 B / 361 fns), `0x823EC4A0` (25,336 / 219),
`0x823F27B8` (21,840 / 200), `0x823215E8` (22,864 / 191), `0x82307CF0`
(19,604 / 177).

Identifying the 15 largest runs (fingerprint heuristics + splits.txt interval
overlap + map coverage) shows the "one owning TU per big run" premise is
**false**. Each run is a dense byte-level **interleave of 5–24 already-pinned,
already-wired, already-in-tree TUs**, with the unclaimed functions sitting in the
small gaps *between* those pins — the COMDAT-scatter pattern of
`project_comdat_scatter_lever_2026-07-19`. Map-name coverage inside these runs is
0–11%; fingerprint hit rates are 0–14 matches per 100–450 functions, mostly at
score 1–2. The *spatial* signal (which classes are pinned immediately around a
gap) is far stronger than the identification tooling.

**That spatial reading invites the conclusion "≈60–70% is a cheap attribution
problem." Our own measurements refuse it, and the refusal is the main result of
this lane.** If those gap functions really were methods of the adjacent,
already-compiled neighbours, then:

* byte-identity homing would find them — it found **4** actionable VAs
  tree-wide, with **58.8% NOMATCH** (our compiled bytes match no retail `.pdata`
  entry of that length at all); and
* the 165 already-named entries would have single compiled definers — bucket A
  measured **0**, with **163/165 defined by no obj of ours**.

Both say the same thing: the gap functions belong to those neighbouring TUs *in
the retail binary*, but **our ported source does not contain correct bodies for
them**. The pool is a **source problem wearing an attribution costume**. Site
count is not defect count; adjacency is blast radius, not yield.

The genuinely cheap items the run survey did surface are few and specific:
`src/system/os/PlatformMgr_Xbox.cpp` (full body in-tree, still unwired — 8 map
names in run `0x8251BB58`; note it did **not** compile in this lane's wiring wave,
so it needs a real fix, not just an objects.json line), `MessageBrokerDDL_Xbox.cpp`
(proven by a literal `.\MessageBrokerDDL_Xbox.cpp` MILO_FAIL path string in 4
functions, but only `MessageBrokerDDL_Wii.h` exists in-tree), and oracle ports for
`MicInputArrow`, `PlayerDiffIcon` (currently a 1-line stub inside `Band.cpp`) and
`NetComponentPostScrollMsg`/`UISyncNetMsgs`. `HamStorePanel` and
`AudioDuckerGroup` exist only in DC3 and need care per the DC3-is-newer caveat.

## What the lane actually landed

Six subagents; three fixers in their own worktrees. Measured against this lane's
own baseline pickle (`/home/free/tmp/laneAL_base_strict.pkl`, 32,182 — verified
byte-identical to each fixer's independent baseline).

| lane | change | measured alone |
|---|---|--:|
| fixC — **tree-wide interior-hole sweep** | 1,084 same-unit gaps + 50 sub-gaps + 8 diff-unit | **+2,271** |
| fixB — wire 101 unwired `src/system` TUs + 17 reveal-pins | objects.json + splits | +0 direct, **+17** reveal-pin, **+0** homing re-arm |
| fixA — full-tree byte-identity homing | 4 ADD-only pins | +0 (**+6 pending map**) |

**Consolidated on this lane's branch, rebuilt and re-measured twice from the
same baseline: 32,182 → 34,471 = +2,289 strict, 2 losses** (`DataFile`
`fn_8276ACA4`/`fn_8276ACCC`, a pairing shuffle inside a unit that is net +3).
Stable across repeated clean rebuilds. Top units: OvershellSlot 103,
NetSession 95, VocalTrackDir 87, RockCentral 78, SongData 74,
PassiveMessenger 56, MainHubPanel 55, DataFunc 52, Performer 48, MetaPanel 41.

### ★ The correction that matters: the interior-hole class is 27× bigger than this lane first measured

This document's first revision reported a **153-function ceiling** for the
MIDDLE class and concluded "do not fund another attribution lane". **That was
wrong, and the error was in the enumeration, not the reasoning.** Deriving
candidate gaps from `report.json`'s `auto_03_*` *unit boundaries* found only 44
gaps / 153 functions, because dtk had already coalesced most unowned regions
into larger auto units whose ends do not coincide with a pin.

Recomputing the gap set **directly from `splits.txt`** — every maximal address
interval no unit claims — gives the real picture:

| | gaps | functions |
|---|--:|--:|
| in-scope unowned gaps | 2,078 | 11,409 |
| of which **interior** (same unit pinned on *both* sides) | **1,084** | **4,128** |
| remaining different-unit gaps | 994 | 7,281 |

Extending each interior hole into its enclosing unit yielded **+2,212 from 1,084
gaps with 2 losses — a 53.6% hit rate**, against the 12.8% the
"87.2%-genuine-scatter" calibration predicted.

**Why interior holes are a different animal from generic splits holes:** retail
packs a TU's code contiguously, so a span fenced on *both* sides by unit U's own
territory is overwhelmingly U's own unpinned code. That contiguity argument does
not exist for a gap between two *different* units — which is exactly where the
87.2%-scatter figure came from. Conflating the two classes is what produced both
the pessimistic calibration and this lane's first wrong ceiling.

**Generalisation worth carrying forward: derive geometric candidate sets from
`splits.txt` itself, never from `report.json` auto-unit boundaries** — the latter
is a coalesced view that silently hides most of the candidates.

### Refusals

**The refusal criteria fired 0 times across all 1,142 applied gaps.** Criterion
(1) *could not* fire: binary-wide, of the 11,409 functions in in-scope unowned
gaps only **163** carry a `target_symbol_map` entry and only **2** of those are
defined by any compiled obj of ours — the name-paired-evidence channel for this
class is fully drained, exactly as Branch 1 above measured. The 11 refusals in
the different-unit batch are a *different* ground: they gained 0 in **both**
directions, so attributing them would have been evidence-free. Those are the
genuine COMDAT scatter.

**All +35 came from the MIDDLE-hole class — the only attribution shape that
paid.** 44 in-scope gaps start exactly at one pinned unit's `.text` end and end
exactly at another's start; 30 have the same unit on both sides. Re-derived at
*function* granularity (per the splits-move lane's "kill clusters, keep
functions" lesson) this gave 50 sub-gaps, and the refusal screen fired on **none
of the 50** — after earlier lanes' fills the residue in these spans is
essentially all anonymous `fn_` code. This beat the 87.2%-genuine-scatter
calibration, so **interior same-unit holes are a better class than splits holes
generally.** It is also now exhausted: 153 carved functions was the whole
ceiling.

### Two negative results worth as much as the positive

**1. Byte-identity homing is drained.** A full-tree sweep (923 TUs, 173,771
function records ≥32 B) funnels: 1,151 UNIQUE records → 104 distinct VAs → 62
uncovered → 54 in-scope → **4** applied (50 refused as genuine ICF collisions —
one 132-byte VA is claimed by **21** different `ObjOwnerPtr<T>::SetOwnerObj`
instantiations; naming it is a coin flip). Composition of the 173,771:
**58.8% NOMATCH** (our bytes match no retail `.pdata` entry of that length —
body divergence, which homing cannot see through by construction), 23.6%
ALL-MAPPED, 14.4% MULTI + 2.5% UNIQUE-ICF (the ICF ceiling). That is ~0.03%
yield against the 11,349 target pool, versus +1,107 on the prior sweep. Worth
~4–6 matches per re-run and only after objs move substantially; **no longer a
campaign-scale lever.** The 4 pins are **inert without map entries** (a pinned
range with no `target_symbol_map` name leaves the target symbol `fn_8XXXXXXX`,
unpairable, 0%) — hand-off to the map owner:
`/home/free/tmp/laneAL_fixA_map_handoff.json` (11 names, 6 of which flip).

**2. Wiring unwired TUs no longer re-arms homing — but *reveal-pin* still
pays.** 139 `.cpp` existed under `src/` outside objects.json; excluding the 9
hard-skipped `src/xdk/` files, 99 of 128 compile cleanly and emit non-empty objs.
Direct yield **+0** — as the precedent predicted, every map-known symbol they
define is a shared COMDAT already emitted elsewhere. The homing sweep over the
*enlarged* obj set returned **the identical 4 blocks** as the sweep without them:
**the +101-from-re-arm precedent did NOT reproduce**, that channel having been
consumed by the intervening waves.

The secondary yield came from a *different* mechanism than expected — **reveal-
pin**: of the symbols the 99 new TUs define, 8 are (a) already NAMED in
`target_symbol_map.json`, (b) defined by no other compiled obj, and (c) at retail
VAs no existing `.text` range covers. Pinning those single-function spans gave
**+3** (`MidiChannel::MidiChannel`, `MemcardAction::MemcardAction`,
`MidiReceiver::MidiReceiver` at 100%; `SampleInst360::SetADSR` 99.90%,
`SongDifficultyDisplay::Save` 99.88%, `HttpReq::HasSucceeded` 99.80%,
`UILabelDir::Save` 72.59%, `DrawUtl::CreateCameraBufferMat` 2.61%). **Generalisation:
after wiring new TUs, check map∩new-obj∩unpinned-VA — not just homing.**

**★ New trap found while wiring: objdiff unit names are keyed by BASENAME**
(`default/<stem>`), so adding `rnddx9/Cam.cpp` clobbers `rndobj/Cam.cpp`'s unit
and zeroes it — **measured −613 strict** before those 16 files were excluded. Any
future wiring wave must check basename collisions first. A further 13 files fail
to compile for real reasons (missing DC3 game headers, Fader/SongPos API
divergence, x86/Win32-only soundtouch files); `src/system/os/PlatformMgr_Xbox.cpp`
is among the non-wired, so the "cheap one-line objects.json fix" reading of it is
wrong — it needs real work.

## Verdict

**The 11,496 counts `.fn` entries, not functions — but it IS a workable pool,
via geometry rather than identity.** It decomposes into:

* ~8,459 (74%) EH/init boilerplate — and these turn out to be **collectable in
  bulk by claiming the address**, not only as a side-effect of porting the
  owning TU. This lane collected 2,212 of them from interior holes alone.
* 163 named — source work, already correctly routed away by the splits-move
  lane as UNPORTED.
* ~2,890 anonymous > 68 B — blocked on identity first (map lane), source second.
* **0 spans attributable by *name* evidence; 1,084 attributable by *contiguity*
  evidence**, which is the distinction this lane got wrong the first time.

**How much of the 11,496 is genuinely reachable? Far more than the name-evidence
view predicted — this lane converted 2,289 of it.** The two framings must be
held together:

* **By name evidence: ~0.** Only 163 of 11,409 gap functions are map-named and
  only 2 are defined by our objs. Branch 1 and Branch 2 above stand: attribution
  *justified by a symbol name* is drained, and objdiff genuinely cannot pair an
  anonymous function by position.
* **By contiguity evidence: ~4,128 (the interior-hole subset), of which 2,212
  landed.** These score through `pair_funclets_by_bytes` — the uniqueness-gated
  byte-signature path — which is precisely the mechanism Branch 2 identified. It
  needs no name. The gains are all anonymous `fn_` reveals, 32–76 bytes (median
  40), 87 KB of newly matched code: **the funclet/thunk crumb class collected in
  bulk, exactly as predicted, once the owning TU claims the address.**

So the composition analysis was right (74% boilerplate) and its *implication*
was wrong. The crumbs are not only a by-product of porting a TU — **claiming the
address alone is enough**, because our already-compiled objs already contain
those funclets; they were merely unpaired for want of an owner.

**Remaining ceiling, honestly:**

* **Interior holes: exhausted.** 4,128 available, 2,212 taken. The unclaimed
  1,916 are functions whose bodies we do not compile correctly — a body-port
  problem, not a splits problem.
* **994 different-unit gaps / 7,281 functions: NOT swept, deliberately.** A
  19-gap sample hit 23/89 = **26%** by argmax, extrapolating to roughly **+1,900**.
  Unlike interior holes there is no contiguity argument, so an argmax fill on a
  32-byte ICF-prone accessor can plant a genuinely *fake* match in the wrong
  unit. **This is the single largest identified remaining lever in this pool and
  it deserves a deliberate decision, not a side effect.** Recommend a dedicated
  lane with a stricter-than-argmax rule (require a margin, not a tie-break) —
  and note the honesty cost is real: fake matches raise the count while lowering
  truth.
* **~2,890 real functions > 68 B** still need identity then source, as before.

**Revised recommendation: the earlier "do not fund another attribution lane"
verdict is withdrawn.** It was derived from a 27×-undercounted candidate set.
Byte-identity homing (+0/4 pins) and obj-supply expansion (+0 direct) *are*
drained; **geometric attribution is not**, and has ~+1,900 of measured-rate
headroom left in the different-unit class.

**What needs source rather than attribution:** the 163 unported named symbols
(61 STL instantiations awaiting their template-argument class, 23 deleting-dtor
thunks, 56 plain functions incl. genuinely unimplemented `PlatformMgr` methods,
4 `StaticClassName<T>`, 2 `MakeString`), plus the ~2,890 real anonymous
functions once the map lane can name them. Named oracle targets surfaced by the
big-run survey: `MessageBrokerDDL_Xbox.cpp` (proven by a literal MILO_FAIL path
string; only `MessageBrokerDDL_Wii.h` is in-tree), `MicInputArrow`,
`PlayerDiffIcon` (a 1-line stub inside `Band.cpp`), `NetComponentPostScrollMsg`/
`UISyncNetMsgs` — all present in the rb3-Wii oracle. `HamStorePanel` and
`AudioDuckerGroup` exist only in DC3 and need care per the DC3-is-newer caveat.

Re-measure any time with `scripts/harvest/autocarve_funnel.py --worktree <wt>`.
