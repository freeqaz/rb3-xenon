export const meta = {
  name: 'underpin-harvest',
  description: 'Empirically confirm + harvest under-pin .text extensions (capture unowned real functions abutting wired pins)',
  phases: [
    { title: 'Harvest', detail: 'one agent per address-spread batch: extend pins, build, attribute real-fn gains, drop breakers/regressors' },
  ],
}

// 173 candidates live in /tmp/underpins_actionable.json (agents read it).
// Round-robin by index (the file is match-rate sorted; we re-spread by address
// inside each agent). NBATCHES kept modest so concurrent dtk re-splits don't thrash.
const N = (args && args.count) || 173
const NB = (args && args.batches) || 8
const batches = []
for (let b = 0; b < NB; b++) {
  const idx = []
  for (let i = b; i < N; i += NB) idx.push(i)
  if (idx.length) batches.push({ b, idx })
}

const SCHEMA = {
  type: 'object',
  additionalProperties: false,
  properties: {
    baseline_matched: { type: 'number' },
    verified: {
      type: 'array',
      items: {
        type: 'object', additionalProperties: false,
        properties: {
          tu: { type: 'string' }, old_end: { type: 'string' }, new_end: { type: 'string' },
          net_delta: { type: 'number' },
          real_gain_fns: { type: 'array', items: { type: 'string' } },
        },
        required: ['tu', 'old_end', 'new_end', 'net_delta'],
      },
    },
    refuted: {
      type: 'array',
      items: {
        type: 'object', additionalProperties: false,
        properties: { tu: { type: 'string' }, reason: { type: 'string' } },
        required: ['tu', 'reason'],
      },
    },
  },
  required: ['verified', 'refuted'],
}

function prompt(b, idx) {
  return `You are harvesting UNDER-PIN .text extensions in the rb3-xenon decomp (/home/free/code/milohax/rb3-xenon). Each candidate is a pinned TU whose .text range ends exactly where an UNOWNED real function (in an auto_ blob) begins — extending the pin to include that function should let it pair in objdiff and reach 100%, IF it genuinely belongs to the TU. Your job: empirically confirm, keep only true net-positive non-stub-fold gains, and report them. NEVER touch the main repo tree; work entirely in your own worktree.

## Your candidates
Read /tmp/underpins_actionable.json (a JSON array, 173 items). You own indices: [${idx.join(', ')}].
Each item: {tu, end, run_end, real_fns, real_bytes, pin_matched, pin_total}. \`end\`/\`run_end\` are integers (decimal). To extend a pin, change that TU's .text line \`end:0x<END>\` -> \`end:0x<RUN_END>\` (uppercase hex of the ints).

## Setup (once)
1. \`scripts/setup_worktree.sh /tmp/wt-up-${b} up-${b}\` (btrfs CoW; few seconds).
2. In the worktree, add a skip-if-present guard to tools/download_tool.py so the build does not re-fetch compilers (sandbox blocks SSL). Right before the line \`print(f"Downloading {url} to {output}")\` in main(), insert:
       if output.exists() and (not output.is_dir() or any(output.iterdir())):
           print(f"{output} already present, skipping"); return
3. cd /tmp/wt-up-${b}. Build BASELINE: \`touch config/45410914/config.yml && ./tools/ninja-locked\`. Confirm exit 0. Record baseline matched = report.json measures.matched_functions. Save a copy: \`cp build/45410914/report.json /tmp/base-${b}.json\`. NOTE: extending a pin only re-splits the TARGET obj (dtk) — no source recompile — so builds are fast.

## Harvest (batch-apply, then prune)
4. Apply ALL your candidates' extensions to config/45410914/splits.txt (match each TU block's .text line by its exact \`end:0x<END>\` and rewrite the end). They are address-spread so they should not interfere.
5. \`touch config/45410914/config.yml && ./tools/ninja-locked 2>&1 | tee /tmp/b-${b}.log\`.
   - If it FAILS with "Split <TU> .text ... ends within symbol ..." or similar: that TU's run_end lands inside an except_data/function. REVERT just that candidate (record it refuted, reason "build-break: <detail>"), rebuild. Repeat until green.
6. Compare report.json to /tmp/base-${b}.json per function (key by unit name + function name; value = match_percent_normalized).
   - GAIN = a function that went to >=100 now and was <100 before. REGRESSION = went below its prior value.
   - For each GAIN, get its size (objdiff/report 'size' or symbols.txt). REAL gain = size > 0x2C (44 bytes). A <=44B "gain" is an ICF-stub fold = FAKE, do not count it.
   - If any REGRESSION exists, identify which extended TU caused it (the regressed fn is at/after that TU's extension, or its except_data shifted), REVERT that candidate (refuted, reason "regression: <fn>"), rebuild, re-measure. Repeat until zero regressions.
7. A candidate is VERIFIED if, attributable to its extension, there is >=1 REAL gain and no regression. Map gains to candidates by address (the gained fn lies in [old_end, run_end) of the candidate's TU). A candidate whose captured fns only produced <=44B stub folds OR no gain => refuted (reason "no real gain" / "stub-fold only").

## Honesty
- Trust the MATCH RESULT, not jeff's size table. The build is the oracle.
- Do not count ICF-stub (<=44B) folds as wins (tools/icf_alias_check.py exists if helpful).
- Final matched with all VERIFIED kept must be > baseline by exactly the count of real gains, with zero regressions.

## Return (StructuredOutput)
baseline_matched, verified[] = {tu, old_end:"0x..", new_end:"0x..", net_delta, real_gain_fns:[names]}, refuted[] = {tu, reason}.
Leave the worktree in place (report its path). Do not commit anything.`
}

const results = await parallel(
  batches.map(({ b, idx }) => () =>
    agent(prompt(b, idx), { label: `harvest:batch-${b}`, phase: 'Harvest', schema: SCHEMA })
  )
)

const verified = []
const refuted = []
let okBatches = 0
results.forEach((r, i) => {
  if (!r) { log(`batch ${i} returned null (died/rate-limited) — needs re-dispatch`); return }
  okBatches++
  ;(r.verified || []).forEach((v) => verified.push(v))
  ;(r.refuted || []).forEach((x) => refuted.push(x))
})
log(`batches ok: ${okBatches}/${batches.length}; verified extensions: ${verified.length}; refuted: ${refuted.length}`)
return { okBatches, totalBatches: batches.length, verified, refuted }
