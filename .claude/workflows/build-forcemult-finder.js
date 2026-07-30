export const meta = {
  name: 'build-forcemult-finder',
  description: 'Build + validate the force-multiplier finder tools (gap #1/#2): (A) inline-policy finder (DC3-inlined-vs-retail-out-of-line bl mismatches, the Str pattern) and (B) member-delta finder (uniform this-relative member-offset deltas = DC3 dropped/added member). Read-only objdiff analysis; validate against the known wins; emit ranked candidate lists for a later apply wave. Returns tool code for the coordinator to commit. NEVER commit to main.',
  phases: [ { title: 'BuildTools', detail: 'inline-policy finder || member-delta finder, each validated on known wins' } ],
}

const REPO = '/home/free/code/milohax/rb3-xenon'
const OUT = '~/tmp/forcemult'

const SCHEMA = {
  type: 'object', additionalProperties: false,
  properties: {
    tool: { type: 'string' }, tool_path: { type: ['string','null'] },
    built: { type: 'boolean' }, validated: { type: 'boolean' },
    validation_notes: { type: 'string' },     // did it re-flag the known wins?
    n_candidates: { type: 'integer' },
    candidates_path: { type: ['string','null'] },
    top_candidates: { type: 'array', items: { type: 'object', additionalProperties: false,
      properties: { target: { type: 'string' }, n_affected: { type: 'integer' }, fix_hint: { type: 'string' } },
      required: ['target','n_affected','fix_hint'] } },
    tooling_gaps: { type: 'array', items: { type: 'string' } },   // gaps/ideas surfaced (feedback loop)
    notes: { type: 'string' },
  },
  required: ['tool','built','validated','n_candidates','tooling_gaps','notes'],
}

phase('BuildTools')

const COMMON = `Work in an isolated worktree: cd ${REPO} && scripts/setup_worktree.sh .claude/worktrees/fm-<slug> fm-<slug> (its build/ is a reflink of main's 6568-state objs — objdiff reads them read-only, NO rebuild needed). NEVER edit/commit main. Tooling input: bin/objdiff-cli diff -p . '<sym>' -f json --include-instructions (the instruction record has match_type, target{opcode,args}, base{opcode,args}); build/45410914/report.json (units->functions, fn names + match_percent_normalized); tools/true_progress.py writes /tmp/true_progress.json (per-fn bucket+counts). Develop the tool in tools/<name>.py in the worktree, run it, mkdir -p ${OUT}. Return the SCHEMA incl. tooling_gaps (concrete tool ideas this surfaced — the feedback loop).`

const inlinePolicy = () => agent(`Build + validate tools/inline_policy_finder.py — detects the INLINE-POLICY force-multiplier (the highest-EV repeatable engine lever). ${COMMON.replace(/<slug>/g,'inline')}

THE PATTERN (proven win: Str::operator==/!= — commit ce16bfa, +6): DC3 (newer source we inherited) made a small method INLINE in a header; retail RB3 kept it OUT-OF-LINE. So our build INLINES the body while retail emits a 'bl <fn>'. In objdiff this shows as: a near-miss where the TARGET has 'bl <callee>' (or a sequence of them) and the BASE has an INSERT/REPLACE cluster of inlined arithmetic implementing that callee (or the reverse: base 'bl', target inlined). The fix = move that method out-of-line (header decl + .cpp def) — and it's a FORCE-MULTIPLIER because every near-miss that calls the method flips at once.

BUILD THE FINDER:
1. Over the near-miss pool (use /tmp/true_progress.json [90,100) + report.json named near-misses), objdiff each and detect the inline-policy signature: target-bl-vs-base-inlined-block (and reverse). Identify the callee symbol (resolve the bl target / the inlined body's identity — strcmp/accessor/operator shape).
2. GROUP by callee method; rank by # of distinct near-miss functions exhibiting it (the multiplier). For each, emit a fix_hint: which header method to move out-of-line (or inline), checking whether it's currently inline in our src/ header (the actionable signal) + the rb3-Wii/DC3 oracle's out-of-line form.
3. VALIDATE: confirm the finder RE-FLAGS String::operator==(const String&) as a top candidate from the pre-ce16bfa state IF reconstructable; at minimum, run it on the current pool and sanity-check a couple hits by hand (objdiff + read the header). Report validation_notes honestly.
4. Emit ${OUT}/inline_candidates.json = ranked [{callee, n_affected, header, current_form, fix_hint}]. Return SCHEMA (tool='inline_policy_finder', tool_path=worktree tools/inline_policy_finder.py).`,
  { label: 'inline-policy-finder', phase: 'BuildTools', schema: SCHEMA })

const memberDelta = () => agent(`Build + validate tools/member_delta_finder.py — detects the DC3 DROPPED/ADDED-MEMBER force-multiplier across the STRUCT_WORK pool. ${COMMON.replace(/<slug>/g,'member')}

THE PATTERN (proven wins: CharSleeve/CharIKSliderMidi mMe -0xC +3; Gem Tail -0x14 +1; postproc pad +1): a class is one (or more) members too small/large vs retail because DC3 dropped/added a member, so EVERY this-relative member access in the class's methods is shifted by a uniform constant. In objdiff: across a near-miss's diffs, the this-relative loads/stores (lwz/lfs/stw/stfs off r3 or off r31-as-this, and addi/subi computing &member) show a UNIFORM offset delta C vs target; C = the missing/extra member's size. The fix = add/remove the member in the header at the right offset — FORCE-MULTIPLIER across the class's methods.

BUILD THE FINDER:
1. Over the STRUCT_WORK pool (/tmp/true_progress.json bucket==STRUCT_WORK, ~667 fns), objdiff each; for each, compute the set of this-relative member-offset deltas. CRITICAL: distinguish a real UNIFORM member-offset delta (all member accesses shifted by the same C, src reg = this/r3 or the class's frame base) from STACK-FRAME slot shifts (off r1 / frame-local, = regalloc, NOT layout) and funclet frame-recon (dest=frame reg) — reuse the true_progress.py classify_insn logic / ~/tmp/recon/common.py.
2. GROUP by CLASS (the unit / the demangled class of the member-accessing fns); a class with a CONSISTENT uniform C across multiple methods = a clean dropped/added-member candidate. Rank by # affected methods × consistency. Emit fix_hint: the class, the offset where the member is missing/extra, the delta C, and a cross-check against DC3 (../dc3-decomp) / rb3-Wii (../rb3) / Ghidra for the member's identity.
3. VALIDATE: confirm it would flag the CharSleeve/CharIKSliderMidi -0xC and Gem Tail patterns (these are now FIXED on main so the delta is gone — instead validate the LOGIC on 1-2 current STRUCT_WORK classes by hand: does the reported uniform-C match the objdiff reality?). Report validation_notes honestly; be careful NOT to flag coupled-base/vbase walls as clean (those are deep, not one-member fixes) — note them separately.
4. Emit ${OUT}/member_candidates.json = ranked [{class, delta, offset, n_affected, fix_hint}]. Return SCHEMA (tool='member_delta_finder').`,
  { label: 'member-delta-finder', phase: 'BuildTools', schema: SCHEMA })

const results = (await parallel([inlinePolicy, memberDelta])).filter(Boolean)
return { results }
