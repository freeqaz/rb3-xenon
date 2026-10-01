// x20_bandpatchmesh_link.cpp -- the residual link surface that registering
// OutfitConfig makes live.  NATIVE-ONLY: this file is in native/, so the X360
// match build never sees it and its blast radius there is ZERO BY CONSTRUCTION
// (not "verified zero" -- the file is not in objdiff.json and cannot be scored).
//
// WHY THIS FILE EXISTS
// --------------------
// `OutfitConfig::Init()` (bandobj/OutfitConfig.cpp:404-409) is what retail's
// BandInit() (bandobj/Band.cpp:114) calls to register the OutfitConfig factory.
// Without it the native driver logs `Can't make OutfitConfig` once per
// head/hands/hair/facehair/eyebrows resource milo, no OutfitConfig instance is
// ever built, BandCharacter::SyncOutfitConfig (BandCharacter.cpp:1630) never
// runs, and so OutfitConfig::SetSkinTextures (:1663) never runs -- leaving the
// band's skin materials on their authored dummy_torso/legs/feet.tex
// PLACEHOLDERS.  That is X19 §5's measured chain and it is why the band is pink.
//
// Registering OutfitConfig makes its vtable live, hence its virtual
// SyncProperty, hence its whole property/handler table and the
// ObjVector<BandPatchMesh> it syncs.  X20 MEASURED the resulting bill at
// exactly 48 undefined symbols (evidence/x20-undef-symbols.txt), which
// decomposed as:
//
//   36  namespace-scope `extern Symbol` globals (primary_color, mats, ...).
//       DECLARED in src/system/utl/Symbols{,2,3,4}.h, DEFINED NOWHERE in the
//       tree.  Retired for free by compiling OutfitConfig.cpp with the existing
//       RB3_SYNCPROP_LOCAL_STATIC / RB3_HANDLE_LOCAL_STATIC macro arms -- see
//       native/CMakeLists.txt.  Cost: two compile definitions, no code.
//   11  BandPatchMesh members.  src/system/bandobj/BandPatchMesh.cpp was a
//       191-line PARTIAL port, so X20 supplied nine faithful ports of the
//       ordinary members here.  ⚠ W17-BPM (2026-09-30) MOVED THOSE NINE INTO
//       THE REAL TU -- they are matched to retail bytes there -- and deleted
//       the copies below, so native now links the real ones.  Still here:
//       the two COUNTED stubs (ReProject, PreRender) described next.
//    1  gRB3OutfitComposeActive.  Supplied below.
//
// ⚠ X9's recorded blocker ("BandPatchMesh.cpp is NOT compiled standalone ...
// LightPreset.cpp is not in rb3-render's source list") is STALE.  X20 measured
// 125 BandPatchMesh symbols already defined in rb3-render's link, emitted from
// rndobj/TexBlender.cpp.o: TexBlender.cpp:383 scatter-includes
// AmbientOcclusion.cpp UNCONDITIONALLY, and that chains
// AmbientOcclusion -> PropKeys -> rndobj/Utl -> UIListDir -> LightPreset ->
// BandPatchMesh.  (math/Rot.cpp:431 has the same edge but under
// `#if !HX_NATIVE`, which is the edge X9 was looking at.)  What is missing is
// therefore NOT the TU -- it is the nine ordinary members the partial port
// never wrote, plus the two below that need the projection subsystem.
//
// WHAT THE BODIES ARE
// ------------------------
// The BandPatchMesh bodies of the shared Milo engine
// (system/bandobj/BandPatchMesh.cpp, the same source this repo's own partial
// port in src/system/bandobj compiles).
// Member layout is taken from THIS repo's src/system/bandobj/BandPatchMesh.h,
// which matches the original ctor's initializer list member-for-member
// (mMeshes / mRenderTo / mSrc / mCategory).  Nothing here is invented.
//
// ✅ W17-BPM2 (2026-09-30): the projection subsystem is now ported and the two
// counted stubs below are deleted. The paragraph that follows is the record of
// why they existed.
// ⛔ (historical) TWO FUNCTIONS WERE NOT PORTED, AND THEY WERE COUNTED, NOT SILENT.
// BandPatchMesh::ReProject() and ::PreRender() reach ProjectPatches() ->
// Construct/ConstructQuad/FindXfm/WorkVerts::Project -- the patch PROJECTION
// subsystem, ~570 further lines that the partial port also omits.  Porting it
// is its own lane.  (W17-BPM: PreRender and ConstructQuad ARE now ported in
// BandPatchMesh.cpp, but under `#ifndef HX_NATIVE` -- they call the unported
// projection functions -- so the native build keeps the counted PreRender stub.)  Rather than let a silent no-op make this lane's frame
// partly fictional (the exact failure milo_link_stubs.cpp's header warns about,
// measured in lane CC-5), each keeps a counter that
// Rb3X20ReportBandPatchMeshStubs() prints on EVERY run.  If the printed counts
// are zero, no behaviour was replaced in that run; if they are not, this
// lane's texture result is qualified by exactly that number.

// Must precede ObjMacros.h: the default (`#else`) macro arm compares against
// the undefined `extern Symbol` globals described above.
#define RB3_SYNCPROP_LOCAL_STATIC 1
#define RB3_HANDLE_LOCAL_STATIC 1

#include "bandobj/BandPatchMesh.h"
#include "bandobj/BandCharDesc.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "rndobj/Mat.h"
#include "rndobj/Mesh.h"
#include "rndobj/Tex.h"
#include "utl/BinStream.h"

#include <cstdio>

// ---------------------------------------------------------------------------
// (1) REAL IMPLEMENTATION -- a global the tree only ever DECLARES here.
//
// OutfitConfig.cpp:131 has `extern bool gRB3OutfitComposeActive;` inside
// MatSwap::Compose's ComposeScope RAII guard.  The definition lives in the
// ENGINE, at milo-native-engine/src/platform/RB3Quad.cpp:225 -- but only in the
// engine's `rb3` GPU-backend flavor.  rb3-xenon configures
// MILO_ENGINE_GPU_BACKEND=dc3 (verified in native/build/CMakeCache.txt), whose
// archive has 38 members and no RB3Quad.cpp.o, so the definition is genuinely
// absent from this link.  Nothing in the dc3 backend READS the flag, so storage
// with retail's initial value is the whole of the correct behaviour here.
//
// ⚠ If MILO_ENGINE_GPU_BACKEND is ever set to `rb3`, this becomes a DUPLICATE
// definition and the link fails loudly.  That is the desired failure mode: the
// native gate would catch it in one run.  Do not make it weak to paper over it.
// ---------------------------------------------------------------------------
bool gRB3OutfitComposeActive = false;

// ---------------------------------------------------------------------------
// (2) FAITHFUL PORTS -- moved to src/system/bandobj/BandPatchMesh.cpp (W17-BPM):
// MeshPair::OutputTex, the BandPatchMesh ctors and operator=, PostRender,
// ListDrawChildren, Compress, Render, both operator>>, PropSync(BandPatchMesh&),
// and the rev pair (file-scope statics there, as retail has them).
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// (3) The two COUNTED stubs this file used to carry (ReProject, PreRender) and
// their Rb3X20ReportBandPatchMeshStubs() probe are gone: W17-BPM2 ported the
// patch-projection subsystem (FindXfm / ProjectPatches / Construct and the
// WorkVerts helpers) into src/system/bandobj/BandPatchMesh.cpp, so native links
// the real members.
// ---------------------------------------------------------------------------
