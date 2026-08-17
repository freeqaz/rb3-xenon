# `VocalTrack::UpdateScrolling` — PRICED REFUSAL (lane W30-SCROLL, 2026-08-17)

**Verdict: the 8,948 B headline prize is UNCOLLECTABLE BY SOURCE WORK.**
Fund this row, if ever, only as a **+1-function** target — and that costs a
diffuse reconstruction of a 2,511-instruction body. **Recommendation: do not
fund.**

Row: `?UpdateScrolling@VocalTrack@@QAAXM@Z`, unit `default/VocalTrack`,
size **8,948 B**, `fuzzy 72.25346` / `mpn 73.98793`.
Measured at `43ac6c43` + build, ruler = shipped **`name_check`**
(`report.json` `provenance`), whole-binary baseline reproduced exactly in the
lane worktree: `matched_functions 44,505 / matched_code 3,760,224 /
total_code 10,320,664 / code% 36.433937`.

## 1. The briefed figures reproduce exactly — and they are not the price

Re-derived independently from `objdiff-cli diff -f json` with **no `-c`** (so
`objdiff.json`'s `functionRelocDiffs=name_check` + `map_file` apply). Ruler
replication verified: this path reports `fuzzy 72.25346`, **identical** to
`report.json`.

| class | count |
|---|---:|
| `delete` | 269 |
| `insert` | 274 |
| `replace` | 58 |
| `diff_op` | 17 |
| **"618 hard diffs"** | **618** ✅ reproduces |
| `diff_arg` | 765 |
| **TOTAL CHARGED** | **1,383** of 2,511 rows |

⛔ **The "618 + 12" framing understates the price by 2.2×.** `matched_code`
keys on `fuzzy == 100` and is **all-or-nothing per row**, so the 8,948 B
requires **all 1,383** charged sites to close, not 630. The `diff_arg`
population is 289 register-only, 173 immediate-only, 101 imm+reg, 94 reg+reg,
50 imm+reg+reg, 24 `branch_dest`, 14 reg×3, **12 bare-symbol**, 6 reg+symbol,
1 reg+reg+symbol, 1 reg×4.

(Much of the register/immediate/stack-offset mass is plausibly *downstream* of
the body divergence and would dissolve with a correct body — cf. the standing
"REGISTER_SWAP is a symptom, not a diagnosis" rule. That is **not** true of the
name charges below, which are independent and gate the bytes.)

## 2. The 12 name charges — correctly counted, then adjudicated on retail bytes

Counted per the standing rule: **only a bare `arg:{Symbol}` is a real
relocation-name charge**; `arg:{Register,Symbol}` is charged by the register.
Bare-symbol rows = **12** ✅ reproduces. (The 7 mixed rows are all
`lbl_*` constant-pool/string references — placeholder targets, charged by their
register component, and they dissolve with regalloc.)

The 12 sites are only **three distinct callee pairs**:

### Pair A — 6 sites. NOT source-collectable.
retail `?Init@Movie@@SAXXZ` @ `0x827c9110` · ours `?TickToMs@@YAMM@Z`

`fn_827C9110` is a 6-instruction tail-call thunk: load the global at
`0x82C78F5C`, load its vtable, `bctr` to slot `+4`. A `float(float)` forwarder
fits perfectly (the float arg rides through `f1` untouched); `Movie::Init()`
(static, void, no args) is implausible as a 6×-per-scroll-update callee. This
is the classic ICF-folded-thunk shape where **ICF itself destroyed which name
the call site meant**.

⛔ **Both proof routes are structurally unavailable:**
- **T1** (retail bytes vs our compiled COMDAT for the folded spelling):
  `TickToMs` is **declared in `src/system/utl/TimeConversion.h` and never
  defined anywhere in the tree** — there is no our-side body to compare.
- **T3** (both names at one address in DC3's `ham_xbox_r.map`): `TickToMs` is
  **absent from DC3's map** (`Movie::Init` is present, at `82555678`).

### Pair B — 5 sites. **Our source is RIGHT; the MAP is WRONG.**
retail `?GetDrumFillInfo@SongDB@@QBAPAVDrumFillInfo@@H@Z` @ `0x82684fa8` ·
ours `?GetVocalNoteList@SongDB@@QBAPAVVocalNoteList@@H@Z`

Proven on retail bytes:
- `fn_82684FA8` = `lwz r3, 0x4(r3)` ; `b fn_82771500` — an 8-byte forwarder
  through `SongDB::mSongData` (offset 4).
- `fn_82771500` is **unmistakably `SongData::GetVocalNoteList(int)`**: loads
  `this->0x128` (`mPlayerTrackConfigList`), calls `UseVocalHarmony()`, then
  returns `mVocalNoteLists[idx+1U]` with a size guard, else `nullptr` for
  `idx != 0`, else `mVocalNoteLists.front()` — line-for-line our
  `src/system/beatmatch/SongData.cpp:1251`.
- Therefore `fn_82684FA8` **is** `SongDB::GetVocalNoteList`, which is exactly
  what our source calls.
- The map's own `SongData::GetDrumFillInfo` lives at `0x82771420`, **not**
  `0x82771500` — so `fn_82684FA8` cannot be `SongDB::GetDrumFillInfo`, which
  would have to tail-call `0x82771420`.

### Pair C — 1 site. Genuine body divergence; source-fixable.
retail `?Empty@LyricPlate@@QBA_NXZ` · ours `?GetLyricAlpha@VocalTrackDir@@QBAMH@Z`

Not a fold: `fn_82BAEE58` really is `Empty()` (`(m0x38 - m0x34) == 0` via
`cntlzw`/`extrwi`). Our side emits an entire block of
`GetLyricColor`/`GetLyricAlpha` calls that retail **does not have** (all rows
`insert`). This is part of the 618, not a naming issue.

⇒ **11 of the 12 name charges cannot be closed by editing source.** The
8,948 B is uncollectable by source work in principle — the exact trap
previously briefed on `?Handle@CustomizePanel@@`.

## 3. The body work is diffuse, not localized

618 hard diffs spread over **103 clusters** across the whole 2,511-row
function; the top 5 clusters are only **40.3%** (largest 98 and 90 rows). Our
`UpdateScrolling` is *already* a heavily worked-over port (it carries a
`goto window_ok` control-flow shape), so this is residual reconstruction, not a
missing port. Closing all 618 buys `mpn 100` = **+1 function and +0 bytes**.

## 4. Spin-off: a systematic MAP defect in SongDB's 8-byte forwarder block

Found while adjudicating Pair B; **evidenced but NOT actioned by this lane** —
it is a multi-row map realignment entangled with an existing alias group and
needs its own A/B.

| addr | map name today | retail body | reads |
|---|---|---|---|
| `0x82684fa0` | `?EnableGems@SongDB@@QAAXHMM@Z` | `lwz r3,4(r3); b SongData::GetDrumFillInfo` | 97.5% |
| `0x82684fa8` | `?GetDrumFillInfo@SongDB@@…` | `lwz r3,4(r3); b SongData::GetVocalNoteList` | **false 100.0%** |
| `0x82770730` | `?GetVocalNoteList@SongDB@@…` | 24 B, indexes `+0x50`/`+0xb0`, tail-calls `GameGemDB::GetDiffGemList` | **31.5%** |

- `0x82684fa8` reads a **false 100.0%** because retail's tail-call target
  `fn_82771500` is **unnamed ⇒ a placeholder ⇒ forgiven** by `name_check`.
  A textbook instance of "a wrong callee scores a clean 100".
- `0x82770730` is 24 B and GemList-shaped; our `GetVocalNoteList` is an 8-byte
  forwarder — hence 31.5%. That address is **not** `GetVocalNoteList`.
- `0x826850b8` (`SongDB::GetFillInfo`) is **consistent** and needs no change:
  it forwards to `SongData::GetFillInfo`, which is a *proven* existing fold with
  `SongData::GetDrumFillInfo` @ `0x82771420`.

⚠ Do not blind-fire the rename: freeing `GetVocalNoteList` from `0x82770730`
un-pairs that row, and **un-pairing is ~80.5% of a map edit's delta**. The
true identity of `0x82770730` is not yet established.

## 5. What this lane deliberately did NOT do

- No source edit (none was warranted — on the 5 Pair-B sites our source is
  *proven correct*, and the one real defect, Pair C, is inseparable from the
  618-cluster reconstruction).
- No alias installed for Pair A: unprovable by T1 **and** T3, and an unproven
  alias lifts `name_check` **by construction** — an integrity hazard, not a win.
- No SongDB map realignment (see §4).
- `?Handle@CustomizePanel@@` was not assessed; budget went to settling this
  row's price, which was the lane's primary question.
