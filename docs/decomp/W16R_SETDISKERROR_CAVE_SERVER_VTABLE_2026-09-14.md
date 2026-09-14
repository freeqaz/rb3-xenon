# W16-R (Fable) — SetDiskError cave settled, Server vtable bounded, xapilib gap identified, `lbl_8217E3B8` decided

**Lane:** W16-R, escalation of W16-P (`2f4c00c5`) and W16-M. Worktree `~/tmp/wt-w16-r`, branch `w16-r` off
main `5c3340b6`. Baseline build `~/tmp/rb3_build_w16r_baseline.log` (rc=0).
**Status of this file:** INTERMEDIATE DRAFT committed at the coordinator's request; sections marked
`[VERIFIED]` rest on retail bytes measured in this lane, `[PENDING]` sections are not yet measured.
Probe helper: `tools/w16r_pe_probe.py` (all probes were heredocs over it; outputs under `~/tmp/w16r/`).

## 0. Verdict table

| # | item | claim tested | instrument | verdict | bytes |
|---|---|---|---|---|---|
| 1 | `0x82516320` SetDiskError `blr` + DataSet detour | "TU5 code cave" (brief) | TU0 vs TU5 vs `band.exe` bytes, PE timestamp, cave shape | **[VERIFIED] REFUTED as TU5 — it is an RB3DX post-link byte patch** (label `RB3DX-hook`); class bounded (11 `.pdata` extents + thunk region) | 0 (no re-carve, no body emptying) |
| 2 | `Server.h` 21 virtuals vs retail `??_7XboxServer` 19 slots | "decide the two surplus by caller vcall offsets" | caller vcall census on 3 Server receivers, wide census 0x3C–0x50, EH map after tables | **[VERIFIED] 19-slot bound firm; surplus pair NOT PROVEN by offsets ⇒ no header edit** | 0 |
| 3 | xapilib gap `0x8283C6AC–0x8283EB88` | "identify TU, pin if XDK, name only what is proven; then `fn_8251D378`" | import-ordinal decode, DC3 `sleep.obj` body twins, caller census, DC3 `PlatformMgr::Init` oracle | **[VERIFIED identification] see §3; [PENDING] pin + map names + set-diff** | pending |
| 4 | `lbl_8217E3B8` "vtable with no RTTI COL" | (a) fn-ptr table / (b) EH map / (c) real vtable | retail bytes: ctor store, slot targets, base vtable | **[VERIFIED] (c) real 2-slot vtable; class spelling unattested ⇒ not named** | 0 |
| 5 | 23 rows re-homed to `Server.cpp` | "name what item 2 proves" | — | slot 15 = `GetSecureConnectionClient@XboxServer` evidenced; surplus pair unproven ⇒ nothing further named | 0 |

## 1. [VERIFIED] SetDiskError cave — mechanism settled: RB3DX post-link patch, not a TU5 cave

Summary of the retail-byte evidence (details in the probe transcript):
- `orig/45410914/band.exe` carries the same PE timestamp as clean TU5 (`0x4E60C7FE`) yet differs from the
  clean TU5 image in a bounded set of `.text` words: the `blr` at `0x82516320`, the `DataSet+0x70` branch, and
  a thunk cluster — the shape of a hot-patch applied to the *linked* image, i.e. RB3DX (RB3 Deluxe) hooks,
  not a title-update rebuild. `config/45410914/config.yml`'s header ("clean TU5 sha1 c5a17091") is therefore
  WRONG about the image in use; correction pending in this lane.
- Class label for rows inside the patched extents: **`RB3DX-hook`** (NOT `TU5_PATCH_CAVE`, which presumes a
  mechanism the bytes refute). Treatment: no re-carve, no body emptying (W16-P already refuted both);
  `run_objdiff`'s "LikelyFixable" on these rows is a false positive by construction.

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

### 3b. [PENDING] pins + map names + measurement
Planned: `xdk/xapilibi/sleep.cpp` = `[0x8283D628,0x8283D668)` + `[0x828450F0,0x82845180)` (DC3 `sleep.obj`
membership); `xdk/xapilibi/xutilitydrive.cpp` = `[0x8283D818,0x8283EB88)` (**placeholder unit name — the
XDK object name is unattested**; cluster bounded by the caller census: every function in it is reached only from
inside the cluster except the two API entries). Map rows for the 7 names above. Predicted: Δ0 rows on both
rulers (pins are additions over `auto_*`; no named row pairs a base symbol our objs define). Measured: pending.

## 4. [VERIFIED] `lbl_8217E3B8` is a real 2-slot vtable — (c)
Stored by ctor `fn_82A87F20`; slots = deleting dtor `0x82A87F88`, `PostLogoutCleanup`-shaped `0x82A886B0`;
base vtable `0x8217E45C` (2 slots `0x82A88930`, `0x82A89670`). No COL because it sits in the Quazal/LSP
`/Od` region which emits no `??_R4`. The W16-M/W16-P location claim ("in `PlatformMgr_Xbox.s`") is STALE on
this tree — the label is in `auto_00_82000400_rdata.s`, referenced from `auto_03_82A87D2C_text.s`. Class
spelling attested only by an adjacent path string (`.\XboxLSP\Client\LSPBackEndServices.cpp`) ⇒ NOT named.
Corollary (unmeasured, not applied): `GetAccountManagementClient` is not virtual on 360 in this class.

## 5. Commits / NOT done — [PENDING]
