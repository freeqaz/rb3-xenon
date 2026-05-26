#include "rndobj/Ribbon.h"
#include "math/Mtx.h"
#include "obj/Object.h"
#include "os/File.h"
#include "obj/Task.h"
#include "rndobj/Draw.h"
#include "rndobj/Mesh.h"
#include "rndobj/Poll.h"
#include "rndobj/Trans.h"
#include "utl/Loader.h"
#include <cmath>

RndRibbon::RndRibbon()
    : mLastTime(-1.0f), mNumSides(4), mMat(this), mWidth(1), mDirty(1), mActive(true),
      mNumSegments(0), mDecay(1), mFollowA(this), mFollowB(this), mFollowWeight(0),
      mTaper(0) {
    mMesh = Hmx::Object::New<RndMesh>();
    mMesh->SetMutable(0x1F);
}

RndRibbon::~RndRibbon() { RELEASE(mMesh); }

BEGIN_HANDLERS(RndRibbon)
    HANDLE_ACTION(expose_mesh, ExposeMesh())
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(RndPollable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(RndRibbon)
    SYNC_PROP_SET(active, mActive, SetActive(_val.Int()));
    SYNC_PROP_MODIFY(num_sides, mNumSides, mDirty |= 1)
    SYNC_PROP_MODIFY(num_segments, mNumSegments, mDirty |= 1)
    SYNC_PROP_MODIFY(mat, mMat, mMesh->SetMat(mMat))
    SYNC_PROP_MODIFY(width, mWidth, mDirty |= 2)
    SYNC_PROP(follow_a, mFollowA)
    SYNC_PROP(follow_b, mFollowB)
    SYNC_PROP(follow_weight, mFollowWeight)
    SYNC_PROP(taper, mTaper)
    SYNC_PROP(decay, mDecay)
    SYNC_SUPERCLASS(RndDrawable)
    SYNC_SUPERCLASS(RndPollable)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(RndRibbon)
    SAVE_REVS(0, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndDrawable)
    bs << mNumSides;
    bs << mMat;
    bs << mActive;
    bs << mWidth;
    bs << mNumSegments;
    bs << mFollowA;
    bs << mFollowB;
    bs << mFollowWeight;
    bs << mTaper;
    bs << mDecay;
END_SAVES

BEGIN_COPYS(RndRibbon)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndDrawable)
    CREATE_COPY(RndRibbon)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mNumSides)
        COPY_MEMBER(mMat)
        COPY_MEMBER(mActive)
        COPY_MEMBER(mWidth)
        COPY_MEMBER(mNumSegments)
        COPY_MEMBER(mFollowA)
        COPY_MEMBER(mFollowB)
        COPY_MEMBER(mFollowWeight)
        COPY_MEMBER(mTaper)
        COPY_MEMBER(mDecay)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(0, 0)

BEGIN_LOADS(RndRibbon)
    LOAD_REVS(bs)
    ASSERT_REVS(0, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(RndDrawable)
    bs >> mNumSides;
    bs >> mMat;
    d >> mActive;
    bs >> mWidth;
    bs >> mNumSegments;
    bs >> mFollowA;
    bs >> mFollowB;
    bs >> mFollowWeight;
    d >> mTaper;
    bs >> mDecay;
    mDirty = 1;
    mMesh->SetMat(mMat);
END_LOADS

void RndRibbon::Poll() {
    if (mDirty & 1) {
        ConstructMesh();
        mDirty = 0;
    }
    UpdateChase();
    mDirty = 0;
}

void RndRibbon::DrawShowing() {
    if (mActive || TheLoadMgr.EditMode()) {
        mMesh->DrawShowing();
    }
}

void RndRibbon::SetActive(bool b) {
    if (mActive != b) {
        mTransforms.clear();
        mLastTime = -1.0;
    }
    mActive = b;
}

void RndRibbon::ExposeMesh() {
    if (!mMesh->Dir()) {
        const char *base = FileGetBase(Name());
        mMesh->SetName(MakeString("%s_mesh.mesh", base), Dir());
    }
}

void RndRibbon::ConstructMesh() {
#ifndef HX_NATIVE
    if (mNumSegments <= 0)
        return;

    mMesh->Verts().resize(mNumSides * mNumSegments * 2);

    unsigned int numFacePairs = (unsigned int)(mNumSegments * mNumSides);
    RndMesh::Face emptyFace;
    std::vector<RndMesh::Face> &faces = mMesh->Faces();
    unsigned int targetFaceCount = numFacePairs * 2;
    int facesBegin = (int)faces.begin();
    unsigned int curFaceCount = (unsigned int)(((int)faces.end() - facesBegin) / 6);

    if (targetFaceCount < curFaceCount) {
        faces.erase(
            (RndMesh::Face *)(facesBegin + (int)targetFaceCount * 6),
            (RndMesh::Face *)((int)faces.end())
        );
    } else {
        faces.insert(
            faces.end(),
            targetFaceCount - (unsigned int)(((int)faces.end() - facesBegin) / 6),
            emptyFace
        );
    }

    int seg = 0;
    if (mNumSegments > 0) {
        int numSides = mNumSides;
        do {
            int baseVert = numSides * seg;
            int baseVert2 = baseVert * 2;
            int side = 0;
            if (numSides > 0) {
                int faceOff = baseVert2 * 6;
                int vertIdx = baseVert2;
                int oneMinusBV2 = 1 - baseVert2;
                do {
                    int ns = mNumSides;
                    int nextVertOff = oneMinusBV2 + vertIdx;
                    unsigned short v0 = (unsigned short)vertIdx;
                    int rem = nextVertOff % ns;
                    int vNextRaw = rem + baseVert2;
                    int v0PlusNS = vertIdx + ns;
                    int vNextWrapRaw = ns + vNextRaw;
                    short *facePtr = (short *)((int)mMesh->Faces().begin() + faceOff);
                    unsigned short vNextWrap = (unsigned short)vNextWrapRaw;
                    unsigned short vNext = (unsigned short)vNextRaw;
                    unsigned short v0PlusNSu = (unsigned short)v0PlusNS;
                    side++;
                    facePtr[0] = v0;
                    vertIdx = vertIdx + 1;
                    facePtr[1] = vNext;
                    facePtr[2] = vNextWrap;
                    short *faceBase = (short *)((int)mMesh->Faces().begin() + faceOff);
                    *(short *)((int)faceBase + 6) = vNextWrap;
                    *(short *)((int)faceBase + 8) = v0PlusNSu;
                    faceOff = faceOff + 12;
                    *(short *)((int)faceBase + 10) = v0;
                    numSides = mNumSides;
                } while (side < numSides);
            }
            seg++;
        } while (seg < mNumSegments);
    }

    mMesh->Sync(0x3f);
#endif // !HX_NATIVE
}

void RndRibbon::UpdateMesh() {
#ifndef HX_NATIVE
    if (mTransforms.size() == 0)
        return;

    int numSides = mNumSides;
    RndMesh::VertVector &verts = mMesh->Verts();
    int seg = 0;
    float angleStep = 6.2831855f / (float)(long long)numSides;
    float halfWidth = mWidth * 0.5f;
    float latestFrame = mTransforms.back().frame;
    Vector3 norm(0.0f, 0.0f, 0.0f);
    if (mNumSegments > 0) {
        do {
            int side = 0;
            int vertRowBase = mNumSides * seg * 2;
            float vCoord = 1.0f / (float)(long long)mNumSides;
            if (numSides > 0) {
                do {
                    int row = 0;
                    float angle = (float)side * angleStep;
                    float uFrac = (float)side * vCoord;
                    do {
                        unsigned int rowSeg = (unsigned int)(row + seg);
                        int vertIdx = mNumSides * row + vertRowBase;
                        unsigned int lastIdx = mTransforms.size() - 1;
                        if ((int)rowSeg <= (int)lastIdx) {
                            lastIdx = ((rowSeg >> 31) - 1) & rowSeg;
                        }
                        float segFrame = mTransforms[lastIdx].frame;
                        float taperScale;
                        if (mTaper) {
                            taperScale = 1.0f - (latestFrame - segFrame) / mDecay;
                        } else {
                            taperScale = 1.0f;
                        }
                        float cosA = (float)cos((double)angle);
                        float sinA = (float)sin((double)angle);
                        Transform *xfm = &mTransforms[lastIdx].value;
                        float posZ = cosA * taperScale * halfWidth;
                        float posX = sinA * taperScale * halfWidth;
                        Vector3 pos(posX, 0.0f, posZ);
                        Multiply(pos, *xfm, pos);
                        RndMesh::Vert &vert = verts[vertIdx];
                        vert.pos = pos;
                        if (row == 0) {
                            norm.x = pos.x - xfm->v.x;
                            norm.y = pos.y - xfm->v.y;
                            norm.z = pos.z - xfm->v.z;
                            Normalize(norm, norm);
                        }
                        row++;
                        vert.norm = norm;
                        vert.tex.x =
                            1.0f - (latestFrame - segFrame) / mDecay;
                        vert.tex.y = uFrac;
                    } while (row < 2);
                    numSides = mNumSides;
                    side++;
                    vertRowBase = vertRowBase + 1;
                } while (side < numSides);
            }
            seg++;
        } while (seg < mNumSegments);
    }
    mMesh->Sync(0x1f);
#endif // !HX_NATIVE
}

void RndRibbon::UpdateChase() {
#ifndef HX_NATIVE
    if (!mFollowA)
        return;

    float currentTime = TheTaskMgr.Seconds(TaskMgr::kRealTime);

    if (currentTime < mLastTime) {
        auto _tmp2 = mTransforms.end();
        auto _tmp1 = mTransforms.begin();
        if (_tmp1 != _tmp2) {
            mTransforms.erase(_tmp1, _tmp2);
        }
    }
    int newSegCount = 0;

    Vector3 followPos;
    if (mActive) {
        const Transform *xfmA = &mFollowA->WorldXfm();
        followPos = xfmA->v;
        if (mFollowB) {
            const Transform *xfmB = &mFollowB->WorldXfm();
            Interp(followPos, (const Vector3 &)xfmB->v, mFollowWeight, followPos);
        }

        int numTransforms = ((intptr_t)&*mTransforms.end() - (intptr_t)&*mTransforms.begin()) / 0x44;
        unsigned int removeCount = 0;
        if (numTransforms != 0) {
            unsigned int k = 0;
            do {
                if (currentTime - mDecay <= *(float *)(((intptr_t)&*mTransforms.begin() + (k + 0x40))))
                    break;
                removeCount++;
                k += 0x44;
            } while (removeCount < (unsigned int)numTransforms);
        }

        if (removeCount < (unsigned int)numTransforms) {
            unsigned int srcIdx = removeCount;
            unsigned int dstIdx = 0;
            do {
                memcpy(&mTransforms[dstIdx], &mTransforms[srcIdx], 0x44);
                srcIdx++;
                dstIdx++;
                numTransforms = ((intptr_t)&*mTransforms.end() - (intptr_t)&*mTransforms.begin()) / 0x44;
            } while (srcIdx < (unsigned int)numTransforms);
        }

        Key<Transform> newKey;
        newKey.frame = 0.0f;
        mTransforms.resize(numTransforms - (int)removeCount, newKey);

        memcpy(&newKey, &Transform::IDXfm(), 0x40);
        newKey.frame = 0.0f;

        numTransforms = ((intptr_t)&*mTransforms.end() - (intptr_t)&*mTransforms.begin()) / 0x44;
        if (numTransforms == 0) {
            newKey.frame = currentTime;
            newKey.value.v = followPos;
            mTransforms.push_back(newKey);
        } else {
            long long numSegs = (long long)mNumSegments;
            float minDistSq = mWidth * mWidth * 0.125f;
            float segInterval = mDecay / (float)numSegs;
            float nextTime = mTransforms.back().frame + segInterval;
            while (nextTime < currentTime) {
                Transform *backXfm = &mTransforms.back().value;
                newKey.frame = mTransforms.back().frame + segInterval;
                Interp((const Vector3 &)backXfm->v, followPos,
                       segInterval / (currentTime - mTransforms.back().frame),
                       (Vector3 &)newKey.value.v);
                float dx = backXfm->v.z - newKey.value.v.z;
                float dy = backXfm->v.x - newKey.value.v.x;
                float dz = backXfm->v.y - newKey.value.v.y;
                if (minDistSq <= dz * dz + (dy * dy + dx * dx)) {
                    mTransforms.push_back(newKey);
                    newSegCount++;
                } else {
                    mTransforms.back().frame = newKey.frame;
                }
                nextTime = mTransforms.back().frame + segInterval;
            }
        }
    }

    // Orient each transform
    int numTransforms = ((intptr_t)&*mTransforms.end() - (intptr_t)&*mTransforms.begin()) / 0x44;
    int startIdx = numTransforms - newSegCount;
    if ((unsigned int)startIdx < (unsigned int)numTransforms) {
        float slerpFwdX = 0.0f;
        float slerpFwdY = 0.0f;
        float slerpFwdZ = 0.0f;
        float prevAngle = -1.0f;

        static int sUpVecFlag;
        static Vector3 sUpVec;

        unsigned int curIdx = (unsigned int)startIdx;
        do {
            float curAngle = prevAngle;
            if (curIdx != 0) {
                Transform &curXfm = mTransforms[curIdx].value;
                Transform &prevXfm = mTransforms[curIdx - 1].value;
                Vector3 forward;
                forward.x = curXfm.v.x - prevXfm.v.x;
                forward.y = curXfm.v.y - prevXfm.v.y;
                forward.z = curXfm.v.z - prevXfm.v.z;
                Normalize(forward, forward);

                if ((int)curIdx >= 3) {
                    Transform &prevPrevXfm = mTransforms[curIdx - 2].value;
                    float pdx = curXfm.v.x - prevPrevXfm.v.x;
                    float pdy = curXfm.v.y - prevPrevXfm.v.y;
                    float pdz = curXfm.v.z - prevPrevXfm.v.z;
                    float dot = pdy * forward.y + pdx * forward.x + pdz * forward.z;
                    float clampedDot = 0.0f;
                    if (-dot < 0.0f)
                        clampedDot = dot;
                    float clampedDot2 = 1.0f;
                    if (clampedDot - 1.0f < 0.0f)
                        clampedDot2 = clampedDot;
                    curAngle = std::acos(clampedDot2);
                    Vector3 newSlerpFwd;
                    Interp(forward, followPos, 0.5f, newSlerpFwd);
                    Normalize(newSlerpFwd, newSlerpFwd);
                    slerpFwdX = newSlerpFwd.x;
                    slerpFwdY = newSlerpFwd.y;
                    slerpFwdZ = newSlerpFwd.z;
                }

                if ((sUpVecFlag & 1) == 0) {
                    sUpVecFlag |= 1;
                    sUpVec.x = 0.0f;
                    sUpVec.y = 0.0f;
                    sUpVec.z = 1.0f;
                }

                Transform invPrev;
                Invert(prevXfm, invPrev);
                Vector3 localVec;
                Multiply(curXfm.v, invPrev, localVec);
                Transform lookAt;
                memcpy(&lookAt, &Transform::IDXfm(), 0x40);
                lookAt.LookAt(localVec, sUpVec);
                Transform result;
                Multiply(lookAt, prevXfm.m, result);
                Normalize(result.m, result.m);

                float sv_y = curXfm.v.y;
                float sv_z = curXfm.v.z;

                if (curAngle != prevAngle) {
                    Invert(result.m, invPrev.m);
                    float sdotX = invPrev.m.z.x * slerpFwdZ +
                                  (invPrev.m.x.x * slerpFwdX +
                                   invPrev.m.y.x * slerpFwdY);
                    slerpFwdY = invPrev.m.z.y * slerpFwdZ +
                                (invPrev.m.x.y * slerpFwdX +
                                 slerpFwdY * invPrev.m.y.y);
                    slerpFwdZ = invPrev.m.z.z * slerpFwdZ +
                                (invPrev.m.x.z * slerpFwdX +
                                 invPrev.m.y.z * slerpFwdY);

                    float newAcosIn = 0.0f;
                    if (-sdotX < 0.0f)
                        newAcosIn = sdotX;
                    float newAcosIn2 = 1.0f;
                    if (newAcosIn - 1.0f < 0.0f)
                        newAcosIn2 = newAcosIn;
                    float newSlerpAngle = std::acos(newAcosIn2);
                    float cosHalfCur = std::cos(curAngle * 0.5f);
                    float halfNew2 = newSlerpAngle * 2.0f;
                    float invCosHalfCur = 1.0f / cosHalfCur;
                    float cosHN2 = std::cos(halfNew2);
                    float sinHN2 = std::sin(halfNew2);
                    float mXX = (cosHN2 + 1.0f) * (invCosHalfCur - 1.0f) * 0.5f + 1.0f;
                    float mXZ = sinHN2 * (1.0f - invCosHalfCur) * 0.5f;
                    float mZZ = (1.0f - cosHN2) * (invCosHalfCur - 1.0f) * 0.5f + 1.0f;
                    lookAt.m.x.x = mXX; lookAt.m.x.y = 0.0f; lookAt.m.x.z = mXZ;
                    lookAt.m.y.x = 0.0f; lookAt.m.y.y = 1.0f; lookAt.m.y.z = 0.0f;
                    lookAt.m.z.x = mXZ;  lookAt.m.z.y = 0.0f; lookAt.m.z.z = mZZ;
                    Multiply(lookAt.m, result.m, result.m);
                }

                memcpy(&curXfm, &result, 0x30);
                curXfm.v.y = sv_y;
                curXfm.v.z = sv_z;
            }
            curIdx++;
            prevAngle = curAngle;
        } while (curIdx < (unsigned int)(((intptr_t)&*mTransforms.end() - (intptr_t)&*mTransforms.begin()) / 0x44));
    }

    UpdateMesh();
    mLastTime = currentTime;
#endif // !HX_NATIVE
}
