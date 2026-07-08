# Middleware strategy — Bink, Quazal, XDK/CRT and the honest denominator

Status: DRAFT-RFC | Date: 2026-07-08 | Author: Claude Opus (paths-to-100 wave) | Theme: strategy

## Summary

The whole-binary denominator (11,074,108 code bytes) is treated as if a large
slice is unownable vendor middleware. **It is not.** Verified: the only truly
source-less vendor code is Bink/RAD (~65 KB, 0.6% of code). Quazal RendezVous is
**game source we already have** (178 TUs in rb3-Wii, only 10 wired), and the CRT
(LIBCMT) ships as real matchable `.cpp` in `src/xdk/LIBCMT`. The 7.88 MB that
looks like "the middleware wall" is actually **unsplit, unidentified `.text`** —
an identification problem (RFCs 04/05/07/09), not a licensing one. This RFC
buckets every byte, fixes what the denominator should be, and recommends a small
formal exclusion (~90 KB) plus a `middleware-excluded%` secondary metric — while
explicitly refusing to inflate the ceiling by writing off game code as "vendor."

## Motivation

RFC-01 (`01-endgame-definitions.md`) needs a grounded endgame number: "100% of
*what*?" A recurring shortcut is "subtract the vendor middleware and call the rest
100%-able." That shortcut is only honest if we know how many bytes are genuinely
unownable. If we over-exclude (writing off Quazal or the CRT as vendor), we quietly
lower the bar and lie to ourselves about the ceiling. If we under-account, RFC-01's
math floats free of the binary. This RFC pins the exact byte budget per bucket so
the endgame taxonomy rests on verified numbers, and it decides a concrete
per-bucket strategy: match-from-source, reconstruct-by-hand, or formally-exclude.

## Current state (verified)

All numbers below are from the live repo at `a1312de`, verified via
`build/45410914/report.json`, `tools/fuzzy_progress.py`, and
`dtk xex info orig/45410914/default.xex`.

### The XEX section map (authoritative, from `dtk xex info`)

Base `band.exe`, load 0x82000000, entry 0x82816080, file time 2010-08-07. Sections
and VirtualSize:

| Section   | VirtualSize | Kind          | Notes |
|-----------|-------------|---------------|-------|
| `.rdata`  | 0x1E95AC (2,004,396) | rodata | strings, vtables, RTTI, jump tables |
| `.pdata`  | 0x06F370 (455,536)   | rodata | unwind info (not counted as code) |
| `BINKCONS`| 0x002920 (10,528)    | rodata | Bink const pool |
| `.text`   | **0x9B48D4 (10,176,724)** | code | the program |
| `BINK`    | 0x010010 (65,552)    | code   | **vendor Bink decoder** |
| `BINKBSS` | 0x0043A0 (17,312)    | bss    | Bink zero-init |
| `.data`   | 0x1F35EC (2,045,932) | data   | |
| `BINKDATA`| 0x003D54 (15,700)    | data   | |
| `.XBMOVIE`| 0x00000C (12)        | data   | |
| `.idata`  | 0x0004CC (1,228)     | data   | import table |
| `.XBLD`   | 0x000160 (352)       | data   | build stamp |
| `.reloc`  | 0x0FFDCC (1,047,500) | reloc  | |

Static libs the linker pulled in (from `dtk xex info`): **LIBCMT, XAPILIB,
XBOXKRNL, XNET, XINP2, D3D9, D3DX9, XGRAPHC, XONLINE, XMP, XPARTY, XIREMAP,
PRNTCTL, XAPOBA, XAUDIO2, XMCORE, XAUD, XHV2, XMIC** (+ LINK/C1/C2 toolchain
markers). This matters: those XDK libs are compiled *into* `.text`; they are not a
separate section. So "XDK code" is not a bucket you can slice off by address — it
is interleaved anonymous `.text` (see Bucket E).

### The report denominator, bucketed (from `report.json`)

`report.total_code = 11,074,108`. This exceeds raw `.text` (10,176,724) because
dtk's function analysis also counts the `BINK` code section and rounds per-function.
`tools/fuzzy_progress.py`'s three-tier classifier splits it as:

| Class (fuzzy_progress) | Code bytes | % of total_code | Units |
|------------------------|-----------:|----------------:|------:|
| `rb3` (band3/network game) | 752,156   | 6.79%  | 163  |
| `engine` (src/system Milo) | 2,379,848 | 21.49% | 609  |
| `other` (unsplit/xdk/vendor)| **7,942,104** | **71.72%** | 1,684 |

Within `other`, by unit-name section token:

| Sub-bucket | Code bytes | Note |
|------------|-----------:|------|
| `auto_..._text` (unsplit, unidentified `.text`) | **7,876,796** | 99.2% of `other` |
| `auto_..._BINK` (raw vendor Bink code) | **65,292** | the real vendor bucket |
| named non-auto (xdk `.cpp` etc.) | 16 | rounding dust |

**The headline correction:** the "middleware wall" narrative is wrong. Of the
7.94 MB in `other`, **only 65 KB (0.82%) is source-less vendor code**. The other
7.88 MB is `.text` we have simply not *split and identified* yet — the
identification problem owned by RFCs 04 (`04-pinning-at-scale.md`), 05
(`05-data-xref-anchoring.md`), 07 (`07-icf-constraint-solver.md`), and 09
(`09-sibling-title-oracles.md`). It is not exclude-able; it is *unlabelled*.

### DC3 cross-check (the same problem, already further along)

`../dc3-decomp/build/373307D9/report.json`: DC3 is at **44.55% code / 61.72%
functions** on a comparable **11,379,344**-byte denominator. DC3's XEX has the same
Bink sections **plus** RAD (`RADCODE` 0xAF4, `RADCONST`, `RADDATA`) — Bink
VirtualSize 0x12600 (75,264), RADCODE 0xAF4 (2,804). Critically, DC3's report shows
the raw `BINK`/`RADCODE` sections as `auto_*` units with empty `total_code` —
**DC3 never matched the vendor Bink decoder itself.** What DC3 *did* match is the
Bink *engine wrappers*: `moviebink/BinkMovieImpl` (77/90 fns), `BinkMovieSys`
(16/18), `synth/BinkReader` (7/8), `utl/BinkIntegration` (10/14). Those are
Milo-engine glue calling the Bink API — matchable RB3/Milo source, not the decoder.
DC3 reached 44% *with the vendor Bink permanently excluded*, which sets the honest
ceiling frame: **vendor middleware costs ≈0.6-0.7% and everyone just excludes it.**

### Quazal is NOT source-less (scout claim corrected)

The brief asks "rb3-Wii has Quazal source? CHECK." **Verified yes.**
`../rb3/src/network/` contains the full Quazal RendezVous SDK as named C++ (namespace
`Quazal`), e.g. `network/Core/InstanceTable.cpp`:
```cpp
namespace Quazal {
    InstanceTable::InstanceTable() : m_pvContextVector(0) {}
    bool InstanceTable::AddInstance(InstanceControl *ic, ...) { ... }
```
Inventory (`find ../rb3/src/network -name '*.cpp'` = **178 TUs**, ~9,122 LOC):
Core 28, Platform 37, net 25, Plugins 24, ObjDup 19, Services 19, Extensions 14,
ddl/Protocol/Products/Utility/RVPackages 3+2+2+2+3. rb3-xenon already mirrors this
tree under `src/network/` (same subdirs), but **only 10 network TUs are wired** in
`config/45410914/objects.json` (MD5, SessionDiscoveryTable, KeyedChecksumAlgorithm,
DuplicatedObject, TimedSignal, PeriodicJob, NetworkEmulator, StringConversion,
Scheduler, ChecksumAlgorithm). So Quazal is a *game-code oracle bucket*, subject to
the same MWCC→MSVC body-divergence wall as all rb3-Wii source (VERIFIED FACTS wall
#2), **not** a vendor exclusion.

### CRT / LIBCMT has real source

`src/xdk/LIBCMT` ships matchable `.cpp`: `osfinfo.cpp`, `rtti.cpp`, `undname.cpp`,
`vsnprnc.cpp`, `vswprnc.cpp` (dc3 has the first three + vsnprnc/vswprnc identically).
Only 3 are wired in the `xdk` group (`osfinfo`, `rtti`, `nuiapi/nuidetroit`). The
rest of LIBCMT (memcpy, memset, strcpy, ctype tables, `_purecall`, EH runtime) is
compiled anonymously into `.text` and lives inside the 7.88 MB unsplit bucket — it
is *reconstructable* (small, idiomatic, and often already present in dc3's headers),
not exclude-able. Note VERIFIED FACT: `strcpy` already shows the `extsb`/`cmplwi`
instruction-selection divergence — the CRT is matchable but not free.

## Proposal

### 1. Adopt a canonical 5-bucket denominator taxonomy

Feed these exact definitions to RFC-01/02/18 so every doc uses one vocabulary.
Byte figures are the verified `report.json` code bytes above.

| Bucket | Definition | Code bytes | Strategy |
|--------|------------|-----------:|----------|
| **A. RB3 game** | band3/ + Quazal network/ (rb3-Wii oracle) | 752,156 wired + unwired Quazal in `other` | **MATCH from rb3-Wii** (body-divergence wall applies) |
| **B. Milo engine** | src/system (DC3 oracle) | 2,379,848 | **MATCH from DC3** (Rosetta) |
| **C. CRT / LIBCMT** | osfinfo/rtti/undname/vsnprnc + anon memcpy/strcpy/EH | small (interleaved in `.text`) | **RECONSTRUCT by hand** (dc3 headers + MSVC CRT idioms) |
| **D. XDK libs** | D3D9/XAUDIO2/XNET/XAM/etc. static libs in `.text` | interleaved, unmeasured | **EXCLUDE-eligible** (no source), but *identify* to size it |
| **E. Vendor Bink/RAD** | `BINK`+`RADCODE` sections | **65,292** (our binary) | **FORMALLY EXCLUDE** |

The 7.88 MB `auto_text` is not a bucket — it is A+B+C+D *before identification*.
Every RFC-04/05/07/09 identification win moves bytes out of `auto_text` into A/B/C/D.

### 2. Formally exclude only Bucket E (Bink/RAD), and report it

Concrete mechanism (mirrors what dc3 already does implicitly by leaving `BINK`
sections as unmatched `auto_` units):

- Keep the `BINK*` section declarations in `config/45410914/splits.txt` (already
  present: `BINKCONS BINK BINKBSS BINKDATA`) so dtk emits them as isolatable
  `auto_..._BINK` units — do **not** try to pin per-function targets inside them.
- Add a `middleware_excluded` accounting mode to `tools/fuzzy_progress.py`: a
  fourth print line that reports STRICT% over `total_code − excluded_vendor_bytes`,
  where `excluded_vendor_bytes` is computed by summing `total_code` of every unit
  whose name matches `auto_\d+_[0-9A-Fa-f]+_(BINK|RAD)` (currently **65,292**).
  This yields `middleware-excluded strict%` = `962,656 / (11,074,108 − 65,292)` =
  **8.75%** today (vs 8.69% raw) — a 0.06-point honesty adjustment, deliberately
  tiny. The point is *not* to inflate; it is to make the exclusion explicit and
  auditable rather than a hand-wave. RFC-18 (`18-metrics-and-dashboard.md`) should
  carry this as a labelled secondary metric, never the headline.

### 3. Do NOT exclude Quazal or the CRT

State this as policy so a future agent does not "clean up" the denominator by
writing off `src/network` or `src/xdk/LIBCMT`:

- **Quazal** → route through the RB3-game match track. Wire the unwired ~168
  network TUs incrementally (RFC-04 pinning + `rb3wii-pair` skill). Same MWCC→MSVC
  divergence economics as band3 — expect the same yield curve, no better.
- **CRT** → route through Bucket C reconstruction. These are small idiomatic
  functions; the dc3 `src/xdk/LIBCMT` headers already give the declarations, and
  MSVC X360 CRT codegen is well-documented in `docs/decomp/`. Wire the 2 unwired
  `.cpp` (undname, vsnprnc/vswprnc) first as a cheap probe.

### 4. Size Bucket D (XDK) via identification, then decide

We cannot currently exclude XDK code honestly because we don't know how many bytes
it is — it's anonymous `.text`. The move is *identify-then-decide*:

- Use RFC-09 sibling-title / RFC-05 data-xref anchoring to label XDK library spans
  (D3D9/XAUDIO2/XNET have stable, recognizable string/RTTI footprints). The XEX's
  static-lib list gives the target names.
- Once a span is identified as pure XDK vendor code with no available source, add
  it to the `middleware_excluded` set alongside Bink. Until then it stays in the
  honest denominator (counted against us — the conservative choice).

### Data flow

```
dtk xex info ──► section map (E bytes, exact)
report.json ──► fuzzy_progress classifier ──► {rb3, engine, other}
                                    │
                    other ──► {auto_text (=A+B+C+D unidentified), auto_BINK (=E)}
                                    │
RFC-04/05/07/09 identification ──► moves auto_text bytes ──► A/B/C/D
                                    │
fuzzy_progress --middleware-excluded ──► STRICT% over (total − E [− identified D])
```

## Alternatives considered

- **Link vendor `.obj`/`.lib` and diff against them (brief option b).** For Bink:
  the Bink SDK is proprietary (RAD Game Tools); we have neither the `.lib` nor a
  license to redistribute matched output. For XDK: the Xbox 360 XDK static libs are
  Microsoft-proprietary and equally unavailable/unredistributable. **Rejected on
  licensing** — flag for RFC-01: any "100%" that depends on proprietary vendor
  objects is not a shippable/legal 100%. This is exactly why formal exclusion (not
  vendor-obj matching) is the right call for E and identified-D.
- **Exclude everything non-band3/network (the whole 9.6 MB `engine`+`other`).**
  Dishonest — the Milo engine (B, 2.38 MB) is matchable via DC3 and is where DC3
  itself got most of its 44%. Excluding it would fabricate a ~90% "score."
- **Redefine the denominator as `.text` VirtualSize only (10.18 MB).** Marginally
  smaller than report's 11.07 MB but loses the BINK-code accounting and diverges
  from objdiff's authoritative `measures`. Keep objdiff's number as the north star
  (project norm: "no denominator gaming").
- **Reconstruct the Bink decoder from public reverse-engineering.** Effort is
  enormous (video codec), yield is 0.6%, and clean-room legality is dubious. Never.

## Effort & expected value

Anchored to comparable past results in this repo.

- **Bucket E exclusion + metric (this RFC's core deliverable):** ~1 day tooling
  (extend `fuzzy_progress.py`, 1 function). EV = **honesty, not points**: raw→
  excluded strict moves 8.69%→8.75% (+0.06pt). Do it precisely so RFC-01 math is
  grounded; do not sell it as progress.
- **Quazal wiring (Bucket A tail):** 168 unwired TUs. Comparable to band3 game
  ports — the port campaigns landed on the order of +5 to +25 matched fns per
  focused wave (grind close-out `a1312de` = +22; Waypoint `d3c6e4f` = +7). Quazal
  is smaller per-TU than band3 and hits the *same* MWCC→MSVC body wall, so expect
  **low tens of matched fns total** across all 168, spread over many sessions — a
  real but modest vein, correctly categorized as game-match work (owned by the
  grind/bodyport tracks, not this RFC).
- **CRT reconstruction (Bucket C):** small idiomatic functions; a probe wiring of
  undname/vsnprnc/vswprnc could yield a handful of clean matches quickly, but
  `strcpy`'s known `extsb`/`cmplwi` divergence warns that instruction-selection
  stalls are common. EV **single-digit matched fns**, cheap to probe.
- **Bucket D sizing:** the *value* here is defensive — it prevents over-exclusion.
  EV is measurement, not matches.

**Net:** this RFC's own EV is ~0 matched functions. Its value is making RFC-01's
endgame number *correct* and preventing a future agent from lowering the bar by
mis-excluding Quazal/CRT. That is a valid deliverable under the honesty norm.

## Risks & failure modes

- **Over-exclusion creep.** The `middleware_excluded` set is a slippery slope: once
  the machinery exists, pressure to dump "hard" units into it grows. Mitigation:
  the set is *pattern-gated* to `auto_..._(BINK|RAD)` + a hand-audited XDK-span
  allowlist with a source-availability note per entry. No unit with an rb3-Wii or
  DC3 source pairing may ever enter it.
- **XDK identification is itself hard.** Sizing Bucket D depends on RFC-05/09
  landing; if they stall, D stays in the honest denominator indefinitely (safe
  failure — we just don't get the small honesty adjustment).
- **Quazal MWCC divergence.** Wiring 168 TUs could burn effort for few matches if
  the body wall bites as hard as band3. Mitigation: treat as normal grind-track
  work with the usual net-positive gate; do not front-load it.
- **Report `total_code` drift.** If dtk's function analysis changes, the bucket
  bytes shift. Mitigation: the classifier reads live `report.json`; re-run at each
  milestone rather than hard-coding.

## Kill criteria

- **Kill the exclusion metric** if the audited vendor-exclude set can be shown to
  exceed ~2% of `total_code` without a per-unit source-availability justification —
  that would mean it's being used to game the denominator, which the project
  forbids. (Today it is 0.59%; a healthy ceiling.)
- **Kill Quazal wiring** as a distinct effort if the first 10-TU wave yields <2
  matched fns net after body-port attempts — fold it back into the general grind
  backlog with no special priority.
- **Kill Bucket-D sizing** if RFC-05/09 identification can't attribute XDK spans at
  useful precision (mirrors the topo-locator kill at precision 0.13); leave XDK in
  the honest denominator and stop trying to carve it out.

## Open questions

- Does RFC-01 want the headline to be raw-strict% or middleware-excluded-strict%?
  Recommendation: **raw is the north star; excluded is a footnote** — the delta is
  0.06pt today and grows only as D is identified. RFC-01/18 to decide.
- Can we get an *upper bound* on total XDK bytes cheaply (e.g. sum of the 19 static
  libs' typical sizes) to frame the maximum possible honest adjustment before doing
  the identification work? [UNVERIFIED — would need per-lib size data we don't have.]
- Are any Bink *wrapper* fns (our `BinkMovieImpl`/`BinkReader`/`BinkClip` units,
  which DC3 matches at 77-90%) currently below their DC3 match rate? If so they're a
  small Bucket-B-adjacent match vein, separate from the excluded vendor decoder.
  (Our report shows these units exist with small `total_code`: BinkMovieImpl 108,
  BinkReader 76, BinkClip 272, BinkMovieSys_Xbox 40.)

## References

- `build/45410914/report.json` — authoritative `measures` + per-unit `total_code`.
- `tools/fuzzy_progress.py` — three-tier classifier (rb3/engine/other), lines
  126-137; extend here for `--middleware-excluded`.
- `config/45410914/splits.txt` — `Sections:` block declaring `BINKCONS BINK
  BINKBSS BINKDATA .XBMOVIE .idata`; MasterAudio pin recipe.
- `config/45410914/objects.json` — group structure (main/engine/hamobj/xdk/band3/
  network/curl); `network` group has 10 wired TUs, `xdk` has 3.
- `dtk xex info orig/45410914/default.xex` (`../jeff/target/release/dtk`) — section
  VirtualSizes + static-lib list (LIBCMT/XAPILIB/XNET/D3D9/XAUDIO2/...).
- `../rb3/src/network/` — Quazal RendezVous source, 178 TUs (namespace `Quazal`).
- `../dc3-decomp/build/373307D9/report.json` — DC3 at 44.55% code / 61.72% fns;
  `moviebink/BinkMovieImpl` 77/90, `BinkReader` 7/8 (wrappers matched, decoder not).
- `../dc3-decomp/config/373307D9/splits.txt` — DC3's BINK+RADCODE section pins
  (273 BINK/LIBCMT lines) left as unmatched vendor `auto_` units.
- `src/xdk/LIBCMT/{osfinfo,rtti,undname,vsnprnc,vswprnc}.cpp` — matchable CRT source.
- Sibling RFCs: `01-endgame-definitions.md` (consumes this bucket math),
  `02-gap-composition-atlas.md` (the 7.88 MB `auto_text` atlas),
  `04-pinning-at-scale.md` / `05-data-xref-anchoring.md` / `07-icf-constraint-solver.md`
  / `09-sibling-title-oracles.md` (identification that drains `auto_text` into A/B/C/D),
  `18-metrics-and-dashboard.md` (carries `middleware-excluded%` as a labelled metric).
- Skills: `rb3wii-pair` (Quazal/game oracle), `dc3-pair` (engine/Bink-wrapper oracle).
- VERIFIED FACTS 2026-07-08 @ a1312de: STRICT 11,240/65,619 fns (17.13%),
  962,656/11,074,108 bytes (8.69%); WIRED fuzzy 94.602%.
