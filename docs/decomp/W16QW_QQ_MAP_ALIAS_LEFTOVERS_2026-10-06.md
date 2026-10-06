# W16-QW — W16-QQ's map/alias leftovers (2026-10-06)

Lane W16-QW, branch `w16-qw`, worktree `~/tmp/wt-w16qw`, based on main `442f01984`.
The brief named two rows from `W16QQ_FRAME_CENSUS_ROWS_2026-10-06.md`:

1. `ContentMgr_Xbox`: `list<CharPollableSorter::Dep*>::_M_create_node` (retail `0x82520150`, 64 B),
   which fell from 100 to 0 after W16-QQ's SortPolls fix.
2. `?DrawShowing@WorldCrowd@@UAAXXZ` (88.68), described by W16-QQ as blocked by two
   wrong-name charges.

**Ruler.** `name_check`, objdiff 4.2.9, read from `report.json` `provenance`. Row figures below
are graded `fuzzy_match_percent` read from `report.json` after a full `tools/ninja-locked` build,
which runs all six patchers.

## Result

| | |
|---|---|
| `list<…>::_M_create_node` @ `0x82520150` | 0 → **100** (map renamed to `list<Content*>`, fold proven on retail bytes) |
| `WorldCrowd::DrawShowing` | 88.68 → **96.85** (source); the two name charges were **already forgiven** |
| whole-binary A/B | **+4 fns / +144 B / +0.001408 pp**; Δhonest +1, Δmasked_equal +3; units at 100 unchanged (582 / 514) |
| row-level diff of the A/B legs | **4 rows up, 0 down**, 1 rename pair (68,908 rows each) |
| native gate (run on `61f92b999`, the final source) | `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0` |

**A/B** (`tools/ab_measure.py --patch`, combined `442f01984..61f92b999` diff, fresh worktree
`~/tmp/wt-w16qw-ab`, kinds `map` + `source`, both legs settled and at a split fixed point):
leg A 54,672 / code% 58.601070 → leg B 54,676 / 58.602478.

**Prediction vs measurement.** I predicted **+1 fn / +64 B**, all from `0x82520150`, on the view
that DrawShowing does not cross. Measured **+4 / +144 B**. The difference is three 40 B EH
funclets of DrawShowing, paired by byte signature (`masked_equal`), which moved once the frame
and tracker slots matched retail:

| row | leg A fuzzy / mpn | leg B fuzzy / mpn |
|---|---|---|
| `list<Content*>::_M_create_node` (was `list<Dep*>`) | 0 / 0 | **100 / 100** (+64 B) |
| `fn_824E3940` (Crowd funclet) | 99.8 / 99.8 | **100 / 100** (+40 B) |
| `fn_824E3968` (Crowd funclet) | 99.8 / 99.8 | **100 / 100** (+40 B) |
| `fn_824E3918` (Crowd funclet) | 99.4 / 99.9 | 99.5 / **100** (+1 fn, +0 B) |
| `WorldCrowd::DrawShowing` | 88.68 / 89.65 | 96.85 / 97.17 |

64 + 40 + 40 = 144 B, so the A/B is fully accounted for. The honest gain is the +1 row; the
other +3 are funclet byte-signature pairings.

## 1. `0x82520150` — name the shared body with a spelling the unit defines

**Why it fell.** objdiff pairs rows by exact name. The map named `0x82520150` with
`list<CharPollableSorter::Dep*>::_M_create_node`. After W16-QQ gated `CharPollableSorter` out of
ContentMgr_Xbox's scatter include (`char/CharPollGroup.cpp`), no object in the unit defined that
name, so the row had no partner.

**Which name.** I scanned the `/Gy` COMDAT symbol tables of all 1,268 compiled objects
(`tools/coff_owned.analyze`):

- `ContentMgr_Xbox.obj` defines three `list<T*>::_M_create_node` spellings: `RndPollable*`,
  `Content*` and `Hmx::Object*`.
- `list<Content*>` is defined by **no other object**.
- The map names neither of the other two anywhere.

The retail body sits inside ContentMgr_Xbox's `.text` block `0x8251F898–0x82521730`, so the
COMDAT the linker kept came from that TU. `list<Content*>` is the one spelling only that TU
emits. It was also already a folded member of the group, with CF5 evidence.

**Proof on retail bytes** (`tools/icf_pair_adjudicate.chase`, built tree, renamed target objs):

- All 9 group spellings chase **PROVEN** against the body at `0x82520150` (64 B / 64 B).
- So do the two unmapped ContentMgr_Xbox spellings, `RndPollable*` and `Object*`.
- **Negative control, same run:** a struct-valued `list<OldMatOption>::_M_create_node` and two
  `_Rb_tree::_M_create_node`s fail with `BYTES-DIFFER`. `list<int>` and other 4-byte elements
  prove, as expected for a 4-byte copy. The chase discriminates.

**Change.**

- `scripts/target_symbol_map.json`: `0x82520150` is now `list<Content*>::_M_create_node`.
- `scripts/symbol_aliases.json`: I relabelled the group survivor with
  `tools/alias_survivor_relabel.py --write --lane "W16-QW 2026-10-06"`. The build's
  `CHECK ALIAS SURVIVORS VS MAP` edge failed on the stale survivor exactly as designed. The tool
  re-chased every membership: **7 folded PROVEN, kept; old label `Dep*` PROVEN, folded; 0
  withdrawn, 0 re-homed, 0 contradicted.** The record is in the group's `relabelled` and
  `rechase_w16os` fields.

**Prediction:** +1 fn / +64 B. **In-tree read** after the rebuild: 54,672 → 54,673 fns,
6,005,316 → 6,005,380 B (+64). The row reads `fuzzy 100 / mpn 100`.

**Not done.** `list<RndPollable*>` and `list<Hmx::Object*>::_M_create_node` also chase PROVEN
here, but I did not add them as folded members. That would be new forgiveness at call sites
outside this brief, and this lane did not measure them.

## 2. `WorldCrowd::DrawShowing` — the name charges were not the blocker

**The W16-QQ premise is refuted.** It said retail resolves `vector<JumpInstance>::erase` and
`vector<SongParser::GemInProgress>::push_back` where ours calls `vector<Hmx::Rect>`, and that
"the Rect spellings are in no alias group". At `442f01984`:

| site | retail callee (address) | ours | alias group | site `match_type` |
|---|---|---|---|---|
| idx 280 `bl` | `vector<JumpInstance>::erase` (`0x82772870`) | `vector<Hmx::Rect>::erase` | folded member of `0x82772870` | **equal** |
| idx 340 `bl` | `vector<GemInProgress>::push_back` (`0x82788308`) | `vector<Hmx::Rect>::push_back` | folded member of `0x82788308`, admitted `6d8274b54` (2026-10-03) | **equal** |

Both spellings appear in the rendered `build/45410914/icf_aliases.map`, and objdiff charges
neither site. The other `[sym]`-flagged rows (idx 45/46) have placeholder retail targets
(`lbl_…`), which objdiff forgives. They are charged for a register difference, not a name.

**Re-adjudicated on retail bytes anyway:** `chase(survivor, ours-Rect)` is **PROVEN** for both.
Control: across 40 other `vector<T>` spellings of the same operation, only `vector<Hmx::Color>`
also proves. That is another 16-byte trivially-copyable element, so it is the expected positive.
The fold is real; the map and aliases needed no change.

**What actually held the row.** Our body was 1,964 B against retail's 2,072, with a frame 0x30
shallower and 39 retail-only instructions. Most of those were dead homes into the scratch slot
`0x50(r31)`:

- inlined `Set()` `this` pointers at every vertex member;
- `clrrwi` + `stw` reloads of `mEnviron`;
- the CSE'd `rects[ri]` reads.

DC3 `8b0a045c2` took the same function to 100 canonical with exactly these spellings. I ported
them one step at a time:

| step | spelling | fuzzy | our size |
|---|---|---|---|
| base | — | 88.68 | 1,964 |
| 1 | vertices through `Verts()` + `Vector3::Set` / `Vector2::Set`, mesh re-read | 92.98 | 2,044 |
| 2 | no cached `env`: `mEnviron` re-read at every use | 94.25 | 2,064 |
| 3 | rect loop: decl order `minX, maxX, ri, minY, maxY`; `Min`/`Max` with `rects[ri]` at both uses | 96.80 | **2,072** |
| 4 | `Cross()` with the camera ref declared inside each arm | 97.25 | 2,080 |
| 5 | one `camXfmCopy.v.Set(...)` | 96.83 | 2,072 |
| 6 | trailing environ tracker written out at loop scope (own slot 0x150) | **96.85** | 2,072, frame **0x260 = retail** |

**Step 5 lowers the score, and I kept it.** With step 6 in place, removing `Set()` reads 97.27,
but then:

- the body is 2,080 B, 8 B too long;
- `v.x` is stored before `v.y`, where retail stores `v.y` first (idx 152/154);
- the `Cross` arms never cross-jump.

With `Set()`, both the store order and the body length are retail's. Neither variant crosses
100, so the choice moves no `matched_code`.

**Left at 96.85 (25 `diff_arg`, 7 insert, 7 delete, 1 replace):**

- **f23/f24 swap** between `halfWidth` and the hoisted `0.5f` (7 rows). DC3 records two refuted
  spellings for it.
- **`Subtract` region scheduling.** Ours copies `mr r11, r3` because `addi r3, r29, 0x1c` is
  scheduled earlier; retail loads the camera vector straight off `r3`.
- **`Cross` arms.** Retail keeps source operand order in arm 2 (`fmuls f6, f9, f12`), so only the
  final `fmsubs`/`stfs` of `x.x` cross-jumps. Ours commutes arm 2's products to match arm 1 and
  merges the `fmuls` too.

All three are register or schedule shapes. The permuter is off by directive.

## Corrections to W16-QQ's doc

- "The Rect spellings are in no alias group": **false at `442f01984`.** Both are folded members,
  and both sites read `equal` (see §2).
- "The two blocking charges are names": **false.** The row carried 196 mismatches, and the name
  sites were not among the charged ones. The gap was source shape.

## Commits

- `fa1f25588` map + alias relabel for `0x82520150`
- `862442adf` DrawShowing steps 1–3
- `61f92b999` DrawShowing steps 4–6
