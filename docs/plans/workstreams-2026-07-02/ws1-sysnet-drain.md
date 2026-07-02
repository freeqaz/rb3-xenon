# WS1 — SYSNET ghidriff-worklist drain (band3 vein's twin)

**Stream:** #1 in `docs/plans/frontier-workstreams-2026-07-02.md` (master doc).
**Written:** 2026-07-02, by a doc-prep agent that re-verified every count below itself.
**Repo:** `/home/free/code/milohax/rb3-xenon`. Main was `@44f57c6` at master-doc time and
`@62b816a` two hours later — **HEAD and `scripts/target_symbol_map.json` move hourly;
every count in this doc must be re-derived at execution time (Phase 0).**

## Objective

Drain the system/network ghidriff worklist — 516 Wii→Xenon identities at 0.967
human-validated precision (`docs/plans/sysnet-port-worklist.md`) — the same way the
band3 worklist vein was fully drained (memory topic
`~/.claude/projects/-home-free-code-milohax-rb3-xenon/memory/project_worklist_drain_close_2026-07-02.md`):

- **Wave A (pin-only):** run the deterministic pin tool over the identities that live
  in already-wired (compiled) TUs. One worktree, hours of work, no porting.
- **Wave B (wire-only):** 9 unwired TUs whose `.cpp` already exists in this repo —
  just add to `objects.json`, compile, pin.
- **Wave C (port-then-pin):** the proven v2 lane workflow (Sonnet ports, Fable
  scopes+reviews, NO whole-binary builds in lanes) over the unwired TUs that have an
  rb3-Wii (and sometimes DC3) source oracle.
- **Park** the 144 Quazal/ObjDup identities that have **no source oracle anywhere**
  (not portable — naming reserve for the manual-reconstruction era, ws6).

## Current state (verified 2026-07-02 ~03:00, main @62b816a)

All verified by running the commands in "How to re-derive" below — do NOT trust these
numbers cold, they decay as concurrent agents land pins.

- `sysnet_port_worklist.json` (repo root, **gitignored/regenerable**): dict with keys
  `_meta`, `tu_summary` (276), `ranked_tus` (276), `worklist` (**516 rows**). Row keys:
  `rb3_addr, wii_addr_bank8, wii_symbol, wii_demangled, tu, category, src_path,
  src_exists, match_type, match_types, confidence_label, simconf, dc3_cannot_provide`.
- `scripts/target_symbol_map.json`: 13,671 keys at check time.
- **Net-new** (address join: lowercase, strip `0x`, lstrip zeros, zfill 8; `rb3_addr`
  not a map key): **361** at my check. The coordinator measured **384** earlier the
  same day — the delta is real concurrent consumption, not an error. (~105 were
  consumed at commit `f9f0d23` "pin: name/micro-pin ~105 sysnet worklist identities
  in wired TUs", which ran `band3_worklist_pin.py --worklist
  sysnet_port_worklist_filtered.json --all-wired --apply`; that filtered file no
  longer exists on disk and is not needed.)
- **Segmentation of the 361** (using the pin tool's exact `is_wired` semantics =
  the quoted `src/`-stripped path appears in `config/45410914/objects.json`):

  | Segment | Rows | TUs | Notes |
  |---|---|---|---|
  | **A: wired (compiled)** | 101 | 63 | 0 are DC3-unreachable. Live pin-tool dry-run: **68 nameable** (55 NAME-only + 13 MICRO-PIN+NAME, across 43 TUs), **32 skipped** (19 foreign-pin, 13 no-size). Confidence of the 68: high 2, bsim≥30 9, bsim20-30 21, **bsim15-20 36 (confirm-on-consume)**. |
  | **B1: unwired, portable** | ~113 | ~83 | rb3-Wii source exists (`../rb3/src/<rel>`); 9 TUs also have a DC3 `.cpp` twin (byte-match potential, same compiler); 9 TUs have the source already in THIS repo but unwired (wire-only). |
  | **B2: unwired, no oracle** | 144 | 85 | `dc3_cannot_provide=true` = rb3-Wii tree lacks the `.cpp` AND DC3 has no same-named file. Overwhelmingly Quazal/ObjDup netcode (`network/ObjDup`, `network/Extensions`, most of `network/Plugins`, `network/Services`). **NOT portable. Park.** |

  (A+B1+B2 ≈ 358–361; the residue drifted between my two measurement runs because
  agents landed pins in the interim. Phase 0 recomputes.)
- **Important semantic:** `dc3_cannot_provide=false` does NOT mean DC3 has the file —
  it means at least one oracle exists. Primary oracle for B1 is the **rb3-Wii tree**
  (`/home/free/code/milohax/rb3/src`); check DC3 (`/home/free/code/milohax/dc3-decomp/src`)
  first anyway, because a DC3 body compiles under the identical MSVC X360 toolchain and
  often byte-matches. Verified DC3-twin B1 TUs: `system/movie/Movie.cpp`,
  `system/meta/SongPreview.cpp`, `system/synth/{Faders,MoggClip,Synth,StandardStream,SynthSample}.cpp`,
  `system/rndobj/Dir.cpp`, `system/obj/Dir.cpp`.

### How to re-derive (Phase 0 runs this)

```bash
cd /home/free/code/milohax/rb3-xenon
# net-new + segmentation (read-only):
python3 - <<'EOF'
import json
wl = json.load(open('sysnet_port_worklist.json'))['worklist']
tm = json.load(open('scripts/target_symbol_map.json'))
norm = lambda a: str(a).lower().removeprefix('0x').lstrip('0').zfill(8)
mapped = {norm(k) for k in tm}
nn = [r for r in wl if norm(r['rb3_addr']) not in mapped]
objs = open('config/45410914/objects.json').read()
wired = [r for r in nn if f'"{r["src_path"].replace("src/","",1)}"' in objs]
unw = [r for r in nn if r not in wired]
b2 = [r for r in unw if r['dc3_cannot_provide']]
print(f'net-new={len(nn)} wired={len(wired)} unwired-portable={len(unw)-len(b2)} no-oracle={len(b2)}')
EOF
# live pin-tool dry-run (read-only, prints per-id actions):
python3 tools/band3_worklist_pin.py --worklist sysnet_port_worklist.json --all-wired
```

Expected (at doc time): `net-new=361 wired=101 unwired-portable=113 no-oracle=144`;
dry-run tail: `68 nameable (13 need micro-pins, 32 skipped)`.

## Evidence & references (absolute paths)

- **Roster (tracked, human checklist):** `/home/free/code/milohax/rb3-xenon/docs/plans/sysnet-port-worklist.md`
  — 516 fns / 276 TUs; precision 0.967 (system 14/15, network 15/15; HIGH+BSim≥30 core
  11/11 = 1.000); **bsim15-20 = confirm-on-consume** (holds the lone measured miss:
  `TrackWidget::Init` aliased to sibling `Empty` — two 20-byte forwarders differing only
  in the vtable-slot immediate, `0x44` vs `0xc`). 144 rows `dc3_cannot_provide=true`.
- **Regen tool:** `/home/free/code/milohax/rb3-xenon/tools/gen_sysnet_port_worklist.py`
  (re-derives net-new against the LIVE map; rewrites both the json and the tracked
  roster md — coordinate before running, it touches a tracked file).
- **Pin tool:** `/home/free/code/milohax/rb3-xenon/tools/band3_worklist_pin.py` —
  deterministic; takes `--worklist <json> --all-wired|--tu X.cpp [--apply] [--root <tree>]`.
  Reads sizes from `config/45410914/symbols.txt`, pins from `splits.txt`; SKIPs
  foreign-pin (address inside another TU's span) and no-size ids; names only when the
  Wii `class::method` resolves to exactly ONE compiled MSVC symbol in the TU's own obj
  (argcount disambiguation fallback). Apply = ADD-ONLY micro-pins to `splits.txt` +
  ADD-ONLY names to `scripts/target_symbol_map.json`. This tool is the ONLY sanctioned
  pin+name path (the GemPlayer-disaster fix) — never hand-edit the map or splits for pins.
- **v2 lane workflow (the band3 drain's exact machinery):**
  `/home/free/.claude/projects/-home-free-code-milohax-rb3-xenon/a74fccf0-44cb-415b-9808-137561334027/workflows/scripts/band3-worklist-port-harvest-v2.js`
  — read it before building the wave; Wave C below transcribes its lane steps.
- **v2 findings (memory):** `~/.claude/projects/-home-free-code-milohax-rb3-xenon/memory/project_worklist_drain_close_2026-07-02.md`
  — NO whole-binary builds in lanes (13 concurrent lanes at load ~1.7 vs the historic
  load-300 storms); reviewer REPRODUCES numbers by re-running objdiff; ≤44-byte
  stub-fold guard; branch diff-hygiene; **lane raw-objdiff 98–99.9% readings often land
  as TRUE 100 after the composed renamer resolves anon-reloc naming — do not discard
  them as fuzzy-only**; Sonnet ports, Fable scopes+reviews.
- **Consumption precedent:** commit `f9f0d23e966f125797dd9b6024d0219287f4f7e1` (~105
  sysnet pins in wired TUs; also documents the auto-`.pdata` backfill and one
  except_data split-widening fix pattern: GuitarController `IsShifted` end pushed
  `0x827778c4`→`0x827778c8` to absorb an 8-byte except_data blob).
- **Integration tooling:** `scripts/harvest/land.sh` (rebase branch onto main with
  dict-union auto-resolve for map/objects, line-union for splits; prints
  `READY:`/`DEFER:`), `scripts/harvest/measure_delta.py` (strict net + per-fn fuzzy
  regression scan between two report.json), `tools/fresh_report.sh` (guaranteed-fresh
  report.json), `tools/icf_alias_check.py` (≤44-byte stub-fold/ICF-alias audit).
- **Worktrees:** `scripts/setup_worktree.sh <path-under-~/tmp> <branch>` (btrfs CoW,
  warm obj cache). Configure line for regenerating a worktree's build graph:
  ```
  python3 configure.py --dtk /home/free/code/milohax/jeff/target/release/dtk \
    --objdiff "$(readlink -f bin/objdiff-cli)" \
    --wrapper /home/free/code/milohax/wibo/build/release/wibo
  ```
  (Absolute flags mandatory — bare `configure.py` re-introduces the cargo/download edges.)
- **Oracles:** rb3-Wii dev decomp `/home/free/code/milohax/rb3/src` (MWCC, named fns,
  MILO_ASSERT strings); DC3 `/home/free/code/milohax/dc3-decomp/src` (same MSVC X360
  compiler+flags — prefer when the file exists).

## Step-by-step procedure

### Phase 0 — freshness + activity check (coordinator, read-only, ~10 min)

1. `git -C /home/free/code/milohax/rb3-xenon log --oneline -20` and
   `git status --short` — identify in-flight TUs. **Owner-WIP rule:** any TU with
   uncommitted edits in main or named in the last ~20 commits' subjects is
   OFF-LIMITS (the band3 drain's owner-WIP list was Band/Track/Game/Tracker/
   BandProfile/GemTrainerPanel/ClosetMgr/GemSmasher/BandTrack — mostly band3, but
   re-derive it fresh; e.g. TambourineManager/DepthBuffer3D were active at doc time).
2. Re-derive counts with the "How to re-derive" block above. If
   `sysnet_port_worklist.json` is missing (gitignored), regenerate:
   `python3 tools/gen_sysnet_port_worklist.py` — note this rewrites the tracked
   roster md too; commit or discard-by-agreement, don't leave it dirty.
3. Record the dry-run output to `~/tmp/sysnet_dryrun_$(date +%s).txt` — it is the
   Wave A ledger.

### Phase A — pin-only drain of wired TUs (one lane, ~68 ids)

Single worktree; this wave IS its own integration, so a composed A/B in-worktree is
correct here (it is one lane, not thirteen).

```bash
cd /home/free/code/milohax/rb3-xenon
scripts/setup_worktree.sh ~/tmp/wt-sysnet-pin sysnet-pin
cd ~/tmp/wt-sysnet-pin
cp build/45410914/report.json ~/tmp/sysnet_pin_BASE.json      # A-side baseline
# dry-run first, absolute worklist path (json is gitignored, not in the worktree):
venv/bin/python3 tools/band3_worklist_pin.py --root ~/tmp/wt-sysnet-pin \
  --worklist /home/free/code/milohax/rb3-xenon/sysnet_port_worklist.json --all-wired
# sanity vs Phase-0 ledger, then:
venv/bin/python3 tools/band3_worklist_pin.py --root ~/tmp/wt-sysnet-pin \
  --worklist /home/free/code/milohax/rb3-xenon/sysnet_port_worklist.json --all-wired --apply
# re-split (auto-backfills .pdata for RUNTIME_FUNCTION-bearing micro-pins) + renamer + fresh report:
touch config/45410914/config.yml
rm -f build/45410914/target_symbol_renames.stamp
./tools/ninja-locked build/45410914/config.json 2>&1 | tee ~/tmp/rb3_build_sysnet_pin.log
tools/fresh_report.sh 2>&1 | tail -5
scripts/harvest/measure_delta.py ~/tmp/sysnet_pin_BASE.json build/45410914/report.json
python3 tools/icf_alias_check.py            # see tool header for args
tools/fresh_report.sh && scripts/harvest/measure_delta.py ~/tmp/sysnet_pin_BASE.json build/45410914/report.json  # NET must repeat
```

**Confirm-on-consume sub-gate (36 of the 68 are bsim15-20):** for every applied
bsim15-20 name, run per-symbol objdiff (`mcp__orchestrator__run_objdiff` with
`project_dir=~/tmp/wt-sysnet-pin`, or `bin/objdiff-cli diff -p . -u <unit>
'<mangled>' -f json`). For tiny (≤64 B) forwarders with same-class siblings, diff the
vtable-slot / type-tag / node-size immediates against the Wii body (the
TrackWidget Init-vs-Empty failure mode). Remove any name that fails from
`scripts/target_symbol_map.json` (and its micro-pin from `splits.txt`) before commit.

Handle a dtk split-validation failure the `f9f0d23` way: if a micro-pin end lands
mid-symbol because of a trailing except_data blob, widen the pin to absorb it and
re-verify with a per-TU build.

Commit only `config/45410914/splits.txt`, `config/45410914/objects.json` (unchanged
in this wave normally), `scripts/target_symbol_map.json` to branch `sysnet-pin`; land
via `scripts/harvest/land.sh ~/tmp/wt-sysnet-pin` → `git merge --ff-only` on READY.

### Phase B — wire-only quick hits (9 TUs whose source already exists here)

TUs (verified present at `src/<rel>` but absent from `objects.json`):
`system/movie/Movie.cpp`, `system/meta/SongPreview.cpp`, `system/synth/Faders.cpp`,
`system/synth/MoggClip.cpp`, `system/synth/Synth.cpp`, `system/synth/StandardStream.cpp`,
`system/synth/SynthSample.cpp`, `system/rndobj/Dir.cpp`, `system/obj/Dir.cpp`.

Per TU (can share one worktree, serially, or fold into Phase C lanes): add the path as
`NonMatching` to `config/45410914/objects.json`, run the configure line, build ONLY that
obj (`./tools/ninja-locked build/45410914/src/<rel:.cpp=.obj>`), fix compile errors
(these files may be unwired precisely because they don't compile — if the fix balloons,
report blocker and defer), then pin: `venv/bin/python3 tools/band3_worklist_pin.py
--root <wt> --worklist <abs sysnet json> --tu <Base>.cpp --apply`, re-split, per-symbol
objdiff.

**⚠ Dir.cpp basename collision:** `splits.txt` headers for engine TUs are bare
basenames; `system/rndobj/Dir.cpp` and `system/obj/Dir.cpp` would BOTH be `Dir.cpp:`.
The pin tool groups by basename and would micro-pin under whichever header exists.
Wire at most ONE of the two per wave and eyeball the splits diff; if both are needed,
resolve the header naming with the coordinator first.

### Phase C — port-then-pin lanes over B1 (v2 workflow, ≤13 concurrent)

Build the target list from the Phase-0 re-derivation, ranked exactly like
`ranked_tus`/the roster: `(#high + #bsim≥30) desc, then total ids desc`, restricted to
unwired + oracle-available + not owner-WIP. At doc time the tier-1 list (≥1 high/b≥30
id, rb3-Wii source confirmed) was:

> `network/Platform/StringConversion.cpp` (2×b≥30, anonymous-namespace Latin1/Utf8
> converters), `network/Core/Scheduler.cpp` (3 ids), `system/bandobj/BandHeadShaper.cpp`,
> `system/bandobj/OutfitConfig.cpp` (1 high), `system/bandobj/PitchArrow.cpp`,
> `system/beatmatch/VocalNoteList.cpp`, `network/Plugins/ChecksumAlgorithm.cpp`,
> `network/Plugins/EncryptionAlgorithm.cpp`, `network/Core/PseudoGlobalVariableList.cpp`,
> `system/bandobj/BandHighlight.cpp`, `system/beatmatch/JoypadGuitarController.cpp`,
> `system/utl/LogFile.cpp`, `system/beatmatch/PhraseList.cpp` (high),
> `system/beatmatch/RGGemMatcher.cpp`, `system/beatmatch/RGState.cpp` (high, `operator=`),
> `system/bandobj/SongSectionController.cpp`, `system/meta/SongPreview.cpp` (DC3 twin).

Tier-2 (multi-id, b20-30): `network/Core/CallContext.cpp` (4),
`network/Protocol/Protocol.cpp` (3), `network/Plugins/{Buffer,ByteStream}.cpp`,
`network/Core/{InstanceControl,InstantiationContext,Job,SystemComponent}.cpp`,
`network/net/{Jobs_Wii,SessionSearcher_RV}.cpp`, `system/beatmatch/{BeatMatchUtl,
DrumTrackWatcherImpl,RealGuitarTrackWatcherImpl}.cpp`, `system/bandobj/{BandFaceDeform,
ChordShapeGenerator}.cpp`, `system/synth/MidiInstrumentMgr.cpp`. Tier-3 = single
bsim15-20 TUs — only if trivially cheap; every name is confirm-on-consume.

Run the v2 workflow structure (Scope → Port+Review pipeline). Per lane, the Sonnet
executor contract (transcribed from `band3-worklist-port-harvest-v2.js`, with sysnet
substitutions):

1. `scripts/setup_worktree.sh ~/tmp/wt-sn-<stem> sn-<stem>`; ALL work in the worktree.
2. **Oracle choice:** if `/home/free/code/milohax/dc3-decomp/src/<rel>` exists, port
   from DC3 (same compiler — near-verbatim, watch DC3-newer drift per CLAUDE.md);
   else port `/home/free/code/milohax/rb3/src/<rel>` MWCC→MSVC using the best
   already-wired sibling `.cpp` in the same directory as the template (`.mStr` →
   accessors, MILO_WARN comma fixes, copy genuinely-missing headers). If HIGH-risk
   deep deps: STOP, report `compiled=false` + blocker — a clean +0 beats a broken build.
3. Wire: add `"<rel>"` as `NonMatching` to `<wt>/config/45410914/objects.json`.
4. Regen build graph with the absolute-flag configure line (see Evidence section).
5. Build ONLY your obj: `./tools/ninja-locked build/45410914/src/<rel:.cpp=.obj> 2>&1 |
   tee ~/tmp/sn_<stem>.log`. Hundreds of MSVC lines scrolling = the warm cache broke —
   STOP and report, do not let it run.
6. Pin+name (deterministic, absolute worklist path):
   `venv/bin/python3 tools/band3_worklist_pin.py --root <wt> --worklist
   /home/free/code/milohax/rb3-xenon/sysnet_port_worklist.json --tu <Base>.cpp --apply`.
7. Re-split + re-rename + rebuild YOUR obj only: `touch config/45410914/config.yml &&
   rm -f build/45410914/target_symbol_renames.stamp && ./tools/ninja-locked
   build/45410914/config.json && ./tools/ninja-locked <your obj>`.
8. **Measure per-symbol ONLY** (`mcp__orchestrator__run_objdiff` with
   `project_dir=<wt>`, or `bin/objdiff-cli diff -p . -u <unit> '<mangled>' -f json`):
   record `per_id = [{va, symbol, pct, size}]`; `strict_100` counts only pct==100 AND
   size>44; `fuzzy_near` = 50≤pct<100. **NO whole-binary builds, no `tools/fresh_report.sh`,
   no bare `ninja` in lanes.**
9. bsim15-20 ids: verify identity per-fn (immediates/strings/callees vs the Wii body)
   before counting the name as honest.
10. Commit src + `objects.json` + `splits.txt` + `scripts/target_symbol_map.json` to
    branch `sn-<stem>`; return the RESULT payload. Never land to main.

Per lane, the Fable reviewer contract: REPRODUCE every per_id number by re-running
objdiff itself; reject ≤44 B "strict" as stub-folds; foreign-pin check (mangled class
matches the TU, VA belongs to it — same-TU sibling aliasing counts as foreign); main-tree
leak check (`git -C /home/free/code/milohax/rb3-xenon status --short`); diff hygiene
(`git -C <wt> diff main --stat` touches ONLY own src/headers + objects.json + splits.txt
+ target_symbol_map.json). Approve iff all pass; a purely-fuzzy honest port MAY be
approved (composed renamer often lifts 98–99.9% to TRUE 100).

### Phase D — integration (coordinator, ONE composed A/B)

1. Baseline: `cp build/45410914/report.json ~/tmp/sysnet_wave_BASE.json` (fresh main).
2. Per approved branch: `scripts/harvest/land.sh <wt-path>` → collect `READY:` lines;
   `DEFER:` branches go back to their lane or are dropped.
3. After all READY: splits-overlap self-check, then composed verify in main (or a
   staging worktree): `rm -f build/45410914/target_symbol_renames.stamp && touch
   config/45410914/config.yml && tools/fresh_report.sh` — **twice**, NET identical.
4. `scripts/harvest/measure_delta.py ~/tmp/sysnet_wave_BASE.json build/45410914/report.json`
   + `python3 tools/icf_alias_check.py`. Reject any branch that does not reproduce at
   integration.
5. `git merge --ff-only` the survivors; re-run `tools/gen_sysnet_port_worklist.py` to
   refresh the roster + shrink the net-new set (commit the regenerated roster md).

### Phase E — residue bookkeeping

- **19 foreign-pin skips** (Wave A dry-run): ids inside other TUs' pinned spans —
  the case-B pattern. Hand the list (in the Phase-0 ledger file) to **ws5**
  (`ws5-caseb-campaign.md`); do not force-pin them here.
- **13 no-size skips:** `symbols.txt` lacks a `size:` for the VA. Per-id salvage:
  derive the function size from Ghidra (port 8002 project RB3Xenon) or the next
  symbol's VA, add/fix the `symbols.txt` entry, re-run the pin tool for that TU.
  Timebox: if a size fix doesn't make the id pin cleanly in ~15 min, skip.
- **144 B2 no-oracle ids:** park. They remain in the worklist json as a naming
  reserve for manual reconstruction (ws6) and for any future Quazal source leak /
  ws2 regen cross-checks. Do NOT attempt blind MSVC reconstruction of Quazal
  middleware from BSim hints.

## Honesty gates & verification

- **Reproduce, don't trust:** every lane number is re-derived by the reviewer running
  objdiff itself; every wave lands only after `measure_delta.py` shows NET > 0, zero
  unexplained strict regressions, zero real fuzzy regressions, and the double
  `fresh_report.sh` NET repeats exactly.
- **≤44-byte stub-fold guard** everywhere a strict-100 is claimed; `icf_alias_check.py`
  at integration.
- **bsim15-20 = confirm-on-consume** (36 of Wave A's 68; every tier-3 lane id):
  per-fn identity verification against the Wii body before the name counts.
- **Deterministic pinning only** via `tools/band3_worklist_pin.py`; never hand-edit
  `scripts/target_symbol_map.json`/`splits.txt` for pins, never run identity_transfer
  or the broad oracle here.
- **No whole-binary builds in lanes** — per-symbol objdiff on warm single-obj builds;
  exactly one composed A/B per wave, at the coordinator.
- **Main-tree hygiene:** no stash/checkout/restore/reset in main; all work in
  `setup_worktree.sh` worktrees under `~/tmp`; branch diffs touch only the four
  sanctioned file classes.
- **Owner-WIP:** re-derive the active-TU list in Phase 0 and skip those TUs entirely.

## Kill criteria

- **Wave A:** if the composed A/B goes net-negative or `icf_alias_check` flags mass
  stub-fold inflation → do not land; bisect the map-diff by TU in the worktree.
  (Expected failure mode is a handful of bad bsim15-20 names, removable per-id.)
- **Per Wave-C lane:** compile infeasible after honest effort (deep unportable deps,
  HIGH risk) → `compiled=false`, clean +0, move on. Same-lane retry only if the
  reviewer identifies a small missed fix.
- **Wave-C campaign:** if the first ~6-lane tier-1 wave yields <3 approved branches or
  <5 verified real matches total, the porting tier is below cost — stop Wave C, leave
  the remainder to ws2 (worklist regen at looser tier) and ws6.
- **MWCC-divergence wall check:** band3 taught that ported MWCC game bodies can
  diverge wholesale (BandProfile 0/64). If the first 3 ported *network* (Quazal-with-
  Wii-source) TUs all read ~0% per-symbol, stop porting network TUs — pin-only value
  doesn't justify the port cost there — and continue with system/ TUs only.
- **B2 is pre-killed** — no oracle, not a porting target, by construction.

## Expected yield

- **Wave A:** up to 68 named ids (13 micro-pins) minus confirm-on-consume rejects.
  Strict contribution = however many bodies already byte-match; the band3/f9f0d23
  precedent suggests a meaningful strict single-digit-to-tens plus a large named-fuzzy
  inventory that feeds later body-ports. Cost: ~half a day, one lane.
- **Waves B+C:** ~113 portable ids across ~83 TUs; realistic scope is the ~25–35
  tier-1/2 TUs (~60–70 ids). Band3 wave analogs (+5, +10 strict per wave with 0 lost)
  plus whole-TU reveal cascade (a ported TU matches non-worklist functions too — the
  BandTrack land was +7 strict off a similar vein) suggest **+40–120 strict** over the
  full drain, higher if DC3-twin TUs byte-match wholesale.
- **Ceiling:** 361 net-new ids at doc time, minus 144 unreachable = **217 reachable**;
  every consumed id also densifies the identity oracle for ws2/ws5/ws6.

## Open questions

1. **Wave A strict yield** is unknown until the composed A/B — the 68 names pair
   existing compiled bodies; nobody has measured how many already match.
2. **Are the 13 no-size skips recoverable?** The band3 twin's matcher-unresolvable
   set stayed dry after retry; these are a different class (missing symbols.txt
   size), so per-id salvage is plausible but unproven.
3. **Quazal-with-Wii-source body-match rate under MSVC** (tier-2 network TUs):
   middleware C++ with no DC3 twin — no precedent either way. The 3-TU wall check
   above resolves this empirically.
4. **Dir.cpp basename collision** (Phase B warning): does the pin tool / splits
   convention need a path-qualified header form before both `Dir.cpp` TUs can be
   wired? Coordinator decision.
5. **Regen ordering vs ws2:** should `gen_sysnet_port_worklist.py` be re-run against
   a fresh looser-tier ghidriff harvest (ws2) before Wave C's tier-2/3, so lanes port
   TUs with the densest possible id sets? Likely yes — sync with ws2's owner.
6. The worklist json on disk is a snapshot (`ghidriff-run3` per `_meta`); if it is
   ever absent or suspected stale, regen is cheap but rewrites the tracked roster —
   coordinate the commit.

## Wave A results (2026-07-02, branch `exec/ws1-waveA-0702`)

**Composed A/B: +46 strict (10936 -> 10982), 0 strict / 0 fuzzy regressions**,
reproduced twice, ICF gate HONEST. Details + per-packet verdicts:
`docs/decomp/handoff/exec-ws1-waveA-run-2026-07-02.md` (+ p1 verdict table in
`exec-ws1-waveA-p1-verdicts.md`).

- Open question 1 answered: of the 31 pre-applied names, 21 survived per-symbol
  verification (10 removed: wrong-fn / not-a-boundary / <50%); 8 of the keeps
  landed strict in the composed report.
- Open question 2 answered: 4 of the 12 no-size skips were salvageable by
  symbols.txt size surgery (3 strict + 1 named near-miss); 6 are genuinely not
  function boundaries; 1 misID reverted; 1 parked (Quazal).
- Port packets landed 14 named stricts + the 24-fn VocalTrackDir guard cascade.
- Pin-tool namer gaps (ctors, template/free fns, overloads, $4 thunks) fixed in
  `tools/band3_worklist_pin.py` + `tools/gen_game_target_map.py` (p5 patch) —
  unblocks the same walls for Waves B/C.
- Post-land TODO: re-run `tools/gen_sysnet_port_worklist.py` (roster regen) on
  main after merge; shared `utl/MakeString.h` +0x800 buffer fix would close 3
  more MakeString stricts.
