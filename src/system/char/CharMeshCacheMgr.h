#pragma once
#include "rndobj/Mesh.h"
#include "utl/MemMgr.h"
#include <vector>

// size: 0x24
class MeshCacher {
public:
    MeshCacher(RndMesh *, bool);
    ~MeshCacher() {
        if (mMesh->GetKeepMeshData()) {
            SyncMesh();
#ifdef HX_NATIVE
            // ★ X11, NATIVE ONLY — the SAME lifetime mismatch as the
            // RndMeshDeform release in BandCharacter::SyncObjects, in a second
            // place, and this one is what empties head.mesh.
            //
            // BandCharacter::SetDeformation does `mgr->Disable(!mInCloset)`, so
            // OUTSIDE the closet every MeshCacher is created disabled. The
            // disabled arm below then calls SetKeepMeshData(false), which clears
            // mVerts and frees mFaces (rndobj/Mesh.cpp:954-965), and skips
            // PopulateMesh. On the console that is right: the deformation has
            // already been pushed to the platform vertex buffer, so the CPU copy
            // is dead weight outside the interactive closet. The dc3 WebGPU
            // backend uploads LAZILY AT FIRST DRAW, which has not happened yet,
            // so the release destroys head.mesh's 2592/2999 verts (4726/5338
            // faces) before they are ever seen.
            //
            // MEASURED: with RB3_TRACE_KEEPMESH=1, head.mesh -- and ONLY
            // head.mesh -- reaches SetKeepMeshData(false) from this dtor.
            //
            // Only the DISABLED arm is changed. The closet arm keeps the shipped
            // restore-from-cache behaviour verbatim. Returning here (rather than
            // falling through to PopulateMesh) deliberately RETAINS THE DEFORMED
            // geometry -- PopulateMesh would write back the pre-deform snapshot
            // and quietly undo the head shaping.
            //
            // The X360 arm is textually unchanged.
            if (mDisabled && !getenv("RB3_RELEASE_MESHDATA"))
                return;
#endif
            mMesh->SetKeepMeshData(!mDisabled);
            if (!mDisabled)
                PopulateMesh();
        }
    }

    // TODO: rename these once you have a better idea of what they do
    void SyncMesh() { mMesh->Sync(mFlags | 0xA0); }
    void Sync(int mask) {
        mFlags |= mask;
        if (mFlags & 0x1F) {
            if (mVerts.size() == 0) {
                MemDoTempAllocations temp;
                mVerts.resize(mMesh->Verts().size());
                for (int i = 0; i < mVerts.size(); i++) {
                    mVerts[i].pos = mMesh->Verts(i).pos;
                    mVerts[i].norm = mMesh->Verts(i).norm;
                }
            }
        }
        if (mFlags & 0x20) {
            if (mFaces.size() == 0) {
                mFaces = mMesh->Faces();
            }
        }
        if (mFlags & 0x400 && mColors.size() == 0) {
            MemDoTempAllocations temp;
            mColors.resize(mMesh->Verts().size());
            for (int i = 0; i < mVerts.size(); i++) {
                mColors[i] = mMesh->Verts(i).color;
            }
        }
    }

    void PopulateMesh() {
        for (int i = 0; i < mVerts.size(); i++) {
            RndMesh::Vert &curVert = mMesh->Verts(i);
            curVert.pos = mVerts[i].pos;
            curVert.norm = mVerts[i].norm;
        }
        if (mFaces.size() != 0) {
            mMesh->Faces() = mFaces;
        }
        for (int i = 0; i < mColors.size(); i++) {
            RndMesh::Vert &curVert = mMesh->Verts(i);
            curVert.color = mColors[i];
        }
    }

    RndMesh *mMesh; // 0x0
    int mFlags; // 0x4
    bool mDisabled; // 0x8
    std::vector<SyncMeshCB::Vert> mVerts; // 0xc
    std::vector<RndMesh::Face> mFaces; // 0x18
    std::vector<Hmx::Color> mColors; // 0x24
};

class CharMeshCacheMgr : public SyncMeshCB {
public:
    CharMeshCacheMgr();
    virtual ~CharMeshCacheMgr();
    virtual void SyncMesh(RndMesh *, int);
    virtual bool HasMesh(RndMesh *);
    virtual const std::vector<SyncMeshCB::Vert> &GetVerts(RndMesh *) const; // fix return
                                                                            // type

    void Disable(bool);
    void StuffMeshes(ObjPtrList<RndMesh, ObjectDir> &);

    std::vector<MeshCacher *> mCache; // 0x4
    bool mDisabled; // 0x10
};
