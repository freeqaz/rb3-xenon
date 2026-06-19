# W7 Hashmap-Thin: MoviePanel + FixedSizeSaveableStream Scan

**Date**: 2026-06-19  
**Scout mode**: Read-only, no source edits, no ninja runs.  
**Baseline**: 8220 matched functions.

## Purpose

A1 dual-COMDAT scan identified two "thin" units with a single call to the hashmap
find COMDATs:
- `MoviePanel` unit: 1 call to `fn_82543F88` (Symbol-key hash_map find)
- `FixedSizeSaveableStream` unit: 1 call to `lbl_82552CD0` (int-key hash_map find)

This dossier characterises each, determines whether the container is a class member
(actionable) or stack-local/refuted (eliminated), and produces the cold-executable
action plan.

---

## Candidate 1: MoviePanel — ELIMINATED

### Identification

The call to `fn_82543F88` (Symbol-key hash_map find) appears at VA **0x8278BE94**,
inside function `fn_8278BE70` (size 0x68, VA 0x8278BE70).

```asm
/* 8278BE84  7C 7F 1B 78 */  mr r31, r3         # save "result_out" arg to r31
/* 8278BE88  38 A1 00 94 */  addi r5, r1, 0x94  # &key (Symbol, saved on stack)
/* 8278BE8C  38 84 00 44 */  addi r4, r4, 0x44  # container = arg2 + 0x44
/* 8278BE90  38 61 00 50 */  addi r3, r1, 0x50  # sret buffer
/* 8278BE94  4B DB 80 F5 */  bl fn_82543F88     # Symbol-key hash_map find
```

r4 at entry is the SECOND argument, not `this` (r3). Therefore the container is
at `arg2 + 0x44`, not at `MoviePanel_this + 0x44`.

### Call chain

```
fn_8278BED8  = ?IsScreenInSceneMap@MetaMusicManager@@ABA_NVSymbol@@@Z
               (confirmed via target_symbol_map.json: 0x8278BED8)
  calls fn_8278BE70 with r4=MetaMusicManager_this, r5=Symbol
  → fn_8278BE70: container = MetaMusicManager_this + 0x44
```

`fn_8278BED8` is `MetaMusicManager::IsScreenInSceneMap(Symbol)` (confirmed by
`target_symbol_map.json` entry `0x8278BED8 → ?IsScreenInSceneMap@MetaMusicManager@@ABA_NVSymbol@@@Z`).

### Container identity

MetaMusicManager retail layout (derived from asm):
- `m_mapScenes` (hash_map\<Symbol, MetaMusicScene*\>) at **0x28** (size 0x1C)
- `m_mapScreenToScene` (hash_map\<Symbol, Symbol\>) at **0x44** (size 0x1C)

Source layout has Hmx::Object base = 0x1C, maps at 0x1C and 0x34. Retail Hmx::Object
base = 0x28, both maps use hash_map. DC3 matches MetaMusicManager 100% at std::map
offsets — retail RB3 diverges here.

### Why not actionable

1. The container (`MetaMusicManager::m_mapScreenToScene` at this+0x44) is NOT a
   MoviePanel member.
2. `MetaMusicManager.cpp` is **not in `config/45410914/objects.json`** — there is no
   compiled base obj for MetaMusicManager. All three MetaMusicManager functions in the
   MoviePanel pin (`IsScreenInSceneMap`, `~MetaMusicManager`, `MetaInit`) read 0%
   because base_size=0 (stub), not because of a map-type mismatch.
3. Adding hash_map to MetaMusicManager.h would fix those fns only if
   MetaMusicManager.cpp were compiled. That is a separate porting task
   (add MetaMusicManager.cpp to objects.json, port the source to hash_map layout
   with a per-TU `RB3_HASH_SYMBOL_DEFINED` gate + new Hmx::Object base size gate).

**Verdict**: REFUTED as a hashmap-thin quick win. Stack-local / wrong-class. Not a
MoviePanel member. actionable=false.

---

## Candidate 2: FixedSizeSaveableStream — GENUINE

### Unit status

`FixedSizeSaveableStream.cpp` IS in objects.json as `NonMatching`. Pin range:
`.text 0x82786468..0x82786788`. Report: 5/11 functions matched (45.5%), 33.1% fuzzy.

### The find-using function

`fn_82786468` (size 0x68 = 104 bytes, VA 0x82786468, at pin offset 0x00):

```asm
/* 82786478  90 A1 00 94 */  stw r5, 0x94(r1)   # save int key to stack
/* 8278647C  7C 7F 1B 78 */  mr r31, r3         # save this
/* 82786480  38 A1 00 94 */  addi r5, r1, 0x94  # &key (int on stack)
/* 82786484  38 84 00 4C */  addi r4, r4, 0x4c  # container = arg2 + 0x4C
/* 82786488  38 61 00 50 */  addi r3, r1, 0x50  # sret buffer
/* 8278648C  4B DC C8 45 */  bl lbl_82552CD0    # int-key hash_map find
```

Caller `fn_82786500` (`HasID`, 0x48 bytes, target_symbol_map `0x82786500 → ?HasID@...`):
```asm
mr r5, r4     # int ID arg → r5
mr r4, r3     # this → r4
addi r3, r1, 0x50
bl fn_82786468
```

So `fn_82786468` receives: r3=sret, r4=`this` (FixedSizeSaveableStream), r5=int.
Container = **this + 0x4C** = `m_mapIDToSymbol` in retail.

### Container is a class member — confirmed

Constructor `fn_82786550` (VA 0x82786550) initialises:
1. `addi r3, r3, 0x4c` → `fn_82547CC8` (hash_map default ctor) at **this+0x4C**
2. `addi r3, r30, 0x30` → `fn_82547CC8` at **this+0x30**

`SetSymbolID` (`fn_82786738`):
- `addi r3, r3, 0x30` → `fn_82590258` (hash_map\<Symbol,int\>::operator[]) at **this+0x30**
- `addi r3, r31, 0x4c` → `fn_82561180` (hash_map\<int,Symbol\> insert) at **this+0x4C**

This maps to:
```
m_mapSymbolToID  (Symbol→int)  retail: hash_map, size 0x1C, at 0x30   (source: std::map, 0x18, at 0x30)
m_mapIDToSymbol  (int→Symbol)  retail: hash_map, size 0x1C, at 0x4C   (source: std::map, 0x18, at 0x48)
```

Source offset 0x30 matches retail (first map occupies 0x30..0x4B in retail vs 0x30..0x47 in source).
The 4-byte shift at m_mapIDToSymbol (0x48→0x4C) comes from the first map being 0x1C (hash_map)
instead of 0x18 (std::map).

### 100% already-matched functions — false comfort

`HasSymbol` (100%) and `HasID` (100%) appear correct but are thin wrappers:
- HasSymbol calls fn_82786420 (GetID, outside the pin, VA-paired) — works regardless of map type.
- HasID calls fn_82786468 (GetSymbol helper, in pin, 0%) — normalized 100% because the
  relocation (target: `fn_82786468`, base: `GetSymbol`) is normalised away. The callee code
  is wrong; the wrapper shell matches. These 100%s are **relocation-normalisation false positives**.

### Affected zero-percent functions (6 total)

| VA         | Inferred function         | Size | Notes |
|------------|--------------------------|------|-------|
| 0x82786468 | GetSymbol helper          | 0x68 | lbl_82552CD0 caller; directly fixed by hash_map |
| 0x82786550 | ctor (void*, int, bool)   | 0x5C | initialises both maps |
| 0x82786618 | InitializeTable           | 0x64 | clears at 0x4C then 0x30 |
| 0x82786680 | SaveTable or LoadTable    | 0x28 | jeff mis-nest: .endfn too early; remaining code at 827866A8..827866C8 is outside .fn bracket — will remain 0% until jeff fix |
| 0x827866D0 | AddSymbol or LoadTable    | 0x28 | accesses m_iCurrentID at 0x68, uses both maps |
| 0x82786738 | SetSymbolID               | 0x4C | uses fn_82590258 + fn_82561180 |

Expected delta: **+4 to +5** (fn_82786680 blocked by jeff mis-nest; fn_827866D0 may have
secondary issues from out-of-pin helpers).

### Map entries needed

Zero-percent functions lack target_symbol_map entries. After hash_map conversion makes
them byte-exact, they will still read 0% without names. Required additions to
`scripts/target_symbol_map.json`:

| VA (hex)   | Mangled name |
|------------|--------------|
| 0x82786468 | `?GetSymbol@FixedSizeSaveableStream@@QBA?AVSymbol@@H@Z` |
| 0x82786550 | `??0FixedSizeSaveableStream@@QAA@PAXH_N@Z` |
| 0x82786618 | `?InitializeTable@FixedSizeSaveableStream@@QAAXXZ` |
| 0x82786680 | `?SaveTable@FixedSizeSaveableStream@@QAAXXZ` (or LoadTable — verify) |
| 0x827866D0 | `?AddSymbol@FixedSizeSaveableStream@@QAAHVSymbol@@@Z` |
| 0x82786738 | `?SetSymbolID@FixedSizeSaveableStream@@QAAXVSymbol@@H@Z` |

Note: fn_82786680 and fn_827866D0 identities need confirmation (check vs rb3-Wii
source body). fn_82786680 accesses `lbl_82C47B60` (a global MaxSymbols constant)
and calls `fn_8277DEF8` (LoadSymbolTable?) — cross-check ordering with source to
distinguish SaveTable vs LoadTable.

---

## Implementation Plan — FixedSizeSaveableStream

### Prerequisite check

Verify no other TU uses `FixedSizeSaveableStream.h` with `std::map` fields
(would cause cross-TU regressions on a header change).

```bash
grep -rn "FixedSizeSaveableStream" src/ config/
# Also check if any extra_cflags gating is needed
```

### Step 1 — Convert maps in header

In `src/system/meta/FixedSizeSaveableStream.h`:

1. Replace `#include <map>` with `#include <hash_map>` (or add it alongside).
2. Change member types:
   ```cpp
   // Before:
   std::map<Symbol, int>    m_mapSymbolToID; // 0x30
   std::map<int, Symbol>    m_mapIDToSymbol; // 0x48
   // After:
   std::hash_map<Symbol, int>  m_mapSymbolToID; // 0x30  (size 0x1c retail)
   std::hash_map<int, Symbol>  m_mapIDToSymbol; // 0x4c  (size 0x1c retail)
   ```
3. Add hash specialisation for Symbol if not already present (guard with a unique
   `#define RB3_FSSM_HASH_SYMBOL_DEFINED` to avoid collision with other hash<Symbol>
   specialisations in the same TU's headers).
4. Update comments: `// 0x48` → `// 0x4c`.

### Step 2 — Update cpp usage

`FixedSizeSaveableStream.cpp` uses:
- `m_mapSymbolToID.find(s)` → hash_map find, same syntax ✓
- `m_mapIDToSymbol.find(i)` → hash_map find, same syntax ✓
- `m_mapSymbolToID.begin()`/`end()` → same ✓
- `m_mapIDToSymbol.clear()` / `m_mapSymbolToID.clear()` → same ✓
- `m_mapSymbolToID[s] = i` → hash_map operator[] ✓
- `m_mapIDToSymbol[i] = s` → hash_map operator[] ✓
- Iterators: change `std::map<...>::const_iterator` to `std::hash_map<...>::const_iterator` in GetID/GetSymbol.

### Step 3 — Add map entries

Add to `scripts/target_symbol_map.json`:
```json
"0x82786468": "?GetSymbol@FixedSizeSaveableStream@@QBA?AVSymbol@@H@Z",
"0x82786550": "??0FixedSizeSaveableStream@@QAA@PAXH_N@Z",
"0x82786618": "?InitializeTable@FixedSizeSaveableStream@@QAAXXZ",
"0x82786680": "?SaveTable@FixedSizeSaveableStream@@QAAXXZ",
"0x827866D0": "?AddSymbol@FixedSizeSaveableStream@@QAAHVSymbol@@@Z",
"0x82786738": "?SetSymbolID@FixedSizeSaveableStream@@QAAXVSymbol@@H@Z"
```
(Confirm fn_82786680 vs fn_827866D0 identities against rb3-Wii source before committing.)

### Step 4 — Build and verify

```bash
# In a worktree (setup_worktree.sh first):
rm -f build/45410914/target_symbol_renames.stamp
touch config/45410914/config.yml
NINJA_JOBS=8 tools/fresh_report.sh
# Re-run once to clear splits-only divergence FP
```

Run `run_objdiff` on each of the 6 functions to confirm:
- fn_82786468: should now show lbl_82552CD0 in base (not _Rb_tree)
- fn_82786550: ctor should match hash_map init order (0x4C first, then 0x30)
- fn_82786618: InitializeTable clear order should match
- fn_82786680: will remain mismatched until jeff mis-nest fix
- fn_827866D0, fn_82786738: should match

### Step 5 — Honesty gate

Check that no >=8-contiguous foreign fn_@0% run appears in the FSSM range.
The unit has no known interleaved foreign functions. fn_82786680 at 0% (jeff
mis-nest) is an own-unit function and does NOT constitute a foreign run.

---

## Summary Table

| Candidate | Kind | Container | actionable | Expected Δ | Blocker |
|-----------|------|-----------|-----------|-----------|---------|
| MoviePanel fn_8278BE70 | Eliminated | MetaMusicManager::m_mapScreenToScene (this+0x44) | NO | 0 | MetaMusicManager.cpp not compiled; not a MoviePanel member |
| FixedSizeSaveableStream fn_82786468 | Genuine hash_map member | m_mapIDToSymbol (this+0x4C) | YES | +4 to +5 | jeff mis-nest blocks fn_82786680; map entries needed |
