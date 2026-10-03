# W16-OU: retail homes for W16-OS's withdrawn and undecided alias memberships (2026-10-03)

Branch `w16-ou`, off main `7d4a63494` (the W16-OS merge). Ruler `name_check` (graded).
Follows the "Found, deliberately not done" list of
[W16OS_ALIAS_SURVIVOR_RELABEL_2026-10-03.md](W16OS_ALIAS_SURVIVOR_RELABEL_2026-10-03.md) §7.
Tool: `tools/alias_locate_home.py` (dry run by default; `--write`; `--null-interior-rows`).

## 1. Result

| item from W16-OS §7 | outcome |
|---|---|
| hashtable bucket-count ctors withdrawn at `0x825a07e0` | **25** of 25 located and admitted: 22 → `0x8255c968`, 2 → `0x822788a8`, 1 → `0x8256b298` |
| BandCamShot spelling `HamCamShot::Target` | gone from `BandCamShot.obj` (0 `HamCamShot` bytes): the DC3 `hamobj/HamCamShot.cpp` scatter-include is removed |
| four folded spellings the map placed at another address | the four map rows point **inside other functions**; nulled with a record. The folds stand |
| 32 CALLEE-UNANCHORED memberships | **13 REFUTED** (our callee located at a different retail address); 11 re-homed to where the member itself is located; 18 still undecided |
| 35 held old labels | 3 refuted and re-homed (a subset of the 13); the other 32 have nothing retail bytes can compare (§5.3) |

W16-OS §7 said 23 hashtable ctors. The population at `0x825a07e0` is 24 folded members withdrawn
as `STALE_SURVIVOR_RECHASE_REFUTED` plus the old survivor label `hashtable<Symbol,float>`
(`FORMER_SURVIVOR_LABEL_REFUTED`), so 25.

Whole-binary A/B, the whole branch as one patch against main `7d4a63494`:

```
python3 tools/ab_measure.py --worktree ~/tmp/wt-w16ou-ab --patch <git diff main..w16-ou> --label w16ou-branch
run dir ~/tmp/wt-w16ou-ab/.ab_measure_runs/20261003-052224-w16ou-branch-2995450, rc=0, kinds [map, source]
leg A: matched=53269 masked=25144 honest=28125 code%=57.384968  (recompiles: 0, settled)
leg B: matched=53269 masked=25144 honest=28125 code%=57.384968  (recompiles: 4, split=1, settle iterations: 2)
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0  Δfuzzy=+0.000000pp
units at 100%: mpn 524 -> 524, all-rows-fuzzy 463 -> 463
```

**Prediction, written before the run:** Δmatched in [−2, +4], |Δbytes| < 2 kB. A should be 0 (every
caller of the bucket-count ctors is an unpaired `hash_map` ctor), B 0 (no paired function of ours
calls a `HamCamShot` spelling), C 0 (the nulled names were never applied to a dtk symbol), D able to
move rows both ways. Measured: 0 on every key. Leg B recompiled 4 TUs, so the source half was not
absent-vs-absent.

### 1.1 Every row that moved

A per-row diff of the two archived leg reports (68,913 rows each, none only in one leg) shows **one**
changed row:

| row | before → after (fuzzy / mpn) | B | why |
|---|---|---:|---|
| **down:** MeshAnim `vector<Key<vector<Color>>>::_M_insert_overflow_aux` | 99.753 → 99.691 (both rulers) | 324 | the carried label `__uninitialized_fill_n<Key<vector<Color>>>` was **withdrawn** at `0x8246f0c0` |

It was below 100 before and after, so it costs 0 functions and 0 bytes. The site at index 56 calls
`0x8246f0c0` in retail and `__uninitialized_fill_n<Key<vector<Color>>>` in ours, and only the
carried label forgave it. The label is REFUTED there: retail's body at `0x8246f0c0` calls the
`vector<Extent>` copy ctor at depth 2, and ours calls `vector<Color>`'s, which chases clean only at
`0x823e4630`. Our `fill_n<Key<vector<Color>>>` chases clean at `fn_8246F020`, where it is now
admitted. The other four charged sites in the same row also name the `Key<vector<Vector2>>` family
on the retail side. So the retail body under this map name calls Vector2-family callees throughout,
and the map name itself is a suspect (§6).

## 2. A: the hashtable bucket-count ctors

### 2.1 The body cannot place them

`locate_retail` finds each of the 25 spellings chasing clean at **five** retail addresses:
`0x822788a8`, `0x8255c968`, `0x8256b298`, `0x825983d8` and `0x825af728`. The five retail bodies are:

- 120 B each, with the same masked bytes;
- identical in **every relocation target**: `lbl_820009FC` (the `1.0f` load factor) twice, and
  `_M_initialize_buckets` at the one address `0x8256ad18`;
- without EH funclets, and so are ours, so `.xdata` cannot be what keeps them apart.

Retail kept five copies of one body. A body-equality instrument cannot say which copy is a given
type's; picking one is the coin flip `icf_pair_adjudicate.uniqueness` warns about.

### 2.2 Retail's call sites can

Each bucket-count ctor has exactly one caller of ours, its `hash_map<K,V>()` default ctor. That
caller is unpaired, because retail folded it. So the walk goes one level higher, to the class ctors
that hold the maps (`AccomplishmentManager`, `SongMgr`, `LessonMgr`, `BandProfile`, …). Those are
paired. From there it reads retail's relocations back down:

1. The paired caller's relocation at the slot where ours names `hash_map<K,V>()` gives a retail
   `hash_map` copy.
2. That copy's body equals ours (masked bytes and relocation shape). Its relocation at the slot where
   ours names the bucket-count ctor gives the address.

Every path for a spelling agrees on one address. Each admission was then chased with
`chase(retail name there, ours)` and is PROVEN with 0 cycle-assumed.

| address | map name there | admitted |
|---|---|---:|
| `0x8255c968` | `hashtable<pair<const Symbol,int>>` bucket-count ctor | 22 |
| `0x822788a8` | `hashtable<pair<const Symbol,vector<PatchSticker*>>>` bucket-count ctor | 2 (`vector<int>`, `vector<Symbol>` values) |
| `0x8256b298` | `hashtable<pair<const int,String>>` bucket-count ctor | 1 (`<Symbol,String>`) |

`CampaignKey` and `CampaignLevel` reach their address only through `Campaign::Campaign`, whose body is
4 B shorter than retail's (396 vs 400 B). Both bodies carry 59 relocations. The two `hash_map` slots
(offsets 152 and 192) have the same type on both sides, and retail names `hash_map<Symbol,int>()` at
both. That retail copy (`0x8255d480`) equals ours byte for byte and calls `0x8255c968`. The tool
labels such a path `OFFSET-ALIGNED`, and these two are the only admissions that rest on it alone.
`hashtable<Symbol,Symbol>` takes the same path and also has three fully PAIRED paths. All of them
agree.

These 25 forgive nothing today, because their only callers are unpaired. The value is that each now
sits at the address retail calls it at, instead of nowhere.

## 3. B: BandCamShot without HamCamShot

`BandCamShot.cpp` scatter-included the whole DC3 `hamobj/HamCamShot.cpp` ("COMDAT-scatter wave 3",
`69b4a87c2`, 07-19). That bought `GetNumShots`, `GetTotalDurationSeconds` and `Store@Target` through
`HamCamShot` spellings at a time when our own BandCamShot did not yet produce them. Measured on this
tree before removing it:

- **0** retail rows (and 0 map rows) are named `HamCamShot`;
- **0** paired functions of ours reference any `HamCamShot` spelling;
- our BandCamShot already defines every `BandCamShot::Target`/`TargetCache` body retail names.

The `_Destroy<HamCamShot::Target>` reference W16-OS saw came from objdiff pairing retail's three EH
funclets (`fn_822B60A4`/`6764`/`70A4`) by byte signature with a HamCamShot-spelled funclet. On this
tree they already pair with our BandCamShot funclets (`bl ??1Target@BandCamShot@@QAA@XZ`, 100%).

The include is replaced by a comment. `BandCamShot.obj` now contains 0 `HamCamShot` bytes. The
`#undef gRev/gAltRev` that preceded it is kept, because code below depends on it. The A/B above
includes this change. `hamobj/HamCamShot.cpp` is still its own compile-only TU, so the 16 alias
groups that carry HamCamShot folded spellings stay `MAP-CONSISTENT`, and none was touched. The only
source file changed is `src/system/bandobj/BandCamShot.cpp`.

## 4. C: the four folded spellings placed elsewhere by the map

| group (folded at) | spelling | map row | retail word at the row | refs to the row | refs to the group address |
|---|---|---|---|---|---:|
| `0x8237f158` | `__uninitialized_copy<CharBones::Bone*>` | `0x8245ee08` | `0x4182000c` (`beq`, mid-function) | 0 | 7 branches |
| `0x826c3888` | `??3@YAXPAX0@Z` | `0x82333e14` | `0x4e800020`, the `blr` of a `li r11,0; clrlwi r3,r11,24` body | 0 | 1,116 branches |
| `0x826c3888` | `TrigTableTerminate` | `0x824f4dc4` | `0x4e800020`, a `blr` inside a float routine | 0 | 1,116 branches |
| `0x8240ddb0` | `??3RndLight@@SAXPAX@Z` | `0x8270d7f8` | `0x4bffecb8`, the tail `b` of a body starting earlier | 0 | 2,308 branches |

"Refs" means all four channels of `icf_pair_adjudicate.retail_refs_to`: relative branches, aligned
data words, `lis`/`addi` pairs, and `.pdata` BeginAddress. No dtk symbol starts at any of the four
rows. Each row names a point inside another function that nothing references. The rows date to the
TU0→TU5 address re-point (`a320bc121`) or the round-4 scanner stack (`13fe51646`). They are nulled,
with `_w16ou_interior_rows_comment`. The folds at the group addresses were already chased PROVEN:
`__uninitialized_copy<Bone*>` 56 B = 56 B, and the other three are `VACUOUS-BUT-IDENTICAL`, which is
the right class for a `blr` or a `b X`. The nulled names were never applied to a symbol (none is a
dtk function start), so the renamer output does not change.

## 5. D: the 32 CALLEE-UNANCHORED memberships and the 35 held labels

### 5.1 Rule

Re-chase the membership. At the depth-≥1 `BYTES-DIFFER` leaf, retail calls `rn` and ours calls `on`.
Locate `on` in retail:

- **BODY:** `locate_retail(on)` returns exactly one address;
- **CALL-SITE:** every retail call site of `on` that the walk can reach agrees on one address Y, and
  retail's body at Y differs from retail's body at `rn`.

The second condition matters. Without it, Y and `rn` could be two copies of one function, as §2.1
shows. If `on` lives somewhere other than `rn`, retail holds both callee bodies and they differ. Then
the member's retail body is not the one at the group address: **REFUTED
(`CALLEE-LOCATED-ELSEWHERE`)**. The member is then located the same way and re-homed. A
call-site-only home also needs `chase` PROVEN with 0 cycle-assumed. If neither instrument returns a
single address, the membership stays as it was and the attempt is recorded in `rechase_w16ou`.

### 5.2 Outcome

| W16-OS disposition | n | REFUTED | re-homed to | unchanged |
|---|---:|---:|---|---:|
| folded (carried: the map name was already in the bucket) | 18 | 10 | 8, all BODY: `0x82276d58`, `0x82698540`, `0x827c6708`, `0x828070e0`, `0x824ce758`, `0x8246f020`, `0x82347770`, `0x825a8b98` | 8 |
| held as a record | 10 | 3 | 3 → `0x823d14c0` (`insert<list<RndAnimatable*>>`, `<RndDrawable*>`, `<unsigned>`) | 7 |
| old label, the map name at another address | 3 | 1 (`insert<list<EventTrigger::ProxyCall>>`, record only; it is located at its own map address `0x824a07a0`) | none | 2 |
| folded (`_Copy_Construct<MsgSinks::Sink>` at `0x822c9048`) | 1 | 0 | none | 1 |

Two of the refuted carried labels are withdrawn without a new home. `clear<Rb_tree<Symbol,int>>` at
`0x82597098`: retail's `~SongRecord` calls its `clear` at `fn_827690D0`, whose body equals ours and
calls `_M_erase` at `0x82768f98` (88 B), not the group's `0x825962a8` (92 B). But
`chase(fn_827690D0, ours)` does not prove, so it is not admitted there. `_Destroy_Range<Flow::DynamicPropertyEntry>`
at `0x823a94f0`: retail calls our callee's body at `0x822a4060`, which differs from the group's
`~CharHair::Strand`. The member itself is not located.

Seven located callees of ours are admitted at their BODY address, each chased clean: four
`_M_create_node<list<T*>>` → `0x82520150`, `__linear_insert<MoveDetector**>` → `0x82347030`,
`_M_rehash<hashtable<Symbol,DataArray*>>` → `0x825d2988`, and `_M_create_node<Rb_tree<int,Symbol>>`
→ `0x82768ff8`. The other located callees are skipped because they are already members elsewhere or
are map names.

The 18 unchanged rows have no paired caller of our callee within four call levels, and no unique body
match. Five of the 18 have a DC3-only class on our side (`Flow::DynamicPropertyEntry` ×2, the
`FlowSwitch` and `FlowNode` callees of `FlowWhile`/`FlowValueCase`, and `HamCharacter`), and two are
`HamCamShot::ListNextShots`.

All admissions (25 from A, 15 from D) were re-chased with the CLI,
`icf_pair_adjudicate.py --chase --pairs`: **40 / 40 `CHASED T1: PROVEN`, 0 `CYCLE-ASSUMED`, 0
undischarged or contradicted slots.**

### 5.3 The held labels retail bytes cannot settle

Of the 35 held labels, 10 were CALLEE-UNANCHORED (3 resolved above, 7 not). The other 25:

- **17 `MISSING(ours)`**: we compile no body for the spelling (DC3 `ObjRefConcrete<X>` dtors,
  `Key<float>` heap-sort helpers, `QuatKeys::Load`, …). There is nothing to compare against retail.
- **3 `SLOT-UNDISCHARGED`**: our vtable is `ObjRefConcrete<X>`, a class with no retail RTTI.
- **5 `VACUOUS` / `VACUOUS-PLACEHOLDER-SLOT`**: there is too little body to locate.

## 6. Found, deliberately not done

- **The MeshAnim row in §1.1.** Retail's body under the map name
  `_M_insert_overflow_aux<Key<vector<Color>>>` calls `Key<vector<Vector2>>` callees at all five
  charged sites. The map name may be the wrong instantiation. This needs an adjudication of that row,
  not alias work.
- **Other interior map rows.** C checked only the four rows that collided with a fold. Nothing yet
  scans the map for rows whose address has 0 references and no dtk symbol start.
- **The 18 unlocated rows and the 17 `MISSING(ours)` held labels.** These need a port of the callee
  (or of the member), or a paired caller to appear.
- **`fn_827690D0`** (`clear<Rb_tree<Symbol,int>>` by call site) does not chase. Its callee
  `_M_erase<Symbol,int>` sits at `0x82768f98` under the name `_M_erase<Symbol,float>`, 88 B.
- **Coordination.** Lane W16-OT is changing class headers in band3/system. This lane's only source
  edit is `BandCamShot.cpp`, and it touches no header.

## 7. Gates (worktree at the branch tip, after a full `./tools/ninja-locked`)

- `tools/alias_survivor_drift.py`: **OK**, 2,078 placed groups. The build edge also passes.
- `icf_alias_finder.py --validate`: **PASS**, 1,897 map-consistent / 224 tolerated / **0 contradicted
  (SURVIVOR_DRIFTED 0)** / 2,121 groups. The 12 new groups: 8 named, 4 with a `fn_` placeholder
  survivor (`TOLERATED PLACEHOLDER_SURVIVOR` 39 → 43).
- `map_name_injectivity.py`: OK, 35,152 applied rows, injective (+1 enumerated exception).
- `gen_symbol_alias_map.py --check`: OK.
- `tools/test_alias_survivor_drift.py`: 14 passed.
- `icf_pair_adjudicate.py --chasetest`: selftest PASSED.
- `scripts/test_tools.py`: **RESULT: PASS** (new=0 timeout=0 broken=0 script-fail=0 uncovered=0
  hollow=0 stale=0 known-bad-hit=0).
- Native gate: §8.

## 8. Native gate (run last)

`tools/native_build_gate.sh`, run last on the tip `f97c18476` (only this doc line follows it):

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

No commit carries a co-author line.
