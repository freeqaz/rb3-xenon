# Next Levers — post-strict-oracle (2026-05-29)

> **VERIFIED CORRECTION (orchestrator, 2026-05-29, post-wave-3).** Lever #1
> ("pin the 38 wired meta_band TUs, +20-60") is **over-optimistic — verified
> against the oracle.** Span-derivation fails for game code:
> - **30/38** unpinned meta_band TUs have **zero** bindiff/autoid coverage → no
>   way to derive a `.text` span at all.
> - **8/38** have bindiff coverage, but the dominant clusters are **1-8% density**
>   (AccomplishmentManager: 13 fns scattered across 16 KB; AccomplishmentProgress:
>   5 fns in 13 KB @1%). This is the **DC3-name-aliasing** problem the doc itself
>   describes (Q2): bindiff matched RB3 fns to *structurally-similar DC3* fns, so
>   the "AccomplishmentManager::*" addresses are physically scattered across many
>   *real* RB3 TUs. Pinning them = loose hulls (collide, like PlatformMgr_Xbox).
> - Only ~5 single-fn (DC3-aliased, low-confidence — likely land 0) + ContextChecker
>   (8%) are even semi-pinnable. **Realistic meta_band pin yield: +2 to +8, not
>   +20-60.**
> - The Q1 engine scatter was **already consumed by wave-3** (ContentMgr +
>   FlowOutPort pinned; the rest are ICF start-covered = un-pinnable, or loose
>   hulls). **No Wave-A worth a dedicated SPLIT session remains.**
>
> **Therefore the pin-wave flywheel is EXHAUSTED at 572** with current
> identification. The two real forward levers are unchanged in spirit but
> re-ranked: **(1) the permuter** (271 fuzzy near-misses — needs NO identification,
> the cleanest remaining yield) and **(2) better game-code identification**
> (re-run bindiff/structural matching with **RB3-Wii** as the reference binary,
> not DC3 — this is what would actually unlock the band3 priority; planner's #5,
> research-grade). Game-code matching is gated on identification, not porting or
> pinning.

**Build-free strategic planning.** Scopes what comes AFTER the strict oracle
pin-wave, now that it's drying up. All numbers recomputed live this session from
`build/45410914/report.json`, the oracle JSONs, and the two rankers. No build, no
edits to objects.json/splits.txt, no commit.

## Baseline (live, this session)

- `measures.matched_functions = **569**`, `matched_code = 98,456`,
  `total_functions = 65,788`. (Post jeff-clamp re-SPLIT + waves 1-2.)
- Category split (the load-bearing CLAUDE.md priority signal):
  - **Game Code: 16 / 326 matched fns, 0.49% matched-code.** ← barely tapped.
  - **Milo Engine Code: 553 / 5,931 matched fns, 11.20% matched-code.**
  - XDK Code: reported 100% matched-code (it's the no-source ceiling; ignore).
- `symbols.txt` is **FRESH** (post-clamp): 65,326 fn syms, only **128 residual
  overlap regions** (was 1353), **0 `jeff_blocked` TUs** in the ranker.

The clamp fix already paid out: the strict ranker that the prompt says surfaces
"22 TUs / 97 fns" is correct and **the basename-looser gate now gives the *same*
22 / 97** — see Q1. The strict oracle is genuinely exhausted.

---

## Q1 — Oracle expansion (wave-4 scoping): mostly scatter, ~10-25 fns left

**`rank` (strict, tail-resolution) and `rank --basename-match` produce the
IDENTICAL set: 22 NEW-pinnable TUs / 97 oracle fns / 70 in dominant clusters.**

The execution-schedule §2.4 figure of "~194 TUs / ~981 fns" is **stale**. It was
computed pre-clamp when far fewer TUs were pinned; the looser basename gate that
*used* to inflate the count now lands on TUs that are **already pinned** (611
pinned basenames; the gate drops 554-582 of the 576-604 source-present TUs as
already-done). `--basename-match` adds **0** new TUs over strict — the inflation
lever is spent. There is no hidden wave-4 in the looser gate.

### The 22 candidates, triaged by density (the real pin signal)

Tight, genuinely pinnable (density ≥10%, source present):

| TU | oracle fns (primary) | density | group | note |
|---|---|---|---|---|
| `ContentMgr.cpp` | 10 (9) | **32%** | engine | flagged as wave-2 follow-up (prefix after FlowCommand) |
| `HeadsetPlaybackEffect.cpp` | 3 (3) | 16% | synth_xbox | small, clean |
| `EnvelopeGenerator.cpp` | 2 (2) | 50% | synth_xbox | **wave-2 ICF drop** — re-pin candidate post-clamp |
| `GainEffect.cpp` | 2 (2) | 40% | synth_xbox | **wave-2 ICF drop** |
| `ThreeDSound.cpp` | 2 (2) | 4.1%→ small span | synth | |
| `MidiChannel.cpp` | 1 | 50% | synth | **wave-2 ICF drop** |
| `AudioDucker.cpp` | 1 (of 3) | 50% | synth | **wave-2 compile-blocked (Fader API)** |
| singletons @100% density | 1 each | 100% | mixed | `HamPartyJumpData`, `MotionBlur`, `WebSvcMgrCurl`, `SoftParticles`, `Fur_NG`, `SongUtl`, `HttpReqCurl`, `Fur`, `xboxmem`, `FlowOutPort` |

Loose hulls (density <7% — pin gets stolen by neighbors; **drop per pin-wave-2
§3 rule 1**): `PlatformMgr_Xbox` (1.7%, 31 fns but 122KB hull),
`UIPanel` (1.8%), `MetaMusicManager` (4.5%), `UILabelDir` (0.9%),
`FlowPickOne` (0.5%).

**The RTTI-low + vtable-only oracle (`unified_id_vtable.json` 37 fns, RTTI-LOW in
`unified_id_rtti.json`) adds NOTHING pinnable.** The 15 C-minus addrs are
`?Init@RndShaderMgr` (rnddx9 vs rndobj — ambiguous), and the rest are
**XDK D3DXShader / d3dx9 / xaudio2 LEAPCORE** = no source anywhere. Same for the
44 `ambiguous_src` (all `xdk/nuispeech/main.cpp`, `xdk/d3d9i/movie.cpp`,
`xdk/xgraphics/main.cpp` — XDK with no transferable source). The non-bindiff
oracles are fully absorbed.

**Realistic wave-4 size: ~13-15 single-/double-fn TUs that pin cleanly + the
3-4 wave-2 ICF drops worth retrying post-clamp. Expected yield: +10 to +25
matched fns**, and several will land 0 (ICF singletons that can't match from
source — the same class that cost wave-1 its 14 drops). This is **scatter
cleanup, not a wave.** Worth ~1 Sonnet session as a *batched* tail, not a
campaign. Highest single item: `ContentMgr.cpp` prefix (9 fns, 32% density,
already named as a follow-up in pin-wave-2 §6).

---

## Q2 — Band3 game-code oracle: the priority, and it's PORT-driven not oracle-driven

This is the CLAUDE.md priority ("decomp value concentrates in the GAME") and it
is where the real unrealized mass sits — but the lever is **porting + pinning,
not the bindiff oracle**, for a structural reason.

### The oracle's game attribution is DC3-named and largely non-transferable

Of 13,491 unique oracle addrs: **6,915 engine, 5,347 XDK, 620 "game", 556
no-src.** The 620 "game" addrs are keyed to **DC3's** `lazer/meta_ham`,
`lazer/game`, `lazer/net_ham` TUs — i.e. *Dance Central 3* game files. Breakdown
of those 620 by RB3 availability:

- **68 fns / 13 TUs already pinned-or-wired** (the meta_band ports done so far:
  AccomplishmentManager 24, AccomplishmentProgress 15, ContextChecker 9, …).
- **0 fns present-in-`src/`-but-unpinned** under the basename gate.
- **552 fns / 88 TUs "absent"** — and these are **DC3-specific class names that
  do not exist in RB3** at all: `Challenges` (34), `MetaPanel` (32),
  `HamStorePanel` (31), `HamSongMgr` (27), `MetagameRank` (20),
  `CampaignProgress` (17), … Verified via `lookup_rb3wii`: MetagameRank,
  Challenges, CampaignProgress, HamSongMgr **return zero hits in RB3-Wii.** They
  are Dance-Central concepts. The bindiff oracle matched an RB3 function to a
  *structurally-similar DC3 game function*; the real RB3 owner is a
  differently-named RB3 TU. So the DC3-name address is a weak hint, not a pin
  target. (A few DC3 names DO map to real RB3-Wii files — MetaPanel, Campaign,
  ProfileMgr, SongStatusMgr, Game — but those are the minority.)

**Conclusion:** the bindiff oracle is the wrong instrument for game code. The
right oracle is **RB3-Wii itself** (named functions + MILO_ASSERT path strings),
which `CLAUDE.md` already designates as the game-code oracle.

### The actual cheap game win is already sitting in the tree: pin the wired-but-unpinned meta_band TUs

- Local `src/band3/meta_band/`: **41 .cpp**; **39 wired in objects.json**;
  **only 1 pinned in splits.txt** → **38 wired-and-compiling-but-UNPINNED.**
- These were ported across the wave-1..3 meta_band sessions (compile-clean per
  `meta_band-port-breaking-changes.md`) but **never had a `.text` span derived**,
  so they contribute **+0** to matched_functions. This is exactly the
  **execution-schedule S3 / D3 "band3 pin wave"** that was scoped but never ran.
- They already pass the hard part (Wii→360 porting + compile). Pinning is the
  mechanical span-derivation step: for each, take the oracle/autoid addrs that
  map to it (where present), or derive the span from RB3-Wii function names via
  `fingerprint_match.py autoid` / the orchestrator DB, snap to `symbols.txt`,
  pin `.text`.

**Wii→360 porting cost for NEW files** (the 131-file meta_band backlog: 172 in
RB3-Wii − 41 local): real but well-documented. `meta_band-port-breaking-changes.md`
is a mature playbook — `Symbol::mStr`→`.Str()`, strip `#pragma force_active`,
`TheUI` pointer-not-ref, single-arg `ObjPtr<T>`, `using Hmx::Object::Handle` for
diamond inheritance, `operator new(unsigned long)`→`size_t`, etc. Observed
wave-rate ~10 TUs/session at ~88% clean. Deep beatmatch-dependent files
(BandStoreUIPanel, AccomplishmentPanel) stay deferred.

**Cheapest band3 wins, ranked:**
1. **Pin the 38 wired-unpinned meta_band TUs** (zero porting; pure span+pin).
   Game-category fns currently 16/326 — this is the direct lever on that number.
2. **Port + pin the next ~10-20 meta_band files from RB3-Wii** that have clean
   include chains (avoid beatmatch/DrumMap dependents).
3. **AccomplishmentManager-cluster tail** — the 68 already-attributed game oracle
   fns confirm these TUs have identifiable functions; finish their spans.

---

## Q3 — Lever ranking (yield-per-effort)

| # | Lever | Est. matched-fn yield | Effort | Model | Depends on |
|---|---|---|---|---|---|
| **1** | **Band3 pin wave: pin the 38 wired-unpinned meta_band TUs** | **+20 to +60** (game-category, the priority) | 1 Sonnet session, main SPLIT lane | Sonnet | clamp landed (✓ done); span derivation per TU |
| **2** | **Permuter campaign on the queue head** | +15 to +50 (hit-or-miss) | 2-3 worktree sessions, parallel | Sonnet+permute skill | C smoke-test passing; runs in worktrees (no SPLIT contention) |
| **3** | **Band3 PORT+pin wave (next ~15 meta_band from RB3-Wii)** | +30 to +80 over 2-3 sessions (priority) | porting-heavy, then SPLIT | Sonnet (port) | playbook (✓), RB3-Wii source (✓), addresses via autoid |
| **4** | **Wave-4 oracle scatter cleanup (Q1)** | +10 to +25 | 1 Sonnet session, batched tail | Sonnet | none (data ready) |
| **5** | **(stretch) widen bindiff coverage / native game-code oracle** | unquantified | research | Opus | n/a — defer |

### Notes on each

- **#1 is the single highest-yield next move** and it is *also* the CLAUDE.md
  priority (game code). It requires **no new porting** — 38 TUs already compile.
  The only work is deriving `.text` spans (the oracle gives addresses for ~13 of
  them directly; the rest derive from RB3-Wii named functions via the autoid /
  orchestrator path) and pinning. It directly attacks the 16/326 Game-Code number
  that the whole project is supposed to be optimizing. **This is the D3 wave the
  execution schedule scoped but never ran.**

- **#2 permuter**: 267 candidates, **205 real**, of which **45 at 99.9-100%, 46
  at 99-99.9%, 51 are ≤128B AND ≥99%** (the highest-hit-rate cell). Caveat is
  real (smoke-test flipped 0/3), so treat as a *parallel, worktree-isolated*
  background campaign that doesn't block the SPLIT lane, spend its budget on the
  small/high-% head (Achievements::Init, CacheMgrXbox ctor, Rnd::OnToggleTimers,
  CharClip::BeatToFrame, UIScreen::InComponentSelect), and abandon per-fn after a
  bounded search. Top units: Rnd_Xbox (13), CharClip (12), Geo (12).

- **#3 is the strategic priority** but is the most effort (porting). Run it
  *after* #1 banks the free pins, so the game category is already moving and the
  porting effort compounds.

- **#4** is genuinely just scatter — batch it onto the end of #1's SPLIT session
  rather than spending a dedicated wave on it.

- **#5**: the strict bindiff oracle is exhausted (Q1). Materially more *game*
  identifications would need either re-running bindiff with RB3-Wii (not DC3) as
  the reference binary, or leaning on the native-port behavioral oracle — both
  are research-grade, defer.

### Recommended next 1-2 waves for the orchestrator

1. **Wave A (Sonnet, main SPLIT lane):** pin the 38 wired-unpinned meta_band TUs
   (#1) + batch the ~13 clean Q1 scatter pins + retry the 3-4 wave-2 synth ICF
   drops post-clamp (#4) in the same SPLIT. Single serialized SPLIT writer.
2. **Wave B (Sonnet, worktree, parallel):** permuter campaign (#2) on the ≤128B /
   ≥99% head, isolated builds so it never touches Wave A's `.ninja-build.lock`.
3. Then **#3** (band3 port+pin) as the ongoing priority stream.

---

## Headline

**The strict oracle is exhausted (22 TUs / 97 fns; basename-looser gate adds 0;
RTTI-low/vtable add 0 — all residue is XDK no-source).** The next real yield is
**not** more oracle — it's the **38 already-compiling meta_band game TUs that are
wired but unpinned**, contributing +0 today. Pinning them is the highest
yield-per-effort move available (**+20-60**, no porting) and it's the
CLAUDE.md-designated priority (Game Code is 16/326). Run it first.
