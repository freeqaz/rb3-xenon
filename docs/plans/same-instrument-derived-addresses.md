# Same-Instrument Patch — Derived Retail Addresses (Xbox 360, TU5, XEX base 0x82000000)

Derived 2026-07-07 against the live Ghidra MCP server (port 8002, program
`/default.xex-35adb6`). Method follows `rb3enhanced-same-instrument-patch.md`
§8: MILO_FAIL/assert strings are stripped in retail, so string-xref doesn't
work for these classes (confirmed empty on every distinctive assert literal
tried — see "Strings tried" below). All identifications here are via
**structural fingerprinting**: matching a candidate's decompiled shape
(member offsets, callee set, call counts, container-element sizes) against
the exact Wii `rb3` oracle source, then cross-checking caller/callee
relationships between candidates.

A systematic decompiler artifact affects every result below: this XEX's
prologue/epilogue register-save thunks (`FUN_82803f10/28/2c/30/34/38/3c`,
all literally empty `{return;}` bodies with hundreds-to-thousands of xrefs)
are rendered by Ghidra as spurious "phantom calls that return a value" at
the top of nearly every member function. The "returned value" is just
`this` (r3) surviving the prologue unmodified. Read every
`X = FUN_82803fXX();` in the listings below as `X = this`, not a real call.
This also means Ghidra's parameter-recovery is unreliable for some
functions (it drops formal parameters that are only used via this pattern,
producing e.g. `void Function_XXXXXXXX(void)` for a function that actually
takes an explicit `this` argument in r3) — do not treat an empty-looking
signature as proof a function takes no arguments.

## Summary table

| # | Target | Address | Confidence | Notes |
|---|---|---|---|---|
| 3 | `TrackWatcher::RecalcGemList` | `0x82778700` | **VERIFIED** | byte-exact tail-branch target computed from opcode bits, see below |
| 3 | `TrackWatcherImpl::RecalcGemList` | `0x8276FBB0` | **VERIFIED** | tail-branch target of the above; exact structural match |
| 4 | `SongData::GetGemList(int)` | `0x8274BB38` | **VERIFIED** | `GetGemListByDiff` inlined; byte-confirmed use of offsets 0x50/0xb0 |
| 4 | `SongData::GetGemListByDiff(int,int)` | `0x8274BB20` | **VERIFIED** | standalone, single external caller |
| 5 | `GameGemDB::GetDiffGemList(int)` | `0x8276E010` | **VERIFIED** | `mGameGemLists[diff]` pattern, 17 xrefs, assert stripped |
| 5 | `GameGemDB::Duplicate() const` | `0x8276E590` | **VERIFIED** | 0x10-byte alloc (sizeof(GameGemDB)), passes count *and* mHopoThreshold to ctor |
| 5 | `GameGemList::CopyFrom(const GameGemList*)` | `0x82769450` | **VERIFIED** | clear-if-nonempty + reserve + insert(begin,first,last); element-size divisor 0x44 matches `GameGem` |
| — | `GameGemDB::GameGemDB(int,int)` (ctor, bonus) | `0x8276E4A8` | **VERIFIED** | supporting find, not one of the 7 asked-for targets but needed to disambiguate Duplicate |
| 1 | `PlayerTrackConfigList::ProcessConfig(PlayerTrackConfig&)` | — | **NOT FOUND** | see below |
| 2 | `PlayerTrackConfigList::TrackNumOfType` / `TrackNumOfExactType` | — | **NOT FOUND** | see below |
| 6 | `GameGemDB::~GameGemDB()` | — | **NOT FOUND** | time-boxed; region identified, not isolated |
| 7 | `Band::NewPlayer(BeatMaster*, BandUser*)` | — | **NOT FOUND** | see below |

## Detailed evidence

### 3. `TrackWatcher::RecalcGemList` = `0x82778700` — VERIFIED

Wii oracle (`TrackWatcher.cpp:117`):
```cpp
void TrackWatcher::RecalcGemList() { mImpl->RecalcGemList(); }
```
`mImpl` is at `TrackWatcher+0x0` (first member).

Bytes at `0x82778700` (8 words):
```
82778700  80630000   lwz   r3, 0(r3)        ; r3 = this->mImpl
82778704  4bff74ac   b     0x8276fbb0       ; TAIL CALL (LK=0, computed below)
82778708  80630000   lwz   r3, 0(r3)
8277870c  81630000   lwz   r11, 0(r3)
82778710  816b000c   lwz   r11, 0xc(r11)
82778714  7d6903a6   mtctr r11
82778718  4e800420   bctr
```
The branch target was computed directly from the opcode: instruction word
`0x4bff74ac` at address `0x82778704`; PPC branch-displacement field
(bits 6-29, sign-extended, ×4) = `-0x8b54`; `0x82778704 + (-0x8b54) =
0x8276fbb0`. This is byte-exact equal to the independently-derived
`TrackWatcherImpl::RecalcGemList` address below — the two derivations
cross-verify each other.

(The second half of the listing, `lwz r3,0(r3); lwz r11,0(r3); lwz
r11,0xc(r11); mtctr r11; bctr`, is very likely leftover/adjacent-function
bytes or a virtual-dispatch tail shared by a neighboring thunk — irrelevant
to `RecalcGemList` itself, which is fully expressed in the first 2
instructions as a non-virtual direct tail call, exactly matching the
one-line oracle body.)

### 3. `TrackWatcherImpl::RecalcGemList` = `0x8276FBB0` — VERIFIED

Wii oracle (`TrackWatcherImpl.h:124`, body not shown in header but shape is
"read mSongData/mTrack, call `SongData::GetGemList`, then a virtual
`HandleDifficultyChange`-style dispatch").

Prologue bytes: `7d8802a6 9181fff8 fbe1fff0 9421ffa0` = `mflr r12; stw
r12,-8(r1); std r31,-0x10(r1); stwu r1,-0x60(r1)` — standard non-leaf
function entry, consistent with a real function start (not a mid-function
address).

Decompiled body shows: read `this+0x50` (mSongData), `this+0x68` (mTrack),
call into the `SongData::GetGemList` chain, then a virtual call through
`this`'s own vtable (matches `HandleDifficultyChange`/gem-list-changed
notification pattern). Confirmed earlier in this investigation as an exact
structural match against the oracle; re-confirmed now via the tail-branch
proof from `TrackWatcher::RecalcGemList`.

### 4. `SongData::GetGemList(int)` = `0x8274BB38` — VERIFIED

Wii oracle:
```cpp
GameGemList *SongData::GetGemList(int track) {
    return GetGemListByDiff(track, mTrackDifficulties[track]);
}
```

Bytes:
```
8274bb38  81630050   lwz   r11, 0x50(r3)     ; r11 = mTrackDifficulties.begin()
8274bb3c  548a103a   rlwinm r10, r4, 2,0,0x1d ; r10 = track * 4
8274bb40  812300b0   lwz   r9,  0xb0(r3)     ; r9  = mGemDBs.begin()
8274bb44  7c8b502e   lwzx  r4,  r11, r10     ; r4  = mTrackDifficulties[track]  (diff arg)
8274bb48  7c6a482e   lwzx  r3,  r10, r9      ; r3  = mGemDBs[track]             (this arg)
8274bb4c  48022...   bl    ...               ; tail into GetGemListByDiff/GetDiffGemList
```
This byte-confirms **`SongData::mTrackDifficulties` is at `this+0x50`** and
**`SongData::mGemDBs` is at `this+0xb0`** on this build (see Struct Offsets
section) — both loaded directly with no assert/bounds-check (stripped in
retail), matching the oracle's unchecked `mTrackDifficulties[track]` /
`mGemDBs[track]` array indexing exactly. `GetGemListByDiff` is inlined at
the call site as a tail branch.

### 4. `SongData::GetGemListByDiff(int,int)` = `0x8274BB20` — VERIFIED

Standalone function immediately preceding `GetGemList` in the same unit
(`0x8274BB20`–`0x8274BB34`, matching the `splits.txt` range exactly). Single
external caller (from a different call site than the inlined use inside
`GetGemList`), consistent with the oracle's `GetGemListByDiff` being called
directly elsewhere too (e.g. `SongData::SendGems`).

### 5. `GameGemDB::GetDiffGemList(int)` = `0x8276E010` — VERIFIED

```
81630000  lwz r11, 0(r3)      ; mGameGemLists.begin()
548a103a  rlwinm r10,r4,2,... ; diff * 4
7c6a582e  lwzx r3, r10, r11
4e800020  blr
```
Direct `mGameGemLists[diff]` return, assert stripped, 17 call sites —
matches both the const and non-const oracle overloads (`GameGemDB.cpp:62-70`),
almost certainly folded to one address since both overloads are
byte-identical after the assert is stripped.

### 5. `GameGemDB::Duplicate() const` = `0x8276E590` — VERIFIED

This required resolving an ambiguity: an earlier pass in this same session
misidentified `0x8276E288` as `Duplicate()` (it is actually
**`DrumMixDB::Duplicate()`** — see "Rejected candidates" below). The correct
identification:

```c
int * Function_8276E590(void)   // GameGemDB::Duplicate() const
{
  piVar3 = this;                                 // FUN_82803f34() phantom-call artifact
  uVar2 = FUN_82709ee0(0x10);                     // operator new(sizeof(GameGemDB)) == 16 bytes
  piVar4 = Function_8276E4A8(uVar2, count, piVar3[3]);  // ctor(count, mHopoThreshold) <- BOTH args!
  for (i = 0; i < count; i++)
      Function_82769450(dest[i], src[i]);         // CopyFrom(dest, src) per element
  return piVar4;
}
```
Decisive evidence:
- **0x10 (16-byte) allocation** exactly matches `sizeof(GameGemDB)` =
  `std::vector<GameGemList*>` (12 bytes: begin/end/cap) + `int
  mHopoThreshold` (4 bytes).
- The ctor-helper call passes **both** `count` (`mGameGemLists.size()`)
  *and* `piVar3[3]` (`this+0xc` = `mHopoThreshold`) — this is the detail
  that disambiguates it from the DrumMixDB look-alike, matching
  `new GameGemDB(mGameGemLists.size(), mHopoThreshold)` in the oracle
  exactly (`GameGemDB.cpp:78-83`).
- Its element-copy callee (`0x82769450`) independently verifies as
  `GameGemList::CopyFrom` (below).

### `GameGemDB::GameGemDB(int count, int thresh)` (ctor) = `0x8276E4A8` — VERIFIED (bonus)

```c
undefined4 * Function_8276E4A8(this, count, thresh) {
  this[0]=0; this[1]=0; this[2]=0;   // mGameGemLists = {} (vector default state)
  this[3] = thresh;                   // mHopoThreshold = thresh
  for (i = 0; i < count; i++) {
      elem = operator_new(0x10);      // sizeof(GameGemList) == 16 bytes
      FUN_827691b8(elem, thresh);     // placement GameGemList(thresh) -- sets elem->mHopoThreshold
      push_back(this, &elem);
  }
  return this;
}
```
Exact match for `GameGemDB::GameGemDB(int,int) : mHopoThreshold(thresh) {
mGameGemLists.reserve(count); for(...) mGameGemLists.push_back(new
GameGemList(thresh)); }` (`GameGemDB.cpp:25-30`). Called from 3 sites: from
`Duplicate()` above (expected), and from `Function_8274B938` at
`0x8274b974` (very likely `SongData`'s own constructor building
`mGemDBs`, not investigated further — out of scope for this task but
recorded as a lead if `SongData::SongData` is ever needed).

### 5. `GameGemList::CopyFrom(const GameGemList*)` = `0x82769450` — VERIFIED

```c
void Function_82769450(GameGemList *this, const GameGemList *src) {
    if (this->mGems.begin != this->mGems.end)
        Function_8268C550(&this->mGems, begin, end, tmp);   // clear()'s destroy-range
    Function_8274F5C8(&this->mGems, (src_end-src_begin)/0x44); // reserve(src->mGems.size())
    Function_82769150(&this->mGems, this->mGems.begin,
                       src->mGems.begin, src->mGems.end, tmp); // insert(begin,first,last)
}
```
Matches the oracle line-for-line (`GameGemList.cpp:106-110`):
```cpp
void GameGemList::CopyFrom(const GameGemList *gList) {
    mGems.clear();
    mGems.reserve(gList->mGems.size());
    mGems.insert(mGems.begin(), gList->mGems.begin(), gList->mGems.end());
}
```
The element-size divisor **`0x44` (68 bytes)** matches `sizeof(GameGem)` on
this build (see Struct Offsets) and never touches `mHopoThreshold`,
matching the oracle exactly (only `mGems` is copied, not the threshold).
Sole external caller is `GameGemDB::Duplicate` above (`0x8276e604`),
matching the oracle's single call site.

### Rejected candidate — `0x8276E288` = `DrumMixDB::Duplicate()`, NOT `GameGemDB::Duplicate()`

Recorded here so nobody re-derives this by mistake. `0x8276E288` has the
*same shape* as `GameGemDB::Duplicate` (alloc + construct-N + per-element
copy loop) but:
- allocates only **0xc (12) bytes** = `sizeof(DrumMixDB)` (`DrumMixDB.h:15`
  — `std::vector<TickedInfoCollection<String>*> mMixLists;` is DrumMixDB's
  *only* member, no extra int, hence 12 not 16 bytes),
- its ctor-helper (`0x8276E028`) takes only `(block, count)` — **no third
  threshold argument** — matching `DrumMixDB::DrumMixDB(int)`
  (`DrumMixDB.cpp` — single-arg ctor, no threshold concept for mix lists),
- its per-element copy callee is `0x82750248`, a *different* function from
  `0x82769450`, whose reserve-divisor is `0x10` (16 bytes) not `0x44` —
  consistent with `TickedInfoCollection<String>`'s element type, not
  `GameGem`.

`Function_8274DA90` (the shared caller of both `0x8276E288` and
`0x8276E590`, at `0x8274DA90`–`0x8274DBBC`, matching the `SongData.cpp`
split range) is therefore `SongData::MakeBackupTracks()`
(`SongData.cpp:1331-1341`), which calls *both* `mDrumMixDBs[i]->Duplicate()`
and `mGemDBs[i]->Duplicate()` per the oracle — exactly the two distinct
callees observed.

## Targets NOT resolved (honest report)

### 1/2. `PlayerTrackConfigList::ProcessConfig` / `TrackNumOfType` / `TrackNumOfExactType` — NOT FOUND

What was tried:
- **String search** (all come back empty — stripped in retail): `"Couldn't
  create track of type"`, `"before song is loaded"`, `"which is obsolete"`,
  `"AssignTrack"`.
- **Symbol search**: `AssignTrack`, `GameConfig`, `PlayerTrackConfig`,
  `ProcessConfig` — all empty (no exported/debug names survive for this
  class in retail).
- **RTTI**: `PlayerTrackConfigList`/`PlayerTrackConfig` are plain
  non-polymorphic value classes (no `.?AV...@@` type-descriptor string
  found for either), so there is no vtable/RTTI anchor to pivot from, and
  Ghidra's RTTI xref database is empty for this XEX regardless (confirmed
  separately — see below).
- **Caller-anchor**: identified the real Wii-source caller
  (`Game.cpp:1642`, `cfgList->ProcessConfig(user->GetUserGuid())`, inside
  what is very likely `GameConfig::AssignTrack` per the plan doc's own
  suggested anchor) but had no way to locate that caller in the retail
  binary within the remaining budget — none of the 6 pre-verified anchors
  (`RepresentSamePart`, `ResolvePartWaitStates`, etc.) are close enough in
  the callgraph to pivot from cheaply, and a semantic `search_code` query
  for this shape returned only decompiler-exception noise (see below).

**Recommended fallback** if this patch needs a hook here: since
`ProcessConfig(const UserGuid&)` unconditionally forwards to
`ProcessConfig(PlayerTrackConfig&)` in the oracle, and *that* unconditionally
calls `TrackNumOfType`, a viable alternate strategy is to hook
`PlayerTrackConfigList::Process(std::vector<TrackType>&)` instead (the
higher-level driver that loops calling `ProcessConfig` once per config) —
it is a less surgical hook point but may be easier to re-derive later with
more budget (it has a more distinctive shape: `MILO_ASSERT` on entry twice,
a `tracktypes` vector assignment, then 3 parallel `reserve`+loop-`push_back`
calls before the `ProcessConfig` loop).

### 6. `GameGemDB::~GameGemDB()` — NOT FOUND (time-boxed)

The ctor is now pinned at `0x8276E4A8`; the destructor should be a small
function elsewhere in the same cluster (`0x8276Dxxx`–`0x8276E7xxx`) that
loops `delete mGameGemLists[i]` (which itself recurses into a non-trivial
`GameGemList::~GameGemList()`, since `GameGem` has a non-trivial destructor
per `GameGem.h`) then frees the vector's own backing buffer. Not isolated
within budget — several visually-similar loop-based candidates exist in
the same address range and none were decompiled/checked before time ran
out. **If resumed**: check callers of the shared `MemFree`-equivalent
(`PORT_MEMFREE`, `0x827bc430`) restricted to the `0x8276Dxxx`–`0x8276E7xxx`
range first (`list_xrefs` on `0x827bc430` returned empty in this session —
worth retrying, or walking `GameGemDB`'s known-good addresses ±0x800 bytes
and decompiling each).

### 7. `Band::NewPlayer(BeatMaster*, BandUser*)` — NOT FOUND

What was tried:
- **RTTI type-descriptor search**: found real RTTI strings for `GemPlayer`
  (`0x82c448bc`), `VocalPlayer` (`0x82c44934`), and
  `RealGuitarGemPlayer` (`0x82c448d4`) — confirming these classes *are*
  polymorphic and their vtables exist — but `list_xrefs` on all three
  TypeDescriptor addresses returned **zero cross-references**. Ghidra's
  RTTI/vtable analyzer does not appear to have run for this XEX (no
  `vftable`/`RTTI` named symbols exist either, confirmed via
  `search_symbols`), so there's no automated path from "type descriptor" to
  "constructor that references it."
- **`Band::` symbol search**: empty (expected, stripped).
- **Semantic `search_code`** with two different natural-language
  descriptions of `Band::NewPlayer`'s shape (switch on track type,
  dispatching to 3 different `new` calls) — every one of the top-5 hits for
  both queries came back as a Ghidra decompiler exception
  (`ghidra.util.exception.ClosedException: File is closed`), not usable
  results. This reproduces the same semantic-search unreliability noted
  earlier in this session for the RTTI/vtable-tracing path — confirmed
  again here, independently, on unrelated queries. **`search_code` should
  be considered unreliable for this binary/session and not retried without
  first checking whether the underlying Ghidra program handle issue has
  been fixed.**
- Looked for a `BuildInstrumentSelection`-adjacent lead (the plan doc's
  pinned `PORT_BUILDINSTRUMENTSELECTION` anchor, `0x82668c70`, resolves to
  the *containing* function `0x82668B98`, which calls into the already-
  verified `SongData::GetGemList` — confirming it's in the right
  neighborhood conceptually (instrument/track selection code) but it does
  not itself call anything resembling `NewPlayer`/`AddPlayer`, and its own
  callees didn't lead anywhere closer within budget).

**Recommended next step** if this patch needs a hook here: since the RTTI
type descriptors exist but aren't xref'd, a promising unexplored path is a
raw byte/data scan for the literal 4-byte pointer values `0x82c448bc`
(GemPlayer), `0x82c44934` (VocalPlayer), `0x82c448d4`
(RealGuitarGemPlayer) appearing anywhere in `.rdata` (they'd appear inside
each class's Complete Object Locator struct, which the vtable's slot `-1`
points to) — this doesn't require Ghidra's RTTI analyzer, just a plain
memory/data search, which the current MCP tool surface doesn't expose
directly (no generic "find this 4-byte value in the data section" tool was
available in this session — `read_bytes` only reads a given address, it
doesn't search). `scripts/dump_vtable.py` (COFF `.obj` symbol/relocation
based) was noted as available but not tried — it works from the *build*
side (this repo's own `.obj` files), not the retail XEX, so it wouldn't
directly give a retail VA, but could give the vtable *layout* (slot count,
which slot is the dtor, etc.) to help recognize the retail vtable once
found by other means.

## Struct field offsets (this Xenon/360 build, byte-confirmed where noted)

| Class | Field | Offset | Confidence |
|---|---|---|---|
| `SongData` | `mTrackDifficulties` (`std::vector<int>`) | `+0x50` | byte-confirmed (`GetGemList` disassembly) |
| `SongData` | `mGemDBs` (`std::vector<GameGemDB*>`) | `+0xb0` | byte-confirmed (`GetGemList` disassembly) |
| `SongData` | `mDrumMixDBs` (`std::vector<DrumMixDB*>`) | `+0xa4` | inferred (consistent across 2 call sites in `MakeBackupTracks`), not byte-confirmed independently |
| `TrackWatcherImpl` | `mSongData` | `+0x50` | from earlier session pass (prior to this compaction), not re-verified this pass |
| `TrackWatcherImpl` | `mTrack` | `+0x68` | from earlier session pass, not re-verified this pass |
| `TrackWatcherImpl` | `mGemList` | `+0x1c` | from earlier session pass, not re-verified this pass |
| `TrackWatcher` | `mImpl` | `+0x0` | byte-confirmed (`TrackWatcher::RecalcGemList` disassembly) |
| `GameGemDB` | `mGameGemLists` (`std::vector<GameGemList*>`) | `+0x0` | byte-confirmed (`GetDiffGemList`, `Duplicate`, ctor disassembly) |
| `GameGemDB` | `mHopoThreshold` | `+0xc` | byte-confirmed (`Duplicate`/ctor disassembly, `this[3]`) |
| `GameGemDB` | `sizeof(GameGemDB)` | `0x10` (16) | byte-confirmed (alloc size in `Duplicate`) |
| `GameGemList` | `mHopoThreshold` | `+0x0` | inferred from header order; ctor sets it via placement, not independently byte-checked at this offset |
| `GameGemList` | `mGems` (`std::vector<GameGem>`) | `+0x4` | inferred (CopyFrom operates on `this+4` directly) |
| `GameGemList` | `sizeof(GameGemList)` | `0x10` (16) | byte-confirmed (alloc size in ctor's per-element loop) |
| `GameGem` | `sizeof(GameGem)` | `0x44` (68) | inferred from `CopyFrom`'s reserve-divisor; not independently confirmed via `GameGem.h` field-by-field layout |
| `DrumMixDB` | `mMixLists` | `+0x0` | inferred (only member) |
| `DrumMixDB` | `sizeof(DrumMixDB)` | `0xc` (12) | byte-confirmed (alloc size in the rejected `0x8276E288` candidate, which is real `DrumMixDB::Duplicate`) |
| `PlayerTrackConfig` fields | — | — | **not derived — derive from 360 struct later** (no candidate function found to read offsets from) |

## Strings tried (all empty in retail — confirms plan doc's stripping claim)

`"Couldn't create track of type"`, `"before song is loaded"`, `"which is
obsolete"`, `"AssignTrack"`, `"PlayerTrackConfigList: Asked for active
UserGuid"` (implicitly, via symbol search only, not re-tried as a literal
string this pass).

## `ports_xbox360.h` fragment

```c
// --- Same-instrument patch: TrackWatcher/SongData/GameGemDB chain (VERIFIED 2026-07-07) ---
#define PORT_TW_RECALCGEMLIST        0x82778700  // TrackWatcher::RecalcGemList — VERIFIED (byte-exact tail-branch proof)
#define PORT_TWI_RECALCGEMLIST       0x8276FBB0  // TrackWatcherImpl::RecalcGemList — VERIFIED (centerpiece hook point)
#define PORT_SONGDATA_GETGEMLIST     0x8274BB38  // SongData::GetGemList(int) — VERIFIED (GetGemListByDiff inlined)
#define PORT_SONGDATA_GETGEMLISTBYDIFF 0x8274BB20 // SongData::GetGemListByDiff(int,int) — VERIFIED
#define PORT_GAMEGEMDB_GETDIFFLIST   0x8276E010  // GameGemDB::GetDiffGemList(int) [const+non-const folded] — VERIFIED
#define PORT_GAMEGEMDB_DUPLICATE     0x8276E590  // GameGemDB::Duplicate() const — VERIFIED
#define PORT_GAMEGEMDB_CTOR          0x8276E4A8  // GameGemDB::GameGemDB(int,int) — VERIFIED (bonus, not one of the 7 asked-for targets)
#define PORT_GAMEGEMLIST_COPYFROM    0x82769450  // GameGemList::CopyFrom(const GameGemList*) — VERIFIED

// --- NOT FOUND this pass — do not wire these up, no address available ---
// PORT_PTCL_PROCESSCONFIG      -- PlayerTrackConfigList::ProcessConfig(PlayerTrackConfig&) -- NOT FOUND
// PORT_PTCL_TRACKNUMOFTYPE     -- PlayerTrackConfigList::TrackNumOfType(TrackType) -- NOT FOUND
// PORT_PTCL_TRACKNUMOFEXACTTYPE -- PlayerTrackConfigList::TrackNumOfExactType(TrackType) -- NOT FOUND
// PORT_GAMEGEMDB_DTOR          -- GameGemDB::~GameGemDB() -- NOT FOUND (time-boxed)
// PORT_BAND_NEWPLAYER          -- Band::NewPlayer(BeatMaster*, BandUser*) -- NOT FOUND
```

## Tooling notes for whoever continues this

- `search_code` (semantic) is unreliable in this session — most results for
  novel queries come back as `ghidra.util.exception.ClosedException: File
  is closed` rather than real decompilations. Don't spend budget on it
  without first confirming the Ghidra program handle is healthy (e.g. try
  `decompile_function` on a plain known-good address first).
- `list_xrefs` returns empty for RTTI TypeDescriptor / vtable addresses —
  Ghidra's Windows-RTTI analyzer has not run (or doesn't apply) for this
  XEX. `search_strings` for `.?AV<ClassName>@@` still works and confirms
  polymorphism + gives the TypeDescriptor address, but there is currently
  no automated path from there to the vtable/constructor.
- Branch-target math for tail-calls (`b`/`bl` opcode `0x4B`, LI field =
  `instr & 0x03FFFFFC` sign-extended from bit 25, target =
  *instruction address* + LI — **not** function-start address) is a cheap,
  fully mechanical way to byte-verify a hypothesized call/tail-call
  relationship between two already-located addresses; used successfully
  above for `TrackWatcher::RecalcGemList` → `TrackWatcherImpl::RecalcGemList`.

---

## Layer-C derivation (round 2) — 2026-07-07 — ALL TARGETS PINNED

Round 1 reported `ProcessConfig`/`TrackNumOfType` as **NOT FOUND** (string/RTTI
stripped, no cheap caller-anchor). Round 2 pins the entire occupancy-gate
chain **VERIFIED** by walking the callee chain from the pinned `SongData` unit
(`SongData::FixUpTrackConfig → PlayerTrackConfigList::Process → ProcessConfig →
TrackNumOfType → TrackNumOfExactType`) and structural-matching each body against
the exact Wii oracle (`beatmatch/PlayerTrackConfigList.cpp`,
`beatmatch/SongData.cpp:268-275`). `PlayerTrackConfigList.cpp` is **not split**
in `config/45410914/splits.txt` (unmatched unit), which is why round 1's
split-scoped scan missed it — but Ghidra auto-analysis had created the functions
regardless. The unit lives **just below** `SongData.cpp` in `.text`
(`0x8274Axxx`, immediately before the first `SongData` split `0x8274BB20`),
i.e. `beatmatch` objects are contiguous.

### Summary (all VERIFIED, prologues HookFunction-safe)

| Target | Address | Prologue (1st word) | Evidence |
|---|---|---|---|
| **`PlayerTrackConfigList::ProcessConfig(PlayerTrackConfig&)`** ← **Layer-C hook target** | **`0x8274ACF8`** | `7d8802a6` mflr r12 | reads `cfg+0x10`(mTrackType), `!= 10`(kTrackNone), calls TrackNumOfType, on `-1`→fail handler `0x82756B70`, else stores `cfg+0x20`(mTrackNum)=num, `mTrackDiffs[num]`=`cfg+0x14`(mDifficulty), `mTrackOccupied[num]=1`. Two callers (see below). |
| `PlayerTrackConfigList::Process(vector<TrackType>&)` | `0x8274B530` | `7d8802a6` mflr r12 | vector-assign `mTrackTypes@0x18`, 3× reserve(`+0x00/+0x0c/+0x24`), loop push_back(mDefaultDifficulty@0x4c / i / 0), `if(mNeedsProcessing@0x50)` loop `mConfigs@0x30` stride `0x24` calling ProcessConfig, `mProcessed@0x51=1`. Single caller = FixUpTrackConfig. |
| `PlayerTrackConfigList::TrackNumOfType(TrackType)` | `0x8274ACA8` | `7d8802a6` mflr r12 | calls TrackNumOfExactType; on `-1`: `ty==7→ExactType(6)`, `ty==9→ExactType(8)`, else return -1. **A REAL separate function — NOT inlined** (round-1 plan predicted inlined; it was wrong). |
| `PlayerTrackConfigList::TrackNumOfExactType(TrackType)` | `0x8274AC50` | (thunk-save prologue) | loop `i<mTrackTypes.size()`(`(0x1c-0x18)>>2`): `if mTrackTypes@0x18[i]==ty && mTrackOccupied@0x24[i]==0 return i;` else -1. |
| `SongData::FixUpTrackConfig(PlayerTrackConfigList*)` (caller anchor, bonus) | `0x8274FDA8` | — | reserve local vec(`mNumTracks@0x10`), loop push_back(`&mTrackInfos@0x44[i]->mType@+0x10`), tail `plist->Process(types)` @ `0x8274fe1c`. |
| `PlayerTrackConfigList::ProcessConfig(const UserGuid&)` (late-join path, bonus) | `0x8274AE30` | — | `ProcessConfig(GetConfigByUserGuid(u))` → `GetConfigByUserGuid`=`0x8274AB18`, tailcalls `0x8274ACF8`. This is the 2nd witness into ProcessConfig + the `Game::AddPlayer` mid-song join route. |
| MILO_FAIL/TrackTypeToSym fail handler (ProcessConfig else-branch) | `0x82756B70` | — | builds Symbol from `.rodata @ 0x8210719c`, the "Couldn't create track of type %s…" formatter. |

**Two-witness confirmation of `0x8274ACF8`** (`list_xrefs`):
`from 0x8274b610` (inside Process `0x8274B530` — the song-start loop) **and**
`from 0x8274ae50` (inside ProcessConfig(UserGuid&) `0x8274AE30` — the late-join
forward). Both retail code paths funnel through this one address.

### Byte-confirmed retail struct offsets (feed Stage 3.4 / Stage 1.1 step 5)

`PlayerTrackConfig` (from ProcessConfig `0x8274ACF8` body):
- `mTrackType @ 0x10`, `mDifficulty @ 0x14`, `mTrackNum @ 0x20`, `sizeof = 0x24`
  (loop stride `/0x24` in Process). All match the plan's 12-byte-vector prediction.

`PlayerTrackConfigList` (from Process `0x8274B530` + ProcessConfig + ExactType):
- `mTrackDiffs @ 0x00`, `mTrackNums @ 0x0c`, `mTrackTypes @ 0x18`,
  `mTrackOccupied @ 0x24`, `mConfigs @ 0x30`, `mDefaultDifficulty @ 0x4c`,
  `mNeedsProcessing @ 0x50` (byte), `mProcessed @ 0x51` (byte).
  **All exactly match the plan's predicted 12-byte-`std::vector` layout** — the
  triple-vector spacing 0x00/0x0c/0x18/0x24 (12 bytes each) is byte-proven here.

`SongData` (from FixUpTrackConfig `0x8274FDA8`): `mNumTracks @ 0x10`,
`mTrackInfos @ 0x44` (begin ptr; matches round-1's `Function_8274C230`
observation). `TrackInfo::mType @ 0x10`.

**`kTrackNone = 10`** byte-confirmed (`cmp ...,10` in ProcessConfig) — matches
Stage-4.1's fix (`PTC_TRACK_NONE` must be `10`, not `-1`).

### RECOMMENDATION for the Layer-C hook

**Detour `ProcessConfig` at `0x8274ACF8` (single hook point).** Set
`PORT_PTCL_PROCESSCONFIG = 0x8274ACF8`. This is the correct, minimal target:

- It is the sole occupancy gate. Both retail entry paths route through it —
  song-start (`FixUpTrackConfig→Process`→loop) and mid-song late-join
  (`Game::AddPlayer→ProcessConfig(UserGuid&)`→`0x8274AE30`→here). **Fallback A
  (hook `Process` instead) is unnecessary** — its stated drawback ("late-join
  path stays stock, crashes on mid-song dup join") does not apply when hooking
  `ProcessConfig`, because the late-join path also funnels through `0x8274ACF8`.
- Prologue `7d8802a6` (`mflr r12`) is PC-relative-free → HookFunction-safe.
- Hook shape (matches the branch's `ProcessConfigHook`): read `cfg->mTrackType`
  (`+0x10`); vocals guard (`ty==3`→call original, Stage 4.6); call original;
  if the original left `cfg->mTrackNum` (`+0x20`) at its unassigned sentinel
  AND the type is occupied (i.e. the stock path would `MILO_FAIL`), instead pick
  an existing slot of the same exact type (occupancy-ignoring) and set
  `cfg->mTrackNum` to it **without** marking `mTrackOccupied`, so the shared
  track is reused and the gem-clone hook (`TrackWatcherImpl::RecalcGemList`
  `0x8276FBB0`) duplicates the gem list for the 2nd player.

**Stage-4.3 update:** `TrackNumOfType` is a **real function at `0x8274ACA8`**
(and `TrackNumOfExactType` at `0x8274AC50`), NOT inlined. So the C reimpl in
Stage 4.3 is optional — the hook can call `0x8274ACA8`/`0x8274AC50` directly if
preferred. Reimplementing in C (per Stage 4.3) is still fine and removes the
runtime dependency; either is correct. If wiring the real functions:
`PORT_PTCL_TRACKNUMOFTYPE = 0x8274ACA8`, `PORT_PTCL_TRACKNUMOFEXACTTYPE =
0x8274AC50` (both `this`-call, args in r3=list/r4=TrackType).

### `ports_xbox360.h` fragment (round 2)

```c
// --- Same-instrument Layer C: PlayerTrackConfigList occupancy gate (VERIFIED 2026-07-07 round 2) ---
#define PORT_PTCL_PROCESSCONFIG        0x8274ACF8  // ProcessConfig(PlayerTrackConfig&) — Layer-C hook target (both paths funnel here)
#define PORT_PTCL_TRACKNUMOFTYPE       0x8274ACA8  // TrackNumOfType(TrackType) — REAL fn (7->6/9->8), not inlined
#define PORT_PTCL_TRACKNUMOFEXACTTYPE  0x8274AC50  // TrackNumOfExactType(TrackType) — occupancy-respecting first-slot scan
// reference-only (derived, not necessarily wired):
#define PORT_PTCL_PROCESS              0x8274B530  // Process(vector<TrackType>&) — Fallback-A hook point (not needed)
#define PORT_PTCL_PROCESSCONFIG_GUID   0x8274AE30  // ProcessConfig(const UserGuid&) — late-join forward (Game::AddPlayer)
#define PORT_SONGDATA_FIXUPTRACKCONFIG 0x8274FDA8  // SongData::FixUpTrackConfig — caller anchor
```

### Method note (for future passes)

The winning route was: (1) oracle says `FixUpTrackConfig` (in the already-pinned
`SongData` unit) is the only runtime entry into `Process`; (2) enumerate Ghidra
functions in `0x8274B000-0x82754400` via `search_symbols('8274b'…)`; (3)
batch-decompile, filter for the FixUpTrackConfig fingerprint (reads `+0x44`
mTrackInfos in a loop + one call to a foreign function). That found
`0x8274FDA8`; its trailing call `0x8274B530` was `Process`; the loop-call inside
`Process` (`0x8274ACF8`) was `ProcessConfig`; its non-vector callee `0x8274ACA8`
was `TrackNumOfType`. Total ~10 decompiles. `search_code` (semantic) and
RTTI-xref (both flagged unreliable in round 1) were **not needed**. Adjacent-unit
callee-chain walking from a pinned neighbor beats string/RTTI hunting for
stripped non-polymorphic value classes.

---

## Layer-B enable (round 3) — 2026-07-07

The three Layer-B unknowns + the mGemList offset, all byte-derived from
`orig/45410914/band.exe` via `tools/va_disasm.py` (PE-section-correct VA map).
This closes the readiness gate: `SameInstReady()` now returns true.

### Values pinned

| Item | Value | Byte proof |
|---|---|---|
| `BANDUSER_OVERSHELLSTATE_OFF` | **0x20** (word) | `ResolvePartWaitStates` @0x8259d9b8: `lwz r11,0x20(r23); cmpwi cr6,r11,0xb` (compare vs kState_ChoosePartWait=11). Cross-proof: `SetOvershellSlotState` stores the state at the same +0x20. `lwz`/`stw` → word width → existing `*(int*)` read correct. |
| `PORT_BANDUSER_SETOVERSHELLSLOTSTATE` | **0x8266DB58** | `va_disasm 0x8266db58 16` → exactly `stw r4,0x20(r3); li r4,1; b 0x8266d2b8`. Standalone body of the predicted inline `{ mOvershellState=id; UpdateData(1); }` (tail-calls UpdateData@0x8266D2B8). **NOT inlined — the standalone fn exists, no C-reimpl needed.** |
| `PORT_OVERSHELLPANEL_UPDATEALL` | **0x8259E5B0** | Call site @0x8259dde8: `mr r3,r21; bl 0x8259e5b0` where r21=panel `this` (saved from r3 at prologue 0x8259d95c), immediately after the ChooseDiff(12) advance. |
| `TWI_MGEMLIST_OFF` | **0x1c CONFIRMED** | `RecalcGemList` @0x8276fbd0: after `lwz r4,0x68(r3)` (mTrack) + `lwz r3,0x50(r3)` (mSongData) + `bl 0x8274bb38` (SongData::GetGemList), `stw r3,0x1c(r31)` = mGemList store, then vcall `[vtable+8]` (HandleDifficultyChange). Was provisional; now byte-confirmed. |

### Call sites in ResolvePartWaitStates (@0x8259D948)

- ChooseDiff advance: `0x8259dddc: li r4,0xc; mr r3,r29; bl 0x8266db58` then `0x8259ddec: mr r3,r21; bl 0x8259e5b0` (SetOvershellSlotState(user,12) → UpdateAll(panel)).
- ChoosePart bounce: `0x8259dc8c: li r4,0xa; mr r3,r23; bl 0x8266db58` then `0x8259dc9c: mr r3,r21; bl 0x8259e5b0`.
- kState enum 10/11/12 (ChoosePart/ChoosePartWait/ChooseDiff) confirmed against these constants.

### UpdateAll identity (confirmation 1.2.1) — PASS (shape match)

`va_disasm 0x8259E5B0`: prologue → `bl 0x8259d280` (= `RefreshJoinableUsers()`) →
`lwz r11,0x74(r27); lwz r10,0x78(r27); addi r30,r27,0x74; subf r10,r11,r10`
(mSlots vector [begin@0x74,end@0x78], compute count) → iterate. Matches rb3 oracle
`OvershellPanel::UpdateAll`: `RefreshJoinableUsers(); for(i<mSlots.size()) mSlots[i]->UpdateState(); ...`.

### Pointer-type compatibility (confirmation 1.2.2) — PASS, no adjustment shipped

In the retail body the participant-vector element is MI-thunk-adjusted to reach
the BandUser subobject: `lwz r10,4(r11); lwz r10,8(r10); add r11,r10,r11; addi r23,r11,4`
(@0x8259d9a8–0x8259d9b4). Our hook instead feeds `GetBandUserFromSlot(mgr,i)`
(0x82682B60) directly. That is **correct without adjustment** because
GetBandUserFromSlot already returns the BandUser subobject: the shipped, working
`GameHooks.c` reads `bandUser->difficulty` (0x8), `->trackType` (0x10),
`->controllerType` (0x14) off that exact return value with no adjustment, and
`mOvershellState` (0x20) is the last field of that same struct. The MI thunk in
ResolvePartWaitStates adjusts the *vector element* (a different base pointer),
not what GetBandUserFromSlot hands back. **Shipped variant: un-adjusted.**

### Edits applied (uncommitted, branch feature/same-instrument)

- `include/ports_xbox360.h` L176–182: set `PORT_BANDUSER_SETOVERSHELLSLOTSTATE=0x8266DB58`, `PORT_OVERSHELLPANEL_UPDATEALL=0x8259E5B0`; rewrote the "STILL UNPINNED" block to "ALL PINNED"; noted UpdateData=0x8266D2B8 (reference-only).
- `source/SameInstrumentHooks.c` L64: `BANDUSER_OVERSHELLSTATE_OFF 0x20` (+ updated the comment block above it).
- `include/rb3/TrackWatcher.h`: `TWI_MGEMLIST_OFF` comment provisional→byte-confirmed; header block updated.

`SameInstReady()`: all 8 gates now nonzero → **returns true** (feature installs,
still gated by `config.AllowSameInstrument`, default false).
Syntax: `gcc -fsyntax-only -DRB3E_XBOX -Iinclude -Isource source/SameInstrumentHooks.c` → **EXIT=0** (only pre-existing `-Wpointer-to-int-cast` warnings from ppcasm.h's B() under x86).
