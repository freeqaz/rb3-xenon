# Lane salvage — V1-VTORDER / S1-STRUCT / V2-PRICED, 2026-08-18

> **STATUS (2026-08-18): CURRENT.** Three vtable/struct lanes were killed by
> harness process exits **twice each**, before committing anything. Their
> worktrees were empty (0 commits, 0 dirty, 0 untracked) but their transcripts
> held 253 KB / 183 KB / 162 KB. This file is the **salvage**, recovered by
> extracting assistant narration from the transcripts rather than re-running the
> lanes. **Two of the three findings are errors in the coordinator's own briefs.**

## Why this file exists at all

The lanes died before commit, twice. Everything below would have been lost on the
second death if it had not been extracted. ★ **The reusable lesson: when a lane
dies before committing, its transcript is still on disk and its narration can be
recovered mechanically** (filter `message.role == "assistant"`, keep `text`
blocks). That reduced 598 KB of JSONL to 6.8 KB of reasoning. **Never read the
raw `.output` JSONL into context — extract it.**

## F1 ⛔ W45's vtable oracle (instrument "I2") WAS NEVER COMMITTED, AND IS GONE

The V1 brief told the lane to *"reuse rather than rebuild"* W45's vtable oracle.
V1 established it **does not exist**: no `wt-w45*` worktree survives, and no W45
commit contains it — the commits touch only `scripts/target_symbol_map.json`,
`src/system/bandobj/TrackInterface.h` and `tools/ident_body_channel.py`. It was
**untracked lane scratch** and is unrecoverable.

⚠ **The coordinator removed that worktree** after checking
`git status --untracked-files=all` came back empty — which it did. So the
instrument was never *in* the worktree (it lived in `~/tmp` scratch or similar).
★ **"Worktree clean" does NOT mean "instrument preserved."** A lane's INSTRUMENT
is a deliverable; if it is not committed it does not exist. Future lane briefs
must require the instrument be committed, not just its findings.

## F2 ★★ A STRICTLY BETTER VTABLE INSTRUMENT EXISTS — identify by RTTI, not by alignment

W45's I2 aligned a name-multiset from our `??_7` slot relocations against
retail's `.rdata` run, and its documented failure mode is **mis-alignment across
a vtable boundary** (vtables are adjacent in `.rdata` with no separator), needing
a confidence margin.

V1's construction removes that trap **by construction** rather than by margin:

> MSVC places an `??_R4` **Complete Object Locator** pointer at vtable slot
> `[-1]`; the COL chains to a **TypeDescriptor** carrying the **literal class-name
> string**. So a retail vtable can be identified **by name, directly from RTTI**.

Universe: CLAUDE.md's own `/GR` evidence counts **2,220 `??_R4` COLs** in retail
`.rdata`. Scan shape: find TypeDescriptors by their `.?AV` name string → find COLs
pointing at them → find vtables by the COL back-pointer at slot `[-1]`.
Confirmed available: `.rdata` at `0x82000400` (0x1f1184 B), `.text`
`[0x82270000, 0x82C4CE3C)`; PE headers little-endian, image content big-endian.

## F3 ⛔ THE COORDINATOR'S COST MODEL FOR THE STRUCT AUDIT WAS WRONG

The S1 brief said to budget **60 min per class** and therefore *"rank hard and
sweep narrowly … a sweep covering 3% must say so."* S1 read the tool's own COST
section and refuted it: **`--all-classes` (`/d1reportAllClassLayout`) returns
every class in the TU from a SINGLE compile — 700 classes / 51 adjustor sets in
13.8 s** — and `audit_header()` is pure Python over a parsed class dict.

⇒ **The audit is compile-bound PER TU, not per class.** The "expensive, must
prioritise" premise collapses, and a **tree-wide sweep is affordable.**

⚠ **The coordinator caused this by mis-generalising its own docstring edit.** It
had promoted a `~10 min (BandSongMgr) … ~50 min under fleet load` latency note
into the docstring hours earlier, then briefed that as *per-class* cost — while
the COST section three paragraphs below said 13.8 s. Both numbers are real (they
differ by **TU size and machine load, not by flag**), and leaving them
unreconciled in one docstring was worse than either alone. **Now reconciled in
the docstring:** take the timeout from the worst case, the batching from the best.

## F4 ⚠ `setup_worktree.sh` does not branch from where a brief says

Both lanes independently found their worktree branched from **`4145778b` on
`grounded2-restoration`**, not the briefed `main @ 19999ec6`. S1 verified
`4145778b` is a **direct ancestor** of `19999ec6` (main 10 commits ahead) and
rebased. **Benign here, but the brief was inaccurate**: `setup_worktree.sh`
branches from the *current* branch's HEAD, so a brief naming a base must either
match the shared tree's current branch or instruct an explicit rebase.

## What was NOT established

Neither lane reached a result. V1 had written the PE parse and was starting the
RTTI scanner; S1 had rebased and was about to run `--selftest` (correctly
refusing to believe any zero from the tool before proving it discriminates);
V2 had not got past worktree setup. **No vtable divergence, no layout divergence,
and no fix was produced by any of the three.** Nothing here licenses a claim
about how common either defect class is.
