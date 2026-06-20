# W9 L2 — SongUpgradeMgr neighbour-TU identification (0x82632C98+)

Date: 2026-06-20. Mode: adversarial discover/planner (READ-ONLY in main @812e1df).
Frontier item: `songupgrademgr-neighbour-tu-id` (kind=pin-neighbour, est +10).

## TL;DR — VERDICT: REAL_ACTIONABLE

The neighbour cluster after SongUpgradeMgr splits into **two** TUs, not one:

1. **A small unidentified manager TU** `[0x82632C98, 0x82632F00)` — 6 fns, a
   `ContentMgr::Callback` sibling with `hash_map@0x1c` + `bool@0x38` + DataArray
   iteration, reusing SongUpgradeMgr's hash_map COMDATs. **NOT** LicenseMgr (set,
   not hash_map), **NOT** SongStatusMgr (its worktree pins are elsewhere). →
   emitted as `discovered_frontier` (needs class ID).

2. **`Instarank.cpp`** `[0x82632F00, 0x826340F0)` — **RTTI-confirmed** (type
   descriptor `.?AVInstarank@@` @ vtable 0x820cf1a4 / R4 0x821dd6f4 / TD
   0x82c44014). 48 fns / 4592 bytes. Oracle: `../rb3/src/band3/meta_band/Instarank.{cpp,h}`
   (155-line cpp, full bodies). Wired-status: **UNWIRED** (not in objects.json).
   This is the clean, high-confidence actionable pin (port+wire+pin one worktree).

The frontier's anchor list (`fn_82632C98/CC0/D18/E38/F00`) was the *manager* TU,
NOT the highest-value target. The richest oracle-backed win in this cluster is
Instarank (fn_82632F00+), which the frontier under-weighted.

## Ground truth gathered

### SongUpgradeMgr boundary (decisive)
- Sibling branch `w9-w9-songupgrademgr-port-pin-convert` @2a962d4 (NOT on main):
  pin `.text [0x82630A98, 0x82632C98)`, `.pdata [0x82224680, 0x82224908)`.
- Its split `.s` last fn = `fn_82632C18` (+128 ends exactly at 0x82632C98).
  86 target fns; all SongUpgradeMgr/SongUpgradeData methods captured.
- Manager-TU pdata begins exactly at VA 0x82224908 → confirms SongUpgradeMgr
  ends at 0x82632C98, neighbour begins there.

### Instarank `.text` map (RTTI + body-confirmed)
Singleton/`this`-recovery thunks (82803f2c/34/38/3c) = funclet frame helpers.
Members (retail, == rb3-Wii header): vbase Hmx::Object @+0x34 (dtor `this-0x34`),
mIsValid@0x4, unk8@0x8, unkc@0xc, mScoreType@0x10, mInstaRank@0x14,
mIsPercentile@0x18, mStr1@0x1c, mStr2@0x28. (const methods receive the +8
vbase-adjusted `this`, so HasHighscore/UpdateString1Label read mStr1@0x24.)

| VA | bytes | method | evidence |
|----|----|----|----|
| 0x82632F00 | 144 | ctor `Instarank()` | sets vtable PTR_..820cf1a4, int=10@+0x10, String ctor@+0x1c/+0x28 |
| 0x82633000 | 104 | `Clear()` | bools@4/8/c/14/18, String-init@0x1c/0x28 (src l.16-24) |
| 0x82633070 | 112 | `Init(...)` | 8 args, members + 2 String-assign (src l.26-37) |
| 0x82633130 | 96 | `HasHighscore() const` | strncpy(mStr1@0x24), strtok '|', ret 'a'\|'b' (src l.49-56) |
| 0x82633190 | 72 | `~Instarank()` | base dtor 82563F20, vbase-adjust this-0x34 |
| 0x826336D0 | 180 | `UpdateRankLabel(UILabel*)` | mIsPercentile branch → SetTokenFmt(instarank_percentile@820cf490 / instarank_rank@820cf480) (src l.39-47) |
| 0x826337D0 | 728 | `UpdateString1Label(UILabel*)` | switch a/b/c/d/e → instarank_highscore_percentile/_rank/_previousbest_rank/_rival_percentile/_rival_rank (rdata 0x820cf508..0x820cf57c) (src l.58-102) |
| 0x82633B50 | 1060 | `UpdateString2Label(UILabel*)` | switch f/g/h/i/j → instarank_nofriend_beat/_friend_beat/_friend_beat_and_more/_rival_close (rdata 0x820cf640..0x820cf690) (src l.104-156) |
| ...funclets/Symbol-guard clears (DAT_82dd02e8/0300/0324)/SetTokenFmt COMDATs (8230A368=DataArrayPtr variadic)... | | | |

End: last Instarank funclet 0x826340CC (DAT_82dd0324&~1); 0x826340F0 starts the
**next** TU (`NetGameData`/`BandNetGameData`, RTTI `.?AVNetGameData@@`@820cf7cc /
`.?AVBandNetGameData@@`@820cf844). → Instarank `.text` end = **0x826340F0**.

Strings confirming Instarank (rdata, base 0x82000400): instarank_rank@0x820cf480,
instarank_percentile@0x820cf490, instarank_highscore_percentile@0x820cf508,
instarank_highscore_rank@0x820cf528, instarank_previousbest_rank@0x820cf544,
instarank_rival_percentile@0x820cf560, instarank_rival_rank@0x820cf57c,
instarank_friend_beat@0x820cf640, instarank_friend_beat_and_more@0x820cf658,
instarank_rival_close@0x820cf678, instarank_nofriend_beat@0x820cf690.
(Referenced via lazy-interned Symbol globals @0x82dd02e0+, guarded by DAT bitflags
— so direct string xrefs are 0; this is the standard `Symbol(literal)` static-init
idiom.)

### Instarank pdata
48 entries, VA [0x82224938, 0x82224AB0] (begin 0x82632F00 .. 0x826340CC).
dtk auto-derives this on resplit; pin only `.text` first.

### Port dependencies (all present in src tree)
- `ui/UILabel.h`: `SetTokenFmt(Symbol,T1..T4)` templated (→ DataArrayPtr), `SetTextToken(Symbol)` — matches disasm (8230A368 = DataArrayPtr variadic ctor).
- `game/Defines.h`: `enum ScoreType` (kScoreBand etc.).
- `meta_band/Utl.h`: `GetFontCharFromScoreType(ScoreType,int)` (free fn, in Utl.cpp — NOT in Instarank text).
- `utl/Std.h` / `utl/Locale.*`: `atoi_s`, `LocalizeSeparatedInt`, `Localize`, `MakeString`.
- `Hmx::Object` virtual base, `DECLARE_MESSAGE(InstarankDoneMsg,...)` (Instarank.h l.33 — generates a tiny static Type(); part of the TU).

## Manager TU `[0x82632C98, 0x82632F00)` (frontier)
6 fns: 82632C98 (String-dtor funclet), 82632CC0 (Load: ReadEndian + hash insert
loop via 82632B98), 82632D18 (288B — iterates DataArray, hash_map[Symbol]→vector<int>
insert via SongUpgradeMgr's 82632A28 operator[], find on +0x1c via 82632150,
bool@+0x38=1; an AddX/discover method), 82632E38/E60 (funclets), 82632E88 (112B —
82804DA8 lookup → 82632730 (find-erase) or 82632D18). Layout: singleton, hash_map@0x1c,
bool@0x38. A `ContentMgr::Callback` manager. Candidates to cross-check by build-order
proximity + container shape: other meta_band content managers near SongUpgradeMgr
alphabetically (Setlist/SongMgr-adjacent). NOT LicenseMgr (std::set<Symbol>), NOT
SongStatusMgr (worktree pins @0x82671A60/0x8264B5F8/etc., far away).

## Coordination notes
- LicenseMgr lead (sibling: "@0x82631FD0") is **inside SongUpgradeMgr's pin** —
  refuted as a LicenseMgr location. (SongUpgradeMgr commit already corrected a
  0x826311B8 mis-name from LicenseMgr::HasLicense → SongUpgradeMgr::HasUpgrade.)
  LicenseMgr (objects.json:696, NonMatching/UNPINNED) lives elsewhere; do not
  pin it here.
- Instarank pin must be self-contained vs **main@8314** AND not collide with the
  SongUpgradeMgr sibling branch (it doesn't — SongUpgradeMgr ends at 0x82632F00−).
  Whichever lands first (SongUpgradeMgr 2a962d4 or Instarank), the other rebases
  cleanly (disjoint .text/.pdata/objects.json line).
