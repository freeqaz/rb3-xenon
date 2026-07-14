# Clean retail TU5 vs RB3 Deluxe (RB3DX) — production + divergence

Status: **COMPLETE, byte-verified.** Read-only on decomp.db/config; nothing committed.
Date: 2026-07-07. Scratch: `rb3-xenon/_tu5probe/clean/`.

## TL;DR / verdict

- **Genuine clean retail TU5 produced** from `base(TU0, encrypted) + tu5/default.xexp`.
  Verified: version **0.0.5.1** (base_version 0.0.5.1), title **45410914**, entry
  **0x8283CD20**, **NO `rbdxcache`** (0 hits vs RB3DX's 1). PE decodes as sane
  section-mapped PPC.
- **RB3DX ≈ clean TU5 + a 170-byte Deluxe patch.** The two PEs are **byte-identical
  except 170 bytes / 14.36 MB (0.001%)**: 92 in `.text` (out of 10.3 MB code), 15
  in `.rdata`, 63 in `.data`. **Section tables are byte-for-byte identical.** RB3DX
  is literally clean TU5 with a handful of in-place hooks.
- **All 7 same-instrument patch functions + Layer-B helpers + CRT thunks are
  byte-identical, same VA, in both.** The code cave 0x82C8A000 is an all-zero free
  run in **both**. **ONE patch serves both RB3DX and clean TU5 unchanged.**
- **The base→RB3DX map transfers to clean TU5 at 100.000%** (12,817/12,817 mapped
  functions byte-identical). **No rebuild needed** — `base_to_tu5_map.json` *is* the
  base→clean-TU5 map.
- **Recommendation: rb3-xenon should target clean TU5 as canonical** (it is what
  RB3Enhanced targets and is DX-free), and it costs essentially nothing — the
  existing TU5 (=RB3DX-derived) map, addresses, patch, and cave all apply as-is.

---

## 1. Production recipe (clean TU5)

The retail `tu5/default.xexp` is a **delta over the ORIGINAL retail base XEX
(encrypted)**. It cannot be applied to the decrypted/decompressed base
(`default-binary_retail.xex`, sha1 `35adb6b4`) — that fails
`PatchIncompatible` because the XexPatcher validates the patch's image-key-source
against the base security-info AES key decrypted with the retail key (xex_patcher.cpp
line 351), and the decrypted base no longer carries the matching key.

**Use the encrypted retail base** instead:

| Input | Path | sha1 | state |
|---|---|---|---|
| Encrypted retail base (TU0) | `/srv/torrents/games/arbys/rb3/default_vanilla.xex` | `d57b7df5` | Retail / Uncompressed / **Encrypted**, v0.0.0.1 |
| TU5 title update | `.../360 xexp/tu5/default.xexp` | `21db6083` | XEX2 delta (LZX) |
| Applier | `rb3-xenon/tools/xexp-apply` (XenonRecomp `XexPatcher::apply`) | built this run | — |

```bash
# build the applier (XenonRecomp sibling checkout required)
cmake -S rb3-xenon/tools/xexp-apply -B .../build -DCMAKE_BUILD_TYPE=Release && cmake --build .../build
# apply
xexp-apply /srv/torrents/games/arbys/rb3/default_vanilla.xex \
           ".../360 xexp/tu5/default.xexp" clean_tu5.xex
dtk xex extract clean_tu5.xex      # -> band.exe  (renamed band_clean_tu5.exe)
```

### Output artifacts (scratch)

| Artifact | Path | sha1 | size |
|---|---|---|---|
| Clean TU5 XEX | `rb3-xenon/_tu5probe/clean/clean_tu5.xex` | `d56e7f31` | 15,675,392 |
| Clean TU5 PE | `rb3-xenon/_tu5probe/clean/band_clean_tu5.exe` | `5f3f667a` | 14,363,648 |

### Verification (clean_tu5.xex)

| Check | Expected | Got |
|---|---|---|
| ExecutionID version | 0.0.5.1 | **0.0.5.1** (raw 0x00000501), base_version 0.0.5.1 |
| Title ID | 45410914 | **45410914** |
| Entry point | 0x8283cd20 (TU5) | **0x8283CD20** |
| `rbdxcache` present | NO (clean, not DX) | **0 hits** (RB3DX = 1 hit) |
| File time | TU5 | Fri Sep 02 2011 (RB3DX identical; base = Aug 07 2010) |
| PE decode | sane PPC, section-mapped | entry = `mflr r12; bl __savegprlr; addi; stwu` ✓ |

The extracted PE is **14,363,648 B — identical size to RB3DX's `band_tu5.exe`** and
has an **identical PE section table** (same VA/vsize/rawptr/rawsize for all 12
sections), so VA↔offset mapping is shared and comparison is exact.

---

## 2. Divergence measurement — RB3DX (`band_tu5.exe`) vs clean TU5 (`band_clean_tu5.exe`)

### 2a. Whole-image (per section)

| Section | bytes | differing | % |
|---|---|---|---|
| .rdata | 2,036,224 | 15 | 0.00 |
| .pdata | 462,336 | 0 | 0 |
| BINKCONS | 10,752 | 0 | 0 |
| **.text** | **10,342,400** | **92** | **0.00** |
| BINK | 66,048 | 0 | 0 |
| **.data** | 360,960 | 63 | 0.02 |
| BINKDATA / .idata / .reloc / … | — | 0 | 0 |
| **TOTAL** | 14,362,624 | **170** | **0.001%** |

The 170 differing bytes cluster in ~20 short spans. Every one falls in an
**unnamed region** — none is inside any named function in the map. Example: at
`0x82575f9c` clean TU5 has `bne 0x82575fa8`, RB3DX has `nop` (a classic Deluxe
"defeat this check" hook). These are RB3DX's own DLC-cache/gameplay mods; they do
not perturb matched engine/game code.

### 2b. Same-instrument patch surface (per-function, from the TU5 retarget doc)

Every function compared over ≥64 bytes at the retarget doc's TU5 VA:

| Function | TU5 VA | clean vs RB3DX |
|---|---|---|
| OvershellPartSelectProvider::IsActive (Layer A) | 0x826684C0 | **IDENTICAL** |
| OvershellPanel::ResolvePartWaitStates (Layer B) | 0x825B6488 | **IDENTICAL** |
| PlayerTrackConfigList::ProcessConfig (Layer C) | 0x8276FA08 | **IDENTICAL** |
| TrackWatcherImpl::RecalcGemList (centre) | 0x82794740 | **IDENTICAL** |
| GameGemDB::Duplicate | 0x827932C8 | **IDENTICAL** |
| GameGemDB::GetDiffGemList | 0x827931C8 | **IDENTICAL** |
| GameGemList::CopyFrom | 0x8278E168 | **IDENTICAL** |
| BandUser::SetOvershellSlotState (Layer-B helper) | 0x8268BAF0 | **IDENTICAL** |
| OvershellPanel::UpdateAll (Layer-B helper) | 0x825B70D0 | **IDENTICAL** |
| BandUserMgr::GetBandUserFromSlot (Layer-B helper) | 0x82682B60 | **IDENTICAL** |
| MemFree (clone teardown) | 0x827BC430 | **IDENTICAL** |
| __savegprlr_14 (CRT block) | 0x82829220 | **IDENTICAL** |
| __restgprlr_14 (CRT block) | 0x82829270 | **IDENTICAL** |

- **Cave 0x82C8A000 (2800 B):** all-zero free run in **both** clean TU5 and RB3DX →
  cave is available and safe on clean TU5 exactly as on RB3DX.
- **PORT_THEBANDUSERMGR 0x82E023B8:** lies in `.data`'s BSS/zero-init region (not
  file-backed; a runtime global pointer). Same VA in both (identical section
  layout), consistent with the retarget doc.

### 2c. Broad sample → full map (aggregate same-VA %)

Rather than 30, the **entire** `base_to_tu5_map.json` was checked: for each mapped
function, the bytes at `tu5_va` (size-bounded) were compared between clean TU5 and
RB3DX.

| Metric | Value |
|---|---|
| Mapped functions with a TU5 VA (matched set) | 12,817 |
| File-backed & compared | 12,817 (0 unmapped/BSS) |
| **Byte-identical clean == RB3DX** | **12,817 (100.000%)** |
| Differing | 0 |

**Aggregate same-VA-same-body: 100.000%.** RB3DX and clean TU5 are the same binary
for every matched function. (This is stronger than "same VA" — it is same VA *and*
same bytes.)

---

## 3. Verdicts & recommendations

### One patch or two?
**ONE.** The 7 patch functions, all Layer-B helpers, the CRT thunk block, the cave
region, and the baked data pointer are byte-identical and same-VA on clean TU5 and
RB3DX. The RB3DX-targeted binary patch (`default_tu5_patched.xex` recipe / the
`same_instrument_full_tu5.patch.toml` + cave blob) lands correctly on clean TU5 with
**no address/offset changes**. A single build/patch serves both. (A memory-applied
`.patch.toml` keyed on title_id+version is even more clearly shared, since both are
title 45410914 / v0.0.5.1.)

Caveat unchanged from the retarget doc: the cave sits in `.data` (RW). Xenia with
`writable_code_segments` JITs it fine; a strict-NX/console target would still want an
executable cave — that is orthogonal to the RB3DX-vs-clean question (identical in both).

### Should rb3-xenon migrate to clean TU5, and is the map reusable?
**Yes, migrate to clean TU5, and the map is reusable as-is.**
- The base→"TU5" map already in the worktree was derived against RB3DX, but since
  RB3DX ≡ clean TU5 for 100.000% of matched functions, **`base_to_tu5_map.json` is
  already the base→clean-TU5 map.** Delta to rebuild ≈ **zero** — no re-derivation,
  no re-verification of function VAs needed. The only honest edit is relabeling the
  reference binary from RB3DX to clean TU5 and (optionally) re-pinning to
  `band_clean_tu5.exe`. The 478 "changed" (unmatched) and 81 "miss" entries are
  unaffected — they were never RB3DX-specific.

### Canonical decomp target
**Target clean retail TU5** (`band_clean_tu5.exe`, this run). Rationale:
- It is exactly what **RB3Enhanced targets** (v0.0.5.1, entry 0x8283CD20) and is
  the real retail-update binary the community aligns to.
- It is **DX-free** (no `rbdxcache`, none of the 170 Deluxe hook bytes), so the
  decomp is not contaminated by third-party mods that would masquerade as "target"
  bytes in ~20 spots.
- **Players run RB3DX**, but because RB3DX = clean TU5 + a 170-byte in-place patch
  that touches zero matched functions, decomping clean TU5 *is* decomping RB3DX for
  all practical function-matching purposes; the DX delta is trivially
  characterizable separately if ever needed.

---

## 4. Reproduce the measurement

```bash
cd rb3-xenon/.claude/worktrees/tu5-migrate     # has tools/tu5_va.py + the map
CLEAN=rb3-xenon/_tu5probe/clean/band_clean_tu5.exe
RB3DX=orig/45410914/band_tu5.exe
# section tables (identical), per-section byte diff, per-function table, and
# full-map transfer check were run with tools/tu5_va.load_sections/va_to_off.
```
Base tools untouched; decomp.db/config read-only; nothing committed.
