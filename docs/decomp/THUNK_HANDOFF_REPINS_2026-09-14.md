# W16-F — W15-D's pin handoff: 23 vtable-proven bodies re-homed, 13 thunk rows closed

**Lane:** W16-F · **Date:** 2026-09-14 · **Branch:** `w16-f` · **Predecessor:** `docs/decomp/THUNK_PERMUTATION_2026-09-14.md` (W15-D)

W15-D proved, name-free, by retail RTTI vtable geometry × `class_layout_report` slot
assignment (721 vtables agreeing slot-for-slot, planted-sabotage control), that 23
addresses are not the functions our map called them. It renamed 18 and left 5 "kept
(at 100; re-pin first)". This lane executes the handoff: the pin moves that let a
correct name pair, the coupled re-pin+rename for the "kept" rows, and the 13 thunk
rows W15-D recorded as "not acted".

**The mechanism throughout.** objdiff pairs target↔base **by name within a unit**. A
pin whose unit's base obj does not define the proven name reads **0 %** however
correct the source is. So a rename without a re-home converts a false 100 into a
false 0, and a re-home without a rename leaves the wrong name in place. The two
edits are only correct **together and in one commit**.

⚠ **Re-homing is NOT metric-neutral** (unlike adding a pin over `auto_*`, which is —
those bytes are already in the denominator). Every move below carries a
pre-registered sign.

---

## 1. Commits

| commit | scope | pre-registered | measured (certified A/B) |
|---|---|---|---|
| `4e337ca1` | 15 `.text` re-homes for rows whose pin was simply in the wrong unit | ≥ 0 for all 15 **by construction** (a row at 0.000 cannot fall) | **+6 fns / +1,648 B / +0.016083 pp** |
| `f46886dc` | the 4 genuine "kept" rows: re-pin **then** rename, same commit | ≤ 0, worst case −4 fns / −368 B | **−1 fn / −176 B** |
| `c4e35f00` | the 13 "not acted" thunk rows: 4 chains rotated, 2 pins moved, 6 pinned over `auto_*` | +12 fns / +144 B | **+13 fns / +156 B / +0.001518 pp** |

Lane total: **+18 fns / +1,628 B** (43,123 → 43,141 matched; 3,925,452 → 3,927,080 B)

---

## 2. The 23 rows proven misnamed by vtable geometry

`old` = the name our map carried before W15-D. `proven` = the vtable-derived name.
`obj` = which of our 1,206 compiled objects defines that symbol (COFF symbol tables
of the **built** worktree — reflinked objs are pre-renamer, so the tree was fully
built before any symbol-name lookup).

### 2a. Re-homed in `4e337ca1` (15 rows, all at fuzzy 0.000 before)

| addr | old → proven | emitting obj | pin move | after |
|---|---|---|---|---|
| `0x822cbc10` | `SetType@StreakMeter` → `PostLoad@BandStarDisplay` | BandStarDisplay.obj | StreakMeter.cpp → BandStarDisplay.cpp | 91.923 |
| `0x8231e660` | `PostLoad@StarDisplay` → `PostLoad@ReviewDisplay` | ReviewDisplay.obj | StarDisplay.cpp → system/bandobj/ReviewDisplay.cpp | **100** |
| `0x8232a148` | `SetType@CharUpperTwist` → `SetType@DialogDisplay` | DialogDisplay.obj | CharUpperTwist.cpp → system/bandobj/DialogDisplay.cpp | **100** |
| `0x82344238` | `Save@BandHighlight` → `PreLoad@BandButton` | BandButton.obj | BandHighlight.cpp → system/bandobj/BandButton.cpp | 5.368 |
| `0x823c8fc0` | `??_ERndDrawable` → `??_GCharTransDraw` | CharTransDraw.obj | Screenshot.cpp → CharTransDraw.cpp | 99.5–99.75 |
| `0x823ce6c0` | `SetType@RndPropAnim` → `SetType@CharNeckTwist` | CharNeckTwist.obj | MetaMusic.cpp → CharNeckTwist.cpp | **100** |
| `0x82429be8` | `??_ECharBlendBone` → `??_GRndPropAnim` | PropAnim.obj | *(none)* → PropAnim.cpp — **addition over `auto_*`** | 99.5–99.75 |
| `0x8247df48` | `SetType@RndSpline` → `SetType@RndGenerator` | Gen.obj | Line.cpp → Gen.cpp | **100** |
| `0x824816a8` | `??_GRndScreenMask` → `??_GRndMultiMeshProxy` | MultiMeshProxy.obj | ScreenMask.cpp → MultiMeshProxy.cpp | 99.5–99.75 |
| `0x82482350` | `??_GRndRibbon` → `??_GRndScreenMask` | ScreenMask.obj | Ribbon.cpp → ScreenMask.cpp | 99.5–99.75 |
| `0x824d0898` | `??_GRndShockwave` → `??_GWorldDir` | world/Dir.obj (+22 others) | Shockwave.cpp → system/world/Dir.cpp | 99.500 |
| `0x824e9ed0` | `??_GRemoteBandUser` → `??_GSpotlightEnder` | SpotlightEnder.obj | BandUser.cpp → SpotlightEnder.cpp | 99.5–99.75 |
| `0x82605ca8` | `SyncProperty@RndParticleSysAnim` → `SyncProperty@BandStorePanel` | BandStorePanel.obj | TrackWatcherImpl.cpp → band3/meta_band/BandStorePanel.cpp | 68.333 |
| `0x8273e568` | `SetType@DxMultiMesh` → `SetType@DxMovie` | rnddx9/Movie.obj | system/rnddx9/CubeTex.cpp → system/rnddx9/Movie.cpp | **100** |
| `0x8273f8f0` | `SetType@DxMovie` → `SetType@DxMultiMesh` | rnddx9/CubeTex.obj | system/rnddx9/Movie.cpp → system/rnddx9/CubeTex.cpp | **100** |

The last two are a straight **swap** — each unit held the other's function.

**Attribution was exact:** the six that crossed to fuzzy 100 sum to **1,648 B**, which
equals the whole-binary Δ to the byte. Zero unexplained residual.

**Destination chosen by two independent criteria that agree on all 15:** (a) which of
our objs defines the proven symbol, and (b) adjacency — which definer unit's existing
`.text` blocks abut or surround the address. 12 of 15 are adjacency-confirmed.

**Weakest-evidence move, corroborated after the fact:** `??_GWorldDir` `0x824d0898` →
`system/world/Dir.cpp`. `??_G` is a weak COMDAT emitted by **23** of our objs and the
nearest definer unit was 1,060 B away rather than adjacent. It landed at **99.500**,
which confirms the destination retrospectively.

⚠ **A mirror-image caveat the A/B flagged:** commit A *costs* three units their 100 %
status (`CharTransDraw`, `MultiMeshProxy`, `SpotlightEnder`) purely by
`DENOMINATOR_SHRANK` — it adds previously-unpairable rows into those units'
denominators. Those units were "100 %" only because a real sub-100 row was invisible.
Units and bytes are separate measures; this is accuracy, not regression.

### 2b. The 4 genuine "kept" rows — re-pin **then** rename, one commit (`f46886dc`)

| addr | old → proven | pin move | 100 → |
|---|---|---|---|
| `0x824e9da8` | `Save@PostProcer` → `Save@SpotlightEnder` | PostProcer.cpp → SpotlightEnder.cpp (inside the `0x824e9d20`+368 block) | **100.000 HELD** |
| `0x825748e8` | `??_GHamIKEffector` → `??_GSetlistToStorePanel` | HamIKEffector.cpp → MetaPanel.cpp | **100.000 HELD** |
| `0x8232ac50` | `??_GCharUpperTwist` → `??_GDialogDisplay` | CharUpperTwist.cpp → system/bandobj/DialogDisplay.cpp | 99.412 |
| `0x82340670` | `Load@HamLabel` → `Load@BandLabel` | HamLabel.cpp → BandLabel.cpp | **26.100** |

**Two of four held 100 under the true name.** That is corroboration, not luck: a wrong
name pairing against a different class's obj does not land on 100, so those two
independently confirm the vtable-derived spelling at zero cost.

The two that fell are the correctness win. `Load@BandLabel` at **26.100** is the
circular-pin hazard in its purest form — DC3's `HamLabel.cpp` was pinned onto retail
`BandLabel` and scored 100 because *DC3's HamLabel **is** RB3's BandLabel renamed*.
The 100 was measuring the rename, not the code.

### 2c. Three rows correctly homed already — no action

| addr | proven | score | why no action |
|---|---|---|---|
| `0x82340740` | `Save@BandLabel` | 4.000 | correctly homed in BandLabel.cpp; the low score is **source divergence**, not pinning |
| `0x82406178` | `PreLoad@RndDir` | 17.189 | same — honest low score, right unit |
| `0x8268c8d0` | `??_GRemoteBandUser` | 100 | already correct |

### 2d. One row REFUSED — `0x8252a598`, a proven ICF fold

W15-D listed this as a rename candidate (`ContentPattern@Callback@ContentMgr` →
`UserName@NullLocalBandUser`). **It is not a misnaming — it is a genuine fold, and
re-pinning it would destroy a correct 100 for nothing.**

Retail body at `0x8252a598` is three instructions:

```
lis   r11, 0x8200
addi  r3,  r11, 0x0c55
blr
```

The string at `0x82000c55` is the **empty string**. Both our
`?UserName@NullLocalBandUser@@UBAPBDXZ` (band3/game/BandUser.obj) and
`?ContentPattern@Callback@ContentMgr@@UAAPBDXZ` (band3/meta_band/AccomplishmentPanel.obj)
are 12 B and relocate against the **same** literal COMDAT `??_C@_00CNPNBAHC@?$AA@` —
byte-**and**-relocation-identical, which is exactly MSVC's folding criterion. Both
names legitimately live at that address; the survivor's spelling is arbitrary.

It is also **26,336 B from its nearest definer** and has **no `.pdata` entry** at all.

⇒ Recorded as **refused, with the retail-byte reason**. This is the one row of the 23
judged unresolvable, and it is unresolvable *because the information was destroyed by
the linker*, not because we lack evidence.

### IDENTIFIED-NO-SOURCE

**None of the 23** — every one has a definer obj with an existing splits heading.

The class does appear in the 13 thunk rows: see §3, where three addresses are proven
misnamed but **cannot be renamed** because the correct owner (`PatchRenderer`,
`CharWeightable::Replace`) has no compiler layout / no definition here. Those are
write items for another lane.

---

## 3. The 13 "not acted" thunk rows (`c4e35f00`)

W15-D left these for two reasons: seven hit a **name-holder conflict** and six sat in
**no unit at all**.

### 3a. The four chains — and the correction that made them safe

Each conflict runs back to a head address W15-D marked *"skipped/not moving"*. **My
own pre-compaction plan assumed those heads were free. They are not** — all four hold
the very name the chain needs. Applying the plan as written would have created
duplicate names. Re-deriving from W15-D's `census.json` showed the heads are
themselves **proven wrong**, which is what makes a rotation legal rather than a shuffle:

| head | map says | vtable slot actually says | why W15-D could not fix it |
|---|---|---|---|
| `0x822af088` | `RndTexRenderer::SyncProperty` | **PatchRenderer::SyncProperty** (vtable +164, slot 7) | `NODEF` — PatchRenderer has no compiler layout here |
| `0x822aec00` | `DxMesh::Copy` | **PatchRenderer::Save** (vtable +164, slot 8) | `NODEF` — same class |
| `0x823aedf0` | `CharWeightable::SetType` | **CharWeightable::Replace** (vtable +28, slot 2) | `NODEF` — no definition of `Replace` |
| `0x822d3908` | `?Save@RndDir@@$4PPPPPPPM@A@…` | **not a vtordisp thunk at all** | see below |

`0x822d3908`'s retail bytes are `3863ff70 4812ff74` = `addi r3,r3,-144; b …` — an
**8-byte plain adjustor**, not a 12-byte vtordisp thunk, and it sits **inside** another
function (its `.pdata` owner begins `0x822d38b0`, length 80). The name is wrong in
*shape*, not merely in class.

All four are set to JSON **`null`**, which `obj_target_symbol_renamer` documents as
"deliberately unclaimed". That is also the only safe spelling: deleting the key is the
`None.encode()` landmine that same file warns about. Those addresses revert to
`fn_<addr>` placeholders, which objdiff **forgives** at call sites — the honest state
for an address we know is misnamed and cannot name.

**The rotations** (11 renames; verified to create zero duplicate names, and confirmed
after the build that each rotated name has exactly one home in `report.json`):

| chain | rotation |
|---|---|
| RndTexRenderer / BandIKEffector | `0x822af088`→null · `0x82445f88`←`SyncProperty@RndTexRenderer` · `0x822c46c0`←`??_EBandIKEffector` · `0x822c57b8`←`Load@BandIKEffector` |
| CharWeightable / CharPollGroup | `0x823aedf0`→null · `0x823aee10`←`SetType@CharWeightable` · `0x823af2a8`←`Load@CharWeightable` · `0x823b08b0`←`Load@CharPollGroup` |
| DxMesh / RndTexRenderer | `0x822aec00`→null · `0x827385b8`←`Copy@DxMesh` · `0x82445f78`←`Load@RndTexRenderer` |
| RndDir / RndLine | `0x822d3908`→null · `0x824048b8`←`Save@RndDir` · `0x8247b7f0`←`Save@RndLine` · `0x8247c3e0`←`Load@RndLine` |

**Two pin moves**, each corroborated by adjacency the vtable proof did not use:

| addr | move | adjacency |
|---|---|---|
| `0x822C57B8` | MidiInstrument.cpp → BandIKEffector.cpp | tail of `0x822C5758–0x822C57C8`; BandIKEffector's next block **starts** at `0x822C57C8` → merges to `0x822C57B8–0x822C5AF0` |
| `0x823AF2A8` | CharPollGroup.cpp → CharWeightable.cpp | tail of `0x823AF22C–0x823AF2B8`; CharWeightable's next block **starts** at `0x823AF2B8` → merges to `0x823AF2A8–0x823AF310` |

Neither drains its source unit, so no splits entry is deleted.

### 3b. The six "address not in any unit" rows — pins over `auto_*`

Destinations confirmed **four independent ways**: vtable geometry, our obj defining the
symbol, the enclosing address gap, and — decisively — where each thunk actually
**branches**:

| addr | name | branches to | inside |
|---|---|---|---|
| `0x82697528` | `Handle@GamePanel` | `0x82696B48` | GamePanel.cpp's own block |
| `0x82812018` | `SetType@UILabelDir` | `0x82811EB8` | UILabelDir.cpp's own block |
| `0x82812038` | `Save@UILabelDir` | `0x8280FF38` | UILabelDir.cpp's own block |
| `0x82812048` | `Handle@UILabelDir` | `0x82810048` | UILabelDir.cpp's own block |
| `0x82812058` | `SyncProperty@UILabelDir` | `0x828101A0` | UILabelDir.cpp's own block |
| `0x82812068` | `PostLoad@UILabelDir` | `0x82810ED0` | UILabelDir.cpp's own block |

UILabelDir's existing pins **bracket the gap** on both sides (`…–0x82812014` and
`0x82812080–…`).

⛔ **The five UILabelDir thunks are pinned INDIVIDUALLY at 12 B, never as one gap
block** — the gap is not homogeneous:

- `0x82812028` is a **16-byte thunk of a different shape** (`lwz r11,-4(r3); subf;
  addi r3,r3,-692; b`) that branches to **`0x822797B0`**, far outside UILabelDir. It
  is a **foreign** thunk.
- `0x82812078` is the **8-byte EH prefix** (`82829530 821262c8` — a `.text` pointer
  plus an `.rdata` pointer) belonging to the function at `0x82812080`.

That foreign thunk doubles as the **negative control** proving the branch-destination
instrument discriminates rather than confirming whatever it is pointed at: 6 correct
placements, 1 correct rejection.

`0x82697528` is the one destination **without** adjacency — it sits 164 B past
GamePanel's last block end (`0x82697484`), in a gap that also holds four unattributed
40-byte forwarders, before Stats.cpp's block at `0x82697538`. Only the 12-byte thunk is
pinned; the four forwarders are **deliberately left unattributed** (see §5).


### 3c. Commit C's measured result and its one surprise

Pre-registered **+12 fns / +144 B**; measured **+13 fns / +156 B** — one row of 12 B
more than predicted. The unit breakdown localises it exactly:

| unit | Δ | accounted for by |
|---|---:|---|
| `system/ui/UILabelDir` | +5 | the five new pins |
| `BandIKEffector` | +2 | `0x822c46c0` 98.333→100, `0x822c57b8` newly named |
| `Line` | +2 | `0x8247b7f0`, `0x8247c3e0` |
| `TexRenderer` | +2 | `0x82445f88`, `0x82445f78` |
| `Anim` | +1 | `0x824048b8` |
| `CharWeightable` | +1 | +2 rotated in, −1 head (`0x823aedf0`) nulled off a false 100 |
| `GamePanel` | +1 | the new pin |
| `BandSwatch` | **−2** | the two nulled heads `0x822af088`, `0x822aec00`, **both false 100s** |
| `EndingBonus` | **+1** | **not one of my rows — see below** |
| `CharPollGroup` | 0 | `0x823b08b0` gained, `0x823af2a8` moved out — nets zero, as predicted |

**The residual, run down and fully explained.** `?Save@EndingBonus@@$4` at `0x822d5868`
branches to **`0x822d3908`** — one of the four heads this commit set to `null`. With the
wrong name gone the target became a **placeholder**, which objdiff *forgives*, so the
row crossed **98.333 → 100**.

⇒ This is CLAUDE.md's naming economics running in reverse: naming an anonymous address
converts a forgiven call site into a checked one, so **removing a proven-wrong name
un-charges its call sites**. Nulling a misnamed body pays at every thunk that targets it.

**And it hands the next lane a lead.** The sibling `?Copy@EndingBonus@@$4` at
`0x822d5898` branches to `0x822d3920`, which the map calls
**`??2SpotlightDrawer@@SAPAXI@Z`** — and it is still charged at **98.333**. A `Copy`
vtordisp thunk whose destination is spelled `operator new` is almost certainly another
misnaming in the same `0x822d39xx` cluster as the `0x822d3908` defect. Not chased here.

---

## 4. The `probe.py` `.pdata` decode fix

W15-B's `~/tmp/w15d/probe.py` decoded the X360 packed `.pdata` second word with `>>2`
where the field is at `>>8`. The packed word (**big-endian**) is
`PrologLen:8 | FunctionLength:22 | flags:2`, so `FunctionLength` is bits 29..8, in
**instructions**:

```python
flen = ((f >> 8) & 0x3FFFFF) * 4     # was (f >> 2) -- off by 6 bits
```

Measured over all **57,733** retail `.pdata` entries: `>>8` fits the distance to the
next entry **57,732/57,732 (100 %, 0 overruns, 61.41 % exact)**; `>>2` **overruns
57,730/57,732**, with a maximum "length" of **2,466,308 B**.

A `selftest()` was added with **a control that must fail** (`if sh==2 and over==0:
ok=False` — otherwise the test is vacuous) plus three known extents cross-checked
against dtk's own carve in `symbols.txt`:

```
  containment >>8 (corrected)   : overruns 0/57732
  containment >>2 (old, buggy)  : overruns 57730/57732
  extent 0x822cbc10: decoded 104 vs dtk symbols.txt 104  OK
  extent 0x8232a148: decoded 316 vs dtk symbols.txt 316  OK
  extent 0x82344238: decoded 1140 vs dtk symbols.txt 1140  OK
SELFTEST PASS
```

⚠ Stated honestly: `symbols.txt` is a **second implementation**, not a fully
independent oracle. The containment check is the oracle-free half.

**What W15-B's counts change to: nothing.** Only `pdcheck.py` and `vt.py` import
`probe`, and neither calls `pdata_extent` or reads `flen`. The bug was real and latent —
it never reached a published number.

---

## 5. What this lane did NOT do

- **Did not attribute the four 40-byte forwarders** at `0x82697484–0x82697524` to
  GamePanel. They are plausibly GamePanel's, but "plausibly" is not evidence, and
  pinning 164 B on a hunch is exactly the speculative attribution the pin-neutrality
  note warns against. They remain `auto_*`.
- **Did not chase the three `NODEF` heads to a correct name.** `PatchRenderer` has no
  compiler layout here and `CharWeightable::Replace` no definition. Naming them needs
  source, not pins.
- **Did not touch `src/`.** No source was modified by this lane, so the native gate
  does not apply.
- **Did not re-pin `0x8252a598`** — refused on retail-byte evidence (§2d).
- **Did not run the permuter** (standing directive: OFF).
- **Did not verify** that the 181 no-expectation classes contain further misnamings —
  out of scope; W15-D's list stands as the backlog.

---

## 6. W16-I follow-up on the leads handed on in §3c and §5 (2026-09-14)

Lane W16-I took the three leads this doc handed on. One subsection each, with the
retail-byte adjudication and the measured Δ. §5 above is left exactly as W16-F
wrote it — it is a dated record — but **§6.2 corrects one of its claims**, and the
correction is the most useful thing in this section.

Measurements are whole-binary `report.json` on the shipped `name_check` ruler,
each after `touch config/45410914/config.yml` + a full `./tools/ninja-locked`.

### 6.1 `0x822d3920` — RESOLVED, and better than the `null` this doc allowed

**Verdict: it is `?Copy@EndingBonus@@UAAXPBVObject@Hmx@@W4CopyType@23@@Z`.** The
map name `??2SpotlightDrawer@@SAPAXI@Z` is refuted on shape alone: an 8-byte
`addi r3,r3,-144; b` adjustor cannot be a static `operator new`, which returns
storage and takes a size. §3c guessed a misnaming in the `0x822d39xx` cluster and
that guess was right.

Two independent instruments agreed **4-for-4** across the whole `?X@EndingBonus@@$4`
thunk family:

1. each retail thunk's own branch destination, read from the target obj's relocations;
2. name-free `(adjustment, final destination)` matching against our four compiled
   `-144` adjustor COMDATs.

All four were additionally mis-pinned into MatAnim/MoveMgr. Fixed map + pins in ONE
commit, per this doc's own rename-and-re-home rule. **Predicted +5 matched_functions
/ +44 matched_code; measured +5 / +44 exactly.** Commit `d170a4ea`.

### 6.2 The three `NODEF` heads — one was NOT undefined, and §5 is wrong about it

§5 says *"`PatchRenderer` has no compiler layout here and `CharWeightable::Replace`
no definition. Naming them needs source, not pins."* The first half is right. **The
second half is wrong, and the reason it looked right is worth keeping.**

Our tree defines `CharWeightable::Replace` at `src/system/char/CharWeightable.cpp:16`,
and `CharWeightable.obj` emits both the body `?Replace@CharWeightable@@MAAX…` and its
adjustor thunk `?Replace@CharWeightable@@$2PPPPPPPM@A@AAX…`.

**Why the search missed it: the thunk-mangling digit encodes ACCESS.** Every other
CharWeightable virtual is public and its thunk is spelled `$4PPPPPPPM@A@` — `ClassName`,
`??_E`, `Copy`, `SetType`, `SyncProperty`, `Save`, `Load`, all already named, all at
exactly 12 B / 100%. `Replace` is the only **protected** one (`M` in its mangle), so
MSVC spells its thunk **`$2`**. Looking for the `$4` form finds nothing, and "no `$4`
form exists" reads exactly like "no definition exists". Same family as the binary-`grep`
trap: a decisive-looking negative produced by asking with the wrong spelling.

Identification of `0x823aedf0`, three ways: (a) retail is the 12-byte vtordisp thunk
`lwz r11,-4(r3); subf r3,r11,r3; b <impl>`, whose words 0–1 are **byte-identical** to
our `$2` COMDAT; (b) by elimination — `fn_823AEDF0` was the only unnamed thunk row in
the unit and `Replace` the only virtual with an unnamed thunk; (c) W16-F's own vtable
read (+28, slot 2). **Predicted +1 / +12; measured +1 / +12 exactly.** Commit `73c9ce58`.

The row was unpaired at 0% beforehand, so there was no un-pairing exposure, and
retail's branch destination `0x823ae888` is unnamed — a placeholder, therefore
**forgiven** — so the thunk's single relocation is uncharged.

**The other two heads stay `null`, and this is now IDENTIFIED-NO-SOURCE rather than a
guess.** `0x822af088` (`PatchRenderer::SyncProperty`) and `0x822aec00`
(`PatchRenderer::Save`) are the same 12-byte vtordisp thunk shape, branching to
`0x822aedc8` (560 B) and `0x822aea68` (136 B). But **`PatchRenderer` has no `.cpp`
anywhere in our tree** — the header is marked "Declaration only", `Band.cpp` re-declares
a local shim for `Init`/`Terminate`, and `BandSwatch.cpp:122` includes it solely to
force-emit the `StaticClassName()` COMDAT. Naming these would install names no compiled
obj defines, which pair to nothing and read 0% forever.

The oracle **does** hold both bodies, so this is a bounded port for a future lane, not a
dead end:

| head | oracle | location | shape |
|---|---|---|---|
| `PatchRenderer::SyncProperty` | rb3-Wii | `src/system/bandobj/PatchRenderer.cpp:94-102` | `BEGIN_PROPSYNCS`, 4 props + `SYNC_SUPERCLASS(RndTexRenderer)` |
| `PatchRenderer::Save` | rb3-Wii | `src/system/bandobj/PatchRenderer.cpp:46` | `SAVE_OBJ(PatchRenderer, 0x5A)` ⇒ `{ MILO_ASSERT(0, 0x5A); }` |

⚠ Two notes for whoever ports it. `PatchRenderer` is **absent from DC3 entirely** (DC3
has no `bandobj/`), so rb3-Wii is the only oracle. And `SAVE_OBJ` expands to
`MILO_ASSERT(0, …)`, which in the match build is `((void)(0))` — so `Save` compiles to
an **empty body**, which is consistent with retail but means it will land in the
empty-function ICF class and need an alias, not just a name.

### 6.3 `0x8252a598` — W16-F's refusal independently verified on retail bytes

Verified by re-deriving it, not by re-reading W16-F's note. The retail body is 12 bytes:

```
lis  r11, 0x8200
addi r3,  r11, 3157      ; r3 = 0x82000C55
blr
```

— a nullary function returning a `const char*`. The map names it
`?ContentPattern@Callback@ContentMgr@@UAAPBDXZ`, i.e. `virtual const char*
ContentMgr::Callback::ContentPattern()`, and that signature fits the body exactly
(no args touched, returns a pointer in r3). **And the byte at `0x82000C55` is `0x00`
— the empty string.** So the function is precisely `return "";`.

That is what makes the refusal correct, and it is stronger than "both candidates are
12 B". Every `const char* f() { return ""; }` in the program compiles to this identical
body relocating against the same empty-string COMDAT `??_C@_00CNPNBAHC@?$AA@`. Once the
relocation is masked there is nothing left to discriminate on — the bodies are identical
**by construction, not by coincidence**, so byte evidence cannot even in principle say
which name this address denotes. This is the irreducible class CLAUDE.md names directly:
the fold is real and *which name the call site meant was destroyed by ICF itself*. A pin
here would be a coin-flip dressed as an identification. W16-F was right to refuse; do not
re-litigate it without a **non-byte** oracle.

⚠ One instrument note, because the obvious corroboration is vacuous. "How many other
retail functions have this shape?" cannot be answered by scanning `.pdata`: a 12-byte
leaf stub touches neither the stack nor LR, so it gets **no unwind record at all** and is
excluded from that population by construction. My scan duly returned 0, which is a fact
about the scan, not about the program — the same sub-`.pdata` blind spot the AUDIT-NC
scope bound describes. Do not read such a 0 as evidence of uniqueness.
