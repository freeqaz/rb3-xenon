# jeff round-3 Class 3 review — except_data / EH COMDAT seed-time suppression

**Reviewer:** Fable review agent · **Date:** 2026-07-30 · **Mode:** read-only confirm-and-design
**Scope:** doc-Class-3 only (`docs/plans/jeff-pdata-boundary-round3.md`, Class 3 sections).
**Verdict up front:** Class 3 seed-time suppression **is genuinely OPEN as code** — but its
claimed match payoff is **already banked** by the existing write-time gate. The headline
fixture `ProfileMgr::SetSynapseEnabled` is **currently at `match_percent_normalized == 100.0`.**
Recommend **DROP as a match lever / DEFER a cosmetic cleanup** (see §e/§verdict).

---

## (a) CONFIRMED: seed-time suppression is open

The genuine-EH classification is applied **only at write time**, never at seed time.

**Seed time — `jeff/src/util/xex.rs` L1076-1166** (the pdata loop). For EVERY pdata entry,
unconditionally:
```rust
// L1103-1105  (runs for ALL entries, any func_type)
let section_addr = SectionAddress::new(obj.sections.at_address(start_addr)?.0, start_addr);
obj.known_functions.insert(section_addr, Some(num_insts_in_func * 4));
obj.pdata_funcs.push(section_addr);

// L1109  — NO gate here
if func_type == 3 {
    obj.add_symbol( ObjSymbol{ name: except_data_{start_addr}, address: start_addr-8, ... } )  // L1111-1123
    // L1125-1134: read word1 (handler VA); insert into known_functions if vacant
    if let Entry::Vacant(e) = obj.known_functions.entry(except_func_section) { e.insert(None); }
    // L1148-1160: add except_record_{start_addr} Object symbol
}
```
Nothing here consults `genuine_except_data_set`. The only validity check is a hard `bail!`
if word1/word2 fails to resolve to *any* section (L1136, L1163) — not a code-section (EH)
test.

**Write time — the gate is here, and only here.** `genuine_except_data_set` is computed in
`jeff/src/cmd/xex.rs` **L2490** against the full un-split module, then threaded into
`write_coff` (called L2526). Inside `write_coff` (`src/util/xex.rs`) the gate is applied at
**L1356-1363**:
```rust
if !genuine_except_data.contains(suffix) {
    log::debug!("Skipping spurious except_data {} @ {:#010X} (treating bytes as code)", ...);
    continue;   // bytes stay as code; no zeroing, no EH ADDR32 relocs
}
```
`genuine_except_data_set` (L1268-1291) criterion: read word1 (handler VA) from the 8-byte
struct, keep the symbol IFF `at_address(word1).kind == Code`.

**Conclusion:** the coordinator's grounding is correct. b1bc97c's fix is a **write-time byte
gate**; the **seed-time structural insertions** (known_functions / pdata_funcs / except_data /
except_record symbols) are **not gated**. Class 3 as *code* = OPEN.

---

## (b) SetSynapseEnabled current status — ALREADY MATCHED (the key finding)

- report.json (fresh — mtime 2026-07-30 00:42, after HEAD `19e27a75` 00:39):
  `?SetSynapseEnabled@ProfileMgr@@QAAX_N@Z`, size **16**, **`match_percent_normalized: 100.0`**,
  `fuzzy_match_percent: 100.0`. VA `0x825475B8` (`scripts/target_symbol_map.json:23631`).
  *(The doc's "at_limit" is stale; my first pass misread `match_percent` — the real field is
  `match_percent_normalized`.)*
- **Not fragmented into 4B/12B.** In `build/45410914/asm/ProfileMgr.s` L3086-3095 it is **one
  16-byte `.fn` COMDAT** `fn_825475B8 [0x825475B8, 0x825475C8)`, with a *nested* Object
  annotation `except_data_82533F08` at `0x825475C0` overlaying its **last 8 bytes** (the real
  instructions `stb r11,0x38(r3); b fn_82546FA8`). An Object symbol nested in a Function symbol
  does **not** carve a separate function COMDAT.
- **Why it matches anyway:** that except_data is **spurious**. Its word1 = the instruction
  `99 63 00 38` (`stb r11,0x38(r3)`) = `0x99630038`, which resolves to **no code section** →
  `genuine_except_data_set` rejects it → `write_coff` skips it → the 8 bytes stay code → the
  16-byte function is byte-clean → **100%**.
- **Provenance of the spurious symbol** (symbols.txt L150520): `except_data_82533F08 =
  .text:0x825475C0`. The name encodes TU0 VA `0x82533F08` (a real `fn_82533F08`, size 0x2C8,
  L149351) but the address is TU5 `0x825475C0` — a **stale TU0-era cache symbol rebased onto
  TU5** (target flipped to TU5 on 2026-07-15, *after* this doc was written 2026-07-12). Its
  genuine home would be `0x82533F00`.

**SetSynapseEnabled is a real matched function today. The doc's "+1 confirmed" is already in
the count. Seed-time suppression would add 0 to matched here.**

---

## (c) Class size — much smaller than the doc's 18k, and match-inert

- symbols.txt: **9,368** `except_data_` + **8,600** `except_record_` lines.
- Genuine PDATA_EH classified (build logs, e.g. `~/tmp/ws1A_p6.log`): **~9,062-9,142**
  ("Classified N genuine PDATA_EH structs (handler VA resolves to code)").
- ⇒ **spurious except_data ≈ ~250-300**, not 18,098. The doc's "18,098 of 27,160 / ~9k
  spurious" is a **TU0-era figure** (wave-6, `project_wave6`), invalidated by the TU5 rebase.
- **Fragmenting-a-real-function subset (symbols.txt-only census):** of the 9,362 except_data,
  **172** land strictly inside a function's `[start, start+size)` (not at start); **171** are
  interior of a >8B body; **66** are strictly interior with code following (the "true mid-body"
  shape). Every sampled one has a name-suffix VA ~0x11000-0x13000 *below* its placement — i.e.
  **stale TU0 cache symbols rebased onto TU5**, all spurious by the word1→code test.
- **Match impact of that subset: ~0.** These are `Object` symbols, never counted in
  `matched_functions`, and `write_coff` already skips them so they don't corrupt neighbor code
  bytes. (SetSynapseEnabled is one of the 66 and scores 100%.)

So the class is **symbols.txt noise + pin-boundary friction**, not blocked matches.

---

## (d) Design feasibility + concrete patch shape

**Feasibility: HIGH / confirmed.** The genuine-EH test needs only what the seed loop already
has: read word1 at `start_addr-8` and test `at_address(word1).kind == Code`. The seed loop
*already performs those exact operations* at L1125-1129 (`read_u32(...start_addr-8)` then
`obj.sections.at_address(except_func)`). All sections are loaded before the pdata loop (the
loop reads `.pdata` from `obj.sections`), so the cross-unit handler VA resolves at seed time —
the same argument that justifies `genuine_except_data_set`'s placement. No post-load data is
required.

**Concrete patch shape** (`src/util/xex.rs`, inside `if func_type == 3` at L1109):
```rust
if func_type == 3 {
    // Read word1 (handler VA) and apply the genuine-EH test at SEED time.
    let handler_va = read_u32(obj.sections.at_address(start_addr - 8)?.1, start_addr - 8);
    let genuine = handler_va
        .and_then(|va| obj.sections.at_address(va).ok())
        .map(|(_, s)| s.kind == ObjSectionKind::Code)
        .unwrap_or(false);
    if !genuine {
        log::info!("Skipping spurious func_type==3 record @ {:08X} (word1 not code)", start_addr);
        // skip except_data / except_record adds (L1111-1164)
        // AND skip the handler-VA known_functions insert (L1131)
    } else {
        // ...existing L1111-1164 body unchanged...
    }
}
```
Notes:
- **Do NOT gate L1104-1105** (the `start_addr` → known_functions/pdata_funcs insert). For a
  `func_type==3` record, `start_addr` is the *real* function start regardless of whether the
  8-byte prefix is genuine EH — gating it would drop legitimate anchors. (This is why doc
  mechanism #1 — "bogus pdata-anchored start mid-body" — did **not** manifest for the
  SetSynapseEnabled fixture: the record's start was the real `fn_825475C8`.) The suppression
  should target only the except_data/except_record symbols + the handler-VA
  `known_functions` insert (L1131).
- The **stale-cache** copies (the 172 rebased TU0 symbols) come from symbols.txt via
  `Symbols::add`, **not** fresh seed. Seed-time suppression alone will NOT evict them —
  they need the same treatment the write-time gate already gives (skip on emit) or a
  one-time cache scrub. In fact a **clean re-split (cold symbols.txt)** would already place
  each fresh func_type==3 except_data at the correct `start_addr-8` and drop the drifted
  copies — so much of the "noise" is a cache-staleness artifact independent of this pass.
- The `write_coff` gate stays as belt-and-suspenders (doc §273-277 — agreed).

**Handler-uniqueness / exotic-EH risk (Q4): LOWER than the doc frames it.** The criterion is
"word1 resolves to a *code section*", **not** `word1 == 0x82804210`. Any code-VA handler
passes, so multiple/exotic handlers (TU5/Bink/XDK) are accepted, not false-negatived. (`grep`
for `__CxxFrameHandler`/`0x82804210` in symbols.txt is empty because the handler extern is
synthesized at write time, L1858-1868 — the seed/classify path never hardcodes it.) The only
false-negative would be a genuine EH struct whose handler VA points to a non-code section —
implausible. Because the *criterion is unchanged* (only its application point moves earlier),
this risk is not new relative to the already-landed write-time gate.

---

## (e) Honest-floor pricing (Δ = matched − masked_equal)

Per `project_honest_floor_2026-07-29` (quote `matched_functions − masked_equal_functions`):

- **SetSynapseEnabled: +0.** Already at `match_percent_normalized == 100.0` and already in
  matched. The write-time gate solved the *match* problem; seed suppression banks nothing more.
- **Noise reduction: real but cosmetic, and ~10-60x smaller than the doc claims.** Removing
  spurious except_data/except_record affects `Object` symbols only — **not counted in
  matched_functions**, so **Δ(matched − masked_equal) = 0**. The tangible effect is
  ~172 (fragmenting) to ~300 (total spurious) fewer symbols.txt lines → reduced pin-boundary
  friction. The doc's "~9k noise reduction" is a stale TU0 figure; genuine EH (~9,062) must
  **stay**, so the removable set is only the spurious ~300.
- **Unblocked-surface: unproven / likely ~0.** No evidence of a real function currently held
  below 100% *because of* a seed-time except_data or handler-VA insertion — the write-time
  gate already neutralizes the byte damage, and mechanism #1 didn't fire on the fixture.

**Honest payoff: ≈ +0 strict; value is cosmetic symbols.txt de-noise (~300 lines).**

---

## (f) Staging-plan check (vs current fleet discipline)

The doc's staging plan (§285-303) **matches** current discipline, with one refinement:

- ✅ "Build in a COPY, `cargo build --release` in `~/tmp/jeff-round3`, never in place; gated
  atomic swap of `../jeff/target/release/dtk`; commit fork source immediately after swap."
  This mirrors CLAUDE.md's wibo/dtk staging discipline and `project_jeff_asm_misnest`.
- ⚠️ **Refinement vs CLAUDE.md:** the build wiring note says configure.py resolves jeff to its
  **prebuilt release binary** (absolute path), and "there are no cargo build edges in
  build.ninja anymore." So the swap target is the release binary; there is no cargo edge that
  would auto-rebuild it — good (matches the doc's intent), but the byte-parity gate the doc
  describes must be run manually (as with wibo) before the swap, since ninja will not.
- ✅ "symbols.txt is committed dtk output; land binary → full re-split → commit symbols.txt +
  splits.txt churn in the same rb3-xenon commit + fork source commit." Current git shows
  `config/45410914/symbols.txt` already dirty in the shared tree — **do this only in an
  isolated worktree** (`scripts/setup_worktree.sh`), never mutate the shared symbols.txt for an
  experiment.
- ✅ Validation authority = `report.json match_percent_normalized == 100` diffed as a **set**,
  not raw count; header/seed-touching change ⇒ **full `./tools/ninja-locked` rebuild**
  (per `feedback_header_patch_full_rebuild_verify` + `feedback_report_cache_stale_ab`: `rm -f
  build/.../report.cache` before each report leg).

---

## Structured summary

- **Class 3 open? (code)** — **YES.** Seed-time func_type==3 insertion (`util/xex.rs`
  L1104-1105, L1109-1165) is unconditional; the genuine-EH gate lives only in `write_coff`
  (L1356) via `genuine_except_data_set` (built `cmd/xex.rs` L2490, applied write-time only).
- **But headline payoff already banked** — `ProfileMgr::SetSynapseEnabled` is **100%
  (match_percent_normalized)** NOW: one 16B COMDAT, spurious except_data skipped by the
  write-time gate. Doc's "at_limit / 4B-12B split" is stale (pre-TU5-flip, 2026-07-15).
- **Patch shape** — inside `if func_type == 3` (L1109): compute `genuine = at_address(word1
  read at start_addr-8).kind==Code`; if `!genuine`, skip the except_data/except_record adds +
  the handler-VA `known_functions` insert (L1131). **Do NOT** gate L1104-1105 (start_addr is
  the real function start). ~10 lines. Feasible — seed loop already reads word1 + resolves
  sections at L1125-1129.
- **Class size** — ~300 spurious except_data (not 18k); 172 land mid-body, 66 strictly
  interior — nearly all **stale TU0 cache symbols rebased onto TU5**, all match-inert.
- **Honest payoff** — **≈ +0 strict** (SetSynapseEnabled already counted). Only cosmetic
  symbols.txt de-noise of ~300 Object lines (Δ(matched−masked_equal)=0). Doc's ~9k figure
  stale.
- **Risks** — asm-misnest overshoot (mitigated: this pass only *removes* Object symbols +
  handler-VA inserts, never resizes functions); exotic-EH false-negative is **lower** than
  doc'd (criterion tests "code section", not `==0x82804210`); stale-cache copies won't be
  evicted by seed suppression alone — a **cold re-split** would fix most noise for free.
- **RECOMMEND: DROP as a match lever; DEFER the cosmetic cleanup.** The write-time gate already
  solved the match problem this class was priced on. If pursued at all, do it as a cheap
  opportunistic symbols.txt-hygiene pass (or just a cold re-split to shed the TU0-rebase
  drift), not as a strict-yield item. Class-1 remains the round-3 priority; struct-layout recon
  outranks all of round-3 (doc §413-426).
