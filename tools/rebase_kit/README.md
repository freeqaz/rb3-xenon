# rebase_kit — landing a lane branch across the two generated JSON files

These three scripts are what the coordinator actually lands lanes with. They
lived in `~/tmp` for three landings, which meant the resolver that **silently
lost bytes** was never reviewed and never tested. They are here now so that
both are possible.

```
rebase_drive.py     drive `git rebase main` in a lane worktree; call a resolver
                    per conflicted file; exit 3 on any file it cannot resolve
resolve_aliases.py  field-wise three-way merge for scripts/symbol_aliases.json
resolve_map.py      key-delta three-way merge for scripts/target_symbol_map.json
_v1_frozen.py       the OLD alias merge, frozen as the test's negative control
test_rebase_kit.py  replays the real W16-J landing through both
```

## Usage

```sh
python3 tools/rebase_kit/rebase_drive.py ~/tmp/wt-my-lane
```

The resolvers are normally invoked by the driver, from inside a stopped rebase,
where they read `ours = :2:` (the main side), `base = REBASE_HEAD~1`, and
`theirs = REBASE_HEAD`. They can be run by hand in the same state.

⛔ **Never run a resolver inside a live rebase of a worktree that is not yours.**
It writes the merged file into the checkout.

## Invariants — read these before trusting a landing

1. **A rebase with 0 conflict rounds means the resolver NEVER RAN.** It does not
   mean the merge was checked. Git will happily fast-forward or auto-merge a
   generated JSON file and produce a result no human or script inspected.
   **Three-way verify anyway.**
2. **A rebase WITH conflict rounds must also be three-way verified.** The
   resolvers hard-stop (exit 3) on anything they cannot merge, but "did not
   stop" is not "was correct" — that is precisely the shape of the v1 incident
   below.
3. **JSON round-trip spellings are load-bearing.**
   * `scripts/target_symbol_map.json` → `json.dumps(d, indent=1, ensure_ascii=False) + "\n"`
   * `scripts/symbol_aliases.json` → `json.dumps(a, indent=1) + "\n"`, with
     `ensure_ascii` **detected from the HEAD file** (`encoding_of`), never assumed.
   Writing either file with any other spelling reformats the whole thing and
   turns the next lane's rebase into a whole-file conflict — which is how a
   merge stops being reviewable.
4. **`resolve_aliases.py --test` exits 1 on ANY difference from EXPECT**,
   including an `evidence`-string-only difference, which is not a merge defect.
   Do not gate on its exit code. `test_rebase_kit.py` calls `three_way()` and
   compares **structurally** instead.
5. **An alias group's structural fields are `survivor`, `folded`, `withdrawn`,
   `address`.** Those decide what objdiff forgives, i.e. what a lost merge
   costs in bytes. `evidence` is prose and carries no score.

## Why v2 exists (the W16-J incident)

v1 replaced a group changed on **both** sides with the lane's copy, and treated
a survivor rename as delete + add. On the W16-J landing that silently discarded
**−3,728 B** of main's concurrent alias memberships. The rebase was clean, the
build was green, and the loss was found by hand afterwards.

v2 pairs groups by composite key `(name, address, survivor)`, re-pairs unpaired
base/theirs groups by **unique address** (a rekey), merges a both-sides-changed
group **field by field**, and exits 3 on anything it cannot merge rather than
guessing.

## The test, and why its control matters

`test_rebase_kit.py` replays the real commits — `OURS=f29132d7` (main at the
moment of the W16-J rebase), `BASE=a8f9e92b` (the lane's base),
`THEIRS=95ebc590` (the lane's pre-rebase tip), `EXPECT=dcd8d6fe` (the
hand-repaired file) — and asserts:

* **v2** reproduces the hand-repaired file structurally: 4 deltas, 1,632 groups,
  **0 structural differences**, and the only field differing anywhere is
  `evidence` on 2 groups (the repair commit wrote its notes by hand; v2 appends
  both sides' notes).
* **v1 FAILS that same check** — measured **2 structural differences, 3 folded
  memberships missing**. That arm is the point: a control that cannot fail
  proves nothing, and this repo has repeatedly been bitten by checks that could
  only come out one way.

It is claimed by `scripts/test_tools.py`'s existing `tools` pytest root, so it
runs with the rest of the suite; no table entry is needed.

The replay needs **full git history**. A shallow checkout SKIPs with an explicit
reason rather than passing vacuously.

## Do not

* Do not change the resolvers' merge semantics without a **failing test first**.
* Do not use `_v1_frozen.py` to land anything. It refuses to run as a script.
