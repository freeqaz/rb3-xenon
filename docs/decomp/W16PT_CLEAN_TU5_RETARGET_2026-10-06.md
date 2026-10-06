# W16-PT — retarget the decomp from the RB3 Deluxe xex to clean retail TU5

**Lane:** W16-PT. Worktree `~/tmp/wt-w16-pt`, branch `w16-pt` off main `dc238f637`.
**Status:** FINAL for the branch. Main's `orig/45410914/` still holds the RB3DX image. Swap it with
§5 **in the same window as the merge**.
**Ruler:** `name_check` (objdiff 4.2.9, `tool_binary_hash 5a51cd51fe0a353f`), read from each leg's
own `report.json`.

## 0. Verdict

| question | answer | evidence |
|---|---|---|
| Which image should the decomp target? | **Clean retail TU5**: `default.xex` sha1 `d56e7f31…`, extracted PE sha1 `5f3f667a…` | §1 |
| How do the two images differ? | **53 words / 170 bytes**, all inside file sections, none in the PE headers, section tables identical | §2, `tools/pe_word_diff.py` |
| Does anything besides the image need to change? | `splits.txt` and `symbols.txt`: **no**, both byte-identical after a forced re-split on the clean image. Three `src/` sites: **yes**, they were written to the RB3DX patches (§4). | §3, §4 |
| Acceptance: does every moved row sit inside a patch group, with nothing else moving? | **Yes, on 73,262 rows.** Image only: 5 function rows moved (plus their 5 unit `.text` aggregates), each containing at least one patched word. Image plus source: the 5 rows below, all at 100. | §3 |
| Net effect, before → after (RB3DX + old source → clean + this branch) | **+5 functions / +1,240 B**, `matched_code_percent` 57.890205 → 57.902306; `total_code` and `total_functions` unchanged | §3 |
| Can the tree be built against the wrong image by accident after the merge? | **No.** `config/45410914/build.sha1` records the xex hash; the `CHECK TARGET IMAGE` edge fails the build before the SPLIT. Negative control measured. | §4.2 |

## 1. The images

| image | xex sha1 | xex sha256 | PE sha1 (`band.exe`) | what it is |
|---|---|---|---|---|
| **clean retail TU5** (new target) | `d56e7f31101f7851c96342349d6f9daff0223753` | `941ecfde…` | `5f3f667a689e804c1efc736cae4ff2416a278747` | base TU0 (encrypted retail) + `tu5/default.xexp`, applied with `tools/xexp-apply` on 2026-07-07 (`docs/plans/clean-tu5-vs-rb3dx-divergence.md` §1). Lives at `_tu5probe/clean/clean_tu5.xex` in main (gitignored). |
| RB3 Deluxe release (old target) | `c5a17091cb44c0119424390a1738d161995e430e` | `6639ce25…` | `2fbdbc6b279e05363517274ab5a09a9ea909ed88` | clean TU5 with the 53 words of §2 patched in place. Same file as `RB3DX-Xbox/default.xex` and `orig/45410914/default_tu5.xex`. |
| vanilla retail TU0 | `35adb6b4eadab3b0aae354e20ed45781ff0b8fc8` | | | `orig/45410914/tu0-archive/`, target until 2026-07-15 |

Both TU5 xexes decode as Retail / Uncompressed / Unencrypted under dtk 1.14.0 (`67e4a0f9`), with entry
`0x8283CD20`, file time `0x4E60C7FE` (Fri Sep 02 2011) and version 0.0.5.1. The clean xex is 1.7 MB larger
on disk; the extracted PEs are both 14,363,648 B. The live dtk extracts the clean xex to a PE with sha1
`5f3f667a` (measured in this lane, matching the 2026-07-07 record).

**Provenance check, run in this lane with `tools/xex2pack/xex2pack.py --verify`** (prediction: the clean
image's page-hash chain verifies and the Deluxe one does not):

| image | page-hash chain | HeaderHash |
|---|---|---|
| clean TU5 `d56e7f31` | **verifies** (no failure, not skipped) | fails — `xexp-apply` rewrote the file-format header and did not recompute it |
| RB3DX `c5a17091` | **breaks at descriptor 0** (VA `0x82000000`; that page holds the patched import ordinals at `0x8200068C`) | verifies |
| TU0 archive | verifies | verifies |

So the clean image's code and data pages are the ones Microsoft's signed chain describes, and the Deluxe
image's are not. The HeaderHash failure on the clean xex concerns only the header the applier edited; it
does not touch anything the split reads.

**A mislabel corrected along the way.** `docs/plans/tu5-acquisition.md`,
`docs/plans/si-hw-fix/console-bytes.md` and `tools/oss-xbox-build/deploy-si-rb3dx/DEPLOY.md` call sha256
`6639ce25…` "clean TU5". It is the sha256 of the RB3DX xex `c5a17091` (measured: `sha256sum
orig/45410914/default.xex`). Banners/inline notes added; nothing else in those docs was changed.

## 2. The 53 words (re-derived in this lane, `tools/pe_word_diff.py`)

Prediction from W16-R §1: 53 words / 170 bytes in 10 groups. Measured: **53 words / 170 bytes**, the same
VAs. Old = RB3DX, new = clean TU5.

| group | VA(s) | section | report row containing it | what Deluxe changed |
|---|---|---|---|---|
| 1 MOGG key path | `0x8200068C`, `0x82000690` | .rdata | — (import records) | xboxkrnl ordinals 0x24B/0x242 → 0x15B/0x159 |
| | `0x82727698`, `…76B8`, `…76D4`, `…76DC` | .text | `?HvDecrypt@ByteGrinder@@QAAXPAE0H@Z` (108 B) | 0x70 frame + key slot 0 → 0x200 frame + stack AES state |
| | `0x82840794/98`, `0x828407C8` | .text | `fn_82840788` (`auto_03_8283F238_text`, 0%) | key-slot range check → nops |
| | `0x82840828/2C/30`, `0x82840868/6C/70`, `0x828408B4` | .text | `fn_82840820` (same unit, 0%) | same, decrypt side |
| | `0x82C4C47C/80/8C/90` | .text (import thunks) | — | thunk ordinals |
| | `0x82C76258`–`0x82C76294` (16 words) | .data | — (`gHvKeyGreen`, not scored) | key table replaced with plain AES keys |
| 2 debugger check | `0x82272E90` | .text | `main` (76 B) | `bl App::Run` → `bcl` to `RunWithoutDebugging` |
| 3 splash wait | `0x82270F40`, `0x82270F84` | .text | `fn_82270E68` (1,864 B, unpaired, 0%) | `beq` → `nop`, `bl` → `nop` |
| 4 SetDiskError | `0x82516320`–`0x82516338` (7 words) | .text | `?SetDiskError@PlatformMgr@@QAAXW4DiskError@@@Z` (208 B) | entry `blr` + trampoline |
| 5 DataSet guard | `0x8275D6E0` | .text | `?DataSet@@YA?AVDataNode@@PAVDataArray@@@Z` (140 B) | `mr r4,r30` → `b 0x82516324` |
| 6 UGC check | `0x82575F9C` | .text | `?IsDemo@BandSongMgr@@QBA_NH@Z` (164 B) | `bne` → `nop` |
| 7 song blacklist | `0x82579098` | .text | `?AddSongData@BandSongMgr@@…` (652 B) | `bl 0x825755B8` → `li r3,0` |
| 8 update prefix | `0x82089B40/44` | .rdata | — (string, not scored) | `"UPDATE:"` → `"D:"` |
| 9 cache name | `0x82089518` | .rdata | — (string, not scored) | `"songcache"` → `"rbdxcache"` |
| 10 strcpy→strncpy | `0x82AE6880` | .text | `?T2Char8@StringConversion@Quazal@@…` @ `0x82AE6860` (52 B, 0% both legs) | `bl 0x82BBCB50` → `bl 0x8282B238` |

## 3. Whole-binary before/after

Three legs, one worktree, same commit for A and B. Every leg: renamer stamp removed, `config.yml`
touched (forced re-split), built to a `symbols.txt` fixed point (stable from the first iteration on
every leg), `report.json` and `report.cache` deleted, report rebuilt. Script: `~/tmp/w16pt/leg.sh`;
reports `~/tmp/w16pt/report_{A,B,C}.json`; logs `~/tmp/rb3_build_w16pt_{A,B,C}_*.log`.

| leg | image | source | matched_functions | matched_code | matched_code_% | fuzzy_% |
|---|---|---|---:|---:|---:|---:|
| A | RB3DX `c5a17091` | main `dc238f637` | 53,505 | 5,932,468 | 57.890205 | 63.695568 |
| B | clean `d56e7f31` | main `dc238f637` | 53,508 | 5,933,436 | 57.899654 | 63.695892 |
| C | clean `d56e7f31` | this branch | **53,510** | **5,933,708** | **57.902306** | **63.695946** |

`total_functions` 68,909, `total_code` 10,247,792 and `masked_equal_functions` 25,193 are identical on all
three legs. `splits.txt` (`7ef9a615`) and `symbols.txt` (`debaf477`) are byte-identical on all three.

**Acceptance test** (`tools/report_row_diff.py`, which maps every moved function row to a VA and requires a
differing word inside `[va, va+size)`; shown to FAIL when one word is withheld from its input):

A → B (image only): 73,262 rows each side, 0 unpaired, **VERDICT PASS**.

| row | size | fuzzy A → B | patched words inside |
|---|---:|---|---|
| `DataSet` | 140 | 98.2857 → **100** | `0x8275D6E0` (g5) |
| `SetDiskError` | 208 | 85.1731 → **100** | 7 words (g4) |
| `AddSongData` | 652 | 99.6319 → **100** | `0x82579098` (g7) |
| `main` | 76 | 96.8421 → **100** | `0x82272E90` (g2) |
| `IsDemo` | 164 | 98.5366 → 99.8781 | `0x82575F9C` (g6) |
| `HvDecrypt` | 108 | 100 → **95.4815** | 4 words (g1) |

plus the `.text` aggregates of the five units holding them (DataFunc, PlatformMgr, BandSongMgr, Main,
ByteGrinder). Nothing else moved. (`main` and its neighbour `fn_82272EB4` swap their objdiff `address`
key between legs; the tool pairs them by name, and `fn_82272EB4` is 40 B / 100 / masked on both.)

The two partial rows are the source written to Deluxe bytes (§4). A → C (image + this branch): **VERDICT
PASS**, five rows, all at 100: DataSet +140, SetDiskError +208, AddSongData +652, main +76, IsDemo +164 =
**+1,240 B / +5 functions**; HvDecrypt is 100 on both legs. B → C (source only, clean image): exactly
`IsDemo` 99.8781 → 100 and `HvDecrypt` 95.4815 → 100 (+272 B / +2 functions), nothing else.

The W16-R §1 prediction was DataSet +140, IsDemo +164, AddSongData +652, SetDiskError +208 "iff the
residue is only the head", `main` "likely". All five crossed. IsDemo needed the source fix; it did not cross
on the image alone. The HvDecrypt regression was not predicted: that row was matched to the Deluxe bytes on
2026-10-01 (`cac1bd4b0`), after W16-R wrote its table, when it was still `fn_82727688` at 0%.

Groups 3, 8, 9, 10 and the import/data parts of 1 moved no row. Their rows are either unpaired (0% on
both images) or not scored data.

## 4. What the branch changes

### 4.1 Source written to Deluxe bytes (commit `2d3f2e042`)

- **`ByteGrinder::HvDecrypt`** (`src/system/synth/ByteGrinder.cpp`). The match-build path keyed a 0x190-byte
  stack AES state, which is Deluxe's reroute. Clean TU5 passes key slot 0 to both XDK wrappers
  (`0x82840788`/`0x82840820`: slot < 8 or `0x585`, non-null pointer or `0x57`, length 16 or `0x18`, then the
  xboxkrnl key-slot call on slot `0xE0 + index`) and uses a 0x70 frame. `gHvKeyGreen` now carries the clean
  `.data` bytes at `0x82C76258` in the match build; the `HX_NATIVE` build keeps the plain AES keys its
  libtomcrypt path needs.
- **`BandSongMgr::IsDemo`** (`src/band3/meta_band/BandSongMgr.cpp`). Clean TU5 returns false when `IsUGC()`
  is **not** set. The source had the opposite test, which matched neither image (Deluxe nops the branch).
- **`gIgnoredContent`** (`src/system/os/ContentMgr_Xbox.cpp`): last entry `"songcache"`, not Deluxe's
  `"rbdxcache"`. Not a scored row: Δ0 (measured, B → C), done for accuracy.

`DataSet`, `SetDiskError`, `AddSongData` and `main` needed no edit; their source already described clean TU5.

### 4.2 The image guard (commit `e35e751de`)

- `config/45410914/build.sha1` was a DOL-template placeholder (`0123…  build/GAMEID/main.dol`). It now holds
  one line, `d56e7f31…  orig/45410914/default.xex`. `scripts/verify_objs_patched.py` already lists it as an
  input that "identifies the binary"; now it does.
- `scripts/verify_target_image.py` checks every line of that file. `tools/project.py` adds a
  `CHECK TARGET IMAGE` edge (its own always-dirty phony, because the shared `always` is only declared once
  `config.json` exists) with write-if-changed + restat, and the SPLIT edge takes its stamp as an implicit
  input. `ab_measure` already treats `CHECK` edges as non-work.
- Measured in the worktree:
  - first build after configure: `CHECK TARGET IMAGE` then one SPLIT;
  - the next build: the check runs and **no SPLIT** (restat holds);
  - **negative control**: the RB3DX xex copied into the worktree, `./tools/ninja-locked` → **rc=1 at
    `[1/3] CHECK TARGET IMAGE`, no SPLIT**. The message names both hashes and their identities and points
    here.
- `scripts/test_verify_target_image.py` (5 tests, collected by `scripts/test_tools.py`'s `scripts` root):
  matching image passes and writes the stamp, a changed image fails and writes nothing, a missing file
  fails, an empty manifest is rc 2, and the repo manifest names exactly one known xex.

### 4.3 Tools and docs that named the old image

| file | change |
|---|---|
| `config/45410914/config.yml` | header rewritten: clean TU5 target, both hashes, history (TU0 → RB3DX → clean) |
| `tools/tu5_va.py` | default PE was `orig/45410914/band_tu5.exe`, a Deluxe-PE copy that no longer exists; now `orig/45410914/band.exe` |
| `tools/xex2pack/xex2pack.py` | docstring: which image fails which hash check |
| `tools/ghidra/pyghidra-service.sh`, `run_apply_symbols.sh` | comments: the `default_tu5.xex-c5a170` program is the Deluxe image; same VAs, differs only inside §2 |
| `tools/pe_word_diff.py`, `tools/report_row_diff.py` | new: the two instruments behind §2 and §3 |
| `CLAUDE.md`, `docs/INDEX.md`, the three mislabelled docs of §1, W16-R §1 | pointers to this doc |

**Checked and left alone, on purpose:**

- `tools/oss-xbox-build/rb3dx_port_audit.py` compares the Deluxe xex at
  `../rock-band-3-deluxe/platform/xbox/default.xex` with `_tu5probe/clean/band_clean_tu5.exe`; it never reads
  `orig/` and is about the Deluxe image by design.
- `tools/tu5_map_build.py` and `tools/tu5_skel_recover.py` still default to `band_tu5.exe`. They are the
  2026-07 TU0 → TU5 migration tools and take explicit paths; not retargeted.
- `tools/xdbg.py` pulls `default.xex` from the console, which runs Deluxe; unrelated to `orig/`.
- `tools/scope_data/xdk.json` names IAT `0x8200068C` `NtCreateFile` and `0x82000690` `RtlInitAnsiString`.
  That is wrong for both images (the ordinals there are `0x24B`/`0x242` clean, `0x15B`/`0x159` Deluxe). It
  has no generator in the tree and predates this lane; recorded, not fixed.
- No tool hardcodes the xex or PE sha1 besides the docs above. Searched with `git grep` for `c5a17091`,
  `2fbdbc6b`, `5f3f667a`, `d56e7f31`, `clean_tu5`, `rbdxcache`, `band_tu5.exe`, `default_tu5.xex` over
  `tools/ scripts/ configure.py .github/`.

## 5. Swapping main's image (exact steps)

**Order.** Do steps 1–2 and the merge (step 3) in one sitting, under the build lock. Either order is safe,
and the two failure modes differ:

- **merged, image not swapped:** every build fails at `CHECK TARGET IMAGE`. Loud, nothing wrong is measured.
- **swapped, not merged:** builds succeed, and `HvDecrypt` reads 95.4815 and `IsDemo` 99.8781 until the
  merge (−108 B and the +164 B not yet collected). Quiet. Do not leave main in this state.

```bash
R=/home/free/code/milohax/rb3-xenon          # main repo
CLEAN=$R/_tu5probe/clean/clean_tu5.xex      # d56e7f31…; rebuild recipe in docs/plans/clean-tu5-vs-rb3dx-divergence.md §1
O=$R/orig/45410914

# 0. preflight: must print PREFLIGHT OK; on PREFLIGHT FAILED do not continue
[ "$(sha1sum < $O/default.xex | cut -c1-40)" = c5a17091cb44c0119424390a1738d161995e430e ] &&
[ "$(sha1sum < $O/band.exe    | cut -c1-40)" = 2fbdbc6b279e05363517274ab5a09a9ea909ed88 ] &&
[ "$(sha1sum < $CLEAN         | cut -c1-40)" = d56e7f31101f7851c96342349d6f9daff0223753 ] &&
[ ! -e $O/rb3dx-archive ] && echo "PREFLIGHT OK" || echo "PREFLIGHT FAILED"

# 1+2. under main's build lock (the same flock tools/ninja-locked takes), archive and install
(
  flock 9
  mkdir -p $O/rb3dx-archive
  cp --reflink=auto -p $O/default.xex $O/rb3dx-archive/default.xex
  cp --reflink=auto -p $O/band.exe    $O/rb3dx-archive/band.exe
  (cd $O/rb3dx-archive && sha1sum default.xex band.exe > SHA1SUMS)
  # new mtime on purpose: it makes the SPLIT re-run
  cp --reflink=auto $CLEAN $O/default.xex.new && mv $O/default.xex.new $O/default.xex
) 9>$R/.ninja-build.lock
cat $O/rb3dx-archive/SHA1SUMS               # c5a17091… default.xex / 2fbdbc6b… band.exe
sha1sum $O/default.xex                       # d56e7f31…
# Leave $O/default_tu5.xex (RB3DX bytes) where it is: the Ghidra program default_tu5.xex-c5a170 is
# addressed by that path, and moving it would import a duplicate program.

# 3. land the branch (rebase onto main first; merge --no-ff with a real message)
git -C ~/tmp/wt-w16-pt rebase main
git -C $R merge --no-ff w16-pt

# 4. build: forced re-split, then a fresh report
cd $R
rm -f build/45410914/target_symbol_renames.stamp && touch config/45410914/config.yml
./tools/ninja-locked 2>&1 | tee ~/tmp/rb3_build_w16pt_swap.log
rm -f build/45410914/report.json build/45410914/report.cache
./tools/ninja-locked build/45410914/report.json 2>&1 | tee -a ~/tmp/rb3_build_w16pt_swap.log

# 5. verify
sha1sum $O/band.exe                          # 5f3f667a… (re-extracted by the split)
python3 scripts/verify_target_image.py       # OK d56e7f31…  orig/45410914/default.xex
git -C $R status --short config/             # empty: splits.txt / symbols.txt unchanged by the re-split
```

**Step 5, the numbers.** Save main's `report.json` before step 1 as the before leg. Then
`python3 tools/pe_word_diff.py $O/rb3dx-archive/band.exe $O/band.exe --json /tmp/w.json` (53 words) and
`python3 tools/report_row_diff.py <before>/report.json build/45410914/report.json --words /tmp/w.json` must
print `VERDICT: PASS` with exactly the five rows of §3 crossing to 100 (+5 functions, +1,240 B). If main has
moved since `dc238f637`, the absolute totals differ; the five rows and the deltas should not.

**Rehearsal.** The block above was run verbatim in the worktree, with `R`/`O` pointed at the worktree and the
RB3DX xex put back first (§6).

**Every other tree.** Worktrees made by `scripts/setup_worktree.sh` after the swap reflink the clean
image. An existing worktree keeps its own `orig/` copy and, once it rebases past the merge, fails at
`CHECK TARGET IMAGE`. Fix it by copying the image across, with a new mtime:

```bash
cp --reflink=auto /home/free/code/milohax/rb3-xenon/orig/45410914/default.xex <wt>/orig/45410914/default.xex
```

A worktree that has **not** rebased keeps building against Deluxe with the old source, which is
self-consistent; its numbers are on the old image and do not compose with post-swap numbers. Compare A/B legs
only within one image.

**Outside the repo (not done here):**

- CI: `.github/workflows/build.yml` copies `/orig` from the container `ghcr.io/rjkiv/rb3-xenon-build:main`,
  which holds an older image (runbook 2026-07-15 §F6, external owner). The new guard fails a CI build with
  the wrong image instead of producing wrong numbers.
- `decomp.db`: re-ingest after the first post-swap report (`venv/bin/python scripts/ingest_report.py
  build/45410914/report.json`); only the five rows move.
- Ghidra: the RB3Xenon project keeps the Deluxe program. Decompiles and xrefs are valid everywhere except
  inside §2's words. Importing the clean xex as a third program is optional, should run with the service
  stopped, and is not needed by the build.

**Rollback.** `git revert -m 1 <merge>`, then put the Deluxe image back under the same lock:
`cp --reflink=auto $O/rb3dx-archive/default.xex $O/default.xex.new && mv $O/default.xex.new $O/default.xex`,
then a forced re-split (step 4).

## 6. Rehearsal record

The §5 block was extracted from this file by script and run under **zsh** in the worktree, with
`R=$HOME/tmp/wt-w16-pt`, `CLEAN` pointed at main's `_tu5probe/clean/clean_tu5.xex`, and the two git lines of
step 3 commented out (the branch was already checked out). Before the run, the worktree's `default.xex` and
`band.exe` were put back to the Deluxe files.

| step | result |
|---|---|
| 0 preflight | `PREFLIGHT OK` |
| 1+2 archive + install | `rb3dx-archive/SHA1SUMS` = `c5a17091… default.xex`, `2fbdbc6b… band.exe`; `default.xex` = `d56e7f31…` |
| 4 build | `[1/3] CHECK TARGET IMAGE`, `[2/3] SPLIT`; the report build ran the check and no SPLIT; rc 0 |
| 5 verify | `band.exe` = `5f3f667a…`; `verify_target_image: OK`; `git status --short config/` empty |
| 5 numbers | `pe_word_diff` 53 words / 170 bytes (archive vs new `band.exe`); `report_row_diff` leg A → rehearsal: 9 moved rows, **PASS**, +5 functions / +1,240 B; leg C → rehearsal: **0 moved rows** |

So one forced split on the swapped image reproduces leg C exactly.

## 7. Not done

- The Ghidra program, the CI container and `decomp.db` (§5, outside the repo).
- `xdk.json`'s wrong IAT names (§4.3), recorded only.
- `fn_82270E68` (group 3, App::Init-shaped, attributed to `RhythmDetector`) and `fn_82840788`/`fn_82840820`
  (group 1, XDK) stay unpaired and unnamed. On the clean image their bytes are now the real retail ones,
  so naming or porting them no longer needs a caveat.
- The `__find<Symbol*>` vs `__find<const Symbol*>` call in `IsDemo` is charged as a relocation-name difference,
  which does not stop the row from scoring 100 (`fuzzy` 100, `mpn` 100). Not investigated.
