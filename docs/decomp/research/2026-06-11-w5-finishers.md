# Wave-5 finishers dossier (2026-06-16, read-only scout @ main, baseline 8038)

## Executive summary

Two lanes:

**Lane A — UIComponent finishers**: Both target functions are WALLS in the current
state. `Update()` (69.84%) has a deep structural divergence (wrong vector element
type, wrong Find<> template instantiation, target 292 bytes larger). `ResourceFileUpdated`
(88.84%) is permuter-class (regalloc: extra String copy-constructor on stack). Neither is
fixable by a single source edit. Both are deferred.

**Lane B — SongMgr bonus reveals**: The SongMgr layout fix was already landed prior to
this scouting session (all 9 layout near-misses now at 100%). The prior dossier's 7
"reveal" candidates reduce to 4 valid GO items + 3 refuted:
- GO (+4): `SaveWrite`, `ContentName(int)`, `GetSongsInContent`, `CacheSongData` — all have
  compiled bodies in SongMgr.obj but no target_symbol_map entries. Add 4 entries, force
  re-rename, expect net +4 at 100%.
- REFUTED (0x823D0078 / ~MemStream): the function at that VA is `fn_823D07E4` in
  auto_03_823CE220_text (auto blob, no compiled base). `??1MemStream@@UAA@XZ` is declared
  `virtual ~MemStream() {}` — empty inline, no standalone COMDAT symbol in MemStream.obj.
- REFUTED (0x82631160 / operator<<(map)): no function at that VA exists in the report.
  The named `operator<<(BinStream&, map<Symbol,vector<int>>)` is at VA 0x8263107c in
  `default/auto_03_82627200_text` — an auto blob with no compiled base (0% due to missing
  TU, not a missing map entry).
- ALREADY DONE (0x82783AE8 / SaveMount): already in map, already at 100%.

**Overall GO: +4 (SongMgr reveals). UIComponent: 0 (both walls).**

---

## Part A: UIComponent::Update (69.84%) — wall analysis

### Measurements (fresh report @ baseline 8038)

```
?Update@UIComponent@@UAAXXZ  69.836 norm  68.795 fuzzy  default/UIComponent
```

Target size: 1192 bytes. Base size: 900 bytes. **Size delta: +292 bytes (target larger).**

### Decisive evidence

The diff shows a deep structural divergence, not a member-offset mismatch:

1. **Wrong vector element type.** Base (ours) calls `_M_erase<UIMesh>` and
   `push_back<UIMesh>` (the `UIComponent::UIMesh` inner class).
   Target calls `_M_erase<Key<Transform>>` and `push_back<SongSection>`.
   The vectors are entirely different types.

2. **Wrong Find<> template.** Base calls `rdir->Find<RndMesh>(...)` and
   `rdir->Find<RndMat>(...)`. Target calls `rdir->Find<RndPostProc>(...)` — a
   completely different resource type.

3. **Extra code paths (41 + 17 deleted instructions in two clusters).** The
   target's `else` branch (rdir == null) has 41 extra instructions before the
   main MILO_WARN: additional PathName vcalls on the Dir() chain and a second
   `TypeDef()->FindArray("types", true)` lookup. The rb3-Wii source has neither.

4. **Extra check in mResourceDir == 0 branch.** Idx 30-46: target has 17 extra
   instructions including a `PathName(Dir())` call + a null-check on a field at
   offset 0x20 before branching to the MILO_FAIL path. The rb3-Wii source has no
   such early check.

### Root cause and wall classification

**STRUCTURAL_DIVERGENCE**: The retail RB3 `Update()` processes a completely
different mesh/material representation than what the rb3-Wii oracle describes.
The rb3-Wii source uses `UIMesh { RndMesh*, RndMat*[5] }` with `Find<RndMesh>` +
`Find<RndMat>`. The retail Xbox binary processes `Key<Transform>` entries with
`Find<RndPostProc>`, suggesting the UIComponent mesh system was substantially
rewritten between the Wii dev build and the 360 retail build. This is NOT a
layout/member fix — the entire inner loop algorithm differs.

Reconstructing the retail Update() requires: (a) identifying what `Key<Transform>`
is in the retail context (possibly a different struct aliased to the same name),
(b) understanding why `RndPostProc` is being searched (PostProc effects?),
(c) mapping the extra pre-loop checks to the correct rb3-era structs. This is a
multi-function structural archaeology task, not a one-liner.

**WALL CLASS: STRUCTURAL_DIVERGENCE (deep, defer — requires UIMesh redefinition
and inner-loop reconstruction).**

---

## Part B: UIComponent::ResourceFileUpdated (88.84%) — wall analysis

### Measurements

```
?ResourceFileUpdated@UIComponent@@QAAX_N@Z  88.839 norm  88.750 fuzzy  default/UIComponent
```

Target size: 224 bytes. Base size: 220 bytes. **Frame: 0xa0 (target) vs 0x90 (base).**

### Decisive evidence

Full listing diff (key cluster idx 14-27):

```
target idx 14: addi r4, r30, 0x118        -- r4 = &mResourceName (src for copy)
target idx 15: addi r3, r31, 0x60         -- r3 = stack+0x60 (dst for String temp)
target idx 16: bl ??0String@@QAA@ABV0@@Z  -- copy-construct String temp at 0x60
target idx 17: lis r11, [fmt str]         -- format string
target idx 18: mr r5, r3                  -- r5 = &temp (copy-ctor return = stack+0x60)
target idx 21: lwz r4, 0x138, r30         -- r4 = mResourcePath.mStr (c_str)
target idx 22: addi r3, r11, [fmt]        -- r3 = format string ptr
target idx 23: bl ??$MakeString@PBDVString@@ -- MakeString(fmt, cstr, &temp)

base idx 18 (replace): addi r5, r30, 0x118 -- r5 = &mResourceName (direct, no copy)
base idx 19 (insert):  addi r3, r11, fmt   -- r3 = format ptr
base idx 20 (insert):  addi r4, r31, 0x50  -- r4 = ptr-slot on stack
base idx 23: bl ??$MakeString@PBDVString@@ -- MakeString(fmt, cstr_ref, &mResourceName)

FilePath slot: target 0x50, base 0x58  (+0x8 in base because no temp String at 0x50)
Frame size: target 0xa0, base 0x90     (+0x10 in target = String temp at 0x60)
Epilogue: addi r1, r31, 0xa0 vs 0x90  (same frame difference)
```

### Root cause and wall classification

The target makes an explicit String copy of `mResourceName` before passing it to
`MakeString`. Both sides call the same template instantiation
`??$MakeString@PBDVString@@@@YAPBDPBDABQBDABVString@@@Z`. The difference is whether
the compiler is allowed to pass the original member reference (base) or must
copy it to a callee-save register slot (target).

This is a **PERMUTER_CLASS** regalloc/scheduling divergence. The source body in
both rb3-Wii and our current `src/system/ui/UIComponent.cpp` is identical:
```cpp
const char *pathstr = MakeString("%s/%s.milo", mResourcePath.c_str(), mResourceName);
```
Passing `mResourceName` directly as `const String &`. The retail compiler chose to
copy; ours didn't. No source-level change can reliably force or prevent this copy —
it's a register-allocation scheduling decision affected by surrounding call context.

Tried variants that do NOT fix this (all produce the same no-copy base):
- `String temp(mResourceName); MakeString(fmt, ..., temp)` — might produce copy
  but changes frame slot order and is semantically equivalent only if optimizer
  doesn't deduplicate
- Since the frame is +0x10 with the copy and FilePath shifts from 0x58 to 0x50,
  adding a `String tmp` local may force the right layout. However, this requires
  verifying that the tmp is actually used for MakeString (not optimized away) AND
  that FilePath ends up at 0x50.

**WALL CLASS: PERMUTER_CLASS (regalloc copy-elision; hand-edit of MakeString form
`MakeString(fmt, mResourcePath.c_str(), String(mResourceName))` is worth one quick
try in a worktree — if it produces the right frame and copy-ctor it's a +1. If not,
defer to /permute).**

**Note — one-try hypothesis for implementer**: change the ResourceFileUpdated body to:
```cpp
void UIComponent::ResourceFileUpdated(bool b) {
    if (!mResourceName.empty()) {
        mResourcePath = GetResourcesPath();
        String resourceName(mResourceName);  // force copy to stack
        const char *pathstr =
            MakeString("%s/%s.milo", mResourcePath.c_str(), resourceName);
        mResourceDir.LoadFile(FilePath(FileRoot(), pathstr), b, true, kLoadFront, false);
        if (!b)
            mResourceDir.PostLoad(0);
    } else
        mResourceDir = 0;
    if (!b)
        Update();
}
```
This is a **single quick test** (A/B in worktree, 5 minutes). If the frame becomes
0xa0 and FilePath slot becomes 0x50, it's a +1. If not, abandon and route to
/permute. Do NOT commit unless it reaches 100% report-normalized.

---

## Part C: SongMgr reveal candidates — analysis

### Current state (baseline 8038)

The SongMgr layout fix (MsgSource base + RB3_MAP_0x1C) was already landed before
this scouting session. All 9 layout near-misses are at 100%:

| function | report-norm |
|---|---|
| ?IsSongMounted@SongMgr@@QBA_NVSymbol@@@Z | 100.000 |
| ?GetContentNames@SongMgr@@UBAX... | 100.000 |
| ?GetCachedSongInfoSize@SongMgr@@QBAHXZ | 100.000 |
| ?ContentName@SongMgr@@QBAPBDVSymbol@@_N@Z | 100.000 |
| ?SaveCachedSongInfo@SongMgr@@QAA_NAAVBufStream@@@Z | 100.000 |
| ?HasSong@SongMgr@@QBA_NVSymbol@@_N@Z | 100.000 |
| ?SongAudioData@SongMgr@@QBAPAVSongInfo@@VSymbol@@@Z | 100.000 |
| ?LoadCachedSongInfo@SongMgr@@QAA_NAAVBufStream@@@Z | 100.000 |
| ?StartSongCacheWrite@SongMgr@@QAAXXZ | 100.000 |

### Verified reveal candidates (map entries missing → base_size > 0 → byte-exact expected)

Confirmed via COFF symbol table of `build/45410914/src/system/meta/SongMgr.obj`:

| target VA | fn name (report) | compiled mangled symbol | base size | GO? |
|---|---|---|---|---|
| 0x82783EC8 | fn_82783EC8 | `?SaveWrite@SongMgr@@IAAXXZ` | 180 bytes | YES |
| 0x82784040 | fn_82784040 | `?ContentName@SongMgr@@QBAPBDH@Z` | 112 bytes (28 insns) | YES |
| 0x827841B0 | fn_827841B0 | `?GetSongsInContent@SongMgr@@IBAXVSymbol@@AAV?$vector@HV?$StlNodeAlloc@H@stlpmtx_std@@@stlpmtx_std@@@Z` | 84 bytes (21 insns) | YES |
| 0x82785120 | fn_82785120 | `?CacheSongData@SongMgr@@IAAXPAVDataArray@@PAVDataLoader@@W4ContentLocT@@VSymbol@@@Z` | 500 bytes (125 insns) | YES |

Byte-exact confidence: HIGH. The layout fix is already applied to SongMgr.obj
(`StartSongCacheWrite` at 100% confirms correct mSongCache offset 0xc8, mState 0x50,
mSongIDsInContent 0x70, etc.). All four functions use these offsets and are pure
behavioral code with no additional layout complexity.

`SaveWrite` (180B) target instructions confirmed: `lwz r3, 0xc8, r30` (mSongCache),
`bl ?SetState@SongMgr@@IAAXW4SongMgrState@@@Z`, `bl ?SaveCachedSongInfo@@` —
exact match to the source body at SongMgr.cpp:426-437.

### Refuted candidates

| candidate | reason |
|---|---|
| 0x823D0078 → ??1MemStream@@UAA@XZ | VA 0x823D0078 = fn_823D07E4 in auto_03_823CE220_text (auto blob, no compiled base). MemStream dtor is declared `virtual ~MemStream() {}` = inline empty = no standalone COMDAT. |
| 0x82631160 → operator<<(map<Symbol,vector<int>>) | No function at VA 0x82631160 in report. Named op<< is at VA 0x8263107c in auto_03_82627200_text (auto blob, no compiled base). Needs a TU pin, not a map entry. |
| 0x82783AE8 → SaveMount | Already in map, already 100%. Done. |

### Implementation plan (numbered, cold-executable)

1. **Worktree**: `scripts/setup_worktree.sh /tmp/wt-songmgr-reveals songmgr-reveals`
2. **Baseline**: `./tools/ninja-locked && python3 -c "import json;print(json.load(open('build/45410914/report.json'))['measures']['matched_functions'])"` — expect 8038.
3. **Add 4 map entries** to `scripts/target_symbol_map.json`:
   ```json
   "0x82783EC8": "?SaveWrite@SongMgr@@IAAXXZ",
   "0x82784040": "?ContentName@SongMgr@@QBAPBDH@Z",
   "0x827841B0": "?GetSongsInContent@SongMgr@@IBAXVSymbol@@AAV?$vector@HV?$StlNodeAlloc@H@stlpmtx_std@@@stlpmtx_std@@@Z",
   "0x82785120": "?CacheSongData@SongMgr@@IAAXPAVDataArray@@PAVDataLoader@@W4ContentLocT@@VSymbol@@@Z"
   ```
   Note: the map uses hex string keys (the existing `"0x82783AE8"` entry for SaveMount
   confirms this key format).
4. **Force re-rename**: `rm -f build/45410914/target_symbol_renames.stamp && touch config/45410914/config.yml`
5. **Build and measure**:
   ```bash
   ./tools/ninja-locked 2>&1 | tee /tmp/rb3_build_songmgr_reveals.log | tail -5
   python3 -c "import json;print(json.load(open('build/45410914/report.json'))['measures']['matched_functions'])"
   ```
   Expect 8042 (+4). If divergence warning fires, re-run once (FP for splits-only changes).
6. **Verify each fn individually**:
   ```bash
   # For each of the 4, run_objdiff and confirm 100% normalized:
   run_objdiff(symbol="?SaveWrite@SongMgr@@IAAXXZ", unit="default/SongMgr", ...)
   ```
   If any reads <100%, check for: (a) source body missing/wrong, (b) layout offsets
   not matching (re-run run_diff_inspect mode=offsets).
7. **Honesty gate**: zero regressions (compare full measures.matched_functions before and
   after; no named function should drop from 100% to <100%). Header is NOT touched so
   cross-TU regression risk is low.
8. **Land**: `git add scripts/target_symbol_map.json && git commit -m "..."`

### Optional: ResourceFileUpdated one-try

After the reveal additions (step 5 green), optionally attempt the `String temp` copy:
1. Edit `src/system/ui/UIComponent.cpp::ResourceFileUpdated` to use `String resourceName(mResourceName)`
2. Run `run_objdiff` on `?ResourceFileUpdated@UIComponent@@QAAX_N@Z` — if frame=0xa0
   and FilePath slot=0x50, proceed to full binary verify.
3. If not frame 0xa0, REVERT and route to /permute.

---

## Expected delta

- SongMgr reveals: **+4** (SaveWrite + ContentName(int) + GetSongsInContent + CacheSongData)
- UIComponent::Update: **0** (structural wall, do not attempt)
- UIComponent::ResourceFileUpdated: **0 expected** (permuter-class; optionally +1 if the
  String temp copy hypothesis holds — single 5-minute test worth attempting first)

**Conservatively: +4. Optimistically: +5.**

## Wall summary per function

| function | norm% | wall class |
|---|---|---|
| ?Update@UIComponent@@UAAXXZ | 69.84 | STRUCTURAL_DIVERGENCE — vector type wrong (Key<Transform> vs UIMesh), wrong Find<> template (RndPostProc vs RndMesh), extra code paths; 292 bytes larger than base; defer to reconstruction |
| ?ResourceFileUpdated@UIComponent@@QAAX_N@Z | 88.84 | PERMUTER_CLASS (regalloc copy-elision: target copy-constructs String temp, base passes direct ref; frame +0x10 in target); one-try String temp copy hypothesis before /permute |
