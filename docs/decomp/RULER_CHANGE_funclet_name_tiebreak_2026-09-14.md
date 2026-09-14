# RULER CHANGE 2026-09-14 — funclet pairing gains a relocation-target-NAME tiebreaker (objdiff 4.2.8 → 4.2.9)

**Lane W16-W (Opus), 2026-09-14.** rb3-xenon worktree `~/tmp/wt-w16-w`, branch `w16-w`, off `main`
at `ac608d27`. objdiff branch `funclet-name-tiebreak`, tip **`a5f0ea903ec12e0fe8266f7a27cf9406ece5c311`**,
off `main` at `0321226`.

Acts on the refutation in
[`W16T_DATARESULTLIST_LEAD_FRIENDSPROVIDER_2026-09-14.md`](W16T_DATARESULTLIST_LEAD_FRIENDSPROVIDER_2026-09-14.md)
§"What is actually charged": the `DataResultList` funclet charge is **not** a wrong local type in our
source — both parents already score 100 — it is an artifact of **how objdiff pairs anonymous EH
funclets**. This lane fixes the pairing.

> **This is a RULER change.** Byte absolutes measured under 4.2.8 are not comparable to absolutes
> measured under 4.2.9 without stating the binary. Read `report.json`'s `provenance.tool_binary_hash`
> — do not infer it. Same discipline as
> [`RULER_CHANGE_2026-08-02.md`](RULER_CHANGE_2026-08-02.md).

---

## 1. What was wrong

`funclet_signature` (`objdiff-core/src/diff/mod.rs`) extracts a symbol's bytes and **zeroes the 4-byte
word at every relocation**, then compares. It therefore **drops the relocation target names**. Every MSVC
EH funclet that unwinds one local with the same frame arithmetic collapses onto **one signature key**,
no matter which destructor it calls.

Inside such an equivalence class `pair_funclets_by_bytes` had no information left to choose with:

* **pass 1** paired only classes unique on *both* sides (correct — a forced choice);
* **pass 2** took the two name-sorted candidate lists and **zipped them**;
* **pass 2b** pushed over-subscribed targets many-to-one onto `right_indices.first()`.

So the partner was decided by **symbol-name sort order** — for `__unwind$NNNNNN` that is an MSVC
per-TU counter, i.e. **arbitrary**. Under the shipped `name_check` ruler the resulting `bl` target-name
disagreement is then charged as `diff_arg`, and the row reads `mpn 100 / fuzzy 99.5` — counted in
`matched_functions`, withheld from `matched_code`.

Two measured classes (both re-verified from raw COFF in §5, not taken from W16-T):

| unit | class | retail members | our members | old outcome |
|---|---|---|---:|---:|---|
| `MetaPerformer` | `subi r31,r12,0x70` + `lwz r3,0x84(r31)` | 1 | 4 | paired with a `~Message` funclet; the one `~DataResultList` funclet sorts **last** |
| `RockCentral` | `subi r31,r12,0x90` + `addi r3,r31,0x50` | 5 | 5 | **all five** retail rows read 99.5 — 0/5 correct |

"0 of 5 correct" is what an arbitrary assignment inside an equivalence class looks like, and is not a
property of `DataResultList`.

⚠ **A code comment at the fallback's call site cited
`docs/sessions/2026-05-26-msvc-funclet-pairing.md` "for the full investigation". That file has never
existed in the objdiff repository.** The citation is deleted in this commit rather than left to send
the next reader hunting. Do not go looking for it.

## 2. What changed

Passes 2 and 2b now break the tie on **relocation target NAME agreement**, using the
already-existing `RelocDesc` / `NamedSig` / `named_symbol_signature` descriptors — which were
`#[cfg(all(feature = "std", feature = "bindings"))]` and reachable only from the report-driver's
global pass. They are **un-gated**: the core pairing must not depend on the `bindings` feature.

Per candidate pair we compute `NameEvidence { agree, conflict }` over the relocation sites, and pair
**agree-first, strongest first**, then fall back to the historical zip:

* a site whose **target-side** name is a placeholder (`fn_<hex>`, `lbl_`, `jumptable_`, …) was never
  identified and carries no identity — **skipped**, exactly as `reloc_eq` forgives it under
  `name_check`. It is neither agreement nor conflict;
* a site present on **only one side** is unverifiable (dtk-split target objects carry relocations
  per-site with no coverage guarantee) — **skipped**;
* names asserted co-located by the project's map file (`SymbolEquivalences::aliases`) count as
  **agreement**, as they do in `reloc_eq`;
* anything else that disagrees is a **conflict**, and a pair with any conflict is never preferred.

**Evidence, not requirement.** A class with no name agreement anywhere is paired exactly as before.
**Pass 1 is untouched.** Pass 3 (fuzzy) gains name agreement only as a tie-break *below* byte
similarity. Descriptors are used **fail-open**: a symbol whose descriptors cannot be resolved keeps
its masked bytes and simply offers no evidence, so it can never drop out of a pairing it previously
participated in.

Workspace version **4.2.8 → 4.2.9** — the output changed, and a version constant across materially
different binaries is no identifier.

### Design alternative rejected
Pairing by **parent association** (an EH funclet lives in its parent's COMDAT, so the parent could
name the partner) is strictly more information than a relocation name. It was rejected because the
target side has no COMDAT structure to read — dtk carves retail into flat `.text` spans — so the
parent link exists only on our side, which cannot resolve a *target*-side ambiguity. The relocation
name is the one signal present and comparable on both sides.

## 3. Measures

Both legs: **same worktree**, same tree state (`ac608d27`, no rb3-xenon source change of any kind),
`report.json` + `report.cache` deleted before each, full `./tools/ninja-locked` with `rc` tested from
the log file, ruler `name_check`. **The only variable is the binary.**

| measure | before (4.2.8) | after (4.2.9) | Δ |
|---|---:|---:|---:|
| `provenance.tool_binary_hash` | **`14ac591a0814e6c9`** | **`5a51cd51fe0a353f`** | — |
| `provenance.tool_commit` | `032122696555` | `a5f0ea903ec1` | — |
| `matched_functions` | 43,280 | **43,281** | **+1** |
| `matched_code` | 3,961,432 | **3,985,784** | **+24,352 B** |
| `matched_code_percent` | 38.663372 | 38.901047 | +0.237675 pp |
| `fuzzy_match_percent` | 49.484955 | 49.485336 | +0.000381 |
| `masked_equal_functions` | 23,033 | **23,034** | **+1** |
| `total_functions` / `total_code` | 69,216 / 10,245,956 | 69,216 / 10,245,956 | **0 / 0** |

The baseline leg reproduces the briefed figures **exactly** (43,280 / 3,961,432 / fuzzy 49.484955),
and its `tool_binary_hash 14ac591a0814e6c9` is the live 4.2.8 binary's own `--version` xxh3 — so the
baseline is provably scored by the unmodified fleet binary.

**Denominators are identical**, so this is a pure pairing effect and not a denominator move.

All three measures landed where the pre-registered expectation put them: `matched_functions`
~unchanged (`mpn` is arg-blind and these rows already scored `mpn 100`, so a correct pairing cannot
add to a count that already contains them), `matched_code` up, `masked_equal_functions` ~unchanged
(the *number* of funclet pairings does not change — only which partner each takes).

### Size of the class, and how much of it this closes

| population (mpn == 100 & fuzzy < 100) | rows | bytes | pp of `total_code` |
|---|---:|---:|---:|
| whole class, before | 3,363 | 204,760 | 1.998 |
| of which anonymous `fn_<8hex>` (the only part a funclet pass can reach) | 3,198 | 135,096 | 1.319 |
| anonymous class, after | 2,593 | 110,784 | 1.081 |
| **closed** | **605** | **24,312** | **0.237** |

⇒ the tiebreaker collects **18.0 % of the reachable class**. The named remainder (165 rows /
69,664 B) is out of scope by construction — `is_funclet_like` never sees it.

⚠ W16-T measured this class at 3,366 rows / 204,884 B on its own baseline (`94b0ddae`); this lane
measures 3,363 / 204,760 at `ac608d27`. The small difference is the intervening commits, not a
disagreement — **re-measure, never inherit.**

## 4. Row set-diff

`python3 tools/rowset_snapshot.py diff ~/tmp/rows_w16w_before.json`:

```
CROSSED IN : 622 rows, 24992 B
FELL OUT   :  16 rows,   640 B
NET bytes  : +24352
```

★ **Every one of the 638 moved rows is an anonymous `fn_<8hex>` funclet. ZERO named rows moved, in
either direction.** The change is scoped exactly to the class it targets; no named function's score
changed.

Largest crossings: `BandCamShot::fn_822B3370` (64 B), `band3/game/Stats::fn_82699980` (64 B),
`Character::fn_823741AC` (60 B), `Cache_Xbox::fn_827DBA30` (60 B), `CharBonesMeshes::fn_8237BB10` /
`fn_8237BBB8` (56 B each).

**Both rows W16-T diagnosed by hand crossed in**: `MetaPerformer::fn_825800B0` and
`RockCentral::fn_824F9BAC`, each `fuzzy < 100 → 100.0`. Those are the two rows a human read off retail
bytes and declared unreachable by any amount of source work.

### The 16 fall-outs are reshuffles, not losses

All 16 are 40 B funclets. **All 13 units containing a fall-out also gained rows, and every one of
those units is net ≥ 0:**

| unit | −out | +in | net | | unit | −out | +in | net |
|---|---:|---:|---:|---|---|---:|---:|---:|
| `AccomplishmentPanel` | 2 | 6 | +4 | | `SampleInst` | 1 | 1 | 0 |
| `BandCharacter` | 2 | 4 | +2 | | `SongSortMgr` | 1 | 4 | +3 |
| `Char` | 1 | 1 | 0 | | `UIList` | 1 | 15 | +14 |
| `GemTrackDir` | 1 | 2 | +1 | | `VocalTrackDir` | 1 | 1 | 0 |
| `MeshAnim` | 1 | 14 | +13 | | `band3/game/Stats` | 1 | 4 | +3 |
| `OutfitConfig` | 1 | 7 | +6 | | `system/obj/Dir` | 2 | 10 | +8 |
| `RockCentral` | 1 | 7 | +6 | | | | | |

`RockCentral::fn_825085B0` is adjudicated on bytes in §5 and is **net 0 for its class**: the class
structurally cannot score better than 4/5 under *any* permutation, and the tiebreaker only moved
*which* row absorbs the unavoidable miss.

## 5. Hand-verified sample (5 rows, read from raw COFF)

Verified with an **independent** PE/COFF reader (`~/tmp/coffprobe.py`, sharing no code with objdiff —
it is the check *on* objdiff, not a restatement of it): for each retail funclet, its reloc-masked
signature and the NAME of every relocation inside it; then every symbol of the same size in **our**
compiled object carrying the identical masked signature.

| # | row | size | retail funclet's `bl` target | our same-signature class | verdict |
|---|---|---:|---|---|---|
| 1 | `MetaPerformer::fn_825800B0` | 40 | `??1DataResultList@@UAA@XZ` | 4 members: `__unwind$345266` → `~Message`, `$345358` → `~Message`, `$357547` → `~_String_base`, **`$366621` → `~DataResultList`** | exactly one agrees, and it sorts **LAST** ⇒ only a name tiebreaker reaches it. Row 99.5 → **100.0** |
| 2 | `RockCentral::fn_824F9BAC` | 40 | `??1DataResultList@@UAA@XZ` | 5 members: `$312109` → `~String`, `$312942`/`$313007`/`$313072` → `~DataArrayPtr`, **`$325531` → `~DataResultList`** | one agrees, sorts **last** of five ⇒ 99.5 → **100.0** |
| 3 | `RockCentral::fn_824FA034` | 44 | `??1String@@UAA@XZ` | 4 members: `$327211` → `~slist<pair<const Symbol,int>>`, **`$327787` → `~String`**, **`$327848` → `~String`**, `$342476` → `~slist<…>` | two agree, two conflict; the zip's first pick (`$327211`) conflicted ⇒ 99.5 → **100.0** |
| 4 | `Object::fn_8275ADE4` | 40 | `??1String@@UAA@XZ` | 2 members: `__unwind$43076` → `~DataNode`, **`$44168` → `~String`** | the agreeing one sorts second ⇒ 99.5 → **100.0** |
| 5 | `RockCentral::fn_825085B0` | 40 | `??1Message@@UAA@XZ` | 5 members → `~ServerStatusChangedMsg`, `~UserLoginMsg`, `~FriendsListChangedMsg`, `~ConnectionStatusChangedMsg`, `~ProfileChangedMsg` | **the fall-out.** See below |

In rows 1–4 the name evidence is unambiguous: retail names a destructor, exactly one (or, in row 3,
one of two interchangeable) of our same-signature funclets calls it, and it is **not** the one the
name-sorted zip would pick. The row reaching `fuzzy == 100` is then proof the new partner is the
agreeing one, since `name_check` charges any disagreement.

**Row 5, the fall-out, in full.** Retail has **5** funclet-like members in this class
(`fn_82508470`, `fn_825084C0`, `fn_82508510`, `fn_82508560`, `fn_825085B0`), **all** calling
`??1Message@@UAA@XZ`. We have 5, calling five *different* `Message` subclass destructors — **none** of
them `~Message` itself. `build/45410914/icf_aliases.map` groups 94 symbols at `0x823426F8` (they
ICF-fold onto `~Message`), and **exactly 4 of our 5** are in that group;
`??1FriendsListChangedMsg@@UAA@XZ` is **not**. So in a 5 ↔ 5 class **at most 4 pairings can ever score
100**, under any permutation:

| | before | after |
|---|---|---|
| at `fuzzy == 100` | `fn_82508470`, `fn_825084C0`, `fn_82508560`, `fn_825085B0` | `fn_82508470`, `fn_825084C0`, **`fn_82508510`**, `fn_82508560` |
| below | `fn_82508510` | `fn_825085B0` |

**Net 0 for the class.** The "regression" is the invariant residue moving from one row to another.
This is the residual arbitrary tail the design explicitly keeps: where the names cannot decide, the
zip still decides, and it is still arbitrary.

## 6. Tests

`objdiff-core/tests/funclet_name_tiebreak.rs`, 5 tests, fixture style borrowed from
`masked_equality_symbol.rs` (`build_bl_obj` — N functions whose `.text` bytes are identical because the
branch displacement lives in the relocation, so they share one masked signature and differ only in
target name):

1. **1-vs-N, agreeing partner sorts LAST** — asserts it is chosen.
2. **N-vs-N where every naive zip pairing is wrong** — the RockCentral shape in miniature; asserts both repair.
3. **CONTROL — no agreeing candidate** — asserts the pairing is exactly what it always was.
4. **CONTROL — determinism** — same base funclets emitted in reverse order; asserts the answer does not move.
5. **CONTROL — placeholder target name decides nothing** — a `fn_<hex>` target callee must read as no
   evidence, so the zip stays in charge (and the site is not charged under `name_check`).

**Proved able to fail.** Stubbing `NameEvidence::is_positive` to `false`:

```
test control_no_agreeing_candidate_still_pairs_unchanged ... ok
test placeholder_target_name_carries_no_evidence ... ok
test n_vs_n_repairs_a_wholly_wrong_zip ... FAILED
test one_vs_n_picks_the_name_agreeing_partner_even_when_it_sorts_last ... FAILED
test choice_is_independent_of_emission_order ... FAILED
test result: FAILED. 2 passed; 3 failed
```

— the three tiebreaker tests go red (the 1-vs-N row reads **97.5** instead of 100.0, the mechanism in
miniature) and **both controls stay green**, which is the discrimination that matters: a sabotage that
reddened everything would prove the tests only detect "something changed".

Full `cargo test -p objdiff-core`: **135 tests, 0 failures**, including the pre-existing
`masked_equality` / `masked_equality_symbol` funclet-pairing contracts.

⚠ **The release build did not compile four in-crate `#[cfg(test)]` call sites** of
`pair_funclets_by_bytes`; only `cargo test -p objdiff-core` caught them. A green
`cargo build --release -p objdiff-cli` is **not** evidence the crate's tests compile.

## 7. Gates (rb3-xenon worktree, under the NEW binary)

* full `./tools/ninja-locked` — **rc=0** (`~/tmp/rb3_build_w16w_2.log`)
* `python3 scripts/verify_ruler_agreement.py --check` — **rc=0**, *"OK: both objdiff-cli entry points resolve the same ruler."*
* `python3 scripts/verify_objs_patched.py --verify-manifest` — **rc=0**, `1212 decomp, 3101 target objects match` (`tree_sha256=0475f32dfa76aa71`)
* `tools/native_build_gate.sh`:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

⚠ `verify_ruler_agreement.py` invokes `bin/objdiff-cli`, which resolves to the **live 4.2.8** binary,
not the staged one. Its PASS therefore says the *config* keys agree; it is not a 4.2.9-vs-4.2.9
score comparison. Re-run it after the swap.

## 8. Cross-repo effect — MEASURED, not caveated

`bin/objdiff-cli` is one binary symlinked by **three** repos. Both other repos were measured
read-only with `--no-cache` and `-o` into `~/tmp` (their trees were **not** rebuilt and their
`report.json` files were **not** touched — mtimes unchanged). Both binaries were run on the same tree
with identical config, so the binary is the only variable.

| repo | `matched_functions` | `matched_code` | `masked_equal_functions` |
|---|---|---|---|
| **rb3-xenon** | 43,280 → 43,281 (+1) | 3,961,432 → 3,985,784 (**+24,352**) | 23,033 → 23,034 (+1) |
| **dc3-decomp** | 31,033 → 31,033 (**0**) | 5,469,268 → 5,473,144 (**+3,876**) | 1,363 → 1,363 (**0**) |
| **rb3 (Wii)** | 31,942 → 31,942 (**0**) | 7,219,476 → 7,219,476 (**0**) | 0 → 0 (**0**) |

* The dc3 **4.2.8 leg reproduces dc3's own stored `report.json` measures exactly** (31,033 /
  5,469,268 / 1,363), which validates the rig rather than assuming it.
* dc3 moves ~6× less than rb3-xenon, consistent with its funclet population: `masked_equal_functions`
  1,363 vs our 23,033 — dc3 has a leaked map, so its functions are named and far fewer fall to the
  anonymous-funclet fallback.
* **rb3 (Wii) is exactly 0 on every measure, and its `masked_equal_functions` is 0** — the funclet
  byte-pairing fallback never fires there at all (MWCC, no MSVC EH funclets). The change is
  **structurally inert** on that repo, not merely measured-flat.

⇒ Landing this is **safe for all three repos and positive for two**. The coordinator should still
re-run each repo's own gates after the swap; these figures are `report generate` only.

## 9. Rollback

```bash
# the live path, shared by rb3-xenon, ../rb3 and ../dc3-decomp:
#   /home/free/code/milohax/objdiff/target/release/objdiff-cli
cp ~/tmp/objdiff-backup/objdiff-cli.4.2.8-a5c35b15 \
   /home/free/code/milohax/objdiff/target/release/objdiff-cli
sha256sum /home/free/code/milohax/objdiff/target/release/objdiff-cli
# must print a5c35b15d7d46ac4712a6b1c1010409e69939158b94dd8e1d36e4c115a2623d8
```

Backup: `~/tmp/objdiff-backup/objdiff-cli.4.2.8-a5c35b15`, **11,946,224 B**, sha256
`a5c35b15d7d46ac4712a6b1c1010409e69939158b94dd8e1d36e4c115a2623d8` — verified **byte-identical to the
currently-live binary** by this lane, not merely assumed.

Staged 4.2.9: `~/tmp/objdiff-build/release/objdiff-cli`, **11,990,000 B**, sha256
`c1b7d95240a35cd6a543ea44de459679e7d718a1af595bc4aef8ca1f84353eb8`,
`objdiff-cli 4.2.9 (a5f0ea903ec1, xxh3 5a51cd51fe0a353f)`.

**The swap is the coordinator's, not this lane's.** The live binary was never modified: its sha256 and
its Sep 1 mtime were re-checked after every build here. Note the binary is **not** a ninja input, so a
swap triggers no recompiles — which is exactly why a bad one goes unnoticed; after swapping, wipe
`report.json` + `report.cache` and do a full build in each repo.

## 10. Traps found on the way (worth reusing)

★ **`configure.py` in a `~/tmp` worktree does NOT resolve objdiff the way main does.**
`_find_local_fork` walks *upward* looking for a sibling `objdiff/Cargo.toml` and only consults
`RB3_OBJDIFF_DIR` **if the walk fails**. From `~/tmp/wt-*` the walk **succeeds** — because
`/home/free/tmp/objdiff` is a **symlink** to `/home/free/code/milohax/objdiff` — so it bakes
`/home/free/tmp/objdiff/...` into `build.ninja` and the env override is **unreachable**. The binary is
the same file, so nothing breaks; but the command *strings* differ from main's, which is the property
CLAUDE.md calls load-bearing for warm-worktree command-hash reuse. **Pass `--dtk/--objdiff/--wrapper`
explicitly, as `scripts/setup_worktree.sh` does.** This was found only by executing the resolution and
printing the answer instead of reasoning about it from the docstring.

⚠ **`import configure` RUNS configure and rewrites `build.ninja`.** This lane imported it to probe
`_find_local_fork` and thereby mutated the worktree's `build.ninja` — and then took its "backup"
*after* the mutation, so the backup was of the already-rewritten file. Recovered by regenerating with
the canonical arguments and **byte-comparing against main's `build.ninja`** (§11), which is the only
honest verification. Same family as the standing rule: *when an artifact matches a baseline you did
not expect it to match, `cmp` against every candidate.* No measurement was affected — the baseline
build predates the import and its `tool_binary_hash` proves the live 4.2.8 scored it.

## 11. Worktree `build.ninja` restored

`build.ninja` was pointed at the staged binary **for the measurement only**, via
`python3 configure.py --dtk … --objdiff ~/tmp/objdiff-build/release/objdiff-cli --wrapper …`, and then
regenerated with the canonical live path:

```bash
python3 configure.py \
  --dtk     /home/free/code/milohax/jeff/target/release/dtk \
  --objdiff /home/free/code/milohax/objdiff/target/release/objdiff-cli \
  --wrapper /home/free/code/milohax/wibo/build/release/wibo
```

Verified **by bytes**, not by assumption:

* `command grep -c 'objdiff-build' build.ninja` → **0**; `command grep -c '/home/free/tmp/objdiff/'` → **0**;
* lines 5 and 36 are byte-identical to the pristine file read at lane start
  (`--objdiff /home/free/code/milohax/objdiff/target/release/objdiff-cli $` and
  `build/binutils /home/free/code/milohax/objdiff/target/release/objdiff-cli`);
* `diff` against main's `build.ninja` is **6 lines, one hunk, `configure_args` only** — main's is
  empty (a bare reconfigure there), the worktree's records the three explicit flags, which is what
  `setup_worktree.sh` writes and the form a worktree *must* keep (an empty `configure_args` in a
  worktree would re-resolve objdiff to `/home/free/tmp/objdiff`, per §10).
  Worktree `a870af5ebaa492eaa0ea784ad72ff707a209ca61175164ed61ac436b3fd9b0ce`;
  main `accb0390df518f9b6efe02300496a5cdb341f709ade0f7a6d69aaf723090fbb8`.

Every tool path and every command string is identical to main's. The coordinator will rebuild it
under whichever binary is live.

## 12. Deliberately NOT done

* **The live binary was NOT swapped.** Out of scope; the coordinator owns it.
* **`../rb3` and `../dc3-decomp` were NOT rebuilt.** Only `report generate` was run against their
  existing built trees, read-only, `--no-cache`, output to `~/tmp`. Their gates were not run.
* **The secondary item (`fn_82654440` in `SessionUsersProviders`, 76 B) was NOT attempted.** The LEAD
  consumed the lane. It remains unnamed since W16-J; no oracle lookup was performed here, so nothing
  is claimed about it either way.
* **The residual arbitrary tail was NOT removed.** 2,593 anonymous rows / 110,784 B remain at
  `mpn == 100 & fuzzy < 100`. Much of it is structurally unreachable by *any* name tiebreaker: classes
  where the target callee is a placeholder (no identity to match), classes where our side has no
  agreeing member at all, and over-subscribed classes like §5 row 5 that cannot score 100 on every
  member under any permutation. **Do not price the remaining 110,784 B as a lever without first
  measuring how much of it carries name evidence at all** — this lane did not measure that split.
* **Parent-association pairing was NOT implemented** (§2, rejected with reason).
* **Optimal (Hungarian) assignment was NOT implemented.** Phase A is greedy by agreement count. A
  strictly optimal assignment could in principle beat it inside a class; it was not attempted and its
  headroom is unmeasured — though §4's "every fall-out unit is net ≥ 0" bounds it as small.
* **No rb3-xenon source file was changed.** This lane's only rb3-xenon artifact is this document.
