# W16-PO — lever 1: the never-worked source-divergence rows (18 rows / 7,068 B)

Lane W16-PO, 2026-10-06, worktree `~/tmp/wt-w16po`, branch `w16-po`, rebased onto main `a3f3339a8`
(after W16-PQ/PR/PS/PT landed). Brief: lever 1 of `CAMPAIGN_STATE_2026-10-06.md` §5. Those are the 14 rows W16-PC's
pool recorded as "not worked" plus the 4 I-class rows with no record. Fix each row to retail's behaviour, or record
why it stops. Ruler: shipped graded `name_check`, read from `report.json`.

The row list was taken from `~/tmp/w16pn-gap/dispo_rows.json` (dispositions `08a` and `10`) and re-read on this
tree before any edit. All 18 rows were unchanged from the campaign doc: **7,068 B open**.

## 1. Result

| | rows | bytes |
|---|---:|---:|
| to 100 | **9** | **1,880** |
| raised, not to 100 | 2 (DelayEffect 86.41 → 95.10, WahEffect 89.28 → 91.76) | — |
| stopped, recorded | 7 | 4,516 |
| still open of the 18 | 9 | 5,188 |

**Prediction, written before the A/B:** the nine crossings give +9 fns / +1,880 B, with possible spill-over from
`NextSubDir` going out-of-line (~40 TUs call it through `ObjDirItr`) and from the new `make_pair` alias, and **0 rows
down**. Run 1 measured +9 / +1,840 with one funclet down 40 B, which was fixed by one more proven fold spelling.
Run 2 measured **+9 fns / +1,880 B, 0 rows down**, with no spill-over in either direction (§5).

**Behaviour defects fixed (native-visible):**
1. **`WahEffect::Process` wrote `mLastInput` and `mLastOutput` on every sample.** Retail's channel loop has no store
   to `this+0x44` / `this+0x48`. The offsets were confirmed with `class_layout_report.py WahEffect --tu
   src/system/synth/WahEffect.cpp`: `0x44 mLastInput`, `0x48 mLastOutput`. DC3's `Process` has neither store; the
   2026-05 scaffold added them. The fields keep their ctor zero.
2. **`RndDir::EndFrame` used `Max()` (an `fsel`), retail compares and branches** (`fcmpu; bge; fmr`, i.e.
   `if (frame < end) frame = end`). The two differ when an animatable's `EndFrame()` is NaN: retail keeps the running
   maximum, and `fsel` takes the NaN.
3. **`DataPoint::AddPair(Symbol, DataNode)` re-interned its key** through `make_pair(name.Str(), value)` and
   `Symbol(const char *)`. Retail copies the `Symbol` word directly (`lwz`/`stw`, no `Symbol` ctor call). The result
   is the same key, without the string-table lookup.

The other fixes are codegen-only. Each was checked to compute the same values as before.

## 2. Rows taken to 100

| row | B | before | lever (mechanism) |
|---|---:|---:|---|
| `ObjectDir::HasSubDir` | 96 | 84.92 | **`NextSubDir` moved out-of-line into Dir.cpp.** Retail keeps `this`/`dir`/`i` in *volatile* r7–r9 across `bl NextSubDir` and saves no GPRs. That is MSVC's intra-TU callee-clobber analysis (W16-ER), which needs the callee's body: retail `NextSubDir` touches only r3/r4/r10–r12/r27–r31. As an inline header method our byte-identical `NextSubDir` did not give HasSubDir that knowledge; as an ordinary definition it does. Behaviour was already retail's (flat walk, no self-test) |
| `CDReadExternal` | 104 | 87.69 | `SetFilePointer(v, ((LONG *)&u)[1], ...)`. Retail spills `u` (`std r5,0xa0`) and reloads its low word (`lwz r4,0xa4`), not a register truncation. Same value |
| `RndDir::EndFrame` | 132 | 89.09 | compare-and-branch instead of `Max()` (defect 2 above) |
| `DataPoint::AddPair(Symbol)` | 164 | 89.02 | `make_pair(name, value)` (defect 3), which took the row to 99.88 with one charge left, the callee name. Then a new alias group, §3 |
| `LoadDtz` | 188 | 88.72 | read the 4-byte size trailer through `const char *sizeBytes = &c[i - 4]` (DC3's spelling). Retail forms `c + i - 4` first |
| `Quat::Set(const Vector3 &)` | 260 | 94.88 | ported DC3 lane w21-ac: the z half reads `x` and `w` through `Hmx::Quat &q = *this` |
| `BlockMgr::AddTask` | 284 | 98.00 | DC3's `BlockRequest::CheckMetadata` / `LessThan` inline helpers, with block number read before ark number. Retail reads task+4 before task+0 and re-reads `mArkfileNum` inside the ordering test |
| `DataStringFlags` | 296 | 94.51 | ported DC3 lane w21-s: a named `DataNode ret(s); return ret;` gives retail's prologue order |
| `DirLoader::~DirLoader` | 356 | 98.76 | ported from DC3: `else if (ObjectDir *dir = mDir) dir->SetLoader(nullptr);`. Calling the inlined `SetLoader` on the null-checked member homed the receiver to a dead EH slot retail lacks |

**Four of the nine were DC3 fixes our copy predated** (Quat::Set, DataStringFlags, ~DirLoader, AddTask), and a fifth
(LoadDtz) was DC3's spelling that our copy had re-spelled. Our engine files are DC3 copies taken before those DC3
lanes ran. A row-by-row body diff against current DC3 is a cheap first step for any long-open engine row; it is how
these five were found.

## 3. The aliases installed (map-kind change)

`scripts/symbol_aliases.json` has one new folded spelling in the existing destructor group `w16hs_0x82768b18`. It was
found by the A/B (see §5). There is also one new group at **0x827cd140**:
- survivor `??$make_pair@PBDVDataNode@@...` (retail's map name)
- folded `??$make_pair@VSymbol@@VDataNode@@...`

Evidence, on the two channels W16-PH used:
- **Byte channel:** `tools/icf_pair_adjudicate.py --survivor … --ours …` → **FLAT T1 PROVEN**.
  - retail 96 B = ours 96 B, 4 relocations, `reloc_tally {}`;
  - `retail_bodytwins 1` against `our_bodytwins 2`, so our two instantiations are relocation-identical and retail holds one body.
- **Type witness:** the charged caller's own retail bytes.
  - `AddPair(Symbol)` builds the map's `pair<const Symbol, DataNode>` by copying one word out of `make_pair`'s result,
    with no `Symbol(const char *)` call.
  - A `pair<const char *, DataNode>` result cannot be converted that way. Our previous source shows it: it needed the
    `bl ??0Symbol@@QAA@PBD@Z` that retail lacks.
  - So retail's call is `make_pair<Symbol, DataNode>`, folded by ICF onto the `const char *` instantiation's address.

There was no prior claim or withdrawal for either spelling. The build's `CHECK ALIAS SURVIVORS VS MAP` and
`CHECK ICF-ALIAS MAP` edges pass (2,134 groups).

## 4. Rows stopped, with why

| row | B | now | why it stops | tried |
|---|---:|---:|---|---|
| `JoypadClient::Poll` | 116 | 93.59 | Behaviour identical (both timers reset while the guide shows). Retail forms `&mRepeatTimer` into a callee-saved register and derives `&mHoldTimer` as `-0x30`; one extra saved GPR | index form `mRepeats[i]` (**worse**, 93.21); two bound references (inert). DC3's w7-av/w9-a record six more refuted spellings for the same residue. Nine spellings across four lanes |
| `ThreadCallPoll` | 252 | 92.22 | Behaviour identical. Retail zero-extends the saved type (`clrrwi r9,r9,0`) before the switch's compare chain; scheduling follows from it. **Our body is DC3's verbatim, and DC3's own build misses the same way (94.44)** | `default:` arm instead of `case kTCDT_None:` (inert); `switch ((unsigned int)oldType)` (**worse**, 90.48) |
| `DirLoader::SaveObjects` | 1,044 | 98.01 | Behaviour identical (TU5 shape: rev 0x1c, no NextName). Two copy-coalescing residues: (a) at `_S_sort` retail reloads the comparator byte and stores it back to its slot, one more by-value copy than ours; (b) in the save loop retail copies `begin()` through r11 into the iterator. DC3's build carries (b) unsolved (w17-d, 99.29) | `sort(ClassAndNameSort())` (**worse**, 96.28); a named copy `ClassAndNameSort cmp(sorter)` (inert) |
| `FindCCPeak` | 920 | 97.17 | Same operations in the same operand order. Retail's allocator reuses f0 for the `ss_data` difference and re-loads `dp_data[n]` for the ratio; ours keeps it live. Register allocation | the one lever with a mechanism (a separate address expression for the neighbour test, `const float *cur = &dp_data[n]`) recompiled (confirmed in the log) and was byte-inert |
| `Intersect(Segment, Triangle)` | 432 | 97.19 | FP operand order and accumulation order under `/fp:fast`; sculpted body at a local optimum | products in `tempDot` flipped, `hitPoint` products and the `.y` add flipped: **all inert**. Dot-accumulation order x,y,z (fixes the three load-order rows) **drops to 90.69** by re-scheduling the hitPoint chain. Bisected: the drop is that change alone |
| `Clip(Polygon, Ray)` | 512 | 97.61 | FP operand order, plus retail homes a `Vector2` temporary's `.y` to 0x54 that the sculpted `SubV2` helper stands in for. DC3's own build is lower (95.90) | not built; same class as Intersect |
| `FlangerEffect::Process` | 768 | 77.93 | No behaviour-class charge. The same divides, modulos, `fcfid` conversions and loads in a different order | not built; scheduling wall |

DelayEffect and WahEffect are raised, not closed:
- **`DelayEffect::Process` 86.41 → 95.10.**
  - Retail keeps one write-cursor register. It forms the store index (stereo: also `+ kMaxDelaySamps`) before
    incrementing and wrapping the cursor, and saves one fewer GPR (`__savegprlr_29`).
  - Our separate `nextWritePos` carried both. The mono loop is `mBuffer[writePos++] = …; if (writePos >= K) writePos
    = 0;`, and the stereo loop captures `wLeft`/`wRight` before `++writePos`. The integer logic now matches exactly.
  - The rest is FP:
    - which `outRight` product stays standalone: flipping the sum order was inert, and so was swapping the definition
      order of `delayedWet`/`delayedDry`;
    - the `0.5` association: `* wetAmount * 0.5f` was inert, and the source keeps retail's left-associative
      `(inRight + inLeft) * 0.5f * wetAmount`.
- **`WahEffect::Process` 89.28 → 91.76** from defect 1.
  - Retail indexes `buf[(sampleIdx + ch)]` afresh each channel iteration, where ours strength-reduces to a walking
    pointer (`stfsu`).
  - A named per-iteration index was inert. The prologue has FP scheduling rows.

## 5. Measurement

Whole-binary A/B:
`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16po-ab --patch ~/tmp/w16po/branch.patch`, where the patch is
`git diff main w16-po` and its kind is source + map. Two runs, each in a fresh worktree at main.

**Run 1** (main `c8e56eba1`, 12 commits):

| leg | matched | honest | code% |
|---|---:|---:|---:|
| A | 53,511 | 28,318 | 57.929047 |
| B | 53,520 | 28,327 | 57.947000 |

- **Δmatched +9 / Δcode_bytes +1,840**, `none` control +1,880 B. Units at 100 % (mpn ruler) +1 (`DataFile`), 0 fell off.
- **The prediction failed by 40 B.** A row-by-row diff of the two archived reports found **one row down**: `fn_827CD40C`,
  a 40 B funclet in DataPointMgr, fuzzy 100 → 99.5 (mpn 100 on both legs, so the function count did not show it).
  - It is AddPair's own unwind funclet. Retail's calls `??1VarStack@@QAA@XZ`, the survivor of W16-HS's group
    `w16hs_0x82768b18` (five 24-byte DataNode-at-+4 destructors).
  - Our funclet now destroys the `make_pair<Symbol,DataNode>` temporary as `??1?$pair@VSymbol@@VDataNode@@`, the
    non-const pair. That spelling did not exist in our objects before this lane, so no group could have held it.
  - `icf_pair_adjudicate.py --survivor ??1VarStack@@QAA@XZ --ours ??1?$pair@VSymbol@@VDataNode@@...` →
    **FLAT T1 PROVEN** (24 B both, 1 relocation agreeing by name, `our_bodytwins 6` → `retail_bodytwins 1`).
  - Type witness: retail's funclet destroys the same frame slot (+0x70) as ours.
  - Added as the sixth folded spelling (commit "fold the non-const pair<Symbol,DataNode> dtor"). The funclet
    returned to 100, and AddPair stayed at 100.
  - ⇒ A source change that introduces a **new template instantiation** can un-forgive an existing fold group. The
    function count cannot see it, because mpn excludes relocation-name arguments. Only a row diff of the two legs at
    fuzzy shows it.

**Run 2** (main `a3f3339a8` after W16-PT's retarget to clean TU5, 13 commits). Prediction before the run:
**+9 fns / +1,880 B exactly, 0 rows down.**

Both legs ran on the clean-TU5 image (`default.xex` sha1 `d56e7f31…`, the same as main's).

| leg | matched | honest | code% |
|---|---:|---:|---:|
| A | 53,516 | 28,323 | 57.941143 |
| B | 53,525 | 28,332 | 57.959490 |

- **Δmatched +9 / Δhonest +9 / Δcode_bytes +1,880 / Δcode% +0.018347 pp**, Δmasked_equal 0. `none` control
  +1,880 B. **The prediction held exactly.**
- Units at 100 % (mpn ruler) 553 → 554 (`DataFile`), 0 fell off.
- **Row diff of the archived reports (68,909 rows on both legs): 11 up, 0 down.** The 11 are the nine crossings in
  §2 plus DelayEffect and WahEffect.
- Run dir: `~/tmp/wt-w16po-ab2/.ab_measure_runs/20261006-102230-branch2-758970`.

Of the 18 rows, 5,188 B remain open. This lane recorded each remaining row with what was tried and why it stops (§4).

## 6. Not done

- No permuter (deferred by directive). The FP-order rows (Intersect, Clip, Delay's two FP sites, Flanger) are its
  market if it is ever funded.
- Clip and Flanger were not built. They were read and classified only.
- `mLastInput`/`mLastOutput` are still declared, initialised and never written. Removing them would change
  `sizeof(WahEffect)` (0x50), which the retail allocation size fixes, so they stay.
