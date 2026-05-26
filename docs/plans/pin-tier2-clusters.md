# Plan — Pin tier-2 cluster candidates (12 new + 3 expansions)

**Goal.** Extend `config/45410914/splits.txt` from the current 8 cluster pins to
20 pins by adding 12 newly-identified candidates, and grow 3 existing pins to
absorb adjacent identified functions. Add the 12 corresponding `objects.json`
entries. This is **mechanical** splits-bootstrap work (per CLAUDE.md "Splits-
bootstrap recipe"). Matching is not the goal — coverage is.

## 0. Re-verification (verified 2026-05-26 — Sonnet must spot-re-check before edits)

All 12 candidates and 3 expansions are present in `autoid.json` (537 proposals
post-Symbols-filter) and densities recompute correctly against
`fingerprints.json`. Source files exist under `../rb3/src/` (and
`../dc3-decomp/src/` for `GameMode.cpp`). Spot-checks confirmed:

- `ShaderOptions.cpp` strings `BLOOM` / `COLORXFM` / `KALEIDOSCOPE`: 3 hits in
  `../rb3/src/system/rndobj/ShaderOptions.cpp`.
- `Console.cpp` strings `set_break` / `Clear dynamic breakpoint`: 9 hits in
  `../rb3/src/system/rndobj/Console.cpp`.
- `PatchPanel.cpp` strings `warp` / `rotate`: 4 hits in
  `../rb3/src/band3/meta_band/PatchPanel.cpp`.
- `GameMode.cpp` `parent_mode`: dc3 11 hits vs rb3 4 hits — **dc3 is the
  richer source** for this TU (project owner caveat acknowledged: cross-check
  if behaviour disagrees).

**Post-edit pin set sorted by address — NO OVERLAPS** (verified via
fingerprints.json walk):

| addr range            | TU                  |
|---                    |---                  |
| 8226C738–82275EA8     | BandCharacter.cpp (expanded) |
| 8227C378–82285200     | BandDirector.cpp (expanded)  |
| 822E4180–822E847C     | VocalTrackDir.cpp   |
| 822F2E68–822F5830     | TrackPanelDir.cpp (expanded) |
| 822FEDF8–82301318     | CrowdAudio.cpp      |
| 8231CCB0–8231E258     | BandWardrobe.cpp    |
| 82322DA0–82324884     | BandCharDesc.cpp    |
| 82453730–82455718     | Console.cpp         |
| 82487438–824884C4     | ShaderOptions.cpp   |
| 824E8468–824EA844     | RockCentral.cpp     |
| 82528C50–82529D64     | MusicLibrary.cpp    |
| 825C10D8–825C3A44     | OvershellSlot.cpp   |
| 825ED848–825EF598     | CalibrationPanel.cpp|
| 8260DDD0–8260E290     | PatchPanel.cpp      |
| 82671A60–**82672130** | GameMode.cpp (see §1.1) |
| 82758380–8275A534     | MasterAudio.cpp     |
| 82777958–827786AC     | GuitarController.cpp|
| 82B4B850–82B4CED0     | QuestFilterPanel.cpp|
| 82B4DBA0–82B4F8D8     | TourDescPanel.cpp   |
| 82B727B8–82B768DC     | VocalTrack.cpp      |

### 1.1 GameMode end-address correction (load-bearing change vs spec)

The spec proposed `[0x82671A60, 0x82671F98)` but `autoid.json` identifies a
3rd hit at `fn_82672040` (size 240 = 0xF0). Excluding it would pin only 2/3
IDs. **Recommended fix: end = `0x82672130`** (= `0x82672040 + 0xF0`). New
density: 3 ids / ~7 fns in span = ~43% (vs spec's 33%). Use the corrected
end address in the splits.txt edit below.

### 1.2 Span-expansion ID-counts confirmed

`autoid.json` recompute against the proposed expanded ranges:

- `BandDirector.cpp` `[0x8227C378, 0x82285200)` → **10 ids** in range (spec
  said 10 ✓). 5 lower-confidence hits beyond `0x82285200` stay un-pinned for
  now; do not extend further.
- `TrackPanelDir.cpp` `[0x822F2E68, 0x822F5830)` → **6 ids** in range
  (spec said 6 ✓). Density jumps to 9.1%.
- `BandCharacter.cpp` `[0x8226C738, 0x82275EA8)` → **7 ids** in range
  (spec said 7 ✓). Absorbs 128 previously-unpinned fns into the head of the
  TU — density drops to 2.8% (was ~3% for the old 23.7KB pin). **Risk
  flagged** (§7).

## 2. Verbatim `splits.txt` blocks — 12 new clusters (copy-paste ready)

Append after the existing `BandCharacter.cpp` block. Use **only `.text`** —
dtk back-fills `.pdata` on next ninja (per CLAUDE.md). One blank line between
blocks; mirror existing indentation (tabs, then spaces before column
alignment — copy the exact whitespace pattern from existing blocks).

```
PatchPanel.cpp:
    .text       start:0x8260DDD0 end:0x8260E290

Console.cpp:
    .text       start:0x82453730 end:0x82455718

ShaderOptions.cpp:
    .text       start:0x82487438 end:0x824884C4

QuestFilterPanel.cpp:
    .text       start:0x82B4B850 end:0x82B4CED0

GameMode.cpp:
    .text       start:0x82671A60 end:0x82672130

BandCharDesc.cpp:
    .text       start:0x82322DA0 end:0x82324884

VocalTrackDir.cpp:
    .text       start:0x822E4180 end:0x822E847C

CrowdAudio.cpp:
    .text       start:0x822FEDF8 end:0x82301318

OvershellSlot.cpp:
    .text       start:0x825C10D8 end:0x825C3A44

GuitarController.cpp:
    .text       start:0x82777958 end:0x827786AC

CalibrationPanel.cpp:
    .text       start:0x825ED848 end:0x825EF598

TourDescPanel.cpp:
    .text       start:0x82B4DBA0 end:0x82B4F8D8
```

## 3. `splits.txt` in-place edits — 3 span expansions

Modify the existing `.text` lines for these 3 TUs. **Leave the `.pdata` lines
alone** — dtk will recompute and back-fill them on the next ninja after
`touch config.yml`.

- `BandDirector.cpp`: `.text start:0x8227E728 end:0x8227EF2C`
  → `.text start:0x8227C378 end:0x82285200`
- `TrackPanelDir.cpp`: `.text start:0x822F3C50 end:0x822F3F20`
  → `.text start:0x822F2E68 end:0x822F5830`
- `BandCharacter.cpp`: `.text start:0x8226FFD8 end:0x82275EA8`
  → `.text start:0x8226C738 end:0x82275EA8`

## 4. `objects.json` additions — 12 new TUs (placement decided)

Add as `NonMatching` entries into the right module. Maintain alphabetical
order within each module's `objects` block (current file is roughly
alphabetical within `system/<subdir>/`, mostly alphabetical within `band3/`).

### 4.1 `engine` module (4 additions)

```
"system/rndobj/Console.cpp": "NonMatching",
"system/rndobj/ShaderOptions.cpp": "NonMatching",
"system/bandobj/BandCharDesc.cpp": "NonMatching",
"system/bandobj/VocalTrackDir.cpp": "NonMatching",
"system/bandobj/CrowdAudio.cpp": "NonMatching",
"system/beatmatch/GuitarController.cpp": "NonMatching",
```

Place `BandCharDesc.cpp`, `VocalTrackDir.cpp`, `CrowdAudio.cpp` alphabetically
near the existing `system/bandobj/BandCharacter.cpp` / `BandDirector.cpp` /
`BandWardrobe.cpp` / `TrackPanelDir.cpp` entries. Place `Console.cpp` and
`ShaderOptions.cpp` as a new `system/rndobj/` subgroup (no rndobj files in
`objects.json` yet — they'll be the first). Place
`GuitarController.cpp` after `system/beatmatch/MasterAudio.cpp`.

### 4.2 `band3` module (7 additions — incl. GameMode placement)

```
"band3/meta_band/CalibrationPanel.cpp": "NonMatching",
"band3/meta_band/OvershellSlot.cpp": "NonMatching",
"band3/meta_band/PatchPanel.cpp": "NonMatching",
"band3/tour/QuestFilterPanel.cpp": "NonMatching",
"band3/tour/TourDescPanel.cpp": "NonMatching",
```

Insert alphabetically before `band3/meta_band/MusicLibrary.cpp` /
`band3/net_band/RockCentral.cpp`.

### 4.3 `GameMode.cpp` — module decision

`GameMode.cpp` has **both** `../rb3/src/band3/game/GameMode.cpp` and
`../dc3-decomp/src/lazer/game/GameMode.cpp`. Strings spot-check showed dc3 is
the richer match (11 vs 4 `parent_mode` hits). However, this codebase's
source tree convention so far is **everything under `src/band3/` mirrors the
rb3-Wii path layout, everything under `src/system/` mirrors dc3-decomp**.
`GameMode.cpp` is game code (not engine), so the right rb3-xenon path is:

```
"band3/game/GameMode.cpp": "NonMatching",
```

…and place it inside the **`band3` module**, alphabetically before
`band3/meta_band/...`.

Do NOT introduce a new `lazer` module — that's dc3-decomp's own organization,
not ours, and CLAUDE.md is explicit that game code follows rb3-Wii layout.
When porting the source, copy the dc3 version (richer) but rename the
directory from `lazer/game/` to `band3/game/` and merge in rb3-Wii's
behaviour if the two diverge (per the "dc3 newer than RB3" caveat).

## 5. Verification protocol after edits

Run from repo root `/home/free/code/milohax/rb3-xenon/`:

```bash
touch config/45410914/config.yml && ninja 2>&1 | tail -30
```

### 5.1 Expected unit count

- Current state: 36 files in `build/45410914/obj/` (8 pinned `.text` +
  matching `.pdata` + auto-emit residue) — verified via
  `ls build/45410914/obj/ | wc -l`.
- After edits: 8 + 12 = 20 pinned `.text` objs + ~20 auto-pdata + auto-text
  residue. Predict **~50–55 obj files** (the auto-text count drops as pinned
  ranges carve out previously-auto regions, but auto-pdata grows). Don't
  treat exact numbers as load-bearing — just confirm growth in the right
  direction.

### 5.2 Per-pin existence checks

For each of the 12 new TUs and the 3 expansions, confirm dtk emitted the
expected per-unit files:

```bash
for tu in PatchPanel Console ShaderOptions QuestFilterPanel GameMode \
          BandCharDesc VocalTrackDir CrowdAudio OvershellSlot \
          GuitarController CalibrationPanel TourDescPanel \
          BandDirector TrackPanelDir BandCharacter; do
  [ -f "build/45410914/asm/${tu}.s" ]  && echo "OK   ${tu}.s"  || echo "MISS ${tu}.s"
  [ -f "build/45410914/obj/${tu}.obj" ] && echo "OK   ${tu}.obj" || echo "MISS ${tu}.obj"
done
```

### 5.3 Asm-head spot-check (catches off-by-one start addrs)

For each new pin, the head of `build/45410914/asm/<TU>.s` should start with a
label at the pinned start addr. Quick check:

```bash
for tu_addr in \
    "PatchPanel 8260DDD0" "Console 82453730" "ShaderOptions 82487438" \
    "QuestFilterPanel 82B4B850" "GameMode 82671A60" \
    "BandCharDesc 82322DA0" "VocalTrackDir 822E4180" \
    "CrowdAudio 822FEDF8" "OvershellSlot 825C10D8" \
    "GuitarController 82777958" "CalibrationPanel 825ED848" \
    "TourDescPanel 82B4DBA0"; do
  set -- $tu_addr
  head -30 "build/45410914/asm/$1.s" | grep -q "$2" && echo "OK   $1 @ 0x$2" || echo "FAIL $1 @ 0x$2"
done
```

### 5.4 Expansion growth check

For the 3 expanded TUs, the new `.s` should be larger than the previous:

```bash
wc -c build/45410914/asm/BandDirector.s build/45410914/asm/TrackPanelDir.s \
      build/45410914/asm/BandCharacter.s
```

Predicted post-expansion sizes (very rough — bytes of asm scale ~50–100× the
`.text` span):
- `BandDirector.s`: was ~2 KB asm (3.3 KB code), expect ~30 KB+ asm.
- `TrackPanelDir.s`: was ~0.6 KB asm (0.7 KB code), expect ~10 KB+ asm.
- `BandCharacter.s`: was ~20 KB asm (23.7 KB code), expect ~35 KB+ asm.

### 5.5 `.pdata` back-fill

After ninja, re-read `splits.txt` and confirm dtk back-filled `.pdata` lines
for the 12 new TUs. If any TU is missing its `.pdata` line after the build,
that's a dtk WARN-tolerated case (per CLAUDE.md "Known issues") — note it
but don't block.

## 6. Skip-with-rationale list

None of the 12 candidates were skipped on re-verification. All sources exist,
all string spot-checks landed, no span overlaps with existing or other new
pins. The one **correction** (not a skip) is GameMode.cpp end-address
(§1.1: `0x82671F98` → `0x82672130`).

If during step 5 a new pin produces a 0-byte `.s` file or dtk errors out,
revert just that block from `splits.txt`, leave the corresponding
`objects.json` entry (compile-only scaffolding is fine per `tools/project.py`
patch), and document the skip here for follow-up.

## 7. Risks

1. **`BandCharacter.cpp` expansion absorbs 128 unpinned fns** at the head
   (`0x8226C738`–`0x8226FFD8`). Density drops to 2.8%. If those fns turn out
   to belong to a different TU, dtk's per-unit `.obj` will be polluted with
   adjacent-TU code and objdiff will permanently fail to match. **Mitigation:**
   leave this expansion as the last edit; if step-5 checks look noisy compared
   to the 2 other expansions, consider walking it back to a tighter range
   (e.g., `[0x8226E000, 0x82275EA8)`).
2. **`VocalTrackDir.cpp`'s 16.7 KB span at 1.9% density** is the weakest of
   the 12 (lowest density). 2 IDs scattered across 105 fns. Risk of bleed
   from neighbor TUs is highest here. Tier-B label is appropriate; the impl
   agent should keep an eye on its `.s` head/tail in §5.3 — if the head label
   doesn't match `0x822E4180` exactly, walk back.
3. **`GameMode.cpp` provenance ambiguity:** dc3 path used as primary source
   (richer strings). When porting source, expect dc3-vs-rb3 behavioural drift
   per the CLAUDE.md "dc3-decomp is newer than RB3" caveat. Cross-check
   `parent_mode`/`enter`/`defaults` semantics against rb3-Wii before
   accepting dc3's version.
4. **`Console.cpp` exists in both dc3 and rb3-Wii** (engine code). Use
   `../rb3/src/system/rndobj/Console.cpp` as the primary (per CLAUDE.md
   "rb3-Wii DEV decomp retains `MILO_ASSERT` strings — richer source
   oracle"). dc3's version is the secondary cross-reference.
5. **Pin-without-compile is fine for now** — `tools/project.py` is patched to
   emit a compile edge from `objects.json` even without splits, and a split
   without a source file simply produces dtk asm and no MSVC build edge.
   But for the 12 TUs we add to **both** files in this plan, the MSVC compile
   will fail until the source is ported (no `.cpp` exists under
   `rb3-xenon/src/` yet). That's expected — `Progress: 0.00% matched` stays
   the baseline. The objdiff target side (`build/.../obj/<TU>.obj` from dtk)
   is what this plan delivers.

## 8. References

- `config/45410914/splits.txt` — current 8 pins (head of file).
- `config/45410914/objects.json` — current engine + band3 modules.
- `autoid.json` (regen via `tools/fingerprint_match.py autoid`) — 537
  proposals; all 15 TUs in this plan are present.
- `fingerprints.json` (regen via `tools/fingerprint_match.py extract`) —
  66,838 RB3 functions, addr-indexed; used for density recompute.
- `docs/splits.md` — splits.txt schema.
- `CLAUDE.md` — "Splits-bootstrap recipe (per new cluster)" + "Build wiring"
  sections; pin **only `.text`** initially.
- `docs/plans/match-first-fn.md` — sibling plan (matching phase, not
  coverage); style template.
- Memory: `project_function_identification.md` (the autoid pipeline producing
  the 537 proposals), `feedback_verify_assumptions.md` (this plan's
  re-verification step in §0).

---

## 9. Sonnet impl-agent step list (mechanical, in order)

1. **Open** `config/45410914/splits.txt` for edit.
2. **Apply expansions** (§3): edit the `.text` start/end on the existing
   `BandDirector.cpp`, `TrackPanelDir.cpp`, `BandCharacter.cpp` blocks. Leave
   `.pdata` lines untouched; dtk will recompute them.
3. **Append the 12 new blocks** (§2) at the end of `splits.txt`. Use tabs +
   spaces matching the existing blocks' indentation exactly. **Use the
   corrected GameMode end `0x82672130`** (§1.1) — *not* the spec's
   `0x82671F98`.
4. **Open** `config/45410914/objects.json` for edit.
5. **Add the 6 engine entries** (§4.1) inside the `engine.objects` block,
   alphabetically by full path (`system/bandobj/...` before
   `system/beatmatch/...` before `system/rndobj/...`).
6. **Add the 7 band3 entries** (§4.2 + §4.3 `band3/game/GameMode.cpp`) inside
   the `band3.objects` block, alphabetically by full path (`band3/game/...`
   before `band3/meta_band/...` before `band3/net_band/...` before
   `band3/tour/...`).
7. **Validate JSON**: `python3 -c "import json; json.load(open('config/45410914/objects.json'))"`.
   Must exit 0.
8. **Force re-split + build**: `touch config/45410914/config.yml && ninja 2>&1 | tail -30`.
9. **Existence checks** (§5.2): run the bash loop; expect all 15 OKs (no MISS).
10. **Asm-head start-addr checks** (§5.3): run the bash loop; expect all OKs.
    A FAIL here means the pin start landed in the middle of a function —
    revert that pin and skip it (document under §6).
11. **Expansion growth check** (§5.4): confirm the 3 expanded `.s` files
    grew vs their pre-edit sizes.
12. **Re-read `splits.txt`** and verify dtk back-filled `.pdata` lines for
    the 12 new TUs (§5.5). Missing `.pdata` is WARN-tolerated.
13. **Stop.** Do not port source, do not run objdiff. This plan is coverage-
    only; matching is downstream work.
