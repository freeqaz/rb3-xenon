# W16-AI — `GoalCmp`'s local static, two UIPanel vtordisp thunks, and a misplaced TU boundary

Lane W16-AI, 2026-09-14. Branch `w16-ai`, based on main `256634c7`.
Commits: `d08b7ffd` (item 1, source), `3dc1148d` (item 2, map + splits).

**Result: +424 B / +5 matched functions, predicted exactly, zero fall-outs.**
`CampaignGoalsLeaderboardChoicePanel` went **44/50 rows, 5,284/5,756 B → 47/48 rows,
5,492/5,540 B**. Two rows (48 B) stand between it and a unit completion; both are the same
ICF fold-alias problem and both are deferred with a precise, addressable reason (§5).

---

## 0. Baseline

Full build rc=0 in a fresh worktree, then `rowset_snapshot.py diff ~/tmp/rows_w16af_main.json`
returned **CROSSED IN 0 / FELL OUT 0 / NET +0**, reproducing the brief's stated baseline of
**43,351 fns / 3,996,676 B** to the digit. Every number below is a delta against that.

---

## 1. Item 1 — `??RGoalCmp@@QBA_NVSymbol@@0@Z`: the oracle is wrong and retail is right

W16-AF §6 sized this and left it as source work. The brief expected the rb3-Wii oracle to
show the `static Symbol` spelling.

**It does not, and that is the most useful thing this lane learned.**
`../rb3/src/band3/meta_band/CampaignGoalsLeaderboardChoicePanel.cpp:59` uses the **extern
global** `campaign_metascore` — character-identical to what we already had. Our source was a
faithful port of the oracle; the **oracle** is what diverges from retail. A lane that had
"ported from the oracle, confirmed identical" and stopped would have concluded the row was
unfixable. This is the oracle-fidelity rule earning its keep: **retail bytes outrank the
oracle**, always.

So I re-adjudicated on the retail image independently of AF, and AF's reading is confirmed:

| element | retail evidence | meaning |
|---|---|---|
| `lwz r11, lbl_82E00398@l(r10)` / `clrlwi. r9,r11,31` / `ori r11,r11,1` / `stw` | `0x82E00398`, zero-init tail of `.data` | MSVC init guard, bit 0 |
| storage `lbl_82E00394`, 4 B | zero-init tail of `.data` | the `Symbol` (one pointer) |
| `addi r4, r11, lbl_820A32E8@l` then `bl fn_827C0728` | literal reads exactly `campaign_metascore`; callee is `??0Symbol@@QAA@PBD@Z` | ctor from a string literal |
| `fn_825F3358`, 32 B | tests/clears the same guard word | guard-rollback EH funclet |

⚠ **Method note worth reusing:** my first read of the literal used the **`.text`**
file-offset delta on an **`.rdata`** address and produced a plausible *wrong* string
(`Without A Signed In Gamer Profile`). Parsing the PE section table properly gave
`campaign_metascore`. A wrong-section delta does not error — it returns a real string from
the wrong place, which is exactly the shape of a confident false confirmation.

**The edit** (`src/band3/meta_band/CampaignGoalsLeaderboardChoicePanel.cpp`): replace the
extern global with `static Symbol campaign_metascore("campaign_metascore");` as the first
statement of the function, shadowing the extern.

**Pre-registered before building:** +176 B / +1 fn for `??RGoalCmp@@QBA_NVSymbol@@0@Z`, with
a stated risk band of **+144..+176** — `fn_825F3358` is at `fuzzy==100` today while *unnamed*,
i.e. it pairs by funclet byte-signature, and our object gaining a real guard funclet could
have perturbed that pairing.

**Measured** (full build rc=0, `fuzzy==100` set-diff): **+176 B / +1 fn, 0 fall-outs.**
Prediction exact; the risk did not materialise.

---

## 2. Item 2a/2b — two UIPanel vtordisp adjustor thunks (`fn_825F4258`, `fn_825F4268`)

Retail bodies are `lwz r11,-0x4(r3); subf r3,r11,r3; subi r3,r3,0x1c; b <target>` — the
`-0x4(r3)` load is the vtordisp, so these are `$4`-form adjustor thunks. Tail-call targets
resolve to `?DataDir@UIPanel@@UAAPAVObjectDir@@XZ` and
`?SetTypeDef@UIPanel@@UAAXPAVDataArray@@@Z`.

Decoding MSVC's `$4PPPPPPPM@<adj>@` mangling: `BM@` = 1×16+12 = 28 = **0x1c**, matching the
observed `subi` constant exactly. Four independent agreements:

1. tail-call target names the method;
2. the mangled adjustment equals the `subi` immediate;
3. our object defines both names at exactly **16 B**, the retail size;
4. **adjacency** — `?Load@UIPanel@@$4PPPPPPPM@BM@AAXAAVBinStream@@@Z` was *already* mapped at
   `0x825F4248` and *already* matched, so the three `BM@` thunks are contiguous at
   `0x825F4248 / 4258 / 4268`.

Neither name was assigned anywhere else in the map. **Predicted +16 B / +1 fn each; measured
+16 B / +1 fn each.**

---

## 3. Item 2c — `fn_825F57C0` / `fn_825F5854` are not this TU's

These are `??0CampaignSongInfoPanel@@QAA@XZ` (148 B) and its unwind funclet (68 B), sitting
inside a **pre-existing** (not W16-AF) CampaignGoals pin `0x825F5700–0x825F5898`.

Proven four ways, all on retail bytes:

1. **RTTI.** Both vtables the ctor stores (`0x820BB8EC`, `0x820BB894`) carry Complete Object
   Locators whose type descriptors read `.?AVCampaignSongInfoPanel@@`. The *already-matched*
   `??0CampaignGoalsLeaderboardChoicePanel@@QAA@XZ` at `0x825F4130` stores three vtables that
   all read `.?AVCampaignGoalsLeaderboardChoicePanel@@`.
2. **Structure.** `fn_825F57C0` calls `??0UIPanel@@QAA@XZ` **directly**, with its
   `Hmx::Object` virtual base at `+0x44` and zeroing `+0x3c`. The CampaignGoals ctor goes
   through `??0TexLoadPanel@@QAA@XZ`, has its vbase at `+0x5c`, stores a third vtable at
   `+0x3c`, and zeroes `+0x54` (= `mCampaignGoalsLeaderboardChoiceProvider`). Different
   classes, not a variant of one.
3. **Size.** Our `??0CampaignSongInfoPanel@@QAA@XZ` COMDAT is **224 B = 148 + 68 + 8**
   (ctor + funclet + EH prefix), exact.
4. **Geometry.** `CampaignSongInfoPanel.cpp`'s own first `.text` block already began at
   `0x825F5898` — *exactly* where the CampaignGoals block ended. The boundary was misplaced
   by precisely this ctor+funclet pair.

Then dtk agreed independently: re-deriving `.pdata` from my `.text` edit, it carved
`0x822270D0–0x822270E0` — **16 B = exactly two unwind entries** — for the moved block,
contiguous with CampaignSongInfoPanel's existing first `.pdata` at `0x822270E0`.

**Why re-homing and not naming in place.** objdiff pairs target↔base **by name within a
unit**, and CampaignGoals' object does not define `??0CampaignSongInfoPanel@@QAA@XZ`.
Naming it in place would have produced a correct name scoring 0% forever — the trap W16-AF §4
records and CLAUDE.md warns about.

**Predicted +216 B / +2 fns, with an honest band of 0..+216** because it depended on our ctor
being byte-exact, which I could not know in advance. **Measured +216 B / +2 fns** — the top of
the band, so our ctor is byte-exact. The funclet `fn_825F5854` paired with **no map entry**,
by funclet byte-signature, as expected.

The first build after the splits edit failed the split-guard ("THE SPLIT REWROTE ITS OWN
INPUT") — documented behaviour. I verified by `git diff` that every rewritten line was the
`.pdata` re-derivation of my own `.text` change, then retried to rc=0.

---

## 4. Predicted vs measured

| item | row(s) | predicted | measured |
|---|---|---|---|
| 1 | `??RGoalCmp@@QBA_NVSymbol@@0@Z` | +176 B / +1 fn (band +144..+176) | **+176 B / +1 fn** |
| 2a | `?DataDir@UIPanel@@$4PPPPPPPM@BM@AAPAVObjectDir@@XZ` | +16 B / +1 fn | **+16 B / +1 fn** |
| 2b | `?SetTypeDef@UIPanel@@$4PPPPPPPM@BM@AAXPAVDataArray@@@Z` | +16 B / +1 fn | **+16 B / +1 fn** |
| 2c | `??0CampaignSongInfoPanel@@QAA@XZ` + `fn_825F5854` | +216 B / +2 fns (band 0..+216) | **+216 B / +2 fns** |
| | **total** | **+424 B / +5 fns** | **+424 B / +5 fns, 0 fall-outs** |

```
CROSSED IN : 5 rows, 424 B      FELL OUT : 0 rows, 0 B
  matched_functions      43351 -> 43356     delta +5
  matched_code           3996676 -> 3997100 delta +424
  matched_code_percent   39.00735 -> 39.01149
  fuzzy_match_percent    49.54272 -> 49.54592
```

Unit effect: `CampaignGoalsLeaderboardChoicePanel` **44/50 → 47/48 rows**, **5,284/5,756 →
5,492/5,540 B**. `CampaignSongInfoPanel` **35/52 → 37/54 rows**, **2,128/4,908 →
2,344/5,124 B** (its denominator legitimately grew by the 216 B it should always have owned).

---

## 5. NOT done, with reasons — the Fable escalation input

**Two rows, 48 B, keep the unit off a completion. Both are the same problem: an ICF
fold-alias whose survivor name is not our spelling. Neither is a source defect I could
locate, and neither is safe to "fix" by fabricating an alias.**

### 5.1 `fn_825F5244` (40 B, `fuzzy 99.50`, `mpn 100.0`, `masked_equal: true`)

The body is equal; the single charge is a relocation **name** at argument level (which is why
`mpn` reads 100 while `matched_code` withholds all 40 B).

- It is the unwind funclet of `??0CampaignGoalsLeaderboardChoiceProvider@@QAA@...@Z` at
  `0x825F50A0`, **which is itself matched at `fuzzy==100`**.
- Retail's funclet does `bl 0x826101B8`, which the map names
  `??1?$map@HPAVUIComponent@@U?$less@H@...@Z` — a `map<int,UIComponent*>` destructor.
- Our TU's corresponding local is `std::hash_map<Symbol,int> lbData`, whose destructor
  symbol our object *does* define.
- **Both are 4-byte tail-call thunks.** Ours is `48000000` (`b` + relocation); retail's is
  `4bffff40` (`b -0xC0` → `0x826100F8`). Relocation-normalized they are **identical**.
- Ours branches to `??1?$hashtable@U?$pair@$$CBVSymbol@@H@...@Z`.
  **Retail's branches to `0x826100F8`, which is ABSENT from `target_symbol_map.json`.**

⛔ **This is why I did not act.** MSVC folds only COMDATs identical *including relocations*,
so two 4-byte thunks branching to **different** functions would **not** fold. Therefore
exactly one of these is true, and the retail bytes as they stand do not decide which:

- **(a) the map's name for `0x826101B8` is wrong** — it is really the `hash_map<Symbol,int>`
  dtor thunk (or a fold survivor whose arbitrary surviving name is the `map` spelling), in
  which case this is a map-identification repair worth 40 B; or
- **(b) the map's name is right**, and our Provider ctor genuinely destroys a different
  container than retail's — a real behavioural divergence, notwithstanding that the parent
  scores 100 (a parent's `bl` into a *funclet* is not what `mpn` charges).

**The decisive next step is one identification: what is `0x826100F8`?** If it is
`??1?$hashtable@...` then (a) holds and the map row is wrong; if it is an `_Rb_tree`
destructor then (b) holds and the divergence is real and behavioural. Per MAPID-1, naming an
anonymous address pays in **bug exposure, not bytes** — which is precisely the value here.

I deliberately did **not** add a `symbol_aliases.json` membership. The brief gates that on a
fold proven on retail bytes, and I could not prove it — retail's branch destination is
unidentified. Per CLAUDE.md an unproven alias lifts `name_check` **by construction**, and the
`none`-ruler control **cannot** catch a fabricated one, so a green A/B here would have been
the hazard's signature, not a clearance.

### 5.2 `fn_825F4288` (8 B, `fuzzy 0`)

`subi r3,r3,0x3c; b ??_GTourDescPanel@@UAAPAXI@Z` — a `WDM@` this-adjustor thunk
(`DM@` = 3×16+12 = 60 = `0x3c`, matching the `subi`). Our object defines **two** 8-byte
`WDM@` candidates: `??_ECampaignGoalsLeaderboardChoicePanel@@WDM@AAPAXI@Z` and
`??_ETexLoadPanel@@WDM@AAPAXI@Z`. The retail branch target is *TourDescPanel's* `??_G`,
i.e. the surviving name of an ICF fold across at least three classes whose `??_G` bodies are
identical. **Which name the call site meant was destroyed by the fold**, and nothing in the
retail bytes distinguishes our two candidates. At 8 B the expected value of a guess is
negative — a fabricated identification is an integrity cost that outlives the byte. Left
anonymous, deliberately.

### 5.3 Other things not done

- **No `symbol_aliases.json` edit at all** (see 5.1) — no fold was proven, so there was
  nothing to record and no `withdrawn`.
- **No other unit opened.** Item 3 of the brief (AF's other deferred items) was not reached:
  AF's §7 residue is the 43 refuted alias memberships, which it explicitly assigns to lane
  W16-AG, plus artifacts that are deliberately not committed. Nothing in AF §7 is a `src/` or
  map item in this unit or `AccomplishmentPanel`, so item 3 had no in-scope work.
- **`CampaignSongInfoPanel`'s other 17 sub-100 rows not touched** — out of lane scope; the
  boundary fix simply gives that unit the 216 B it should always have owned.
- **jeff / objdiff / wibo not rebuilt.**

---

## 6. Gates

Run in the brief's order, in the worktree, after the last source edit:

```
./tools/ninja-locked                                      BUILD rc=0
python3 scripts/verify_ruler_agreement.py --check          rc=0
python3 scripts/verify_objs_patched.py --verify-manifest   rc=0
tools/native_build_gate.sh
```

The native gate is **mandatory for this lane** — item 1 edits `src/band3/`.

```
BUILD rc=0
python3 scripts/verify_ruler_agreement.py --check          rc=0
  OK  functionRelocDiffs = name_check
  OK  combineDataSections = true
  OK  combineTextSections = true
  OK  ppc.calculatePoolRelocations = false
  OK: both objdiff-cli entry points resolve the same ruler.
python3 scripts/verify_objs_patched.py --verify-manifest   rc=0
  [patch-state] OK: 1213 decomp, 3115 target objects match
                2026-09-14T18:46:30Z (tree_sha256=821d066705d4b77e)
```

`tools/native_build_gate.sh`, run as the last action:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

Lane-internal before/after, whole-binary, ruler `name_check`:

| | matched_functions | matched_code |
|---|---|---|
| before (`256634c7`) | 43,351 | 3,996,676 B |
| after (`3dc1148d`) | **43,356** | **3,997,100 B** |
| delta | **+5** | **+424 B** |

`total_functions` 69,216 / `total_code` 10,245,956 B at measurement time.
