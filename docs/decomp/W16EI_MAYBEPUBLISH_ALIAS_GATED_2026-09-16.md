# W16-EI — `UIStats::MaybePublish` is ALIAS-GATED, not source-gated (vein closed)

**Row:** `?MaybePublish@UIStats@@QAAXPAVUIScreen@@@Z`, 2,604 B, unit
`default/band3/meta_band/UIStats`. **Tree:** `ec15a785`, worktree
`~/tmp/wt-w16-ei`, branch `w16-ei`. **Ruler:** graded `name_check`, resolved from
`report.json`'s own `provenance.diff_config` (22 keys) — labelled on every run.

## Verdict

**This row cannot reach `fuzzy == 100` by any source edit, and therefore its
2,604 B cannot be collected by source work.** Two of its charges name ICF fold
*survivors*; which name the retail map holds for an address is not a property
this file can change. The bytes are gated on installing two alias memberships,
and even with both installed **63 further charges remain**.

Recommendation: **do not re-brief this row as a source-matching target.** It has
now consumed four prior attempts (Opus/DP-2, Fable, Sonnet — which regressed it
−1.2 pp — and one more) plus this lane.

## The briefed figure, verified literally

The brief's numbers reproduce to the last digit in this worktree after a full
`./tools/ninja-locked` (rc=0):

| key | briefed | measured here |
|---|---|---|
| `size` | 2,604 | 2,604 |
| `fuzzy_match_percent` | 99.57911 | 99.57911 |
| `match_percent_normalized` | 99.6559 | 99.655914 |

The `splits.txt` heading is single and path-qualified (`band3/meta_band/UIStats.cpp:`,
three `.text` blocks) — the `b341d7ab` double-heading defect is genuinely gone,
not merely assumed gone.

## Charged-site classification (the deliverable)

65 of 652 instructions are charged. `diagnose` and `mismatches` disagree on the
composition and **`mismatches` is the incomplete one** — it reported "65, all
`diff_arg`", while `diagnose` on the same ruler reports 63 `diff_arg` **+ 1
insert + 1 replace**. A mode that lists one type is not a census.

| class | count | can source close it? |
|---|---:|---|
| offset / immediate (stack slots) | 54 | only by reproducing MSVC temp-slot allocation exactly |
| register | 10 | no — permuter class, permuter OFF by directive |
| symbol, `lbl_<hex>` placeholder target | 6 | **not charged** (forgiven) — only their register half counts |
| symbol, genuinely charged | **2** | **NO — structurally impossible** |
| insert (idx 523) | 1 | real shape difference |
| replace (idx 530) | 1 | real shape difference |

### The two blocking charges

**idx 246** — target `bl ?GetContainerName@MemcardXbox@@UAAPBDXZ`; we emit
`bl ?GetBandUsers@BandUserMgr@@QAAAAV?$vector@PAVBandUser@@...@XZ`.
`target_symbol_map.json` places `GetContainerName@MemcardXbox` at **`0x82801f78`**,
and `symbol_aliases.json` *already carries a group at that exact address*
(survivor `GetContainerName@MemcardXbox`, folded `GetColor@UIColor`, evidence
class `ourside_comdat_identity`). All three spellings are one-line
member-address getters compiling to `addi r3,r3,N; blr` — which is exactly what
`std::vector<BandUser*>& BandUserMgr::GetBandUsers() { return mUsers; }`
(`src/band3/game/BandUserMgr.cpp:141`) compiles to. Our call is *correct*; our
spelling is just missing from the group.

The retail map's only `GetBandUsers` entries are the **2-arg const** form
(`0x82683b78`, `QBAHPAV...H@Z`) and `GetBandUsersInSession` (`0x82683d60`) —
neither is what retail's call site here targets, confirming retail used the
0-arg reference-returning overload whose name the fold destroyed.

**idx 249** — target `bl ??0?$vector@HV?$StlNodeAlloc@H@stlpmtx_std@@@stlpmtx_std@@QAA@ABV01@@Z`
(`vector<int>` **copy** ctor, `0x827c1378`); we emit the `vector<BandUser*>`
copy ctor. A map-wide search finds **no spelling of the `vector<BandUser*>`
copy ctor anywhere** (`hits: []`). The existing alias map *does* fold the two
**allocator** ctors (`ABV?$StlNodeAlloc@...`) at `826B8B28` — so this is a
one-arity gap in coverage, not a new claim.

Both are the documented `CustomizePanel` shape: closing every instruction-level
charge would buy `mpn == 100` (+1 function) and **exactly 0 bytes**.

### Why `matched_functions` is not a cheaper prize

`mpn` excludes *non-immediate* arg diffs, so the register charges fall out of it
— but the 54 offset charges are **immediates** and are charged to `mpn` too,
which is precisely why `mpn` sits at 99.655914 rather than 100. Reaching
`mpn == 100` still requires the entire stack-slot wall plus the insert and the
replace. There is no cheap half-win here.

## What I got WRONG (the most useful line in this doc)

I opened by predicting **DP-2's in-source census was stale**, reasoning that
"~61 offset diffs" could not leave fuzzy at 99.579 on a 651-instruction body.

**The prediction failed. DP-2's census reproduces exactly** — 4 user slots
DIFFER and 8 SHIFTED of 32, today as then (plus 6 PERMUTED, a class the tool has
since gained). My inference was wrong because **fuzzy% does not track mismatch
count**: 65 of 652 instructions (10.0%) are charged and fuzzy still reads 99.58,
because a `diff_arg` costs ~0.006 pp. `diagnose`'s own naive estimate for the
same state is "~90.0% (587/652 equal)" and the raw ruler reads 99.0876.

⇒ **Reusable rule: never infer charge count from the fuzzy percentage.** That
inference is the mechanism by which this row keeps getting re-briefed as
"nearly done" — a 99.58 that is actually 65 charges across four mechanisms.

## What DP-2 got wrong, now corrected in-source

DP-2 filed two `stack-layout` instrument bugs and warned in the source that the
frame-size line was "vacuous 0==0". **Both are fixed and the caution is now
actively harmful** — it tells lanes to distrust a working instrument. Today:

```
Frame size:          TGT 0x102f0   BASE 0x102f0   Δ +0x0
Callee-saved GPRs:   TGT 18      BASE 18      Δ +0
  frame evidence: TGT stwux r1,r1,r12=-0x102f0
                  BASE stwux r1,r1,r12=-0x102f0
```

and it now self-discloses fingerprint degeneracy ("28 of 32 target slots share a
fingerprint with another (largest group 17) … MATCH/PERMUTED above is decided by
aligned access rows, not by fingerprint"), which was DP-2's bug #2. The source
note has been rewritten to say so.

## The stack wall, characterised

18 of 32 user slots are misplaced: 4 DIFFER, 6 PERMUTED, 8 SHIFTED. The offset
histogram has **10 distinct deltas** (+32×13, +16×12, −48×7, −8×5, −64×5,
−32×4, +72×3, +24×3, −4, +4) — there is no uniform shift, so this is not a
class-layout defect (DP-2 already refuted that: r31 is the frame base of a
0x102f0 = 66,288 B frame, dominated by `unsigned char stackbuf[0x10100]`).

The PERMUTED class is the one that forecloses the obvious lever: the tool states
both sides use the **same slot SET with variables assigned differently**, i.e.
MSVC temporary-slot shaping, which a declaration reorder does not address.
Frame size and callee-save count already match exactly.

The insert/replace pair is one genuine shape difference, at the
`static Message msg("exit_stats", DataNode(new DataArray(0), kDataArray));`
temporary: retail re-forms the temp's address (`addi r5, r31, 0x78`) where we
cache the ctor's returned `this` (`mr r29, r3` at 523, `mr r5, r29` at 530).

## Hand-off: the two alias memberships (NOT installed here — deliberately)

A future alias lane could add, **with proof**:

1. `?GetBandUsers@BandUserMgr@@QAAAAV?$vector@PAVBandUser@@V?$StlNodeAlloc@PAVBandUser@@@stlpmtx_std@@@stlpmtx_std@@XZ`
   → the existing group at `0x82801f78`.
2. `??0?$vector@PAVBandUser@@V?$StlNodeAlloc@PAVBandUser@@@stlpmtx_std@@@stlpmtx_std@@QAA@ABV01@@Z`
   → a group whose survivor is `??0?$vector@HV?$StlNodeAlloc@H@stlpmtx_std@@@stlpmtx_std@@QAA@ABV01@@Z`
   at `0x827c1378`.

The proof path is `tools/ourside_fold_sweep.py` (our-side COMDAT byte- and
relocation-identity to the map-resident survivor, plus retail corroboration at
the survivor address).

**I did not install them, on purpose.** They pay **0 bytes on this row** (63
charges would remain), so this lane could not A/B-justify them; and a *map-only*
patch is exactly the shape `ab_measure`'s `control_none_shape()` flags
`ALIAS_SUSPECT` — `name_check` up with `none` flat is, by construction, what a
*fabricated* alias also looks like. An alias that cannot be priced on the row
that motivated it should be proven and measured by a lane that owns the
whole-binary alias question, not smuggled in by a row lane.

## What I did NOT do

- No permuter (OFF by standing directive; the register rotation is its class).
- No alias installation (above).
- No stack-slot reordering experiments: with the two symbol charges unclosable,
  even a perfect slot match pays **0 bytes**, and the brief's instruction is not
  to grind for a partial improvement that pays zero.
- No change to `splits.txt`, `objects.json`, `symbol_aliases.json`, or any map.
- I did not re-audit the other three rows in DP-2's cluster (BandDirector,
  BandCharacter) — out of scope for this lane.

## The alias hand-off is SMALLER than it looks — measured, and it argues against installing

After writing the hand-off above I sized it, and the result weakens it enough to
be worth stating plainly. Both candidate aliases would forgive charges at
**zero currently-collectable rows**:

- The 0-arg `GetBandUsers()` has only **two** call sites in the whole tree:
  `src/band3/game/Game.cpp:765` (`Game::PopulatePlayerLists`) and
  `src/band3/meta_band/UIStats.cpp:164` (this row).
  - This row pays **0 bytes** whatever happens, because 63 other charges remain.
  - `Game::PopulatePlayerLists` is **not in `target_symbol_map.json` at all**
    (0 hits of 29,553 rows) and **not among the 356 named functions** of unit
    `default/band3/game/Game` in `report.json`. objdiff pairs by NAME, so an
    unidentified row scores 0% regardless of any alias.
- The `vector<BandUser*>` copy construction occurs **exactly once in the tree** —
  `UIStats.cpp:164`, i.e. this row again.

⇒ Installing either alias today moves **nothing**, while carrying the documented
integrity hazard (an unproven alias lifts `name_check` *by construction*, and a
map-only patch's `name_check`-UP / `none`-FLAT shape is indistinguishable from a
fabricated one). **Recommendation: do not install them until some row that would
actually collect bytes depends on them.** They are recorded here so the next lane
finds the analysis instead of redoing it.

## A/B of this lane's change (pre-registered, then measured)

The only code change is the rewritten comment block. Pre-registered before
measuring: Δfunctions 0, Δbytes 0, no row crossing, none falling out, and leg B
recompiling **exactly 1 TU** — with the named failure mode that the note adds 29
lines above the function, so any line-number dependence in the TU would perturb
codegen. That was checked and cleared first: `__LINE__` occurs **0** times in
`UIStats.cpp`, and its only `__FILE__` (line 31) is a path literal.

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-ei --from-dirty`:

```
leg A: matched=43956 masked=23224 honest=20732 code%=40.242733  (recompiles: 0, settled)
leg B: matched=43956 masked=23224 honest=20732 code%=40.242733  (recompiles: 1, settle iterations: 2)
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.000000pp   units at 100% [mpn]: 189 -> 189  (0 reached 100, 0 fell off)
```

Prediction confirmed on every field, including the falsifiable one (leg B
`msvc=1`), so the run was a real measurement rather than the absent-vs-absent
vacuity `ab_measure` refuses on. The doc you are reading is a `.md` and was
added after the measurement; it cannot affect the build, so the landed source is
byte-identical to the measured source.
