# W16-BK — re-home the `UIPanel` factory `0x82802418` from `UIColor.cpp:` to `UI.cpp:`, and the `?NewObject@UIScreen@@` name is on the WRONG ADDRESS

Lane W16-BK, 2026-09-15. Worktree `~/tmp/wt-w16-bk`, branch `w16-bk`, based on
main `312da843` (roadmap-only on top of `0416c2ec`).

**Headline.** The re-home landed as filed by W16-BI. The *unexpected* result is
item 3: BI's identification of `0x828023A0` as `?NewObject@UIScreen@@` is
**correct**, and therefore the map row that currently carries that name —
`0x823f5c20`, scoring **100.0 % on 72 B in `default/UI`** — is **WRONG**. It is
`?NewObject@NetSearchResult@@`. Its 100 % is a **false credit** that survives
only because its ctor callee is unnamed and `name_check` forgives placeholder
targets. I did not act on it (out of bar); it is filed with complete
retail-byte + RTTI evidence below.

---

## 0. Leg A, verified literally (not inherited)

The brief's baseline was **reproduced exactly**, in this worktree, after a
settle build. ⚠ The worktree as handed over was **NOT settled** — my first
`ninja` ran **409 edges**. Reading its `report.json` as leg A without building
would have been an unsettled read, the exact failure CLAUDE.md forbids.

| key | leg A (measured) | brief said |
|---|---:|---:|
| `matched_functions` | 43,552 | 43,552 ✓ |
| `matched_code` | 4,040,380 | 4,040,380 ✓ |
| `matched_code_percent` | 39.42962 | 39.4296 ✓ |
| `total_functions` | 69,240 | 69,240 ✓ |
| `total_code` | 10,247,068 | 10,247,068 ✓ |
| `masked_equal_functions` | 23,076 | 23,076 ✓ |
| `fuzzy_match_percent` | 49.699745 | — |

Provenance read from `report.json` (never inferred): objdiff **4.2.9**,
`tool_binary_hash 5a51cd51fe0a353f`, `tool_commit a5f0ea903ec1`, ruler
**`functionRelocDiffs=name_check`**. ⚠ `provenance.diff_config` is a **list of
`"k=v"` strings**, not a dict — a `.get('functionRelocDiffs')` on it raises.

Quiescence: builds 2 and 3 each ran **7** edges, all always-run checks
(`CHECK ICF-ALIAS MAP`, `CHECK SPLIT CURRENT`, `CHECK MAP NAME-INJECTIVITY`,
`CHECK TARGET OBJS RENAMED`, `PROGRESS`), and `git status config/45410914/` was
clean ⇒ `symbols.txt` is at a fixed point.

**Anti-vacuity for every COFF/name read in this document:**
`[renamed-check] 25837/29358 map names present in 3113 target objs = 88.0%`.
A reflinked pre-renamer tree reads ~0 %, so the pre-compile renamer has run and
a *negative* name lookup here is informative rather than structural.

**Baseline control.** My own leg A rowset set-diffs against the brief's
`~/tmp/rows_w16bh_main.json` (copied to `rows_w16bk_base.json`, original
untouched) at **CROSSED IN 0 / FELL OUT 0 / all four measures Δ0** — so the two
baselines are the same measurement and either may be used. 40,887 rows at
`fuzzy == 100`.

---

## 1. Block census on retail bytes — what actually lives in `0x82802080–0x828024B0`

Method: retail bytes only (`tools/retail_body.py` over
`orig/45410914/band.exe`), plus **RTTI Complete Object Locator** resolution for
class identity. Row identity was fixed by matching the `report.json` row order
to the `symbols.txt` address order **and the size sequence**
(40,124,84,48,252,32,100,120,184,32,72,40,100,40 — an exact 14/14 match), so no
row is attributed by guess.

The block's tail is **two `[8 B EH prefix][factory][40 B unwind funclet]`
groups**, a structure the brief did not know about:

| addr | size | fuzzy | what it is (retail-byte evidence) | correct home |
|---|---:|---:|---|---|
| `0x82801F80` | 40 | 100 | `?SetColor@UIColor@@` | UIColor |
| `0x82801FA8` | 124 | 100 | `?Copy@UIColor@@` | UIColor |
| `0x82802028` | 84 | 100 | `??0UIColor@@IAA@XZ` | UIColor |
| `0x82802080` | 48 | 100 | `?ClassName@UIColor@@` | UIColor |
| `0x828020B8` | 252 | 100 | `?SetType@UIColor@@` | UIColor |
| `0x828021B4` | 32 | 100 `me` | funclet | UIColor |
| `0x828021D8` | 100 | 100 | `?Save@UIColor@@` | UIColor |
| `0x82802240` | 120 | **0** | **`UIColor::Load`** — `bl ?ReadEndian@BinStream@@`, `bl ?Load@Object@Hmx@@`, `bl ??5@YAAAVBinStream@@AAV0@AAVColor@Hmx@@@Z` | UIColor (**stays**) |
| `0x828022C0` | 184 | 100 | `?SyncProperty@UIColor@@` | UIColor |
| `0x82802378` | 32 | 100 `me` | funclet | UIColor |
| `0x82802398` | 8 | — | `except_data` = EH prefix of `0x828023A0` | boundary |
| `0x828023A0` | 72 | **0** | **`?NewObject@UIScreen@@`** — `li r3,0x40` (=`sizeof(UIScreen)`), `bl 0x827bd2f0` (operator new), `bl 0x827f1e90` | **UI.cpp** |
| `0x828023E8` | 40 | 93.4 `me` | unwind funclet of `0x828023A0` (`lwz r3,80(r31)`; `bl 0x8240ddb0` = operator delete) | **UI.cpp** |
| `0x82802410` | 8 | — | `except_data` = EH prefix of `0x82802418` | **UI.cpp** |
| `0x82802418` | 100 | **0** | **`?NewObject@UIPanel@@`** — `li r3,0x68` (=104=`sizeof(UIPanel)`), operator new, `bl 0x82812920` = `??0UIPanel@@QAA@XZ` | **UI.cpp** |
| `0x8280247C` | 40 | 93.4 `me` | unwind funclet of `0x82802418` | **UI.cpp** |
| `0x828024A8` | 8 | — | `except_data` = EH prefix of `0x828024B0`, which is **already** `UI.cpp:`'s | **UI.cpp** |

Funclet attribution is not by adjacency alone: the factory frames are
`addi r31,r1,-112` + `stw r3,80(r31)` and each funclet is
`addi r31,r12,-112` + `lwz r3,80(r31)` + `bl` operator delete — the same frame,
reloading the same saved allocation to free it on a ctor throw.

### Why the factories belong to `UI.cpp` — three independent lines

1. **Source.** `src/system/ui/UI.cpp` is the **only** TU that registers them:
   line 963 `REGISTER_OBJ_FACTORY(UIScreen)`, **964 `REGISTER_OBJ_FACTORY(UIPanel)`**,
   968 `REGISTER_OBJ_FACTORY(UIColor)`. `X::NewObject` is a header-inline static,
   so it is emitted only in a TU that references it.
2. **Our objects.** `UI.obj` defines **10** `?NewObject@…` COMDATs, all
   `IMAGE_COMDAT_SELECT_ANY`, including `?NewObject@UIPanel@@SAPAVObject@Hmx@@XZ`
   and `?NewObject@UIScreen@@SAPAVObject@Hmx@@XZ`.
   **`UIColor.obj` defines 58 function symbols and NOT ONE `NewObject`.**
   ⇒ the 0 % on `0x82802418` has a single complete cause: *the unit it is pinned
   to cannot define the name*. No source defect is involved.
3. **Retail neighbourhood.** `default/UI`'s already-pinned `0x8280xxxx` blocks
   are UI.obj's COMDAT run, and they already score 100 % on
   `?NewObject@UIColor@@` (84 B), `PanelDir` (112), `Screenshot` (112),
   `UITrigger` (112), `UIGuide` (84), `UIFontImporter` (108). Note
   `?NewObject@UIColor@@` itself is in **`default/UI`**, not in `default/UIColor`
   — the strongest possible confirmation that a factory's home is `UI.cpp`.
   ⚠ And `0x828024B0`/`0x82802508`, the 88+32 B pair immediately after my new
   boundary, are **not** factories: `0x828024B0` forms the `.rdata` string
   `"UIPicture"`, calls `??0Symbol@@QAA@PBD@Z` and sets a guard bit at
   `0x82e07808` ⇒ it is a **dynamic initializer** (`??__E`-class) for a static
   class-name `Symbol`, and `0x82802508` is its matching atexit destructor. Same
   TU, different COMDAT kind.

### Boundary decision

**`0x828023A0`.** `UIColor.cpp:` keeps `0x82802080–0x828023A0`; `UI.cpp:`
receives a new adjacent block `0x828023A0–0x828024B0`.

* Both factory groups move, not just the UIPanel one, because both are UI.obj
  COMDATs on the evidence above. Leaving one behind would be knowingly wrong,
  and it makes the `0x828023A0` follow-up a **map-only** edit later.
* The cut is at a symbol start (`except_data_827DCEC0` ends exactly at
  `0x828023A0`), so dtk cannot "end within symbol".
* The EH prefix `0x82802398` stays with the **preceding** block, matching the
  file's existing convention — `0x828024A8` is today the last 8 bytes of
  `UIColor.cpp:`'s block and is the prefix of `UI.cpp:`'s `0x828024B0`. Prefixes
  are not function rows and are not in `total_code`, and the empirical proof that
  a prefix outside the block is harmless is that every 100 % factory row listed
  above already has its prefix in a neighbouring block.
* A **separate** `.text` line, not a merge with `0x828024B0–0x82802530`: the
  minimal edit, and `UI.cpp:` already uses adjacent separate blocks throughout.
* `UIColor.cpp:` is **not** drained (keeps `0x82801F80–0x8280207C` and
  `0x82802080–0x828023A0`), so the vanishing-unit hazard does not apply.

---

## 2. PRE-REGISTERED PREDICTION

*Committed before the splits edit was made — this section's commit precedes the
edit's commit on `w16-bk`, so it cannot have been fitted to the result.*

Four function rows + two `except_data` objects move `default/UIColor` →
`default/UI`.

| # | claim | predicted |
|---|---|---|
| 1 | `?NewObject@UIPanel@@SAPAVObject@Hmx@@XZ` (100 B) | 0 → **100** ⇒ **+1 fn / +100 B** |
| 2 | `fn_828023A0` (72 B) | 0 → **0**, Δ0 |
| 3 | `fn_828023E8`, `fn_8280247C` (40 B, 93.4, `masked_equal`) | central **Δ0**; band ±40–80 B |
| 4 | collateral funclet re-pairing in UI.obj's pool | not predicted; down to −40 B possible |
| 5 | `total_functions` / `total_code` | **UNCHANGED** 69,240 / 10,247,068 |
| 6 | `masked_equal_functions` | 23,076 ± a few |
| 7 | `default/UIColor` unit | 14 fns/9 matched/1,268 B → **10 fns/9 matched/1,016 B** |
| 8 | `default/UI` unit | 200/133/23,612 → **204/134/23,864 B** |
| 9 | whole binary | 43,552 → **43,553**; 4,040,380 → **4,040,480 B**; 39.42962 → **≈39.43060 %** (+0.000976 pp) |

Basis for (1): `UI.obj` defines the name (verified above); W16-BI measured our
COMDAT **RAW-identical at 23 of 25 words**, the two differing words being the
only reloc-bearing ones, with operator new alias-forgiven (group 1546) and
`??0UIPanel@@QAA@XZ` exactly map-named at `0x82812920`.

Basis for (2): the name it deserves is already held by `0x823f5c20`, and
**two map rows may never carry one name**, so it stays a forgiven placeholder
wherever it is pinned. Re-homing it buys **pairability for later**, not bytes
now.

Basis for (3): our funclets are anonymous `__unwind$<ordinal>` (208 of them in
UI.obj) and can pair **only by byte signature**, which is why these read 93.4 %
`masked_equal` today. At 93.4 they contribute **0 functions and 0 bytes**, so
the downside is structurally bounded at zero and any crossing is unbanked
upside. The ±40 B band is the mechanism CLAUDE.md records for `b341d7ab` (a
40 B EH funclet re-paired to a different byte-equal counterpart in a larger
merged pool).

Basis for (5): a re-home creates and destroys no function row and changes no
size. **If `total_code` or `total_functions` moves, my model is wrong** and the
`.pdata` re-derivation or a phantom `type:label` row is the cause — I must
explain it rather than smooth it.
