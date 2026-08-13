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
//   11  BandPatchMesh members.  src/system/bandobj/BandPatchMesh.cpp is a
//       191-line PARTIAL port (its own header says so: "Only the worklist
//       target functions and the helpers required to compile + emit them").
//       The class's ordinary members were never ported.  Supplied below.
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
// PROVENANCE OF THE BODIES
// ------------------------
// Ported from the rb3-Wii MWCC decomp oracle,
// /home/free/code/milohax/rb3/src/system/bandobj/BandPatchMesh.cpp (shared Milo
// engine, the same source this repo's own partial port names as its oracle).
// Member layout is taken from THIS repo's src/system/bandobj/BandPatchMesh.h,
// which matches the Wii ctor's initializer list member-for-member
// (mMeshes / mRenderTo / mSrc / mCategory).  Nothing here is invented.
//
// ⛔ TWO FUNCTIONS ARE NOT PORTED, AND THEY ARE COUNTED, NOT SILENT.
// BandPatchMesh::ReProject() and ::PreRender() reach ProjectPatches() ->
// Construct/ConstructQuad/FindXfm/WorkVerts::Project -- the patch PROJECTION
// subsystem, ~570 further lines that the partial port also omits.  Porting it
// is its own lane.  Rather than let a silent no-op make this lane's frame
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

// The partial port DEFINES these two (BandPatchMesh.cpp:183-191, via
// BEGIN_CUSTOM_PROPSYNC) but BandPatchMesh.h DECLARES neither, so they are
// invisible to any other TU. Declared here rather than in the shared header:
// the header is scatter-included into the X360 scoring TU and this lane keeps
// its shared-`src/` surface empty.
bool PropSync(BandPatchMesh::MeshPair &, DataNode &, DataArray *, int, PropOp);
bool PropSync(
    BandPatchMesh::MeshPair::PatchPair &, DataNode &, DataArray *, int, PropOp
);

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
// (2) FAITHFUL PORTS -- nine ordinary BandPatchMesh members.
// ---------------------------------------------------------------------------

// The two class statics BandPatchMesh.h:106-107 declares. Nothing in the tree
// defines them (grep over src/ + native/): the partial port never did, and the
// scatter-include host's `#define gRev gRev_BandPatchMesh` renames a FILE-scope
// gRev, not these. Both are written by operator>> from the stream's rev word
// before any read, so zero-init is the whole of the correct initial state.
unsigned short BandPatchMesh::gRev = 0;
unsigned short BandPatchMesh::gAltRev = 0;

RndTex *BandPatchMesh::MeshPair::OutputTex() const {
    if (mesh && mesh->Mat())
        return mesh->Mat()->GetDiffuseTex();
    else
        return 0;
}

BandPatchMesh::BandPatchMesh(Hmx::Object *o)
    : mMeshes(o), mRenderTo(true), mSrc(o, 0), mCategory(0) {}

BandPatchMesh::BandPatchMesh(const BandPatchMesh &mesh)
    : mMeshes(mesh.mMeshes), mRenderTo(mesh.mRenderTo), mSrc(mesh.mSrc),
      mCategory(mesh.mCategory) {}

BandPatchMesh &BandPatchMesh::operator=(const BandPatchMesh &mesh) {
    mSrc = mesh.mSrc;
    mMeshes = mesh.mMeshes;
    mRenderTo = mesh.mRenderTo;
    mCategory = mesh.mCategory;
    return *this;
}

void BandPatchMesh::PostRender() {
    for (ObjVector<MeshPair>::iterator mp = mMeshes.begin(); mp != mMeshes.end(); ++mp) {
        for (ObjVector<MeshPair::PatchPair>::iterator pp = mp->patches.begin();
             pp != mp->patches.end();
             ++pp) {
            RndMesh *patch = pp->mPatch;
            if (patch && !patch->Dir()) {
                delete patch;
            }
        }
        mp->patches.clear();
    }
}

void BandPatchMesh::ListDrawChildren(std::list<RndDrawable *> &list) {
    if (mRenderTo) {
        for (int i = 0; i < mMeshes.size(); i++) {
            for (int j = 0; j < mMeshes[i].patches.size(); j++) {
                list.push_back(mMeshes[i].patches[j].mPatch);
            }
        }
    }
}

void BandPatchMesh::Compress(BandCharDesc *desc) {
    ObjectDir *pdir = desc->GetPatchDir();
    for (int i = 0; i < mMeshes.size(); i++) {
        for (int j = 0; j < mMeshes[i].patches.size(); j++) {
            RndMesh *patch = mMeshes[i].patches[j].mPatch;
            if (patch) {
                RndTex *tex = mMeshes[i].patches[j].mTex;
                if (tex && pdir && tex->Dir() == pdir) {
                    delete tex;
                }
                if (!patch->Dir())
                    delete patch;
            }
        }
    }
}

void BandPatchMesh::Render(RndTex *tex, RndMat *mat) {
    for (int i = 0; i < mMeshes.size(); i++) {
        RndTex *outputtex = mMeshes[i].OutputTex();
        if (outputtex == tex) {
            for (int j = 0; j < mMeshes[i].patches.size(); j++) {
                BandPatchMesh::MeshPair::PatchPair &ppair = mMeshes[i].patches[j];
                RndMesh *patch = ppair.mPatch;
                if (patch) {
                    RndMat *patchmat = patch->Mat();
                    if (patchmat) {
                        // Wii oracle reads `patchmat->mColor` directly; that
                        // member is protected on X360's RndMat, and
                        // GetColor() (BaseMaterial.h:203) returns exactly it.
                        mat->SetColor(patchmat->GetColor());
                        mat->SetTexWrap(patchmat->GetTexWrap());
                        mat->SetBlend(patchmat->GetBlend());
                        mat->SetDiffuseTex(patchmat->GetDiffuseTex());
                    } else {
                        mat->SetColor(1, 1, 1);
                        mat->SetTexWrap(kTexBorderBlack);
                        mat->SetBlend(RndMat::kPreMultAlpha);
                        mat->SetDiffuseTex(mMeshes[i].patches[j].mTex);
                    }
                    Transform tf88;
                    tf88.Reset();
                    tf88.m.y *= (float)tex->Height() / (float)tex->Width();
                    patch->SetLocalXfm(tf88);
                    patch->SetMat(mat);
                    if (mat->GetDiffuseTex())
                        patch->DrawShowing();
                    patch->SetMat(patchmat);
                    patch->DirtyLocalXfm().Reset();
                }
            }
        }
    }
}

BinStream &operator>>(BinStream &bs, BandPatchMesh::MeshPair &mp) {
    bs >> mp.mesh;
    return bs;
}

BinStream &operator>>(BinStream &bs, BandPatchMesh &mesh) {
    int rev;
    bs >> rev;
    BandPatchMesh::gRev = getHmxRev(rev);
    BandPatchMesh::gAltRev = getAltRev(rev);
    bs >> mesh.mSrc;
    if (BandPatchMesh::gRev > 3)
        bs >> mesh.mMeshes;
    else {
        mesh.mMeshes.resize(1);
        bs >> mesh.mMeshes[0].mesh;
    }
    if (BandPatchMesh::gRev < 1) {
        Symbol s;
        bs >> s;
    }
    if (BandPatchMesh::gRev < 4) {
        Symbol s;
        bs >> s;
    }
    if (BandPatchMesh::gRev > 1) {
        if (BandPatchMesh::gRev > 2)
            bs >> mesh.mRenderTo;
        else {
            Symbol s;
            bs >> s;
            mesh.mRenderTo = !s.Null();
        }
    }
    if (BandPatchMesh::gRev > 3)
        bs >> mesh.mCategory;
    return bs;
}

BEGIN_CUSTOM_PROPSYNC(BandPatchMesh)
    SYNC_PROP(meshes, o.mMeshes)
    SYNC_PROP(src, o.mSrc)
    SYNC_PROP(render_to, o.mRenderTo)
    SYNC_PROP(category, o.mCategory)
END_CUSTOM_PROPSYNC

// ---------------------------------------------------------------------------
// (3) ⛔ NOT PORTED -- counted, never silent.  See the header note.
// ---------------------------------------------------------------------------
static long gX20ReProjectHits = 0;
static long gX20PreRenderHits = 0;

bool BandPatchMesh::ReProject() {
    ++gX20ReProjectHits;
    return mRenderTo;  // the ported tail of the real body; the ProjectPatches
                       // call it wraps is what is missing.
}

void BandPatchMesh::PreRender(BandCharDesc *, int) { ++gX20PreRenderHits; }

// Printed on EVERY run by main_render.cpp, so "these stubs were not reached"
// is a MEASUREMENT in each frame's own log rather than a claim in a comment.
extern "C" void Rb3X20ReportBandPatchMeshStubs() {
    std::printf(
        "X20 BANDPATCHMESH STUB PROBE: ReProject hits=%ld, PreRender hits=%ld%s\n",
        gX20ReProjectHits,
        gX20PreRenderHits,
        (gX20ReProjectHits == 0 && gX20PreRenderHits == 0)
            ? "  => NO unported patch-projection behaviour was exercised"
            : "  => ⛔ UNPORTED BEHAVIOUR WAS REACHED; results are qualified"
    );
}
