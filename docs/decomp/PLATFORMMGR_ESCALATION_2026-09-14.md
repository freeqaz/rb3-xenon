# PlatformMgr / Server — Fable escalation audit of W16-L's Opus stop verdicts (lane W16-M, 2026-09-14)

Branch `w16-m` on main `073152eb`, worktree `~/tmp/wt-w16-m`. Ruler: `name_check` (read from
`report.json` provenance). Every step priced on a full `./tools/ninja-locked` build (logs
`~/tmp/rb3_build_w16m_*.log`, each ending `BUILD_RC=0`) and a whole-binary set-diff of the
`fuzzy == 100` row set (`tools/rowset_snapshot.py` against `~/tmp/w16m_rowset_baseline.json`).
Under audit: `docs/decomp/PLATFORMMGR_BODIES_2026-09-14.md` §2 (Poll "uncollectable"), §3 (Server
tail "NOT PROVABLE") and its "Not done" list. Standing rule applied: an Opus "can't be fixed" is a
claim to test on retail bytes, not a result.

## 1. Per-claim verdicts

| # | Opus claim (W16-L) | Finding on retail bytes | Verdict | bytes |
|---|---|---|---|---:|
| 1a | `??2Friend@@SAPAXI@Z` vs map's `??2CriticalSection@@SAPAXI@Z` @0x827bd2f0 is an **unproven fold** ⇒ uncollectable | `tools/alloc_fold_gate.py --json` recomputes the verdict from our compiled COMDAT bytes: **ADMIT** — same 8-byte `li r4,…; b MemAlloc` thunk, same relocation target. Installed into `scripts/symbol_aliases.json` group 1542 (`operator_new_alloc_thunk`, survivor `??2CriticalSection@@SAPAXI@Z`) with the gate evidence string. | **REFUTED** | 0 (row still open on 1c) |
| 1b | `push_back<Friend*>` vs map's `push_back<ChatReceiver*>` @0x82b5f808 is an unproven fold | Retail Poll's `bl` displacement at the site resolves to **0x82b5f808** exactly (read from the bytes, not guessed); our `vector<Friend*>::push_back` COMDAT is relocation-identical to the survivor including callee names (T1, chased through `_M_insert_overflow`). There is no separate unnamed `push_back<Friend*>` in retail — it folded. Alias installed (T1 evidence recorded). | **REFUTED** | 0 |
| 1c | "regalloc construct" — retail hoists `mFriendsBuffer + 8` as the induction base; cap 6 source variants, permuter OFF | Six variants built and measured (table §3). Half of Opus's reading is wrong: the XONLINE_FRIEND member offsets are **right** (checked against the xdk header; -0x8/0x0/0x10 are xuid/gamertag/flags relative to a `szGamertag` cursor), and the cursor **is** reproducible from source (V5/V6 pin the IV at buffer+8 and close 4 of the 6 sites). Two residual sites survive every variant: retail loads `mFriendsBuffer` **before** the `mFriendsEnum` guard and keeps it live across it; every source spelling that forces that placement makes MSVC keep a second IV family (V1: −14 fns / −440 B, 11 EH-funclet rows fell out). Poll 99.245 → **99.544 fuzzy / 99.566 mpn**, not crossed. | **SURVIVES in part** — offset half REFUTED, placement half stands | 0 of 1,844 |
| 2 | `Server` surplus tail virtuals: NOT PROVABLE, structural (reason 1: slots [9..18] all fold to `li r3,0; blr`) | Reproduced from `.rdata`, RTTI-confirmed (COL 3 words before, TD `.?AVServer@@` / `.?AVXboxServer@@`): `??_7Server` @0x820577bc = **19 slots**, [9..18] all = 0x823591e8 (`li r3,0; blr` null hub); `??_7XboxServer` @0x8205793c = 19 slots, overrides [11]=`lwz r3,0x80(r3); blr`, [13]=`lwz r3,0x84(r3); blr`, [14]=`lwz r3,0x88(r3); blr`, [15]=`lwz r3,0x7c(r3); b fn_82A89FF8`. Our compiled primary `??_7Server@@6B0@@` = **21 slots**, [0..8] structurally identical to retail (purecall ×3, `blr` folds, null hub at [7]). `fn_82A89FF8` = `/Od` Quazal accessor `p=fn_82A89F38(x); return p ? p->0x1c : 0`, itself over `p=fn_82A89EF8(x); return p ? p->0x8 : 0` — a three-deep unnamed holder chain with no DC3 map name (DC3's map carries zero Quazal names). Narrowed, not identified ⇒ which two of {SecureConnection, AccountManagement, MasterProfileID, CreateProfile, DeleteProfile, CustomAuthData} are surplus stays unprovable. **Nothing removed from `Server.h`.** | **SURVIVES** | 0 (vtable unscored) |
| 3A | `??0PlatformMgr@@QAA@XZ` 364 B @68.8 (secondary) | Retail's store set is 13 members + 7 statics, `new JobMgr` through the 0x827bd2f0 thunk (`??2JobMgr` ADMIT-ed into group 1542), and the tail is the `/Oi` **memset intrinsic** (four `std` through one `lis/addi`), not a scalar initializer list. | CROSSED | **+364** |
| 3C | `?UpdateSigninState@PlatformMgr@@QAAXXZ` 340 B @76.0 (secondary) | Retail block-copies the old cache with the `/Oi` **memcpy intrinsic** (four `ld` into r8/r7/r6/r9, then four `std`), uses `subic/subfe/and` = `if (call()!=0) x=0`, and zeroes `mSigninSameGuest, mSigninChangeMask, mSigninMask` in **that** source order (cl 10224 emits adjacent same-value stores in source order — DC3's spelling is reversed). | CROSSED | **+340** |
| 3B | `??0ProfileSwappedMsg@@QAA@PAVLocalUser@@0@Z` 164 B @0 | Inline in `Profile.h` (DC3 and rb3-Wii identical); the COMDAT is only emitted by a **constructing** TU, and retail's only constructor is the unnamed `fn_8251D6C8` (404 B @0), which needs two unidentified externs (`fn_82B54288`, `fn_82525DE0`) plus native shims. Not attempted. | NOT DONE | 0 |
| 3D | `fn_8251D378` = `PlatformMgr::Init` 220 B @0 — name it only with both callees resolved | Callees resolved: `fn_8283D810` = XNotifyCreateListener wrapper → `mListener`; `fn_82531980` = `WinSockSocket::Init`; `UpdateSigninState`; `ThreadCall`. **Unresolved**: `fn_8283EAE0(0,0x8000,0x8000)`, `fn_8283EB28`, `fn_82A6AC18`→`fn_82A6AB90` (XOnlineStartup-consistent, unproven). Per the brief's rule ("an unproven name is charged, a placeholder is forgiven") the map was **not** edited. | NOT DONE (deliberately) | 0 |

## 2. Whole-binary pre → post (`build/45410914/report.json`, name_check)

| measure | baseline (main 073152eb, settled) | HEAD 30a92733 | Δ |
|---|---:|---:|---:|
| matched_functions | 43,200 | 43,202 | **+2** |
| matched_code (B) | 3,936,636 | 3,937,340 | **+704** |
| matched_code_percent | 38.421364 | 38.428234 | +0.006870 |
| fuzzy_match_percent | 49.32626 | 49.32808 | +0.00182 |
| total_functions / total_code | 69,217 / 10,245,956 | 69,217 / 10,245,956 | 0 / 0 |

Set-diff: CROSSED IN 2 rows / 704 B (`??0PlatformMgr@@QAA@XZ` 364, `?UpdateSigninState@PlatformMgr@@QAAXXZ` 340); FELL OUT 0.

## 3. Per-step predicted vs measured

| step | commit | predicted (pre-registered) | measured |
|---|---|---|---|
| step 1: aliases 1a+1b | cff2030e | Poll stays <100 (1c open); whole-binary Δ0 | Poll 99.245 → 99.267; Δ0 fns / 0 B ✔ |
| V1 gamertag `char*` cursor | f4ee4c53 → reverted 410fd1c5 | Poll closes the addi pair; Δ0 if IV re-biases to +0 | **Poll WORSE 99.267 → 98.870; −14 fns / −440 B** (11×40 B EH-funclet rows fell out) — prediction FAILED, reverted |
| V2 indexed loop, no pointer IV | aef0c83b | +1 fn / +1,844 B (all 10 sites close) | 99.267 → 99.525; Δ0 — prediction FAILED (partial) |
| V3 hoist gamertag above flag test | 0a83d62f | Δ0, same 6 sites | 99.525 identical; Δ0 ✔ (INERT) |
| V4 multi-use indexed gamertag temp | 72638d30 → reverted 570906e2 | closes the base site | 99.525 → 99.267 (back to V1-shape IV at +0); Δ0 — FAILED, reverted |
| V5 single `const char *gamertag` cursor | 8afdd978 | IV pinned at buffer+8, ≥4 sites close | 99.267 → 99.512 fuzzy / 99.566 mpn; Δ0 ✔ |
| V6 source guard + do/while (cap 6/6) | 98741585 | addi pair closes, pre-guard load stays | 99.512 → 99.544 fuzzy / 99.566 mpn; Δ0 ✔ (2 residual sites, as predicted) |
| S3A ctor store set + `??2JobMgr` alias | cf518bc8 | +364 B if it crosses | 68.835 → 96.319; Δ0 — did not cross (3 `mr rN,r29`) |
| S3C UpdateSigninState body | ac461235 | +340 B if it crosses | 75.988 → 88.871; Δ0 — FAILED |
| S3A2 ctor tail = `memset` intrinsic | ac461235 | +364 B | **+1 fn / +364 B ✔ exact** |
| S3C2 oldCache = `memcpy` intrinsic | ac461235 | +340 B | 88.871 → 97.529; Δ0 — FAILED |
| S3C3 zero-store source order | 30a92733 | +340 B | **+1 fn / +340 B ✔ exact** |

Two lessons that generalise: (i) the `/Oi` intrinsic spellings (`memset`/`memcpy`) are the lever where cl 10224 codegen differs from DC3's cl 11886 spelling of the same body; (ii) cl 10224 emits adjacent same-value member stores in **source order**, so a DC3 body can be semantically identical and still read <100.

## 4. Commits on `w16-m` (oldest first)

`5aed50c3` tools(alloc_fold_gate) null-address tolerance · `cff2030e` aliases 1a+1b · `85ee6ad0` alias re-serialize (no content change) · `f4ee4c53`/`410fd1c5` V1 + revert · `aef0c83b` V2 · `0a83d62f` V3 · `72638d30`/`570906e2` V4 + revert · `8afdd978` V5 · `98741585` V6 · `cf518bc8` S3A · `ac461235` S3C+S3A2+S3C2 · `30a92733` S3C3 · (this doc).

## 5. NOT done, and why

- **Poll not crossed** (1,844 B): 2 residual sites (pre-guard `mFriendsBuffer` load + its register consequence); cap of 6 variants exhausted; permuter OFF by directive.
- **No `Server.h` virtual removed**: the removal is not proven (§1 row 2). Header layout comment in `Server.h` (W16-G) left as is.
- **`??0ProfileSwappedMsg` not written** (3B) and **`fn_8251D378` not named** (3D) — reasons in §1.
- **`PlatformMgr::SetDiskError`** (other unit, `default/PlatformMgr`): retail 0x82516320 is a bare `blr` (prefix `82829530 82087b70`, body `4e800020`); our `src/system/os/PlatformMgr.cpp:155` body is non-empty and `PlatformMgr.h:125`'s "retail SetDiskError `stw r4,0x34(r3)`" comment is wrong; report row is 208 B @85.2 (extent discrepancy vs the 4 B body). Verified for W16-J, not touched (out of scope).
- Aliases **not** installed: `PropertyEventProvider`, `??_UVert@RndMesh`, `OutfitConfig` (0x82b66c48 map rename had already retired the OutfitConfig REFUSE) — no witnessed charge in scope.
- DC3 XSocial storage statics (8) remain declared and unreferenced in `PlatformMgr_Xbox.cpp`.

## 6. Side findings for other lanes

- `src/xdk/LIBCMT/stddef.h:20` `offsetof` macro bug (found while checking XONLINE_FRIEND offsets).
- `fn_823EC788`/`fn_823EC980`/`fn_82A87F20` are mis-pinned into `CharClipDriver.s` (they are XboxServer virtuals / Quazal code).
- `PlatformMgr_Xbox.s` vtable `lbl_8217E3B8` carries no RTTI COL.
- Retail `.text` file offset is VA − 0x8200B200 (section table: `.text` va 0x82270000 raw 0x264e00); `.rdata` is VA − 0x82000000; `.data` is VA − 0x82012400. The `.s` address column is synthetic — key on `.fn fn_<addr>`.
- `scripts/target_symbol_map.json` values are str, list **or null**, and one key is a non-address comment — any inversion must tolerate all three.
- `scripts/dump_vtable.py` never selects the virtual-base primary (`??_7X@@6B0@@`) — it partial-matches the first `??_7X…6B…` symbol, which for `Server` is the `6BMsgSource@@@` table.
