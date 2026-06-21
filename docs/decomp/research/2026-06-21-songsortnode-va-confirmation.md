# SongSortNode STEP-1 VA-confirmation sweep (HARD-FRONTIER #2, work item 1)

Recon-gate for the ICF-scattered-TU reconstruction campaign. The gate that
wave-16 SKIPPED: independently confirm each oracle VA before carving.
**No builds.** Ghidra was contended (single-process, shared with other waves),
so confirmation was done with a Ghidra-independent disassembly path
(`tools/va_disasm.py` + `tools/va_size.py`, capstone over `orig/45410914/band.exe`,
validated byte-exact against dtk split asm at 0x82389698).

Machine-readable table: `2026-06-21-songsortnode-va-confirmation.json`.

## Gate verdict (decisive)

**0 of 53** SongSortNode.cpp oracle methods reach the spec's CONFIRMED bar
(BinDiff sim >= 0.5 AND body-shape matches name). **Every** oracle entry for this
TU has sim < 0.5. The oracle's `size` field is the BinDiff *matched-region* size,
not the function size — it understates true sizes 3-30x (e.g. SortNode::Handle
oracle 140B vs **true 908B**; GetTotalMs oracle 0B vs **true 4676B**).

| class | n | meaning |
|---|---|---|
| CONFIRMED | **0** | none — no VA passes sim>=0.5 + shape |
| RECON (65-400B, sim 0.10-0.5) | 15 | body-port candidates, gated on STEP-2 build |
| WALL (>400B, or sim<0.10) | 13 | Handle/Renumber/Dispatch — codegen-heavy, defer |
| UNPLACEABLE (<=44B) | 23 | guard-thunks / ICF-folded leaves — defer to case-B |
| MISATTRIBUTED (reject VA) | 2 | hand-verified wrong VA/overload |

## The two findings that matter

1. **VAs are NOT mis-located — bodies just DIVERGE.** Disassembling each VA shows
   the retail body is the *right kind* for its name (the 6 `::Handle` methods all
   start with `lha r11,8(r5)` = read the message token from the DataArray arg then
   symbol-dispatch; the getters load the named member). So this is **not** a
   VA-finding problem the way wave-16 assumed — it is a **body-reconstruction**
   problem. The low sims reflect retail-vs-Wii codegen distance, not bad attribution.

2. **Two genuine MISATTRIBUTIONS caught (the wave-16 trap, confirmed real):**
   - `0x824eeaa0` "OwnedSongSortNode::GetTotalMs()" — a **4676-byte** function with
     a -0x4c0 frame and many callee-saves. That is not an accessor. sim 0.000.
     **REJECT** (the VA hosts a large unrelated function).
   - `0x8264f2a0` "ShortcutNode::ShortcutNode(SongSortCmp*, Symbol, bool)" — disasm
     is a **memberwise COPY constructor** (`mr r30,r4` then field-copy 0xc..0x38
     from one source object), NOT the 3-arg value-ctor. sim 0.424 is the
     ICF/structural-family resemblance. **REJECT** as the named overload.

## Why the 23 UNPLACEABLE are a wall, not low-hanging fruit

The <=44B "accessors" (GetType/GetToken/Compare etc., true size 32B, sim ~0.41)
disassemble to MILO **Symbol / singleton static-init GUARD-THUNK** sequences
(`lis r11,-0x7d23; lwz r11,off(r11); rlwinm <bit>; ...`), not method bodies. These
ICF-fold binary-wide — the ~0.41 sim is guard-template resemblance, identical
across hundreds of TUs. Span-pinning them mints fake matches (the documented
wave-14 +57 refutation). They are exactly the case-B byte-transport class: only an
oracle-name-keyed byte-equality pass can claim them, and only if byte-identical.

## RECON candidates (15) — the only port targets, all gated on STEP-2

```
0x8226fcf8  80B sim .413  FunctionSortNode::GetToken() const
0x822ab970  96B sim .436  SetlistSortNode::GetTotalMs()
0x82394d78  64B sim .416  FunctionSortNode::GetTier() const
0x823c2600 336B sim .152  SortNode::Renumber(vector<SortNode*>&)
0x823d0c08  72B sim .428  OwnedSongSortNode::GetTier(Symbol) const
0x823d6c48  64B sim .416  SortNode::SetShortcut(ShortcutNode*)
0x823d8d08  64B sim .416  FunctionSortNode::GetAlbumArtPath()
0x82450208  88B sim .418  ShortcutNode::GetDateTime() const
0x82510308 112B sim .432  OwnedSongSortNode::GetIsCover() const
0x82564358  96B sim .304  ShortcutNode::FinishSort(NodeSort*)
0x82591860 364B sim .267  SubheaderSortNode::Handle(DataArray*, bool)
0x827575a8 120B sim .423  OwnedSongSortNode::GetTitle() const
0x8282c6e0  88B sim .383  OwnedSongSortNode::GetAlbumArtPath()
0x829ba998 176B sim .422  SubheaderSortNode::SubheaderSortNode(SongSortCmp*, Symbol, bool)
0x82aeef10  48B sim .423  OwnedSongSortNode::GetAlbum() const
```

The rb3-Wii oracle bodies for these are simple
(`/home/free/code/milohax/rb3/src/band3/meta_band/SongSortNode.cpp`, note path is
`meta_band/` not the bindiff_src's `src/meta_band/`), e.g.
`GetTitle() const { return mSongRecord->mData->Title(); }`. So the *logic* is known.
The open question (NEXT STEP, needs a build) is whether the retail body's leading
**guard prefixes** (Symbol static-init, `TheSongMgr` singleton-guard) port cleanly
under MSVC, or reproduce the BandProfile body-divergence wall. Even the RECON-band
getters at sim ~0.42 begin with the same guard-thunk preamble as the UNPLACEABLE
group, so they may collapse into the same wall once carved.

## Honest implication for the #2 campaign

This is a **negative-leaning recon result, with evidence**: SongSortNode is a
**worse** reconstruction target than the spec hoped. Best case is the 15 RECON
candidates IF their guard-prefixed bodies port clean (unproven), but 23/53 are
guard/ICF stubs only reachable via case-B byte-transport, 13/53 are >400B/codegen
walls, and 2 oracle VAs are outright wrong. There is no "~10-16 easy SongSortNode
matches" sitting behind this gate; STEP-2 must prove tractability on ONE RECON
getter (suggest 0x827575a8 GetTitle or 0x82510308 GetIsCover — smallest clean
oracle bodies) before committing the full TU port.
