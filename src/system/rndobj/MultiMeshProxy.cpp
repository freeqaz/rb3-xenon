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
        mMultiMesh->Mesh()->DrawShowing();
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
