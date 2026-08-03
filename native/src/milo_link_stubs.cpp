// milo_link_stubs.cpp -- the residual link surface of rb3-milo (X2).
//
// HOW TO READ THIS FILE
// ---------------------
// It has THREE sections, and the distinction is the whole point:
//
//   (1) REAL IMPLEMENTATIONS. Genuine behaviour that happens to have no home in
//       the compiled fork -- a virtual the decomp never got to, a global the
//       tree only ever declares. These are not stubs and are not owed anything.
//
//   (2) GPU / RENDER BACKEND. Everything here lives in src/system/rnddx9/ on
//       X360 (Direct3D 9), which the port replaces wholesale -- CLAUDE.md's
//       scope list names rnddx9/rndwii as "replaced by an OpenGL/Vulkan/Metal
//       backend". X2 loads and does not draw, so a no-op is the CORRECT
//       behaviour here, not a placeholder for it.
//
//       ★ X3 UPDATE -- MOST of this section is now retired by
//       `-DRB3_ENGINE_RENDER=1` (rb3-render links libmilo-engine.a, which
//       supplies the real bodies), but NOT all of it, and the exception is a
//       correction to X2's own mapping table:
//
//         retired    Rnd_Wgpu.cpp        TheNgRnd, TheShaderMgr, TheUI,
//                                        FlushPostProcessingForOverlay
//         retired    Tex_Wgpu.cpp        RndTex::{Sync,Presync}Bitmap,
//                                        {Make,Finish}DrawTarget
//         retired    Mesh_Wgpu.cpp       RndMesh::DrawShowing
//         retired    MeshGpuCache.cpp    RndMesh::OnSync, CleanupGpuMesh
//         retired    Part_Wgpu.cpp       DrawParticlesBillboard
//         retired    TransparentQueue.cpp FlushTransparentDraws
//         ⛔ KEPT    RenderState_Native.cpp is NOT IN THE ENGINE'S SOURCE LIST.
//                    milo-native-engine/CMakeLists.txt:361-370 defers it (with
//                    Skeleton_Native.cpp, PlatformMgr_Native.cpp, HttpServer,
//                    DebugPanel) as DC3 glue, because rnddx9/RenderState.h
//                    pulls xdk/D3D9.h. So TheRenderState and the 17
//                    RndRenderState::Set* bodies stay stubbed even in the
//                    render target -- and that is CORRECT, not a gap: the
//                    WebGPU backend carries its own pipeline state and never
//                    reads a D3D9 device state. X2's table listed this row as
//                    engine-supplied; it was wrong.
//         ⛔ KEPT    SpotlightDrawer::DeSelect, RndFont::CellDiff -- no engine
//                    counterpart exists; still leaves.
//
//   (3) OFF-PATH SINGLETONS AND HANDLER-ONLY SYMBOLS. Reached only from
//       BEGIN_HANDLERS blocks, editor paths, or subsystems X2 does not stand up
//       (Ham/dance, UI manager). Null/default, same convention as
//       m6_symbols.cpp / m8_link_stubs.cpp.
//
// ⚠ THE HAZARD THIS FILE CREATES, STATED PLAINLY. A stub that is actually
// REACHED silently replaces real behaviour with nothing, and the run then
// reports partly-fictional results -- this repo has already measured that exact
// failure (a no-op CrowdRating::Poll running 25,905 times in one rb3-score4 run
// while the run reported its numbers as real, lane CC-5). The instrument for it
// exists: configure with -DRB3_STUB_PROBE=ON, see native/src/cc5_stub_probe.c.
// Section (2) is EXPECTED to be reached (a milo full of RndMesh/RndTex will ask
// the GPU to sync); sections (1) and (3) are not, and a hit there is a bug.

#include "obj/Object.h"
#include "os/Debug.h"
#include "utl/Symbol.h"
#include "obj/Msg.h"  // X7: Message get_customize_slot_msg

// ===========================================================================
// (1) REAL IMPLEMENTATIONS
// ===========================================================================

#include "world/Instance.h"

// WorldInstance::Load is DECLARED (world/Instance.h:27) and defined nowhere in
// the tree -- Instance.cpp has PreLoad and PostLoad but no BEGIN_LOADS block,
// so the vtable slot dangles. A genuine decomp gap, not a platform gap: Milo's
// BEGIN_LOADS/END_LOADS macro expands to exactly this body and every sibling
// class in rndobj/ and world/ has it. Reproducing the macro expansion is the
// real implementation, not an approximation -- and it matters, because
// WorldInstance is on the load path for venue milos.
void WorldInstance::Load(BinStream &bs) {
    PreLoad(bs);
    PostLoad(bs);
}

// rndobj/Rnd.cpp:110 declares `extern int lbl_82F14008` — an unhomed retail
// DATA label, not a function gap — and no TU defines it. It is the heap-overlay
// cursor: Rnd::OnToggleHeap (:1286-1288) increments it and wraps to -1 past
// MemNumHeaps(), and Rnd::UpdateHeap (:1379-1387) reads -1 as "show every heap"
// and any other value as a heap index. Retail's copy lives in .bss, so
// ZERO-INITIALISED is the faithful state (overlay starts on heap 0), and a
// plain tentative definition is exactly that — deliberately NOT `= -1`, which
// would silently start the overlay in a different mode from retail.
//
// Reached only from Rnd::PreInit's overlay wiring in the render target;
// --gc-sections drops it in rb3-milo.
int lbl_82F14008;

// char/Char.h:10 declares `extern float gCharHighlightY` and no TU defines it.
// Char.cpp:146/168-169 uses -1 as the "nothing highlighted" sentinel (it reads
// the value then resets it to -1) and CharIKMidi.cpp:172 tests `== -1.0f`, so
// -1 is the correct initial state: a zero-initialised definition would read as
// "highlight at y = 0" rather than "no highlight".
float gCharHighlightY = -1.0f;

#include "char/CharClip.h"
#include "char/Waypoint.h"
#include "rndobj/EnvAnim.h"

// `operator<<(BinStream&, const ObjOwnerPtr<T>&)` is DECLARED (obj/Object.h:755)
// and its definition is COMMENTED OUT in obj/ObjPtr_p.h:394 -- and it is
// commented out in DC3's copy of the same header too
// (dc3-decomp/src/system/obj/ObjPtr_p.h:206), so this is a shared undecompiled
// function, not a xenon regression. Only the SAVE direction is missing; the
// `>>` twin right below it is complete.
//
// The body is not guesswork: every sibling container serializer in that header
// writes an object reference as its NAME, empty string for null
// (ObjPtrVec at :589-597, ObjPtrList at :936-944), and the `>>` twin reads it
// back with ptr.Load(bs, true, nullptr). So this is the Milo convention
// reconstructed, not invented.
//
// Explicitly instantiated for exactly the Ts the link asks for.
// char/CharClipGroup.cpp saves ObjOwnerPtr<CharClip> vectors and
// char/Waypoint.cpp saves ObjOwnerPtr<Waypoint>; neither runs during a load.
//
// ⚠ X4a: RndEnvAnim was ADDED HERE, and the way it arrived is the finding.
// `81d23046` (lane DD-2, "SAVE_OBJ stubs") replaced RndEnvAnim::Save's
// MILO_ASSERT stub with a real body whose `bs << mKeysOwner` is the tree's
// third ObjOwnerPtr save site -- and it landed on main having broken
// rb3-milo AND rb3-render's link, undetected. That is EXACTLY the defect class
// tools/native_build_gate.sh exists for (its header already records `b2958f2d`
// doing the same thing), so the gate is not at fault; it was not run.
//
// ⛔ THIS LIST IS A RECURRENCE TRAP, and it will fire again. The declaration in
// obj/Object.h:760-761 is an EXACT match for an ObjOwnerPtr argument, so it beats
// the base-class `operator<<(BinStream&, const ObjRefConcrete<T1,ObjectDir>&)`
// (ObjPtr_p.h:156, which IS defined) in overload resolution. Every new
// `bs << someObjOwnerPtr` therefore compiles clean everywhere and fails only at
// NATIVE LINK -- invisible to the X360 build, which never links. The permanent
// fix is to move this definition into ObjPtr_p.h under `#ifdef HX_NATIVE` so
// implicit instantiation covers every future T; it is deliberately NOT done here
// because a shared-header edit is a wider blast radius than this lane's charter,
// and it is recorded as owed work in docs/plans/x4a-venue-render-2026-08-02.md.
template <class T1>
BinStream &operator<<(BinStream &bs, const ObjOwnerPtr<T1> &ptr) {
    if (ptr.Ptr())
        bs << ptr.Ptr()->Name();
    else
        bs << "";
    return bs;
}
template BinStream &operator<< <CharClip>(BinStream &, const ObjOwnerPtr<CharClip> &);
template BinStream &operator<< <Waypoint>(BinStream &, const ObjOwnerPtr<Waypoint> &);
template BinStream &operator<< <RndEnvAnim>(BinStream &, const ObjOwnerPtr<RndEnvAnim> &);
// ★ X7: AND IT FIRED AGAIN, exactly as the note above predicts it would.
// bandobj/BandCharacter.cpp saves an ObjOwnerPtr<BandCharDesc>; compiled clean
// everywhere and failed only at native link. That is now FOUR Ts added
// reactively (CharClip, Waypoint, RndEnvAnim, BandCharDesc) — the fifth should
// be the header fix, not another line here.
#include "bandobj/BandCharDesc.h"
template BinStream &operator<< <BandCharDesc>(BinStream &, const ObjOwnerPtr<BandCharDesc> &);

// ★ X11: Hmx::Matrix4::Col3 — the ONE symbol that made X10's Direction-B
// D3D9 row load-bearing, and the reason closing it is not a free tidy-up.
//
// It is declared in math/Mtx.h:128 (a MATH header) but DEFINED at
// src/system/rnddx9/Cam.cpp:13 — parked at the top of the Direct3D9 camera TU,
// above DxCam::DxCam(). rndobj/Lit_NG.cpp, which IS a native target source,
// calls it from Hmx::operator*(const Transform &, const Hmx::Matrix4 &). So
// guarding the rnddx9 scatter edge in rndobj/CubeTex.cpp — which is otherwise
// dead weight, 105 `Dx*::` symbols that --gc-sections already strips — took
// this one math accessor out with it and broke the link with EXACTLY ONE
// undefined reference. Measured, not predicted: the guard was applied, the
// link failed, and `undefined reference to` yielded a set of size 1.
//
// This body is COPIED VERBATIM from rnddx9/Cam.cpp:13-15, not reconstructed.
// There is no ODR conflict: natively that TU is now excluded, and this file is
// never compiled for X360.
//
// ⚠ The right long-term home is math/Mtx.cpp beside its declaration, but MOVING
// it is a shared-`src/` change that relocates a symbol out of rnddx9/Cam.cpp's
// COMDAT and is therefore match-relevant — it needs its own A/B and it is NOT
// done here. Filed in the handoff table.
#include "math/Mtx.h"
Vector3 Hmx::Matrix4::Col3(int col) const {
    return Vector3(x[col], y[col], z[col]);
}

// ===========================================================================
// (2) GPU / RENDER BACKEND — X360 src/system/rnddx9/, deleted by X3
// ===========================================================================

#include "rnddx9/RenderState.h"
#include "rndobj/Font.h"
#include "rndobj/Mesh.h"
#include "rndobj/Part.h"
#include "rndobj/ShaderMgr.h"
#include "rndobj/Rnd_NG.h"
#include "rndobj/Tex.h"
#include "world/SpotlightDrawer.h"

// --- render state (rnddx9/RenderState.cpp; X3: engine RenderState_Native.cpp)
// Each is a thin setter over one D3D9 device state. rndobj's material / mesh /
// postproc code calls them freely during Draw; nothing on the LOAD path does.
RndRenderState TheRenderState;
void RndRenderState::SetBlendEnable(bool) {}
void RndRenderState::SetBlendOp(BlendOp) {}
void RndRenderState::SetBlend(Blend, Blend, Blend, Blend) {}
void RndRenderState::SetColorWriteMask(uint) {}
void RndRenderState::SetTextureFilter(uint, FilterMode, bool) {}
void RndRenderState::SetTextureClamp(uint, ClampMode) {}
void RndRenderState::SetBorderColor(uint, bool) {}
void RndRenderState::SetFillMode(FillMode) {}
void RndRenderState::SetCullMode(CullMode) {}
void RndRenderState::SetAlphaTestEnable(bool) {}
void RndRenderState::SetAlphaFunc(TestFunc, uint) {}
void RndRenderState::SetDepthTestEnable(bool) {}
void RndRenderState::SetDepthWriteEnable(bool) {}
void RndRenderState::SetDepthFunc(TestFunc) {}
void RndRenderState::SetStencilTestEnable(bool) {}
void RndRenderState::SetStencilFunc(TestFunc, u8) {}
void RndRenderState::SetStencilOp(StencilOp, StencilOp, StencilOp) {}

#ifndef RB3_ENGINE_RENDER
// --- the two renderer singletons (rnddx9/Rnd.cpp + ShaderMgr.cpp;
//     X3: engine Rnd_Wgpu.cpp defines both as references to gWgpuRndInstance)
//
// ★ THESE ARE THE ONLY TWO ENTRIES IN THIS FILE THAT ARE UNSAFE TO USE, and
// they are deliberately left that way. Both are declared as REFERENCES
// (rndobj/Rnd_NG.h:82 `extern NgRnd &TheNgRnd`, rndobj/ShaderMgr.h:202
// `extern RndShaderMgr &TheShaderMgr`), so the link demands a definition even
// though X2 has no renderer to point them at. Binding them to null means any
// use faults immediately at address 0, with a stack trace naming the caller --
// which is precisely what should happen if the load path ever reaches the
// renderer. Pointing them at a zeroed buffer instead would let the caller
// wander on through garbage members: the same silent-fiction class the header
// note above warns about. Loud beats convenient.
static NgRnd *const kNoRenderer = nullptr;
static RndShaderMgr *const kNoShaderMgr = nullptr;
NgRnd &TheNgRnd = *kNoRenderer;
RndShaderMgr &TheShaderMgr = *kNoShaderMgr;

// --- texture GPU residency (rnddx9/Tex.cpp; X3: engine Tex_Wgpu.cpp)
// RndTex::PostLoad calls SyncBitmap() after it has read the bitmap, so this IS
// reached on the load path -- and no-op is right: the pixels are in
// RndTex::mBitmap either way, they simply never reach a GPU that is not there.
// The census counts objects, not uploads.
void RndTex::SyncBitmap() {}
void RndTex::PresyncBitmap() {}
void RndTex::MakeDrawTarget() {}
void RndTex::FinishDrawTarget() {}

// --- mesh + particle GPU paths (rnddx9/Mesh.cpp, rnddx9/Part.cpp;
//     X3: engine Mesh_Wgpu.cpp / Part_Wgpu.cpp / MeshGpuCache.cpp)
// CleanupGpuMesh runs from RndMesh's destructor (rndobj/Mesh.cpp:131), so it
// DOES execute in X2 -- releasing a GPU resource that was never created.
void RndMesh::DrawShowing() {}
void RndMesh::OnSync(int) {}
void CleanupGpuMesh(RndMesh *) {}
void DrawParticlesBillboard(RndParticleSys *) {}

// --- draw-order flushes (declared locally by their callers:
//     meta/MoviePanel.cpp:29, ui/PanelDir.cpp:23)
void FlushPostProcessingForOverlay() {}
void FlushTransparentDraws() {}
#endif // !RB3_ENGINE_RENDER

// --- misc render leaves
void SpotlightDrawer::DeSelect() {}

// rndobj/Font.h:92 -- inter-cell metric for the DX9 glyph-cell layout, used by
// FontBase's text measurement. 0 is the identity ("no gap") for a layout that
// is never rasterised here.
float RndFont::CellDiff() const { return 0.0f; }

// ===========================================================================
// (3) OFF-PATH SINGLETONS AND HANDLER-ONLY SYMBOLS
// ===========================================================================

#include "hamobj/HamDirector.h"
#include "hamobj/HamWardrobe.h"
#include "ui/UI.h"

// Ham (the Dance Central lineage) is not part of RB3's runtime at all. These
// are reachable only from world/Crowd.cpp's 3D-crowd handlers, which want a
// HamWardrobe that RB3 never constructs.
HamDirector *TheHamDirector;
HamWardrobe *TheHamWardrobe;
void HamWardrobe::ForceCrowdAnimationStart(Symbol) {}
void HamWardrobe::ForceCrowdAnimationEnd() {}

// ui/UI.h:159. The UIManager singleton is created by App boot, which X2 does
// not run. ui/ IS compiled (38/38 clean), so only the INSTANCE is missing, not
// the class -- PanelDir and the UI object classes are real here, which is what
// keeps PanelDir off the unregistered-*Dir desync path.
// ⚠ The engine defines this too (Rnd_Wgpu.cpp:79 `UIManager* TheUI = nullptr;`),
// which is a duplicate-definition collision the basename diff does NOT show --
// so it is guarded on the same macro as section (2).
#ifndef RB3_ENGINE_RENDER
UIManager *TheUI;
#endif

// --- Symbol globals referenced only from BEGIN_HANDLERS / SYNC_PROP blocks.
// Same convention and rationale as native/src/m6_symbols.cpp: declared in
// utl/Symbols*.h, interned at App boot, never reached on the load path.
// Default-constructed rather than interned, so nothing here depends on
// Symbol::Init ordering.
Symbol dir;
Symbol event;
Symbol events;
Symbol keys;
Symbol none;
Symbol reset_on_end;
Symbol start;
// X7: BEGIN_HANDLERS(BandConfiguration) -- bandobj/BandConfiguration.cpp:130-137.
// Same three-line convention as the block above. They are dispatch keys only:
// SyncPlayMode is reached natively by a DIRECT call from the driver, not
// through Handle(), so none of these needs to be interned for placement to
// work. Defined so the TU links.
Symbol release_configuration;
Symbol store_configuration;
Symbol sync_play_mode;

// ---- X7: Symbol globals pulled in by the bandobj band-member TUs
// (BandCharacter / BandCharDesc / OutfitConfig / BandHeadShaper and their
// BEGIN_HANDLERS + SYNC_PROP blocks). Same convention as the block above:
// declared in utl/Symbols*.h, interned at App boot, default-constructed here
// so nothing depends on Symbol::Init ordering. All 129 verified present as
// `extern Symbol X;' in utl/Symbols*.h -- none is invented.
Symbol bass;
Symbol brow_height;
Symbol brow_separation;
Symbol cam_teleport;
Symbol category;
Symbol change_face_group;
Symbol chars_dir;
Symbol chin;
Symbol chin_height;
Symbol chin_num;
Symbol chin_width;
Symbol clear_group;
Symbol closet_teleport;
Symbol color0;
Symbol color1;
Symbol color2;
Symbol coop_bk;
Symbol coop_gk;
Symbol copy_prefab;
Symbol drum;
Symbol drum_venue;
Symbol earrings;
Symbol enable_debug_interests;
Symbol enter_closet;
Symbol enter_venue;
Symbol enter_vignette;
Symbol extras;
Symbol eye;
Symbol eyebrows;
Symbol eye_color;
Symbol eye_height;
Symbol eye_num;
Symbol eye_rotation;
Symbol eyes;
Symbol eye_separation;
Symbol facehair;
Symbol feet;
Symbol find_target;
Symbol flag_string;
Symbol force_vertical;
Symbol game_over;
Symbol gender;
Symbol genre;
Symbol get_character;
Symbol get_matching_dude;
Symbol get_play_flags;
Symbol glasses;
Symbol group_override;
Symbol guitar;
Symbol hair;
Symbol hands;
Symbol head;
Symbol head_lookat_weight;
Symbol heads;
Symbol height;
Symbol hide;
Symbol hide_categories;
Symbol in_closet;
Symbol install_filter;
Symbol instruments;
Symbol instrument_type;
Symbol in_tour_ending;
Symbol is_loading;
Symbol jaw_height;
Symbol jaw_width;
Symbol keyboard;
Symbol legs;
Symbol list_dircuts;
Symbol list_drum_venues;
Symbol list_interest_objects;
Symbol list_outfits;
Symbol list_venue_anim_groups;
Symbol load_dircut;
Symbol load_prefab_prefs;
Symbol mesh_name;
Symbol mic;
Symbol milo_reload;
Symbol mouth;
Symbol mouth_height;
Symbol mouth_num;
Symbol mouth_width;
Symbol muscle;
Symbol name;
Symbol nose;
Symbol nose_height;
Symbol nose_num;
Symbol nose_width;
Symbol on_extra_loaded;
Symbol on_post_merge;
Symbol outfit;
Symbol patches;
Symbol piercings;
Symbol play_group;
Symbol portrait_begin;
Symbol portrait_end;
Symbol pre_clear;
Symbol prefab;
Symbol prefabs_list;
Symbol proxies;
Symbol restore_categories;
Symbol rings;
Symbol rotation;
Symbol save_from_closet;
Symbol save_prefab;
Symbol scale;
Symbol select_extras;
Symbol set_context;
Symbol set_file_merger;
Symbol set_play;
Symbol set_singalong;
Symbol shape;
Symbol shape_num;
Symbol skin;
Symbol skin_color;
Symbol sort_targets;
Symbol start_load;
Symbol start_venue_shot;
Symbol sync_interests;
Symbol tempo;
Symbol test_prefab;
Symbol test_tour_ending_venue;
Symbol texture;
Symbol toggle_interests_overlay;
Symbol torso;
Symbol unload_venue;
Symbol use_mic_stand_clips;
Symbol uv;
Symbol weight;
Symbol wrist;

// X7: utl/Messages.h:76 declares this as a Message (not a Symbol) and
// BandWardrobe.cpp:1053 HandleType()s it. Message has no default ctor that is
// safe pre-Symbol::Init, so it is constructed from a Symbol the same way the
// engine does; it is never dispatched on the load path.
Message get_customize_slot_msg(Symbol(), DataNode(0));


// ══════════════════════════════════════════════════════════════════════════
// ⛔ X8 DEFECT FIX -- THESE 139 GLOBALS WERE DEAD DISPATCH KEYS.
//
// Every `Symbol foo;` above is DEFAULT-CONSTRUCTED, i.e. the NULL symbol
// (Symbol.h:16, mStr = gNullStr). The comment they were added under says they
// are "dispatch keys only ... never reached on the load path". That is false,
// and it cost this lane a whole debugging pass:
//
//   obj/ObjMacros.h:184  #define HANDLE_ACTION(symbol, action)
//                            if (sym == symbol) { (action); return 0; }
//
// -- this arm of the macro compares the incoming message symbol against THE
// GLOBAL ITSELF, not against a string literal. A null global therefore matches
// NOTHING, and every BEGIN_HANDLERS entry keyed on one silently falls through
// to HANDLE_CHECK and reports "unhandled msg".
//
// MEASURED: BandWardrobe::SetVenueDir -> SyncPlayMode() ->
// mModeSink->Handle(sync_play_mode_msg) produced
//   "BandConfiguration (...small_club_01_base.milo) unhandled msg: sync_play_mode"
// so the venue's authored band-slot transforms were never applied and all four
// members stayed at char/main/main.milo's defaults, with rc=0 and no warning.
// A dead dispatch key fails SILENTLY -- it is indistinguishable from "the
// message was never sent".
//
// retail defines these in src/system/utl/Symbols*.cpp as
//     Symbol sync_play_mode("sync_play_mode");
// (rb3-Wii oracle rb3/src/system/utl/Symbols.cpp:960). rb3-xenon ships the
// Symbols*.h HEADERS but no corresponding .cpp, which is why they had to be
// hand-defined here at all.
//
// Interned in a FUNCTION rather than at static-init, deliberately: the Symbol
// ctor (utl/Symbol.cpp) dereferences gStringTable, which does not exist until
// Symbol::Init() -> PreInit(). Static-init construction would be a null deref
// or an ordering lottery. Call this AFTER Symbol::Init().
// ══════════════════════════════════════════════════════════════════════════
void InternSymbolGlobals_MiloLinkStubs() {
    dir = Symbol("dir");
    event = Symbol("event");
    events = Symbol("events");
    keys = Symbol("keys");
    none = Symbol("none");
    reset_on_end = Symbol("reset_on_end");
    start = Symbol("start");
    release_configuration = Symbol("release_configuration");
    store_configuration = Symbol("store_configuration");
    sync_play_mode = Symbol("sync_play_mode");
    bass = Symbol("bass");
    brow_height = Symbol("brow_height");
    brow_separation = Symbol("brow_separation");
    cam_teleport = Symbol("cam_teleport");
    category = Symbol("category");
    change_face_group = Symbol("change_face_group");
    chars_dir = Symbol("chars_dir");
    chin = Symbol("chin");
    chin_height = Symbol("chin_height");
    chin_num = Symbol("chin_num");
    chin_width = Symbol("chin_width");
    clear_group = Symbol("clear_group");
    closet_teleport = Symbol("closet_teleport");
    color0 = Symbol("color0");
    color1 = Symbol("color1");
    color2 = Symbol("color2");
    coop_bk = Symbol("coop_bk");
    coop_gk = Symbol("coop_gk");
    copy_prefab = Symbol("copy_prefab");
    drum = Symbol("drum");
    drum_venue = Symbol("drum_venue");
    earrings = Symbol("earrings");
    enable_debug_interests = Symbol("enable_debug_interests");
    enter_closet = Symbol("enter_closet");
    enter_venue = Symbol("enter_venue");
    enter_vignette = Symbol("enter_vignette");
    extras = Symbol("extras");
    eye = Symbol("eye");
    eyebrows = Symbol("eyebrows");
    eye_color = Symbol("eye_color");
    eye_height = Symbol("eye_height");
    eye_num = Symbol("eye_num");
    eye_rotation = Symbol("eye_rotation");
    eyes = Symbol("eyes");
    eye_separation = Symbol("eye_separation");
    facehair = Symbol("facehair");
    feet = Symbol("feet");
    find_target = Symbol("find_target");
    flag_string = Symbol("flag_string");
    force_vertical = Symbol("force_vertical");
    game_over = Symbol("game_over");
    gender = Symbol("gender");
    genre = Symbol("genre");
    get_character = Symbol("get_character");
    get_matching_dude = Symbol("get_matching_dude");
    get_play_flags = Symbol("get_play_flags");
    glasses = Symbol("glasses");
    group_override = Symbol("group_override");
    guitar = Symbol("guitar");
    hair = Symbol("hair");
    hands = Symbol("hands");
    head = Symbol("head");
    head_lookat_weight = Symbol("head_lookat_weight");
    heads = Symbol("heads");
    height = Symbol("height");
    hide = Symbol("hide");
    hide_categories = Symbol("hide_categories");
    in_closet = Symbol("in_closet");
    install_filter = Symbol("install_filter");
    instruments = Symbol("instruments");
    instrument_type = Symbol("instrument_type");
    in_tour_ending = Symbol("in_tour_ending");
    is_loading = Symbol("is_loading");
    jaw_height = Symbol("jaw_height");
    jaw_width = Symbol("jaw_width");
    keyboard = Symbol("keyboard");
    legs = Symbol("legs");
    list_dircuts = Symbol("list_dircuts");
    list_drum_venues = Symbol("list_drum_venues");
    list_interest_objects = Symbol("list_interest_objects");
    list_outfits = Symbol("list_outfits");
    list_venue_anim_groups = Symbol("list_venue_anim_groups");
    load_dircut = Symbol("load_dircut");
    load_prefab_prefs = Symbol("load_prefab_prefs");
    mesh_name = Symbol("mesh_name");
    mic = Symbol("mic");
    milo_reload = Symbol("milo_reload");
    mouth = Symbol("mouth");
    mouth_height = Symbol("mouth_height");
    mouth_num = Symbol("mouth_num");
    mouth_width = Symbol("mouth_width");
    muscle = Symbol("muscle");
    name = Symbol("name");
    nose = Symbol("nose");
    nose_height = Symbol("nose_height");
    nose_num = Symbol("nose_num");
    nose_width = Symbol("nose_width");
    on_extra_loaded = Symbol("on_extra_loaded");
    on_post_merge = Symbol("on_post_merge");
    outfit = Symbol("outfit");
    patches = Symbol("patches");
    piercings = Symbol("piercings");
    play_group = Symbol("play_group");
    portrait_begin = Symbol("portrait_begin");
    portrait_end = Symbol("portrait_end");
    pre_clear = Symbol("pre_clear");
    prefab = Symbol("prefab");
    prefabs_list = Symbol("prefabs_list");
    proxies = Symbol("proxies");
    restore_categories = Symbol("restore_categories");
    rings = Symbol("rings");
    rotation = Symbol("rotation");
    save_from_closet = Symbol("save_from_closet");
    save_prefab = Symbol("save_prefab");
    scale = Symbol("scale");
    select_extras = Symbol("select_extras");
    set_context = Symbol("set_context");
    set_file_merger = Symbol("set_file_merger");
    set_play = Symbol("set_play");
    set_singalong = Symbol("set_singalong");
    shape = Symbol("shape");
    shape_num = Symbol("shape_num");
    skin = Symbol("skin");
    skin_color = Symbol("skin_color");
    sort_targets = Symbol("sort_targets");
    start_load = Symbol("start_load");
    start_venue_shot = Symbol("start_venue_shot");
    sync_interests = Symbol("sync_interests");
    tempo = Symbol("tempo");
    test_prefab = Symbol("test_prefab");
    test_tour_ending_venue = Symbol("test_tour_ending_venue");
    texture = Symbol("texture");
    toggle_interests_overlay = Symbol("toggle_interests_overlay");
    torso = Symbol("torso");
    unload_venue = Symbol("unload_venue");
    use_mic_stand_clips = Symbol("use_mic_stand_clips");
    uv = Symbol("uv");
    weight = Symbol("weight");
    wrist = Symbol("wrist");
}
