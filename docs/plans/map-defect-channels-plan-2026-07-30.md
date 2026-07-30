# Plan — map-defect channels (execution plan, 2026-07-30)

> **STATUS (2026-07-30):** PLANNING doc, ref-embedded for cold pickup. Written by
> a read-only review of the BP-4/BP-7 landings against **current `main`**
> (`0d5a71fd`, headline 40,888 / honest proxy 39,379). All four channels were
> RE-RUN read-only this session; the counts below are current, not inherited.
> **Nothing here is applied.** This supersedes the stale premises in the task
> brief (which quoted the BP-4-era numbers 13 phantoms/62+, 8 false-100s,
> 110/85 — all predate the BP-7 drain).

## 0. TL;DR — the brief's premises are half-stale; here is the live picture

The "map-defect channels" finding is real, but **BP-7 already drained the two
cheapest channels** (commit `979a4fd0`, −28 false credit). What actually remains
is one large unworked channel (adjustor-thunks) and one medium decisive one
(StaticClassName open-chains). **Every remaining channel is a CORRECTNESS /
measurement-honesty lever, not a strict-match generator** — none of them yields
net-new real matches, and none touches `masked_equal_functions` (verified: the
deleted/relabelled rows are *name*-paired false-100s, not byte-fallback twins).

| # | channel | detector | CURRENT count | worked so far | remaining actionable | honest-floor direction |
|---|---|---|---|---|---|---|
| 1 | Save/Load stream-direction | `saveload_direction_scan.py` | 17 CONTRADICT / **11 false-100** | BP-4+BP-7 applied ~5 | **≈0 independent** — 8 of 11 false-100 are thunk rows (ch.2), 3 are held PropAnim `Keys` | Δ0 / +1 where port unpaired target |
| 2 | adjustor-thunk permutation | `thunk_name_consistency_scan.py` | 294 MISMATCH / **280 false-100** | BP-4 applied 5 reciprocal pairs (10 entries); BP-7 deferred | **~106 same-class** (~85 body-corroborated); ~188 cross-class = ICF `merged_call`, **not defects** | Δ0 (reciprocal swaps) / −1 (target-unnamed drains) |
| 3 | phantom classes | (string test; no standing script) | **DRAINED** — 34 deletes in BP-7 (−26) | BP-7 part C | **7 held**, all blocked on splits carves (MiniLeaderboardDisplay confirmed still un-carved) | headline DOWN, honesty UP (already banked) |
| 4 | StaticClassName-literal | `staticclassname_literal_scan.py` | **18 CONTRADICT, ALL false-100** | BP-7 applied 5 (2 closed cycles) | **18 open chains** | Δ0 attribution (VA keeps 100% under right name) |

**Current honest floor (this session's dirty tree):** `matched_functions 40890 −
masked_equal_functions 1509 = 39381`; committed baseline `0d5a71fd` = **39,379**
(±2 symbols.txt-drift band — see §7). Fields live under `report.json`→`measures`.

**Net honest yield estimate for the whole plan: ≈0 strict, ~40 labels corrected.**
The value is (a) removing at-100% dishonesty and (b) *unblocking* future real
matches — a correctly-named VA can pair the moment its source lands. Price every
row by `Δ(matched − masked_equal)` and expect it to be 0 or slightly negative;
that is success here, not failure (per [[project-honest-floor-2026-07-29]] §5).

---

## 1. What was confirmed this session (read-only, current `main`)

Re-ran all three standing detectors against `orig/45410914/band.exe` (the
decompressed retail PE, imagebase `0x82000000`) + `build/45410914/report.json` +
the working-tree `scripts/target_symbol_map.json`:

- `python3 scripts/harvest/saveload_direction_scan.py --out ~/tmp/md_saveload.json`
  → 758 rows, **17 CONTRADICT, 11 at false-100**.
- `python3 scripts/harvest/thunk_name_consistency_scan.py --out ~/tmp/md_thunk.json`
  → 1727 rows, **294 MISMATCH, 280 at false-100** (~106 same-class / ~188 cross-class).
- `python3 scripts/harvest/staticclassname_literal_scan.py --out ~/tmp/md_scn.json`
  → 317 rows, **300 AGREE, 18 CONTRADICT (all false-100), 3 NO_SOURCE_LITERAL**.

Map hygiene: duplicate names now **1** (`?NodeCmp@@YAHPBX0@Z`, a legitimate
internal-linkage function on the map's `_internal_linkage_allow` list — NOT a
defect); BP-7 part D resolved the two `StaticClassName` dups. 26,749 VA entries.

⚠ **Caveat on these counts:** `config/45410914/symbols.txt` is **dirty in the
shared tree right now** (`git diff --stat`: 30 insertions / 61 deletions,
another agent). The detectors read it for VA→size, so a clean-baseline re-run may
shift a row or two. Treat the counts as ±2 and re-run on a restored `symbols.txt`
before applying (§7).

Sources this session read in full: `project_map_defect_channels_2026-07-29.md`
(memory), `docs/plans/lane-bp4-map-contradiction-adjudication-2026-07-29.md`,
`docs/plans/lane-bp7-map-ownership-2026-07-29.md`, `project_honest_floor_2026-07-29.md`,
`project_bandexe_read_traps_2026-07-29.md`, `hub_campaign.md`, `hub_measurement.md`.

---

## 2. Channel 4 — StaticClassName-literal (DO THIS FIRST)

**Confirmed count:** 18 CONTRADICT, all at a false 100.0% (BP-7 left 19 open
chains; one has since resolved, 18 remain). Detector:
`scripts/harvest/staticclassname_literal_scan.py` (added BP-7). Output this
session: `~/tmp/md_scn.json`.

**Mechanism.** `OBJ_CLASSNAME(literal)` expands to a `StaticClassName()` that
builds a `Symbol` from the string `"literal"`. Every such body is **identical
machine code except the single relocation that supplies the string pointer**.
objdiff's report path runs `functionRelocDiffs=None` (hardcoded, `report.rs:392`;
see [[project-honest-floor-2026-07-29]] §1), so that field is invisible and
*every* such body scores 100% against *every* other. The whole family can be
arbitrarily scrambled across the map while reading perfectly clean. This is the
at-100% defect class ([[project-correctness-vs-metric-2026-07-29]]) in its
purest form. The detector reads the `.rdata` string the retail body **actually
builds** (walks the `lis`/`addi`|`lis`/`ori` pair) and compares it to the
`OBJ_CLASSNAME` argument the mapped class declares in our own source.

**Worked example (current data).**
`0x8240e0c0` is mapped `?StaticClassName@BaseMaterial@@SA?AVSymbol@@XZ`. The
mapped class `BaseMaterial` declares `OBJ_CLASSNAME(BaseMaterial)` in dc3 source,
so the truth string would be `"BaseMaterial"` — but the retail body at that VA
builds `"Movie"`. So `0x8240e0c0` is a `StaticClassName@Movie`-family body wearing
the `BaseMaterial` name, reading a false 100%. (`BaseMaterial` is itself a phantom
per §4, which is why no repoint was possible in BP-7 — its own name has nowhere
to go.)

**Fix shape — repoint, but only inside a closed permutation cycle.** Because the
Rnd/Dx/Ng renderer split makes several classes share one DTA literal
(`RndCubeTex`, `DxCubeTex`, … all `OBJ_CLASSNAME(CubeTex)`), literal→class is
**one-to-many** and a bare repoint can steal a name a *correct* AGREE row needs.
BP-7 applied only closed cycles (`0x82739158→0x82738F08→0x827382F8` DxMesh/
DxEnviron/DxCam; `0x82739208↔0x82739288` DxMovie/DxCubeTex). **Fix work here =**
(a) build the directed graph literal→(VA that should hold it), (b) extract the
cycles, (c) for each cycle member confirm the proposed name has a real COMDAT in
the obj of the unit owning the VA, (d) apply via `map_repoint_apply.py`. Open
chains that terminate on an AGREE VA need that AGREE row re-examined first (BP-7's
explicit worklist note).

**Honest-floor price:** **Δ0, metric-neutral by construction** — each VA keeps its
100%, under the correct name. This is a pure attribution/correctness fix. It is
**not a mirage** (a mirage only churns `masked_equal`; this moves neither matched
nor masked_equal and permanently corrects the label). Highest value-per-risk of
any channel.

**Risks:** (1) applying a non-cycle open chain breaks a correct pairing — **only
apply closed cycles**; (2) some contradicts point at phantom classes (§4) whose
name has nowhere to go — leave those to the phantom lane; (3) the `bl`-to-
absolutes trap does not bite here (the discriminator is a string, not a branch),
but see §6 for the general rule.

---

## 3. Channel 2 — adjustor-thunk permutation (LARGEST remaining; do SECOND)

**Confirmed count:** 294 MISMATCH, 280 at false-100. Partition (this session):
**~106 same-class / ~188 cross-class**. Detector:
`scripts/harvest/thunk_name_consistency_scan.py`. Output: `~/tmp/md_thunk.json`.

**Mechanism.** An MSVC `this`-adjustor thunk is 3 instructions and no logic:
`lwz r11,-4(r3)` / `subf r3,r11,r3` / `b BODY` (or `addi r3,r3,-N ; b BODY`). Its
mangled name is **fully determined** by its jump target's name — so the check is
*self-consistent*, no oracle: even if BODY's own name is wrong, the thunk must
agree with it. A disagreement is an internal contradiction. Both a thunk and every
other 3-instruction thunk with the same displacement are byte-identical, so under
`functionRelocDiffs=None` a misnamed thunk reads a clean false 100%.

**The 188 cross-class are NOT defects — do not touch them.** They are mostly
**ICF-hub `merged_call` artifacts**: the real body folded with an identical body
elsewhere and the map labelled the folded address with the other alias; three
target VAs receive 21/19/17 thunks (unmistakable ICF-hub signature). The thunk
name there is arguably *correct*. Only the **~106 same-class** rows are genuine
intra-class permutations.

**Worked example (BP-4's motivating case, still live).** ScoreDisplay's thunk
run is displaced by one slot: `0x82320390` is mapped
`?Load@ScoreDisplay@@$4...` but tail-jumps to `0x8231fee0 =
?Save@ScoreDisplay@@` — i.e. the thunk carries its neighbour's method name.
(This same row surfaces in the saveload scan at false-100 — see §5, it is a thunk
row, not an independent saveload defect.) It reads 100% because the 3-instruction
body is displacement-identical to the correct thunk.

**Fix shape, two tiers:**
- **Reciprocal pairs (safe, Δ0, no tooling):** where two thunks in one class each
  carry the other's name, the correct names *already exist* at each other's VAs —
  a straight swap via `map_repoint_apply.py`, no name synthesis. BP-4 applied 5
  such pairs (TexMovie, FreestylePanel, RndCamAnim, CharBonesBlender, CamShot,
  Spotlight, WorldInstance, CharPosConstraint, ScoreDisplay were named as having
  reciprocal pairs). Re-enumerate the reciprocal subset from `~/tmp/md_thunk.json`
  and apply.
- **Body-corroborated singletons (needs a small applier):** for **~85** of the
  106, retail's body size at the target VA equals our COMDAT size for the name the
  map gives that VA, so the *body* names are corroborated and the *thunk* names
  are wrong. The fix is to **synthesize the correct thunk mangling** =
  `<body method+class qualifier>` + `$<displacement tag>` + `<signature>`,
  reusing the incumbent thunk's `$`-tag (so the swap stays inside one sub-object).
  The detector already emits a `suggest` field (naive `name.replace(sq,tq,1)`);
  harden it into a real mangling rebuild before trusting it. The remaining ~21 are
  genuine source divergences (e.g. `?Copy@UIComponent@@` 148B ours vs 500B retail)
  — **not** naming errors; leave them.

**Honest-floor price:** reciprocal swaps **Δ0** (both bodies present, names trade).
Body-corroborated singletons: **Δ0** where the synthesized name has a COMDAT;
**−1** each where the correct thunk name has no COMDAT (drains a false-100 — a
deliberate honesty gain, price it as such). Expect net headline 0 to slightly
negative, ~85 labels corrected.

**Risks:** (1) misclassifying an ICF-hub cross-class row as a defect — **gate on
same-class only**; (2) a wrong `$`-tag reuse puts the thunk in the wrong sub-object
— assert the tag is byte-identical before/after; (3) name-synthesis bugs — dry-run
the applier and diff the map line before landing.

---

## 4. Channel 3 — phantom classes (ALREADY DRAINED; residual is carve-gated)

**Confirmed state:** **DRAINED** by BP-7 part C (34 deletes, measured −26 matched,
masked_equal 0). Corrected census from BP-7: **12 phantoms / 41 poisoned entries**
(not BP-4's 13/62+). Two corrections landed: **RndSpline is NOT a phantom** (it
declares `OBJ_CLASSNAME(Spline)`; the literal `"Spline"` occurs at control density
— the whole point of building Channel 4's real-argument parse), and 11 of BP-4's
"62+" were substring false-positives (phantom class as a parameter/template arg).
Root cause was **DC3-exclusive leakage** (10 of 12 from dc3-decomp: `flow/`,
`rndobj/{BaseMaterial,MetaMaterial}`, `char/ClipCollide`, Kinect `gesture/`,
`hamobj/DancerSequence`), only 2 from the rb3-Wii DEV oracle.

**Residual = 7 held repoints, all blocked on a splits carve.** Confirmed this
session: `0x82319c48` (`?SetType@…`, the MiniLeaderboardDisplay triplet from
BP-4 §9 / BP-3) **still sits inside `MetaPanel.cpp`'s `.text` span
`0x82319c40–0x82319f74`**. `system/hamobj/MiniLeaderboardDisplay.cpp` **is** wired
in `objects.json:947` (NonMatching) but has **no `splits.txt` `.text` pin** — it
is compile-only scaffold, so its COMDATs pair to nothing. A bare repoint reads 0%.

**Fix shape:** a **paired splits+map change** — carve the MiniLeaderboardDisplay
cluster out of MetaPanel.cpp's mega-unit (pin a `.text` range for
`system/hamobj/MiniLeaderboardDisplay.cpp`), rebuild so its COMDATs pair, *then*
repoint the triplet (`0x82319950` StaticClassName / `0x82319a50` ClassName /
`0x82319c48` SetType) to MiniLeaderboardDisplay. This is a splits lane, out of
scope for a pure-map lane, and **lower priority** than §2/§4.

**Honest-floor price:** the drain is already banked (−26). The 7 held repoints, if
unblocked by the carve, could be **+several** real matches (they'd pair a
correctly-named body that currently reads 0%) — the only place in this whole plan
with net-positive strict potential, and it's gated on splits work.

**Risks:** carving a mega-unit is the riskiest edit here (splits churn, ownership
seams). Do it as its own lane with a full A/B; do not fold it into a map-only wave.

---

## 5. Channel 1 — Save/Load stream-direction (effectively DRAINED)

**Confirmed count:** 17 CONTRADICT, 11 at false-100. Detector:
`scripts/harvest/saveload_direction_scan.py`. Output: `~/tmp/md_saveload.json`.

**Mechanism.** A Milo `Save(BinStream&)` and its `Load(BinStream&)` compile to the
*same* skeleton (`r3=stream, r4=&member, bl <operator>`), differing only in which
stream operator is called: `??6`=`operator<<`=WRITE=Save, `??5`=`operator>>`=
READ=Load. Under `functionRelocDiffs=None` the call target is invisible, so a Load
body wearing a `?Save@…` name reads a clean false 100%. The detector resolves each
`bl` through the map and reads the callee direction. **This channel is where the
`bl`-to-absolutes discipline was born** (§6).

**Why it is effectively drained.** Of the 11 false-100 CONTRADICTs, **8 are
adjustor-thunks** (they carry `$4...` and belong to Channel 2 — ScoreDisplay,
BandList, PracticeSection×2, RndTransAnim, HamMove, UIFontImporter, Screenshot),
and the other **3 are the PropAnim `Keys` family** (`0x8242aa00` ColorKeys,
`0x8242b7a8` QuatKeys, `0x8242e3e8` ColorKeys) which is a **held cluster-shift**:
a bare method flip is structurally impossible there (`Load` takes `BinStreamRev`,
`Save` takes `BinStream`, so a flipped signature has no COMDAT), and BP-4 §4 /
BP-7 §2 both showed the whole run must move as a block with an RTTI/vtable class
oracle. The low-percent CONTRADICTs (BandHighlight 0%, StreakMeter, etc.) are
source divergences, not map defects.

**Worked example.** The founding anchor `0x82690b28` (mapped
`?Save@SetUserDifficultyMsg@@`, body calls `??5`×2 = a Load) was **already fixed**
in BP-7 part A (repointed to `?Load@SetUserDifficultyMsg@@`, class half preserved
as a documented ICF-hub arbitrary). Nothing of that shape remains open.

**Fix shape / price:** the only remaining independent work is the **PropAnim Keys
cluster**, and it needs the RTTI route (read each Keys class's vtable Complete
Object Locator, take the Save/Load slots). Priced Δ0 (method+class relabel of
already-100% bodies) — **lowest priority**, hardest oracle, do last if at all.

**Risk:** the Keys map is self-inconsistent (BP-4 §4: `0x8242E210` mapped
`Key<Vector3>` calls the `Color` per-element reader), so any partial application
manufactures the exact mispairs this channel exists to find. All-or-nothing, with
the class oracle first.

---

## 6. The `bl`-to-absolutes trap (bake into every adjudication)

**What it means operationally.** When you compare two retail bodies to decide "are
these the same function (a real ICF fold / twin)?", you must first neutralize the
PC-relative branch fields — but **how** you neutralize them changes the verdict:

- **MASKING** the `bl` displacement bytes (zeroing them) makes "identical code the
  linker failed to fold" and "same skeleton, *different callee*" look **identical**
  — because two copies of the same function at different addresses have different
  `bl` bytes *purely* because the field is PC-relative, and masking also hides a
  genuinely different callee. BP-4's first cut got **119 "twins"** this way.
- **RESOLVING** each `bl` to its **absolute target VA** (leaving every other byte
  exact — `norm_body` in `icf_contradiction_adjudicate.py`) makes normalized-equal
  mean "calls the same things and holds the same immediates" = a true twin.
  Re-running flipped **118 of 119** to "retail did NOT fold these."

So masking **inverts** the fold adjudication for exactly the SKELETON_ONLY class.
**Ordering requirement, mandatory in any fold/twin/ICF analysis in this plan:
resolve every branch field to its absolute target BEFORE adjudicating identity.
Never mask branch bytes.** (Channels 4 and 1's method test are branch-*reading*,
not fold-adjudicating, so they are safe — but any future "are these two VAs the
same body?" step must obey this.) Ref: `project_map_defect_channels_2026-07-29.md`
§"MEASUREMENT TRAP"; `lane-bp4-…md` §1.

---

## 7. Validation protocol (mandatory, every leg)

Per [[project-honest-floor-2026-07-29]], [[hub-measurement]], and
[[project-bandexe-read-traps-2026-07-29]]:

1. **Full matched-SET A/B, never a raw count.** Quote
   `Δ(matched_functions − masked_equal_functions)` from `report.json`→`measures`.
   A landing whose honest Δ ≈ 0 is *expected* here (correctness fix) — do not
   discard it for that reason, but **do** discard anything that moves
   `masked_equal_functions` (that would be a byte-fallback churn mirage).
2. **`symbols.txt` restore on BOTH legs.** `config/45410914/symbols.txt` is both a
   dtk input and a dtk-regenerated output; feeding the drifted copy into the next
   split silently moves ~5 functions. **`git checkout -- config/45410914/symbols.txt`
   before every split-forcing build, on both A and B legs.** (It is dirty in the
   shared tree right now — restore in your *worktree*, never touch the shared
   tree's copy; see §8.) Ref: `project_bandexe_read_traps_2026-07-29.md` §3-ter.
3. **Same split-state both legs** — the split-churn floor (~2 fns) only cancels if
   both legs are measured post-`touch config.yml` + full build. An absolute count
   is **not portable across worktrees** (±2 band); quote deltas only.
4. **Cache hygiene for map edits.** A `target_symbol_map.json` edit is consumed by
   the pre-compile `obj_target_symbol_renamer` step, which re-runs when
   `config.json` mtime advances. So: `rm -f build/45410914/renames.stamp
   build/45410914/report.cache` and `touch config/45410914/config.yml` before the
   rebuild, so the renamer actually re-applies and the report is regenerated fresh
   (stale `report.cache` has produced false +110 before —
   [[report-cache-stale-ab]]).
5. **band.exe reads:** `.text` is RVA `0x270000` / raw `0x264E00` (delta `0xB200`);
   `va − 0x82000000` is valid **only for `.rdata`**. Assert the anchor
   `off(0x824DAAD0) == 0x004CF8D0` before any `.text` read (the standing detectors
   already do, via `icf_contradiction_adjudicate.PE`). Ref:
   `project_bandexe_read_traps_2026-07-29.md` §1.
6. **`bl`-to-absolutes before any fold adjudication** (§6) — never mask branches.

---

## 8. Staging & landing discipline (shared tree)

- **Work in a worktree**, never the shared main tree, for any build/measure:
  `scripts/setup_worktree.sh ~/tmp/wt-mapdefect <branch>`. Logs to
  `~/tmp/rb3_build_mapdefect.log`. The shared `symbols.txt` is dirty — do not
  `git checkout`/`restore`/`stash` it in main (CLAUDE.md hard rule); restore only
  inside your worktree.
- **One owner of `scripts/target_symbol_map.json` per wave.** BP-4 and BP-7 were
  each sole owner; concurrent map edits will collide.
- **Appliers (all line-surgical, never `json.dump` the map):**
  - `scripts/harvest/map_repoint_apply.py` — repoint/swap; asserts `old` (fails
    loudly on a stale fragment), requires a per-row `why`, asserts the duplicate
    *delta* (introduces no NEW dup).
  - `scripts/harvest/map_row_delete.py` — delete; asserts `old`, per-row `why`,
    asserts post-condition key count.
  - `scripts/harvest/map_flag_arbitrary.py` — appends to the list-valued metadata
    keys (`_bijection_arbitrary` etc.) that `map_repoint_apply` cannot touch.
  - **Constraint:** one name per VA is hard (`obj_target_symbol_renamer` does
    `renames[name].encode`); ICF aliasing is representable only in the metadata
    lists, never as a per-VA name list.
- **Order within a wave:** deletes before repoints (a repoint into a name freed by
  a delete trips the collision assert otherwise — BP-7 §6 hit this).
- **Fragment files** with per-row justification, one per part, e.g.
  `docs/plans/lane-<id>-partX-*.json` (follow BP-7's naming). Land **path-limited**
  commits (`git add` only the map + the fragment + this-style doc); never `amend`
  (shared `.git/index`, [[project-shared-index-commit-race]]).

---

## 9. Recommended execution order & why

1. **Channel 4 — StaticClassName closed cycles** (18 open contradicts). Highest
   value-per-risk: oracle-free decisive discriminator, Δ0 metric-neutral, closes
   at-100% dishonesty with zero headline risk. Build the cycle-extraction +
   AGREE-cross-check, apply cycles only.
2. **Channel 2 — thunk reciprocal pairs** (safe subset of ~106 same-class). Δ0, no
   name synthesis (names already exist at each other's VAs). Enumerate reciprocals
   from `~/tmp/md_thunk.json`, apply.
3. **Channel 2 — thunk body-corroborated singletons** (~85). Needs a hardened
   name-synthesis applier; Δ0 or −1 per row (honesty drain). Do after (2) proves
   the tooling.
4. **Channel 3 residual — MiniLeaderboardDisplay carve** (7 held). Its own
   **splits** lane (paired splits+map, full A/B). The only net-positive-strict
   opportunity here, but gated on the riskiest edit — schedule separately.
5. **Channel 1 — PropAnim Keys cluster** (3 held). Last, needs the RTTI/vtable
   class oracle; Δ0, hardest, easiest to make worse.

**Total honest yield estimate:** ~0 net strict, ~40 labels corrected across
1/2/4; potential **+several** strict from the §4 carve if that lane runs. The
headline may tick **down** a few where a body-corroborated thunk or open chain
drains a false-100 without a repoint target — that is honesty improving, priced
by `Δ(matched − masked_equal)`, and is the intended outcome.

**Top risk:** applying a **non-cycle** StaticClassName repoint (or a cross-class
thunk row) — because literal→class and folded-target→name are one-to-many, a bare
repoint steals a name a *correct* AGREE/OK row needs and manufactures a fresh
mispair. Gate strictly on closed cycles / same-class reciprocity, and obey the
`bl`-to-absolutes ordering (§6) in any fold step.

---

## 10. References (cold-pickup)

- Memory: `project_map_defect_channels_2026-07-29.md` (the finding, with BP-7
  corrections); `project_honest_floor_2026-07-29.md` (pricing rule §5,
  `functionRelocDiffs=None` at `report.rs:392`); `project_bandexe_read_traps_2026-07-29.md`
  (VA→offset, split-churn/symbols.txt drift §3-ter, at-100% NewAwardPanel);
  `hub_measurement.md`, `hub_campaign.md`.
- Docs: `docs/plans/lane-bp4-map-contradiction-adjudication-2026-07-29.md` (channels
  discovered; §1 masking trap; §4 Keys; §5 thunks; §6 phantoms; §9 MiniLeaderboard);
  `docs/plans/lane-bp7-map-ownership-2026-07-29.md` (drain landed; §3 RndSpline/41
  corrections; §4 StaticClassName channel; §0 pricing note).
- Fragments (landed, as models): `docs/plans/lane-bp7-part{AB,C,D,E,F-*}-*.json`.
- Detectors: `scripts/harvest/saveload_direction_scan.py`,
  `thunk_name_consistency_scan.py`, `staticclassname_literal_scan.py`,
  `icf_contradiction_adjudicate.py` (PE reader + anchor); appliers
  `map_repoint_apply.py`, `map_row_delete.py`, `map_flag_arbitrary.py`.
- This session's read-only outputs: `~/tmp/md_saveload.json`, `~/tmp/md_thunk.json`,
  `~/tmp/md_scn.json`.
- Commits: `7e189706` (BP-4 landing), `979a4fd0` (BP-7+BP-6 drain −28), `ef8c02fe`
  (state 40,857/39,348), later `1b7089e2` BP-8 (+23), `42fe0db0` BQ-2 (+8),
  `0d5a71fd` (current state 40,888 / **39,379**).
