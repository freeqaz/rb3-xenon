# Lane G — content join over the homing scan's MULTI residue (2026-07-24)

**Result: 26,338 → 26,541 strict (+203 NEW, 0 LOST).** Branch `laneG-multi`.

Plus a bigger non-scoring finding: **423 existing `target_symbol_map` entries are
mispaired** and are currently scoring as honest 100% matches. See "Map trust
audit" below.

## The problem this lane attacks

Round 4 of the homing scan (`docs/plans/homing-scan-round4-2026-07-24.md`) swept
all 914 objs and proposed only plain-`UNIQUE` homes, deliberately leaving
~26.9k `MULTI` and ~3.7k `UNIQUE-ICF` occurrences unpinned:

> ICF/COMDAT twins. Both the homing compare and objdiff's normalized diff mask
> relocation targets — the only discriminator — so a mispair would still read
> 100%. Dishonest numbers.

That is exactly right *for byte identity alone*. But every masked slot is a
relocation in our obj (we know the symbol) and a fully-resolved instruction in
retail (we can decode the address). Comparing them **positionally** — offset by
offset — recovers the discarded discriminator.

New tool: **`scripts/harvest/multi_content_disambiguate.py`**.
`homing_scan.py` results in, homing-scan-format results out (resolved records
re-classified `UNIQUE`), so `homing_gen4.py` / `homing_apply4.py` consume it
unchanged. `homing_gen4.py` gained `--reveal-frag`, which scripts the map-only
sub-wave (VAs already inside an existing splits `.text` range need a name, not a
pin) that round 4 did by hand.

## Acceptance rule

    accept candidate c  iff
      (a) c has >= 1 AGREE and 0 CONFLICT, and
      (b) every rival c' resolves to different decodable content **at a slot
          where c positively AGREED**.

Clause (b) is the honesty clause: rivals must be *positively excluded at a
confirmed slot*, never merely "less good" or "undeterminable". Everything else
is reported UNRESOLVED. No rank-and-guess, no address-proximity tie-breaks.
A retail VA claimed by two different mangled names is dropped entirely.

## Which discriminators paid off

| class | what it is | verdict |
|---|---|---|
| `str` | our reloc → `??_C@` COMDAT; retail's decoded VA holds the same C string | **best**. Map-free ground truth. Cracks the whole `DECLARE_MESSAGE` / `StaticClassName` / `Type()` boilerplate family |
| `f32`/`f64` | our reloc → `__real@<hex>` (the mangled name *encodes* the bytes); retail's VA holds them | **works**, map-free, but thin — few functions reference FP constants at a masked slot |
| `vfstr` | string at referenced VA + 0x20 (vftable → class-name delta) | works, small volume |
| `sym` (ungated) | our reloc → a name in `target_symbol_map`; retail's `bl`/address = that VA | **UNSAFE — do not use.** Measured 75.7% precision. It inherits the map's own mispairs, and worse, a *bad* map entry makes the true candidate CONFLICT so a decoy wins |
| `sym` (trust-gated) | same, but only callees whose own map entry is content-corroborated (`--trust-file`) | **safe enough**: 95.1% measured precision floor, and inspection says the 12 "misses" are themselves map bugs |
| `op` (instruction form at a masked slot) | documented, **not implemented** | masking zeroes whole 4-byte instructions, opcode included, so form is a free exclusion signal. Unimplemented because `TIE`/`NOT-EXCLUDED` were already tiny (137 / 1) |
| address proximity / spatial cluster | — | **deliberately not used as a decision input** (gen4 still uses it for *owner* voting only, as in round 4) |

## Precision — how it was established

`--validate` is a **held-out ground-truth measurement**, not a self-check. It
runs the resolver on functions whose retail home is *already known* (exactly one
byte-identical hit is mapped to that very symbol), feeds **all** hits as
candidates, and scores the pick against the map.

Full-tree run, map-free evidence only (`--no-sym`):

```
RESOLVED-STRONG 1,526 decisions — 750 HIT, 776 MISS
    ... and all 776 MISSes are MISS/TRUTH-CONFLICT
```

i.e. **zero misses where the map's own label was content-corroborated**. Every
single "miss" is a case where the *ground truth itself* is contradicted by
band.exe content. Spot-checked by hand and confirmed unambiguous:

| symbol | map says | that VA actually references | resolver picked | which references |
|---|---|---|---|---|
| `?StaticClassName@Object@Hmx@@` | `0x8227ae48` | `"TrackPanelDir"` | `0x82271a90` | `"Object"` |
| `?StaticClassName@Sfx@@` | `0x826fc3e8` | `"FxSendCompress"` | `0x826b1c88` | `"Sfx"` |
| `?StaticClassName@CharHair@@` | `0x8236a3a8` | `"CharIKFoot"` | `0x8236a628` | `"CharHair"` |
| `?StaticClassName@CharBonesObject@@` | `0x825718e8` | `"UGCPurchasePanel"` | `0x82369b28` | `"CharBonesObject"` |

`X::StaticClassName` is `static Symbol s("X")` — it *must* reference the literal
`"X"`. So the resolver is right and the map is wrong in all four.

With trust-gated `sym` added: `RESOLVED-STRONG` 1,898 (1,122 HIT / 776 MISS, all
TRUTH-CONFLICT) and `RESOLVED-SYM` 247 (235 HIT / 12 MISS, all
`MISS/NO-EVIDENCE` — the map's VA references nothing checkable, so the miss is
*unadjudicable*, not a demonstrated tool error). Inspecting those 12 shows an
off-by-one chain (`Load@BoolKeys`→`Load@FloatKeys`→…, each landing one slot off
in address order) that again points at the map, not the tool.

**Stated precision: ~100% for the map-free `str`/`f32` classes (0 demonstrated
errors in 1,898 held-out decisions), ≥95.1% floor for trust-gated `sym`.**

Tightening sym to ≥2 distinct trusted callees (`--min-sym-agree 2`) gives 100%
but drains the vein (247 → 3 decisions), so it was not used.

## Waves applied

| wave | evidence | map entries | strict total | NEW | LOST |
|---|---|---|---|---|---|
| base | — | — | 26,338 | — | — |
| A | map-free only (`--no-sym`) | 152 (76 pins + 76 reveal) | 26,432 | +94 | 0 |
| B | + trust-gated `sym` | 124 (61 pins + 63 reveal) | 26,541 | +109 | 0 |
| C | flywheel re-run (map grew → trust set grew) | 1 | 26,541 | +0 | 0 |

276 map entries → +203 strict = **73.6% flip rate, zero regressions.** (Lower
than round 4's 99.5% because a correct *name* only flips a function when the
rest of the unit's carve also lines up.)

Wave C is dry, so the MULTI content-join vein is **drained at the current obj
set**. Like the homing scan, it only refills when a body-port / source-wiring
wave changes what we compile — re-run it after every such wave.

## Resolved vs left unresolved

Final pass over 30,649 `MULTI` + `UNIQUE-ICF` records:

| verdict | count | why not resolved |
|---|---|---|
| `NO-EVIDENCE` | 18,081 | function references no string, no FP constant, no trusted callee at any masked slot. **Structurally unresolvable by this method** — it is the honest floor |
| `ALREADY-HOMED` | 10,601 | our name is already on one of the hits; nothing to do |
| `NO-WINNER` | 1,285 | every candidate has at least one content CONFLICT (usually: our source diverges, so the true home is not in the hit set at all) |
| `TIE` | 137 | ≥2 candidates satisfy the content — refused by rule |
| `DROP-CONTESTED` | 33 VAs | one retail VA claimed by ≥2 different mangled names. These are *genuine* ICF folds (`?ByteCode@SyncAllMsg@@` vs `?StaticByteCode@SyncAllMsg@@`; `vector<int>::reserve` vs `vector<float>::reserve`) where retail really does have one shared function. Left unmapped: the map holds one name per VA and we decline to pick arbitrarily |
| **RESOLVED** | **276** | applied |

Residual pool after this lane: **~19,500 records that no content evidence can
touch** (18,081 NO-EVIDENCE + 1,285 NO-WINNER + 137 TIE), plus the 33 contested
ICF folds. Attacking `NO-EVIDENCE` needs a genuinely different discriminator —
`.pdata` prolog/epilog shape, or caller-side identity (who calls this VA), not
callee-side.

## ★ Map trust audit — 423 existing entries are mispaired

`multi_content_disambiguate.py --trust-audit` re-checks the **existing** map
against map-free content: for every function whose name already sits on one of
its byte-identical hits, does that VA actually reference our strings/constants?

```
10,659 names checked -> 2,191 corroborated, 423 CONTRADICTED, rest no content
```

Those 423 currently read as clean 100% matches, because objdiff's normalized
diff masks relocations — precisely the failure mode round 4 warned about,
already present in the tree from earlier lanes. Artifacts:

* `docs/plans/laneG-map-trust-contradicted-2026-07-24.json` — all 423 names + VAs.
* `docs/plans/laneG-map-mispair-audit-2026-07-24.json` — the 95 for which content
  determines the correct home. **19 are cleanly fixable** (correct VA is
  currently unmapped); the other **76 are a rotation** — the correct VA is held
  by another (also-wrong) entry, e.g. `CamShot::StaticClassName` should be at
  `0x824bd6c0`, which currently claims to be `RndMovie::StaticClassName`. The
  family is overwhelmingly `StaticClassName` / `Type` local-static-Symbol
  functions, which are byte-identical apart from the string pointer.

Repairing these is a **permutation problem, not a lookup** — it needs a
coordinated multi-entry swap (`invcorr_mispair_repoint.py` territory) and was
left for a follow-up lane, since a partial repair can strand matches. It does
not change the score either way (a mispair and its repair both read 100%), but
it makes the number honest and un-breaks the `sym` evidence class for everyone.

## Re-run recipe

```bash
WT=/home/free/tmp/wt-laneG-multi; L=~/tmp/laneG
# 1. homing scan over all objs, 40 per invocation (MINSZ=32, funclets OFF)
HOMING_NO_DEFAULTS=1 HOMING_WT=$WT HOMING_ROOT=$WT \
HOMING_TMAP=$WT/scripts/target_symbol_map.json HOMING_OUT=$L/scan/bNNN.json \
  python3 scripts/harvest/homing_scan.py key=/abs/a.obj ...
# merge the per-batch jsons -> $L/merged.json, then:
# 2. audit the map, so callee evidence only uses corroborated entries
python3 scripts/harvest/multi_content_disambiguate.py --results $L/merged.json \
  --worktree $WT --trust-audit --trust-out $L/trust.json
# 3. measure precision on held-out ground truth BEFORE trusting the output
python3 scripts/harvest/multi_content_disambiguate.py --results $L/merged.json \
  --worktree $WT --validate --trust-file $L/trust.json
# 4. propose, then gen/apply exactly like round 4 (--reveal-frag is new)
python3 scripts/harvest/multi_content_disambiguate.py --results $L/merged.json \
  --worktree $WT --trust-file $L/trust.json --out $L/prop.json
python3 scripts/harvest/homing_gen4.py --results $L/prop.json --worktree $WT \
  --out-prefix $L/g --reveal-frag $L/g_reveal.json
python3 scripts/harvest/homing_apply4.py --blocks $L/g_blocks.json \
  --frag $L/g_map_fragment.json --worktree $WT --units-file $L/units.txt \
  --out-frag $L/frag.json
python3 scripts/harvest/tu5_map_apply_fragment.py $L/full.json \
  $WT/scripts/target_symbol_map.json
touch config/45410914/config.yml && rm -f build/45410914/report.cache && ninja
```

Repeat 2→4 until a wave yields 0 (the map grows → the trust set grows → a few
more resolve). It converged in three waves here.

## Negative results — do not redo

* **Ungated `sym` evidence.** 75.7% precision. Its errors are *silent* mispairs.
  Always `--trust-file`.
* **`--min-sym-agree 2`.** Correct (100%) but kills 244 of 247 decisions —
  sym-class functions almost always have exactly one trusted callee.
* **Round-5 plain-UNIQUE re-harvest.** Re-scanning all 914 objs found 211
  distinct plain-UNIQUE VAs left from round 4, but gen4 legitimately drops
  essentially all of them (125 already covered with names in map, 63
  name-ambiguous, 22 name-already-in-map) — net 3 map entries, +0 strict. Round
  4 really did drain plain-UNIQUE. Don't re-scan until objs change.
