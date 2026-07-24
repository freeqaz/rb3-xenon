# Homing scan round 4 — full-tree sweep (2026-07-24)

**Result: 25,145 → 26,253 strict (+1,108 NEW, 0 LOST).**
Branch `laneD-homing`, commits `77dee07e` (+1,081) and `4867a4c2` (+27).

## What the method is

`scripts/harvest/homing_scan.py` takes every function in our freshly-compiled
`.obj`s, masks its relocation sites, and compares the masked bytes against
**every** `band.exe` `.pdata` entry of *identical length* (also masked at the
same offsets). Classification per function:

| class        | meaning                                                     | proposable |
|--------------|-------------------------------------------------------------|-----------|
| `UNIQUE`     | exactly one identical retail VA, and it is unmapped          | **yes**   |
| `UNIQUE-ICF` | several identical VAs, all but one already mapped            | no        |
| `MULTI`      | several identical VAs, several unmapped (ICF/COMDAT twins)   | no        |
| `ALL-MAPPED` | identical VAs exist but all are already mapped               | no        |
| `NOMATCH`    | no identical retail function of that size                    | no        |

Only plain-`UNIQUE` is ever proposed. `MULTI`/`UNIQUE-ICF` would look like a
100% match afterwards *even when mispaired*, because both the homing compare
and objdiff's normalized diff ignore relocation targets — the very thing that
discriminates ICF twins. Proposing them buys dishonest numbers.

## What round 4 changed vs rounds 1-3

Rounds 1-3 (+93 strict total) only scanned the handful of TUs each wave had
just touched. Round 4 swept **all 914 built objs** in 23 batches (~4 min).

* 1,667 distinct plain-UNIQUE retail homes found.
* 1,086 got a tight splits pin + a `target_symbol_map` name (251 units, 5 new).
* 372 more were already inside an existing splits `.text` range and needed a
  **map name only** (no pin) — the "reveal" sub-wave.

## Tooling added (round 5 is now cheap)

* `scripts/harvest/homing_scan.py` — env-parameterized:
  `HOMING_WT`, `HOMING_TMAP`, `HOMING_OUT`, `HOMING_MINSZ`, `HOMING_ROOT`,
  `HOMING_BAND`, `HOMING_NO_DEFAULTS` (drop the 6 hardcoded round-1 objs),
  `HOMING_FUNCLETS` (include `__unwind$`/`__ehhandler$` etc).
  Batch-drive it: `NAME=/abs/path.obj` args, ~40 objs per invocation.
* `scripts/harvest/homing_gen4.py` — full-tree generator. Two things gen3
  could not do:
  1. **gap-fill** — appends `.text` ranges to a unit that *already* has a
     splits block instead of emitting a duplicate header (a duplicate header
     makes dtk carve two objs for one source file);
  2. **spatial-cluster owner voting** — a scatter-included COMDAT surfaces
     `UNIQUE` in every TU that includes it, so per-VA "first TU wins" sprays a
     single retail neighbourhood across several units. gen4 clusters VAs with
     a 0x600 gap threshold and votes one owner per cluster, tie-broken by
     class-name-in-mangled-name then lexically.
* `scripts/harvest/homing_apply4.py` — textual splits splicer. Inserts each new
  `.text` line into sorted position inside the target unit's block, appends
  brand-new blocks at EOF, asserts **no new/new and no new/existing overlap**,
  and emits the per-wave map fragment for `tu5_map_apply_fragment.py`.

Round-5 recipe:

```bash
# 1. scan (batched)
HOMING_NO_DEFAULTS=1 HOMING_TMAP=$WT/scripts/target_symbol_map.json \
HOMING_OUT=~/tmp/scan/bNNN.json python3 scripts/harvest/homing_scan.py \
  key1=/abs/a.obj key2=/abs/b.obj ...
# 2. merge the per-batch jsons into one dict, then
python3 scripts/harvest/homing_gen4.py --results merged.json \
  --worktree $WT --out-prefix ~/tmp/h5
# 3. apply per wave, measure LOST after EACH
python3 scripts/harvest/homing_apply4.py --blocks ~/tmp/h5_blocks.json \
  --frag ~/tmp/h5_map_fragment.json --worktree $WT \
  --units-file wave.txt --out-frag ~/tmp/waveN.json
python3 scripts/harvest/tu5_map_apply_fragment.py ~/tmp/waveN.json \
  $WT/scripts/target_symbol_map.json
touch config/45410914/config.yml && rm -f build/45410914/report.cache && ninja
```

## Measured waves

| wave | units | pinned fns | strict total | NEW | LOST |
|------|-------|-----------|--------------|-----|------|
| base | –     | –         | 25,145       | –   | –    |
| 0 (BandCrowdMeter seed) | 1  | 13  | 25,158 | +13   | 0 |
| 1    | 84    | 392       | 25,550       | +405 | 0 |
| 2    | 83    | 351       | 25,897       | +752 | 0 |
| 3    | 83    | 330       | 26,226       | +1,081 | 0 |
| reveal (map-only) | – | 372 | 26,253 | +1,108 | 0 |

1,086 pins → 1,081 strict matches = **99.5% hit rate, zero regressions**.
That hit rate is the honest calibration of the method: a reloc-masked
byte-identical UNIQUE home is essentially always the real home.

## Residue deliberately NOT pinned

| pool | count | why |
|------|-------|-----|
| `MULTI` | 26,961 occurrences | ICF/COMDAT twins — mispair risk, reloc targets are the only discriminator and both the scan and objdiff mask them |
| `UNIQUE-ICF` | 255 distinct VAs | same reason; the one unmapped VA may be a genuinely different source function that ICF happened to fold |
| name-ambiguous | 63 VAs | several TUs claim the same VA with *different* mangled names (STL template twins) — no way to pick the right name |
| name-already-in-map | 22 VAs | our name is already mapped at a *different* VA. These are **mispair leads** for `invcorr_mispair_repoint.py`, e.g. `??_DStreakMeter@@QAAXXZ` @ `0x822da628`, `?Copy@VocalTrackDir@@…` @ `0x822ffb10`, `??0CharBonesMeshes@@QAA@XZ` @ `0x8237c1c8` |
| covered by another unit's range | (folded into the reveal wave) | pinned nothing; map name only |

## New TU clusters discovered

Five units had **no** splits block at all before this round and were created:
`band3/game/VocalGuidePitch.cpp`, `system/beatmatch/PhraseList.cpp`,
`system/meta/MemcardMgr.cpp`, `system/midi/MidiVarLen.cpp`,
`system/os/UsbMidiGuitar.cpp`.

The biggest gap-fills (retail scattered these TUs' COMDATs far from their main
span): `DataFunc.cpp` 72 fns / 60 ranges, `SongData.cpp` 42/38,
`network/net/NetSession.cpp` 27/25, `MasterAudio.cpp` 26/23,
`band3/meta_band/BandProfile.cpp` 25/20, `GemPlayer.cpp` 24/19,
`BandList.cpp` 22/18.

## Negative result — do NOT redo this: funclets + `MINSZ=16`

A second full sweep was run with `HOMING_MINSZ=16 HOMING_FUNCLETS=1` (~40 min,
~3 GB of batch JSON). It surfaced 376 extra plain-UNIQUE proposals across 58
units. **All 376 were EH funclets** (367 `__unwind$`, 9 `__catch$`) — lowering
the size floor from 32 to 16 bytes produced *zero* new non-funclet homes, so
the 32-byte floor costs nothing and should stay.

Applied and measured: **gross +3, −2 LOST** (`default/VocalTrackDir`
`fn_822EE094` and `fn_822EE0E8` — previously-matching anonymous functions that
the added tight ranges re-carved). Net +1. The whole wave was reverted.

Conclusion: funclets do not credit as strict matches from a homing pin, and
inserting sub-parent ranges into a unit can break its existing carve. Round 5
should keep `MINSZ=32` and leave `HOMING_FUNCLETS` off.

## Flywheel note

Our `.obj`s did not change in this round, so re-running the scan against the
same objs yields nothing new. The scan only refills after a **body-port /
source-wiring wave changes what we compile**. Run it after every such wave.
