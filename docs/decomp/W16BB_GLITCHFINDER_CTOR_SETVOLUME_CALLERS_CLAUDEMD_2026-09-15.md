# W16-BB — the `GlitchFinder` ctor that was never GlitchFinder, the `SetVolume` caller adjudication, and a stale CLAUDE.md paragraph

**Lane:** W16-BB (Opus) · **branch** `w16-bb`, based on main `9f7c571a0204` · **worktree** `/home/free/tmp/wt-w16-bb`
**Date:** 2026-09-15 · **objdiff** 4.2.9 `5a51cd51fe0a353f` · **ruler** `name_check` (read from `report.json`'s `provenance.diff_config`, not assumed)

## Lane measures

| | `matched_functions` | `matched_code` | `matched_code_percent` |
|---|---:|---:|---:|
| before (main `8ecff1e77fdf`) | 43,489 | 4,033,120 B | 39.362858 |
| after (`w16-bb`) | **43,490** | **4,033,384 B** | **39.365433** |
| Δ | **+1** | **+264 B** | +0.002575 |

Priced by set-diff of the `fuzzy == 100` row set (`tools/rowset_snapshot.py diff`), not by the net gap:

```
CROSSED IN : 1 rows, 264 B
   +    264 B  default/Cheats::?CheatsInit@@YAXXZ
FELL OUT   : 0 rows, 0 B
NET bytes  : +264
```

Zero offsetting losses inside the net. `total_code` 10,246,004 and `total_functions` 69,217 unchanged.

⚠ One measured detail worth recording rather than rounding away: aggregate `fuzzy_match_percent` moved
**−0.000038 pp** (49.665512 → 49.665474) while `matched_code` *rose*. That is the expected signature of
naming an anonymous address under `name_check` — forgiven placeholder call sites become checked ones, so
rows that never reach 100 lose a hair of partial credit while `matched_code` (all-or-nothing at
`fuzzy == 100`) is untouched. It is the measured *cost* of the naming bet in item 2, and exactly why
MAPID-1's rule is that naming pays in **bug exposure, not bytes**. I did not separately attribute it to a
single commit; it is stated as an observation with its candidate mechanism, not as an attributed finding.

---

## Item 1 — `??0GlitchFinder@@QAA@XZ`: **the briefed premise was false**, and the real defect was in the map

**Briefed:** a 340 B row at fuzzy 0, the largest unmatched named row in `default/StringTable`; diff our
`GlitchFinder` ctor against retail and fix `src/system/utl/GlitchFinder.cpp/.h`.

**Measured:** there is no GlitchFinder ctor in retail to match. `glitch_find` and `GlitchFinder` occur
**zero** times in retail `band.exe` (checked with `command grep -a`, because the agent shell's `grep` is a
shim through `ugrep -I` and is binary-blind — it yields false negatives shaped like decisive ones). The
profiler is **compiled out of retail entirely**, so `??0GlitchFinder@@QAA@XZ` can have no retail address,
and no amount of work in `GlitchFinder.cpp` could ever have moved that row. **The defect was in
`scripts/target_symbol_map.json`**, which is the house rule firing exactly as written: *if retail's
diverging operands coherently describe a different function, the defect is in the map, not the source.*

`0x827C2780` is **`??0CheatsManager@@QAA@XZ`**, established five independent ways:

1. **Vtable.** A vtable at `0x82118394` is stored into the object; its first slot `0x827C2FC8` is *already*
   mapped as `??_GCheatsManager@@UAAPAXI@Z`. A ctor that installs CheatsManager's vtable is CheatsManager's ctor.
2. **String.** The body loads the literal `"cheats_buffer"` and passes it to `Symbol::Symbol(const char*)`
   then `DataArray::FindData`. `CheatsManager` is the only class with a `cheats_buffer` config key.
3. **Member-for-member offset agreement** against `src/system/utl/Cheats.h`'s annotated layout.
4. **Two EH vector-ctor-iterator calls** with element size `0xc` and count `2` — exactly
   `mQuickJoyCheats[2]` and `mJoyCheatPtrsMode[2]`.
5. **Sole caller.** `?CheatsInit@@YAXXZ` allocates with `li r3, 0xc0` — `sizeof(CheatsManager)`.

### What shipped

`9ad1a5a8` — map row `"0x827c2780"`: `??0GlitchFinder@@QAA@XZ` → `??0CheatsManager@@QAA@XZ`.
**Predicted +1 fn / +264 B; measured +1 fn / +264 B, exactly**, via `?CheatsInit@@YAXXZ`. This is MAPDEF-3's
asymmetry: *repairing a wrong existing name pays* (the caller's charged site closes), whereas *adding a new
name to a previously-anonymous address has zero byte upside*. The 264 B is CheatsInit's body crossing, not
the ctor's — the ctor row itself is still unpairable, see below.

`9d78af2d` — `src/system/utl/Cheats.cpp`: moved a `SetName("cheats_mgr", ObjectDir::Main())` inside the
existing `#ifdef HX_NATIVE` block. Our body was 364 B against retail's 340 B, and those six instructions
were **exactly** the 24 B surplus. This is the standing DC3-is-newer trap: DC3-era additions belong behind
`HX_NATIVE`. Corroborated by the rb3-Wii DEV oracle's ctor, which has no `SetName` either, and by retail's
body running straight from `FindData("cheats_buffer")` to the epilogue. **Δ0 as predicted** (the row cannot
pair — see below — so byte-exactness cannot yet be collected), and landed on merit per *accuracy beats
headline %*. Our body is now **85/85 instructions relocation-normalized identical to retail at 340 B**.

### Why the row still reads 0, and the proposal that fixes it

`StringTable.cpp` scatter-`#include`s `GlitchFinder.cpp`, `Locale.cpp`, `TimeConversion.cpp` and
`movie/Movie.cpp` — but **not** `Cheats.cpp`. So `StringTable.obj` cannot define
`??0CheatsManager@@QAA@XZ`, and objdiff pairs target↔base **by name**: the row is **structurally
unpairable** and reads 0 however correct our source is. `Cheats.obj` *does* define it (COFF symbol 8882,
section 2181, value `0x8`).

Filed as **`docs/decomp/W16BB_map_proposals.json` item BB-1** (commit `439ab786`): merge Cheats.cpp's
`.text 0x827C1A84–0x827C2780` and `0x827C28D4–0x827C3340` into one block and delete StringTable's
`.text 0x827C2780–0x827C28D4`. **splits.txt is barred to this lane**, so this is a proposal, not an edit.

Priced at **exactly +1 fn / +340 B** rather than "up to", because the body is already byte-identical — and
the *collectability* was audited rather than assumed, since `matched_code` keys on `fuzzy == 100` and **one**
charged relocation-name site would yield `+1 fn / +0 B` (RESIDUAL-1):

> Our body has 9 branch relocations; retail has 8 `bl` + 1 `b` tail, and every offset corresponds
> one-to-one (`+0x004 +0x018 +0x060 +0x0ac +0x0d0 +0x124 +0x134 +0x144 +0x150`). Resolving each retail
> target through the map: **7 AGREE, 2 FORGIVEN, 0 CHARGED.** The 2 forgiven are `__savegprlr_27` /
> `__restgprlr_27`, unnamed in the map *and* excluded by objdiff's `vetted_reloc_name_diff` regalloc screen.

**Stated as the weak half:** the **data** relocation class is *not* measured and cannot be from here — a
charged-site list only exists once the row pairs, which requires the very splits edit I am barred from.
Those refs are *expected* forgiven (the map is a **function** map; the retail data addresses are unnamed, so
they render as placeholder `data_`/`lbl_` names). **Worst case +1 fn / +0 B.**

★ **Instrument trap recorded:** the `0x0012` records interleaved in the COFF relocation list are
`IMAGE_REL_PPC_PAIR`, trailing each REFHI/REFLO and pointing at the dummy `@comp.id` symbol. They are not
references. Counting them reads **41** relocations where the truth is **24**.

★ **Second trap, which overturned one of my own claims:** I first read the COMDAT as **708 B** from the
section's `rawsz`. That bills seven `__unwind$106278..106284` EH funclets. The correct extent is symbol
value `0x8` → first `__unwind$` at `0x174` = **364 B**. This is the STLPORT-1 billing trap that CLAUDE.md
documents, and it is the *same one-sided reader artifact* that once manufactured a non-existent "+8 B
STLport source bug".

### Sweep of the rest of the block

No other GlitchFinder row exists below 100 that a member-layout fix would explain, because **no
GlitchFinder body exists in retail at all**. The sweep the brief asked for is vacuous by the item's own
finding, and I am saying so rather than reporting it as clean coverage.

---

## Item 2 — AW-2 `?SetVolume@DirectInstrument@@QAAXH@Z` at `0x82A478D0`: shipped, **Δ0**, with the census corrected twice

**Briefed:** 17 call sites; adjudicate each by reading the COFF relocation in our compiled caller obj at the
same function offset; deliver a map row + alias group **with evidence**, or a written census saying why not.

**Measured: the briefed census was wrong in both directions, and the exposure is 2 sites, not 17.**

### The 18th reference AW missed

A reference census over retail (`bl` + `b` tail-calls + pointer words in all non-`.text` sections) found
**18** references, not 17. The extra one is a **`b` tail-call** at `0x82677444` — a class a `bl`-only census
cannot see. The map identifies its owner as **`?ForceTrackerStars@Game@@QAAXH@Z`** (`0x82677440`), which
gave a **second proven folded spelling** and is the reason the alias group could ship at all.

⚠ **This finding cost me a retracted claim, recorded because the retraction is the useful part.** I first
attributed that site to `?E3CheatAutoplayAccuracy@Game@@QAAXXZ`, having decoded X360 `.pdata` with the wrong
shift: `(w >> 2) & 0x3FFFFF` produced absurd extents (13,572 B for a 212 B function). The correct packed
layout is **big-endian**, `PrologLen = w & 0xFF`, `FunctionLen = (w >> 8) & 0x3FFFFF` in instructions.
Calibrated against two independently known extents (340 B @ `0x827C2780`, 264 B @ `0x827C3208`). The
re-run left all 17 `bl` owners unchanged — so those attributions were always right — and correctly showed
the `b` site as **NO-PDATA**: an 8-byte leaf touches neither stack nor LR, so it gets no unwind record. That
is the sub-`.pdata` stub stratum, and it is why every `.pdata`-keyed instrument is structurally blind here.

### The adjudication: 2 of 18 sites are chargeable, not 17

| class | sites | chargeable? | why |
|---|---:|---|---|
| named owner, our source defines the caller | **2** | **YES** | `GamePanel::Handle` (offset `0x59C`), `Game::ForceTrackerStars` (offset `0x4`) |
| unnamed owners (`fn_…`) | 15 | no | the **row** is unpairable, so no charged site can exist in it |
| XDK unit, `base_path: null`, no source | 1 | no | structurally unpairable; recorded **unadjudicable**, not guessed |

So AW's "converts 17 forgiven sites into checked ones" overstates the exposure **9×**. 16 of the 18 sit in
rows that cannot be scored at all — *pairability is the gate*, and an unpaired row is invisible to callee
adjudication while looking identical to a row with nothing wrong.

### The discriminating control

An unproven alias lifts `name_check` **by construction**, and the usual `none`-ruler control **cannot catch
a fabricated one** (`none` ignores relocation names, so it reads flat by construction — that flatness is the
*signature of the hazard, not a clearance*). So I ran the map row **without** the alias first:

- map row alone → **−8 B** on `?ForceTrackerStars@Game@@QAAXH@Z`, `GamePanel::Handle` unmoved.
- map row **+** alias group → **Δ0, 0 rows out.**

The −8 B is the naming bet losing, *measured*, on exactly one of the two chargeable sites. That is a control
that **could** have failed and did fail informatively, rather than a green light asserted.

### What shipped

`4a568624` — map row `"0x82a478d0": "?SetVolume@DirectInstrument@@QAAXH@Z"`, plus `symbol_aliases.json`
group 1648 (survivor `?SetVolume@DirectInstrument@@QAAXH@Z`, folded `?ForceStars@TrackerManager@@QAAXH@Z`).
Evidence tier **T1**: retail `0x82A478D0` is `908300004E800020`; **both** our COMDATs are exactly those 8
bytes with **0 relocations**. Relocation-free bodies of identical bytes *necessarily* fold under `/OPT:ICF`,
so this is total relocation-free byte identity, not a shape argument.

Survivor choice is the prudent one: 8 B at risk rather than 1,812 B. `tools/icf_alias_finder.py --validate`
**PASS, 0 CONTRADICTED**. Both JSON files round-tripped byte-identically and neither was reformatted.

### The other 11 signature-identical addresses

Genuinely uncalled — **0 references each** (`bl`, `b` tail-calls and pointer words in every non-`.text`
section), keyed on the `.fn` symbol and never the `.s` address column (dtk's address column is **synthetic**
for multi-block units). AW's claim survives its own re-test. Addresses: `0x82345394 0x8236E428 0x824BA5A0
0x827C0138 0x827D9638 0x82A86A6C 0x82A98CBC 0x82A98CFC 0x82ACEE08 0x82AEDDA8 0x82B456C0`.

---

## Item 3 — the stale CLAUDE.md paragraph

`95ec6c43`, single hunk, **+36/−13**, nothing else in the file touched.

The "3 mispaired … `UIStats`, `AccomplishmentProgress` and `Game` each carry both a path-qualified and a
bare heading" paragraph is rewritten as a **dated record**: the defect, that `b341d7ab` fixed it on
2026-08-31 (splits.txt only, +52/−58), that the halves were interleaved and the path-qualified heading kept.

**Two corrections drawn from `b341d7ab`'s own message**, because the old paragraph's prediction was wrong:

- ⚠ **NEGATIVE RESULT.** The old text predicted the fix "moves matched bytes". Measured
  42,274 → 42,273 fns / 3,772,560 → 3,772,520 B / 36.819992 → 36.819603 %. The whole −40 B is one row,
  `fn_8267F574`. Recorded as a refutation, not quietly dropped.
- ★ **Durable lesson kept**, as the brief required: `configure.py`'s unresolved-heading hard fail **cannot**
  catch this, because *both* headings resolve. Plus the three instruments that can.

Re-verified on the current tree: **0 of 1,292 duplicate headings, 0 bare+path pairs, 0 `base_path`s spanning
>1 unit, 23 benign basename collisions.** (2026-09-14 by W16-AX §3, independently again here.)

---

## Item 4 — `fn_827C91A0`: **declined**, and exactly what would change that

The *function* is identified beyond reasonable doubt; the *name* is not.

Retail `0x827C91A0` (36 B, 9 instructions) is
`lis r11,0x82C8; lis r10,0x8200; lwz r3,-0x70a4(r11); lfs f0,0x10b4(r10); fmuls f1,f1,f0; lwz r11,0(r3); lwz r11,8(r11); mtctr r11; bctr`
— i.e. `TheTempoMap->vtable[slot 2](arg * 1000.0f)`. Resolved literally: `0x820010B4` is the float
**1000.0** in `.rdata`; `0x82C78F5C` is the `.data` pointer **TheTempoMap**; vtable slot 2 is `TimeToTick`.
Reference census: **exactly 1 caller**, `?UpdateNowBar@GamePanel@@QAAXXZ` at `0x826951A4`; 0 tail-calls, 0
pointer refs. Map neighbours bracket it: `0x827c9190 ?TickToBeat@@YAMH@Z`, `0x827c91c8 ?SecondsToBeat@@YAMM@Z`.

`src/system/utl/TimeConversion.cpp:60` **already** implements exactly this as
`float SecondsToTick(float sec) { return MsToTick(sec * 1000); }`, with an in-source comment recording that
the name is convention-derived and that *"the map row `0x827c91a0` is deliberately left ANONYMOUS rather
than given this invented name."*

**I re-tested that claim rather than inheriting it, and it holds.** No oracle declares the name:
rb3-Wii's `TimeConversion.h` declares nine conversions — `MsToTick`, `MsToBeat`, `TickToMs`, `BeatToMs`,
`BeatToTick`, `TickToBeat`, `SecondsToBeat`, `TickToSeconds`, `BeatToSeconds` — and `SecondsToTick` is
**not** among them; DC3's header is a strict subset (three of those commented out). So the name is an
invention that happens to fit the convention, and under `name_check` a placeholder callee is **already
forgiven** at GamePanel's single call site: naming it has **zero byte upside** and converts one forgiven site
into a checked one. Naming it would be a bet, not a proof. Consistent with W16-AT and W16-AX, who both
declined.

⚠ **A vacuous instrument, reported as vacuous.** I checked retail for a string witness of the name and got
**0** — but the control kills the test: **`SecondsToBeat`, which *is* declared in the oracle and *is*
already mapped at `0x827c91c8`, also reads 0.** Retail stripped the `MILO_ASSERT` path strings that carry
such names, so this instrument cannot distinguish a real name from an invented one *for this whole family*.
Its 0 is not evidence, and I am not counting it as any.

**Evidence that would change this decision** (the brief requires this to be explicit, since a confident
decline is re-audited):

1. A **header declaration** of `SecondsToTick` (or any other spelling) in any oracle — ours, rb3-Wii, or
   DC3. This is the decisive one; today all three lack it.
2. A **leaked map/PDB** or symbol export naming `0x827C91A0`, in RB3 or in a sibling build sharing the TU.
3. A retail **string or RTTI witness** of the symbol — *but only from a build that retains assert paths*,
   since the retail-string instrument is proven vacuous here by the `SecondsToBeat` control.
4. A second **caller** whose own source spells the callee, which would make the name adjudicable at a call
   site rather than by convention. Today there is exactly one caller and our source for it does not name it.

---

## Commits on `w16-bb`

| sha | item | what | measured |
|---|---|---|---|
| `9ad1a5a8` | 1 | map: `0x827c2780` is CheatsManager's ctor, not GlitchFinder's | **+1 fn / +264 B**, predicted exactly |
| `9d78af2d` | 1 | `Cheats.cpp`: guard the DC3-era `SetName`; body now byte-exact vs retail | Δ0 (row unpairable), landed on merit |
| `4a568624` | 2 | map row + alias group for `0x82A478D0` with T1 evidence | **Δ0**, 0 rows out; control without alias = −8 B |
| `95ec6c43` | 3 | CLAUDE.md doubled-headings paragraph → dated record | n/a (docs) |
| `439ab786` | 1 | proposal BB-1: re-home the ctor out of StringTable | pre-priced **+1 fn / +340 B** |

## Gates

Run in the worktree, in the brief's order. `tools/native_build_gate.sh` **last**, because item 1 touches
shared `src/`.

| gate | result |
|---|---|
| full `./tools/ninja-locked` (`~/tmp/rb3_build_w16bb_6.log`) | **rc=0** |
| `scripts/verify_ruler_agreement.py --check` | **rc=0** — `OK: both objdiff-cli entry points resolve the same ruler`; all four keys pinned (`functionRelocDiffs=name_check`, `combineDataSections=true`, `combineTextSections=true`, `ppc.calculatePoolRelocations=false`) |
| `scripts/verify_objs_patched.py --verify-manifest` | **rc=0** — `1215 decomp, 3114 target objects match 2026-09-15T00:53:32Z (tree_sha256=72dba075555cc864)`; denylist OK |
| `tools/icf_alias_finder.py --validate` | **rc=0** — `VALIDATE: PASS -- 1400 map-consistent, 247 tolerated, 0 contradicted, 1648 total` |
| `tools/native_build_gate.sh` | **rc=0**, `skipped=0` — line below |

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

The native gate was the last **build-affecting** action, as the brief requires (item 1 touches shared
`src/`). Only a docs-only commit follows it: nothing under `docs/` is a build input. The CLAUDE.md
comment-only-break incident concerned a comment inside `rndobj/TexRenderer.cpp`, which
`ScatterIncludes.cmake` parses for `#if`/`#include` directives — a `.md` file has no such path into the
build graph.

The post-gate rebuild reproduced the lane measures to the last digit (`+1 fn / +264 B`, same single crossed
row), so there is zero build nondeterminism behind the figures above.

## NOT done, and why

1. **`splits.txt` re-home of `0x827C2780–0x827C28D4`** — barred to this lane by the brief's concurrency
   bars. Filed as proposal BB-1 with a collectability audit; worth **+1 fn / +340 B**.
2. **Removing `StringTable.cpp:103`'s `#include "utl/GlitchFinder.cpp"`** — becomes dead weight only *after*
   BB-1 lands, and `StringTable.cpp` is explicitly barred to me. Also touches shared `src/`, so it needs its
   own native gate run. Noted in BB-1's `second_order_effect`.
3. **Naming `fn_827C91A0`** — declined; see item 4 for the four kinds of evidence that would change it.
4. **The data-relocation half of BB-1's collectability audit** — unmeasurable from here; a charged-site list
   requires the row to pair, which requires the barred splits edit. Bounded at worst case +1 fn / +0 B.
5. **`GlitchFinder.cpp`/`.h` were never edited** — the briefed target. Not an omission: retail contains no
   GlitchFinder body, so there was nothing to match. The value was in the map row instead.
6. **The 15 unnamed + 1 XDK `SetVolume` call sites** — recorded unadjudicable rather than guessed, per the
   brief. They become adjudicable only if their owners are named (identification) or XDK source appears.
