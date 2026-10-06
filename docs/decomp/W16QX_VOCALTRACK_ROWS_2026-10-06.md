# W16-QX: VocalTrack / VocalTrackDir sub-100 rows (2026-10-06)

Branch `w16-qx`, off main `1fe4ab41c`. Brief: bring the sub-100 rows in the
`VocalTrack` and `VocalTrackDir` units to 100 by fixing source against retail
bytes. Start with `UpdateScrolling` (8,948 B at 96.97), `RebuildHUD` (99.96)
and `VocalTrackDir::PostLoad` (99.34).

## Result

Measured with `tools/ab_measure.py --patch` on the whole lane diff (code, map,
splits, aliases) in a clean worktree at `1fe4ab41c`. The ruler is `name_check`.
Both legs were settled and read at a split fixed point.

```
leg A: matched=54676 honest=29474 code%=58.602478
leg B: matched=54680 honest=29478 code%=58.660637
Δmatched=+4  Δhonest=+4  Δcode_bytes=+5960  Δcode%=+0.058159pp
units: UILabel +2, VocalTrackDir +2; units at 100 582 -> 582
```

Every changed row is listed below. Their sizes sum to exactly +5,960 B.

| row | size | leg A fuzzy | leg B fuzzy | how |
|---|---:|---:|---:|---|
| `VocalTrackDir::PostLoad` | 3,656 | 99.338 | **100** | `gRevs` made a file-static aggregate |
| `VocalTrack::RebuildHUD` | 2,188 | 99.963 | **100** | widen loop through a `RangeShift &` |
| `fn_822FA7B8` (VocalTrackDir) | 40 | 99.5 | **100** | `_M_erase` alias, proven |
| `fn_825971C8` (SongSortMgr) | 40 | 99.5 | **100** | same alias (collateral) |
| `RndTransformable` vtordisp `Replace` / `Print` thunks | 16 + 16 | unpaired | **100** | named and re-homed to UILabel |
| `map<int,Color>::~map` (0x822FC4F8) | 4 | unpaired | **100** | named |
| `VocalTrackDir::SetRange` | 700 | 93.709 | 95.057 | re-read `Mat()` for the write-back |

The first A/B (before the destructor alias) read **+5,696 B**, 224 B under the
+5,920 B prediction. The shortfall was the cost of naming 0x822FC4F8: six 44-byte
catch funclets (FocusTracker `fn_826D87A4`/`fn_826D8980`, Font
`fn_82473C54`/`fn_8247423C`, OverdriveTracker `fn_826DE21C`/`fn_826DE30C`)
went from 100 to 99.545. Retail calls `bl 0x822FC4F8` there, and our objs spell
each instantiation's own `map<…>::~map`. Before the naming, those call sites were
forgiven placeholders; after it, they were checked. That is the documented
"naming is a bet" mechanism. The fix proved the three folds rather than reverting
the name (commit `e0d36a5b4`), and the six funclets returned to 100. The second
A/B came out at +5,960 B, which is +5,696 B plus the recovered 264 B.

**Behaviour.** None of the kept source changes alters behaviour:
- `PostLoad` changes linkage only.
- `RebuildHUD` computes the same values in the same order.
- `SetRange` calls `Mat()` twice instead of caching it, and `Mat()` is a plain getter.

No behavioural defect was found in the rows that remain below 100. Each residue
below is scheduling or register assignment over semantically identical code.

## Per-row record

### `VocalTrackDir::PostLoad`: 99.34 → 100 (`c7c05ca10`)
`gRevs` was a public static member of `VocalTrackDir`, which gives it external
linkage. Retail hoists the vbptr load above the two rev stores, and the compiler
can only do that if the stores provably cannot alias the object. It can prove
this for an internal-linkage object. The fix moves `gRevs` out of the header and
makes it a file-static anonymous struct in `VocalTrackDir.cpp`, the same idiom as
`BandConfiguration.cpp` / `BandDirector.cpp`. The header keeps a comment pointing
to it.

### `VocalTrack::RebuildHUD`: 99.96 → 100 (`dbebf580f`)
In the widen loop, the commutative operand order of `rs.unk4 + (maxRange - rs.unk8)`
only came out right when the loop binds `RangeShift &rs = *it`.
These spellings were inert or worse:
- operand swap
- a `half` local
- `-=`/`+=` through `it->` (forces reloads)
- the reference-free form (reloads)

### `VocalTrackDir::SetRange`: 93.71 → 95.06 (`e196f55ca`); left below 100
The texture-transform write-back now calls `mPitchWindowMesh->Mat()` again
instead of reusing a cached `RndMat *`. Retail recomputes `&mat->mTexXfm` for the
second copy. The remaining residue is the position of `(60 - min)` relative to the
pitch-range computation. These spellings were inert or worse:
- an oracle-style reference copy (93.62)
- a const getter
- direct `mTexXfm` access through a temporary friend (reverted)
- explicit `memcpy`
- three operand orders for the `mMiddleCZPos` expression
- an inline member expression

### `VocalTrack::UpdateScrolling`: 96.97, unchanged
There are 23 mismatch clusters. Most are the scheduling of inlined
`deque::size()` and deque-iterator copies; the rest are small loop-schedule
clusters. One idea was to spell the Vector3 x component late (idx 2027). It
produced byte-identical output. This row has a long prior history (W16-FS,
W16-GB, about 20 commits), and nothing here moved it. It was not funded further.

### `VocalTrack::UpdateTambourineGems`: 87.22, unchanged
Every mismatch is inside the inlined `deque::size()` / iterator-copy code.
- Three term orders and nestings of `_Deque_iterator_base::_M_subtract` in
  `_deque.h` were fully inert, for this row and for `UpdateScrolling`. `_deque.h`
  was restored.
- `TambourineGemPool::SetGemState` (retail 0x82BA29A0) is a 100% row. It inlines
  the same STLport code and emits a different add order. **The order is therefore
  decided by the calling context, not by the header.** This refutes the
  attribution to the STLport source in the earlier lane; a `_deque.h` edit cannot
  close these rows.
- A const-reference `gems` local was inert.

### `VocalTrack::ProcessStaticLyrics`: 97.69, unchanged
After `f3 = f7`, retail reloads f3, whereas we forward the stored value, and the
register assignment differs. Attempts:
- moving `l5 = nullptr` after the store (94.62)
- `d2 = f4; d2 -= f3` (97.69, inert)
- `-(f3 - f4)` (96.47)

### `VocalTrack::PrepareNoteTubes`: 99.93 (mpn 100), unchanged
There are two integer `add` operand swaps (base + offset). Three index spellings
for `notes->mNotes[combineNote]` were inert.

### `VocalTrack::BuildScrollingDeployZones`: 99.93, unchanged
Retail bases the induction pointer on `mAlternateNoteList` (+0x2d4) and reaches
`mNextDeployZone` at −0x1ac; ours does the reverse. An `int &dz` reference was
inert.

### Map, splits and alias changes (`295978d78`, `e245d9d30`, `e0d36a5b4`)
- `target_symbol_map.json` (via `tools/gated_map_write.py`, gate passed) gained
  three names:
  - 0x822FC4F8 `map<int,Hmx::Color>::~map`
  - 0x827F4298 `RndTransformable::Replace` vtordisp thunk (`$4PPPPPPPM@BDM@`:
    vtordisp −4, adjustment 0x13C)
  - 0x827F42C8 `RndTransformable::Print` vtordisp thunk
- `splits.txt`: the two thunk ranges moved from the `VocalTrackDir.cpp` heading
  to `UILabel.cpp`, the base obj that defines them. This is a re-homing, so it is
  not metric-neutral; it is what made the rows pairable.
- `symbol_aliases.json` gained two groups. Both were adjudicated with
  `tools/icf_pair_adjudicate.py --chase`, and each has a retail call-site witness:
  - 0x82768F98: `_Rb_tree<int,float>::_M_erase` folded onto the `<Symbol,float>`
    survivor. CHASED T1 PROVEN, 92/92 B, one retail body-twin. Witness: retail's
    catch funclet `fn_822FA7B8` calls 0x82768F98.
  - 0x822FC4F8: the `map<unsigned short,RndFont::CharInfo>`,
    `map<TrackerPlayerID,int>` and `map<TrackerPlayerID,OverdriveTracker::DeployData>`
    destructors folded onto `map<int,Color>::~map`. CHASED T1 PROVEN. Flat T1 is
    vacuous for a 4-byte tail-branch thunk; the `clear` → `_M_erase` chain
    slot-folds.
  - `icf_alias_finder.py --validate` PASS (0 contradicted);
    `alias_survivor_drift.py` OK.

## Deliberately not done
- No permuter (standing directive).
- No `_deque.h` change: it was measured inert above, and the 100% `SetGemState`
  row shows the header is not the lever.
- No further `UpdateScrolling` grind beyond screening its clusters.

## Gate
`tools/native_build_gate.sh` in the lane worktree after the source commits.
`src/` has not changed since; only the alias commit and this doc followed:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

The gate is re-run as the lane's last action after this doc is committed.
