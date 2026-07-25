# Identification discriminators beyond callee-side content (lane K, 2026-07-25)

Branch `laneK-discrim`, from `ca711730` (27,223 strict).
**Result: 27,223 -> 27,346 = +123 strict, 0 LOST at every step.**

## The problem this lane was given

The identification stack homes one of our compiled functions on a retail VA by
**reloc-masked byte identity** (`scripts/harvest/homing_scan.py`).  When several
retail VAs are byte-identical the scan reports `MULTI` / `UNIQUE-ICF` and
refuses.  `multi_content_disambiguate.py` cracks part of that residue by reading
what the *callee itself* references through the masked slots -- strings,
`__real@` float constants, trusted callee names.

What it cannot touch is the pool with nothing to say about itself: **18,081
NO-EVIDENCE** records (no string, constant or trusted callee at any masked slot)
and **1,285 NO-WINNER** records.  Lane G's verdict was that cracking those needs
a different discriminator -- `.pdata` prolog shape, or caller-side identity.

This lane built and *measured* those discriminators.

## Scoreboard

| discriminator | held-out precision | reach | verdict |
|---|---|---|---|
| **caller-side call-graph inversion**, sibling-family <= 16 | **99.09 %** (542/547) | 173 of 5,177 unhomed target names in round 1; 199 over the fixed point | **FUNDED — landed +123** |
| caller-side inversion, no family cap | 97.91 % (1032/1054) | 357 | measured, **not funded** |
| caller-side inversion, sibling-family 17+ only | 96.65 % (490/507) | 3,843 refused | **not funded** — this bucket carries almost all the error |
| `.pdata` prolog shape (PrologLen / 32-bit / EH flag) | n/a | varies in **18 of 33,714** hit sets (0.053 %) | **DEAD — do not rebuild** |
| call-graph shape (out-degree, leaf/non-leaf, callee count) | n/a | 0 by construction | **DEAD — do not build** |
| neighbourhood fingerprints (`prev4`, alignment, `.pdata` gap) | n/a | varies in ~95 % of sets, isolates the truth in 53.6 % | **UNUSABLE — no key on our side** |
| `--min-anchors 2` | 97.84 % (272/278) | -74 % yield | no precision gain, dropped |
| `--strict-anchors` (anchor must be unambiguously homed) | 97.21 % (558/574) | -46 % yield | **worse than baseline**, dropped |
| `--content-guard` on the derived VA | rejects 41, of which **1** was actually wrong | — | 40:1 false-positive, **off by default** |

Precision is measured held-out, the same way `multi_content_disambiguate.py
--validate` does it: restrict to functions whose retail home is already known,
feed in **all** byte-identical candidates, score the pick.  The candidate set is
rebuilt to mirror production exactly (all *unmapped* hits, plus the incumbent,
since in production both the name and its home would be unmapped), and the
contested-drop pass runs before scoring.  The resolver never learns which
candidate is the truth.

## Why `.pdata` prolog shape is dead (the brief's discriminator #1)

`scripts/harvest/pdata_shape_probe.py` decodes the Xbox 360 `RUNTIME_FUNCTION`
bitfield (`PrologLen` bits 0-7, `FuncLen` bits 8-29, `ThirtyTwoBit` bit 30,
`ExceptionFlag` bit 31) for every candidate in every hit set and asks whether it
*varies within the set*.

    pdata/constant : 33696
    pdata/varies   :    18

It is constant in 99.95 % of hit sets, and the reason is structural rather than
incidental: **the candidates are byte-identical by construction**.  A hit set is
built out of functions whose masked machine code is equal, so their prologs are
equal, so their unwind records are equal.  The same argument kills every probe
computed *from the function's own bytes* — call-graph shape, out-degree,
leaf/non-leaf, recursion, frame size.  None of these can ever separate a hit set.
Do not rebuild any of them.

The same probe shows the *surrounding* fingerprints do vary (`prev4` in 31,900 /
33,714 sets, and it would isolate the truth in 6,104 of 11,383 measurable sets).
They are still unusable: they describe the retail neighbourhood, and our
COMDAT-per-function objects have no corresponding key to compare against.  A
fingerprint with no counterpart on our side is not evidence.

29 % of the residual (8,752 records) is exception-flagged, so the word preceding
the function is a `__ehhandler$` pointer.  That chain
(`handler -> scope table -> type descriptors`) *does* carry real content — RTTI
class-name strings — and our objs have the matching `__ehhandler$<F>` and
`??_R0?AV<Class>@@@8` symbols.  That is the one unexplored content channel left;
it is not built here.

## What does work: caller-side call-graph inversion

`scripts/harvest/caller_side_invert.py`.

Masking destroys the *callee's* identity, but our object files still record, for
every call site, which symbol the call points at.  So:

> if a caller `G` is confidently homed at retail `VA(G)`, and our `G` has a
> relocation at offset `o` naming callee `F`, then the retail instruction at
> `VA(G)+o` decodes to `F`'s retail address.

That is a **derivation, not a filter** — one homed caller determines the callee's
VA outright — and it needs zero content in `F`.  Of the 123 resolutions landed,
**120 are content-side NO-EVIDENCE** and 3 are TRUTH-AGREE; i.e. this lane's
output is almost entirely disjoint from what the content resolver can reach.

The tool is built around one global table rather than per-function heuristics:

    claims[v] = { every callee symbol name that some anchor's relocation names
                  at a slot where retail resolves to VA v }

with six guards:

1. **Anchor verification** — `G` is an anchor only if its name maps to exactly
   one VA *and* our compiled `G` is reloc-masked byte-identical to retail there.
   (13,401 anchors out of 440,521 compiled functions.)
2. **Unanimity** — all anchors naming `F` must resolve to the same VA.
3. **Exclusivity** — `claims[v] == {F}`.  If any other callee name also resolves
   to `v`, that VA is an ICF fold (or an anchor is mispaired) and *both* names
   are refused.  This is the guard the naive formulation lacks.
4. **Hit-set containment** — `v` must be one of `F`'s own byte-identical
   candidates.  A vote from a mispaired anchor lands somewhere that is not
   byte-identical to our `F` and dies here.  This is the guard that makes the
   derivation safe.
5. **Unclaimed** — `v` must not already be mapped, and no two functions may
   derive the same `v` in one wave.
6. **Sibling-family cap** — refuse when more than N (default 16) of *our own*
   compiled functions share `F`'s reloc-masked bytes.

### The sibling-family cap is the whole precision story

`famsize[F]` = how many distinct names of ours compile to `F`'s exact masked
bytes.  Precision by bucket:

    family 1     : 38/38   = 100.00 %
    family 2-4   : 289/292 =  98.97 %
    family 5-16  : 215/217 =  99.08 %
    family 17+   : 490/507 =  96.65 %     <- 17 of the 22 errors live here

Large families are template/STL instantiation swarms (`_M_splice_insert_dispatch`,
`_Param_Construct`, `ObjDirItr<T>::operator++`, `StaticClassName` — that last one
has 560 members).  Retail ICF-folds *some* members of such a swarm, which makes
"who calls it" genuinely ambiguous: two different instantiations legitimately
resolve to one address.  Capping at 16 costs 3,843 candidate names and buys the
error rate down from 1 in 48 to 1 in 109.

### The measured error rate is a lower bound

Of the 22 disagreements at no cap, the incumbent map is provably the wrong one in
several: for 3 of them map-free content **corroborates our pick and contradicts
the incumbent** (`StaticClassName@DxParticleSys`, `@NgDOFProc`, `@NgLight` — the
`StaticClassName` family the repair lane was built for), and 1 more is a mutual
error.  Those are scored as misses here.  The honest statement is *"99.09 %
measured against an incumbent map that is itself known to contain 385
content-contradicted entries"* — not that 5 of 547 picks are wrong.

### Fixed point

Resolutions from round N become anchors in round N+1 (`--iterate`).  It converges
in 3 rounds and is worth about +15 % yield (173 -> 199).

## The span predictor — required before any wave

`scripts/harvest/span_predictor.py`.  A repair lane learned that pointing a name
at its true home only pays when that home is pinned **and** inside the span of
the unit that compiles the symbol; otherwise the previous pairing was a false
match and the repair costs it.  The predictor classifies every proposal:

    PAYS        pinned, to the unit that compiles the symbol -> map entry alone
    WRONG-UNIT  pinned, but to a different unit -> objdiff never pairs them
    UNPINNED    in no .text range -> needs a splits pin (homing_gen4/apply4)

On wave 1 it predicted 49 PAYS; 42 survived the map's name-collision check and
**all 42 converted** (+42, 0 lost).  On wave 2, 81 pins were generated and **all
81 converted** (+81, 0 lost).  100 % conversion on both waves — the predictor is
exact, and it should be run before every future identification wave.

## What landed

| wave | mechanism | applied | delta | lost |
|---|---|---|---|---|
| 1 | caller-side, PAYS tier — map entries only | 42 | +42 | 0 |
| 2 | caller-side, UNPINNED tier — 76 splits ranges over 43 units + 81 map entries | 81 | +81 | 0 |
| 3 | re-run at the fixed point | 0 | 0 | 0 |

`multi_content_disambiguate.py --trust-audit` before and after:
**385 contradicted names before, 385 after, 0 new** (and 105 more names became
content-checkable).  No contradiction was re-introduced.

## What remains, and why

Round-1 verdict census over 7,207 target names (5,177 genuinely unhomed):

    BIG-FAMILY   3,843   refused by the family cap (96.65 % if funded)
    NO-ANCHOR      988   no caller of this function is itself homed
    NOT-IN-HITS     84   caller-derived home found, but our body is NOT
                         byte-identical there -- see below
    SHARED-VA       78   two callee names resolve to one VA (ICF fold)
    DISAGREE        11   anchors point at different VAs
    RESOLVED       173

* **BIG-FAMILY** is the biggest reachable pool and the honest decision is to
  leave it.  Funding it would add ~3,800 candidates at a *measured* 1-in-30
  error rate into a map where mispairs are invisible in the score.
* **NO-ANCHOR** shrinks as the map grows; re-run this lane after every wave that
  homes new functions.
* **SHARED-VA / DISAGREE** are structurally unreachable from the caller side:
  when retail folds two instantiations into one address, no amount of caller
  evidence can un-fold them, and the map is 1:1 by construction.

### Byproduct: a precise near-miss worklist

`NOT-IN-HITS` means a homed caller pinpoints `F`'s retail address, but our
compiled body is *not* byte-identical there.  That is an identification with a
known target and a known gap — exactly what a body-port lane wants.  The tool
writes these to `<out>_nonbyte.json` (346 entries at the fixed point).  They are
not strict-matchable as-is and were deliberately not applied.

Also surfaced: 7 caller-derived homes whose *name* is already in the map at a
different VA.  Those are mispair-repair candidates, not inserts, and belong to
`map_rotation_repair.py`'s rotation machinery rather than to a fragment apply.

## Reproducing

```bash
scripts/setup_worktree.sh ~/tmp/wt-K laneK && cd ~/tmp/wt-K
./tools/ninja-locked                                   # objs must be current

# 1. reloc-masked byte-identity scan over every obj (parallel, ~2 min)
#    (see the batch driver in the lane log; HOMING_NO_DEFAULTS=1 + key=path args)

# 2. measure precision before trusting anything
python3 scripts/harvest/caller_side_invert.py --results merged.json \
    --worktree $PWD --validate

# 3. resolve to a fixed point
python3 scripts/harvest/caller_side_invert.py --results merged.json \
    --worktree $PWD --iterate 6 --out prop.json

# 4. only ever apply what the span predictor says can pay
python3 scripts/harvest/span_predictor.py --proposals prop.json \
    --worktree $PWD --out pays.json --only PAYS
#    UNPINNED tier goes through homing_gen4.py -> homing_apply4.py

# 5. map edits are textual, never json.dump
python3 scripts/harvest/tu5_map_apply_fragment.py frag.json \
    scripts/target_symbol_map.json
touch config/45410914/config.yml && rm -f build/45410914/report.cache
./tools/ninja-locked

# 6. re-audit
python3 scripts/harvest/multi_content_disambiguate.py --results merged.json \
    --worktree $PWD --trust-audit --trust-out trust_post.json
```
