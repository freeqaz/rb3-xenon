# WS2 — Loose-band (BSim simconf 10–15) worklist regen + honesty gate

**Date:** 2026-07-02  ·  **Stream:** round-2 ws2 (Option A, no Ghidra run)  ·
**Branch:** `exec/r2-ws2`  ·  **Verdict: PASS** (hand off full band, confirm-on-consume).

Recipe: `docs/plans/workstreams-2026-07-02/ws2-worklist-regen.md`.
Pipeline lives in `../rb3` (vet + ingest tools); this repo consumes the ingested
identities. Scratch: `~/tmp/ws2-regen/`.

## What was produced

- **`ghidriff_identities_loose.json`** (gitignored/regenerable) — 960 rows,
  BSim simconf **10–15 only** (`--only-band 10,15`), source-tagged
  `ghidriff-run3-simconf10`. Categories: band3 355 / system 385 / network 214 /
  main 4 / null 2. This is the *incremental* tranche below the strict 978-row
  0.900-tier `ghidriff_identities.json` (which is untouched).
- **`docs/plans/band3-port-worklist-loose.md`** — 301 **net-new** band3 rows /
  105 TUs (net-new vs the LIVE `target_symbol_map.json`).
- **`docs/plans/sysnet-port-worklist-loose.md`** — 474 net-new sys/net rows /
  269 TUs.
- Tool patches: `../rb3` `ingest_ghidriff_accepts.py` (`--vetted/--matches/
  --bsim-floor/--source-tag/--only-band`, carries `rb3wii_check`); this repo's
  `tools/gen_{band3,sysnet}_port_worklist.py` (`--ident/--out-suffix`,
  `bsim10-15` label, `rb3wii_check` column, `__file__`-derived repo root so a
  worktree copy writes into its own tree).

These are **CANDIDATES for future rounds** — no strict matches are minted this
round. Consumption is confirm-on-consume via the v2 harvest workflow.

## How the band was derived

1. **Re-vet** the immutable run-3 archive at `--min-bsim-simconf 10` with
   `--sibling-check on --sibling-action REJECT` (calibrated literal-diff
   downgrade of sibling-aliased small bodies). ACCEPT grew 2,207 (≥15) → 3,176
   (≥10); **78 rows sibling-downgraded to REJECT**.
2. **Ingest** at `--bsim-floor 10 --only-band 10,15` → 960 rows (BSIM only,
   sdk/seed/null/judged-wrong excluded).
3. **Gen** net-new worklists vs the live map.

## Honesty gate — 20-pair stratified judging (the PASS)

Sample: 10 from simconf 12–15, 10 from 10–12; mixed band3/system/network;
`rb3wii=contradicted` rows excluded from the main set and judged separately (3).
Bodies read from the run-3 diff-json `modified` section (both sides' decompiled
`code`); M1/M17/C2 cross-checked via Ghidra (port 8002) + the live
`target_symbol_map.json` + rb3-Wii source. Full per-pair evidence:
`~/tmp/ws2-regen/loose-band-judging.json`.

### Confusion matrix (main sample, n=20, non-contradicted)

| verdict | count | meaning |
|---|---|---|
| CONFIRMED | 18 | body + resolved callees + immediates/strings all consistent with the Wii symbol |
| PLAUSIBLE | 2 | trivial byte/word getter (M15 GetIsLooped, M16 WidgetDrawType); correct shape + const-ness, no contradicting evidence, but body too small to fully disambiguate from a sibling getter |
| WRONG | 0 | — |

**Judged precision = 0.90 (worst case: both PLAUSIBLE counted wrong) … 1.00
(PLAUSIBLE counted correct).** Point estimate **≈ 0.90–1.00**, comfortably above
the ≥0.80 pass bar and above the doc's predicted ~0.85 (the sibling-check REJECT
pre-scrubbed the dominant failure mode; n=20 gives a wide CI, so the true band
precision is likely the predicted ~0.85–0.90).

Representative CONFIRMED evidence (member/vtable-slot/stride deltas are the
expected MWCC→MSVC layout differences, not identity errors):
- **M18** `__ct Quazal::RTT(Ui)` — *identical* body `*p=arg<<3; p[1]=0; p[2]=arg`.
- **M20** `MigrationInProgress DuplicatedObject` — mask `&0x3fffff`, `SignalError
  0xe000000e`, CallRegister::MigrationInProgress; the `0xe000000e`+`0x3fffff`
  literals match exactly.
- **M11** `NewTrack(BandUser)` — GetTrackType; consts `3,0xc`; two `new`+ctor
  branches (Vocal 0x2f0/0x310, Gem 0xec/0x104).
- **M17** `CharClipSet::ResetEditorState` — `ResetPreviewState()` +
  `ObjectDir::ResetEditorState(this)`, exact 2-call match vs `CharClipSet.cpp:51`
  (Ghidra-resolved; no diff-json body).

### Contradicted sample (n=3, judged separately)

| id | verdict | note |
|---|---|---|
| C1 `GetSectionStartTick TrackerSectionManager` | CONFIRMED | identical indexed getter `*(*p + i*0xc)` — **false** contradiction |
| C2 `__rs BinStream OldColorOption` | **WRONG** | 0x82297d38 is a **proven pin** `?Load@TransformCrowd@@` in the live map — sibling read-operator alias (ReadEndian 4 + sub-read). **True** contradiction. |
| C3 `AddActivity Quazal::Job(PCc)` | CONFIRMED | null-branch StackTracer Instance+AddActivity, StringStream `<< " __suppl "` chain — **false** contradiction |

**Contradicted precision ≈ 2/3.** So excluding `contradicted` from
confirm-on-consume handoff (the default) is appropriately conservative but **not
junk** — they are a labels-only reserve. Net verdict:

- **≥0.80 → PASS.** Hand off the **full non-contradicted band** with per-fn
  confirm-on-consume; keep `contradicted` rows as a labels-only reserve.

## Open-question answers

1. **Does `--sibling-check on` retro-downgrade any of the existing 978?**
   **Yes — 23 of the 978** strict-tier ACCEPTs are flagged as sibling-aliasing
   false identities by the calibrated check (of 78 total flags; the other 55 are
   in the 10–15 band). This is a **free precision repair** for the strict tier.
   Rows already consumed as landed matches landed via byte-equality (safe);
   **unconsumed flagged rows should be pulled from ws1's remainder.** Full list
   in `~/tmp/ws2-regen/` (recompute: sibling-flagged ∩ `ghidriff_identities.json`
   addrs). Notable: 4 are `Track::*` getters (`GetPlayer/GetTrackIcon/
   PlayerDisconnectedAtStart/PushGameplayOptions` — all alias
   `GetBandUser TrackConfig`, member 0x28 vs 0x40), plus UIList/VocalPlayer/
   FocusTracker/`clear _List_base` STL siblings.
2. **`rb3wii_check=contradicted` (173 rows in the loose band)** — judged 2/3
   still correct (C1/C3 false, C2 true). Keep excluded from handoff by default;
   they are a labels-only reserve, not discarded.

## Free fold: R4 cross-validation corroboration (docs/decomp/gameid/)

The R4 gameid-crossval experiment (`docs/decomp/gameid/VERDICT.json`, NEGATIVE
for span-bracketing) left `crossval_agree.json` — 146 per-fn game-code identity
hints (BinDiff conf≥0.7 ∧ BSim sim≥0.5 agree on `.cpp` stem, 0.95 pin precision),
93 unpinned. Independent signal from the loose ghidriff band. Overlap:

- **11 loose-worklist rows share a Xenon addr with an unpinned crossval hint.**
- **8 of those also agree on the TU/stem (double-agreement)** — the
  highest-confidence loose ids, consume-first: `Buffer::operator+= (0x82A79AF8)`,
  `ChangeMasterStationOperation::GetImplicitStationConnection (0x82A86C30)`,
  `HMACChecksum::ctor (0x82B1B1B8)`, `SearchSettings::ctor (0x823E0E88)`,
  `Message::GetLastError (0x82A7B5E8)`, `RC4Encryption::ctor (0x82AE6A20)`,
  `ScoreDisplay::SetValues (0x8230D328)`, `SystemComponent::Trace (0x82A78CF0)`.
- **3 disagree on stem** (`Credentials`↔`Bitmap`, `NetMessenger`↔`CameraManager`,
  `Scoring`↔`TrackerUtils`) — treat as extra caution / possible sibling alias.
- Full list: `~/tmp/ws2-regen/crossval_corroboration.json`.

Folded as a consume-first hint, not merged into the row schema (crossval has no
`wii_symbol`). Cheap, high-value.

## Consumption guardrails (unchanged from v2 + this band)

- Per-symbol objdiff in lanes; ONE composed whole-binary A/B at integration;
  reviewer reproduces numbers; ≤44 B stub-fold guard; never inject CW names into
  `target_symbol_map.json`.
- **Per-fn confirm-on-consume**: diff vtable-slot/type-tag/node-size immediates +
  strings + resolved callees vs the Wii body before porting each loose id
  (the C2-style sibling alias is exactly what this catches).
- `fn_resolver.py` T4b grades all BSim rows flat 0.93 — **do NOT feed the loose
  file into T4b** until confidence is graded by simconf (0.85 for the 10–15
  band). Loose rows stay in `_loose` files with `source=ghidriff-run3-simconf10`.

## Regenerate

```bash
# ../rb3
VENVPY=build/SZBE69_B8/ghidra/ghidriff-venv/bin/python
ARCH=build/SZBE69_B8/ghidra/ghidriff-xenon/run3-archive
$VENVPY tools/ghidra/vet_xenon_identities.py --run-dir $ARCH --min-bsim-simconf 10 \
  --sibling-check on --sibling-action REJECT \
  --diff-json $ARCH/json/*.ghidriff.json --out ~/tmp/ws2-regen/vetted_simconf10.json
$VENVPY tools/ghidra/ingest_ghidriff_accepts.py --gate full \
  --vetted ~/tmp/ws2-regen/vetted_simconf10.json --matches $ARCH/json/*.matches.json \
  --bsim-floor 10 --only-band 10,15 --source-tag ghidriff-run3-simconf10 \
  --out ../rb3-xenon/ghidriff_identities_loose.json
# rb3-xenon
python3 tools/gen_band3_port_worklist.py  --ident ghidriff_identities_loose.json --out-suffix _loose
python3 tools/gen_sysnet_port_worklist.py --ident ghidriff_identities_loose.json --out-suffix _loose
```
