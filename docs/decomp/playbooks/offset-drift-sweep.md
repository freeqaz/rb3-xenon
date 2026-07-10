# Offset-drift sweep — mechanical layout/header-drift hunting

Playbook for finding and closing the **layout-drift class** of near-miss: a
member added/removed/resized in a shared header (or a TU's static/global
definition order) that shifts offsets across every function touching it. This
is the *complement* of [`nearmiss-harvest.md`](nearmiss-harvest.md) — that
playbook forbids header edits and sculpts body evaluation-order in the
96–99.99% band; this one specifically hunts the header/layout bugs across the
whole **85–99.99%** band, because one header fix can close 5–8 functions at
once.

Proven over 2 rounds (2026-07-10): **round 1 +8 strict, round 2 +23 strict,
0 net regressions**. Hit rate on GO-verdict candidates ≈ 70% to a strict
close, the rest bank fuzzy or fall to permuter. The multiplier (functions
closed per edit) is what makes this the highest-ROI near-miss lever when the
pool still has un-fixed shared-header drift.

Tool: `scripts/harvest/offset_drift_sweep.py`. Full method reference and the
per-round records: `docs/decomp/handoff/offset-drift-sweep-2026-07-10.md`
(round 1, method) and `offset-drift-sweep-r2-2026-07-10.md` (round 2, lessons).

## The loop (one round)

```bash
# 1. regenerate the pool from a FRESH report (main moves fast — never reuse)
python3 scripts/harvest/gen_nearmiss_pool.py --min 85 --max 99.99 -o ~/tmp/pool.json

# 2. sweep: run objdiff per fn, classify every offset/immediate arg diff by
#    memory-operand base register. ~12 s warm for ~350 fns (objdiff-cached).
python3 scripts/harvest/offset_drift_sweep.py ~/tmp/pool.json -o ~/tmp/drift.json

# 3. rank by struct/global delta uniformity (see below), recon the top cluster,
#    then land verified fixes in gated worktree lanes.
```

The sweep classifies each `l*`/`st*` memory operand in a diff by its base
register:
- **stack** (`r1`) — frame-slot moves, usually a *consequence* not a cause.
- **frame** (`r30`/`r31`) — ambiguous (frame copy vs `this`).
- **struct** (any other base reg) — struct-member offset drift candidate.
- **global** (trailing reloc symbol, e.g. `r11, 0x22, r11, lbl_82C926B8`) —
  member/ordering drift of TU statics or a global object.

**objdiff arg format** is comma-separated: `lwz r7, 0x3c, r31` means
`lwz r7, 0x3c(r31)`. **Delta semantics: TGT = retail, SRC = ours, so
`delta = ours − retail`.**

## Interpretation guide (validated across both rounds)

Rank candidates by the **uniformity** of the dominant struct/global delta:

- **Uniform-signed dominant delta across ≥3 struct/global diffs = layout
  drift** → GO for recon. A member added/removed/resized in a shared header.
  A uniform `+8` = 4-byte member + 4-byte pad before an 8-aligned member; a
  uniform `+0x10` = an `ObjPtr`/`ResourceDirPtr`-sized member.
- **Balanced ±N pairs = access-order / regalloc divergence** → permuter-class
  or at-limit, NOT a header fix. (`CheckBSPTree` std-order reversal, `Rot`
  ±4/±8 pairs.)
- **`oris`/`ori` ladder where ours = 2× retail** = a used-as-`1<<k` enum
  gained an extra enumerator, OR (more common in RB3) MSVC function-local
  static GUARD bits shifted by a retail-absent `HANDLE_MESSAGE` arm.
- **`li rN, <size>` feeding operator new = a sizeof witness** for the
  allocated class — a direct read of the (wrong) struct size.

## Recon-before-edit is mandatory — the sweep reading is a *hypothesis*

**The single most important discipline in this playbook: the raw sweep reading
is refuted more often than confirmed.** Across both rounds, recon overturned
the majority of raw layout readings. Never edit a header off the sweep output
alone. Spend one recon agent per candidate cluster verifying against retail
witnesses (Ghidra decompile of the retail address, `run_objdiff full_listing`,
`__savegprlr_N` prologue, absolute-address math) before proposing any edit.

The recurring false positives that *look* like layout drift are catalogued in
[`../patterns/false-layout-drift.md`](../patterns/false-layout-drift.md) —
read it before trusting any candidate. In one line each:

- **Anchor-bias artifact** (the dominant FP): TGT and SRC address the same
  member through base registers biased differently (`r27+0x18` vs `r27+0x30`),
  so the *absolute* addresses are identical and the delta is meaningless.
  Compute absolutes first.
- **Vbase-anchor mirage**: a class with a virtual base compiles methods
  against the vbase anchor; a trailing Wii-only member moves the anchor and
  fakes uniform drift on members whose real offsets are unchanged.
- **Diagonal-pairing artifact**: an insert/delete reorder shifts objdiff's row
  pairing so identical instruction streams show phantom deltas
  (`inserts=0 deletes=0` is the tell that there's no real drift).

## The confirmed fix families (what recon promotes GO)

When recon *does* confirm drift, the fix is almost always one of these — all
proven this campaign:

1. **Delete a Wii/DC3-only member.** DC3-era additions to shared engine
   headers (`InlineHelp::mResourceDir`, `LabelNumberTicker::unk74`) are absent
   in retail RB3. Deleting the member closes every function in the cascade. The
   member may be *mid-class* (later members drift +N, earlier anchor-relative
   negative offsets drift −N) or *trailing* (vbase-anchor mirage).
2. **De-inline a helper to match retail.** Retail keeps small helpers
   out-of-line (`SafeName` = fn_82733060); our dc3 header inlines them, so
   callers show inserts + regswap the sweep misreads as member drift. See
   [`../patterns/fixable-inline-boundary.md`](../patterns/fixable-inline-boundary.md).
3. **Reorder TU statics** to match MSVC's .bss emission order (see
   [`../patterns/fixable-struct-layout.md`](../patterns/fixable-struct-layout.md#tu-static--global-bss-emission-order)).
   Two wins this campaign (Locale, DataFile).
4. **Prefer the rb3-Wii convention over dc3 for retail** even in `src/system`
   engine code: CharBonesSamples uses `BinStream&` + a mutable file-static
   `gVer` + assert-free `SetVer`, not dc3's `BinStreamRev`. **dc3 oracle text
   is newer and can be a mirage for RB3** — Ghidra-decompile the retail
   function before porting a dc3 body in the 85–95% band.
5. **Drop retail-absent guards / arms.** Retail dropped Wii `if(dir)` /
   `if(i!=-1)` wrappers (`OnEnterCloset`) and whole `HANDLE_MESSAGE` arms; the
   MILO_ASSERT stays.

## Lane discipline & landing

Group verified candidates into **disjoint-file lanes** so they run as parallel
worktree agents without conflict. Hard rules:

- **Anything touching `scripts/target_symbol_map.json` is its own SERIALIZED
  lane** (one owner per round — concurrent map edits corrupt each other).
- Single-`.cpp` body edits are freely parallel-safe (own lane).
- Shared-header edits (the widest blast radius) go in their own lane and
  **land LAST, after rebasing** onto the day's other landings, because their
  whole-binary A/B must not race a moving baseline. Apply one header edit at a
  time with its own checkpoint so a late revert doesn't discard earlier safe
  wins.
- **Map the regression surface explicitly**: grep every TU that includes each
  touched header and flag the *pinned* ones. Recon under-reports this — round 2
  found `math/Utl.h` reached 40 files / 23+ pinned TUs the recon agent hadn't
  named. Widen the A/B, don't trust the narrow claim.

Landing gate (non-negotiable, per the shared playbook invariants): composed
whole-binary A/B via `scripts/harvest/ab_supervise.sh` run **twice**
(run1==run2), `measure_delta.py --fuzzy-eps 1.0` showing net>0 with 0 strict +
0 fuzzy regressions, and `tools/icf_alias_check.py` reading HONEST. **Baseline
against a genuine fresh build** — a warm/seeded report can be inflated and
produce phantom regressions (round 2 lane-2 hit this).

## Pitfalls & standing leads

- **Misbounded tiny `fn_` symbols poison map-entry closes.** Round 2:
  fn_823CB928 was sized 0x2C in symbols.txt and swallowed a neighboring
  helper, so the map entry paired `SetVer` against float code. When a new tiny
  map entry reads a false 0%/low-%, check its symbols.txt size against the real
  `blr`. A systematic size-vs-`blr` scan around newly mapped tiny functions is
  an open tooling lead.
- **The sweep only sees PAIRED near-misses.** Unpinned TUs and 0%-paired
  functions with layout drift are invisible — pin expansion feeds this pool.
- **A big `frame`-class count with a small `struct`-class count** = local /
  temporary layout divergence (inlining, local-type size), not the class
  header. Investigate before blaming the struct.
- **`cls '?'` li-immediate rows ARE actionable** — a `li r6, 0x2` vs `0x0`
  (XAUDIO2_VOICE_NOPITCH) was a real arg-constant bug worth ~1 KB of a fn.
- Register refuted/permuter-class verdicts in
  `scripts/harvest/nearmiss_verdicts.json` so `gen_nearmiss_pool.py` excludes
  them next round.

## Results log

| Round | Date | Swept | GO | Landed | Notes |
|---|---|---|---|---|---|
| 1 | 2026-07-10 | 349 | 7 lanes | +8 strict | Locale statics, SynapseAPO, Practice/Overshell Handle (2.8+5.8 KB) |
| 2 | 2026-07-10 | 355 | 13 | +23 strict | LabelNumberTicker unk74 (+8), InlineHelp mResourceDir (+5), SafeName de-inline, CharBonesSamples convention port, MinEq removal survived 40-file surface |
