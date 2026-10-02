# W16-NI: five Save bodies from retail asm, and the vector<map<int,float>> spelling (2026-10-02)

Lane W16-NI, branch `w16-ni`, based on main `860f55931`. It takes the two follow-ups that
`W16NF_ENGINE_ANON_ROWS_2026-10-02.md` left open (§4 last paragraph, §6). The 90–99.99
named-row sweep belongs to lane W16-NH and was not touched.

## Result (whole-binary A/B)

`tools/ab_measure.py --worktree ~/tmp/wt-w16-ni-ab --patch <git diff main..w16-ni>`, fresh
worktree at main, one patch (kinds `map, source`), both legs settled, split fixed point on both
legs (0 extra re-splits), ruler `name_check`, objdiff-cli sha256 `c1b7d95240a35cd6` stable:

| | leg A (main) | leg B (branch) | Δ |
|---|---:|---:|---:|
| matched_functions | 50,934 | 50,936 | **+2** |
| masked_equal | 24,536 | 24,534 | −2 |
| honest | 26,398 | 26,402 | **+4** |
| matched_code | | | **+512 B** |
| code% | 53.838802 | 53.843803 | +0.005001 pp |
| fuzzy | 60.079838 | 60.086094 | +0.006256 pp |

`none` control: +608 B (not applicable to a source patch; reading only). Units at 100%:
442 → 442.

The total is the sum of two changes, each measured on its own (below): five Save bodies,
**+5 fns / +676 B** (224+128+124+112+88), and the scaffold respelling, **−3 fns / −164 B**.
676 − 164 = 512 exactly.

## 1. Five Save bodies

All five were `SAVE_OBJ(...)` / `MILO_ASSERT(0, line)` stubs. Xbox retail has real bodies, and no
other source tree carries one. Each body was read off
retail (objdiff target listing) and turns out to mirror the unit's own `Load`/`PreLoad`:

| row | size | retail body | before → after |
|---|---:|---|---|
| `?Save@TrackDir@@` | 224 | rev 6; `PanelDir::Save`; if `!IsProxy()`: `mDrawGroup`, `mAnimGroup`, `mYPerSecond`, `mTopY`, `mBottomY`, `mSlots`, `mWarnOnResort` | 0.71 → 100 |
| `?Save@BandStarDisplay@@` | 128 | rev 2; if `IsProxy()`: `mStarType`; `RndDir::Save` | 3.13 → 100 |
| `?Save@CrowdAudio@@` | 124 | rev 5; `Hmx::Object::Save`; `RndPollable::Save` (resolves to `Hmx::Object::Save`, so retail shows two calls to it) | 3.23 → 100 |
| `?Save@DialogDisplay@@` | 112 | rev 0; `bs << mDialogLabel << mTopBone << mBottomBone` (chained: r3 is reused); `Hmx::Object::Save` | 1.43 → 100 |
| `?Save@BandHighlight@@` | 88 | rev 0; `UIComponent::Save` | 4.55 → 100 |

Offsets were checked against the header comments for TrackDir (retail `this` is the
`Hmx::Object` vbase; TrackDir starts at vbase−0x410, so −0x1bc/−0x1b8/−0x1b4/−0x1b0/−0x198 land on
0x254/0x258/0x25c/0x260/0x278) and BandStarDisplay (`mStarType` 0x244).

**TrackDir needed one fold alias.** Retail writes `mSlots` (`vector<Transform>`) through a `bl` to
`0x8274e650`, which the map names `operator<<(BinStream&, vector<ObjectDir::Viewport>)`.
`ObjectDir::Viewport` is `{ Transform mXfm; }`, so the two bodies are the same. Before the alias,
TrackDir::Save read 99.91, with that call as its only charge.
`tools/icf_pair_adjudicate.py --chase`: FLAT T1 **PROVEN** and CHASED T1 **PROVEN**, sizes
132/132, 7 relocations, empty reloc tally, **1** retail body of this shape. The witness call site
is `0x827dd5dc` in retail TrackDir::Save, where our aligned caller spells the Transform version.
The survivor's only other retail caller (`0x826ea844`) is ObjectDir's own Viewport write, which
spells the survivor. The group was added to `scripts/symbol_aliases.json` with an `admitted[]`
record. `tools/icf_alias_finder.py --validate`: PASS, 0 contradicted, 2,033 groups. After the
alias, 99.91 → 100.

## 2. `vector<map<int,float>>` vs retail's `vector<list<int>>`

**The brief's premise was half stale.** I searched the tree for the container spelling:
`Submix::mChannelsPerSlot` already declares `std::vector<std::list<int> >`. Every retail caller
of `0x82793830/910/B80/8A0/C58` (found by `bl fn_<addr>` over the retail asm) sits in Submix or
SlotChannelMapping, and every one of those rows scores **100**. One exception: the 176 B
`_M_insert_overflow_aux(__true_type)` reads 99.886. That residual is not a container type (see
below). BeatMatcher has no `map<int,float>` at all.

The tree's only remaining spelling of `vector<map<int,float>>` was the CharClip.h scaffold
`template class std::vector<std::map<int, float> >;`. Its comment rested on the premise that
"retail .text genuinely contains vector<map<int,float>> machinery", and W16-NF §4 refuted that on
retail bytes. W16-NF had already renamed every row that premise named to `list<int>`, so the
scaffold no longer fed a single named `vector<map>` row. What it still fed was measured with one
`ab_measure --from-dirty` run per variant:

| variant | Δmatched | Δmasked | Δhonest | Δbytes |
|---|---:|---:|---:|---:|
| delete the line outright | −17 | −10 | −7 | −672 |
| respell as `template class std::vector<std::list<int> >;` | **−3** | −2 | **−1** | **−164** |

Deletion loses six **real** Accomplishment rows, all 100 → 0: the `length_error` and
`logic_error` copy ctors, the `bad_alloc` and `exception` dtors, `bad_alloc`'s scalar deleting
dtor, and `allocator<char>::allocate`. These are vector's growth/throw path, which any vector
instantiation in that TU's include graph donates. Retail pins them in Accomplishment, so the
real donor is some vector growth in retail Accomplishment code that our Accomplishment.cpp does
not reproduce. That donor is unidentified and is a lead for whoever owns Accomplishment.
Respelling keeps all six donors and drops the wrong type, so it is strictly better than deletion.
**Respelling landed**, and the scaffold comment was rewritten to say what it really donates.

### Rows that went down in the whole-branch A/B, with reasons

- `??1?$map@HM...` (`~map<int,float>`), **SongSortMgr, 4 B, 100 → 0 — the −1 honest.** Retail
  `0x825971f0` is a shared fold thunk `b fn_827690D0` with ten callers (VocalTrackDir ×2, Song ×2,
  Utl, SongRecord, PerfectSectionTracker ×3, AmbientOcclusion). Its pin in SongSortMgr is
  linker-arbitrary; our SongSortMgr.obj defined `~map<int,float>` **only** because the wrong-type
  scaffold destroyed `map<int,float>` elements in every TU that includes CharClip.h. SongSortMgr
  itself has no `map<int,float>`. Accepted: the row was paired only through a fabricated donor.
- `??$__destroy_aux@V?$map@HM...` (CharClip, 4 B, 95 → 0; not counted before or after, since a
  row below 100 adds nothing to matched). Retail `0x8237de00` is `b ?Clear@Transitions@CharClip@@`,
  called from CharClip and LightPreset, so **the map name is wrong** and the row only ever paired
  against the scaffold's instantiation. Its true identity is some `__destroy_aux<T>` whose `~T`
  folds to `Transitions::Clear`. Out of this lane's scope; flagged for a map lane.
- **RockCentral −5 (mpn ruler), plus Campaign, Character, CharClip, CharHair, Multiplayer,
  SessionMessages, StorePanel, Debug, ContextChecker:** every one is an anonymous `fn_` EH-unwind
  funclet (40–60 B, `masked_equal`, paired by byte signature only). Removing the
  `vector<map>` COMDATs and their funclets from the including TUs changes the candidate pool, so
  the signature pairing reshuffles (RockCentral's five go 100 → 99.8–99.9; others go both ways).
  These rows carry no asserted identity. Net masked_equal is −2, and honest, which excludes them,
  is +4.

### What was not fixed, deliberately

- `_M_insert_overflow_aux<list<int>>(__true_type)` (Submix, 0x82793B80, 99.886). Its one charge is
  the `list<int>` copy ctor, which retail resolves to `0x82370c80` (`list<CharPollableSorter::Dep*>`).
  Retail carries **11** template-twin bodies of that copy ctor, and the map places `list<int>`'s own
  copy ctor at a separate live address, `0x82766ef8`, which CHASED T1 **REFUTES** against our
  body. An alias onto `0x82370c80` would assert one body where the map asserts two, so it was not
  added. This is a template-twin attribution question, not a container-type defect.
- `0x822a0f38`, named `operator<<(BinStream&, vector<map<int,float>>)` (CharClip, 100 B, 0%): its
  retail body divides by a **0x70** element stride and loops over `fn_822A0008`, so it is
  `operator<<(vector<T>)` with `sizeof(T) == 0x70`, not a map. The map name is wrong; flagged, not
  touched.

## Verification

- In-worktree `report.json` after each step: the four non-TrackDir Saves at fuzzy 100 directly;
  TrackDir 99.91 before the alias, 100 after.
- Native gate (`tools/native_build_gate.sh`), run last on the final code (`cdf15548c`; only
  this docs edit follows it):
  `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`

## Commits

- `c0e567ff6` five Save bodies
- `a127890a7` Viewport/Transform `operator<<` fold alias
- `834fc8071` CharClip scaffold respelling (`vector<list<int>>`)
- this doc
