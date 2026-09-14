# W16-R (Fable) — SetDiskError cave settled, Server vtable bounded, xapilib gap identified, `lbl_8217E3B8` decided

**Lane:** W16-R, escalation of W16-P (`2f4c00c5`) and W16-M. Worktree `~/tmp/wt-w16-r`, branch `w16-r` off
main `5c3340b6`. Baseline build `~/tmp/rb3_build_w16r_baseline.log` (rc=0).
**Status of this file:** FINAL. Sections marked `[VERIFIED]` rest on retail bytes measured in this lane;
`[MEASURED]` sections carry whole-binary numbers from a full build + `report.json` in this worktree.
Probe helper: `tools/w16r_pe_probe.py` (all probes were heredocs over it; outputs under `~/tmp/w16r/`).

## 0. Verdict table

| # | item | claim tested | instrument | verdict | bytes |
|---|---|---|---|---|---|
| 1 | `0x82516320` SetDiskError `blr` + DataSet detour | "TU5 code cave" (brief) | TU0 vs TU5 vs `band.exe` bytes, PE timestamp, cave shape | **[VERIFIED] REFUTED as TU5 — it is an RB3DX in-place byte patch** (label `RB3DX-hook`); class bounded EXACTLY: 3 cross-extent branches, 0 in clean TU5; 19 `.text` words in 12 `.pdata` extents + 4 import-thunk words | 0 (no re-carve, no body emptying) |
| 2 | `Server.h` 21 virtuals vs retail `??_7XboxServer` 19 slots | "decide the two surplus by caller vcall offsets" | caller vcall census on 3 Server receivers, wide census 0x3C–0x50, EH map after tables | **[VERIFIED] 19-slot bound firm; surplus pair NOT PROVEN by offsets ⇒ no header edit** | 0 |
| 3 | xapilib gap `0x8283C6AC–0x8283EB88` | "identify TU, pin if XDK, name only what is proven; then `fn_8251D378`" | import-ordinal decode, DC3 `sleep.obj` body twins, caller census, DC3 `PlatformMgr::Init` oracle | **[VERIFIED identification] §3a; [MEASURED] 2 source-less pins + 7 map names, CROSSED IN 0 / FELL OUT 0, fuzzy +0.00094 pp from one new pairing** | 0 |
| 4 | `lbl_8217E3B8` "vtable with no RTTI COL" | (a) fn-ptr table / (b) EH map / (c) real vtable | retail bytes: ctor store, slot targets, base vtable | **[VERIFIED] (c) real 2-slot vtable; class spelling unattested ⇒ not named** | 0 |
| 5 | 23 rows re-homed to `Server.cpp` | "name what item 2 proves" | — | slot 15 = `GetSecureConnectionClient@XboxServer` evidenced; surplus pair unproven ⇒ nothing further named | 0 |

## 1. [VERIFIED] SetDiskError cave — mechanism settled: RB3DX in-place patch, not a TU5 cave

**Comparands (all hashes measured in this lane, worktree paths):**

| image | sha1 | what it is |
|---|---|---|
| `orig/45410914/default.xex` | `c5a17091cb44c0119424390a1738d161995e430e` | the image the build targets |
| `orig/45410914/default_tu5.xex` | `c5a17091…` (byte-identical) | same file |
| `/srv/torrents/cold-archive/games/arbys/RB3DX-Xbox/default.xex` | `c5a17091…` (byte-identical) | **RB3 Deluxe (RB3DX) release xex** |
| `_tu5probe/clean/clean_tu5.xex` → `band_clean_tu5.exe` | PE `5f3f667a689e804c1efc736cae4ff2416a278747` | genuine clean retail TU5 (`base TU0 + tu5/default.xexp`, produced 2026-07-07, `docs/plans/clean-tu5-vs-rb3dx-divergence.md`) |
| `orig/45410914/band.exe` (build-extracted from `default.xex`) | PE `2fbdbc6b279e05363517274ab5a09a9ea909ed88` | ours; contains the string `rbdxcache` (1 hit; clean: 0) |
| `orig/45410914/tu0-archive/default.xex` | `35adb6b4eadab3b0aae354e20ed45781ff0b8fc8` | vanilla retail TU0 |

⇒ **The image we target is the RB3DX-lineage TU5, not clean retail TU5.** `config/45410914/config.yml`'s
header is *correct* about the hash (`c5a17091` IS `default.xex`) and *silent* about lineage — it says "TU5
bytes in place … the binary all downstream consumers run (RB3Enhanced, same-instrument patch, players)", it
never says "clean". (An earlier draft of this section called the header "WRONG"; that was an overclaim and is
withdrawn — the correct statement is *hash right, lineage unstated*.) The 2026-07-07 divergence doc already
recommended targeting clean TU5 and measured RB3DX = clean TU5 + **170 bytes / 53 words**; that recommendation
was not taken up, and nothing in this lane changes it — see NOT-done.

**Mechanism, on retail bytes (both images decode with identical section tables, identical XEX headers, identical
PE timestamp `0x4E60C7FE` = Fri Sep 02 04:11:42 2011, LINK 10.0.10224.0, XAPILIB 2.0.11164.0 — so this is NOT
a relink; it is a byte patch applied to the linked TU5 image):**

| VA | clean TU5 (A) | ours / RB3DX (B) |
|---|---|---|
| `0x82516320` | `mflr r12` (SetDiskError prologue) | `blr` — **function neutered** |
| `0x82516324` | `bl __savegprlr` | `lwz r4,4(r3)` |
| `0x82516328` | `addi r31,r1,-0x80` | `cmpwi r4,2` |
| `0x8251632C` | `stwu r1,-0x80(r1)` | `beq 0x82516334` |
| `0x82516330` | `lwz r11,0x34(r3)` | `b 0x8275D6F0` (→ `DataSet` epilogue) |
| `0x82516334` | `mr r29,r3` | `mr r4,r30` |
| `0x82516338` | `cmpwi cr6,r11,3` | `b 0x8275D6E4` (→ back into `DataSet`) |
| `0x8275D6E0` (`DataSet+0x70`) | `mr r4,r30` | `b 0x82516324` (detour into the freed head) |

So the patch (a) disables `PlatformMgr::SetDiskError` at its entry (the 8 callers are unchanged; the 180 B body
after the 7-word head is byte-identical to clean and EH-bound as W16-P found) and (b) reuses its dead head as a
5-instruction trampoline for `DataSet`: only perform the two calls when the node type (`r11->[0xC]`, read via
`lwz r4,4(r3)` on the patched path) `== 2`, else jump straight to the epilogue — a robustness guard, in DX, not
a TU5 fix. TU0 corroborates independently: TU0's SetDiskError body signature is absent at the TU5 address (0 hits)
and TU0's `DataSet` (at `0x82738AD0`) has a normal `mr r4,r30` at `+0x70`, i.e. the cave does not exist in TU0
either — it exists in exactly one of the three images, the RB3DX one.

**Bounded class, EXACT (instrument: `~/tmp/w16r/census.py` — every `b`/`bc` whose source lies inside one `.pdata`
extent and whose target lies strictly inside ANOTHER extent, not at its entry; run on all three images):**

- ours − clean = **3** hits: `82516330→8275D6F0`, `82516338→8275D6E4` (both in extent `[82516320,825163F0)` →
  `[8275D670,8275D6FC)`) and `8275D6E0→82516324` (the reverse). **clean − ours = 0.** Non-helper cross-extent
  branches: ours 10, clean 7 (the 7 are pre-existing Quazal-region tail branches at `0x82AB1xxx`/`0x82B19xxx`,
  present in both images, not caves). ⇒ **W16-P's `blr`-census and the brief's cave hypothesis both reach the
  same single cave; there is no second one.** (The TU0 leg of the census reads 14,680 and is NOT comparable —
  it is a `.pdata`-parse artefact of the TU0 layout, noted and not investigated.)
- Whole-image word diff, ours vs clean: **53 words / 10 patch groups** (from `_tu5probe/clean/dx_vs_retail_diff.txt`,
  2026-08-29, ⚠ gitignored scratch — index preserved here so the rows are not re-hunted):

| group | words | VA(s) | row(s) in `report.json` (unit, size, fuzzy) | effect |
|---|---|---|---|---|
| 1 MOGG AES key path | 26 | `0x8200068C/90` (.rdata import ordinals), `0x82C4C47C–90` (4 import-thunk words, no `.pdata`), `0x82727698,B8,D4,DC`, `0x82840794,98,C8`, `0x82840828–30,68–70,B4`, `0x82C76258–94` (.data 64 B key table) | `fn_82727688` `default/ByteGrinder` 108 B @0; `fn_82840788` 148 B @0 and `fn_82840820` 164 B @0 in `auto_03_8283F238_text` | xboxkrnl `XeKeysSetKey/XeKeysAesCbc` (0x242/0x24B) → `XeCryptAesKey/XeCryptAesCbc` (0x159/0x15B), stack AES state, replaced keys |
| 2 anti-debug | 1 | `0x82272E90` | `main` `default/Main` 76 B @96.84 | `bl App::Run` → `bcl` to `RunWithoutDebugging` |
| 3 splash/ESRB skip | 2 | `0x82270F40`, `0x82270F84` | `fn_82270E68` `default/RhythmDetector` 1,864 B @0 (⚠ unit attribution suspect — this is `App::Init`-shaped, not RhythmDetector; not re-pinned here) | `beq`→`nop`, `bl`→`nop` |
| 4 SetDiskError neutered + trampoline | 7 | `0x82516320–38` | `?SetDiskError@PlatformMgr@@QAAXW4DiskError@@@Z` `default/PlatformMgr` 208 B @85.17 | this section |
| 5 DataSet type guard | 1 | `0x8275D6E0` | `?DataSet@@YA?AVDataNode@@PAVDataArray@@@Z` `default/DataFunc` 140 B @98.29 | `mr r4,r30` → `b 0x82516324` — **its whole residue is this one word** |
| 6 UGC check disabled | 1 | `0x82575F9C` | `?IsDemo@BandSongMgr@@QBA_NH@Z` 164 B @98.54 | `bne`→`nop` ⇒ always false |
| 7 special-song table disabled | 1 | `0x82579098` | `?AddSongData@BandSongMgr@@…` 652 B @99.60 | `bl 0x825755B8`→`li r3,0` (lane CR-3 found this independently) |
| 8 content prefix | 2 | `0x82089B40/44` (.rdata) | string `"UPDATE:"`→`"D:"` | mounts content from `D:` |
| 9 song cache name | 1 | `0x82089518` (.rdata) | `"songcache"`→`"rbdxcache"` | the lineage fingerprint |
| 10 strcpy→strncpy | 1 | `0x82AE6880` | `fn_82AE6860` `auto_03_82AE5FE8_text` 52 B @0 | `bl 0x82BBCB50`→`bl 0x8282B238` |

(The 8 unchanged SetDiskError call sites — `0x8227153C, 0x825338E0, 0x82533AE4, 0x82533B14, 0x82533BE0,
0x8253566C, 0x82B8C730, 0x82B8C7D8` — sit in `AsyncTask`, `CDReader`, `AsyncFile_Win`, `StreamChecksum` rows;
they are byte-identical in both images and are NOT in the class.)

**Class label and treatment:** rows whose residue is inside a group above are **`RB3DX-hook`** (NOT
`TU5_PATCH_CAVE` — that presumes a mechanism the bytes refute). Structurally unmatchable against *this*
image by source work: no re-carve, no body emptying (W16-P refuted both), and `run_objdiff`'s "LikelyFixable"
on groups 4/5/6/7 is a false positive by construction. **The only way these rows reach 100 is targeting clean
TU5** (`5f3f667a`), where the 2026-07-07 doc measured the map transferring at 100.000% (12,817/12,817).
Predicted payout of that flip, priced from this table: `DataSet` +140 B, `IsDemo` +164 B, `AddSongData` +652 B
crossing (their residues are exactly one word each — `IsDemo`/`AddSongData` residues not re-verified against
report.json charged sites in this lane); `SetDiskError` +208 B iff the remaining 14.8% residue is only the
head (not verified); `main` likely (76 B, one word) — **not measured, out of this lane's scope.**

## 2. [VERIFIED] `??_7XboxServer` 19 slots vs `Server.h` 21 virtuals — bounded, not decided

- Retail table at `.rdata 0x8205793C`: 19 slots, then the EH unwind map; `??_7Server` at `0x820577BC` likewise.
  Slot 15 body `0x823EC788` = `GetSecureConnectionClient`; slots 16–18 are ICF-folded `li r3,0; blr` (`0x823591E8`).
- Caller census over the three retail receivers (`TheNet.mServer` `0x82CBFAEC`, `lbl_82C6EB50`,
  `gXboxServer` `0x82CBFD14`): max vcall offset **0x38** (= slot 14). 204 wide-census vcall sites at
  0x3C–0x50 have zero Server receivers ⇒ bound is firm at 19.
- Surplus pair must come from {GetAccountManagementClient, GetMasterProfileID, CreateProfile, DeleteProfile,
  GetCustomAuthData}. `GetCustomAuthData` cannot be one of the null-stub folds unless its body changed;
  caller evidence leans `GetMasterProfileID`, but NO caller offset pins the second. **Verdict: NOT PROVEN ⇒
  `src/network/net/Server.h` is NOT edited** (a wrong removal shifts every later slot). Recorded here so the
  next lane starts from the bounded set, not from 21-choose-2.

## 3. xapilib gap `0x8283C6AC–0x8283EB88` (+ `RtlSleep` at `0x828450F0`)

### 3a. [VERIFIED] identifications (evidence class per row)
| retail | name | evidence |
|---|---|---|
| `0x8283D628` (56 B) | `SleepEx` | instruction-for-instruction twin of DC3 `sleep.obj:SleepEx` (`0x82DDC2D0`) |
| `0x828450F0` (144 B) | `RtlSleep` | DC3 `sleep.obj:RtlSleep` (`0x82DDC248`) with `RtlpFormatTimeOut` inlined; ⚠ this lane first said `SleepEx` — WITHDRAWN once DC3's trio was read |
| `0x8283D660` (8 B) | `Sleep` (already in map) | `li r4,0; b RtlSleep` |
| `0x8283D810` (8 B) | `XNotifyCreateListener` | `li r4,5; b` xam ordinal **0x28A = XamNotifyCreateListener** (XEX import stub = `01 MM OOOO/02 MM OOOO/mtctr/bctr`, decoded via jeff `xex_imports.rs`); DC3 `PlatformMgr::Init` calls `XNotifyCreateListener(0xA7)` at the same position retail calls this with `r3=0xA7` |
| `0x82531980` (100 B) | `?Init@WinSockSocket@@SAXXZ` | body = DC3 `NetworkSocket_Win.cpp:292` (`sInit` byte, `XNetStartupParams{0xD,1}`, `XNetStartup`, `WSAStartup(0x202)`); second caller is `??0WinSockSocket`. (The brief's refuted `XNetStartup` guess is consistent: this function *calls* `XNetStartup` at `0x8284D7E8`, it is not it.) |
| `0x8283EAE0` (72 B) | `XMountUtilityDrive(BOOL fFormatClean, DWORD dwBytesPerCluster, DWORD dwFileCacheSize)` | XDK-semantic, no byte twin (DC3 has no utility-drive object and 0 string hits): `r4 = fFormatClean ? 0xF : 0`, `dwBytesPerCluster` range-checked (`> 0xFFFFF` ⇒ `ERROR_INVALID_PARAMETER 0x57`), strings `\Device\Harddisk0\Cache%u\`, `\??\cache:`, sets mounted flag `0x82E08C54 = 1` |
| `0x8283EB28` (60 B) | `XUnmountUtilityDrive` | `*0x82E08C54 == 0 ⇒ 0x10D0 ERROR_NO_MEDIA_IN_DRIVE`; callee `0x8283E348` deletes the `\??\cache:` symlink and **clears the mounted flag** (`*(0x82E08C50+4) = 0`) — a Flush that cleared the flag would make the next Flush fail, so Flush is excluded |
| `0x8251D378` (220 B) | `?Init@PlatformMgr@@QAAXXZ` | all callees now resolved: SetName (vbase vcall 0x40), XMountUtilityDrive, XUnmountUtilityDrive (probe: result byte kept at `this+0x27`), WinSockSocket::Init, XOnlineStartupEx (W16-P), XNotifyCreateListener(0xA7), UpdateSigninState, ThreadCall — DC3 `PlatformMgr_Xbox.cpp:175` oracle in the same order minus the utility-drive probe. ⚠ our tree DECLARES `Init` (`PlatformMgr.h:268`) but defines it in neither `PlatformMgr*.cpp` ⇒ naming is identification only, Δ0 by construction |

Second caller of the mount pair: `fn_82561168` (job handler: `this+4==2` ⇒ `XMountUtilityDrive(TRUE,0x8000,0x8000)` then callback on `this+0x20`; `this+4==0xC` ⇒ `XUnmountUtilityDrive()`), unnamed.

### 3b. [MEASURED] pins + map names — commit `c94b6019`
Applied: `xdk/xapilibi/sleep.cpp` = `.text [0x8283D628,0x8283D668) + [0x828450F0,0x82845180)` (DC3 `sleep.obj`
membership: SleepEx/Sleep/RtlSleep); `xdk/xapilibi/xutilitydrive.cpp` = `.text [0x8283D818,0x8283EB88)`
(**placeholder unit name — the XDK object name is unattested**; cluster bounded by the caller census: every
function in it is reached only from inside the cluster except the two API entries). Both `NonMatching` with no
source (repo convention for XDK pins). 7 map rows: `0x8251D378 ?Init@PlatformMgr@@QAAXXZ`,
`0x82531980 ?Init@WinSockSocket@@SAXXZ`, `0x8283D810 XNotifyCreateListener`, `0x8283D628 SleepEx`,
`0x828450F0 RtlSleep`, `0x8283EAE0 XMountUtilityDrive`, `0x8283EB28 XUnmountUtilityDrive`.

Build: first full build rc=1 on the split-guard ("the split rewrote its own input" — derived `.pdata` lines
added, by design; `~/tmp/rb3_build_w16r_pin1.log`); second full build rc=0 (`~/tmp/rb3_build_w16r_pin2.log`,
`[APPLIED] 3096 files checked, 1823 files patched`). ⚠ `Sleep` (8 B leaf at `0x8283D660`) got NO derived
`.pdata` record — expected, an 8-byte leaf touches neither stack nor LR.

**Predicted:** Δ0 on every measure (additions over `auto_*` reattribute only; no named row pairs a base symbol
our objs define). **Measured** (`tools/rowset_snapshot.py diff`, int-coerced):

| measure | before | after |
|---|---|---|
| fuzzy==100 row set | — | **CROSSED IN 0 / FELL OUT 0** |
| `matched_functions` | 43,214 | 43,214 |
| `matched_code` | 3,938,584 | 3,938,584 |
| `fuzzy_match_percent` (aggregate) | 49.33902 | 49.33996 (**+0.00094 pp**) |

**The prediction was wrong on the last line, and the reason is a finding:** `?Init@WinSockSocket@@SAXXZ`
(100 B) newly PAIRS with our `NetworkSocket_Win.obj` at **fuzzy 95.0 / mpn 96.0** — I had assumed our tree does
not define `WinSockSocket::Init`; it does (`src/system/os/NetworkSocket_Win.cpp:309`). 95 B / 10,245,956 B =
0.00093 pp, which is the whole delta. The exposed residue: ours is 104 B vs retail 100 B, one extra
`stb r10, 0x51(r1)` and an r9↔r10 swap around the `XNetStartupParams` init (`cfgSizeOfStruct=0xD, cfgFlags=1`
after `memset`). Not fixed — a source row, out of this lane's item list; recorded as naming-exposed residue.
New units appear at 0 as predicted: `default/xdk/xapilibi/sleep` (SleepEx 56 B, Sleep 8 B, RtlSleep 144 B),
`default/xdk/xapilibi/xutilitydrive` (18 fns incl. the two API entries 72 B / 60 B).
`?Init@PlatformMgr@@QAAXXZ` 220 B reads 0 in `default/PlatformMgr_Xbox` (declared, never defined in our tree).
`XNotifyCreateListener` 8 B sits in `auto_03_8283D668_text` (the `0x8283D668–0x8283D818` gap is NOT pinned).

## 4. [VERIFIED] `lbl_8217E3B8` is a real 2-slot vtable — (c)
Stored by ctor `fn_82A87F20`; slots = deleting dtor `0x82A87F88`, `PostLogoutCleanup`-shaped `0x82A886B0`;
base vtable `0x8217E45C` (2 slots `0x82A88930`, `0x82A89670`). No COL because it sits in the Quazal/LSP
`/Od` region which emits no `??_R4`. The W16-M/W16-P location claim ("in `PlatformMgr_Xbox.s`") is STALE on
this tree — the label is in `auto_00_82000400_rdata.s`, referenced from `auto_03_82A87D2C_text.s`. Class
spelling attested only by an adjacent path string (`.\XboxLSP\Client\LSPBackEndServices.cpp`) ⇒ NOT named.
Corollary (unmeasured, not applied): `GetAccountManagementClient` is not virtual on 360 in this class.

## 5. Commits / NOT done

**Commits on `w16-r`** (no Co-Authored-By trailers; exact file lists):
- `89ad13df` — intermediate state: this doc's first draft (cave settled, vtable bounded, gap identified).
- `c94b6019` — `config/45410914/splits.txt`, `config/45410914/objects.json`, `scripts/target_symbol_map.json`:
  the two source-less pins + 7 names, with the measured numbers of §3b in the message.
- (this commit) — this doc, final.

**Native gate:** NOT run, and not required — **no file under `src/` was edited in this lane** (config, map and
docs only). Say so rather than paste a gate line for a change that does not exist.

**NOT done (explicit):**
- `Server.h` surplus pair — UNDECIDED (bounded to a 5-candidate set, `GetMasterProfileID` favoured by callers,
  second member unpinned by any vcall offset). `src/network/net/Server.h` untouched. Item 5 therefore not done
  beyond slot 15.
- `lbl_8217E3B8`'s class NOT named (spelling attested only by an adjacent path string); `symbols.txt` untouched
  (the label is correct as a vtable, so nothing to correct).
- The XDK object name for the utility-drive cluster — `xutilitydrive.cpp` is a placeholder heading.
- Gaps still unpinned: `0x8283C6AC–0x8283D628` and `0x8283D668–0x8283D818` (DC3 `closehandle.cpp` /
  `getoverlappedresult.cpp` twins could pin `CloseHandle 0x8283D2D8` / `XGetOverlappedResult 0x8283D320`).
- `WinSockSocket::Init` 104-vs-100 B residue exposed by the naming — not fixed.
- `config/45410914/config.yml` header — NOT edited. Hash verified correct; recommend one added line
  "(this is the RB3DX-lineage TU5, = RB3DX-Xbox/default.xex; clean retail TU5 is `_tu5probe/clean/clean_tu5.xex`,
  PE sha1 5f3f667a…)". Not done here because a `config.yml` touch forces a full re-split for a comment.
- The clean-TU5 retarget (2026-07-07 recommendation) — NOT attempted; priced only by row count above.
- Group 3's row (`0x82270E68` attributed to `default/RhythmDetector`, 1,864 B @0) looks mis-attributed; not
  re-pinned.
- `IsDemo` / `AddSongData` "residue is exactly one word" — read from the word diff, NOT re-verified against
  `report.json`'s charged-site list.
