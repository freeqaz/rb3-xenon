# BANKED CAPABILITY: objdiff global-byte-equality fork (identity-transfer case-B unlock)

**Status: BUILT + PROVEN, NOT INTEGRATED (2026-06-21).** Honest payoff in the current
repo state = **+0** (do-no-harm). The fork is a *capability* whose payoff (+150–220
ceiling) is gated UPSTREAM on a future case-B harvest wave. Do not make it the project
default until that wave + validation.

## Where it lives
- Branch **`caseb-global-byteeq @ b1c92be`** in the **`../objdiff`** repo (separate from
  rb3-xenon). The objdiff repo is currently *parked* on this branch.
- Built ONLY to `/tmp/objdiff-fork-target` (`cargo build --release --target-dir
  /tmp/objdiff-fork-target -p objdiff-cli`). The **shared `../objdiff/target/release/
  objdiff-cli` was NOT rebuilt** (still dated Jun 11) — so all current rb3-xenon builds
  use stock objdiff. All 70 objdiff-core tests pass; per-unit diff semantics untouched.
- It captured ~362 lines of pre-existing WIP that were uncommitted on objdiff's `main`
  (someone had started this exact fork); that WIP is preserved in the branch commit.

## What it does
A cross-unit GLOBAL byte-equality second pass in the **report driver** (objdiff-cli
`report.rs generate`), NOT inside `diff_objs` (which only sees one unit's target/base
pair). It promotes an unmatched NAMED target fn whose retail body was carved into a
*foreign* unit's span (identity-transfer **case-B**) when a byte-identical base symbol
exists in any unit — the matches stock objdiff structurally cannot pair within-unit.

## The honesty gate (why it's sound — and why +0 now)
Byte-equality ALONE is NOT a match (`code.rs:101-131`): the fork enforces
**masked-bytes-equal + reloc-target-NAME-equal** (full reloc honesty contract), PLUS a
REQUIRED oracle gate (`--global-byte-eq-oracle unified_id_rb3wii.json`): a promoted VA
must be oracle-named **sim ≥ 0.5** AND attribute to the claiming unit's source. It
**refuses to run without the oracle**. In the current repo it correctly REJECTED the 4
unsafe-demo promotions as un-oracle'd STL template folds (`_Vector_base<T>`,
`_M_create_node`) — exactly the ≤44B ICF-stub misattribution class `icf_alias_check.py`
guards. Hence honest unlock now = +0 (do-no-harm).

## Usage
```
# production (honest, off by default = stock semantics unless flag passed):
objdiff-cli report generate --global-byte-eq \
  --global-byte-eq-oracle unified_id_rb3wii.json \
  [--global-byte-eq-log promos.json] -o report.json
# demo/diagnostic ONLY (bypasses oracle gate — NEVER for a real measure):
OBJDIFF_CASEB_UNSAFE_NO_ORACLE=1 ...   # proved transport +4 (9558->9562)
OBJDIFF_CASEB_DEBUG=1 ...              # prints the gate funnel
```
**PROCESS GATE for any landing:** every promotion in `--global-byte-eq-log` MUST be
re-audited with `tools/icf_alias_check.py` (real-bodied >44B, oracle-correct) before the
count is trusted.

## Why +150–220 is gated upstream (the case-B harvest plan)
The ceiling requires the case-B methods to be (a) oracle-named in identity-transfer
micro-pins AND (b) defined byte-exact by our compiled obj. That means PORTING the
scattered TUs' sources first — and wave-16 proved **ported MWCC→MSVC bodies diverge from
retail** (BandProfile: 0/64 reached 100%). So a case-B harvest wave is:
1. Port a scattered TU's source (so the obj defines the methods) — the hard, attrition-prone step.
2. `identity_transfer.py --tu X --apply` (now collision-safe) to micro-pin/name its case-B methods.
3. Build the report with the FORKED objdiff `--global-byte-eq --global-byte-eq-oracle`.
4. Audit every promotion via `icf_alias_check.py`; composed-verify; land only real-bodied.

## Integration checklist (when a case-B harvest wave is greenlit)
- [ ] Validate the fork is a strict superset: forked `objdiff-cli` WITHOUT `--global-byte-eq`
      must produce the IDENTICAL report.json as stock (do-no-harm regression test).
- [ ] Rebuild the shared `../objdiff/target/release/objdiff-cli` from the fork branch
      (this makes it the project default binary — only after the do-no-harm test passes).
- [ ] Wire `--global-byte-eq --global-byte-eq-oracle` into `tools/fresh_report.sh` behind
      a flag so normal measures stay stock unless explicitly harvesting case-B.
- [ ] Restore/clean the `../objdiff` repo git state (currently parked on the fork branch
      with an untracked `modify_url.py`).
