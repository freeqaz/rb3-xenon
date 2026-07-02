# exec-r3-span-confirm — run plan (2026-07-02)

**Stream:** ws7 reopen R3 — build `tools/span_confirm.py`, the oracle-plurality
span-identity confirmer. Stream doc:
`docs/plans/workstreams-2026-07-02/ws7-dead-lever-reaudit.md` (Part 1, R3).
Planner: Fable. Worker: 1 Opus packet. Worktree:
`/home/free/tmp/wt-exec-r3-span-confirm` (branch `exec/r3-span-confirm-0702`).
Baseline report snapshot:
`/home/free/tmp/exec-r3-span-confirm-baseline-report.json` (informational only —
this stream has **no matching impact by design**; no builds required).

## Phase-0 verification (done by planner, 2026-07-02)

All load-bearing claims re-verified LIVE before spending the lane:

1. `dc3_oracle.json` is **committed** at repo root (git ls-files confirms),
   33,987 rows, schema per row:
   `{dc3_va, rb3_va, dc3_name, dc3_tu, similarity, confidence}` —
   e.g. `{"dc3_va":"0x82330000","rb3_va":"0x82706a20","dc3_name":"?asciiDigitToHex@@YAED@Z","dc3_tu":"keygen_xbox.obj","similarity":1.0,"confidence":0.9933}`.
2. Prototype `~/tmp/ws7audit/plurality_repro2.py` re-run on today's state
   (main @00c5b19, 1,079 `.text` spans in splits.txt) reproduces the audit
   numbers **exactly**:
   - raw plurality on DC3-shared pinned TUs: n≥5 178/250 = 71%, n≥10 67%,
     n≥20 63%;
   - **margin-gated (n≥5 in-span rows, top ≥ 2× second, top ≥ 3):
     132/158 = 84% precision at 158/250 = 63% coverage.**
   The R3 premise holds; the stream is alive.
3. Independent (non-oracle-located) candidate sources exist for the triage
   report:
   - `ghidriff_identities.json` — 978 ACCEPT rows (`rb3_addr`, `tu`,
     `wii_symbol`, `tier`, `match_types`); **320 rows fall outside current
     splits.txt `.text` spans** (top unpinned TUs: DuplicationSpace.o 6,
     Session.o 5, Station.o/CallContext.o/ObjDupProtocol.o/WKHandle.o/
     SongData.o 4 each — mostly Quazal network region).
   - `docs/decomp/gameid/crossval_agree.json` — 146 `agree_fns` with
     `{addr, stem, bindiff_conf, bsim_sim, already_pinned, size}`.
   - `sysnet_port_worklist.json` / `band3_port_worklist.json` — same
     ghidriff/BSim provenance (`rb3_addr`, `tu` fields), usable as extra
     candidates.
   All of these are located by Wii↔Xenon signal (ghidriff/BinDiff/BSim), NOT
   by `dc3_oracle.json` (DC3↔RB3) — so voting them with the DC3 oracle is
   non-circular.
4. Circularity exclusion re-confirmed: `pin_candidates.json` /
   `game_splits.json` are **oracle-cluster-derived** (ws3 pipeline) and MUST
   NOT appear in the triage report as confirmable candidates (the oracle
   would be voting for itself).

## Deliverables (single packet, verdict "land" bar)

1. `tools/span_confirm.py` — committed, stdlib-only, read-only over committed
   inputs. Modes: single-span verdict, batch, and `--calibrate`.
2. Calibration gate reproduced by the committed tool: **≥80% CONFIRM
   precision at ≥50% coverage on DC3-shared pinned TUs** (planner measured
   84% @ 63% today with the gate n≥5 / top≥3 / top ≥ 2× second).
3. Triage report `docs/decomp/research/2026-07-02-span-confirm-triage.md`
   over the current unpinned candidates from the independent sources above,
   with per-candidate CONFIRM/CONTRA/ABSTAIN and honest coverage stats
   (ABSTAIN-heavy in the Quazal region is an acceptable, expected outcome —
   DC3 shares Quazal but oracle density there is unverified).
4. This handoff doc + tool + report committed on branch
   `exec/r3-span-confirm-0702`.

Consumers: ws2 worklist-regen triage, ws5 case-B target selection.
Anti-consumers (hard rule): ws3 oracle-cluster targets — the tool must print
a circularity warning in its output header.

## Success / kill bars (from the stream doc — do not soften)

- Land: tool runs end-to-end, calibration ≥80%/≥50% on DC3-shared TUs,
  report is honest. No matching-impact requirement.
- Kill: if the committed tool cannot reach 80% precision at 50% coverage on
  the pinned-TU ground truth with any reasonable gate in the audited family
  (n≥5, margin 2×, top≥3 was already shown sufficient), report the failure
  honestly instead of shipping — that would mean the audit numbers don't
  survive committed-tool rigor.

## Post-run integration notes (for the reviewer)

- No A/B needed (no source/config edits that affect matching). Verify:
  `python3 tools/span_confirm.py --calibrate` output matches the report's
  claimed numbers; spot-check 2–3 CONFIRM and 1 CONTRA row by hand against
  `dc3_oracle.json`.
- The ws2/ws5 docs should get a one-line pointer to the tool when this lands.
