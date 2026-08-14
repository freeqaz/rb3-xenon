# "We do not hold the body" censused BY UNIT — it is a long grind, and its fat head is mis-pins (lane BODYWRITE-1, 2026-08-14)

Lane IDENT-1 settled that identification tooling is worth ~0.2% of `total_code`
and reframed the frontier as its class 3 — **"we do not hold the body", 5,363
rows / 1,194,012 B / 11.57% of `total_code`, sitting in units that already pin
and already compile** — calling it *"ordinary decomp work wearing an
identification hat"*. This lane was chartered to census that class by unit and
then write the bodies.

**The census says the premise does not hold.** The class is not a set of fat
targets waiting to be written; it is ~725 small units of code we have
**already written**, which **diverges**. Three separate measurements say so,
and one of them cost the lane its top target.

Baseline `65154fb0`, worktree build, ruler `name_check`: `matched 44,394 /
masked_equal 22,897 / total_code 10,320,664 / code% 36.068184`.
Tools: `tools/nobody_unit_census.py`, `nobody_why.py`,
`nobody_contamination.py`, `nobody_size_audit.py`.

## 1. The shape — 725 units, median 820 B, nothing above 32 kB

`nobody_unit_census.py` reconstructs the binary-wide class totals from its own
per-unit table and refuses if they disagree, so a census that silently dropped
rows could not read as a small one.

| | rows | bytes | % `total_code` |
|---|---|---|---|
| we have *a* body | 4,415 | 261,732 | 2.54% |
| no body, **no base obj** (IDENT-1 class 2) | 7,299 | 1,371,596 | 13.29% |
| no body, but already funclet-paired | 22,381 | 856,076 | 8.29% |
| **no body, HAS base obj (class 3)** | **6,990** | **1,253,680** | **12.15%** |

(Class 3 here is IDENT-1's 5,363 plus the 1,492 rows / 61,488 B it reported at
`0 < mpn < 100` — a partial pair is still not a body. The two reconcile.)

**The distribution is the planning result:**

| | |
|---|---|
| units carrying the class | **725** |
| median unit | **820 B** |
| mean unit | 1,729 B |
| **largest unit** | **29,320 B** (0.28% of `total_code`) |
| units ≥ 32,768 B | **0** |
| units ≥ 16,384 B | 3 |
| top 4% of units | 25% of bytes |
| top 12.6% of units | 50% of bytes |
| top 28.6% of units | 75% of bytes |

⇒ **This is 500+ small units, not 20 fat ones.** There is no unit anywhere in
the binary whose class-3 mass is worth even a third of a percent of
`total_code`. Half the class needs ~91 separate TUs opened; 75% needs ~207.
**Any plan that funds this as a wave of fat targets is mispriced.**

## 2. The fat head was a MIS-PIN, and reading it cost the lane its top target

`default/system/rnddx9/Rnd` was the largest class-3 unit in the binary
(29,320 B) in a unit reading **5.2% matched** — the obvious place to start.

It is 97% somebody else's TU. `system/rnddx9/Rnd.cpp` pinned `.text`
`0x8272BAC8-0x82733C70`; its first **24,260 B** are an RBN/UGC song-metadata
validator. The evidence is retail bytes:

- the 15,060 B function at `0x8272C8E8` references **`.\Validator.cpp`** and 99
  distinct UGC assert strings (`ValidateInt( file, nDrumRank, 0x29 )`,
  `(nVocalParts >= 0) && (nVocalParts <= kRBNVocalPartMax)`, …);
- the block is **contiguous and closed** — 12 functions reference the UGC string
  range, **zero** after `0x827319CC` do, and the last of them ends *exactly* at
  `0x827319CC`;
- `fn_8272BB98` is called by **all 12 and nothing else** — the TU's assert helper;
- **neither oracle holds it** (`file.Fail()` is absent from dc3-decomp and
  rb3-Wii), so this is RB3-only code nobody has ever identified.

Meanwhile the real `DxRnd` rows sit at `0x82732BE8-0x82733B28` and **14 of the
unit's 18 named rows are already at `mpn` 100**. The "5.2% matched" was an
artifact of billing the unit 24 kB of a foreign TU.

Pin shrunk to `0x827319CC` (deliberately conservative — only what the strings
prove). Measured exactly as pre-registered for a pin change: **Δmatched +0,
Δcode_bytes +0, Δcode% +0.000000pp, Δtotal_code 0, 0 units fell off**,
`pairable units 1728 → 1729`. Class 3 fell by **exactly 24,260 B** and class 2
rose by **exactly 24,260 B** — the block is now correctly labelled *unidentified
and unwired* rather than *a body we owe*.

⚠ `0x8272BAC8` still carries the map name
`?_M_incr@_List_iterator_base@stlpmtx_std@@` — almost certainly an ICF-arbitrary
assignment onto Validator.cpp's first function.

### Sizing the contamination, with an instrument that had to be fixed first

`nobody_contamination.py` flags a unit when its class-3 rows form a large
contiguous block **outside the address hull of its named rows**.

⛔ **Its first version was VACUOUS: built from the `rnddx9/Rnd` case, it did not
fire on `rnddx9/Rnd`.** That same `?_M_incr` row is mapped to `0x8272BAC8`, the
pin's first address, so one shared STL COMDAT stretched the hull across the
entire foreign block and every mis-pinned row read *inside*. The hull is now
built only from symbols defined in **exactly one** base obj.

| verdict | units | class-3 B | % class |
|---|---|---|---|
| **MIS-PIN SUSPECT** | 8 | 67,352 | **5.37%** |
| outside hull but small/adjacent | 234 | 452,832 | 36.12% |
| inside hull (genuine gap) | 197 | 174,132 | 13.89% |
| **UNDECIDABLE (<3 TU-owned named rows)** | 286 | 536,536 | **42.80%** |

★ The fix cost coverage honestly: UNDECIDABLE went 16.4% → 42.8%. **This is a
lower bound from an instrument that can say "I don't know", which is worth more
than a confident number from one that cannot fail.** Other flagged units:
`RockCentral` (11,260 B outside), `VocalTrackDir` (8,404 B),
`AccomplishmentSongConditional`, `Joypad`, `RetryAudioPanel`, `Debug`, `Font`.

## 3. The clean core is DIVERGENCE, not absence — the lane's premise inverted

Restricting to the 197 units whose pin the detector can clear leaves **174,132 B
— 1.69% of `total_code`**. For each, compare our source against the better of
the two oracles:

| | units | bytes | % |
|---|---|---|---|
| **ours ≥ oracle (we already hold the code)** | **136** | **121,832** | **70.0%** |
| ours < oracle (may be missing code) | 60 | 52,012 | 29.9% |
| no oracle (RB3-only TU) | 1 | 288 | 0.2% |

Function-level diffs confirm the line counts are not lying. Our engine function
lists are **identical to DC3's** in `MeshDeform`, `Wind`, `CharBones`, and we
have *extras* DC3 lacks (`CharClipSet::RandomizeGroups`). The only oracle-side
surpluses are **rb3-Wii dev-build functions** (`RndMeshDeform::Reskin`,
`SetMesh`, `CopyWeights`) that retail-360 does not appear to ship.

⇒ **"We do not hold the body" is literally true (no byte-identical body) and
semantically misleading. It does NOT mean we never wrote the code.** For 70% of
the clean core we hold source at *greater* volume than either oracle, and the
row is anonymous-and-unpaired because our compiled body **diverges**. The work
is ordinary near-miss matching — with the extra cost that the row must also be
*identified* before it can pair, which is the expensive half IDENT-1 already
priced at ~0.2%.

## 4. A targeting hazard: `report.json` sizes can be wildly inflated

`nobody_size_audit.py` re-derives every class-3 byte figure from dtk's own asm
`size:` extents. Aggregate inflation is small — **8,256 B, 0.7% of the class as
billed** (44 rows over-billed, 64 under-billed, over 2,357 rows where dtk offers
a second opinion; the other 4,633 rows have none).

⛔ **But it is concentrated, and it lands exactly where it does damage.**
`default/Shader`'s top class-3 row `fn_824A59D4` is billed **8,852 B** — 73% of
that unit's class-3 mass and its obvious first target. Its real body is
**12 bytes**: `li r11,1; clrlwi r3,r11,24; blr` — it returns `true`. Its three
neighbours are billed exactly right (20/28/1436), so this is the known
"dtk bills an UNBOUNDED symbol to the next boundary" hazard, per-symbol.

⇒ **This is a TARGETING hazard, not a denominator one.** 0.7% never moves a
headline, but it manufactured the single fattest-looking prize in a unit this
lane was about to open. **Price a class-3 candidate from the asm extent, never
from `report.json`'s `size`.**

## 5. What this lane deliberately did not do

- **Did not write bodies.** The census removed the premise: the clean core is
  code we already hold, and its residue is divergence requiring per-row
  identification first. Writing new bodies into units whose function lists
  already match DC3 would have been invention, not decomp.
- **Did not touch the Quazal units** (`PRUDPEndPoint` 12,992 B, `ObjDupProtocol`
  12,060 B, `PRUDPStream`, `NATTraversalEngine`, `StationURL` — ~57 kB, all at
  **0.0% matched with 0 owned symbols**). These are the 7-line
  `namespace Quazal {}` map scaffolds AUTOID-1 sized; they are `/Od` vendor
  middleware with no oracle, and bodies there buy pairable rows with no content.
- **Did not name anything.** Naming a class-3 row before its body matches buys a
  pairable row at 0% — `ForceEmit_*`-class metric fitting, out of bounds.
- **Did not pin `Validator.cpp` as its own unit.** That needs a real source
  file, and no oracle has one; it would become another declared-but-sourceless
  unit. It is now correctly an `auto_*` carve awaiting identification.
- **Did not extend the Rnd pin cut past `0x827319CC`.** The stretch
  `0x827319CC-0x82732A98` (~4 kB, anonymous) may be a third TU, but nothing
  proves it, so it stays pinned.
- **`src/` did not move**, so `tools/native_build_gate.sh` was correctly not run
  (changes are `config/45410914/splits.txt` + four `tools/` scripts).

## 6. Where the remaining work actually is

| slice | bytes | note |
|---|---|---|
| class 3, total (post-fix) | 1,229,420 | 725 units, median 820 B |
| — mis-pin suspect (lower bound) | 67,352 | fix pins, do not write bodies |
| — undecidable pin state | 536,536 | needs a better hull instrument |
| — clean pin, we already hold source | 121,832 | **divergence + identification** |
| — clean pin, oracle has more | 52,012 | the only genuine write candidates |

**The genuine "write a body" surface is at most 52,012 B — 0.50% of
`total_code` — spread over 60 units, and most of those gaps are tens of lines.**
The largest are `MetaPerformer` (8,728 B), `GemTrackDir` (4,052 B), `PatchDir`
and `MeshDeform` (3,280 B each), all rb3-Wii-oracle game/bandobj units where the
Wii **dev** build's surplus functions are themselves suspect.
