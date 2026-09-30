# W16-GN — a screen for W16-GI's nothrow-stub mechanism, and what it found

Date 2026-09-30 · base main `8af79551` · worktree `~/tmp/wt-w16-gn` · branch `w16-gn`
Pre-GI validation tree: `~/tmp/wt-w16gn-pre`, branch `w16-gn-pre` at `85b84e32`.

(Results sections are filled in after measurement; the pre-registration below
was committed BEFORE the source edit it predicts.)

## Pre-registration P1 — port `RndPropAnim::ForeachKeyframe`

Written before editing `src/system/rndobj/PropAnim.cpp`.

State: `?ForeachKeyframe@RndPropAnim@@QAA?AVDataNode@@PBVDataArray@@@Z` is
`{ return DataNode(0); }` (16 B, a leaf) in our build. Retail's paired callee at
the call site in `?Handle@RndPropAnim@@UAA?AVDataNode@@PAVDataArray@@_N@Z` is
`fn_82429C38`, 0x8D0 = 2,256 B by `.pdata`, NON-leaf, in no `.text` split
(auto unit). Caller row: 3,136 B, fuzzy 98.8648; charges = a pervasive r10/r11
swap in the local-static guard checks + ONE delete at idx 480: retail reloads
the guard word right after `bl ForeachKeyframe` (idx 468), ours does not.

Port: rb3-Wii's control flow (retail has NO `RemoveKey` call, so DC3's newer
`sRemoveFrame` branch is excluded), spelled in our DC3-shaped accessor API.

Predictions:
- P1a (confident): the idx-480 delete disappears (a calling callee forces the
  guard reload).
- P1b (uncertain, ~50%): the r10/r11 swap also resolves and the row crosses:
  +1 fn / +3,136 B whole-binary. If it does not, fuzzy rises a little, 0 B.
- P1c: leg B recompiles exactly 2 TUs (PropAnim.obj, and MetaMusic.obj via its
  scatter-include of PropAnim.cpp); no other row in PropAnim or MetaMusic moves
  (only `Handle` calls `ForeachKeyframe`; no map entry is added, so the ported
  body is compiled but unscored).
- Falsifier F1: the idx-480 delete survives with a real body ⇒ the "our stub
  is proved memory-inert, so the reload is elided" reading is wrong.
- Not predicted to move: `scripts/symbol_aliases.json` lists
  `?ForeachKeyframe@RndPropAnim@@…` as a folded member of a `return DataNode(0)`
  group. That membership exists BECAUSE of our stub (retail's body is 2,256 B
  and cannot fold with a 16 B body). It becomes contradicted by this port; it is
  another lane's file and is NOT touched here. Expected metric effect: 0 (the
  only call site's target is a forgiven placeholder `fn_82429C38`).
