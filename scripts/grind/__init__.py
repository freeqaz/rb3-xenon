"""rb3-xenon side of the decomp_synth bootstrap grind loop.

Three project seams, kept dependency-light (stdlib + decomp_synth only):

- ``splice_scorer.SpliceScorer`` — a ``CandidateScorer`` that splices one
  candidate C++ function into its real translation unit (replace an existing
  definition, else append), runs ``objdiff-cli``, and restores the TU on every
  path. Stashes the full objdiff JSON in ``CandidateScore.meta`` so the refine
  prompt can render a mismatch-cluster diagnosis.
- ``recipe.Rb3XenonRecipe`` — the PowerPC / MSVC X360 prompt recipe (system +
  output contract + dialect card + oracle source + m2c/asm seeds; refine injects
  the objdiff diagnosis or the compile-error tail).
- ``campaign`` — the CLI driver (task list -> per-function bootstrap loop under a
  USD budget ceiling; JSONL attempt ledger + winner capture).
"""
