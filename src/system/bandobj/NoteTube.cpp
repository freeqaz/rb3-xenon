#include "bandobj/NoteTube.h"
#include "rndobj/Env.h"
#include "math/Utl.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "decomp.h"
#include "macros.h"
#include <math.h>
#include <float.h>
#include <algorithm>

NoteTube::NoteTube()
    : mPitched(false), mPart(-1), unk_0x24(false), mGlowLevel(-1), unk_0x2C(0),
      unk_0x2D(0), unk_0x30(0), unk_0x34(0), mEndX(0), mBackMat(0), mFrontMat(0),
      mBackPlate(0), mFrontPlate(0), mBackParent(0), mFrontParent(0), mXPos(0),
      mAlpha(1) {
    mPoints.reserve(100);
}

void NoteTube::SetNumPoints(int i) {
    if (i > mPoints.capacity())
        MILO_WARN(
            "Reallocating NoteTube point buffer to %d; please alert HUD/Track owner!", i
        );
    mPoints.resize(i);
}

void NoteTube::SetPointPos(int i, Vector3 v) {
    if (i < mPoints.size())
        mPoints[i] = v;
    else
        MILO_WARN("note tube has too few points\n");
}

DECOMP_FORCEACTIVE(NoteTube, "point pos query out of bounds\n")

void NoteTube::SetGlowLevel(int i) {
    mGlowLevel = 3 - i;
    MILO_ASSERT_RANGE(mGlowLevel, 0, NumGlowLevels(), 73);
}

void NoteTube::BakePlates() {
    if (mBackPlate)
        mBackPlate->Bake();
    if (mFrontPlate)
        mFrontPlate->Bake();
}

void NoteTube::SetDeployTiming(float f1, float f2) {
    if (mBackPlate)
        mBackPlate->SetDeployTiming(f1, f2);
    if (mFrontPlate)
        mFrontPlate->SetDeployTiming(f1, f2);
}

void NoteTube::CreateMeshes() {
    int numpoints = mPoints.size();
    if (numpoints >= 2) {
        if (mPitched) {
            if (numpoints == 2) {
                float segmentLength = mPoints[1].x - mPoints[0].x;
                MILO_ASSERT(segmentLength > 0.0f, 0x67);
                segmentLength = Min(segmentLength * 0.5f, unk_0x34);
                mPoints[0].x += segmentLength;
                mPoints[1].x -= segmentLength;
            } else {
                float segmentLength = mPoints[1].x - mPoints[0].x;
                MILO_ASSERT(segmentLength > 0.0f, 0x75);
                segmentLength = Min(segmentLength, unk_0x34);
                mPoints[0].x += segmentLength;

                segmentLength = mPoints[numpoints - 1].x - mPoints[numpoints - 2].x;
                MILO_ASSERT(segmentLength > 0.0f, 0x79);
                segmentLength = Min(segmentLength, unk_0x34);
                mPoints[numpoints - 1].x -= segmentLength;
            }
        }
        if (mFrontPlate && mFrontMat) {
            if (mFrontPlate->mNumVerts == 0)
                InitializePlate(mFrontPlate, mFrontMat, mFrontParent);
            DrawToPlate(mFrontPlate);
        }
        if (mBackPlate && mBackMat) {
            if (mBackPlate->mNumVerts == 0)
                InitializePlate(mBackPlate, mBackMat, mBackParent);
            DrawToPlate(mBackPlate);
        }
    }
}

void NoteTube::SetMeshVert(RndMesh::Vert &v, float f1, float f2, float f3, float f4) {
    v.pos.Set(f1, 0, f2);
    v.tex.Set(f3, f4);
    v.color.alpha = mAlpha;
    mEndX = std::max(mEndX, f1);
}

void NoteTube::InitializePlate(TubePlate *plate, RndMat *mat, RndGroup *parent) {
    MILO_ASSERT(plate, 0x99);
    MILO_ASSERT(parent, 0x9A);
    RndMesh *mesh = plate->mMesh;
    MILO_ASSERT(mesh, 0x9D);
    mesh->SetMutable(0x3F);
    mesh->SetGeomOwner(mesh);
    mesh->SetMat(mat);
    mesh->SetTransParent(parent, false);
    mesh->SetShowing(false);
    plate->SetParent(parent);
    parent->AddObject(mesh, 0);
    Transform &xfm = mesh->DirtyLocalXfm();
    xfm.Reset();
    xfm.v.x = mXPos;
    plate->SetBeginX(mXPos);
    plate->SetWidthX(0);
}

void NoteTube::DrawToPlate(TubePlate *plate) {
    static bool warnOnReallocate = false;

    MILO_ASSERT(plate, 0xB4);
    RndMesh *mesh = plate->mMesh;
    MILO_ASSERT(mesh, 0xB7);
    MILO_ASSERT(!mesh->Showing(), 0xB8);

    if (mXPos < plate->mBeginX)
        plate->mBeginX = mXPos;

    float baseX = mXPos - mesh->LocalXfm().v.x;
    RndMesh::VertVector &verts = mesh->Verts();
    std::vector<RndMesh::Face> &faces = mesh->Faces();
    int vertStart = verts.size();
    int faceStart = faces.size();
    int numPoints = mPoints.size();

    if (mPitched) {
#define mWidth unk_0x30
        MILO_ASSERT(mWidth > 0.0f, 0xCE);
#undef mWidth
        int numEdges = numPoints * 2;
        plate->AllocateVerts(numEdges + 4, warnOnReallocate);

        bool atFront = mFrontPlate == plate;
        float uvX1, uvX0, uvY1, uvY0;
        LookupPitchedUVCoordinates(uvX1, uvX0, uvY1, uvY0, atFront);
        uvX1 -= 0.0078125f;
        uvX0 += 0.0078125f;

        float xStart = (0.015625f + (baseX + mPoints[0].x)) - 2.0f * unk_0x30;
        SetMeshVert(
            verts[vertStart],
            xStart,
            unk_0x30 + mPoints[0].z,
            uvX1,
            uvY1
        );
        SetMeshVert(
            verts[vertStart + 1],
            xStart,
            mPoints[0].z - unk_0x30,
            uvX1,
            uvY0
        );

        float halfWidth = 0.0f;
        int vertIdx = vertStart;
        for (int i = 0; i < (int)mPoints.size(); i++) {
            float px = mPoints[i].x;
            float pz = mPoints[i].z;
            if (i % 2) {
                if (i < numPoints - 1) {
                    float angle =
                        atan((mPoints[i + 1].z - pz) / (mPoints[i + 1].x - px));
                    halfWidth = unk_0x30 * (float)tan(angle * 0.5f);
                } else {
                    halfWidth = 0.0f;
                }
            }
            SetMeshVert(
                verts[vertIdx + 2],
                baseX + (px - halfWidth),
                pz + unk_0x30,
                uvX0,
                uvY1
            );
            SetMeshVert(
                verts[vertIdx + 3],
                baseX + (px + halfWidth),
                pz - unk_0x30,
                uvX0,
                uvY0
            );
            vertIdx += 2;
        }

        int lastVert = vertStart + numEdges;
        float lastX = (baseX + (2.0f * unk_0x30 + mPoints[numPoints - 1].x)) - 0.015625f;
        float lastZ = mPoints[numPoints - 1].z;
        SetMeshVert(
            verts[lastVert + 2],
            lastX,
            lastZ + unk_0x30,
            uvX1,
            uvY1
        );
        SetMeshVert(
            verts[lastVert + 3],
            lastX,
            lastZ - unk_0x30,
            uvX1,
            uvY0
        );

        int numFaces = numEdges + 2;
        plate->AllocateFaces(numFaces, warnOnReallocate);
        for (int i = 0; i < numFaces; i++) {
            if (i % 2) {
                faces[faceStart + i].Set(
                    vertStart + i + 2, vertStart + i + 1, vertStart + i
                );
            } else {
                faces[faceStart + i].Set(
                    vertStart + i, vertStart + i + 1, vertStart + i + 2
                );
            }
        }
    } else if (unk_0x24) {
        static float kMaxQuadSize = 5.0f;
        float length = mPoints[1].x - mPoints[0].x;
        float uvScale = length / (16.0f * unk_0x30);
        int numColumns = 1;
        for (float remaining = length; remaining > kMaxQuadSize;
             remaining -= kMaxQuadSize)
            numColumns++;
        int numVerts = (numColumns + 1) * 2;
        plate->AllocateVerts(numVerts, warnOnReallocate);

        SetMeshVert(
            verts[vertStart],
            baseX + mPoints[0].x,
            unk_0x30 + mPoints[0].z,
            0.0f,
            0.0f
        );
        SetMeshVert(
            verts[vertStart + 1],
            baseX + mPoints[0].x,
            mPoints[0].z - unk_0x30,
            0.0f,
            1.0f
        );

        int vertIdx = vertStart + 2;
        for (int i = 1; i < numColumns; i++) {
            float offset = (float)i * kMaxQuadSize;
            float u = (uvScale * offset) / length;
            float xOff = offset + (baseX + mPoints[0].x);
            SetMeshVert(
                verts[vertIdx],
                xOff,
                unk_0x30 + mPoints[0].z,
                u,
                0.0f
            );
            SetMeshVert(
                verts[vertIdx + 1],
                xOff,
                mPoints[0].z - unk_0x30,
                u,
                1.0f
            );
            vertIdx += 2;
        }

        int lastVert = vertStart + numVerts;
        SetMeshVert(
            verts[lastVert - 2],
            baseX + mPoints[1].x,
            unk_0x30 + mPoints[1].z,
            uvScale,
            0.0f
        );
        SetMeshVert(
            verts[lastVert - 1],
            baseX + mPoints[1].x,
            mPoints[1].z - unk_0x30,
            uvScale,
            1.0f
        );

        int numFaces = numColumns * 2;
        plate->AllocateFaces(numFaces, warnOnReallocate);
        for (int i = 0; i < numFaces; i++) {
            if (i % 2) {
                faces[faceStart + i].Set(
                    vertStart + i + 2, vertStart + i + 1, vertStart + i
                );
            } else {
                faces[faceStart + i].Set(
                    vertStart + i, vertStart + i + 1, vertStart + i + 2
                );
            }
        }
    } else {
        plate->AllocateVerts(8, warnOnReallocate);

        float x0 = baseX + mPoints[0].x;
        float x0High = 0.05f + x0;
        SetMeshVert(
            verts[vertStart], x0 - 0.05f, unk_0x30 + mPoints[0].z, 0.0f, 0.0f
        );
        SetMeshVert(
            verts[vertStart + 1],
            x0 - 0.05f,
            mPoints[0].z - unk_0x30,
            0.0f,
            1.0f
        );
        SetMeshVert(
            verts[vertStart + 2],
            x0High,
            unk_0x30 + mPoints[0].z,
            0.015625f,
            0.0f
        );
        SetMeshVert(
            verts[vertStart + 3],
            x0High,
            mPoints[0].z - unk_0x30,
            0.015625f,
            1.0f
        );

        float x1 = baseX + mPoints[1].x;
        float x1High = 0.05f + x1;
        SetMeshVert(
            verts[vertStart + 4],
            x1 - 0.05f,
            unk_0x30 + mPoints[1].z,
            0.984375f,
            0.0f
        );
        SetMeshVert(
            verts[vertStart + 5],
            x1 - 0.05f,
            mPoints[1].z - unk_0x30,
            0.984375f,
            1.0f
        );
        SetMeshVert(
            verts[vertStart + 6],
            x1High,
            unk_0x30 + mPoints[1].z,
            1.0f,
            0.0f
        );
        SetMeshVert(
            verts[vertStart + 7],
            x1High,
            mPoints[1].z - unk_0x30,
            1.0f,
            1.0f
        );

        plate->AllocateFaces(6, warnOnReallocate);
        for (int i = 0; i < 6; i++) {
            if (i % 2) {
                faces[faceStart + i].Set(
                    vertStart + i + 2, vertStart + i + 1, vertStart + i
                );
            } else {
                faces[faceStart + i].Set(
                    vertStart + i, vertStart + i + 1, vertStart + i + 2
                );
            }
        }
    }

    float widthX = plate->mWidthX;
    plate->mWidthX = std::max(widthX, mEndX);
    mEndX = 0.0f;
}

DECOMP_FORCEACTIVE(NoteTube, "mWidth > 0.0f")

void NoteTube::LookupPitchedUVCoordinates(
    float &f1, float &f2, float &f3, float &f4, bool b
) {
    f1 = 1.0f;
    f2 = 0.0f;
    f3 = 0.0f;
    f4 = 1.0f;
    MILO_ASSERT(mPitched, 0x1A8);
    MILO_ASSERT(mGlowLevel != -1, 0x1A9);
    int i2, column;
    if (b) {
        MILO_ASSERT((mPart == 0) || (mPart == 1), 0x1AF);
        column = (mPart == 0) + 2;
        i2 = 0;
    } else {
        column = mGlowLevel;
        i2 = 1 + mPart + (unk_0x2C != 0 ? 3 : 0);
    }
    if (i2 != -1) {
        MILO_ASSERT(column != -1, 0x1C0);
        f2 = column * 0.25f;
        f1 = f2 + 0.25f;
        f3 = i2 * 0.125f;
        f4 = f3 + 0.125f;
    }
}

TubePlate::TubePlate(int i)
    : mMesh(Hmx::Object::New<RndMesh>()), mParent(0), mAllocationCount(i),
      mBeginX(FLT_MAX), mWidthX(0), mBaked(0),
      mActiveMs(FLT_MAX), mInvalidateMs(FLT_MAX),
      mMatSize(0), mDeploy(0) {
    mMesh->Faces().reserve(mAllocationCount);
    Reset();
}

TubePlate::~TubePlate() { RELEASE(mMesh); }

void TubePlate::AllocateVerts(int num, bool warn) {
    RndMesh::VertVector &verts = mMesh->Verts();
    int newsize = num + verts.size();
    verts.resize(newsize);
    mNumVerts += num;
}

void TubePlate::AllocateFaces(int num, bool warn) {
    std::vector<RndMesh::Face> &faces = mMesh->Faces();
    int newsize = num + faces.size();
    int cap = faces.capacity();
    if (newsize > cap) {
        int count = mAllocationCount;
        float ceiled = std::ceil((float)(newsize - cap) / (float)count);
        faces.reserve((int)ceiled * count + cap);
        if (warn)
            MILO_WARN(
                "TubePlate: Reallocating faces from %d to %d; please alert HUD/Track owner",
                cap,
                faces.capacity()
            );
    }
    faces.resize(newsize);
}

void TubePlate::Bake() {
    if (!mBaked) {
        MILO_ASSERT(mMesh, 0x21A);
        RndMesh::VertVector &verts = mMesh->Verts();
        if (mDeploy) {
            mMatSize = verts[verts.size() - 2].tex.x;
        }
        if (!mDeploy)
            mMesh->SetMutable(0);
        mMesh->Sync(0x2BF);
        mMesh->SetShowing(true);
        mBaked = true;
    }
}

void TubePlate::SetShowing(bool b) { mMesh->SetShowing(b && mBaked); }

float TubePlate::CurrentStartX(float f) const { return mBeginX + f; }
float TubePlate::CurrentEndX(float f) const { return mBeginX + mWidthX + f; }

void TubePlate::Reset() {
    mMesh->SetShowing(false);
    if (mParent)
        mParent->RemoveObject(mMesh);
    mMesh->SetMutable(0x3F);
    mMesh->Verts().clear();
    mMesh->Faces().clear();
    mWidthX = 0;
    mBaked = false;
    mBeginX = FLT_MAX;
    mActiveMs = FLT_MAX;
    mInvalidateMs = FLT_MAX;
    mMatSize = 0;
    mDeploy = false;
    mNumVerts = 0;
}

String TubePlate::GetMatName() {
    if (mMesh && mMesh->Mat()) {
        return String(mMesh->Mat()->Name());
    } else
        return String("<no mat>");
}

void TubePlate::SetDeployTiming(float f1, float f2) {
    mActiveMs = f1;
    mInvalidateMs = f2;
    mDeploy = true;
}

void TubePlate::PollDeploy(float f) {
    if (mBaked && mActiveMs < f && f < mInvalidateMs) {
        RndMesh::VertVector &verts = mMesh->Verts();
        float f3 = (mInvalidateMs - f) / (mInvalidateMs - mActiveMs);
        float f1 = mWidthX * f3;
        float f2 = mMatSize * f3;
        for (int i = verts.size() - 2; i < verts.size(); i++) {
            RndMesh::Vert &curvert = verts[i];
            curvert.pos.x = f1;
            curvert.tex.x = f2;
        }
    }
}
