# SongDB's forwarder block was a NAME PERMUTATION (lane W31-SONGDB, 2026-08-17)

**Verdict: ACTIONED. +2 matched functions / +5,148 B / +0.049877 pp, 21 rows
crossed to `fuzzy == 100`, ZERO fell off.** Measured at `2364b291` + build,
ruler = shipped **`name_check`** (`report.json` `provenance`).
Baseline reproduced exactly in the lane worktree, and it agrees with W30's:
`matched_functions 44,505 / matched_code 3,760,224 / total_code 10,320,664 /
code% 36.433937`.

Commit: `0cb82a94` on branch `w31-songdb`.

## 1. The defect: a permutation, not three bad rows

W30 flagged three rows and refused to blind-fire. Re-derived from retail bytes
rather than inherited, the defect is a **full permutation of 7 names across the
9 standalone 8-byte forwarders** in `SongDB`.

**The family invariant** — and the whole basis of the identification — is that
every one of these is `SongDB::X(...) { return mSongData->X(...); }`. Our own
source implements exactly that (`src/band3/game/SongDB.cpp:157-180, 634-648`),
so **the callee NAMES the forwarder.**

⚠ The invariant is *demonstrated, not assumed*: `RecalculateGemTimes`
@`0x826851b0` was already consistent and served as the control. Before the fix
the block scored **1 consistent / 6 mismatched / 4 unknown-callee**; after, it
is **5 consistent / 0 mismatched** (the 4 remaining unknown-callee rows are
addressed in §4).

| addr | map name BEFORE | retail tail-call target | TRUE name |
|---|---|---|---|
| `0x82684f68` | `SetFakeHitGemsInFill` | `SongData::GetGemList` @`0x82770730` | **`GetGemList`** |
| `0x82684f70` | `GetGemList` | `SongData::GetGemListByDiff` @`0x82770718` | **`GetGemListByDiff`** |
| `0x82684f78` | `GetGems` (40 B, real body) | calls `0x82770730`, then `+4` | `GetGems` ✅ already right |
| `0x82684fa0` | `EnableGems` | `SongData::GetDrumFillInfo` @`0x82771420` | **`GetDrumFillInfo`** |
| `0x82684fa8` | `GetDrumFillInfo` | `fn_82771500` | **`GetVocalNoteList`** |
| `0x82684fb0` | *(no map name at all)* | `SongData::GetVocalNoteListCount` @`0x82770800` | **`GetVocalNoteListCount`** |
| `0x826851a8` | `GetGemListByDiff` | `fn_82770498` | **`SetFakeHitGemsInFill`** |
| `0x826851b0` | `RecalculateGemTimes` | `SongData::RecalculateGemTimes` | ✅ CONTROL |
| `0x826851b8` | `GetVocalNoteListCount` | `fn_82771208` | **`EnableGems`** |
| `0x826851d0` | `ChangeDifficulty` | `fn_82771430` | ✅ already right |

Structurally this is a **3-cycle** (`f68 → f70 → 851a8 → f68`) plus a **4-chain**
(`0x82770730 → fa8 → fa0 → 851b8 → fb0`, the last slot previously empty).

⚠ Two corrections to W30's §4 table, both found by re-deriving:
- `SongDB::GetFillInfo` @`0x826850b8` forwards to **`SongData::GetDrumFillInfo`**,
  not `SongData::GetFillInfo`. It is still *consistent* — but only via the
  separately proven T1 fold `GetDrumFillInfo` ≡ `GetFillInfo` @`0x82771420`.
- `0x826850cc` / `0x826850d4` look like standalone forwarders to a raw-word scan
  but are the two tails **inside** the branchy 36-byte `GetFillInfo`. A scan of
  this shape must check `.pdata`/heading geometry before counting a hit.

## 2. Four unnamed callees identified from retail bytes

Each confirmed on retail bytes **and** against our source **and** against caller
semantics:

| addr | size | identification | evidence |
|---|---|---|---|
| `0x82770730` | 24 B | **`SongData::GetGemList`** | `mTrackDifficulties`(`0x50`)[i], `mGemDBs`(`0xb0`)[i], tail-call `GameGemDB::GetDiffGemList` — our `SongData.cpp:1170` with `GetGemListByDiff` inlined |
| `fn_82771500` | 136 B | **`SongData::GetVocalNoteList`** | `this->0x128` → `PlayerTrackConfigList::UseVocalHarmony()`, then `mVocalNoteLists[idx+1]` w/ size guard else `.front()` — our `SongData.cpp:1251` |
| `fn_82771208` | 240 B | **`SongData::EnableGems(int,float,float)`** | inlines `GetGemList`, walks gems at stride `0x44`, flags out-of-range with `stb 0x80,0x11(r11)` == our `gem.unk18 = 0x80` |
| `fn_82771430` | 204 B | **`SongData::ChangeDifficulty`** | `this->0x50[idx] = r5` — writes the very array `GetGemList` reads as difficulty |
| `fn_82770498` | 8 B | **`SongData::SetFakeHitGemsInFill`** | `stb r4,0x28(r3); blr` == `mFakeHitGemsInFill = b` |

★ `fn_82771208`'s `(int, float, float)` shape independently corroborates the
mangled signature `?EnableGems@SongDB@@QAAXHMM@Z` = `void(int,float,float)` —
an arity/type check that is free and that no name-keyed instrument provides.

### Byte geometry was checked BEFORE naming anything
Per the standing rule that a **phantom row (dtk mis-carve) is indistinguishable
from an unidentified one** by every name-keyed instrument: `fn_82771500` has its
**own retail `.pdata` entry**, 136 B, `0x82771500`–`0x82771588`, ending exactly
where the next named symbol begins. It is a real function. (`0x82684fa0`/`fa8`
etc. correctly have **no** `.pdata` — an 8-byte leaf stub touches neither stack
nor LR, so it gets no unwind record, exactly as CLAUDE.md predicts.)

⚠ The `.pdata` bitfield is `proglen = w & 0xFF`, `funclen = (w >> 8) & 0x3FFFFF`
**in words**, read big-endian. A wrong decode reads 8-byte forwarders as 2,560 B.
Validate against a known extent before trusting it.

### Call-site semantics — a fully independent third instrument
A retail-wide `bl`/`b` xref, attributed to the enclosing `.pdata` function:

- `0x82684f68` (map said `SetFakeHitGemsInFill`, a **setter**) has **15 callers**,
  all gem **readers**: `GemPlayer::AllCodaGemsHit`, `IgnoreGemsUntil`,
  `GetSoloData`, `TrackerSectionManager::GetGemIDsForRange`, `Player::DelayReturn`.
  A `void(bool)` setter is not called 15× from query functions. `GetGemList` is.
- `0x82684fa8` (map said `GetDrumFillInfo`) has **22 callers, overwhelmingly
  vocal**: `VocalGuidePitch::Poll`, `VocalPart::PostLoad`,
  `TrackerUtils::CountVocalPhrasesInSong`, `VocalTrainerPanel::Enter`,
  `VocalTrack::UpdateScrolling` (×5).
- `0x82770730` (map said `GetVocalNoteList@SongDB`) has **21 callers, nearly all
  inside SongDB and all gem-related**: `GetTotalGems`, `GetSustainGemCount`,
  `DisableCodaGems`, `GetGem`, `GetGems` — i.e. our `mSongData->GetGemList(t)->mGems`.

## 3. Why the rename was SAFE to fire (checked before, not after)

⛔ Proving a name wrong does **not** make renaming it safe: if the base obj
cannot define the new name the row reads a **permanent 0%**. Checked in the
built worktree:

- our compiled `SongDB.obj` **DEFINES all 7** SongDB spellings ⇒ every in-block
  move pairs.
- `SongDB.obj` has `?GetGemList@SongData@@…` only as **UNDEF**. So renaming
  `0x82770730` *without* also moving its `splits.txt` pin from `SongDB.cpp` to
  `SongData.cpp` would have pinned that row at 0% forever. The pin move is part
  of the patch. (`SongData.obj` defines it, and our body is **byte-identical**
  to retail there.)
- map **name-injectivity** verified: `distinct == total`, 0 collisions.

⚠ Worktree hygiene note that nearly cost a false conclusion: main's *working
tree* carried **2 uncommitted map entries** that `HEAD` did not (a concurrent
lane on the shared tree). Counting against main made a correct +1 edit read as
−1. **Count against your worktree's `HEAD`, never against dirty main.**

### The alias at `0x82770730` was not a fold — it was this defect in disguise
`scripts/symbol_aliases.json` held a T1 group: survivor
`?GetVocalNoteList@SongDB@@…`, folded `?GetGemList@SongData@@…`. Its byte test
passed **precisely because retail `0x82770730` really is our
`SongData::GetGemList`**. Once the address is named correctly the two spellings
sit at distinct map addresses, so the equivalence would forgive a genuinely
wrong callee — and the alias gate would fatal on it. **Withdrawn** with
`folded: []` + a `withdrawn_reason`; nothing pruned, per house rule.

## 4. What this lane deliberately did NOT do

- **Did not name the four identified SongData addresses.** Direct byte
  comparison of our compiled COMDATs against retail (relocations masked) says
  `GetVocalNoteList` differs in **23/34 words** and `EnableGems` differs in
  size, so naming them pairs the rows at `fuzzy < 100` = **+0 bytes**. Per the
  standing economics that is the "bug exposure, not bytes" case; it is safe
  (every caller's spelling is already correct) but it is not a byte lever, and
  it belongs with an actual body-port of those three functions.
- **Did not pin `fn_82770498`.** It is byte-identical to our
  `SongData::SetFakeHitGemsInFill`, but it sits in an **unpinned** gap
  (`0x82770498`–`0x827704C0`), so naming alone buys nothing; it needs a pin,
  which is a separate (small, ~16 B with the adjacent getter) lever.
- **Did not touch `src/**`** ⇒ the native gate is not applicable to this patch.
- **Did not action the `Game`/`TrackerManager` family** — see §6.

## 5. ★ The economics INVERT for a called family — correcting the briefed ratio

Pre-registered before the A/B, and reported predicted-vs-measured:

| quantity | predicted | measured |
|---|---|---|
| Δ`matched_functions` | +2 | **+2** ✅ exact |
| Δ`none` (pairing channel) | +32 B | **+32 B** ✅ exact |
| Δ`name_check` bytes | +56 B | **+5,148 B** ❌ 92× low |

The two channels I could compute from first principles were exact. The direct
rows really are **+56 B**. The other **+5,092 B (98.9%) is the CALLER CASCADE**:
16 rows across 9 units, each sitting at **99.80–99.98%** behind a *single*
charged relocation-name site, all crossing at once —
`PerfectOverdriveTracker::TranslateRelativeTargets` +1,168,
`GemPlayer::GetSoloData` +556, `VocalTrainerPanel::Enter` +504,
`Player::DelayReturn` +392, `PracticePanel::MarkGemsAsProcessed` +340, …
**Every one is a caller found independently in the retail xref scan.**

⛔ **This contradicts the standing figure "un-pairing is 80.5% of a map edit's
delta, the cascade only 19.5%".** Here it inverts: **cascade 98.9%, pairing
0.6%, direct-name 0.5%.** The ratio is **not a constant** — it tracks how
heavily the renamed symbols are **called**, and these forwarders carry 15–22
call sites each. Do not price a family rename off the 19.5%.

★ And the `none` control did its job as a **forward** instrument: it moved by
exactly the pre-registered pairing figure, confirming the mechanism rather than
merely not-contradicting it. `ab_measure` correctly labelled the shape
`NOT_APPLICABLE` (kinds = map+splits, so `none` movement is expected).

★ **The family lever, sized.** The row W30 flagged — `0x82684fa8`, a *false*
100.0% — is worth **0 bytes on its own**, because its retail callee is unnamed
and therefore **forgiven** by `name_check`. Only fixing the whole family pays.
This is a clean instance of "a uniformly wrong name FAMILY is invisible to
`name_check`", and here the family was worth **5,148 B**.

## 6. Spin-off, EVIDENCED BUT NOT ACTIONED: `Game` → `TrackerManager`

A whole-binary scan for the same shape (named 8-byte `lwz r3,N(r3); b <named>`
forwarders whose method name disagrees with the callee's) returns **26 hits**.
Most are **legitimate** delegation and must not be blind-fired
(`MasterAudio::SetForegroundVolume → Fader::SetVal`,
`SampleInst360::SetVolumeImpl → Voice::SetVolume`,
`SessionMgr::GetLocalUserListImpl → NetSession::GetLocalUserList`).

The real lead is the **`Game` forwarder block @`0x82677440`–`0x82677498`**,
which is 7/10 consistent with 3 anomalies — and the defect is on the
**`TrackerManager` side**, not the `Game` side:

| addr | map name | forwards to | contradiction |
|---|---|---|---|
| `0x82677458` | `Game::OnPlayerQuarantined(Player*)` | `TrackerManager::HandleGameOver(`**`float`**`)` @`0x82693158` | a forwarder passes `r4` through untouched — a `Player*` cannot land in a `float` param |
| `0x82677490` | `Game::OnRemoteTrackerEndStreak(Player*,int,int)` | `TrackerManager::OnPlayerQuarantined(Player*)` @`0x826936f8` | arity **3 vs 1** |
| `0x826770d0` | `Game::GetMainPerformer` | `FileLoader::GetSize` | implausible |

Going one level down confirms it is a **two-level chain**
`Game::X → TrackerManager::X → Tracker::Y`:

- `0x826936f8` forwards to `Tracker::RemoteEndStreak(Player*,int,int)` — **3
  args, matching its caller** ⇒ it is really
  `TrackerManager::OnRemoteTrackerEndStreak`, not `OnPlayerQuarantined`.
- `0x82693158` forwards to `Tracker::HandleRemovePlayer(Player*)` and is called
  with a `Player*` ⇒ the map name `HandleGameOver(float)` contradicts **both**
  sides.

⚠ **Not actioned here** because the `Tracker`-level names are themselves
suspect, so the correct assignment cannot be closed without deriving that level
too — and an incoherent partial rename would un-pair rows for nothing. Given
`Game`'s forwarders are called from across the game layer, the cascade
prospects look comparable to this lane's. **Recommended as the next lane.**

### Reusable instruments (all in `~/tmp/w31/`, ~40 lines each)
`rdis.py` (capstone PPC disassembly of retail VAs, branch targets annotated with
map names — note **llvm-objdump cannot do raw binary**, it has no `-b binary`),
`pdata.py` (authoritative extents), `fwd2.py` (per-unit forwarder invariant
check — **raw-word matching, because linear disassembly desyncs and silently
drops rows**), `xref.py` (retail call sites attributed to enclosing function),
`cmpbody.py` (our COMDAT vs retail, relocations masked), `fwdscan.py`
(whole-binary defect screen).
