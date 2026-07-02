# Port handoff — TambourineManager.cpp (rb3-xenon, WAVE-5)

Lane: `w5-tambourine`. Branch: `w5-tambourinemanager` (from `5cb96d4` on main).
Worktree: `/home/free/tmp/wt-w5-tambourinemanager`.
TU: `src/band3/game/TambourineManager.cpp` (412-line MWCC-Wii oracle -> MSVC PPC-Xenon),
wired NonMatching in `config/45410914/objects.json` (band3 module, mirrors StatCollector).

## Result
- **1 true-100 strict pin**: `TambourineGems` (100.0%, byte-equal, size-exact 24=24).
- **1 size-exact reloc-only partial (kept)**: `HandleButtonDown` (99.22%, 256=256).
- **3 dropped** (size-mismatch <100, reverted per policy): `TambourineSwing`,
  `TambourineSucceed`, `TambourineFail`. Ported source kept (fuzzy-paired).
- ICF verdict: **HONEST**. `build/45410914/icf_aliases.map` has 15 entries
  (debug target, ICF mostly off); neither pinned addr (0x826dbaa8 / 0x826dd6f0)
  appears -> genuine matches, not fold aliases. The 24B `TambourineGems` getter
  was the plan's flagged ICF risk; cleared.

## PINNED
| VA | size | norm% | fn | .pdata | note |
|---|---|---|---|---|---|
| 0x826dbaa8 | 24 (0x18) | 100.0 | `TambourineGems` | (none, leaf) | true-100 byte-equal; `return mPlayerRef.mVocalParts[0]->mVocalNoteList->mTambourineGems` (0x28->0x390->0->8->+0x24) |
| 0x826dd6f0 | 256 (0x100) | 99.22 | `HandleButtonDown` | 0x8222E908 | size-exact; ALL 10 diffs are reloc-naming (external sdata base `lbl_82DA0017` for TheProfileMgr/TheTaskMgr, float const, `MsToTick`<->`Movie::Terminate` ICF fold, intra-unit `bl TambourineSwing` cosmetic). Identity certain by adjacency + size. cf SlipTrack::Poll (99.46) precedent. Likely stays ~99.2 in the full report (sdata base rarely resolves) -> counts fuzzy, not strict. |

Mangled names extracted from the built `TambourineManager.obj` COFF (machine
0x01f2 = PPC BE), verified against `scripts/target_symbol_map.json` (ADD-only,
lowercase hex). `.pdata` BeginAddress read from the target `.pdata`
RUNTIME_FUNCTION table via `build/45410914/asm/DepthBuffer3D.s`.

## DROPPED (size-mismatch <100 — honest negatives, pins removed)
| VA | size (b/t) | norm% | fn | reason |
|---|---|---|---|---|
| 0x826dd580 | 264/288 | 65.0 | `TambourineSwing` | +4 layout drift (below) + register-alloc cascade (base promotes Symbol/vtable base to r29 -> `__savegprlr_29` + bigger frame; target keeps all-volatile r9/r10/r11) + string reloc `except_record_826E2938` vs MSVC COMDAT string |
| 0x826dc7c0 | 716/776 | 88.5 | `TambourineSucceed` | +4 layout drift + register off-by-one (base `__savegprlr_26` vs target `_25`) + `static Message` static-init + float/string reloc-naming |
| 0x826dcb88 | 560/620 | 85.6 | `TambourineFail` | same family as Succeed |

Their `.text`/`.pdata` ranges were returned to the unwired `DepthBuffer3D.cpp`
container; `target_symbol_map.json` entries removed.

## KEY LEAD for a lander with whole-binary A/B — the +4 layout drift
`mGemStates` (last member, a real 12B `std::vector<int>`) lands at **base 0x94 vs
target 0x90** (proven directly in the objdiff instruction listing: base
`lwz 0x94/0x98` vs target `lwz 0x90/0x94`). Every `mGemStates` access is +4, so
TambourineSwing/Succeed/Fail (which touch it) can't be byte-exact.
- Localized: `mTambourineIdx` matches at 0x54 in both; `mTambourineActive` 0x6c,
  `unk60` 0x70 confirmed. The +4 is entirely in the region **after `unk68`
  (0x78) and before `mGemStates`** — i.e. the member `unk6c`.
- `src/band3/game/TambourineManager.h` types `unk6c` as `std::vector<int>` (12B),
  but the real member there is **8B** (from the target: 0x7c + 8 + unk74/78/7c(12)
  = 0x90). `unk6c` is DEAD in the .cpp, so it's a header-guess artifact.
- FIX (owner-gated): shrink `unk6c` to an 8-byte placeholder (e.g. `int unk6c;
  int unk70;`) -> `mGemStates` moves to 0x90 and sizeof(TambourineManager) drops
  by 4. **BUT** `TambourineManager` is embedded by value in `VocalPlayer` at
  0x38c (`VocalPlayer.h`), so this shifts VocalPlayer's tail (mSectionStart*
  0x418->0x414). Requires a whole-binary composed A/B to confirm no VocalPlayer
  regression — NOT doable in an isolated lane (owner contention). Deferred here.
- Even WITH the layout fix, the 3 dropped fns retain register-allocation cascades
  + string/float reloc-naming (permuter-class) -> they will NOT reach true-100.
  So the layout fix is a VocalPlayer-layout lever, not a strict-pin lever.

## MWCC->MSVC adaptation notes (source, all compile-verified)
- `ObjectDir::sMainDir` (protected in xenon) -> public `ObjectDir::Main()`.
- `VocalPhrase::mStartTick` / `mDurationTicks` (Wii names) -> xenon
  `beatmatch/VocalNote.h` unk fields `unk8` / `unkc` (same offsets 0x8/0xc);
  `mTambourinePhrase` already named in xenon.
- `Sequence::mFaders` (protected in xenon) -> public accessor `Sequence::Faders()`.
- `mBank = nullptr`, `Hmx::Object::New<Fader>()`, `dynamic_cast<Sequence*>`,
  `std::vector<VocalPhrase>::iterator`, the trailing `VocalPlayer::IsAutoplay` +
  `BandTrack::SetTambourine` link-order defs — all compile clean under
  cl 16.00.11886.00 (only the shared xdk `__va_start`/`__frsqrte` intrinsic warns).
- No MILO_DEBUG blocks in this TU; the 3 assert-bearing dropped fns compile via
  `Debug.h`'s retail `MILO_ASSERT(cond,line) -> ((void)(cond))` path (not HX_NATIVE).

## Splits carve (DepthBuffer3D was an UNWIRED container -> low-risk)
Container `DepthBuffer3D.cpp` `.text [0x826D9F60,0x826DE1E0)` carved into disjoint
DepthBuffer3D fragments + `TambourineManager.cpp` holding only the 2 pinned fns:
`.text [0x826DBAA8,0x826DBAC0)` + `[0x826DD6F0,0x826DD7F8)`, `.pdata
[0x8222E908,0x8222E910)`. Overlap self-check: **0 pdata / 0 text overlaps, full
range coverage, no gaps**. DepthBuffer3D stays unwired (no report attribution to
protect).

## Verify method (per-function, no whole-binary build)
```
touch config/45410914/config.yml; rm -f build/45410914/target_symbol_renames.stamp
tools/ninja-locked build/45410914/config.json     # re-split -> obj/TambourineManager.obj (+ reconfigure objdiff.json)
tools/ninja-locked pre-compile                     # renamer: fn_<addr> -> mangled (from map)
tools/ninja-locked build/45410914/src/band3/game/TambourineManager.obj   # base obj
/home/free/code/milohax/rb3-xenon/build/tools/objdiff-cli diff -u default/TambourineManager '<mangled>' \
    --full-listing --analyze --format markdown -o /tmp/x.md   # (json for norm%; NEVER -o /dev/stdout)
```
Files touched: `src/band3/game/TambourineManager.cpp` (new), `config/45410914/objects.json`
(+1), `config/45410914/splits.txt` (carve), `scripts/target_symbol_map.json` (+2 ADD-only).
`global_fuzzy_pairs.json` in the worktree is a setup artifact — NOT staged.

## For the lander
- Land order-agnostic; touches only the unwired DepthBuffer3D container + band3 module.
- After land: grep `TambourineManager.cpp` in objects.json survived (shallow-union
  incident guard); confirm 0x826dbaa8 + 0x826dd6f0 report inside `TambourineManager`
  unit (not a foreign one). Both pins byte/size verified.
