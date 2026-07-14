# Handoff: port-BeatMatcher (lane w3-beatmatcher)

> **STATUS (2026-07-06):** SUPERSEDED — since verified and pinned. Commits `e6d9835`
> (RndOverlay::Showing fix) and `18c33de` landed after this handoff: 4 of the 5 worklist
> fns (InSolo, PostDynamicAdd, Poll, Jump) are 100.0% normalized and pinned in
> `scripts/target_symbol_map.json` / `config/45410914/splits.txt`; `ResetPitchBend` was
> honestly REVERTED (88.24%, real body divergence) per `80ac963`. The build-environment
> blocker described below is resolved; do not re-run the "Next steps" section as written.

Status: **STAGED + COMMITTED, UNVERIFIED (blocked on build environment).**

## What is done
- Full TU ported verbatim from the rb3-Wii oracle
  (`/home/free/code/milohax/rb3/src/system/beatmatch/BeatMatcher.cpp`, 606 lines)
  → `src/system/beatmatch/BeatMatcher.cpp`. All 20 includes resolve against
  existing xenon headers; the HX_NATIVE guard blocks compile out (HX_NATIVE
  undefined in the xenon match build) so codegen tracks the Wii console path.
- Added 2 missing headers (copied from oracle):
  - `src/system/beatmatch/MercurySwitchFilter.h` — **+ `#include <algorithm>`**
    added (uses `std::min`; math/Utl.h does NOT transitively pull it under MSVC
    stlport, and sibling beatmatch TUs each add `<algorithm>` explicitly).
  - `src/system/beatmatch/Playback.h` — declares `TheBeatMatchPlayback`.
- Wired `"system/beatmatch/BeatMatcher.cpp": "NonMatching"` under the **engine**
  module in `config/45410914/objects.json` (right after BeatMatchController.cpp).
- Committed as WIP checkpoint: **c77942da** on branch **w3-beatmatcher**.

## Why unverified
`scripts/setup_worktree.sh` warms the MAIN repo cache via `ninja-locked
all_source` (line 149) BEFORE reflinking `build/45410914` into the worktree.
~12 concurrent worktree setups (other lanes: bp-MetaPanel, bp-DirectInstrument,
bp-OverdriveTracker, bp-GameplayOptions, ...) were all queued on the shared
build flock. The build dir never reflinked within the session window
(`build/` stayed at 4.0K = just the `compilers` symlink; no `config.json`, no
`build.ninja`). So NO configure / compile / objdiff / pin was possible.

## Next steps (for whoever resumes)
1. Confirm setup finished: `ls build/45410914/config.json` present in the worktree.
   If the setup PID died without reflinking, re-run
   `scripts/setup_worktree.sh .claude/worktrees/wt-w3-beatmatcher w3-beatmatcher`
   (idempotent) or hand-reflink `obj/` + `config.json` from main.
2. `python3 configure.py` then
   `tools/ninja-locked build/45410914/src/system/beatmatch/BeatMatcher.obj`.
   Iterate to a clean compile (watch for MSVC vs MWCC parse nits; SongData.cpp
   is the proven template — a verbatim port compiled there).
3. Verify the 5 worklist addrs with objdiff-cli (unit `default/BeatMatcher`):
   | addr | fn | mangled (expected, CONFIRM from COFF) | bsim |
   |---|---|---|---|
   | 0x8276ba08 | InSolo(int) bool **virtual** | `?InSolo@BeatMatcher@@UAA_NH@Z` | 15-20 (16.0) |
   | 0x8276bf98 | PostDynamicAdd(int,float) | `?PostDynamicAdd@BeatMatcher@@QAAXHM@Z` | 20-30 (22.0) |
   | 0x8276cad8 | Poll(float) | `?Poll@BeatMatcher@@QAAXM@Z` | 15-20 (19.1) |
   | 0x8276cb48 | Jump(float) | `?Jump@BeatMatcher@@QAAXM@Z` | 20-30 (23.3) |
   | 0x8276ccf8 | ResetPitchBend(int) **virtual** | `?ResetPitchBend@BeatMatcher@@UAAXH@Z` | 15-20 (16.2) |
   Extract mangled names from the built `BeatMatcher.obj` COFF symtab (llvm-nm),
   never hand-guess.
4. **Sibling-aliasing WARNING**: these 5 addrs sit in a densely interleaved run
   at 0x8276bxxx-0x8276ccf8. Neighbors in `target_symbol_map.json` are FOREIGN
   TUs — 0x8276b9f8 `CMllr::SetAdaptClassFile`, 0x8276bd60 `MoggClip::IsStreaming`,
   0x8276cbf0 `TaskMgr::ResetBeatTaskTime`. InSolo(16B), ResetPitchBend, and
   Poll(f) are tiny → high ICF-fold risk. PIN ONLY at normalized==100 (the build
   gate is the only protection below which sibling-aliasing poisons the map).
5. Pins go in `config/45410914/splits.txt` (new `BeatMatcher.cpp:` section,
   `.text start:VA end:VA+size`; add `.pdata` only if the fn has a
   RUNTIME_FUNCTION in `orig/45410914/band.exe`) and add-only entries in
   `scripts/target_symbol_map.json`. Then rebuild + re-verify (pairing shifts
   normalized%). Run `python3 tools/icf_alias_check.py --tu BeatMatcher.cpp`.

## Gotchas
- `game/Player.h` include is proven-compilable (Player.cpp, GemPlayer.cpp,
  TrackPanel.cpp etc. all include it and are wired NonMatching).
- Do NOT edit shared beatmatch headers owned by other lanes
  (BeatMatchController / SongData / TrackWatcherImpl); this lane only ADDS
  MercurySwitchFilter.h + Playback.h.
