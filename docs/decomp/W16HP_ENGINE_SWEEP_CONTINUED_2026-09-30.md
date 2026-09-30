# W16-HP — engine fuzzy sweep, continued; HM's four wrong names settled; BinStreamRev names retired

**Branch** `w16-hp`, rebased onto main `b9ef0b326`. **Ruler** `name_check` (graded,
read from `report.json` `provenance.diff_config`). Continues W16-HM
(`W16HM_ENGINE_FUZZY_SWEEP_2026-09-30.md`). Out of scope throughout:
`src/system/{bandobj,rndobj,char,rnddx9}` (another session).

## 1. Whole-binary A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-hp-base --patch <main..w16-hp diff>`,
leg A = main `b9ef0b326`, one run over the whole branch diff (36 files; kinds map +
splits + source, so both legs were force-re-split and read at a `symbols.txt`
fixed point). objdiff-cli pinned across legs; leg A settled at 0 recompiles, leg B
1,036.

```
leg A: matched=44982 masked=23422 honest=21560 code%=43.047062
leg B: matched=45011 masked=23432 honest=21579 code%=43.086998
Δmatched=+29  Δmasked_equal=+10  Δhonest=+19  Δcode%=+0.039936pp  Δcode_bytes=+4092
Δfuzzy=+0.030060pp   (legA 51.984646 -> legB 52.014706)
units at 100% [mpn]: 217 -> 218   [all-rows-fuzzy]: 191 -> 192   (PreloadPanel; 0 fell off)
```

**Row-level diff of the two archived leg reports: 36 rows up, 0 down.** 29 renamed
rows appear as GONE/NEW pairs; every one re-pairs at an equal or higher score,
**except one, on purpose**: `HAQManager::GetButtonText` (0x82BB1788, 460 B) read
20.93 fuzzy as a false pairing and is now the unnamed `fn_82BB1788` at 0 (§2).
It was never matched, so it costs fuzzy only.
The tree this lane hands over reproduces leg B exactly (45,011 / 4,415,152 B /
43.086998%).

Prediction before running: net positive, 0 rows down other than the HAQ
un-pairing. It held.

Validators on the final rebased tree: `tools/map_name_injectivity.py` OK (29,894
applied rows, injective); `tools/icf_alias_finder.py --validate` PASS (1,482
map-consistent, 254 tolerated, 0 contradicted).

## 2. HM's four wrong map names, settled on retail bytes

| HM's name | retail address | what it is | evidence | action |
|---|---|---|---|---|
| `PreloadPanel::Load` | `0x827B4668` | **`PreloadPanel::StartCache`** | sets `mMounted` (+0x50), `sCache->Clear()` / `StartSet(0)`, `preload_files` loop from index 1, `EndSet()`; called from inside both `0x827B4880` and `0x827B49E8` | renamed; the real **`Load` is `0x827B4880`** (slot 0 of `??_7PreloadPanel@@6BUIPanel@@@`) and **`PollForLoading` is `0x827B49E8`** (slot 13). Both were pinned into **StorePanel** -- `.text` `0x827B4880-0x827B4AE0` re-homed to PreloadPanel. `0x827B4B00` is `StorePanel::IsLoaded` (UIPanel::IsLoaded, TheContentMgr, state==2), so the boundary is at `0x827B4AE0` |
| `DevHostname(Symbol)` | `0x8250F898` | **`Debug::Poll`** | `MainThread()`, clear `mTry` (+0xc), `if (mFailThreadMsg) Fail(...)`, then `mNotifyThreadMsg` -> String, clear, `gNotifyThreadSync.Set()`, `Notify(str)` -- our Poll line for line | renamed; `0x8250F538` (its callee) named **`Debug::Notify`** (`mNoDebug` +4, `mModalCallback` +0x1c, `gNotifyThreadSec`, store to +0xfc, `Wait(200)`) |
| `OSCMessenger::MakeOSCAddress` | `0x82BB2A20` | **`VorbisReader::setupCypher(int)`** | `mKeyIndex` +0x70 -> `KeyChain::getKey`; `mMagicA/B` +0x68/+0x6c, `r4` (moggVersion) as the 6th arg and `TheSynth+0x38` (grinder) -> `GrindArray`; 16-byte XOR with `mKeyMask` +0x108; `ctr_start`. Sits between two VorbisReader functions, only caller is VorbisReader's `fn_82BB2E70`. No OSC string exists anywhere in retail | renamed; `.text 0x82BB2A20-0x82BB2BCC` re-homed OSCMessenger -> VorbisReader. **100** on the first build |
| `HAQManager::GetButtonText` | `0x82BB1788` | **unidentified** key-derivation routine | `void f(int version)` over a static crypt state (`0x82E4BE10`: key +0, mask +0x340, magic +0x360/+0x364, key index +0x368) with a local grinder; only caller is OggMap-pinned `fn_82BB1FD8`, which shares that static. `GetButtonText`'s `"%i %s | "` format string does **not exist** in retail | **name removed**, not replaced -- no oracle has this routine, so there is nothing true to put there |

## 3. Rows raised

Before = leg A, after = leg B (`fuzzy_match_percent`). Rows that crossed 100 in
**bold**. "renamed" rows compare against the row that sat at the same address.

| row | B | before | after | what fixed it |
|---|---:|---:|---:|---|
| `ObjectDir::PreLoad` | 3092 | 84.71 | 92.76 | RB3 rev dialect: plain int into the TU pair (initialised, alt first), raw stream to every reader; five funclets to 100 |
| `LightPreset::Load` | 3024 | 81.28 | 86.39 | same; residue = HM's deferred AddRef loops (ref model) |
| `CamShot::Load` | 2996 | 88.58 | 96.15 | same; residue = ObjPtrList/ObjRefConcrete fold names, `Key<float>` readers |
| `UIComponent::Update` | 1192 | 95.37 | 98.54 | `MILO_FAIL_RTL` at its four fail sites |
| `StandardStream::InitInfo` | 872 | 66.88 | 97.17 | rb3-Wii front half (float `i4/sampleRate` -- ours divided the stale `mInfoChannels` as int; 0xC000 blocks; no file receiver); residue = MsToSamp FP scheduling |
| `MoviePanel::Load` | 752 | 78.53 | 99.97 | rb3-Wii's inline `SystemLanguage`/`SupportedLanguages` lookup, two-arg `FileExists`, no `UsingCD()`; residue = ICF-folded `push_back` name |
| `UIComponent::SetTypeDef` | 500 | 82.30 | 96.72 | `MILO_FAIL_RTL`; residue = block placement |
| `BandSongMgr::ReadCachedMetadataFromStream` | 400 | 53.54 | 80.30 | no throwaway BinStreamRevs |
| `Hmx::Object::Property` | 388 | 91.41 | **100** | `MILO_FAIL_DTA` = `MiloStripEval` (right-to-left, by-value copy) |
| `PreloadPanel::Load` (renamed address) | 384->360 | 14.36 | **100** | settled to `0x827B4880` + re-home; DC3 `SetLoaderPeriod` native-only |
| `PreloadPanel::StartCache` (was "Load") | 384 | -- | **100** | renamed; no `SetSize`, two-arg `FileExists` |
| `PreloadPanel::PollForLoading` | 248 | 0 (StorePanel `fn_`) | **100** | named + re-homed; no `FileCache::PollAll` |
| `VorbisReader::setupCypher` (was OSC) | 428 | 10.49 | **100** | renamed + re-homed |
| `Debug::Poll` (was DevHostname) | 128 | 31.59 | **100** | renamed; `Fail(const char*)` (retail never loads r5) |
| `Debug::Notify` | 116 | 0 (`fn_`) | **100** | named; no main-thread `Modal` in retail |
| `BlockMgr::SpinUp` | 304 | 78.30 | **100** | no `UsingCD()` test; rb3-Wii CDRead args (last sector into `mBuffer+0xF800`) |
| `Hmx::Object::HandleProperty` | 240 | 79.67 | **100** | `MILO_FAIL_DTA` evaluates, never emits |
| `PanelDir::DisableComponent` | 224 | 81.25 | **100** | PanelDir's scatter-include no longer drags `ui/Utl.cpp` in, so `bl IsNavAction` survives the assert |
| `ArkFile::ArkFile` | 172 | 58.72 | **100** | `int mFail`; rb3-Wii initialiser list |
| `UIListMesh::Draw` | 152 | 30.61 | **100** | no edit-mode material save/restore |
| `SyncStore::RemoveSyncObj` | 128 | 80.91 | **100** | `MILO_FAIL_RTL` |
| `MiniLeaderboardDisplay::PreLoad` | 128 | 26.00 | **100** | RB3 rev dialect (aligned aggregate), bool read from rev 1 |
| `ArkFile::Fail` | 16 | 35.00 | **100** | `int mFail != 0` |
| `UIButton::PostLoad` (was `~BinStreamRev`) | 4 | 95.0 | **100** | renamed (§4); its vtordisp thunk 98.33 -> **100** |
| `ObjVector<SpotlightEntry>::~ObjVector` (was a Keys reader) | 4 | 95.0 | **100** | renamed (§4) |
| `CamShotCrowd` ObjVector reader | 92 | 96.96 | **100** | raw stream (§4) |
| funclets | 40-64 | 93.4-99.9 | 99.5-100 | 17 EH funclets in Dir, LightPreset, BandSongMgr, CameraShot, MoviePanel, Object |

Also in the map, Δ0 on every row and kept for truth: **`0x82510040` is
`SystemLanguage()`, not `SystemLocale()`**. It reads `0x82CC99A8`, which retail's
`SetSystemLanguage` body writes (`IsSupportedLanguage`, then `Terminate`/store/`Init`
or store). Renaming it alone dropped ten callers from 100 (AppLabel, DateTime,
LocaleOrdinal, RockCentral x6, StoreInfoPanel) because our source had followed the
old name; those call sites now say `SystemLanguage()`, as rb3-Wii does, and all
ten are back at 100. W16-L's "retail-proven `SystemLocale`" comment in
StoreInfoPanel rested on this map name, so the callee half of its proof was
circular. Its `const char*` half stands, and the comment now says so.

Examined and left:
- `MemFindHeap` / `MemFree` (TU structure): retail addresses `gHeaps` + `gNumHeaps` off one
  anchor shared with `MemFindAddrHeap`, so they are one TU in retail. Not a local fix.
- `HDCache::WriteDone`: `1 << (mWriteBlock % 32)` (rb3-Wii) measured **worse** (68.2 -> 64.1).
  Reverted.
- `NetStream::ClientConnect`: a rotated do-while loop compiled identically. Reverted.
- `Archive::Enumerate`: register allocation (the `MakeString<const char*>` spelling was kept as retail truth, Δ0).
- `HAQManager::PrintComponentInfo` @0x82390368 is another wrong name. The body pushes an
  `ObjOwnerPtr<CharClip…>` into an ObjVector and has no dynamic_casts. Left named; not identified.

## 4. BinStreamRev map names (coordinator scope)

Retail has no `BinStreamRev` (0 x `.?AVBinStreamRev@@`). 25 map rows in our half
were spelled with it. **21 retired**, each by changing the reader's source
signature to `BinStream&` **and** respelling the map row to the name our
compiled object now defines (verified per row against the object's symbol table
before renaming -- the back-references shift when `BinStreamRev` drops out, so no
name was edited by hand):

- **world/CameraShot (4)** -- `LoadSubPart`, `CamShotFrame::Load`, the
  `CamShotFrame`/`CamShotCrowd` `ObjVector` readers. `CamShot::Load` now reads a
  plain `int` into two initialised statics (alt, rev -- retail writes
  `lbl_82CC7490` +0/+4 off one base and the readers `lhz lbl_82CC7494`).
- **world/LightPreset (7 + 1)** -- `Keyframe::LegacyLoadP9`,
  `SpotlightDrawerEntry::Load`, five vector/`ObjVector` readers. Load's alt, rev
  and loading flag are separate initialised statics (+0 / +4 / +6).
  ⚠ **An aggregate was tried first and measured negative**: it turned
  `~AutoLoading`'s and `SpotlightDrawerEntry::Load`'s direct `stb`/`lhz` of
  `lbl_82CC6E92`/`lbl_82CC6E90` into `addi` + offset (100 -> 73.75 and 100 ->
  97.16). HM's rule holds: the aggregate is only safe where nothing else reads
  the rev directly. The eighth LightPreset row, "`operator>>(BinStreamRev&,
  Keys<float,float>&)`" at `0x824B1D98`, was a 4-byte `b ~vector<SpotlightEntry>`
  called from the Keyframe ctors/dtor and `~LightPreset`: it is
  **`ObjVector<SpotlightEntry>::~ObjVector`**, renamed (95 -> 100, four funclets
  to 100).
- **world/Spotlight (1), world/Crowd (2)** -- the `(BinStreamRev &)bs` casts
  become the raw stream.
- **obj/Dir (2)** -- the `Viewport` reader carries the rev check on `BinStream&`;
  `ObjectDir::PreLoad` reads into the TU pair and passes the raw stream everywhere.
- **ui/PanelDir (1)** -- no source change needed; map respelled and its proven
  T1 alias group's survivor/folded **swapped** (W17-LOAD pattern), reason
  recorded in the group's evidence. No new fold added.
- **ui/UIListState (1)** -- "`??1BinStreamRev@@UAA@XZ`" at `0x8280F488` is
  `b UILabel::PostLoad`, the branch target of `UIButton::PostLoad`'s vtordisp
  thunk (`0x8280F698` -> `0x8280F6A0`): renamed **`UIButton::PostLoad`**
  (95 -> 100; the thunk 98.3 -> 100).
- **hamobj/SongLayout (1), band3/meta_band/BandSongMgr (1)** -- `SongLayout::Load`
  reads a plain int; `ReadCachedMetadataFromStream` reads `unk114`/`unk11c`
  straight off the stream (no throwaway `BinStreamRev`s).

**Left (4), all blocked on another session's files:**

| row | why left |
|---|---|
| Crowd `list<RndMultiMesh::Instance>` reader `0x824E1DA0` | element reader `RndMultiMesh::Instance::Load(BinStreamRev&)` is rndobj's; Crowd keeps the cast at that call site (and at its `list<Transform>` reads, whose row is rndobj/MeshAnim's -- converting them charged WorldCrowd::Load 100 -> 99.95) |
| HamCamTransform `ObjVector<CharBlendBone::ConstraintSystem>` `0x822A7A10` | a `char/` reader; its alias group's folded member is a different type, so a swap would add a new spelling |
| TrackWatcherImpl `vector<Key<Vector2>>` `0x82480BF8` | instantiated through the scatter-included `rndobj/PartAnim.cpp` |
| UITransitionHandler `Key<RndMatAnim::TexPtr>` `0x8275CF68` | instantiated through the scatter-included `rndobj/MatAnim.cpp` |

## 5. Findings worth reusing

### 5.1 Retail has exactly one `bl` to `Debug::Fail`, from `Debug::Poll`
So no `MILO_FAIL_DTA` site reaches `Fail` in retail, and `Fail` takes one argument
(retail reads r3/r4, never r5). Two consequences, both measured:
- `MILO_FAIL_DTA` is `MiloStripEval(args)`.
- Some `MILO_FAIL` sites keep a **right-to-left, by-value** argument setup (a stripped
  varargs call), and others keep `((void)(args))`. A blanket switch of `MILO_FAIL` to
  `MiloStripEval` measured **+7 / −11 rows**: `CacheMgrXbox::Poll` fell 100 → 85.4 and
  CharBones rows fell. It was not kept. `MILO_FAIL_RTL` is the per-site spelling.

### 5.2 A scatter-include can dead-code a retail call
PanelDir.cpp scatter-includes UIListWidget.cpp, which scatter-includes Utl.cpp.
Once `IsNavAction`'s body is in the TU, MSVC proves the assert condition pure and
deletes the call. `__declspec(noinline)` does **not** stop this (measured Δ0).
Keeping the body out of the TU does.

### 5.3 `UsingCD()` is absent at two retail sites
Retail has no `UsingCD()` test in `MoviePanel::Load` or `BlockMgr::SpinUp`, and no
`UsingCD` getter sits beside `SetUsingCD`. I gated it per site, not globally.

### 5.4 BinStreamRev retirement: take the name from the object
When `BinStreamRev` leaves a signature, the MSVC back-reference numbering changes
(`AAVBinStream@@AAVBinStreamRev@@` becomes `AAVBinStream@@AAV0@`). Every respelling
here was checked against the compiled object's symbol table before the map was
touched. Where the element or list reader belongs to rndobj/char, keep the
`(BinStreamRev &)bs` cast at the call site: converting it charges the caller
against the other lane's still-Rev map name.

### 5.5 Four more thunk/getter identifications off retail bytes
Each of `0x827B4668`, `0x8250F898`, `0x8280F488` and `0x824B1D98` was settled from the
retail branch graph (vtable slots, vtordisp-thunk branch targets, `bl` callers),
not from the oracle.

## 6. What I did not do

- Nothing in `src/system/{bandobj,rndobj,char,rnddx9}`. Four BinStreamRev rows (§4)
  wait on those files.
- No new alias or fold. The one alias edit is the PanelDir survivor/folded swap,
  recorded in that group's evidence.
- `symbols.txt` untouched. The splits edits are the two re-homes in §2; the
  `.pdata` lines are dtk's re-derivation.
- No name invented for `0x82BB1788`; it was removed.
- `LightPreset::Load`'s AddRef loops are still deferred (HM's ref-model note stands).
- Permuter not run (standing directive).
- Of HM's ~170 unexamined rows I worked the top of the size × (100 − fuzzy)
  ranking and the rows whose callee sets showed a retail/DC3 divergence. The rest
  of the tail (mostly FFT/DSP scheduling, Geo, Spotlight geometry, vendor Xinput)
  is still open.

## 7. Native gate

`tools/native_build_gate.sh` on the branch tip `70a603948` (all source commits in
place), run after the A/B and the validators as the last build action:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

Only this docs-only commit follows it. It touches no build input.
