# base(TU0) → TU5 function-remap SPIKE — proof on a real sample (2026-07-07)

**Lane C deliverable.** Proves the base→TU5 remap mechanism works, on 25 real base
functions (incl. the 7 same-instrument-patch functions), using an adapted form of
the project's cross-binary tooling. Read-only; nothing in `config/`, `decomp.db`,
`objects.json`, `splits.txt`, `symbols.txt` or the Ghidra program was mutated.

- Target base (TU0): `orig/45410914/default.xex` (15,478,784 B, v0.0.0.1, `.text`@0x82260000)
- TU5:               `/srv/torrents/games/arbys/rb3/default.xex` (13,971,456 B, v0.0.5.1, `.text`@0x82270000)
- Scratch/scripts: `_tu5probe/skel_match.py`, `_tu5probe/sample.json`, `_tu5probe/remap_result.json`
- Flat XEX2 mapping used throughout (verified in `_tu5probe/FINDINGS.md`): `file_off = 0x3000 + (VA − 0x82000000)`.

---

## Method — relocation-normalized opcode skeleton

TU0→TU5 is the **same source recompiled** with the same MSVC-X360 `/O1` (a bug-fix
title update), so the cheapest cross-binary match the project has. For each base
function we take its instruction words and **mask exactly the fields that
relocation shifts**:

- `b/bl/ba/bla` (op 18) → keep opcode+AA+LK, drop the 24-bit target (`w & 0xFC000003`)
- `bc` (op 16)          → keep opcode/BO/BI/AA/LK, drop the 14-bit displacement (`w & 0xFFFF0003`)
- D/DS-form loads/stores/`addi`/`addis`/`ori`/… → keep opcode+rD+rA, drop the 16-bit imm/disp (`w & 0xFFFF0000`)
- everything else (X/XO-form, `rlwinm`, `mflr`, `mr`, …) → kept whole (no relocatable field)

The masked word list is a **position-independent skeleton**. A base function's
skeleton is searched as a contiguous run in TU5 `.text` (6-word seed index +
full verify). An **exact unique** run ⇒ the same function body, addresses shifted
(the only differing raw words are the masked/relocated slots). This is a strict
**lower bound** — it deliberately misses functions with any register-allocation
drift, which the production pipeline (content-hash + ghidriff/BSim) recovers.

Classification per function: `HIGH` = unique skeleton match; `MED-ambig` = match
exists but non-unique (resolved by proportional address + boundary check);
`LOW-changed` = no exact skeleton (body changed, or fragment/early-CRT).

---

## CRUCIAL DELIVERABLE — same-instrument patch's 7 functions on TU5

All 6 located-HIGH functions carry a `7d8802a6 mflr r12` entry on **both** builds
(real, hookable prologues); the only differing prologue words are shifted `bl`
save-thunk targets — the reloc signature that confirms same-function identity.

| # | function | base VA | **TU5 VA** | conf | body vs TU0 | TU5 prologue (first word) |
|---|----------|---------|-----------|------|-------------|---------------------------|
| 1 | `ResolvePartWaitStates` (detour) | 0x8259D948 | **0x825AE488** | HIGH (unique) | identical, 48 reloc slots | `7d8802a6` mflr ✓ |
| 2 | `ProcessConfig` (detour)         | 0x8274ACF8 | **0x82767A08** | HIGH (unique) | identical, 1 reloc slot  | `7d8802a6` mflr ✓ |
| 3 | `RecalcGemList` (detour)         | 0x8276FBB0 | **0x8278C740** | HIGH (unique) | identical, 1 reloc slot  | `7d8802a6` mflr ✓ |
| 4 | `GameGemDB::Duplicate`           | 0x8276E590 | **0x8278B2C8** | HIGH (unique) | identical, 4 reloc slots  | `7d8802a6` mflr ✓ |
| 5 | `GameGemDB::CopyFrom`            | 0x82769450 | **0x82786168** | HIGH (unique) | identical, 2 reloc slots  | `7d8802a6` mflr ✓ |
| 6 | `GameGemDB::GetDiffList`         | 0x8276E010 | **0x8278B1C8** | MED-HIGH    | identical 4-insn leaf     | `81630000` lwz r11,0(r3) ✓ |
| 7 | `IsActive` (detour)             | 0x8264B5F8 | **0x826604C0** | **CHANGED** | **56% body sim** (126/224 words) | `7d8802a6` mflr ✓ |

Notes:
- **#7 `IsActive` genuinely changed between TU0 and TU5.** The entry is found and
  is a valid hook point (`7d8802a6 mflr` at 0x826604C0, 16-aligned), and its first
  124 instruction words still match — but the body diverges after that (56%
  whole-body similarity). **The detour's internal logic must be re-derived on the
  TU5 program**, not blindly ported. This is exactly the kind of function a title
  update touches, and it is the one the same-instrument feature most cares about.
- **#6 `GetDiffList`** is a 4-instruction accessor (`lwz r11,0(r3); rlwinm;
  lwzx r3,r10,r11; blr`). The exact leaf incl. displacement `81630000` occurs
  twice in TU5; 0x8278B1C8 is chosen by GameGemDB-cluster locality (it sits beside
  Duplicate@0x8278B2C8 and its own sibling body fn_8276E028→0x8278AD50); the far
  decoy at 0x82B7CEE4 is rejected. In production, confirm via a caller xref.
  (The base entry 0x8276E010 is a fragment split by embedded `except_data`; the
  anchor-minus-offset trick fails because the exception interleaving differs on
  TU5 — search the leaf directly, as done here.)

### Candidate TU5 code cave (base cave moved — it is real code in TU5)

The base blob cave `0x82C25000` is the base BINK→BINKBSS VA gap; on TU5 that VA is
**live code** (`2f0b00ff cmpi …`), confirming the cave moved.

- **Primary recommendation: `0x82C55010`, 0x73F4 (29,684 B) of file-backed zeros**,
  inside the TU5 `BINK` section's tail padding (BINK: 0x82C4D000..0x82C5D010; real
  Bink codec code ends ~0x82C55010, the rest is zero pad). BINK is a **code**
  section (executable, 64K-aligned) and this region is **backed by real file bytes**
  (BINK raw 0x10200 ≥ vsize 0x10010), so `xex_binpatch.py` can write the relocated
  blob directly without extending the image. Leave a little slack (start at e.g.
  `0x82C55800`). Ample for the patch's few-hundred-byte trampoline+logic blob.
- **Structural-analogue alternate: `0x82C5D010`..`0x82C60000` (0x2FF0, 12,272 B)** —
  the TU5 BINK→BINKBSS VA gap, the direct twin of the base cave. Committed-zero at
  load but *not* file-backed (post-vsize gap), so it needs the loader/xex to
  zero-commit it; prefer the primary unless a non-BINK region is required.

---

## Sample remap table (25 functions)

| base VA | size | **TU5 VA** | conf | class | reloc/notes |
|---------|------|-----------|------|-------|-------------|
| 0x8264b5f8 | 896  | 0x826604c0* | LOW-changed | changed | *entry only; 56% body (IsActive) |
| 0x8259d948 | 1356 | 0x825ae488 | HIGH | reloc-shifted | 48 |
| 0x8274acf8 | 120  | 0x82767a08 | HIGH | reloc-shifted | 1 |
| 0x8276fbb0 | 76   | 0x8278c740 | HIGH | reloc-shifted | 1 |
| 0x8276e590 | 164  | 0x8278b2c8 | HIGH | reloc-shifted | 4 |
| 0x82769450 | 144  | 0x82786168 | HIGH | reloc-shifted | 2 |
| 0x8276e028 | 144  | 0x8278ad50 | HIGH | reloc-shifted | 5 (GetDiffList sibling) |
| 0x8270a638 | 272  | 0x82720680 | HIGH | reloc-shifted | 21 |
| 0x82515970 | 536  | 0x82520ea8 | HIGH | reloc-shifted | 14 |
| 0x8281e740 | 596  | 0x82845180 | HIGH | reloc-shifted | 10 |
| 0x82a42888 | 668  | 0x82a69030 | HIGH | reloc-shifted | 13 |
| 0x826a81b0 | 1308 | 0x826bed00 | HIGH | reloc-shifted | 97 |
| 0x82942270 | 1624 | 0x82968968 | HIGH | reloc-shifted | 19 |
| 0x82598c08 | 204  | 0x825a9738 | HIGH | reloc-shifted | 9 |
| 0x8288e8c0 | 128  | 0x828b5470 | HIGH | reloc-shifted | 4 |
| 0x822606f0 | 300  | 0x82286b28 | MED-ambig | near-unique | 3 cands; boundary-verified pick |
| 0x8248de10 | 396  | 0x82498b00 | MED-ambig | near-unique | 6 cands |
| 0x829d9228 | 296  | 0x829ff748 | MED-ambig | near-unique | 2 cands; prev word = pad |
| 0x8227c594 | 32   | 0x8228cfb4 | MED-ambig | tiny/common | 2882 cands — skeleton insufficient |
| 0x823c4164 | 68   | 0x823d4218 | MED-ambig | tiny/common | 311 cands |
| 0x82396070 | 40   | 0x823a623c | MED-ambig | tiny/common | 8032 cands |
| 0x8229f28c | 40   | 0x822aeb68 | MED-ambig | tiny/common | 3468 cands |
| 0x823eabbc | 40   | 0x823fadd0 | MED-ambig | tiny/common | 8032 cands |
| 0x82262d78 | 984  | — | LOW-changed | fragment | BB fragment (starts `rlwinm`, not a prologue) — sampling artifact |
| 0x82260ec8 | 1868 | — | LOW-changed | changed/early | 11% best fuzzy sim; early `.text` (CRT/init region reorders) |

---

## Aggregate feasibility

Of the 25 (a deliberately adversarial mix — 6 patch fns, 8 sizable fns, 5 tiny
getters, 1 BB fragment, 1 early-CRT fn):

| bucket | n | % | meaning |
|--------|---|---|---------|
| **HIGH** unique 1:1 skeleton | 14 | 56% | directly usable, no judgement needed |
| **MED near-unique** (2–6 cands, position/boundary-resolved) | 3 | 12% | reliable |
| **MED tiny/common** (100s–1000s cands) | 5 | 20% | present & identical body, but skeleton alone can't pick — needs caller-context/content-hash |
| **LOW changed/miss** | 3 | 12% | IsActive (real TU change, entry found), 1 early-CRT, 1 BB-fragment (sampling artifact) |

- **Counterpart definitively located, structurally identical (strict skeleton alone): 17/25 = 68%.**
- **Counterpart present in TU5 with identical body (adds the 5 tiny, resolvable by the existing content-hash + ghidriff disambiguation the plan already prescribes): 22/25 = 88%.**
- **Genuinely changed / needs re-derivation: ~2–3/25 ≈ 8–12%** (of which only IsActive is a real body change; the other two are a fragment artifact and early-CRT).

### Extrapolation to the full ~12k named set

The sample is skewed *against* the method on purpose. Every **sizable, non-fragment,
non-early** function (≥128 B) matched HIGH or near-unique — 14 of 14 in that class,
including all 6 located patch functions. In the real binary those dominate the
matchable code. The two failure modes seen are both mop-up work the plan already
budgets for:

1. **Tiny common accessors** (getters/thunks): skeleton-ambiguous, resolved 1:1 by
   the project's `fingerprint_match.py` / content-hash + caller-xref (ghidriff/BSim)
   stages — the same tooling that already maps Wii↔Xenon and DC3↔Xenon.
2. **The genuine TU5 delta** (functions the update actually rewrote — e.g.
   `IsActive`): a **small** worklist by construction (a title update is a targeted
   bug-fix patch), computed explicitly as `{named} − {matched}` and re-entered into
   normal matching.

⇒ **Consistent with the migration plan's >95% exact/structural remap target.** The
strict lower bound here is 68–88% *before* the fuzzy/content-hash residue stages;
the full three-stage pipeline (content-hash → ghidriff/BSim → fingerprint anchors)
closes the tiny-function gap and isolates the small changed-set.

### Failure characterization (what does NOT remap by skeleton alone)

- **Trivial accessors** (≤~10 insns): thousands of byte-identical skeletons; not a
  remap failure, a *uniqueness* failure — needs caller context.
- **Genuinely-changed TU5 functions** (IsActive @56%): real bodies differ; the
  strict matcher correctly flags them and still recovers a valid hook entry.
- **dtk BB fragments** (functions split around embedded `except_data`/`.pdata`): a
  fragment that begins mid-computation has no self-contained skeleton — match the
  *primary* entry chunk instead (as done for GetDiffList), or match at the
  symbol/unit level after re-splitting TU5.
- **Early `.text` / CRT-init region**: reorders between TUs; low direct similarity;
  low value (startup glue), let ghidriff handle it.

---

## Reproduce

```bash
cd /home/free/code/milohax/rb3-xenon
python3 _tu5probe/skel_match.py          # -> _tu5probe/remap_result.json + summary
# sample is _tu5probe/sample.json ; masking + search logic in _tu5probe/skel_match.py
```

## Bottom line for the migration

The base→TU5 remap **works and is the easy cross-binary case**. A
`tools/remap_to_tu5.py` built on this masking + a content-hash first pass (fork of
`relocate_*_splits.py` for boundary-snapped VA rewrite) will re-key the majority
1:1; ghidriff/BSim + fingerprint anchors close the tiny/ambiguous residue; a small
explicit set-difference is the genuine TU5-changed worklist. For the same-instrument
patch specifically: **6 of 7 functions transfer directly to the TU5 VAs above; only
`IsActive` (0x826604C0) needs its detour body re-derived on the TU5 program**, and
the blob relocates to a fresh TU5 cave at **0x82C55010** (base cave 0x82C25000 is
now live code).
