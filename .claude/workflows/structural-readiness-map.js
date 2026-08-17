// ============================================================================
// STATUS 2026-07-30: DEAD AS WRITTEN -- DO NOT RUN WITHOUT REWORK.
// This workflow drives 'decomp_synth' (scan_and_permute), which lives in a
// separate private repo and is NOT present here, so the commands below will
// fail. Separately, the source-permuter is OFF by standing user directive:
// do not route to /permute until the user re-opens it.
// Kept under version control deliberately: an agent-facing prompt that is
// wrong silently steers every future lane, and while these were untracked
// nothing prevented that. Fix or delete in review -- do not shadow-edit.
//
// CORRECTION 2026-08-17 (task #114): SECOND, INDEPENDENT REASON NOT TO RUN --
// EVERY HARDCODED FAN-OUT NUMBER IN THIS FILE IS WRONG, AND THIS IS THE ONE
// FILE THAT STILL PRESENTS THEM AS CURRENT.
//
//   * The per-unit delta pairs baked into the phase prompts below -- BandDirector
//     (+32, 24), Part (-64, 19), Rnd (-80, 13), MidiParser (-48, 14), WaveFile
//     (-48, 9), DataFile (-16, 18), Crowd (-4, 52/45), Shockwave (+724),
//     Cache_Xbox (+2048), and the rest -- are from the layout_fix_rank **v1**
//     map, which docs/plans/structural-readiness-2026-06-03.md s1 RETRACTED as
//     EH-cleanup-funclet false positives ("~150 fns, not ~700"). This file was
//     never updated to the v2 map.
//   * The v2 map is inflated too. layout_fix_rank's addi branch built its base
//     register as `'r' + m.group(2)` where group 2 already carried its `r`, so
//     bases were spelled rr1/rr12/rr31 and could not match STACK_REGS -- the
//     r12-funclet fix s1 announces never actually took effect. Measured paired
//     over 2128 near-miss fns: the as-shipped tool put 100.0% of the 1385 offset
//     rows it parsed into the STRUCT bucket and 0 into stack; corrected, 31.2% /
//     68.8%. Fixed 2026-08-17 in b57f9e7e.
//   * The "97% of the 1064 near-miss functions ... differ from retail ONLY by
//     struct-field-offset immediates" premise in meta.description and phase 1 is
//     not what the repaired tool measures. On the current tree only 56.7% of
//     near-miss functions carry ANY offset-class delta and 22.1% carry struct
//     evidence. (Different tree and a different denominator from the 2026-05-29
//     hand sample in plans/struct-offset-sweep.md that this 97% descends from --
//     so treat it as unsupported, not as refuted at matched grain.)
//   * /home/free/tmp/layout_fix_rank.json, which phases 1-6 tell every agent to
//     read, is a scratch path outside the repo. It is gone.
//
// If this workflow is ever revived: delete the hardcoded numbers, do not port
// them. Re-run tools/layout_fix_rank.py at b57f9e7e or later. Re-measurement of
// record: <decomp-bench>/archive/runs/rb3x-layout-fix-rank-rerank-2026-08-17/.
// ============================================================================
export const meta = {
  name: 'structural-readiness-map',
  description: 'Map + adversarially verify the foundational base-class layout fixes (struct/header/vtable alignment) that unblock the 1064-fn offset-class near-miss wall. Analysis only — no edits, no builds.',
  phases: [
    { title: 'Map', detail: '6 Opus agents triangulate retail layout per struct-family domain' },
    { title: 'Verify', detail: 'per-domain Sonnet verifier confirms each offset claim vs Ghidra + oracles' },
  ],
}

// ---- shared context every agent gets ----
const PREAMBLE = `
You are working in the rb3-xenon decomp (Xbox 360 Rock Band 3, MSVC X360 PowerPC,
/O1 /Oi /GR /EHsc). We are matching retail machine code from C++ source.

CRITICAL FRAMING — read carefully:
- This is an ARCHITECTURE / STRUCT-ALIGNMENT task. We are getting the codebase
  READY for a later per-function grind. DO NOT grind/permute individual functions.
  DO NOT edit any source, run any build, or commit anything. ANALYSIS ONLY.
- Empirical finding from tools/layout_fix_rank.py: 97% of the 1064 near-miss
  functions (match 80-99.99%) differ from retail ONLY by struct-field-offset
  immediates (lwz/stw/addi rX, off(rBase) with a wrong off). These cascade from a
  few SHARED base-class layout divergences vs retail. Fixing a base class flips
  MANY functions at once. Your job: nail the EXACT correct retail layout for your
  domain's classes, with evidence, so a header edit can be made + measured later.

GROUND TRUTH + ORACLES (in priority order):
1. objdiff offset deltas = the EMPIRICAL truth of "what offset retail reads vs us".
   Run:  python3 scripts/analysis/diff_inspect.py --symbol "<MangledOrDemangled>" --compare-asm --project-dir .
   and:  python3 scripts/analysis/diff_inspect.py --symbol "<sym>" --offsets --project-dir .
   A line like  ~ lwz r3, 0x40(r30)  [off:+4]  means target=0x40, ours=0x3c (we are +4 too big at that field).
   The fan-out map (your units' per-fn deltas) is at: /home/free/tmp/layout_fix_rank.json
   (keys: per_fn[], units[], delta_clusters[]). grep/jq your domain's units out of it.
2. DC3 source (../dc3-decomp/src) = the byte-faithful engine twin (same compiler/flags,
   leaked-map names). grep -rn "class RndDrawable" ~/code/milohax/dc3-decomp/src/
   BUT: dc3 is NEWER than RB3 — it sometimes ADDS a trailing field RB3 lacks (UIPanel
   case) or RB3 has a field dc3 lacks (ObjectDir). dc3 can be a FALSE FRIEND. Verify.
3. rb3-Wii source (../rb3/src) = the GAME-code oracle (MWCC, named, has the older
   layout RB3 shipped). grep -rn "class RndDrawable" ~/code/milohax/rb3/src/
   For RB3-vs-DC3 version differences, rb3-Wii usually has the RB3-correct shape.
4. Ghidra (port 8002, UP but SLOW under load) = read ABSOLUTE field offsets from retail
   machine code of a cited ctor/dtor/accessor. Use NAME-based (hits cache) when possible:
   python3 tools/ghidra/ghidra-decompile.py "RndGroup::SetFrame"   (may take 60-180s; retry once on timeout)
   Treat Ghidra as the TIEBREAKER when oracles disagree on an absolute offset. Do not
   block your whole analysis on it — the objdiff deltas + oracles usually suffice.

OUR CURRENT HEADERS live under src/system/ (engine) and src/band3/ (game). Read the
actual current header before claiming "ours is X" — prior sessions already landed
String 0xc, ObjPtr 0xc, Rnd MI, ObjectDir +4, UIPanel -4, so the tree has moved.

KEY DISTINCTION for every delta you report:
- STRUCT delta (base reg is an object ptr like r30/r29) = layout bug = IN SCOPE, fixable by header edit.
- STACK delta (base reg r1/r31) = regalloc/permuter-class = OUT OF SCOPE, ignore it.
The fan-out map already separates these (struct_deltas vs stack_deltas per fn).

DO NOT touch configure.py or tools/scope_map.py (another agent owns them).
`;

const STRUCT_SCHEMA = {
  type: 'object',
  additionalProperties: false,
  properties: {
    domain: { type: 'string' },
    structs: {
      type: 'array',
      items: {
        type: 'object',
        additionalProperties: false,
        properties: {
          klass: { type: 'string', description: 'class/struct name' },
          header: { type: 'string', description: 'header file path that defines it' },
          current_layout: { type: 'string', description: 'our current field offsets (read from the header), key fields with offsets + size' },
          retail_layout: { type: 'string', description: 'the correct retail field offsets + size, triangulated' },
          field_deltas: { type: 'string', description: 'field-by-field: which field is at wrong offset and by how much; the dominant delta and why' },
          root_cause: { type: 'string', description: 'WHY it diverges: extra/missing base, dc3-newer-trailing-field, MI sub-object size, vtable slot, member reorder, etc.' },
          exact_edit: { type: 'string', description: 'the precise header edit to make (add/remove/reorder which field; flip which base), including any HX_NATIVE guard needed' },
          coupling: { type: 'array', items: { type: 'string' }, description: 'other structs/edits this MUST land together with (e.g. a compensating shift), or [] if standalone' },
          blast_radius_units: { type: 'array', items: { type: 'string' }, description: 'units whose near-misses this fix would touch' },
          blast_radius_fns: { type: 'integer', description: 'approx number of near-miss fns this would unblock' },
          risk_class: { type: 'string', enum: ['BOUNDED', 'COUPLED', 'NEEDS_BODY_PORT', 'SPECULATIVE'] },
          predicted_effect: { type: 'string', description: 'predicted: net-positive cascade / net-zero(compensating) / needs paired edit; and rough +N estimate' },
          evidence: { type: 'array', items: { type: 'string' }, description: 'concrete citations: objdiff [off:+N] on which fn, dc3/rb3-Wii grep lines, Ghidra offsets from which fn_addr' },
          confidence: { type: 'string', enum: ['high', 'medium', 'low'] },
        },
        required: ['klass', 'header', 'current_layout', 'retail_layout', 'field_deltas', 'root_cause', 'exact_edit', 'coupling', 'blast_radius_units', 'blast_radius_fns', 'risk_class', 'predicted_effect', 'evidence', 'confidence'],
      },
    },
    notes: { type: 'string', description: 'cross-cutting observations, things you could not resolve, suggestions for the grind session' },
  },
  required: ['domain', 'structs', 'notes'],
}

const VERDICT_SCHEMA = {
  type: 'object',
  additionalProperties: false,
  properties: {
    domain: { type: 'string' },
    verdicts: {
      type: 'array',
      items: {
        type: 'object',
        additionalProperties: false,
        properties: {
          klass: { type: 'string' },
          claim_checked: { type: 'string', description: 'the specific layout/offset claim you verified' },
          verdict: { type: 'string', enum: ['CONFIRMED', 'REFUTED', 'UNCERTAIN'] },
          corrected_layout: { type: 'string', description: 'if REFUTED/partially: the corrected layout; else "as stated"' },
          ghidra_offset_evidence: { type: 'string', description: 'absolute offsets you read from Ghidra (fn name/addr + what it reads), or why Ghidra was unavailable' },
          recommend: { type: 'string', enum: ['LAND_NOW', 'BUILD_VERIFY', 'SPEC_ONLY', 'DROP'] },
          land_priority: { type: 'integer', description: '1=highest fan-out/safest first; for ordering the landing sequence' },
          notes: { type: 'string' },
        },
        required: ['klass', 'claim_checked', 'verdict', 'corrected_layout', 'ghidra_offset_evidence', 'recommend', 'land_priority', 'notes'],
      },
    },
    notes: { type: 'string' },
  },
  required: ['domain', 'verdicts', 'notes'],
}

const DOMAINS = [
  {
    key: 'obj-core',
    title: 'FOUNDATIONAL: Hmx::Object / ObjectDir / ObjRef-ObjPtr remnants / Dir',
    body: `Your domain is the FOUNDATIONAL object layer — these classes are bases of almost
everything, so getting them exactly right is the highest-leverage work.

Classes/headers to nail:
- Hmx::Object  (src/system/obj/Object.h). PRIOR NOTE (2026-05-29, may be stale —
  VERIFY against current header): Object was "still structurally off: mName@0x20 /
  mDir@0x24 vs retail 0x18/0x1c; retail inlines TypeProps@0x4 + mNote as const char*
  @0x14 (not a String); vtable has one extra slot (InitObject extra and/or IsDirPtr
  placement) so SetName is at vtable+0x40 not +0x44". DETERMINE if this is STILL true
  in the current tree (String 0xc + ObjPtr 0xc already landed since that note). If
  Object is still +8 on mName/mDir, that is a uniform cascade.
- ObjectDir (src/system/obj/Dir.h) — note ObjectDir +4 was already LANDED (commit
  ee014aa); confirm current size is correct (0xa0) and not regressed.
- The +16 ObjPtr/base remnant cluster (delta_clusters delta=+16): Group(4), FileCache(4),
  AsyncFile(3), CharIKFoot(3), CharBoneOffset(2). +0x10 on these suggests a shared
  base member is +0x10 too big (two ObjPtr=0x14-vs-0xc? a remaining DC3-added base?).
  Find the common base and the offending field.
- Dir cluster (delta -32 on Dir(3), -8): check src/system/obj/Dir.h / RndDir.
- DataArray/DataNode dispatch (src/system/obj/Data.h) ONLY if it shows up in your
  units — the "DataArray::Sym/Node via vtable indirection" note (game-code fan-out)
  may belong here or to game-band; coordinate via your notes.

Docs to read: docs/plans/engine-baseclass-layout-bugs.md (§1,2,4,4b), docs/plans/
hmx-object-layout.md, docs/plans/objptr-family-relayout-migration.md (§12 = ground truth).`,
  },
  {
    key: 'rndobj',
    title: 'BIGGEST CASCADE: RndObj MI chain (Draw/Trans/Anim/Highlightable/Group/Mesh/Part/Tex/Light)',
    body: `Your domain is the rendering MI cascade — by fan-out the largest. RndGroup, RndMesh,
RndDrawable, RndTransformable, RndAnimatable, RndHighlightable, RndPollable form a
multiple-inheritance chain; sub-object offsets/sizes drive the deltas below.

Units + dominant struct deltas from the fan-out map (read /home/free/tmp/layout_fix_rank.json):
- Part(-64,19) [cluster -64 also: SpotlightDrawer, PartAnim, PollAnim], TexBlender(-64)
- Rnd(-80,13) [cluster -80 also touches CharBonesBlender]
- Draw(+32,3), DepthBuffer3D(+80,4), LightPreset(+60,3), Spotlight(+48,3)
- Gen(+16,7), Group(+16,4), MeshAnim(+160,3), Mesh(-8,8), TexRenderer(-32,4), Rnd_Xbox(-32,3)
Note LightPreset(40 near-miss fns, mixed deltas) and Mesh(29, mixed) are MULTI-bug
units — multiple base divergences stacked; decompose them.

Headers: src/system/rndobj/{Draw,Trans,Anim,Group,Poll,Highlightable,Mesh,Part,
TexBlender,TexRenderer,Light*,Gen}.h. Prior session landed Rnd MI (+85) +
RndDrawable mClipPlanes removal + RndTransformable reorder; CONFIRM what is current
vs what still diverges. Doc: docs/plans/engine-baseclass-layout-bugs.md §3 + the Rnd
MI sections in the memory note (RndTransformable@0x34, Hmx::Object vbase@0x148, etc.).
Triangulate each Rnd base-class size/offset; identify which sub-object is mis-sized.`,
  },
  {
    key: 'char',
    title: 'Character/animation structs (Crowd, CharBone*, CharClip, CharHair, HamCamTransform, Dancer)',
    body: `Your domain is character + animation. Crowd is the single biggest near-miss unit (52
fns, 45 struct, mixed deltas dominated by -4) — likely multiple base bugs.

Units + dominant struct deltas (read /home/free/tmp/layout_fix_rank.json):
- Crowd(-4,52 fns / mixed), CharBone(-64,7), CharBonesBlender(-80,2), CharClip(-48,3),
  CharHair(+4,4), CharIKFoot(+16,3), CharBoneOffset(+16,2), CharSleeve(+12,2),
  CharNeckTwist(+52,2), HamCamTransform(+32,5), DancerSequence(+36,3)
Note CharClip shares delta -48 with MidiParser/WaveFile (midi-data domain) — that may
be a COMMON base (e.g. an embedded vector/array struct or a shared accessor) — flag it.
Note the +16 on CharIKFoot/CharBoneOffset overlaps obj-core's +16 ObjPtr-remnant
cluster — likely the same shared-base root cause; coordinate via notes.

Headers: src/system/char/*.h (CharBone.h, CharClip.h, CharBonesObject.h, CharHair.h,
etc.), and hamobj where relevant. Oracle: rb3-Wii src/system/char is the GAME-correct
shape; dc3 char may be newer. Triangulate CharBone/CharBonesObject base layout first
(it underlies most Char* units).`,
  },
  {
    key: 'midi-data',
    title: 'MIDI + Data structs (MidiParser, MidiInstrument, WaveFile, DataFile, DataFunc, StreakMeter)',
    body: `Your domain is MIDI parsing + the Data/DataArray family.

Units + dominant struct deltas (read /home/free/tmp/layout_fix_rank.json):
- MidiParser(-48,14 COHERENT) [cluster -48 also: WaveFile(9), CharClip(3)]
- WaveFile(-48,9 COHERENT), MidiInstrument(-52,4)
- DataFile(-16,18 COHERENT) [cluster -16 also: StreakMeter(6), NetLoader_Xbox(2)]
- DataFunc(+160,3), StreakMeter(-16,6 COHERENT)
The shared -48 across MidiParser/WaveFile/CharClip strongly suggests a COMMON embedded
struct or base whose size is off by 0x30 — find it (a shared file/stream/array member?).
The -16 across DataFile/StreakMeter suggests a -0x10 base/member.

Headers: src/system/midi/*.h (MidiParser.h, MidiInstrument.h, etc.), src/system/obj/
Data.h + DataFile, src/system/utl (WaveFile / file structs). Doc: docs/plans/
permuter-sweep-struct-cascades-2026-05-29.md (DataArray dispatch + the self-contained
table: MidiParser +16, SampleZone sizeof 0x1c-vs-0x50, etc. — re-measure, the tree moved).`,
  },
  {
    key: 'game-band',
    title: 'GAME: BandDirector(+32) + meta_band Accomplishment + Store/SongDB/EventTrigger/RhythmBattle',
    body: `Your domain is RB3 GAME code (src/band3/). BandDirector is the biggest COHERENT
game unit: 29 near-miss fns, 24 share delta +32 (+0x20) — a clean single layout bug.

Units + dominant struct deltas (read /home/free/tmp/layout_fix_rank.json):
- BandDirector(+32,24 COHERENT) [cluster +32 also: Draw(3) — but Draw is rndobj]
- HamCamTransform(+32,5), StorePanel(+32,4), SongDB(+32,2), Line(+32,2)
- EventTrigger(+16,4), AccomplishmentManager(+16,9 / 41 near-miss mixed),
  AccomplishmentProgress(+4,7), NetSync(+16,2), RhythmBattle(+176,3), UIEventMgr(+16,2)
The +32 cluster (BandDirector/HamCamTransform/StorePanel/SongDB/Line) likely share a
base or an embedded member that is +0x20. AccomplishmentManager (41 fns!) is a multi-bug
giant — decompose its deltas.

Headers: src/band3/**/*.h (band3/game, band3/meta_band, band3/world, etc.). ORACLE
PRIORITY HERE: rb3-Wii (../rb3/src/band3) is the GAME oracle — same game code, named,
RB3-correct. dc3 does NOT have RB3 game classes. Use rb3-Wii heavily. Confirm
BandDirector's base chain + members against rb3-Wii BandDirector.h.`,
  },
  {
    key: 'ui-flow-bounded',
    title: 'UI base family + Flow + bounded self-contained (Panels, UIManager, PanelDir, Flow*, Mic, Cache_Xbox)',
    body: `Your domain is UI bases + Flow + small self-contained structs (the likely "land-now"
bucket plus the held UIManager spec).

Units + dominant struct deltas (read /home/free/tmp/layout_fix_rank.json):
- UI/UIManager(-8, negative offsets = virtual base), UITransitionHandler(-80,2)
- MoviePanel(-4,6 COHERENT), CreditsPanel(-4,2), TourDescPanel(-4,7), CalibrationPanel(-64?),
  PanelDir(-8,4), Overlay(-8,2)  [the -4/-8 UI-panel family — same class as the landed UIPanel -4 fix]
- FlowSound(+40,3), FlowNode(-44,3), FlowIf(-96,4 COHERENT), FlowQueueable(-32,2)
- Mic(+8,3), Shockwave(+724,2), Cache_Xbox(+2048,2)  [large deltas = likely a wrong sizeof / missing big member]

For UIManager: the FULL RB3-360 virtual-base layout is already banked but HELD (needs a
UI.cpp body port). Read docs/plans/ui-base-layout-reconstruction.md AND the
project-engine-baseclass-layout-wall memory's "2026-06-02 UIManager wall" section.
RE-VERIFY the held layout against the current header + objdiff; classify it
NEEDS_BODY_PORT and produce the turnkey spec (don't try to make it BOUNDED).
For the -4/-8 panel family: this is the same dc3-newer-trailing-field pattern as the
landed UIPanel fix — find which base (UIComponent/UIScreen/PanelDir) carries a dc3-only
field. For Flow* and Mic/Shockwave/Cache_Xbox: these are mostly self-contained sizeof/
member bugs — good BOUNDED candidates. Headers: src/system/ui/*.h, src/system/flow/*.h.`,
  },
]

phase('Map')
const results = await pipeline(
  DOMAINS,
  // STAGE 1: MAP (Opus, inherit) — triangulate retail layout, return structured specs
  (d) => agent(
    `${PREAMBLE}

# YOUR DOMAIN: ${d.title}
${d.body}

# METHOD
1. Read the current header(s) for your classes — record OUR layout (don't trust stale notes).
2. Pull your units' per-fn deltas from /home/free/tmp/layout_fix_rank.json.
3. For 2-4 representative near-miss fns per struct, run diff_inspect --compare-asm /
   --offsets to SEE the exact [off:+N] deltas and which base register (struct vs stack).
4. Triangulate the correct retail layout: rb3-Wii + dc3 source for field lists, objdiff
   deltas for the offsets, Ghidra (cited ctor) for absolute offsets when needed.
5. For each struct, fill the schema: current vs retail layout, field deltas, root cause,
   the EXACT header edit, coupling, blast radius, risk_class, predicted effect, evidence.

RISK CLASS GUIDE:
- BOUNDED: a standalone header edit (add/remove/reorder a field, fix a sizeof) with no
  compensating edit needed; should net-positive on build. The land-now bucket.
- COUPLED: must land together with another edit (a compensating shift) or it nets zero/regresses.
- NEEDS_BODY_PORT: the fix requires rewriting function bodies from the binary (e.g. UIManager) — spec it, don't expect a header-only win.
- SPECULATIVE: plausible but you couldn't confirm the offset — flag low confidence.

ALSO: write your full structured spec to /home/free/tmp/readiness/${d.key}.json (use the
Write tool) so it is durable, THEN return it as your StructuredOutput.
Remember: ANALYSIS ONLY. No source edits, no builds, no commits.`,
    { label: `map:${d.key}`, phase: 'Map', schema: STRUCT_SCHEMA }
  ),
  // STAGE 2: VERIFY (Sonnet) — adversarially confirm each claim against Ghidra + oracles
  async (mapResult, d) => {
    if (!mapResult || !mapResult.structs || !mapResult.structs.length) {
      return { domain: d.key, map: mapResult, verdict: null }
    }
    const claims = mapResult.structs.map(s =>
      `- ${s.klass} (${s.header}): root=${s.root_cause}; retail=${s.retail_layout}; edit=${s.exact_edit}; risk=${s.risk_class}; predicted=${s.predicted_effect}`
    ).join('\n')
    const verdict = await agent(
      `${PREAMBLE}

# VERIFICATION PASS for domain: ${d.title}
A mapping agent produced these layout claims. Your job is ADVERSARIAL: try to REFUTE
each load-bearing offset claim. A wrong base-class layout edit regresses the WHOLE
baseline, so we must not green-light a bad one. Default to UNCERTAIN if you cannot
positively confirm an offset.

CLAIMS TO CHECK:
${claims}

# METHOD (per struct)
1. Confirm the OUR-side layout by reading the actual current header.
2. Confirm the RETAIL offset empirically: pick the fn the mapper cited (or another
   near-miss in that unit) and run diff_inspect --compare-asm --project-dir . to see
   the real [off:+N] delta and base register. Does it match the claim's direction+size?
3. Confirm the ABSOLUTE retail offset via Ghidra on a cited ctor/dtor/accessor
   (ghidra-decompile.py "Class::Method", be patient, retry once on timeout). Read the
   field offset from the machine code. If Ghidra is unavailable, say so and rely on
   objdiff delta + both source oracles agreeing.
4. Cross-check rb3-Wii AND dc3 source — do they agree on the field list/order? If dc3
   and rb3-Wii DISAGREE, rb3-Wii wins for RB3 (note the dc3-false-friend).
5. Classify: CONFIRMED (offset proven), REFUTED (claim wrong — give corrected layout),
   UNCERTAIN (can't prove). Recommend LAND_NOW (BOUNDED + CONFIRMED), BUILD_VERIFY
   (CONFIRMED but coupling/compensation uncertain — needs a real build to know net),
   SPEC_ONLY (NEEDS_BODY_PORT), or DROP (REFUTED/SPECULATIVE). Set land_priority
   (1=do first; favor high fan-out + high confidence + BOUNDED).

Write your verdicts to /home/free/tmp/readiness/${d.key}.verdict.json (Write tool) too.
ANALYSIS ONLY.`,
      { label: `verify:${d.key}`, phase: 'Verify', model: 'sonnet', schema: VERDICT_SCHEMA }
    )
    return { domain: d.key, map: mapResult, verdict }
  }
)

const findings = results.filter(Boolean)
log(`mapped+verified ${findings.length}/${DOMAINS.length} domains`)
return {
  domains: findings.map(r => r && r.domain),
  artifacts_dir: '/home/free/tmp/readiness/',
  findings,
}
