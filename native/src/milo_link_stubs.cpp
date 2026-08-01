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
// Explicitly instantiated for exactly the two Ts the link asks for.
// char/CharClipGroup.cpp saves ObjOwnerPtr<CharClip> vectors and
// char/Waypoint.cpp saves ObjOwnerPtr<Waypoint>; neither runs during a load.
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
