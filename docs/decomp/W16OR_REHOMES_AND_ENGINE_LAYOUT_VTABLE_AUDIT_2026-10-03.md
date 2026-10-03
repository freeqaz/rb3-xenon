# W16-OR — W16-OP's re-homes landed, `class_layout_report` sizeof fix, engine layout + vtable audit (2026-10-03)

**Branch** `w16-or`, worktree `~/tmp/wt-w16-or`, started off main `08777873d`,
rebased onto `4466f2a76` (W16-OQ landing) before the A/B.
**Scope** W16-OP's four deferred re-homes; `scripts/harvest/class_layout_report.py`;
classes defined in `src/system/{obj,utl,os,meta,ui,synth,track,beatmatch}`.
Not touched: `src/network` and the Quazal block (one header there,
`net/NetSession.h`, is *included* read-only), and `src/system/{char,world}`
(lane W16-OQ).
**Ruler** shipped `name_check` (report.json `provenance.diff_config`).

## 0. Result

| part | what | evidence |
|---|---|---|
| re-homes | 4 address runs moved to the TU that defines them; 1 new TU | every re-homed body we define pairs at fuzzy 100 (§1) |
| tool | `class_layout_report` reports the compiler's `sizeof` for virtual-base classes | FadePanel 196 → 200, BandMatchmaker 164 → 168; new selftest leg I (§2) |
| sizeof leg | `retail_sizeof_witness` over the 8 scoped dirs | 151 classes witnessed, **151 agree**, 0 defects (§3) |
| override leg | `vtable_override_pattern` over the 8 scoped dirs | 188 tables; 8 RETAIL_OVERRIDES → **0**; 3 of 25 OURS_OVERRIDES were real (§4) |
| A/B | whole binary | **+20 fns / +15 honest / +2,240 B** (predicted +2,240 B exactly); **0 rows down** (§5) |

## 1. Re-homes (W16-OP §7)

Every address below was read on retail bytes from the dtk asm, keyed on the
`.fn fn_<addr>` symbol. `splits.txt` `.text` lines only; dtk re-derived the
`.pdata` and the split-guard's one-build fixed point was committed with them.

| run | from | to | content |
|---|---|---|---|
| `0x8229CC10–0x8229CE30` | `Song.cpp` | `BandCharacter.cpp` | `BandSong::CreateSong` (344 B) + its 5 EH funclets. `BandCharacter.cpp` compiles `bandobj/Band.cpp`, which defines it. |
| `0x82665DE4–0x82666160`, `0x82666290–0x826662E0` | `UIList.cpp` | `FriendsProvider.cpp` | FriendsProvider's TU head: `InviteFriend` (0x82665DF0) + its 2 guard-reset funclets, a `Friend` sort comparator (0x82665F00), `InitData` (F70), `Text` (FD8), `Mat` (0x82666078), and two STL sort helpers (0x826660F0, 0x82666290). UIList.obj can define none of them. |
| `0x82667FE0–0x8266849C` | `PropKeys.cpp` | **new TU** `band3/meta_band/OvershellProfileProvider.cpp` | retail's whole OvershellProfileProvider TU: `Handle` (7FE0), `Text` (80D0), `Reload` (81A0), ctor (8278), `??_E` (8378), dtor (8388), `??_G` (8450) + guard/EH funclets. |

The brief named only `Mat`/`InitData` and the ctor. The whole runs were moved
because each run is one TU in retail. The neighbouring pins prove the
boundaries: StoreOfferProvider ends at `0x82665DE4`, FlowSlider owns
`0x82666160–0x826661CC`, BandLabel starts at `0x8266643C`, and CharProvider
ends at `0x82667FE0`.

**New bodies, all from retail bytes:**

- `FriendsProvider::InviteFriend(int)`: two function-local static Symbols
  (`invite_subject`, `invite_body`; guard bits 0, 1), then
  `TheNetSession->InviteFriend(mFriends[i], Localize(subject), Localize(body))`.
  `0x82CBFAF8` is `TheNetSession` (the same global `MetaPanel::OnSendBackSoundMsgToAll`
  and `UploadErrorMgr::Init` use). Slot 3 is `NetSession::InviteFriend(Friend*, const char*, const char*)`.
- `OvershellProfileProvider` (retail primary table `0x820D7954`, secondary
  `0x820D78FC`):
  - `Handle`: one local-static `num_data` → `NumData()`. It needs
    `/DRB3_HANDLE_LOCAL_STATIC`, set in objects.json.
  - `Text`: `dynamic_cast<AppLabel*>(label)`, then the static
    `overshell_swap_profile`, then
    `SetTokenFmt(sym, mUsers[data]->UserName())`. The `char*`/`const char*`
    instantiations are already a proven alias pair.
  - `Reload`: `mUsers.clear()`, then
    `BandUserMgr::GetLocalUsersWithAnyController(mUsers)`, then erase every
    user with `!IsSignedIn() || IsGuest()` (LocalUser virtual-base slots 4 / 3).
  - ctor: `mBandUserMgr(mgr)`, then `Reload()`.
  - dtor: empty.
  - `Mat` (`return 0`) and `NumData` (`mUsers.size()`) are defined in the same
    TU. In retail their bodies are ICF-folded onto `0x823591E8` / `0x82B7B280`,
    which matches retail's slots 2 / 10.
  - The Wii-only profile-swap stubs stay `#ifdef HX_NATIVE`.
- The old `#ifdef HX_NATIVE` OvershellProfileProvider glue in `OvershellSlot.cpp`
  is removed. It initialised `unk20..unk2c`, members the header does not
  declare, and it only survived because no build compiles that block.

Row results in the worktree build: OvershellProfileProvider unit 14/14 rows at
fuzzy 100. `CreateSong` and its 5 funclets reach 100. FriendsProvider
`InviteFriend`/`InitData`/`Mat` reach 100, and the two guard funclets move
100 → 100.

**Left at 0, deliberately:**

- `FriendsProvider::Text` (0x82665FD8): it uses `AppLabel::FriendRecord`, and
  unifying that with `Friend` is the naming bet W16-OP declined.
- The sort comparator and the two STL helpers: no source calls `std::sort`
  over `Friend*` in that TU.

## 2. `class_layout_report.py`: true `sizeof` for virtual-base classes

`/d1reportSingleClassLayout` prints `size(N)` up to the end of the virtual-base
region. It omits the tail pad to the class's alignment (W16-OP §1.2:
FadePanel prints 196, while retail and our fuzzy-100 `NewObject` both allocate
0xc8).

**Fix.** For every reported class with a virtual base, one more compile runs.
It uses a wrapper TU that `#include`s the real source and instantiates
undefined templates on `sizeof(T)` and `__alignof(T)`. The two `C2079`
diagnostics report the values.

- JSON keeps the printed value as `size_printed`.
- `size_source` is `compiler-sizeof`, or `printed-UNVERIFIED` when the probe
  cannot name the class.
- Text mode prints a note when the two values differ.

Two traps hit while building it:

- an absolute `/home/...` source operand is parsed by `cl` as an option
  (`D9002`), so the wrapper path is passed relative;
- `cl` prints the template argument on a following `N=200` line.

| class | printed | sizeof | `__alignof` | retail allocation (W16-OP) |
|---|---:|---:|---:|---:|
| FadePanel | 196 | **200** | 8 | 200 |
| BandMatchmaker | 164 | **168** | 8 | 168 |

**Selftest.** New leg I requires *both* printed == 196 *and* sizeof == 200, so
it fails if the probe is removed. Selftest: **8/8 PASS**.

## 3. sizeof leg: 151 scoped classes

`retail_sizeof_witness.py --json` (selftest: 925 witnesses, 750 classes,
PreloadPanel {164: 1}) found 151 witnessed classes defined in the 8 scoped
dirs. Classes whose name is also defined elsewhere in the tree were excluded.

**Instrument.** The per-class `class_layout_report` run cost about 2 min per
batch of 8 under the shared load, roughly 2 h in total. It was replaced by one
header-only TU: every declaring header plus `sizeof`/`__alignof` probes,
compiled in 4 batches of 45 to stay under MSVC's 100-error cap.

- **Control.** The 29 classes that already had a per-class compiler report
  agree **29/29** with the header-only probe.
- **Caveat.** A header-only TU does not carry per-TU `/D` flags such as
  `RB3_MAP_0x1C`, so it is a screen. Any disagreement would be re-measured
  in the class's own TU.

| verdict | count |
|---|---:|
| AGREE (retail == compiler) | 150 |
| `FilePath` retail 52 vs 12 | 1 → **witness limit, not a defect** |
| CONFLICT | 0 |

`FilePath` resolves on the witness sites. Both are `PoolAlloc(52)` →
`??0FileCacheEntry@@QAA@ABVFilePath@@…`, so the allocated object is a
`FileCacheEntry`. That struct is non-polymorphic and has a FilePath first
member, whose vtable is the "last offset-0 store" the witness keys on. The
compiler gives `sizeof(FileCacheEntry) == 52`, which **agrees**.

**Result: 151/151, no layout defect in scope.** Witness limit worth recording
in the tool: an aggregate whose first member is polymorphic is attributed to
that member.

## 4. Override-pattern leg: 188 primary tables

Scoped run: `compared 188, RETAIL_OVERRIDES 8, OURS_OVERRIDES 25, no_base 55,
parent_no_primary 10, ours_unreadable 3`. Every candidate was adjudicated on
retail bytes.

Hubs used below:
- `0x823591E8` = `li r3,0; blr`
- `0x82533618` = `li r3,1; blr`
- `0x826C3888` = `blr`
- `0x828299B8` = `_purecall` (calls the handler, then aborts)

**RETAIL_OVERRIDES (8, all real, all fixed):**

| class:slot | retail C (body) | fix |
|---|---|---|
| `UILabel:1` | `0x827F2410` `lwz r3,0x144(r3)` → tail-call vtable+4 | `GetDistanceToPlane` → `mText->GetDistanceToPlane` |
| `UILabel:7` | `0x827F23B8` `bl RndDrawable::Collide(mText)`, then `? this : 0` | `CollideShowing` |
| `UILabel:8` | `0x827F23F8` tail-call vtable+0x20 of `mText` | `CollidePlane` |
| `StorePanel:13` | `0x827B51F8` `UIPanel::PollForLoading`; `!TheNetCacheMgr->IsReady() && byte 0x2c` → `HandleNetCacheMgrFailure` | `PollForLoading` (protected); 0x2c = `mHasFailed`, compiler-verified |
| `UIButton:17` | `li r3,1` hub (UILabel's is `li r3,0`) | `CanHaveFocus() { return true; }` |
| `SongPreview:11` | `li r3,0` hub (`ContentMgr::Callback`'s returns ".") | `ContentDir() { return 0; }` |
| `CacheIDXbox:3` | `0x8252E068` `lwz r3,0x10(r3)` (folded with `ArkFile::Size`) | `GetDeviceID() const { return mContentData.DeviceID; }` (0x10) |
| `AsyncFileWin:15` | `li r3,0` hub, while **AsyncFile's own slot is `_purecall`** | AsyncFile's body is now `HX_NATIVE`-only; AsyncFileWin (and the non-retail AsyncFileHolmes) return false |

The CacheIDXbox, AsyncFile and NetCacheMgrXbox answers match DC3's
already-corrected headers, so these were drift in our copies. The four new
bodies (UILabel ×3, StorePanel) sit inside their own units' pins and were
named in the map.

**OURS_OVERRIDES (25): 22 fold-consistent, 3 real:**

- **`AsyncFile:15`**: the same defect as AsyncFileWin above (we overrode a
  slot retail leaves pure).
- **`NetCacheMgrXbox:6`** `Handle`: ours was a forwarder to
  `NetCacheMgr::Handle`. A forwarder is a distinct body that ICF cannot fold
  into its target, yet retail's slot *is* `NetCacheMgr::Handle` (`0x827CE8A8`,
  occupancy 2 = both tables). Removed; DC3 removed the same wrapper.
- **`Screenshot:5`** `DrawShowing`: ours had a real (editor-only) body, while
  retail's slot is the `blr` hub. Now spelled with `LOADMGR_EDITMODE` (false
  outside the native build), so the body is empty. It remains listed because
  the tool compares symbols, not bodies.
- The 22 fold-consistent rows are empty `Poll`/`Mount`/`Unmount`/`Delete`/
  `SetAutoSoloButtons`/`SetStereoPair`/`SetFXSend`/`SetADSR`/
  `UpdateExtendedCustom` bodies on the `blr` hub, plus `return 0` /
  `return false` / `return true` / `return 0.0f` bodies on their hubs
  (`BufFile::Truncate`, `MicNull::GetDroppedSamples`,
  `RealGuitarController::IsShifted`, `Profile::HasCheated`, RootContent's
  five, `GuitarController::GetCapStrip`). Each body was read and is identical
  to its parent's.

**Post-condition:** RETAIL_OVERRIDES **8 → 0**, OURS_OVERRIDES **25 → 23**.
Predicted 22; Screenshot's now-empty override is still a symbol.

## 5. A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-or-ab --patch <git diff
4466f2a76 w16-or -- src config scripts/target_symbol_map.json>`, fresh
`setup_worktree.sh` at main `4466f2a76`. Patch kinds: configgen, map,
source, splits. Run dir
`~/tmp/wt-w16-or-ab/.ab_measure_runs/20261003-043223-w16or_branch-2643657/`.

```
leg A: matched=53247 masked=25139 honest=28108 code%=57.360065  (recompiles: 0, settled)
leg B: matched=53267 masked=25144 honest=28123 code%=57.381924  (recompiles: 353, split=1, patch_steps=7, settle iterations: 2)
split fixed point: leg A converged after 0 extra re-split(s), leg B after 0
Δmatched=+20  Δmasked_equal=+5  Δhonest=+15  Δcode%=+0.021859pp  Δcode_bytes=+2240
Δfuzzy=+0.018261pp   (legA 63.553116 -> legB 63.571377)
units at 100% [mpn ruler]: 524 -> 524 (1 reached: OvershellProfileProvider NEW_UNIT; 1 fell off: FriendsProvider 9/9 -> 14/18)
units at 100% [all-rows-fuzzy ruler]: 463 -> 463 (same pair)
[control none] Δmatched_code=+2072 B -- NOT_APPLICABLE (kinds=configgen,map,source,splits)
```

**Prediction, written before the run** (§1 and §4 row reads):

| source | bytes |
|---|---:|
| `CreateSong` 344 + 5 funclets 99.3–99.9 → 100 | +544 |
| FriendsProvider `InviteFriend` 208 + `InitData` 100 + `Mat` 120 | +428 |
| OvershellProfileProvider 7 named rows 904 + 4 funclets 99.5 → 100 | +1,072 |
| UILabel ×3 | +100 |
| StorePanel | +96 |
| **total** | **+2,240 B** |

Functions were predicted at +15 named, plus up to 9 funclets if their `mpn`
was below 100.

**Measured: +2,240 B exactly; +20 fns** (15 named + 5 funclets whose `mpn`
crossed). The sizeof leg contributed nothing (no defect found). The override
leg's header-only fixes (UIButton, SongPreview, CacheIDXbox, AsyncFile,
NetCacheMgrXbox, Screenshot) change only vtable data and ICF-folded hub
bodies, so they were predicted at 0 B, and measured at 0 B.

**Unit level.** `PropKeys` −7 and `UIList` −2 are the re-homed rows leaving
those units. `FriendsProvider` leaves the at-100 set because its 4 unmatched
rows (Text, the comparator, 2 STL helpers) arrived from UIList, where they
were already 0.

**Row level.** Leg reports keyed `(unit, symbol)`, with a row missing from
leg B counted as down so the check can fail: 34 up, 33 "down". All 33 are
`MISSING_IN_B`, i.e. re-homed. Re-keyed **by address**, every one of the 33
is present in leg B at an equal or higher fuzzy and `mpn`, so **0 rows went
down.** The only in-place change outside the touched units is
`Song fn_827C7808` 99.8 → 99.9 (up).


## 6. Deliberately not done

- `FriendsProvider::Text`: the `AppLabel::FriendRecord` ↔ `Friend` naming
  bet stands (W16-OP §7).
- `tools/vtable_order_sweep.py` (count + order) was not re-run on the
  engine dirs; the brief named the two OP tools only.
- Secondary (non-primary) vtables, plus the 55 `no_base` / 10
  `parent_no_primary` / 3 `ours_unreadable` tables, are not covered by the
  override leg.
- `retail_sizeof_witness.py` was not changed for the aggregate-first-member
  limit found in §3; it is recorded here.

## Native gate (run last, after the A/B, on the committed tree `54d85961d`)

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

The native build globs `src/system/{os,utl}` and compiles UILabel and
Screenshot. So the AsyncFile `GetFileHandle` move (`AsyncFileNative` keeps
the HX_NATIVE body), the NetCacheMgrXbox `Handle` removal and the UILabel
overrides were all linked by it.
