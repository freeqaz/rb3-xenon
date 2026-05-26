#include "char/CharBonesMeshes.h"
#include "char/CharUtl.h"
#include "math/Rot.h"
#include "obj/Object.h"
#include "rndobj/Trans.h"
#include "utl/Str.h"
#include <string.h>

RndTransformable *CharBonesMeshes::sDummyMesh;

CharBonesMeshes::CharBonesMeshes() : mMeshes(this, (EraseMode)0, kObjListOwnerControl) {}

CharBonesMeshes::~CharBonesMeshes() { mMeshes.clear(); }

bool CharBonesMeshes::Replace(ObjRef *ref, Hmx::Object *obj) {
    ObjPtrVec<RndTransformable>::iterator it = mMeshes.FindRef(ref);
    if (it != mMeshes.end()) {
        RndTransformable *trans = obj ? dynamic_cast<RndTransformable *>(obj) : 0;
        mMeshes.Set(it, trans);
        if (!*it) {
            mMeshes.Set(it, sDummyMesh);
        }
        return true;
    }
    return Hmx::Object::Replace(ref, obj);
}

void CharBonesMeshes::ReallocateInternal() {
    CharBonesAlloc::ReallocateInternal();
    String str;
    mMeshes.clear();
    mMeshes.reserve(mBones.size());
    for (int i = 0; i < mBones.size(); i++) {
        RndTransformable *trans = CharUtlFindBoneTrans(mBones[i].name.Str(), Dir());
        if (!trans) {
            if (strncmp("bone_facing", mBones[i].name.Str(), 0xB)) {
                str += MakeString("%s, ", mBones[i].name);
            }
            trans = sDummyMesh;
        }
        mMeshes.push_back(trans);
    }
    if (mMeshes.empty())
        return;
    else
        AcquirePose();
}

void CharBonesMeshes::AcquirePose() {
    ObjPtrVec<RndTransformable>::iterator curMesh = mMeshes.begin();

    // Copy positions
    char *scaleOff = mOffsets[TYPE_SCALE] + mStart;
    char *pos = mStart;
    for (; pos < scaleOff; pos += sizeof(Vector3), ++curMesh) {
        *(Vector3 *)pos = (*curMesh)->LocalXfm().v;
    }

    // Copy scales using MakeScale
    pos = mOffsets[TYPE_SCALE] + mStart;
    char *quatOff = mOffsets[TYPE_QUAT] + mStart;
    for (; pos < quatOff; pos += sizeof(Vector3), ++curMesh) {
        MakeScale((*curMesh)->LocalXfm().m, *(Vector3 *)pos);
    }

    // Copy quaternions using Quat::Set
    pos = mOffsets[TYPE_QUAT] + mStart;
    char *rotxOff = mOffsets[TYPE_ROTX] + mStart;
    for (; pos < rotxOff; pos += sizeof(Hmx::Quat), ++curMesh) {
        ((Hmx::Quat *)pos)->Set((*curMesh)->LocalXfm().m);
    }

    // Copy X rotations
    float *rotIt = (float *)(mOffsets[TYPE_ROTX] + mStart);
    float *rotyOff = (float *)(mOffsets[TYPE_ROTY] + mStart);
    for (; rotIt < rotyOff; rotIt++, ++curMesh) {
        *rotIt = GetXAngle((*curMesh)->LocalXfm().m);
    }

    // Copy Y rotations
    float *rotzOff = (float *)(mOffsets[TYPE_ROTZ] + mStart);
    for (; rotIt < rotzOff; rotIt++, ++curMesh) {
        *rotIt = GetYAngle((*curMesh)->LocalXfm().m);
    }

    // Copy Z rotations
    float *endOff = (float *)(mOffsets[TYPE_END] + mStart);
    for (; rotIt < endOff; rotIt++, ++curMesh) {
        *rotIt = GetZAngle((*curMesh)->LocalXfm().m);
    }
}

void CharBonesMeshes::PoseMeshes() {
    ObjPtrVec<RndTransformable>::iterator curMesh = mMeshes.begin();

#ifdef HX_NATIVE
    { static int sPoseMeshLog = 0;
      if (sPoseMeshLog < 3 && mMeshes.size() > 20) {
        sPoseMeshLog++;
        fprintf(stderr, "POSEMESHES dir='%s' servo='%s' meshCount=%d boneCount=%d\n",
                Dir() ? Dir()->Name() : "null", Name(), (int)mMeshes.size(), (int)mBones.size());
        // Dump ALL bone mesh mappings
        for (int k = 0; k < (int)mMeshes.size(); k++) {
            RndTransformable* bt = mMeshes[k];
            fprintf(stderr, "  servo_mesh[%2d] '%s' bone='%s'\n",
                    k, bt ? bt->Name() : "NULL",
                    k < (int)mBones.size() ? mBones[k].name.Str() : "?");
        }
      }
    }
#endif

    // Set positions
    auto& start = mStart;
    Vector3 *pos = (Vector3 *)start;
    auto& scaleOffset = mOffsets[TYPE_SCALE];
    Vector3 *scaleOff = (Vector3 *)(start + scaleOffset);
    for (; pos < scaleOff; pos++, ++curMesh) {
        (*curMesh)->SetLocalPos(*pos);
    }

    // Handle quaternions and rotations if we have enough meshes
    auto& quatOffset = mOffsets[TYPE_QUAT];
    if (mCounts[TYPE_QUAT] < mMeshes.size()) {
        curMesh = mMeshes.begin() + mCounts[TYPE_QUAT];

        // Apply quaternion rotations
        Hmx::Quat *quat = (Hmx::Quat *)(start + quatOffset);
        Hmx::Quat *quatEnd = (Hmx::Quat *)(start + mOffsets[TYPE_ROTX]);
        for (; quat < quatEnd; quat++, ++curMesh) {
            Normalize(*quat, *quat);
            MakeRotMatrix(*quat, (*curMesh)->DirtyLocalXfm().m);
        }

        // Apply X rotations
        float *rotIt = (float *)(start + mOffsets[TYPE_ROTX]);
        float *rotyOff = (float *)(start + mOffsets[TYPE_ROTY]);
        for (; rotIt < rotyOff; rotIt++, ++curMesh) {
            MakeRotMatrixX(*rotIt, (*curMesh)->DirtyLocalXfm().m);
        }

        // Apply Y rotations
        float *rotzOff = (float *)(start + mOffsets[TYPE_ROTZ]);
        for (; rotIt < rotzOff; rotIt++, ++curMesh) {
            MakeRotMatrixY(*rotIt, (*curMesh)->DirtyLocalXfm().m);
        }

        // Apply Z rotations
        float *endOff = (float *)(start + mOffsets[TYPE_END]);
#ifdef HX_NATIVE
        { static int sRotzLog = 0;
          if (sRotzLog < 3) {
            sRotzLog++;
            float *rp = rotIt;
            int idx = mCounts[TYPE_ROTZ];
            for (; rp < endOff; rp++, idx++) {
                fprintf(stderr, "  ROTZ[%d] '%s' value=%.4f\n",
                        idx, idx < (int)mBones.size() ? mBones[idx].name.Str() : "?", *rp);
            }
          }
        }
#endif
        for (; rotIt < endOff; rotIt++, ++curMesh) {
            MakeRotMatrixZ(*rotIt, (*curMesh)->DirtyLocalXfm().m);
        }
    }

    // Handle scales if we have enough meshes
    if (mCounts[TYPE_SCALE] < mMeshes.size()) {
        curMesh = mMeshes.begin() + mCounts[TYPE_SCALE];
        Vector3 *scale = (Vector3 *)(start + scaleOffset);
        Vector3 *scaleEnd = (Vector3 *)(start + quatOffset);
        for (; scale < scaleEnd; scale++, ++curMesh) {
            Transform &xfm = (*curMesh)->DirtyLocalXfm();
            Vector3 scaleVec;
            MakeScale(xfm.m, scaleVec);
            xfm.m.x *= scale->x / scaleVec.x;
            xfm.m.y *= scale->y / scaleVec.y;
            xfm.m.z *= scale->z / scaleVec.z;
        }
    }
}

void CharBonesMeshes::StuffMeshes(std::list<Hmx::Object *> &oList) {
    for (int i = 0; i < mMeshes.size(); i++) {
        oList.push_back(mMeshes[i]);
    }
}

BEGIN_PROPSYNCS(CharBonesMeshes)
    SYNC_PROP(meshes, mMeshes)
    SYNC_SUPERCLASS(CharBonesObject)
END_PROPSYNCS

void CharBonesMeshes::Init() { sDummyMesh = Hmx::Object::New<RndTransformable>(); }

void CharBonesMeshes::Terminate() {}
