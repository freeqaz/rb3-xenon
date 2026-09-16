# W16-EF — the `ButtonDownMsg` name is RIGHT, and three `Game` rows closed

Lane W16-EF, 2026-09-16. Worktree `~/tmp/wt-w16-ef`, branch `w16-ef`, based on `ace069a9`.
Predecessor: `docs/decomp/W16CB_GAME_HANDLE_ADJUDICATION_ANONROWS_LOADSONG_2026-09-15.md`.

## 0. Result, measured

`tools/ab_measure.py --worktree ~/tmp/wt-w16-ef --from-dirty`, run dir
`.ab_measure_runs/20260916-024145-from-dirty-3850571`. Both legs settled to zero work,
both at a `symbols.txt` split fixed point (sha chain `1e8375f9 -> 1e8375f9`, 0 extra
forced re-splits on either leg), `objdiff-cli` sha stable (`c1b7d95240a35cd6`), ruler
`functionRelocDiffs=name_check` read from `objdiff.json` options, tree restored.

| | leg A | leg B | Δ |
|---|---:|---:|---:|
| `matched_functions` | 43,950 | 43,953 | **+3** |
| `matched_code` | 4,122,600 | 4,123,008 | **+408 B** |
| `matched_code_percent` | 40.232000 | 40.235977 | +0.003977 pp |
| `fuzzy_match_percent` | 49.998850 | 50.001920 | +0.003070 pp |
| `masked_equal` | 23,224 | 23,224 | +0 |
| honest (`matched − masked_equal`) | 20,726 | 20,729 | +3 |

`total_code` 10,247,068 / `total_functions` 69,240, unchanged on both legs.
Whole-binary Δmatched (+3) equals the all-units net (+3): **zero collateral anywhere
else in the binary.**

`default/band3/game/Game`: matched 327 → **330** rows, matched_code 31,924 → **32,332**
(of 40,844), unit fuzzy 89.595634 → **90.365295**. Named sub-100 rows **15 / 7,180 B →
13 / 6,772 B**.

Rows that moved (extracted from the A/B's own archived `legA/legB_report.json.gz`, not
from an in-tree read):

| size | fuzzy A → B | mpn A → B | symbol |
|---:|---|---|---|
| 232 | 53.4655 → **100.0000** | 55.1034 → 100.0 | `?OnMsg@Game@@QAA?AVDataNode@@ABVRemoteLeaderLeftMsg@@@Z` |
| 176 | 49.1364 → **100.0000** | 50.8409 → 100.0 | `?SetRealtime@Game@@QAAX_N@Z` |
| 156 | 19.8205 → 98.5897 | 20.8462 → **100.0000** | `?OnMsg@Game@@QAA?AVDataNode@@ABVButtonUpMsg@@@Z` |
| 792 | 99.2828 → 99.3081 | 99.4596 → 99.4848 | `??1Game@@UAA@XZ` (alias; +0 B by design) |

The two rows reaching `fuzzy == 100` are the whole +408 B (232 + 176). `ButtonUpMsg`
pays **+1 function and +0 bytes** — it reaches `mpn` 100 while `fuzzy` stays at 98.5897,
which is the ordinary arg-only stratum (`mpn` excludes arg-only penalties). That is the
expected shape, not a shortfall.

## 1. ★ The accuracy question — CB's identification is RIGHT, and its falsifier is unsound

CB named `0x8267b808` as `?OnMsg@Game@@QAA?AVDataNode@@ABVButtonDownMsg@@@Z` and wrote
that *"a wrong identification lands near 0"*. The row sits at **3.24%**. The brief asked
whether CB's own falsifier is firing on CB's own result. It is not — but the falsifier
itself is the thing that needs correcting.

**Four independent lines of retail-byte evidence, all read off `orig/45410914/band.exe`
via `tools/retail_body.py`, before any source was written:**

1. **`0x8267b808` and `0x82679900` have instruction-for-instruction identical
   prologues.** Both: `lbz` of the gate byte at `0x2f(r4)` → accessor on the message →
   virtual call through slot 0 → pad range check `[0,4)` → `JoypadGetPadData` →
   `mType == 2` (`kJoypadAnalog`) → per-pad counter at `[0x48(this)] + pad*4`. They
   diverge in exactly one instruction: `addi r9, r9, 1` at `0x8267B888` (down) versus
   `addi r9, r9, -1` at `0x8267997C` (up). That is the canonical press/release counter
   pair, and it is not a shape two unrelated functions fall into.
2. **Three-argument shape** (return buffer / `this` / message), matching
   `DataNode Game::OnMsg(const ButtonDownMsg &)`.
3. **Caller set.** Each of the three handlers has exactly **one** retail caller, all
   inside `?Handle@Game@@` at ascending offsets `+0x138c` / `+0x1404` / `+0x1480` —
   matching our `HANDLE_MESSAGE` declaration order exactly.
4. **Body content** is button-driven: `audition_cam_toggle`, `deploy_if_possible`,
   `audition_keyboard_synth_volumes`, and `DirectInstrument::Enable/Disable/SetVolume`.

**⛔ The epistemic correction, which is the durable part of this section:**

> **"A wrong identification lands near 0" is NECESSARY BUT NOT SUFFICIENT.
> A *correct* identification of a function whose body we have only STUBBED also lands
> near 0.** objdiff pairs by name and scores our body against retail's; a 3-line stub
> against a 1,664 B retail body scores ~3% whether or not the name is right.

So a near-zero score is evidence about **body coverage**, not about **name correctness**,
and it cannot discriminate between the two hypotheses at all. The 3.24% here is fully
explained by our stub. Using a low score as a naming falsifier would have retracted a
correct identification — the exact opposite of the accuracy-positive move. This is the
same family as the house rule that an objdiff `AT_LIMIT` on a reloc-name-only row is the
detector restating its own input: **a metric that is downstream of the thing you are
testing cannot adjudicate it.** Only retail bytes can.

**Verdict: the name stays. The 1,664 B is genuine source divergence** — an unported
body, not a misidentification. It is the single largest remaining row in the unit and is
the natural target for the next lane (see §7).

## 2. The three source levers (all in `src/band3/game/Game.cpp`)

Each was chosen from a **named mechanism read off retail bytes**, never from a guess at
what might move the number.

**(a) `OnMsg(RemoteLeaderLeftMsg)` — 232 B, 53.4655 → 100.** Retail builds
`game_outro_msg` as a **function-local static**, not as the file-scope
`extern Message game_outro_msg` our source referenced (`utl/Messages.h:73`): guard word
`0x82E021C8` bit 0, `Message` storage `0x82E021C0`, the `Symbol` built as a **stack
temporary** at `r31+0x50` from `"game_outro"` (`0x8202F40C`), then
`??0Message@@QAA@VSymbol@@@Z` and an `atexit` thunk. One guard bit covers both, which is
the single-static form `src/band3/bandtrack/TrackPanel.cpp:638-656` already documents —
a two-static form would consume guard bits `0x1` and `0x2`.

**(b) `SetRealtime` — 176 B, 49.1364 → 100.** Two separate findings:
- Retail **opens the function with an unused function-local static `Symbol`**
  (`"drum_trainer"`, guard `0x82E021B4` bit 0, storage `0x82E021B0`, string at
  `0x820DCB94`, all at `0x82678BB8..0x82678BFC`). The guard test **precedes** the
  `mProperties.mInDrumTrainer` load (`lbz 0x2d`), so it is the first statement. Same
  residue already recorded in `GetSongToTaskMgrMs` and `Poll`.
- Retail **materialises a pointer to the container** (`addi r29, r28, 0x6c`) and reads
  `_M_finish` as `4(r29)` every iteration, where `FOREACH` over the member directly
  re-reads `0x70(this)`. Binding `std::vector<Player *> &players = mAllActivePlayers;`
  first reproduces retail's addressing shape.

**(c) `OnMsg(ButtonUpMsg)` — 156 B, body ported from `0x82679900`.** Reached `mpn` 100
(+1 function, +0 bytes). `mUnkTU5GuidePitch` is used purely as the `int[4]` at its `+0x0`
— exactly as the class comment in `Game.h:50-56` already records ("an int array at +0x0
indexed 0..3, per-track counters, decremented at `0x82679960`"). It is only
forward-declared, so the access is spelled as a cast rather than by inventing a
definition we cannot justify.

## 3. CB handoff #1 — the `~Shuttle` fold membership, INSTALLED via the gate

`??1Shuttle@@QAA@XZ` added to group `0x826c3888` (survivor: the `StlNodeAlloc`
`_List_node<int>` copy-ctor), 86 → **87** members; 1,657 groups total, unchanged.
Installed **only** through `tools/fold_thunk_gate.py` with a synthesized one-pair
worklist (`~/tmp/w16ef_shuttle_worklist.json` — the house worklist contains zero Shuttle
pairs). The group was never hand-edited. Backup of the pre-state:
`~/tmp/w16ef_aliases_before.json`.

**Be precise about what the gate actually certified.** It ADMITted at **`FT-EMPTY`**,
with the honest self-declaration *"body carries no relocation: the fold is real but the
byte comparison is vacuous"*. The 8-byte body has no relocations, so there is nothing for
the byte test to compare — the ADMIT is therefore **not** independent proof.

> The install rests on **CB's separate call-site evidence** (a one-argument
> destructor-then-delete sequence, which a two-argument copy-ctor cannot be), not on the
> gate's byte test. Recorded plainly here so no later lane cites `FT-EMPTY` as proof of
> folding. This is the `129,360 irreducible pair-bytes` class in `CLAUDE.md`: the fold is
> real, but which name the call site meant was destroyed by ICF itself.

Effect: `??1Game@@` 99.2828 → 99.3081 fuzzy (mpn 99.4596 → 99.4848), **+0 bytes**, as CB
predicted. It removed that row's one relocation-name charge and did not cross it.

## 4. CB handoff #3 — `OnSetShuttle` ADJUDICATED: a genuine ICF fold; our source is RIGHT

CB flagged `?OnSetShuttle@Game@@` as carrying TGT `?Enable@Metronome@@QAAX_N@Z` vs BASE
`?SetActive@Shuttle@@QAAX_N@Z`, unadjudicated. Decided on retail bytes, as instructed:

- `0x826f07b8` is **`stb r4, 8(r3); blr`** — 8 bytes, **zero relocations**, i.e.
  maximally foldable. Any `void f(bool)` storing to `+0x8` folds here.
- `Shuttle::mActive` is at offset `0x8`.
- The call site passes `this + 0xe0`, and that object's member offsets (`mMs` @ 0,
  `mEndMs` @ 4, `mPadNum` @ 0xc) provably identify a `Shuttle`.
- The map holds **no `?SetActive@Shuttle@@` address at all**, so the survivor resolved to
  the arbitrary `Metronome::Enable` spelling.

**⇒ Our source is correct; the charge is ICF survivor-naming, not a wrong callee.** The
row has exactly one charge and is capped at **99.92308**.

**I did NOT install the alias that would close it.** The lane authorization covered
exactly one alias — CB handoff #1 — and installing a second on my own judgment is the
integrity hazard `CLAUDE.md` names explicitly (an unproven alias lifts `name_check` **by
construction**, and the `none` control cannot catch a fabricated one; flatness there is
the *signature*, not a clearance). **260 B priced, deliberately unrealised in this lane.**
It is a clean, evidenced candidate for whoever owns the next alias wave.

## 5. Pre-registration scorecard

| # | prediction, written before measuring | outcome |
|---|---|---|
| 1 | All three source levers land; `RemoteLeaderLeftMsg` pays **exactly +232 B**; zero collateral elsewhere | **HELD** (+232 exact; all-units net == whole-binary Δ) |
| 2 | `SetRealtime` will **not** cross — 13 register charges look like regalloc | **MISSED** — it reached fuzzy 100 / mpn 100 |
| 3 | `??1Game@@` stays sub-100 after the alias, +0 B / +0 fns | **HELD** (99.2828 → 99.3081, +0 B) |
| 4 | Archived A/B legs reproduce my in-tree per-row table exactly | **HELD** (all four rows, and the unit totals) |

**#2 is the most informative line in this lane and is worth reading twice.** I predicted
`SetRealtime` would not cross because its charge presented as 13 register differences.
**All 13 dissolved the moment the single structural `addi r29, r28, 0x6c` was fixed** —
the register pressure was downstream of the addressing shape. This is `CLAUDE.md`'s
*"a `REGISTER_SWAP` label is a symptom, not a diagnosis"* rule beating my own
pre-registration, and it is now a 13th recorded instance. **Never defer a row as
regalloc-bound on that label alone.**

A minor bookkeeping correction against my own narration: mid-lane I stated "12 named
sub-100 rows remaining". The archived leg B says **13 / 6,772 B**. The byte figure was
right; the row count was off by one.

## 6. What I did NOT do, and why

- **Did not touch `?Reset@Game@@`** (396 B, 99.8788). Brief's "do not fund" — CB measured
  a plain r4/r6 register swap and the permuter is OFF by standing user directive.
- **Did not touch `fn_8235F858`** (308 B). A mis-homed **Tour** body; re-homing an
  already-pinned address is **not** metric-neutral, and it belongs to whoever owns Tour.
- **No `splits.txt` edits of any kind.** `Game` is one of the three units whose heading
  `b341d7ab` repaired; I did not go near it.
- **Did not grind the 17 anonymous `fn_*` rows / 1,740 B.** Placeholder targets are
  already forgiven by `name_check` — zero byte upside.
- **Did not install a second alias** (§4), and did not prune any group.
- **Did not attempt `??0Game@@` (1,504 B), `UpdatePausedState` (972 B), `AddPlayer`
  (396 B), `ResetVoiceChatState` (296 B), or `GemTrainerLoopPanel` (140 B).** Not
  adjudicated, not attempted, not priced — pure budget. Silence would read as coverage,
  so: these are untouched and their scores are exactly as leg B reports them.
- **Did not pursue CB handoff #2 (the `??1Game@@` EH-state lever) or #5 (where retail
  sets practice mode).** Both remain open exactly as CB left them.
- **Never ran `ninja <one>.obj` or `objdiff-cli --build`** — both skip the six obj
  patchers, which are part of the ruler. Every score here is from a full
  `./tools/ninja-locked` build plus `report.json`, or from `ab_measure`.

## 7. For the next lane

1. **`?OnMsg@Game@@...ButtonDownMsg@@@Z`, 1,664 B at 3.24% — the single largest row in
   the unit, and the name is now ADJUDICATED CORRECT (§1).** It is an unported body, not
   a naming problem. The paired `ButtonUpMsg` body landed in this lane and its prologue
   is instruction-for-instruction identical to `ButtonDownMsg`'s, so the first ~20
   instructions are already written and proven. The divergence is the button-driven tail:
   `audition_cam_toggle`, `deploy_if_possible`, `audition_keyboard_synth_volumes`, and
   `DirectInstrument::Enable/Disable/SetVolume`. **Highest-value target in the unit.**
2. **`OnSetShuttle` (260 B) is fully adjudicated and needs one alias**, evidence in §4.
   Route it through whoever holds alias authorization.
3. CB handoffs **#2** (EH-state lever on `??1Game@@`; `Tail.h:53` carries the same
   `ATanInterpolator`, so a lever pays twice) and **#5** (practice-mode home) are
   untouched and still open.
4. `??0Game@@` (1,504 B, 80.80) and `UpdatePausedState` (972 B, 66.30) are the next two
   by size and were not looked at by this lane at all.

## 8. Provenance

- All retail bytes read from `orig/45410914/band.exe` via `tools/retail_body.py <va>
  <size> --dis`. (⚠ `tools/va_disasm_tu5.py` crashes — it looks for
  `orig/45410914/band_tu5.exe`, which does not exist. Use `retail_body.py`.)
- Scores from `report.json` (`name_check`), every numeric `int()`/`float()`-coerced,
  absent keys treated as 0. ⚠ `provenance.diff_config` is a **list**, not a dict.
- Per-row table in §0 extracted from the A/B's own archived leg reports, so it is
  settled-leg data rather than an in-tree read.
- Alias install: `tools/fold_thunk_gate.py` only; never a hand edit.

**⚠ MERGE-ORDER WARNING: this lane modifies `scripts/symbol_aliases.json`** (one member
added to group `0x826c3888`). Lane **W16-EE** was briefed as possibly touching the same
file. Checked at 2026-09-16 02:45: `~/tmp/wt-w16-ee` is clean with no commits ahead of
`main`, so there is no conflict **as of that moment** — but W16-EE is a live lane and
that can change. **Land with the alias file's state re-checked, not assumed.**
