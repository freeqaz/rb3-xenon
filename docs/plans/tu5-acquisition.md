# TU5 acquisition + validation (Lane A) — 2026-07-07

Read-only investigation. No decomp config / decomp.db / Ghidra program mutated.
Scratch + machine checkpoint under `_tu5probe/` (`lane_a_findings.json`).

## Verdict

**`/srv/torrents/games/arbys/rb3/default.xex` is CLEAN RETAIL Rock Band 3 Title
Update 5 (v0.0.5.1), directly usable by the decomp toolchain — no fallback
needed.** It is uncompressed + unencrypted, dtk ingests it, and it decodes as
sane PPC. Use it as the canonical TU5 target.

- **Canonical clean-TU5 path:** `/srv/torrents/games/arbys/rb3/default.xex`
- sha1 `c5a17091cb44c0119424390a1738d161995e430e`
- sha256 `6639ce25745505b598480499ca53b421fdec5604d813f5ee2c8152ecdad2a5ea`
- size `13,971,456 B`

## 1. Identity + cleanliness — evidence

### Header identity (dtk `xex info` + raw XEX2 parse)
| field | BASE (decomp target, TU0) | TU5 candidate |
|---|---|---|
| version / base_version | **0.0.0.1** | **0.0.5.1** |
| title_id | 0x45410914 | 0x45410914 |
| media_id | 0x4fc9256f | 0x4fc9256f |
| load address | 0x82000000 | 0x82000000 |
| pe_data_offset | 0x3000 | 0x3000 |
| entry point | 0x82816080 | 0x8283cd20 |
| PE name | band.exe | band.exe |
| dtk classification | Retail / Uncompressed / Unencrypted | Retail / Uncompressed / Unencrypted |
| **build file-time** | Sat **Aug 07 2010** (disc) | Fri **Sep 02 2011** (TU5) |
| sections (count/names/order) | 12 | **12, identical names + order** |
| import libraries | `xam.xex`, `xboxkrnl.exe` | **identical** |
| optional-header keys | 18 | **identical set (18)** |
| static libraries (dtk) | 15 libs (LIBCMT/XAPILIB/XBOXKRNL/XNET/…/XAUDIO2) | **identical set + versions** |

The **Sep 02 2011** basefile timestamp is genuine TU5 provenance (RB3 TU5 shipped
in 2011); base is the Aug 2010 disc build.

### Cleanliness (not RB3DX / not Enhanced / not lightly modded)
- **0 injected sections** — 12 sections, identical NAMES and order to base.
- **0 extra import libs / 0 extra optional headers** — a hooked/modded XEX
  (RB3DX, RB3Enhanced) injects import-table entries or optional headers; none here.
- **Static-library set + versions identical to base** — a clean recompile, not a
  patched/hooked binary.
- String scan (`strings`) for `RB3DX / RB3Enhanced / RBEnhanced / rb3.ini / RB3E /
  Xenia / AllowSameInstrument / .dll / Enhanced` → **0 hits each**.
- `forge` (2) and `custom` (46) and `deluxe` (2) hits are **legit retail asset
  paths** (`beforeiforget.mid`, `ui/customize/…`, `songs/updates/hillbillydeluxe`);
  base has the same set (forge 2 / custom 45 / deluxe 2). The +1 `custom` in TU5 is
  TU-added content, not a mod.
- `songs/updates/*_update.mid` paths are exactly the DLC/song-fix MIDs a Title
  Update bundles.

### Disc-zip provenance
`/srv/torrents/games/arbys/Rock Band 3 (RF) (45410914).zip` carries
`default.xex` = **13,807,616 B, encrypted vanilla base v0.0.0.1**, sha1
`d57b7df52ef41192d258356dfe6a2243f42ea058` — **byte-identical to
`/srv/.../rb3/default_vanilla.xex`**. So the disc zip is the ENCRYPTED BASE, NOT
TU5. Provenance chain:

```
disc base (enc, v0.0.0.1)  --official MS TU5 xexp-->  TU5 (v0.0.5.1)  --decrypt-->  /srv/.../rb3/default.xex   (canonical clean TU5)
```

**Conclusion: `/srv/.../rb3/default.xex` is clean retail TU5.** Not modded.

## 2. Direct usability + PPC sanity

- dtk (`jeff`) reads it: `xex info` → Retail/Uncompressed/Unencrypted; entry
  0x8283CD20; PE name band.exe.
- `dtk xex extract` → valid `MZ`/`PE` image, **12 sections, 14,363,648 B**,
  image_base 0x82000000. (base band.exe = 14,137,856 B; TU5 is larger, consistent
  with a bug-fix TU that added code.)
- Spot-checked VAs decode as sane PPC on the dtk-extracted PE: entry `0x8283cd20`
  = `mflr r12` (7d8802a6, a real prologue); `0x82272e88` = `bl 0x82270e68`;
  `.text` start `0x82270000` = `rlwinm/lis/stw/blr` then a fresh `mflr` prologue.

No fallback (decrypt `default_vanilla.xex` + apply official xexp) is required. It
remains the documented backup if this file were ever lost: same retail key that
produced base `band.exe`, apply MS TU5 xexp, no XDK — but **unnecessary now**.

## 3. Ingestion readiness (dtk / Ghidra)

Base VA math transfers unchanged: **same `image_base 0x82000000` and
`pe_off 0x3000`** as the current base target. dtk decompresses the XEX natively
(`config.yml` `object:` points straight at the `.xex`).

Ingestion steps (on a worktree, TU0 frozen — do not touch the running base-patch test):
```
cp /srv/torrents/games/arbys/rb3/default.xex  orig/45410914/default.xex   # worktree only
jeff/dtk xex extract orig/45410914/default.xex                            # -> band.exe (helper-tool PE)
# then re-anchor config/45410914/{splits.txt,symbols.txt} to TU5 VAs (Lane 3 base->TU5 map)
touch config/45410914/config.yml && python3 configure.py && tools/ninja-locked
```

## 4. CRITICAL mapping correction (changes how TU5 VAs must be read)

**The `/srv` TU5 xex is a NORMAL basic-compressed retail XEX** (raw section blocks
concatenated WITHOUT the loaded-image zero gaps). **rb3-xenon's BASE xex is a
pre-FLATTENED image** (zero gaps materialized). Consequence:

- On **BASE**, `flat_off = 0x3000 + (VA - 0x82000000)` == the true dtk PE section
  offset. That's why `tools/xex_binpatch.py` and `_tu5probe/word_at.py` flat math
  work on the base and why the running same-instrument patch is correct.
- On **TU5**, flat mapping **DRIFTS and returns wrong bytes**. Proof (measured):

| VA | dtk PE (CORRECT) | flat 0x3000+VA (WRONG) |
|---|---|---|
| TU5 entry `0x8283cd20` | `7d8802a6` mflr | `4bfe4588` b |
| TU5 `0x82272e88` | `4bffdfe1` bl | `485458a1` bl (coincidentally also a bl) |
| BASE `0x82816080`,`0x82272e88` | — | **flat AGREES with dtk PE** |

**Implications for downstream lanes:**
1. `_tu5probe/FINDINGS.md`'s flat-mapped TU5 words (e.g. `485458a1`) are
   **unreliable** — always read TU5 via `dtk xex extract`/`xex split`/`xex disasm`
   (PE section mapping), never flat `0x3000+VA`.
2. The **base = TU0** conclusion still holds and is actually cleaner under the
   correct mapping: RB3Enhanced's TU5 port site `0x82272e88` is a **real `bl`
   (4bffdfe1)** on dtk-correct TU5 vs `stw (907f0000)` on base.
3. **Patch re-target gotcha:** `tools/xex_binpatch.py` assumes a flat image. To
   patch TU5 in place with the existing flat-offset math, first produce a
   **flattened TU5 image** (base-analogue), or rework the patcher to section-map.
   The code cave and all detour/call VAs must be re-derived from the dtk-extracted
   TU5 PE, not from flat reads.

## Scratch artifacts (`_tu5probe/`)
- `lane_a_findings.json` — machine-readable checkpoint of everything above.
- `probe2.py` — XEX2 header/import/security-digest parser.
- `decode_flat.py` / `decode.py` — capstone PPC decode (flat vs section).
- (large `*.xex` / `*_band.exe` scratch removed after verification to save space.)
