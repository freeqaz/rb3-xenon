// Retail INLINES this TU's single ObjPtr<RndCam> owner-ctor (three stores, no
// `bl`) but keeps ObjPtr<Hmx::Object>'s out of line.  The define is TU-WIDE, so
// the two Hmx::Object sites in Load are spelled with the explicit two-arg ctor,
// which stays declared-only/out-of-line.  Must precede every include.
#define RB3_OBJPTR_INLINE_OWNER_CTOR 1
#include "rndobj/Gen.h"
#include "math/Geo.h"
#include "math/Rand.h"
#include "math/Rot.h"
#include <cmath>
#include "obj/Object.h"
#include "rndobj/Anim.h"
#include "rndobj/Cam.h"
#include "rndobj/Draw.h"
#include "rndobj/MultiMesh.h"
#include "rndobj/Part.h"
#include "rndobj/Trans.h"
#include "rndobj/Utl.h"

// RB3-360 retail rev storage for THIS unit: there is none.  Adjudicated on retail
// bytes -- the target reads the revision straight into a stack local and reloads
// it with `lwz` + SIGNED `cmpwi` at every test (`lwz r11,0x54(r31)`;
// `cmpwi cr6,r11,0x9`), with no store between the ReadEndian and the first
// reload.  So retail neither masked the packed rev nor spilled it to a file
// static: it kept the raw 32-bit `int` and compared it signed.  Nothing outside
// Load reads a revision in this TU (the alt-rev word was written and never
// read), so the aggregate was pure overhead -- it also burned a callee-saved
// register for its base pointer, which is why our prologue used __savegprlr_25
// against retail's __savegprlr_26 and our frame was 0x10 larger.

RndGenerator::RndGenerator()
    // Four two-arg spellings: retail's ??0RndGenerator@@ calls these ObjPtr
    // ctors OUT OF LINE, and RB3_OBJPTR_INLINE_OWNER_CTOR (needed for Load's
    // ObjPtr<RndCam> site) is TU-WIDE, so it would inline them here too --
    // measured 81.3% -> 61.1% before this spelling was applied.
    : mPath(this, nullptr), mPathStartFrame(0), mPathEndFrame(0), mMesh(this, nullptr),
      mMultiMesh(this, nullptr), mParticleSys(this, nullptr), mNextFrameGen(-9999999), mRateGenLow(100), mRateGenHigh(100),
      mScaleGenLow(1), mScaleGenHigh(1), mPathVarMaxX(0), mPathVarMaxY(0),
      mPathVarMaxZ(0) {}

RndGenerator::~RndGenerator() { ResetInstances(); }

BEGIN_HANDLERS(RndGenerator)
    HANDLE(set_path, OnSetPath)
    HANDLE(set_ratevar, OnSetRateVar)
    HANDLE(set_scalevar, OnSetScaleVar)
    HANDLE(set_pathvar, OnSetPathVar)
    HANDLE(generate, OnGenerate)
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(RndTransformable)
    HANDLE_SUPERCLASS(RndAnimatable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(RndGenerator)
    SYNC_SUPERCLASS(RndDrawable)
    SYNC_SUPERCLASS(RndTransformable)
    SYNC_SUPERCLASS(RndAnimatable)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

static int gSaveRev_RndGenerator = 0xB;
BEGIN_SAVES(RndGenerator)
    bs << gSaveRev_RndGenerator;
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndTransformable)
    SAVE_SUPERCLASS(RndDrawable)
    SAVE_SUPERCLASS(RndAnimatable)
    bs << mMesh << mPath;
    bs << mRateGenLow << mRateGenHigh << mScaleGenLow << mScaleGenHigh;
    bs << mPathVarMaxX << mPathVarMaxY << mPathVarMaxZ;
    bs << mPathEndFrame << mPathStartFrame;
    bs << mMultiMesh << mParticleSys;
END_SAVES

BEGIN_COPYS(RndGenerator)
    CREATE_COPY_AS(RndGenerator, d)
    MILO_ASSERT(d, 48);
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndTransformable)
    COPY_SUPERCLASS(RndDrawable)
    COPY_SUPERCLASS(RndAnimatable)
    ResetInstances();
    if (ty == kCopyFromMax)
        return;
    COPY_MEMBER_FROM(d, mMesh)
    COPY_MEMBER_FROM(d, mPath)
    COPY_MEMBER_FROM(d, mRateGenLow)
    COPY_MEMBER_FROM(d, mRateGenHigh)
    COPY_MEMBER_FROM(d, mScaleGenLow)
    COPY_MEMBER_FROM(d, mScaleGenHigh)
    COPY_MEMBER_FROM(d, mPathVarMaxX)
    COPY_MEMBER_FROM(d, mPathVarMaxY)
    COPY_MEMBER_FROM(d, mPathVarMaxZ)
    COPY_MEMBER_FROM(d, mPathEndFrame)
    COPY_MEMBER_FROM(d, mPathStartFrame)
    COPY_MEMBER_FROM(d, mMultiMesh)
    COPY_MEMBER_FROM(d, mParticleSys)
END_COPYS

BEGIN_LOADS(RndGenerator)
    int rev;
    bs >> rev;
    if (rev > 9) {
        Hmx::Object::Load(bs);
    }
    if (rev > 1) {
        RndTransformable::Load(bs);
        RndDrawable::Load(bs);
        RndAnimatable::Load(bs);
    }
    ResetInstances();
    bs >> mMesh >> mPath;
    if (rev < 7) {
        bool bd0;
        bs >> bd0;
        if (!bd0) {
            MILO_NOTIFY("%s no longer supports childOfGen", Name());
        }
    }
    if (rev < 1) {
        bs >> mRateGenHigh;
        bs >> mScaleGenHigh;
    }
    if (rev < 8) {
        ObjPtr<RndCam> cam(this);
        bool bd0;
        int ic0;
        bs >> bd0 >> ic0 >> cam;
    }

    if (rev > 0) {
        bs >> mRateGenLow;
        bs >> mRateGenHigh;
        bs >> mScaleGenLow;
        bs >> mScaleGenHigh;
        bs >> mPathVarMaxX;
        bs >> mPathVarMaxY;
        bs >> mPathVarMaxZ;
        if (rev < 9) {
            mPathVarMaxX *= DEG2RAD;
            mPathVarMaxY *= DEG2RAD;
            mPathVarMaxZ *= DEG2RAD;
        }
    } else {
        mRateGenLow = mRateGenHigh;
        mScaleGenLow = mScaleGenHigh;
        mPathVarMaxX = mPathVarMaxY = mPathVarMaxZ = 0;
    }
    if (rev == 3) {
        int x;
        ObjPtr<Hmx::Object> obj(this, nullptr);
        bs >> obj >> x;
    }
    if (rev > 3 && rev < 0xB) {
        ObjPtr<Hmx::Object> obj(this, nullptr);
        bs >> obj;
    }
    if (rev > 4 && rev < 0xB) {
        bool bd0;
        bs >> bd0;
    }
    if (rev > 5) {
        bs >> mPathEndFrame;
        bs >> mPathStartFrame;
    } else {
        if (mPath)
            mPathEndFrame = mPath->EndFrame();
        mPathStartFrame = 0;
    }
    if (rev > 6) {
        bs >> mMultiMesh >> mParticleSys;
    }
END_LOADS

void RndGenerator::Print() {
    TheDebug << "   path: " << mPath << "\n";
    TheDebug << "   mesh: " << mMesh << "\n";
    TheDebug << "   rateGenLow: " << mRateGenLow << "\n";
    TheDebug << "   rateGenHigh: " << mRateGenHigh << "\n";
    TheDebug << "   scaleGenLow: " << mScaleGenLow << "\n";
    TheDebug << "   scaleGenHigh:" << mScaleGenHigh << "\n";
    TheDebug << "   pathVarMax: (" << mPathVarMaxX << ", " << mPathVarMaxY << ", "
             << mPathVarMaxZ << ")\n";
    TheDebug << "   multiMesh: " << mMultiMesh << "\n";
    TheDebug << "   particleSys: " << mParticleSys << "\n";
}

float RndGenerator::StartFrame() {
    if (mPath)
        return mPath->StartFrame();
    return 0;
}

float RndGenerator::EndFrame() {
    if (mPath)
        return mPath->EndFrame();
    return 0;
}

void RndGenerator::SetFrame(float frame, float blend) {
    RndAnimatable::SetFrame(frame, blend);
    if (mNextFrameGen == -9999999.0f) {
        mNextFrameGen = frame;
    } else {
        int dir = mPathEndFrame - mPathStartFrame > 0 ? 1 : -1;
        mCurParticle = mParticleSys ? mParticleSys->ActiveParticles() : NULL;
        for (std::list<Instance>::iterator it = mInstances.begin();
             it != mInstances.end();) {
            float elapsed = frame - it->startFrame;
            if (elapsed > (float)dir * (mPathEndFrame - mPathStartFrame) || elapsed < 0) {
                if (elapsed < 0)
                    mNextFrameGen = it->startFrame;
                it = mInstances.erase(it);
                if (mCurParticle) {
                    mCurParticle = mParticleSys->FreeParticle(mCurParticle);
                }
            } else {
                ++it;
                if (mCurParticle)
                    mCurParticle = mCurParticle->next;
            }
        }
        if (mRateGenLow < 0)
            return;
        float fabs = std::fabs(mPathEndFrame - mPathStartFrame);
        if (frame - fabs > mNextFrameGen) {
            mNextFrameGen = frame - fabs;
        }
        if (frame + mRateGenHigh < mNextFrameGen) {
            mNextFrameGen = frame + mRateGenHigh;
        }
        while (frame >= mNextFrameGen) {
            Generate(mNextFrameGen);
            mNextFrameGen += RandomFloat(mRateGenLow, mRateGenHigh);
        }
    }
}

void RndGenerator::ListAnimChildren(std::list<RndAnimatable *> &list) const {
    if (mPath)
        list.push_back(mPath);
}

void RndGenerator::UpdateSphere() {
    Sphere s;
    MakeWorldSphere(s, true);
    Transform xfm;
    FastInvert(WorldXfm(), xfm);
    Multiply(s, xfm, s);
    SetSphere(s);
}

void RndGenerator::ListDrawChildren(std::list<RndDrawable *> &list) {
    if (mMesh)
        list.push_back(mMesh);
    if (mMultiMesh)
        list.push_back(mMultiMesh);
    if (mParticleSys)
        list.push_back(mParticleSys);
}

void RndGenerator::ResetInstances() {
    mInstances.clear();
    if (mParticleSys)
        mParticleSys->Exit();
    if (mMultiMesh)
        mMultiMesh->Instances().clear();
}

void RndGenerator::Generate(float frame) {
    Instance inst;
    inst.xfm.Reset();
    float rx = 0;
    float ry = 0;
    float rz = 0;
    inst.startFrame = frame;
    if (mPathVarMaxX > 0)
        rx = RandomFloat(-mPathVarMaxX, mPathVarMaxX);
    if (mPathVarMaxY > 0)
        ry = RandomFloat(-mPathVarMaxY, mPathVarMaxY);
    if (mPathVarMaxZ > 0)
        rz = RandomFloat(-mPathVarMaxZ, mPathVarMaxZ);
    Vector3 angles(rx, ry, rz);
    MakeRotMatrix(angles, inst.xfm.m, true);
    Multiply(inst.xfm, WorldXfm(), inst.xfm);
    float scale = mScaleGenLow;
    if (scale < mScaleGenHigh)
        scale = RandomFloat(scale, mScaleGenHigh);
    inst.scale.Set(scale, scale, scale);
    mInstances.push_front(inst);
    if (mParticleSys) {
        mCurParticle = mParticleSys->AllocParticle();
        mParticleSys->InitParticle(mCurParticle, NULL);
    }
}

void RndGenerator::DrawMesh(Transform &t, float) {
    mMesh->SetWorldXfm(t);
    mMesh->Draw();
}

void RndGenerator::DrawMultiMesh(Transform &t, float f) {
    *mCurMultiMesh++ = RndMultiMesh::Instance(t);
}

void RndGenerator::SetPath(RndTransAnim *path, float start, float end) {
    mPath = path;
    if (mPath && start == -1) {
        mPathStartFrame = mPath->StartFrame();
    } else
        mPathStartFrame = start;
    if (mPath && end == -1) {
        mPathEndFrame = mPath->EndFrame();
    } else
        mPathEndFrame = end;
}

DataNode RndGenerator::OnSetPath(const DataArray *da) {
    SetPath(da->Obj<RndTransAnim>(2), -1, -1);
    return 0;
}

DataNode RndGenerator::OnSetRateVar(const DataArray *da) {
    SetRateVar(da->Float(2), da->Float(3));
    return 0;
}

DataNode RndGenerator::OnSetScaleVar(const DataArray *da) {
    SetScaleVar(da->Float(2), da->Float(3));
    return 0;
}

DataNode RndGenerator::OnSetPathVar(const DataArray *da) {
    SetPathVar(da->Float(2), da->Float(3), da->Float(4));
    return 0;
}

DataNode RndGenerator::OnGenerate(const DataArray *da) {
    Generate(GetFrame());
    return 0;
}

void RndGenerator::DrawParticleSys(Transform &t, float) {
    if (mCurParticle) {
        *(Vector3 *)&mCurParticle->pos = t.v;
        mCurParticle = mCurParticle->next;
    }
}

typedef void (RndGenerator::*DrawFunc)(Transform &, float);

void RndGenerator::DrawShowing() {
    auto& _ref1 = mMesh;
    auto& _ref2 = mParticleSys;
    if (mPath && (_ref1 || mMultiMesh || _ref2)) {
        DrawFunc func = nullptr;
        if (_ref1)
            func = &RndGenerator::DrawMesh;
        else if (mMultiMesh) {
            RndMultiMesh::InstanceList &meshInsts = mMultiMesh->Instances();
            if (meshInsts.size() != mInstances.size()) {
                meshInsts.resize(mInstances.size());
            }
            mCurMultiMesh = meshInsts.begin();
            func = &RndGenerator::DrawMultiMesh;
        } else if (_ref2) {
            mCurParticle = _ref2->ActiveParticles();
            func = &RndGenerator::DrawParticleSys;
        }

        if (func) {
            int dir = mPathEndFrame - mPathStartFrame > 0 ? 1 : -1;
            {
                auto it = mInstances.begin();
                if (it != mInstances.end()) {
                    do {
                        float elapsed = GetFrame() - (*it).startFrame;
                        Transform xfm;
                        mPath->MakeTransform(elapsed * dir + mPathStartFrame, xfm, true, 1);
                        Scale(it->scale, xfm.m, xfm.m);
                        Multiply(xfm, it->xfm, xfm);
                        (this->*func)(xfm, elapsed);
                        ++it;
                    } while (it != mInstances.end());
                }
            }
        }

        if (mMultiMesh) {
            mMultiMesh->Draw();
        } else if (_ref2) {
            _ref2->Draw();
        }
    }
}

bool RndGenerator::MakeWorldSphere(Sphere &sphere, bool b) {
    if (b) {
        sphere.Zero();
        if (mPath) {
            CalcSphere(mPath, sphere);
            if (sphere.GetRadius()) {
                RndMesh *mesh = mMesh;
                if (!mesh && mMultiMesh) {
                    mesh = mMultiMesh->Mesh();
                }
                if (mesh) {
                    float sum =
                        mMesh->GetSphere().GetRadius() + Length(mMesh->GetSphere().center);
                    sphere.radius += mScaleGenHigh * sum;
                } else if (mParticleSys) {
                    sphere.radius += mScaleGenHigh
                        * Max(mParticleSys->StartSize().x, mParticleSys->StartSize().y);
                }
            }
        }
        return true;
    } else {
        if (mSphere.GetRadius() != 0.0f) {
            Multiply(mSphere, WorldXfm(), sphere);
            return true;
        }
        return false;
    }
}
