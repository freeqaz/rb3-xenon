export const meta = {
  name: 'rbtree-recon',
  description: 'R-B tree +4 coupled-base recon: settle the ctor-init question (decisive), predict top improvers, pre-classify candidate compensation pads. Read-only.',
  phases: [
    { title: 'Recon', detail: 'member-identity + improvement-prediction + pad-classification, in parallel' },
  ],
}

const REPO = '/home/free/code/milohax/rb3-xenon'

const BACKGROUND = `
BACKGROUND — the "R-B tree +4" coupled-base lever (rb3-xenon decomp, matching MSVC-X360 PPC machine code):

Our STLport \`_Rb_tree\` (src/system/stlport/stl/_tree.h) is 0x18 bytes:
  _Rb_tree_base::_M_header  = 0x10  (_STLP_alloc_proxy<_Rb_tree_node_base,...>: color@0 + parent@4 + left@8 + right@c; allocator EBO-folds to 0)
  _M_node_count (size_type) = +4 -> 0x14
  _M_key_compare (empty less<> MEMBER, not base, so padded to 4) = +4 -> 0x18
Retail RB3 STLport \`_Rb_tree\` is 0x1c -- exactly ONE extra 4-byte member after _M_key_compare that our DC3/rb3-Wii-derived STLport stripped.
Consequence: EVERY std::map/set/multimap/multiset member in the game is 4 bytes too small, so every data member declared AFTER a map/set member sits 4 bytes too low vs retail. This is why AccomplishmentManager (12 maps) has 41 functions stuck at 99.8-99.9%.
A prior crude A/B (adding an uninitialized \`void* _M_ab_pad;\` after _M_key_compare) measured net +10 with AccomplishmentManager +28 -- strong evidence the fix is correct AND that the extra word is NOT written by the ctor.

The _Rb_tree default ctor (lines 365-367) is:
  _Rb_tree() : _Rb_tree_base(allocator_type()), _M_node_count(0), _M_key_compare(_Compare()) {}
_M_empty_initialize() (the base) writes header.color, header.parent=0, header.left=&header, header.right=&header (4 stores). Plus _M_node_count=0 (1 store). So a default map ctor = 5 stores; if retail writes a 6th store into map_subobject+0x18, the extra member IS initialized.

Oracles: Ghidra MCP on port 8002 (python3 tools/ghidra/pcode_inspect.py "Name", or tools/ghidra/ghidra-decompile.py) -- may be slow under load, retry/timeout-tolerant. dc3 twin source: ../dc3-decomp/src (same compiler/flags, byte-identical _tree.h so NOT a counter-example but useful for STLport version). rb3-Wii: ../rb3/src. Retail dtk asm (when present): build/45410914/asm/<unit>.s.
`

phase('Recon')

const [member, predict, pads] = await parallel([
  () => agent(`${BACKGROUND}

YOUR TASK (DECISIVE): determine whether the retail map/set default-constructor WRITES the 6th word (the extra 4-byte member at map_subobject+0x18), and recommend the exact source change.

Method:
1. Inspect a retail function that default-constructs a std::map or std::set and count the stores into a freshly-constructed map sub-object. AccomplishmentManager::AccomplishmentManager (mangled ??0AccomplishmentManager...) inlines 12 map default-ctors -- look for the repeating per-map store pattern. Also try simpler single-map classes (e.g. NameGenerator, BandSongMgr, Campaign) whose ctor is less cluttered. Use Ghidra MCP (port 8002) pcode_inspect.py / ghidra-decompile.py. If Ghidra times out, find any std::map/set ctor or insert path and inspect the node/header init.
2. A default map ctor with the 0x18-member UNINITIALIZED shows exactly 5 stores per map (color, parent=0, left=&hdr, right=&hdr, node_count=0). If you see a 6th store (e.g. a 0 or a pointer into map_subobject+0x18), the member is initialized -- report its value.
3. Cross-check by examining the per-map STRIDE in a multi-map class's ctor/dtor (consecutive map sub-objects should be 0x1c apart in retail).

Decide and report (schema): whether the ctor writes the 6th word; if yes, the value and the exact init-list addition; the exact C++ declaration line to add after \`_Compare _M_key_compare;\` at _tree.h:316 (default to \`  int _M_buf_alloc;\` or similar uninitialized POD if NOT written); your best guess at the real STLport member name; the evidence (cite the function + store offsets you saw); confidence. Be rigorous -- this gates a ~20-minute whole-binary rebuild, so a wrong call is expensive.`,
    { label: 'member-identity', schema: {
      type: 'object', additionalProperties: false,
      properties: {
        ctor_writes_sixth_word: { type: 'boolean' },
        init_value: { type: ['string', 'null'] },
        recommended_decl: { type: 'string', description: 'exact C++ line to add after _M_key_compare at _tree.h:316' },
        recommended_init: { type: ['string', 'null'], description: 'init-list addition for each ctor, or null if uninitialized' },
        member_name_guess: { type: 'string' },
        evidence: { type: 'string', description: 'function(s) examined + store offsets observed' },
        confidence: { type: 'string', enum: ['high', 'medium', 'low'] },
      },
      required: ['ctor_writes_sixth_word', 'recommended_decl', 'recommended_init', 'evidence', 'confidence'],
    }, phase: 'Recon' }),

  () => agent(`${BACKGROUND}

YOUR TASK: predict which classes will IMPROVE vs REGRESS when _Rb_tree grows 0x18 -> 0x1c, to set an expected-gain estimate and confirm the lever is net-positive.

Run \`cd ${REPO} && python3 tools/rbtree_blast.py\` to see all 90 map-embedding classes with their match states. Then VERIFY the prediction on the highest-value classes by examining their current objdiff near-misses:
- Multi-map classes with NO compensation pad (e.g. AccomplishmentManager [12 maps, 41 near], SongMgr [6], the 2-map classes) should IMPROVE -- the +4 per map aligns their post-map members. Confirm by checking a couple of their 99.8-99.9% functions show a uniform small offset delta (use: cd ${REPO} && python3 scripts/analysis/diff_inspect.py --symbol "<fn>" --compare-asm --project-dir . , look for [off:+4]/[off:-4] annotations on member accesses past the maps).
- Classes that ALREADY MATCH and have a map followed by a manual pad will REGRESS (need the pad removed later) -- note them.

Report (schema): per high-value class your predicted direction + rough expected_fn_gain + the offset evidence; an overall expectation (net positive? rough magnitude?); confidence.`,
    { label: 'improvement-predict', schema: {
      type: 'object', additionalProperties: false,
      properties: {
        classes: { type: 'array', items: {
          type: 'object', additionalProperties: false,
          properties: {
            cls: { type: 'string' },
            predicted: { type: 'string', enum: ['improves', 'regresses', 'neutral'] },
            expected_fn_gain: { type: 'integer' },
            reason: { type: 'string' },
          }, required: ['cls', 'predicted', 'reason'],
        } },
        overall_expectation: { type: 'string' },
        confidence: { type: 'string', enum: ['high', 'medium', 'low'] },
      },
      required: ['classes', 'overall_expectation', 'confidence'],
    }, phase: 'Recon' }),

  () => agent(`${BACKGROUND}

YOUR TASK: pre-classify the candidate "compensation pads". Run \`cd ${REPO} && python3 tools/rbtree_blast.py --pads-only\` to get 37 candidate members (each an unk*/pad* scalar declared within 8 lines after a map/set member). For EACH, decide whether it is:
- "real_field": a genuine retail field at a documented offset (usually has a \`// 0xNN\` comment matching retail layout, or corresponds to a named field in ../rb3/src). These must be KEPT.
- "compensation": a member that exists ONLY to push later members +4 to compensate our undersized map. Once the tree grows these DOUBLE-count and must be removed. The known example is AccomplishmentProgress::unk50 (a prior session ADDED it as compensation). A compensation pad typically has no meaningful purpose, sits immediately after the LAST map before a real field, and removing it (with tree at 0x1c) restores alignment.
- "uncertain": can't tell from the header alone.

For ambiguous ones, cross-ref the class in ../rb3/src (rb3-Wii named oracle) -- if rb3-Wii has a real field at that position, it's real_field; if the field is absent in Wii and only appears in our 360 header, it's likely compensation.

This is ADVISORY (the build's regression list is the final arbiter) but it pre-stages the unwind list. Report every pad with verdict + reason, plus a one-line summary.`,
    { label: 'pad-classify', schema: {
      type: 'object', additionalProperties: false,
      properties: {
        pads: { type: 'array', items: {
          type: 'object', additionalProperties: false,
          properties: {
            file: { type: 'string' }, line: { type: 'integer' },
            cls: { type: 'string' }, member: { type: 'string' },
            verdict: { type: 'string', enum: ['real_field', 'compensation', 'uncertain'] },
            reason: { type: 'string' },
          }, required: ['cls', 'member', 'verdict', 'reason'],
        } },
        summary: { type: 'string' },
      },
      required: ['pads', 'summary'],
    }, phase: 'Recon' }),
])

return { member, predict, pads }
