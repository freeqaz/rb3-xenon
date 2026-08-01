# MESSAGE_TIMER: the lever was already pulled in June — the residual was six wrong per-TU restores

**Lane BU-3, 2026-07-30.** Base: main `cd7dc281`. Worktree `~/tmp/laneBU3/wt`,
branch `laneBU3`. Two jobs, measured separately, committed separately.

| job | change | honest Δ | code% Δ | regressions |
|---|---|---|---|---|
| **A — MESSAGE_TIMER** | `config/45410914/objects.json`, 6 TUs | **−5** | **+0.00238pp** | 7 anon funclets −0.1..−3.8 |
| **B — StaticClassName 0.0% pool** | `splits.txt` + `target_symbol_map.json` | **+6** | **+0.00393pp** | **0** |
| **A + B combined** (verified build) | | **+1** | **+0.00631pp** | |

The combined leg was built and measured, not inferred: `matched 40938 /
masked_equal 1518 / honest 39420 / code% 34.503548`. It is **exactly additive**
(−5 and +6), so the two changes do not interact and either can be landed alone.

Baseline measured in this worktree, same split both legs (`symbols.txt`
restored, `report.cache` removed, `config.yml` touched on every leg):
`matched 40936 / masked_equal 1517 / honest 39419 / code% 34.49724`.
⚠ Main's quoted headline was stale by ~48 — always measure your own A-leg.

---

## Job A — the handoff's framing was wrong, and that is the main finding

BS-4 (`9599caef`) flagged `MESSAGE_TIMER` in `BEGIN_HANDLERS` as a still-open
sibling lever with the same `MILO_DEBUG` root cause as `START_AUTO_TIMER`.

**It is not open. It was closed on 2026-06-20 in wave W9 (`3b86e9a`, +217).**
`src/system/obj/Object.h:990` already gates the per-handler `MessageTimer`
behind a dedicated `MILO_MESSAGE_TIMERS` macro that is **undefined by default**,
i.e. the retail shape. Anyone reading BS-4's note and grepping for a global
`MILO_DEBUG` flip will find the work already done and conclude the vein is dry.

The residual lever is much narrower and was invisible from the handoff: W9 then
**restored** the timer per-TU via `/DMILO_MESSAGE_TIMERS` in `objects.json` on
**six** units — `char/CharBoneDir.cpp`, `ui/UI.cpp`, `world/Dir.cpp`,
`obj/Dir.cpp`, `rndobj/Group.cpp`, `char/CharLipSync.cpp`. That restore set was
derived from match-improvement, i.e. metric-fitted. The real question is
therefore *"does retail construct a MessageTimer in those six TUs?"*

### Answer: no. Four independent lines, all from the binary.

**(1) String absence.** All four distinctive subsystem literals are absent from
retail `.rdata`: `message_timer_start`, `message_timer_dump`, `message_timer_on`,
`"Message Tracker Dump"` (the `DataRegisterFunc` names in `MessageTimer::Init`
and the `MILO_LOG` in `Dump`). *Positive controls fire* — `CharBoneDir` 1,
`CharLipSync` 2, `UIProxy` 1 — so this is real absence, not a broken grep.
Section geometry re-derived from `band.exe`'s own header, confirming the memory
note: `.text` VA `0x82270000` / raw `0x264E00` ⇒ delta `0x8200B200`; `.rdata`
delta is `0x82000000`. The two are different and mixing them is a classic trap.

**(2) No MessageTimer member is located anywhere.** `MessageTimer.cpp` *is*
pinned, but every symbol the map places inside its `0x822722B0` cluster is an
STL float-sort template or `DataArray::FindSym` — not one `MessageTimer::*`.
The single map entry mentioning the class, `?Active@MessageTimer@@` at
`0x8270fec8`, is a **phantom**: the body is `lis r11,0x8210; addi r3,r11,-0x6c28;
blr` — it returns an `.rdata` pointer, not a `bool` — and it has **zero** `bl`
callers. Retail inlined the real `Active()`, as `/O1 /Ob2` would.

**(3) Call census — the decisive test.** Decoded every `bl` in retail `.text`
and resolved targets. A MessageTimer expansion needs `Timer::Restart` in the
inline ctor and `Timer::SplitMs` + `MessageTimer::AddTime` in the inline dtor.
Both Timer functions exist out-of-line in retail and our own build calls them
out-of-line, so retail would too:

| target | callers | any `Handle`? |
|---|---:|---|
| `?Restart@Timer@@` `0x82511728` | 61 | **none** |
| `?SplitMs@Timer@@` `0x82270188` | 40 | **none** |
| `??0Timer@@` `0x82511548` | 59 | one — `?Handle@LockStepMgr@@` |

Every caller is a `Poll`/`Draw`/`Load`/ctor doing hand-written timing. The one
`Handle` that calls `Timer::Timer()` calls **neither** `Restart` nor `SplitMs`,
so it is a plain local `Timer` in a handler body — not a MessageTimer.
`MessageTimer::AddTime` is not located at all.

**(4) Instruction-level proof in the one TU that asserted otherwise.** objdiff
on `?Handle@CharBoneDir@@`: our timer-restored base emits
`lbz ?sActive@MessageTimer@@`, `bl ??0Timer@@`, `bl ?Restart@Timer@@`, the
`mObject`/`mMessage` stores at `0x90`/`0x94`, `bl ?SplitMs@Timer@@`,
`bl ?AddTime@MessageTimer@@` — **21 instructions, every one an `insert`**, i.e.
retail has no counterpart. Frame Δ **+0x40**; target 272 bytes vs our 396.

⇒ Retail contains **zero** MessageTimer expansions. All six restores are wrong.
Removing them is undoing metric-fitting *against* proven-correct source — the
inverse of gaming.

### Blast radius, PAIRED and post-filter

Site count is not defect count. 6 TUs → 7 handler blocks → **3 paired bodies**
(the rest are vtordisp thunks already at 100%):

| function | before → after |
|---|---|
| `?Handle@CharBoneDir@@` | 52.65 → **100.00** |
| `?Handle@CharBoneTwist@@` | 49.78 → **100.00** |
| `?Handle@UIManager@@` | 33.31 → 62.48 |

**5 unpaired and cannot move the metric**: `WorldDir::Handle`,
`ObjectDir::Handle`, `RndGroup::Handle`, `CharLipSync::Handle`,
`Automator::Handle`. They carry the same defect and get the fix free once named
— so this pre-pays, exactly as BS-4's did.

Two functions reaching **exact 100%** is the strongest possible confirmation:
retail's bodies are byte-identical to ours with the timer gone.

### Measurement — bank the number, not the prediction

Predicted +2..+4. **Got honest −5, code% +0.00238pp.** Full churn:

```
crossed INTO 100%: 3   CharBoneDir::Handle, CharBoneTwist::Handle, fn_823EB278
FELL OUT of 100%:  7   fn_823B73BC 99.9   fn_82803B8C 96.19  fn_82806354 99.9
                       fn_8280637C 99.9   fn_828063A4 99.9   fn_828063CC 99.9
                       fn_82452910 99.81
```

Every loss is an **anonymous funclet slipping 0.1–3.8pp** — EH-funclet
re-pairing churn from the deleted `~MessageTimer` funclets changing which
funclets objdiff's fuzzy byte-fallback pairs. Not one real body regressed.
Same signature as BS-4 (which landed at −2 honest / +0.0039pp), slightly worse
on the count axis.

**Recommendation: land for correctness, on the code axis, not for points.** It
is committed separately from Job B precisely so it can be dropped independently
if the coordinator judges −5 too rich.

### What I did NOT do

- **Did not narrow the gate to only the paying TUs.** The evidence says retail
  expanded it nowhere; keeping a restore to dodge funclet artifacts would be
  gaming against known-correct source. Same call BS-4 made.
- **Did not remove the `MILO_MESSAGE_TIMERS` branch from `Object.h`** — it stays
  for the native build and costs nothing while undefined.
- **Did not chase `UIManager::Handle`** from 62.48 to 100 — that is body work.
- **Did not investigate the 7 funclet drops individually.**

---

## Job B — see commit `4fcc282e`

Two moves from BS-2's live 0.0% pool, **+6 honest / +0.00393pp / 0 regressions**:
the Rnd.cpp StaticClassName run tail (+2) and the Ham.obj `Song`/`HamSong` pair
(+4, predicted +2). Full evidence, the declined DxLight/RndLight 2-cycle, and
the re-verified exclusion list are in that commit message.

Method note that generalizes: **a homogeneous 0x80-stride StaticClassName run is
a single obj's contribution** — if a split boundary falls mid-run and the bodies
on one side read 100% while the other side reads 0.0%, the boundary is wrong.
But the inverse does *not* hold: in the `0x8227a6xx` run each 0x80 body belongs
to a *different* unit, so contiguity alone proves nothing there. Discriminate
with the supplier check (`command grep -a` or Python over the .obj for the COMDAT — a `.obj` is BINARY, and plain `grep` in an agent shell is shimmed to `ugrep -I`, which silently matches nothing in binary files; see CLAUDE.md "grep is BINARY-BLIND"), not adjacency.

---

## For the next lane

- ⛔ **Do not re-hunt "is MESSAGE_TIMER in retail".** It is not, and the global
  gate has been in place since June. Settled by string absence + a whole-`.text`
  `bl` census on `Timer::Restart`/`SplitMs`/`??0Timer` + instruction-level
  objdiff. Re-deriving costs a lane and yields the same answer.
- ★ **Treat metric-fitted per-TU restore sets as suspect.** W9's six
  `/DMILO_MESSAGE_TIMERS` units were chosen by match improvement, and all six
  were wrong. Any `extra_cflags` entry that encodes a *behavioural* claim about
  retail deserves a binary check.
- ★ **Supplier check before any attribution carve.** `grep -a` the compiled
  `.obj` for the mangled COMDAT. `MeshDeform.obj` supplies 4 StaticClassName
  COMDATs; `Rnd.obj` supplies 52. That single fact decided both Job B moves and
  killed the DxLight link.
- ⚠ `?Active@MessageTimer@@` `0x8270fec8` and `?ClassName@RndLight@@`
  `0x8273ff80` are both demonstrable map phantoms/mispairs found in passing.
