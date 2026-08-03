#define RB3_OBJPTR_INLINE_OWNER_CTOR_EH 1
#include "rndobj/MultiMeshProxy.h"
#include "obj/Object.h"
#include "rndobj/Mesh.h"

#ifdef HX_NATIVE
RndMultiMeshProxy::RndMultiMeshProxy() : mMultiMesh(this), mIndex() {}
#else
RndMultiMeshProxy::RndMultiMeshProxy() : mMultiMesh(this), mIndex(0) {}
#endif

BEGIN_HANDLERS(RndMultiMeshProxy)
END_HANDLERS

BEGIN_PROPSYNCS(RndMultiMeshProxy)
END_PROPSYNCS

BEGIN_SAVES(RndMultiMeshProxy)
    MILO_FAIL("Attempting to save a MultiMesh proxy");
END_SAVES

BEGIN_COPYS(RndMultiMeshProxy)
    MILO_FAIL("Attempting to copy a MultiMesh proxy");
END_COPYS

BEGIN_LOADS(RndMultiMeshProxy)
    MILO_FAIL("Attempting to load a MultiMesh proxy");
END_LOADS

void RndMultiMeshProxy::DrawShowing() {
    if (mMultiMesh && mMultiMesh->Mesh()) {
        RndMesh *theMesh = mMultiMesh->Mesh();
        theMesh->SetWorldXfm(mIndex->mXfm);
        // Retail emits a DIRECT `bl RndDrawable::Draw()` here, not a vcall.
        // DC3's copy calls DrawShowing(), which compiles to a 4-instruction
        // virtual dispatch -- exactly the 12B gap (92B retail vs 104B).  See
        // the Draw.h header note: Draw() is non-virtual in retail.  (lane DW-3)
        mMultiMesh->Mesh()->Draw();
    }
}

void RndMultiMeshProxy::UpdatedWorldXfm() {
    if (mMultiMesh) {
        Transform &tfm = mIndex->mXfm;
        tfm = WorldXfm();
    }
}

void RndMultiMeshProxy::SetMultiMesh(
    RndMultiMesh *mesh, const std::list<RndMultiMesh::Instance>::iterator &it
) {
    mMultiMesh = nullptr;
    if (mesh) {
        SetLocalXfm(it->mXfm);
    }
    mMultiMesh = mesh;
    mIndex = it;
}
