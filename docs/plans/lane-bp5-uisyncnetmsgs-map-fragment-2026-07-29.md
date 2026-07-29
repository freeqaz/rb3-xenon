# Lane BP-5 — map fragment justification

Fragment: `~/tmp/bp5_map_fragment.json` (13 rows, flat `{addr: mangled_name}`)
Apply with: `python3 scripts/harvest/tu5_map_apply_fragment.py ~/tmp/bp5_map_fragment.json scripts/target_symbol_map.json`
Branch that makes it live: `laneBP5` @ `55720e33`

## Why these 13 became applicable only now

Lane BO-1 produced these exact 13 rows and BP-4 §8 re-verified them **per row:
13 CONFIRM, 0 REFUTE, 0 UNDECIDABLE** — class identity from RTTI Complete Object
Locators (`.?AVComponentFocusNetMsg@@` etc.), `Save`/`Load` pinned by vtable slot
(slot[1]=Save, slot[2]=Load) *and* independently by the `??5`/`??6`
stream-direction discriminator, `Dispatch` bodies matched against the exact
static-local ctors the oracle constructs with correct multiplicity.

BP-4 nevertheless marked all 13 **INERT and did not apply them**, for one reason:
none of the names had a COMDAT in our build, because
`src/band3/game/UISyncNetMsgs.cpp` did not exist in the tree. Renaming a target
`fn_` to a name our base obj cannot supply is unpaired-before/unpaired-after,
Δ=0 by construction.

**`55720e33` removes exactly that blocker.** The TU is now ported and pinned, so
all 13 COMDATs exist in `build/45410914/src/band3/game/UISyncNetMsgs.obj`.
BP-4's own note — "Porting that TU from the rb3-Wii oracle then makes all 13
live" — is now satisfied.

## Independent corroboration added by this lane

1. **Injectivity, checked twice.** `tu5_map_apply_fragment.py` asserts on both
   addr and name collision. It inserted all 13 with **zero** collisions against
   the map at `cd1af208`, and a re-check against **main's current** map
   (26,756 entries) is also clean. So all 13 VAs were unmapped and all 13 names
   unused — no repoint, no name theft, pure insertion.
2. **Sizes agree row-for-row.** Every VA's `symbols.txt` size equals the size of
   the COMDAT the name now supplies (0x8C, 0x94, 0x64, 0x58, 0x100, 0x7C, 0x6C,
   0x94, 0xD8, 0xD8, 0xA4, 0x200, 0x400).
3. **The measurement agrees.** Applying the fragment moves the unit from
   38/51 (fuzzy 32.64%) to **50/51 (fuzzy 99.73%)**, and 11 of the 13 land at a
   **true 100%** immediately. A wrong name cannot produce a 100% byte match
   against a differently-shaped COMDAT, so 11 rows are confirmed a fourth time
   by construction. The remaining 2 (`Dispatch@ComponentSelectNetMsg` 97.5%,
   `Dispatch@ComponentScrollNetMsg` — later 100%) paired at plausible
   near-match, never at noise level.

## Price

Measured in `~/tmp/wt-bp5`, same worktree and same split state on both legs,
`report.cache` removed before each read, `symbols.txt` restored before each
split-forcing build:

| leg | matched_functions | masked_equal | matched − masked_equal |
|---|---|---|---|
| baseline (pre-carve) | 40831 | 1514 | 39317 |
| `55720e33` source patch only | 40858 (+27) | 1509 | 39349 (**+32**) |
| + this fragment | 40870 (+39) | 1509 | 39361 (**+44**) |

**The fragment's own marginal value is +12 matched / +12 proxy.** It is *not*
false credit: the 12 are true 100% byte matches on named COMDATs, and
`masked_equal` does not rise (1509 in both legs).

## Per-row table

| VA | name | retail size | result with fragment |
|---|---|---|---|
| 0x8269f3f8 | `??0ComponentFocusNetMsg@@QAA@PAVUser@@PAVUIComponent@@@Z` | 0x8C | 100% |
| 0x8269f4e0 | `??0ComponentSelectNetMsg@@QAA@PAVUser@@PAVUIComponent@@_N@Z` | 0x94 | 100% |
| 0x8269f5c8 | `?Save@ComponentSelectNetMsg@@UBAXAAVBinStream@@@Z` | 0x64 | 100% |
| 0x8269f630 | `?Load@ComponentSelectNetMsg@@UAAXAAVBinStream@@@Z` | 0x58 | 100% |
| 0x8269f690 | `??0ComponentScrollNetMsg@@QAA@PAVUser@@PAVUIComponent@@@Z` | 0x100 | 100% |
| 0x8269f7e8 | `?Save@ComponentScrollNetMsg@@UBAXAAVBinStream@@@Z` | 0x7C | 100% |
| 0x8269f868 | `?Load@ComponentScrollNetMsg@@UAAXAAVBinStream@@@Z` | 0x6C | 100% |
| 0x8269f8e0 | `??0NetComponentSelectMsg@@QAA@PAVUser@@PBD@Z` | 0x94 | 100% |
| 0x8269f9f8 | `??0NetComponentScrollMsg@@QAA@PAVUser@@PBDHH@Z` | 0xD8 | 100% |
| 0x8269fba0 | `??0NetComponentPostScrollMsg@@QAA@PAVUser@@PBDHH@Z` | 0xD8 | 100% |
| 0x8269fd40 | `?Dispatch@ComponentFocusNetMsg@@UAAXXZ` | 0xA4 | 100% |
| 0x8269fdf0 | `?Dispatch@ComponentSelectNetMsg@@UAAXXZ` | 0x200 | 97.5% (spill-class, see below) |
| 0x826a00d0 | `?Dispatch@ComponentScrollNetMsg@@UAAXXZ` | 0x400 | 100% |

Ordering dependency: apply the fragment only with `55720e33` (or later) in the
tree. Without the port the rows are inert, exactly as BP-4 found.

## Deliberately NOT in this fragment

* **`?Save@/?Load@ComponentFocusNetMsg@@`.** Their bodies are *not* in the
  carved span — they were ICF-folded to `0x82690A10` / `0x82690B28`, which the
  map currently calls `?Save@SetUserTrackTypeMsg@@` / `?Save@SetUserDifficultyMsg@@`.
  BP-4 §2 proved `0x82690B28` is a **Load** (both its `bl`s resolve to `??5`
  operators) and §8 pinned the class via `ComponentFocusNetMsg`'s vtable slot[2].
  BP-4 could not repoint it because no COMDAT existed; **`55720e33` now supplies
  `?Load@ComponentFocusNetMsg@@`, so that repoint has become applicable.**
  It is a *repoint*, not an insertion, it lies outside my span, and BP-4 warned
  it trades two current false-100s — so it belongs to the map owner with
  `map_repoint_apply.py`, not to this fragment. Flagged as newly-unblocked work.
* **The 4 guard-clear `??__F` atexits and 33 EH funclets** in the span. All
  already sit at a true 100% via objdiff's byte fallback with no map entry, so
  naming them buys ~0 and only risks perturbing a working pairing.

## Residual

`?Dispatch@ComponentSelectNetMsg@@` holds at **97.5%** — 4 of 128 instructions.
Retail spills `panel` into frame slot `0x50` twice and tests it on `cr0`; we keep
it in a register and test on `cr6`. Frame sizes are equal (0xb0 both) and 8 of 9
user stack slots match, so this is a pure spill/regalloc decision, not a layout
or logic defect. I tried the assignment-in-condition form
(`if ((panel = screen->FocusPanel()) != 0)`), the standard MSVC lever for
store-then-compare-on-cr0: **byte-identical output, no change** — MSVC normalizes
it. That refutes the source-shape reading and leaves this permuter-class, which
is banned by standing directive, so it is left open and honest.
