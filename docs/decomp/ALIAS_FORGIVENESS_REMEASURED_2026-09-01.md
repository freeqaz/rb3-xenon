# Alias forgiveness re-measured — lane S3-ABLATE, 2026-09-01

Re-measurement of how much of our score rests on ICF alias forgiveness. The
standing figure (lane ALIAS-2, 2026-08-16, `64088f62`) was **818,416 B /
7.929877 pp over 1,528 groups / 15,196 folded memberships, `matched_functions`
+0**. Since then `scripts/symbol_aliases.json` changed shape drastically —
1,591 groups but only **5,338** memberships — so the exposure was unknown.

## Headline

**811,492 B / 7.920118 pp over 1,591 groups / 5,338 memberships.**

| | 2026-08-16 | 2026-09-01 | change |
|---|---:|---:|---:|
| groups | 1,528 | 1,591 | +63 (+4.1%) |
| folded memberships | 15,196 | **5,338** | **−9,858 (−64.9%)** |
| bytes forgiven | 818,416 | **811,492** | **−6,924 (−0.85%)** |
| pp forgiven | 7.929877 | 7.920118 | −0.0098 |
| bytes per membership | 53.9 | **152.0** | **2.8× denser** |
| Δ`matched_functions` | **0** | **+3,154** | see below |

⇒ **Memberships collapsed by 64.9% and the byte exposure did not move.** The
retired memberships were carrying essentially nothing; the file is now 2.8×
denser in value per membership. The mechanism as a whole is unchanged in size.

This corroborates `97771c75` (2026-08-19) rather than merely agreeing with it:
that commit retired **9,395 memberships (15,162 → 5,767) for −4,128 B** as a
*fabrication* class — the generator emitted "the transitive closure over
body-identity witnesses as one group per survivor candidate", i.e. N groups at N
distinct retail addresses each claiming the same M folded spellings, so at least
N−1 of every N was fabricated. It kept the emptied groups (never pruned),
exactly as the standing rule requires. ⇒ **A membership count was never a proxy
for exposure**, and the 9,858 memberships lost since 08-16 account for only
6,924 B — **0.7 B per membership** against a surviving average of 152 B.

Share of the score that rests on it, at this tree:

| measure | absolute | rests on aliases | share |
|---|---:|---:|---:|
| `matched_code` | 3,772,988 | 811,492 | **21.51%** |
| `matched_functions` | 42,295 | 3,154 | **7.46%** |
| honest (`matched − masked_equal`) | 19,380 | 3,154 | 16.27% |

## ⛔ The Δ0-on-`matched_functions` validity check is STALE — do not apply it

Every prior record, and the brief for this lane, states that a genuine
alias-forgiveness measurement shows **Δ`matched_functions` = 0** — the
"arg-blind shape" — because `mpn` excludes arg-only penalties and a
relocation-name charge is an arg penalty. **Today it measures +3,154, and the
measurement is correct; the RULE is what expired.**

Cause, in the fork, dated exactly: **`b14ba45`, 2026-08-20, "NameCheck: let a
vetted wrong-callee reach `match_percent_normalized`"** — four days after
ALIAS-2 measured the +0. Under `NameCheck` a *vetted* relocation-name
difference is deliberately **no longer folded into `arg_diff_score`**, so it now
reaches `mpn`. Its own comment gives the reason: folding it "made the canonical
metric blind to an entire bug class: repointing all 13 `bl` sites of a matched
function at a nonexistent decoy, changing ZERO instruction bytes, left
`match_percent_normalized` at exactly 100.0."

The change is scoped to `arg_diff_score` and never touches `diff_score`, so it
moves **`mpn` only, not `fuzzy`**. Two consequences:

* the **byte** comparison 818,416 → 811,492 **is apples-to-apples** (both
  `fuzzy`-based, both `name_check`);
* the **function** exposure going 0 → 3,154 is **entirely a ruler change, not an
  alias-file change**. Nothing about the aliases became more load-bearing.

⇒ **The genuine signature of an alias measurement today is the `none` ruler
being exactly flat, not Δ`matched_functions` = 0.** Had this lane trusted the
briefed check it would have reported the measurement as broken.

Only two scoring-relevant objdiff commits exist since ALIAS-2 (`b14ba45` and
`19ee5fd`, both 2026-08-20); `7a334d4` is analysis labelling and `bbeb3e1` is
formatting.

## The `none`-ruler control, and the channel split

`none` ignores relocation names, so if the whole mechanism is relocation-name
comparison the ablation must read **exactly 0** there. Measured:

| ruler | FULL | EMPTY | Δ |
|---|---:|---:|---:|
| `name_check` (graded) | 3,772,988 (36.824165%) | 2,961,496 (28.904047%) | **−811,492 B / −7.920118 pp** |
| `none` | 4,424,104 (43.179028%) | 4,424,104 (43.179028%) | **0 B / 0.000000 pp / 0 fns** |

⚠ Per the standing rule, that flatness is **not a clearance** — `none` cannot
see relocation names by construction, and a *fabricated* alias would read flat
there too. It establishes only that the mechanism operates through relocation
names and nowhere else.

Splitting the 811,492 B by what the removal actually charges:

| channel | rows | bytes | share |
|---|---:|---:|---:|
| **MPN_TOO** (`mpn` fell below 100 as well) | 3,117 | **732,668** | **90.29%** |
| ARG_ONLY (`fuzzy` fell, `mpn` held at 100) | 1,879 | 78,824 | 9.71% |
| GONE_FROM_REPORT (row lost its pairing) | 0 | 0 | 0.00% |

**No row leaves the report** — the pairing channel is not involved; rows stay
paired and score lower. `total_code` (10,245,956) and `total_functions` (69,219)
are identical on both legs, so the denominator is untouched and this is pure
numerator movement.

## Instruments — four, agreeing exactly

The project rule is to size this by **ablation, never by a name-keyed census**.
Four independent instruments were run and agree to the byte and the function:

| instrument | path | mutation | Δbytes | Δpp | Δfns |
|---|---|---|---:|---:|---:|
| `alias_forgiveness_audit.py measure` | ninja | `groups = []` | −811,492 | −7.920118 | +3,154 |
| `s3_alias_decompose.py` | ninja | `folded = []` | −811,492 | −7.920118 | +3,154 |
| `s3_ruler_legs.py` | direct objdiff-cli | `folded = []` | −811,492 | −7.920118 | +3,154 |
| **`ab_measure.py --from-dirty`** | full protocol | `folded = []` | **−811,492** | **−7.920118** | **+3,154** |

`ab_measure` was run on a **second, independently built worktree**, whose FULL
baseline reproduced the first tree's exactly (38,896 rows@100 / 3,772,988 /
42,295). Its own reconciliation: the 4,996 fallen rows sum to **exactly**
811,492 B, and the 3,154 rows losing `mpn == 100` equal Δ`matched_functions`
exactly.

Two mutations — pruning groups outright vs emptying `folded: []` — give an
identical delta, so the standing "never prune" rule costs nothing in
measurement fidelity.

## Tool provenance — the fleet `objdiff-cli` was swapped mid-lane, and it was NEUTRAL here

`bin/objdiff-cli` symlinks to the fleet-shared
`../objdiff/target/release/objdiff-cli`, which was **rebuilt at 08:59:42 today**,
mid-lane (`358c715835cc` → `210aab60ca30`; file sha256 now
`ee78f52f68469805…`). This lane's first pass ran **before** the swap and the
per-group sweep ran **across** it, so the numbers could not be attributed to a
tool state as measured.

Handled by re-measuring rather than reconciling. The lane rebased onto current
main (`3ed3b213`) — which touched **only `tools/` and `docs/`, zero `src/` or
`config/`, so the tree's code is unchanged — rebuilt, and re-ran **both legs on
the current binary**. `report.json` now self-declares
`tool_commit 210aab60ca30` / `tool_binary_hash faf3390631a58473` / version
4.2.8.

| | pre-swap (`358c7158`) | post-swap (`210aab60`) |
|---|---:|---:|
| FULL `matched_code` | 3,772,988 (36.824165%) | 3,772,988 (36.824165%) |
| EMPTY `matched_code` | 2,961,496 (28.904047%) | 2,961,496 (28.904047%) |
| Δ | −811,492 B / −7.920118 pp / +3,154 fns | −811,492 B / −7.920118 pp / +3,154 fns |
| fallen rows | 4,996 | 4,996 — **set-identical** |
| `none` ruler Δ | 0 / 0 / 0 | 0 / 0 / 0 |

⇒ **The swap is demonstrated inert for this measurement**, by set equality of
the fall sets and not by assuming a rebuild is neutral. Every headline figure in
this document is from the **post-swap** run, both legs on `210aab60ca30`.

⚠ The per-group sweep's baseline predates the swap. Its numbers are reported
above as **bounds** anyway, and the binding limitation is the 27.3% sample size,
not the tool question — but they are the one artifact here not re-measured
end-to-end on the new binary, and are labelled provisional for that reason.

## `icf_alias_finder.py --validate` — RED, and honest

**`VALIDATE: FAIL`, rc=1** — 1,369 map-consistent, 219 tolerated, **1
CONTRADICTED (FATAL)**, 1,591 total.

The instrument is **not vacuous** on this tree — every documented failure mode
was checked and is absent:

* 1,591/1,591 groups reached, 6,929 member spellings looked up;
* target side: 3,088 live target objs, **27,877 mangled names indexed**
  (`UNRENAMED_TARGET_OBJS` would fire near 0);
* compiled side: 1,205 objs, 837,191 symbols, **floor 1,045 objdiff.json base
  objs, all reached** (`EMPTY_COMPILED_INDEX` / `INCOMPLETE_COMPILED_INDEX`
  clear);
* `STALE_SPELLING` is **82**, not the ~1,421 that is the signature of an empty
  compiled index.

The contradiction:

```
FAIL [SetJump @ 0x827029d8]: target objs name 2 members:
  ['?SetJump@StandardStream@@UAAXMMPBD@Z',
   '?UpdateTimeByFiltering@StandardStream@@UAAXXZ']
```

The group claims these two fold, but our target objs now name both — at
different addresses — which refutes the fold. Its evidence line is T1 only.

★ **The contradiction is metric-inert.** Ablating this group alone measures
**0 bytes / 0 functions / 0 rows**. So the CI gate can be returned to green at
**zero metric cost** by withdrawing this membership (keeping the group with
`folded: []`, never pruning it).

⚠ **This is a live CI failure**, not a local artifact: `--validate` is wired at
`.github/workflows/build.yml:132`, and this worktree's alias file is
byte-identical to main's (sha256 `7b41c0e756c55235`). Checked for drift rather
than assumed — this lane branched at `897b9763` and main has since advanced to
`292c82e3`, but **none of the 10 intervening commits touched any of
`--validate`'s three inputs** (`scripts/symbol_aliases.json`,
`config/45410914/splits.txt`, `scripts/target_symbol_map.json`), so the verdict
applies to main as it stands. The historical record has
this gate PASSing with 0 contradicted on 2026-08-14 and 2026-08-17, so it is a
regression. The likely mechanism is the documented one: a **map identification
landed and exposed a wrong alias** — naming an anonymous address pays in bug
exposure, not bytes.

## Concentration — over ROWS (complete) and over GROUPS (sampled)

### Over rows — COMPLETE, not sampled

All 4,996 fallen rows are known exactly from the FULL-vs-EMPTY leg, so this
needs no sweep and carries no sampling error:

| | share of 811,492 B |
|---|---:|
| top 1 row | 1.09% |
| top 10 rows | 7.22% |
| top 50 rows | 17.44% |
| top 100 rows | 23.92% |
| top 250 rows | 35.68% |
| top 500 rows | 47.64% |
| top 1,000 rows | 62.96% |
| top 2,000 rows | 79.75% |

**The forgiveness is DIFFUSE.** Median fallen row is **76 B**, mean 162 B, max
8,840 B; it takes ~1,000 of 4,996 rows to reach two-thirds of the bytes. There
is no small set of rows to attack.

By unit it is equally spread: **680 units** carry fallen bytes, top 10 units =
15.2%, top 100 = 58.6%.

The head is a single recognisable family — large `DataNode` message
dispatchers, which are exactly the functions with the most `bl` sites and so the
most relocation-name charges:

| row | bytes |
|---|---:|
| `RockCentral::RecordPerformance` | 8,840 |
| `MetaPerformer::Handle` | 7,504 |
| `OvershellSlot::UpdateView` | 6,900 |
| `SaveLoadManager::GetDialogMsg` | 6,592 |
| `inflate` | 5,296 |
| `Player::Handle` | 5,228 |
| `Campaign::Handle` | 4,996 |
| `VocalPlayer::Handle` | 4,936 |
| `BandDirector::Handle` | 4,732 |

All nine are `MPN_TOO` — i.e. under today's ruler these are rows where removing
the alias costs a **function**, not just bytes.

### Over groups — a 27.3% sample; top-N shares reported as BOUNDS, not estimates

Per-group ablation was run over a **seeded shuffle** of the 1,107 groups that
have ≥1 folded spelling, so any prefix is an unbiased random sample. It reached
**302 / 1,107 (27.3%)** before the lane closed. (The other 484 groups already
carry `folded: []`; their delta is 0 **by construction**, not by measurement.)

What the sample settles:

* **39.1% ± 4.7% of live groups forgive >0 bytes ⇒ ~433 of 1,107.** GROUNDED-1
  measured **448 of 1,493** — so the *count* of paying groups is essentially
  unchanged even though the file lost 64.9% of its memberships.
* distribution of per-group necessity bytes over the 302 sampled:
  **184 forgive 0** · 21 forgive 1–99 B · 75 forgive 100–999 B · 17 forgive
  1k–9k B · **5 forgive ≥10k B**.

**The head, measured directly (no extrapolation).** These are *lower bounds* on
the population's top-N shares, since a bigger unsampled group can only raise
them:

| rank | group | rows | bytes | Δfns | folded | share of 811,492 B |
|---|---|---:|---:|---:|---:|---:|
| 1 | `operator_new_alloc_thunk` | 370 | **77,640** | 371 | 121 | **≥ 9.57%** |
| 2 | `SetObjConcrete` | 142 | 41,192 | 146 | 33 | |
| 3 | `PoolAlloc` | 98 | 26,872 | 100 | 1 | top 3 ≥ **17.96%** |
| 4 | `0DataNode` | 6 | 11,648 | 6 | 38 | |
| 5 | `clear` | 52 | 10,864 | 52 | 55 | top 10 ≥ **23.27%** |

⚠ **The sample CANNOT settle GROUNDED-1's "top 10 = 55.6% / top 100 = 96.4%"
statistic, and must not be quoted against it.** That statistic is a share of the
*sum of per-group necessity bytes*, whose denominator grows as more groups are
measured — so computing it inside a partial sample is **biased high by
construction**. At n = 302 the population's top-10 maps to roughly the sample's
top-3, i.e. the estimate would rest on three groups. What *is* sound is the
union-coverage bound above and the ~433-group count.

⚠ Per-group necessity bytes **OVERLAP and must never be summed as a partition**:
a row needing two groups is counted under both. The sampled sum extrapolates to
~926,141 B against a measured total of 811,492 B, and that excess *is* the
overlap.

⇒ **Pricing rule.** Two facts point opposite ways and both hold: the bytes are
**diffuse over rows** (median 76 B; ~1,000 of 4,996 rows to reach two-thirds)
but **concentrated over groups** (one group, `operator_new_alloc_thunk`, carries
≥9.6% of the whole mechanism, and ~433 groups carry all of it). So fold work
should be priced **per group, never per row** — and the allocator/`Obj`
thunk families are where the mechanism actually lives.

## What this lane did NOT verify

* **Did not adjudicate any membership on retail bytes.** This lane sized and
  attributed; it ran neither `alias_membership_adjudicate.py` nor
  `alias_contradiction_refine.py`, so the PROVEN / NEEDS_SOURCE / CONTRADICTED
  partition from ALIAS-2 is **not** re-measured here and its 2026-08-16 shares
  should not be assumed to still hold over a membership set that lost 64.9% of
  its rows.
* **Did not withdraw the `SetJump` membership**, only priced it. The CI gate is
  left RED deliberately — this lane commits no alias-file change.
* **Did not verify the 484 `folded: []` groups by measurement.** Their delta is
  0 *by construction* (there is nothing to remove), which is stated, not spent.
* **Did not re-check the ALIAS-2 numbers on their own tree**; the comparison
  above assumes the 08-16 figures as recorded.
* **Did not investigate why `--validate` regressed** beyond identifying the
  contradicted group — the commit that introduced the conflicting map name was
  not bisected.
* **Did not complete the per-group sweep** — 302 of 1,107 (27.3%). The full
  sweep is ~1,107 legs; the machine was under load average 60–105 from a
  concurrent grinding lane, giving 6–13 s/leg against the ~2.5 s the ALIAS-2 doc
  records. Top-N *share* statistics are therefore unsettled; only the bounds and
  the ~433-group count above are claimed.
* **Did not re-run the per-group sweep on the post-swap binary** (see tool
  provenance above).
* **Did not read the instruction-level diff of any fallen row.** The claim that
  the `MPN_TOO` channel is the `b14ba45` mechanism rests on the `none` ruler
  being exactly flat plus the fork's source and commit date — a complete causal
  chain, but not a per-row confirmation.

## Reproducing

```bash
scripts/setup_worktree.sh ~/tmp/wt-s3ablate s3-ablate
cd ~/tmp/wt-s3ablate && ./tools/ninja-locked            # MANDATORY: reflinked
                                                        # target objs are PRE-RENAMER
python3 tools/alias_forgiveness_audit.py measure --wt <wt> --fell ~/tmp/fell.json
python3 tools/s3_alias_decompose.py <wt> ~/tmp/fell_chan.json   # channel split
python3 tools/s3_ruler_legs.py <wt> none                        # the none control
python3 tools/icf_alias_finder.py --validate
python3 tools/s3_group_sweep.py --wt <wt> --out ~/tmp/groups.jsonl   # per-group
python3 tools/s3_concentration.py ~/tmp/groups.jsonl
```

⛔ **`tools/alias_group_ablate.py` has no signal handler and its `finally` does
not run under a kill.** Killing it in this lane left
`scripts/symbol_aliases.json` at **1,590 groups / 5,337 memberships** — a
silently damaged input. It was caught only by checking the artifact
(`git status`), never by trusting the command. `tools/s3_group_sweep.py`
installs SIGTERM/SIGINT/SIGHUP handlers for this reason.

⚠ Its documented **2.5 s/group** is stale: the current gate chain makes it
**~12 s/group** (~5.3 h for 1,591 groups). The bypass path in
`s3_group_sweep.py` is ~8 s/group, and under a loaded machine both are slower
still.
