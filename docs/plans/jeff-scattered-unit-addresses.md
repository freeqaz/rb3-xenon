# dtk asm listings printed synthetic addresses for scattered units

> **STATUS (2026-07-26): FIXED in jeff, branch `laneAF-va-fragments` (`57b52d6`,
> on top of `b50881e`). Verified, NOT yet swapped into the fleet binary** — the
> swap window was not quiescent (three other lanes had live `ninja` builds).
> Certified binary staged at `~/tmp/laneAF/staged/dtk.laneAF-va-fragments`
> (sha1 `bfdc7173…`, reports `dtk 1.9.2 57b52d64…`). Swap recipe at the bottom.
>
> **Strict-match delta: 0 (by design).** This is an *analysis-correctness* fix.
> The scored artifact is the `write_coff` `.obj`, which does not carry addresses;
> all target `.obj` files and `config/45410914/symbols.txt` are byte-identical
> before and after, and whole-binary strict is 30,093 → 30,093 (0 lost/0 gained).

## The defect

A split unit's section is assembled by concatenating several **non-contiguous**
ranges of the retail image — MSVC scatters a TU's COMDATs all over `.text`, and
`config/45410914/splits.txt` pins each piece separately. **634 of our 897 units
have more than one `.text` range** (distribution runs 2 … 90 ranges per unit).

`split_write_obj` (jeff `src/util/split.rs`) appended each fragment's bytes to
the growing section but recorded `virtual_address`/`file_offset` only for the
**first** fragment. Every address-facing consumer then computed
`virtual_address + offset` — a fiction for every byte past that first range.

The visible symptom: a function header whose printed address contradicts the
`.fn` label directly beneath it. From `build/45410914/asm/CharLipSync.s`:

```
# .text:0x60 | 0x822C5EC8 | size: 0x28          <- synthetic, WRONG
.fn fn_822DC878, global                          <- 0x822DC878 is the truth
/* 822C5EC8 002BACC8  2B 05 00 00 */  cmplwi cr6, r5, 0x0
```

**The `.fn` / `.obj "pdata@…"` LABEL is ground truth; the comment column was
not.** The same fiction propagated into generated `.L_<addr>` branch labels and
into the "unaligned symbol entry" / "skipping relocation" warnings.

## Measured blast radius (before the fix)

Over the full listing corpus (`build/45410914/asm/**/*.s`, 12,935 files):

| Scope | Headers | Wrong | |
|---|---|---|---|
| All `.text` + `.pdata` function headers | 312,942 | **57,925** | 18.5% |
| …restricted to scattered (multi-range) units | 37,409 | 31,973 | **85.5%** |
| …contiguous single-range units | 4,768 | **0** | 0% |

**594 distinct units affected.** Worst offenders were near-totally wrong:
`RockCentral.s` 1,505, `BandCharacter.s` 1,135, `BandDirector.s` 1,070,
`LightPreset.s` 750, `AccomplishmentPanel.s` 737.

`.pdata` was hit by the identical mechanism (25,951 wrong headers across 550
units). `.rdata`/`.data` were **not** affected — they are pinned as single
whole-section dumps, never per-TU multi-range.

Contiguous units being 100% correct is what pins the concatenation as the sole
cause. This corrects the handoff's "~20% of printed addresses are real": across
the whole corpus ~81% were already real; the ~20%-real figure is accurate **for
the scattered subset**, where only 14.5% were real.

## The fix

New `ObjSection::va_fragments: Vec<ObjSectionFragment>` records `(offset, size,
virtual_address, file_offset)` for each contributing range; `virtual_address_at`
/ `file_offset_at` / `display_address` resolve through it. **Sections built from
a single range leave the vector empty and keep the previous arithmetic exactly**,
so DOL/REL/ELF and every non-split path are bit-for-bit unchanged. The section
banner for a scattered section now enumerates its real ranges instead of a
`first_va..first_va+size` span that never existed:

```
# scattered across 18 ranges | size: 0x298
#   0x821F6C90..0x821F6C98 | size: 0x8 @ .pdata:0x0
#   0x821F8488..0x821F8490 | size: 0x8 @ .pdata:0x8
...
```

6 regression tests added (`va_fragment_tests` in `src/obj/sections.rs`);
jeff test suite 132 → 138, all green, clippy count unchanged at 141
(no findings in the new code).

## Verification evidence

Worktree `~/tmp/wt-laneAF-jeff`, branch `laneAF-jeff`, off main `7dd6f685`.

1. **Pre-flight — the live fleet binary is not reproducible from jeff HEAD.**
   The live `../jeff/target/release/dtk` reports `fc5d2af` (built from a working
   tree with the Class-4 pass uncommitted); HEAD is `b50881e`. Re-splitting with
   a clean `b50881e` build changes 4 `.s` + 5 `.obj` files — the live binary
   emits zero-size `__MERGED_fn_<addr>` tombstone symbols that committed source
   does not. **Match-neutral: 30,093 → 30,093, 0 lost, 0 gained.** Recorded as a
   hygiene defect, not a blocker; the new binary is built from committed source
   and reports its own commit, so this drift closes when it is swapped in.
2. **Control leg** = pristine `b50881e`, snapshotted to `~/tmp/laneAF/snap/`.
3. **Fixed leg** — vs the pristine control:
   - target `.obj`: **0 files differ** (byte-identical)
   - `config/45410914/symbols.txt`: **identical**
   - `.s`: **exactly 634 files changed** = exactly the 634 scattered units
   - strict: **30,093 → 30,093, 0 lost, 0 gained**
4. **Defect eliminated:** on freshly regenerated listings, **126,935 headers
   correct / 0 wrong** (was 57,925 wrong).

### Residual 140 "wrong" headers are stale orphan `.s` files — a separate bug

A post-fix rescan still shows 140 mismatches in 9 files (`FxSend.s`, `Utl.s`,
`Dir.s`, `FxSendDelay.s`, `AccomplishmentManager.s`, `Synth.s`, `Movie.s`,
`SynthSample.s`, `FlowOutPort.s`). All 9 sit at the **root** of
`build/45410914/asm/` with mtimes hours older than the build, and every one is
shadowed by a correct, freshly-regenerated path-prefixed sibling
(`system/synth/FxSend.s`, `system/obj/Utl.s`, `system/rndobj/Utl.s`,
`system/obj/Dir.s`, `system/world/Dir.s`, …).

**These are leftovers from an earlier `splits.txt` in which those units were
pinned under bare names; ninja never deletes a `.s` when a unit is renamed or
re-pathed.** `FlowOutPort` is the clearest case — it no longer appears in
`splits.txt` at all, and its two "functions" are addresses that now belong to
`Morph.cpp` and `BandUser.cpp`.

They are **actively misleading** (an analyst opening `asm/Utl.s` gets an
obsolete carve of a different address range) and they are pure build-directory
garbage. Suggested cleanup — for the splits/build owner, not applied here:
have the SPLIT step prune `.s` files not corresponding to a current unit, or
`rm -rf build/45410914/asm` before a re-split. Not committed: `build/` is
gitignored, and these 9 files exist per-tree.

## Swap / rollback recipe (for a quiescent window)

`dtk` is fleet-shared and is **not** a ninja input, so a swap triggers no
rebuild — it takes effect at whoever's next re-split. Confirm quiescence first
(`pgrep -af "ninja|dtk xex"` must show no builds).

```bash
# 1. land the source
git -C /home/free/code/milohax/jeff merge --ff-only laneAF-va-fragments

# 2. back up, then atomically swap the CERTIFIED binary (the exact one A/B'd)
cd /home/free/code/milohax/jeff/target/release
mv dtk dtk.pre-laneAF-bak
mv ~/tmp/laneAF/staged/dtk.laneAF-va-fragments dtk

# 3. confirm identity
./dtk --version                    # dtk 1.9.2 57b52d64...

# rollback (single mv):
#   mv dtk.pre-laneAF-bak dtk
```

Post-swap sanity check in any worktree: `touch config/45410914/config.yml &&
./tools/ninja-locked`, then confirm a scattered unit's header matches its label,
e.g. `grep -A1 '^# .text:0x60' build/45410914/asm/CharLipSync.s` should show
`0x822DC878` above `.fn fn_822DC878`.

**Expect a 15-line `symbols.txt` drift on the first post-swap re-split** (6
insertions / 9 deletions), e.g. `fn_822907F8` 0xB8 → 0xA8 plus a new
`fn_822908A0` 0x10. This is *not* caused by the address fix — it is the
`fc5d2af` → `b50881e` step. `symbols.txt` is a load-bearing split **input** and
the Class-4 pass converges across re-splits, so a binary built from committed
source settles on a marginally different fixed point than the uncommitted-source
binary did. **Measured match-neutral** (it is present in both legs of the A/B,
which came out 30,093 = 30,093, 0 lost / 0 gained). Per project rule
`config/45410914/symbols.txt` is never committed, so it simply remains a
working-tree diff — as it already was on main before this lane started.

## Over-carve funnel — DRAINED (do not fund a new merge pass)

Step-0 census of whether any head+anon-tail over-carve population survives the
Class-2 (`merge_fallthrough_leaf_fragments`, +67) and Class-4
(`merge_branch_reached_overcarve_tails`, +35) passes. Cross-checked at every
stage against a second source: the **raw PE `.pdata` table read straight from
`orig/45410914/band.exe`** (raw 0x1F1600, 57,733 big-endian 8-byte entries)
rather than the `.s`-scraped pdata, plus `symbols.txt` sizes vs `report.json`
sizes, plus `target_symbol_map.json` vs the current carve boundaries.

| Stage | Count |
|---|---|
| (a) named (non-`fn_`/`lbl_`) functions | 21,910 |
| (b) low-but-nonzero match (0 < x < 5%) — the over-carve signature | 88 |
| (c) …with an exactly-adjacent anonymous `fn_` tail | 48 |
| (d) …in scope (excludes `[0x82800000, 0x82D00000)` XDK/Quazal) | 44 |
| (e) …not blocked by a genuine retail `.pdata` anchor | 11 |
| (e′) …tail truly anonymous (not a separately-identified real function) | 8 |
| (f) …merge structurally expressible (no cross-unit conflict) | 8 |
| **hand-verified genuine head+tail over-carve** | **1** |

**Hand-verified true-positive ratio 1/8 (12.5%).** The single survivor is
`CamShot::Disable` (`fn_824BD7E0`, 16B + 4B + 20B ≈ 40 bytes, currently 1.25%)
— a 3-piece chain of the familiar shape, fixable by hand in minutes.

The 7 false positives were each a *different* failure mode, which is why the
raw counts are not trustworthy without reading asm: 3 were real independently
called functions (the "tail" has 2–8 `bl` call sites elsewhere in the binary),
2 were **stale `target_symbol_map.json` entries** whose mapped VA no longer
lands on a function start under the current carve, 1 was a wrong-name mispair
onto a complete 44-byte thunk, and 1 was a compound misidentification where the
mapped VA is itself a tail fragment of an earlier differently-carved function.

**Verdict: ~1 function / ~40 bytes of upside against a ~66,000-function binary.
No new jeff merge pass is justified.** This closes the
`project-overcarve-fragmentation-2026-07-17` vein; combined with Class 1 & 3
already being census-settled NO-GO, the jeff boundary-merge family is done.

**Side finding for the map owner (not applied — single-owner rule):**
`scripts/target_symbol_map.json` carries entries that are stale relative to the
current carve — 2 of the false positives above plus 5 stage-(c) entries whose
`symbols.txt` size disagrees with `report.json`. That is a map-validation
question, not an over-carve one.

## Related, settled by this lane

- **EH-funclet mis-carve: NOT A BUG.** The reported "`subi r31, r12, N` is an
  EH-funclet prologue that jeff mistakes for a function start" does not hold.
  39,735 `.fn` blocks open with `subi r31, r12, imm` (r31 in 100% of cases), and
  **98.4% of them are independently `.pdata`-anchored in retail — 100% (8,916 /
  8,916) within named units.** MSVC X360 emits a real unwind record per catch/
  finally funclet, so retail itself treats these as separate entities and jeff is
  reproducing that faithfully. This is an analyst-expectation gap (one source
  function ≠ one `.fn`), not a tool defect. The 644 unanchored rows are all in
  anonymous `auto_` buckets, several duplicated across overlapping carve windows.
- **Mis-nested `.fn`/`.endfn`: real but worth ~0, not +50–200.** 11,690 of
  11,755 `.s` files are clean; 817 events remain in 65 files, and **every one is
  an anonymous `auto_03_*_text.s` bucket — zero in named units.** The `+50–200`
  estimate predates the 2026-05 `prune_overlapping_phantom_functions` fix
  (`1900431`), which removed the harmful form (the `framing.s`/`ogg_sync_init`
  case, where mis-nesting starved a real COMDAT in a *named* unit).
  **Decisive:** none of the 51 actionable files appear in `report.json` at all,
  and all 5,666 `auto_*` units score **0 functions at 100% out of 27,607**. The
  surviving pattern is also self-cancelling — `.endfn` tags are emitted in
  forward rather than LIFO order when starts are carved back-to-back, so pushes
  and pops still balance (only 4 files end unclosed, from carve-window
  boundaries). Do not fund this as a match lever.
