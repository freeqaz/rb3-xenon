# Port handoff — MemMgr.cpp (port-frontier 2026-07-02)

Worktree: `~/tmp/wt-pf-MemMgr`  ·  Branch: `bp-pf-MemMgr` (from `aa58cee`)

## Result: 3/3 targets landed at report-normalized 100.0% (verified_committed)

| target VA | symbol (MSVC mangled) | tier | final norm % | disposition |
|---|---|---|---|---|
| 0x827963d8 | `?FreeBlockStats@MemHeap@@QAAXAAH000@Z` | BSim 30.0 | **100.0** | pinned (INSIDE split) |
| 0x827966e8 | `?Lock@MemHandle@@QAAPAXXZ` | ExactInstr | **100.0** | pinned (micro-pin carve) |
| 0x82798278 | `?MemOrPoolFreeSTL@@YAXHPAX@Z` | BSim 15.4 | **100.0** | pinned (micro-pin carve) |
| 0x827977d0 | `_MemAlloc(int,int)` | BSim 18.1 | — | **SKIPPED** (see below) |

All three are HONEST (none appear in `icf_aliases.map`; each is a distinct real body).

## Key structural finding: TU-grouping vs the refactored xenon split

The retail X360 `MemMgr.o` TU groups the same functions the rb3-Wii source does:
`Heap::FreeBlockStats`, `MemHandle::Lock`, `MemOrPoolFreeSTL`, `_MemAlloc`. The
rb3-xenon port had refactored `MemHeap`/`FreeBlockStats` out into `MemHeap.cpp`,
so two of the targets physically live inside **MemHeap.cpp's** `.text` span
(0x82796440–0x827989F0) even though the oracle says TU=`MemMgr.o`.

Pairing rule confirmed empirically: a target VA→mangled entry pairs only if the
mangled symbol exists in the **base obj of the unit whose split range covers that
VA**. So:

- **FreeBlockStats (0x827963d8)** was already INSIDE MemMgr.cpp's split
  (0x827963D8–0x82796440). The target is the retail **4-arg**
  `Heap::FreeBlockStats(int&,int&,int&,int&)` (4 outputs, NO member writes) — a
  distinct overload from the xenon `MemHeap::FreeBlockStats(int&×5)` in
  MemHeap.cpp (which writes `mMinFreeBytes`). Added a **4-arg overload**
  declaration to `MemHeap.h` and **defined it in MemMgr.cpp** so it lands in
  MemMgr.obj. → 100%.
- **Lock (0x827966e8)** and **MemOrPoolFreeSTL (0x82798278)** are physically in
  MemHeap.cpp's range. dtk had MERGED Lock into `fn_82796688` (0x7C). Neither VA
  is a pdata anchor (the whole run 0x82796528→0x82796720 and the 0x82798250 area
  are pdata-less LEAF runs), so I could split/carve them without hitting the
  pdata wall:
  - `symbols.txt`: shrank `fn_82796688` 0x7C→0x60, added `fn_827966E8 size:0x1C`.
  - `splits.txt`: carved `0x827966E8–0x82796704` and `0x82798278–0x82798298` OUT
    of MemHeap.cpp (fragmenting its `.text` into 3 ranges) and pinned them under
    **MemMgr.cpp** (TU-faithful). Every fragment boundary is a real function
    start, so no whole function was split mid-body.
  - `MemHandle::Lock` was **undefined** in xenon (only declared in MemMgr.h) — I
    added the trivial body (`++mAlloc->mLockCount; return (char*)mAlloc+0x10`).
  - `MemOrPoolFreeSTL` already existed (MemMgr.cpp:276, the retail 2-arg form).

## The reloc-naming residue (MemOrPoolFreeSTL 98.75 → 100)

MemOrPoolFreeSTL's only diff at 98.75% was its two tail-call reloc targets being
unnamed in the target obj. Naming the callees closed it to 100%:
- `0x82797aa0` → `?MemFree@@YAXPAX@Z`
- `0x82795da0` → `?PoolFree@@YAXHPAX@Z`
(both bodies confirmed via disasm; both mangled names present in our objs).

## Skipped: _MemAlloc (0x827977d0)

`fn_827977D0` is a **472-byte (0x1D8), pdata/xdata-anchored** heavy heap
allocator (`except_record_827977D0`, thread-mem-stack + strategy-dispatch +
heap-fit loop). The xenon MemMgr.cpp `MemAlloc(int,int)` is an explicit
`return malloc(size)` stub pending `MemHeap::Alloc`. Reproducing this byte-exact
needs the full heap-allocator decomp — far beyond a bsim-18 confirm target. Not a
port-then-pin candidate this wave.

## Regression

Both carves only REMOVED the two MemMgr-TU functions (Lock, MemOrPoolFreeSTL)
from MemHeap.obj's target — they were never matched in the MemHeap unit (wrong
TU). The intact MemHeap ranges emit byte-identical target objs. Spot-checked via
objdiff-cli: FirstFit/BestFit/LRUFit/LastFit all still 100.0%; the only sub-100
MemHeap fns observed (AttemptMerge 98.28, Print 82.67) are pre-existing partials
in the intact-middle range, not at any carve boundary. Whole-binary report
confirm was regenerating at handoff time. Expected net: **+3 matched, 0 lost**.

## Files changed (all committed to bp-pf-MemMgr)
- `src/system/utl/MemMgr.cpp` — 4-arg `MemHeap::FreeBlockStats` def + `MemHandle::Lock` def
- `src/system/utl/MemHeap.h` — 4-arg `FreeBlockStats` overload decl
- `config/45410914/symbols.txt` — split fn_82796688 + add fn_827966E8
- `config/45410914/splits.txt` — carve Lock + MemOrPoolFreeSTL into MemMgr.cpp; fragment MemHeap.cpp
- `scripts/target_symbol_map.json` — 5 entries (3 targets + 2 callee names)

Commits: `6b198a9` (FreeBlockStats), `074c889` (Lock), `0cac640` (MemOrPoolFreeSTL + callees).
