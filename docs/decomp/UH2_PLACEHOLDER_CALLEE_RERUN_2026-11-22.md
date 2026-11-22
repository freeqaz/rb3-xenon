# UH2: placeholder-callee census rerun on current main (2026-11-22)

> **STATUS (2026-11-22):** lane record. Branch `uh2`, not merged. **No new wrong
> callee in the requested rings**, so there is no source change and nothing to price.
> The census gained a column that sizes the UNALIGNED blind spot.

## Question

The shipped ruler (`functionRelocDiffs=name_check`) forgives any relocation whose
retail target is a placeholder (`fn_8XXXXXXX`), so a row can read 100 while calling
the wrong function. W16-UH (`W16UH_PLACEHOLDER_CALLEE_CENSUS_2026-10-07.md`)
built `tools/placeholder_callee_census.py` to find such sites and adjudicate them on
retail bytes, and W16-UK fixed the "right callee, body differs" rows. The last run
was on 10-07 (`b070a9c6d`). This lane reruns the census on current main over IN-CORE,
IN-SOON, IN-RB3ENG and VIA-DC3. Every non-PROVEN pair that is new since then gets
a verdict, and the unaligned rows are sized and read.

**Prediction going in:** a few new wrong callees, most likely in TUs written since 10-07
from retail bytes with no oracle (AuditionMgr, BandNetGameData), because their authors
had to name callees by address.

## Tree and build

Worktree `~/tmp/wt-uh2`, rebased onto main `d11946928` (it includes `bngd` and
`audmgr`). After a full `./tools/ninja-locked` build, `verify_objs_patched.py --check`
reports a fixed point of the 6 passes. `report.json` (ruler `name_check`):
**55,114 matched fns / 59.466010% code**. A first pass on `55008f5c5`, before `bngd`
landed, gave the same in-scope verdicts, except that BandNetGameData's ctor and
`Poll` were then NO-OURS; with `bngd` in, they are named on both sides and leave the
census.

## Controls (`--controls`, on `d11946928`)

| control | result |
|---|---|
| K1 known answer (`SystemInit` site at `0x82522608`; `GlitchFinder::Init` must not PROVE) | site found (`StageKitInit`); GlitchFinder **REFUTED** (retail 480 B, ours 156 B) |
| K2 positive (1,500 named slots, (N,N) at fuzzy 100) | **1,463 / 1,500 PROVEN (97.5%)** |
| K3 negative (nearest-size other callee) | **0 / 1,500 PROVEN** |
| K3b depth-0 bytes only (measurement) | 34 / 1,500 (2.27%) |

The comparator can pass, can fail, and fails on the known wrong callee.

## Census

| ring | paired rows | STRICT | BL | unaligned | rows with sites | of them fuzzy 100 | sites | PROVEN | NO-OURS | REFUTED |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| IN-CORE | 2,514 | 2,452 | 26 | 36 | 51 | 48 | 63 | 6 | 53 | 4 |
| IN-SOON | 11,052 | 10,907 | 99 | 46 | 151 | 144 | 202 | 67 | 83 | 52 |
| IN-RB3ENG | 4,186 | 4,100 | 70 | 16 | 25 | 24 | 48 | 31 | 10 | 7 |
| VIA-DC3 | 9,054 | 8,852 | 164 | 38 | 117 | 108 | 178 | 10 | 113 | 55 |
| **requested scope** | **26,806** | | | **136** | **344** | **324** | **491** | **114** | **259** | **118** |

In-scope distinct (A, F) pairs: **203** (34 PROVEN, 144 NO-OURS, 25 REFUTED). Over all
walked rings: 428 pairs (35 PROVEN, 361 NO-OURS, 29 REFUTED, 2 MAPPED-ELSEWHERE,
1 BYTES-EQUAL). The out-of-scope pairs (`JoinSession`, `CreateXAudio2Object`,
`~FxSend360`) are unchanged from 10-07 and were not re-read.

**Why the in-scope site count halved (1,024 → 491).** 214 of the 236 pairs PROVEN on
10-07 are gone, and the address of **every one of the 214** is now in
`target_symbol_map.json`. The map grew by 100 rows net, 36,313 at `e5a47716c` to
36,413 now. Their sites are named on both sides, so the ruler now checks them. This is
a real change, not a broken walk: K1 still finds its site and K2 still passes.

## What is new since 10-07, and the verdicts

`/home/free/tmp/uh2/diffcensus.py` keys every in-scope pair against
`~/tmp/w16uh/census_after.json` (the 10-07 final run). Counts are in-scope distinct pairs.

| verdict | same as 10-07 | changed | new |
|---|---:|---:|---:|
| PROVEN | 22 | 8 (REFUTED → PROVEN: W16-UK's body fixes) | 4 |
| REFUTED | 25 | 0 | 0 |
| NO-OURS | 129 | 0 | **15** |

No old pair gained a call site: every site of every non-PROVEN pair is one 10-07
already saw. So the only sites needing a verdict are the 15 new NO-OURS pairs, all
in today's `AuditionMgr` (IN-SOON). All 15 rows score fuzzy 100.

### The 15 AuditionMgr callees: all **right callee**

Each callee has no compiled body (declared, "not yet written" or "Name ours"), so bytes
cannot be compared. Two checks were made by hand: the declaration's address comment
equals the address retail calls at the census site, and the retail body at that address
fits the role. Bodies were read from the target objs (relocations in code order) and,
for `0x82B8C928`, with `tools/va_disasm.py`.

| retail | our callee (site) | retail body (what was checked) |
|---|---|---|
| `0x82560B08` 208 B | `AuditionDownloader::AuditionDownloader(const char*, const char*)` (`EnterDownload`) | `String` ctor, `operator new`, `memset`, then `bl 0x82B8C928` (`SetEnabled`), all inside AuditionMgr's span |
| `0x82560E80` 80 B | `AuditionDownloader::Close` (`ExitLoad`) | calls `0x82560C28`, then `MemFree` |
| `0x82561460` 112 B | `~AuditionDownloader` (`EnterUnmount`) | `bl 0x82560E80` (`Close`), then `~String` |
| `0x825614F8` 84 B | `AuditionDownloader::Poll` (`Poll`) | two in-span helpers |
| `0x82561A70` 168 B | `AuditionMgr::SendToScreen(Message&)` (`EnterValidated`) | loads a screen global and releases a `DataArray` (message handle) |
| `0x82562608` 584 B | `AuditionMgr::EnterState4` (`SetState`) | `UIEventMgr::HasActiveTransitionEvent` / `TriggerEvent`, `SetEnabled`, `UpdateScreen` |
| `0x82562978` 1,156 B | `AuditionMgr::EnterLoad` (`SetState`) | `UpdateScreen`, `BufStream`, `FileCache`, `TheSongMgrPtr` → `0x82577480`, `SetState` |
| `0x825632F0` 276 B | `AuditionMgr::EnterState7` (`SetState`) | in AuditionMgr's span; the source declares this address |
| `0x82563578` 400 B | `AuditionMgr::PollLoad` (`Poll`) | in AuditionMgr's span; the source declares this address |
| `0x82577480` 252 B | `BandSongMgr::RemoveSong(Symbol)` (`EnterFinish`) | in `BandSongMgr.obj`; `_M_find`/`erase` over the id and Symbol hash maps |
| `0x825EBF28` 172 B | `AuditionSessionBuilder::Finish` (`EnterFinish`) | calls two builder helpers in the same `auto_` span |
| `0x825EC118` 484 B | `AuditionSessionBuilder` ctor (`EnterStart`) | `Hmx::Object` ctor, `AddSink`, `OvershellPanel::GetSlot` / `OvershellSlot::GetUser` loop: fits the 5-argument ctor |
| `0x825EC480` 120 B | `PrepareAuditionSongFiles(Symbol)` (`ThreadStart`) | `FileGetPath`, `FileMkDir`, `MakeString` |
| `0x8272BAD8` 80 B | `~AuditionSongBuffer` (`ExitLoad`) | no relocations (a plain free of owned buffers) |
| `0x82B8C928` 8 B | `AuditionNetClient::SetEnabled(bool)` (`SetState`) | `lwz r3,0x3C(r3); b 0x82B8D9F8`, and `0x82B8D9F8` is `stb r4,0x69C(r3); blr`: it does take the bool |

These 15 retail functions still have no body in our tree. That is AuditionMgr's open
frontier (identification and port work), not a callee defect.

### The 25 carried-over REFUTED pairs: unchanged, right callees

These are all on 10-07's "right callee" lists. `ObjPtrList<T>::operator=` (`0x8230C628`,
16 spellings) is W16-UK's open stack-temporary residual: ours is now 204 B against
retail's 220 B. The others are the "bodies equal" set: `GetUser*` ×5 (`0x825150C8`),
`~MiniLeaderboardDisplay`, `E3CheatGetMinVelocity`, `Singer::Rollback` and
`MetaPerformer::GetVenue`.

`GetVenue` now reads retail 16 B against ours 12 B. This is a carving artifact, not a
body difference. Side by side, both are `lwz r11,0x40(r4); stw r11,0(r3); blr`, and
retail's extent also counts a trailing zero pad word (it is carved inside
`StoreSongSortNode.obj`).

### Dispersion screen (rerun on the new census)

The screen flags any callee of ours that stands for two retail addresses across all
aligned slots, named or placeholder. It fires on **0** census pairs, against 5 on 10-07
(those were the map defects and fixed sites W16-UH recorded). Control: the same walk
finds **62** callees of ours standing for two or more retail addresses, so it is not
structurally empty. All 62 go through named slots, so none has a placeholder address,
and the ruler already checks named slots.

### Fabricated externs

A tree-wide search for `merged_*` symbols finds only a comment (`flow/FlowPtr.h:63`).
`PlatformMgr_Xbox.cpp`'s two fabricated externs from 10-07 are gone.

## The UNALIGNED blind spot

The census now prints, per ring, the unaligned rows that make any call and their bytes
(the new column `unalgn_w/calls (B)`). In scope:

| class | rows | bytes | retail placeholder call slots |
|---|---:|---:|---:|
| no call on either side | 127 | 13,108 | 0 |
| call counts differ | 8 | 5,228 | 5 (`ThreadMemStack`) |
| retail calls, ours does not | 1 | 268 | 0 |
| **total unaligned** | **136** | **18,604** | **5** |

So the blind spot that could hide a wrong callee is **9 rows, 5,496 B**. Only one of
them calls a placeholder. I read all 9 by hand: the call sequences were diffed with
`difflib`, retail against ours.

| row (ring) | size, fuzzy | reading |
|---|---|---|
| `MakeBSPTree` (IN-CORE) | 1,580 B, 97.27 | ours calls `_List_base<BSPFace>::clear` twice out of line; retail has no such calls. Inlining, not a wrong callee |
| `NgLight::SphereConeTest` (VIA-DC3) | 1,236 B, 60.35 | the only difference is the save helpers (`gprlr_27` against `gprlr_24` + `fpr_26`): a frame, regalloc |
| `FileMakePath` (IN-CORE) | 792 B, 94.70 | ours makes one extra `strtok` call: a body difference |
| `SynthEmitter::SynthEmitter` (VIA-DC3) | 496 B, 86.13 | ours has an out-of-line `ObjPtr<RndTransformable>` ctor; save helper 29 against 28 |
| `NewFile` (IN-CORE) | 424 B, 84.39 | ours makes one extra `operator new` call: a body difference |
| `ThreadMemStack` (IN-CORE) | 348 B, 88.16 | the 5 `fn_8283D580` slots pair in order with our 5 `GetCurrentThreadId` calls; ours has one extra `CriticalSection::Exit` (as on 10-07) |
| `RndShader::Init` (VIA-DC3) | 268 B, 59.78 | retail saves and restores registers (`gprlr_28`); ours calls nothing: a frame difference |
| `map<Symbol,DataNode>::operator[]` (IN-CORE) | 220 B, 63.73 | ours has out-of-line `DataNode` ctor, `DataArray::Release` and save helpers |
| `operator>>(BinStream&, ObjectStage&)` (VIA-DC3) | 132 B, 79.39 | ours has an out-of-line `ObjPtr<ObjectDir>` ctor |

**None is a wrong callee.** Every difference is an extra or missing call (out-of-line
inlining, or an extra library call), or a save-helper choice. Their named callees are
checked by the ruler. The two extra calls (`FileMakePath`'s `strtok`, `NewFile`'s
`operator new`) are body work for a later lane.

## Not done

- No source change and no `ab_measure` run: no wrong callee was found, so there is
  nothing to price. No full build beyond the two needed (first base, then rebase).
- OUT-NET and OUT-360-OTHER (out of scope) were not re-adjudicated; their pair
  verdicts are unchanged from 10-07.
- BL-aligned rows pair slots by index (`relocation order is code order`). A row that
  drops one call and adds another of the same count would misalign silently. This is
  not sized here. 99 + 26 + 70 + 164 = 359 in-scope rows are BL-aligned.
- Unpaired rows (retail names our base obj does not define) are outside this census
  by construction. That is the identification gap.

## Reproduce

```
cd ~/tmp/wt-uh2 && ./tools/ninja-locked
python3 tools/placeholder_callee_census.py --controls
python3 tools/placeholder_callee_census.py --out ~/tmp/uh2/census3.json
```

The side scripts live in `/home/free/tmp/uh2/` (`diffcensus.py`, `unaligned2.py`,
`relocs.py`, `dispersion.py`, `disp_ctl.py`) and are not committed.
