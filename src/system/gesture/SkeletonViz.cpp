#include "gesture/SkeletonViz.h"
#include "SkeletonViz.h"
#include "gesture/BaseSkeleton.h"
#include "hamobj/HamCharacter.h"
#include "math/Geo.h"
#include "math/Mtx.h"
#include "math/Rot.h"
#include "math/Utl.h"
#include "math/Vec.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "os/File.h"
#include "rndobj/Cam.h"
#include "rndobj/Draw.h"
#include "rndobj/Env.h"
#include "rndobj/Line.h"
#include "rndobj/Mat.h"
#include "rndobj/Utl.h"
#include "rndobj/Poll.h"
#include "rndobj/Trans.h"
#include "utl/BinStream.h"
#include "utl/Loader.h"
#include <algorithm>

SkeletonViz::SkeletonViz()
    : mUsePhysicalCam(0), mPhysicalCamRotation(0), mCurrentCamRotation(0),
      mAxesCoordSys(kCoordCamera), mUtlLine(0), mSkeletonEnv(0), mCamMesh(0),
      mJointMesh(0), mJointMat(0), mPhysicalCam(0), mLineWidthScale(0),
      unk218(true) {
    unk194.Reset();
    Hmx::Matrix3 rot(Vector3(1, 0, 0), Vector3(0, 0, 1), Vector3(0, 1, 0));
    Multiply(rot, unk194.m, unk194.m);
    unk1d4 = unk194;
    for (int i = 0; i < kNumBones; i++) {
        mBoneLines[i] = nullptr;
    }
}

SkeletonViz::~SkeletonViz() {
    for (int i = 0; i < kNumBones; i++) {
        delete mBoneLines[i];
    }
}

BEGIN_HANDLERS(SkeletonViz)
    HANDLE_ACTION(rotate, Rotate(_msg->Float(2)))
    HANDLE_SUPERCLASS(RndTransformable)
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(SkeletonViz)
    SYNC_PROP(use_physical_cam, mUsePhysicalCam)
    SYNC_PROP_SET(
        physical_cam_rotation, mPhysicalCamRotation, SetPhysicalCamRotation(_val.Float())
    )
    SYNC_PROP_SET(
        axes_coord_sys, mAxesCoordSys, SetAxesCoordSys((SkeletonCoordSys)_val.Int())
    )
    SYNC_SUPERCLASS(RndTransformable)
    SYNC_SUPERCLASS(RndDrawable)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(SkeletonViz)
    SAVE_REVS(6, 1)
    SAVE_SUPERCLASS(RndPollable)
    SAVE_SUPERCLASS(RndDrawable)
    SAVE_SUPERCLASS(RndTransformable)
    bs << mUsePhysicalCam;
    bs << mAxesCoordSys;
    bs << mPhysicalCamRotation;
END_SAVES

BEGIN_COPYS(SkeletonViz)
    COPY_SUPERCLASS(RndPollable)
    COPY_SUPERCLASS(RndDrawable)
    COPY_SUPERCLASS(RndTransformable)
    CREATE_COPY(SkeletonViz)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mUsePhysicalCam)
        COPY_MEMBER(mAxesCoordSys)
        COPY_MEMBER(mPhysicalCamRotation)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_LOADS(SkeletonViz)
    PreLoad(bs);
    PostLoad(bs);
END_LOADS

INIT_REVS(6, 1)

void SkeletonViz::PreLoad(BinStream &bs) {
    LOAD_REVS(bs)
    ASSERT_REVS(6, 1)
    if (d.rev > 5) {
        Hmx::Object::Load(bs);
    }
    RndDrawable::Load(bs);
    RndTransformable::Load(bs);
    if (d.rev > 0) {
        d >> mUsePhysicalCam;
    }
    if (d.rev > 1) {
        int cs;
        d >> cs;
        mAxesCoordSys = (SkeletonCoordSys)cs;
    }
    if (d.rev > 2 && d.rev < 4) {
        int x, y;
        d >> x;
        d >> y;
    }
    if (d.altRev > 0) {
        d >> mPhysicalCamRotation;
    }
    mCurrentCamRotation = mPhysicalCamRotation;
    if (d.rev > 4 && d.altRev < 1) {
        ObjPtr<HamCharacter> hChar(this);
        d >> hChar;
    }
    if (TheLoadMgr.EditMode()) {
        LoadResource(true);
    }
}

void SkeletonViz::PostLoad(BinStream &bs) {
    if (TheLoadMgr.EditMode()) {
        mResource.PostLoad(nullptr);
        UpdateResource();
    }
}

float SkeletonViz::PhysicalCamRotation() const { return mPhysicalCamRotation; }
void SkeletonViz::SetUsePhysicalCam(bool use) { mUsePhysicalCam = use; }
void SkeletonViz::SetPhysicalCamRotation(float rotation) {
    mPhysicalCamRotation = rotation;
    mCurrentCamRotation = rotation;
}
void SkeletonViz::Rotate(float amt) { mCurrentCamRotation += amt; }
void SkeletonViz::SetAxesCoordSys(SkeletonCoordSys cs) { mAxesCoordSys = cs; }

void SkeletonViz::Init() {
    if (!mResource) {
        for (int i = 0; i < kNumBones; i++) {
            mBoneLines[i] = Hmx::Object::New<RndLine>();
        }
        LoadResource(false);
        UpdateResource();
    }
}

void SkeletonViz::LoadResource(bool postload) {
    static Symbol objects("objects");
    mResource.LoadFile(
        FilePath(FileSystemRoot(), "ham/skeleton.milo"), postload, true, kLoadFront, false
    );
    if (!postload) {
        mResource.PostLoad(nullptr);
    }
}

void SkeletonViz::UpdateResource() {
    Transform xfm;
    xfm.Reset();
#ifdef HX_NATIVE
    if (!mResource.IsLoaded()) {
        MILO_LOG("SkeletonViz::UpdateResource - skeleton resource not loaded, skipping\n");
        return;
    }
#else
    MILO_ASSERT(mResource.IsLoaded(), 0x1E8);
#endif
    mSkeletonEnv = mResource->Find<RndEnviron>("skeleton.env", true);
    mCamMesh = mResource->Find<RndMesh>("camera.mesh", true);
    mCamMesh->SetTransParent(this, false);
    mCamMesh->SetLocalPos(xfm.v);
    mPhysicalCam = mResource->Find<RndCam>("physical.cam", true);
    mPhysicalCam->SetTransParent(this, false);
    mPhysicalCam->SetLocalXfm(xfm);
    mJointMesh = mResource->Find<RndMesh>("joint.mesh", true);
    mJointMesh->SetTransParent(this, false);
    mJointMesh->SetLocalPos(xfm.v);
    mJointMat = mResource->Find<RndMat>("joint.mat", true);
    mUtlLine = mResource->Find<RndLine>("utl.line", true);
    mUtlLine->SetTransParent(this, false);
    mUtlLine->SetLocalXfm(xfm);
    mSphereMesh = mResource->Find<RndMesh>("sphere.mesh", true);
    RndLine *boneLine = mResource->Find<RndLine>("bone.line", true);
    for (int i = 0; i < kNumBones; i++) {
        if (!mBoneLines[i])
            mBoneLines[i] = Hmx::Object::New<RndLine>();
        mBoneLines[i]->Copy(boneLine, kCopyShallow);
        mBoneLines[i]->SetTransParent(this, false);
        mBoneLines[i]->SetLocalXfm(xfm);
    }
}

void SkeletonViz::SetPhysicalCamScreenRect(const Hmx::Rect &r) {
    MILO_ASSERT(r.x >= 0 && r.y >= 0 && r.w > 0 && r.h > 0, 0x64);
    MILO_ASSERT(mPhysicalCam, 0x65);
    mPhysicalCam->SetScreenRect(r);
}

void SkeletonViz::DrawLine3D(
    const Vector3 &vec1,
    const Vector3 &vec2,
    float f,
    const Hmx::Color &color1,
    Hmx::Color *color2
) {
    Vector3 localVec1, localVec2;
    Multiply(vec1, unk1d4, localVec1);
    Multiply(vec2, unk1d4, localVec2);
    mUtlLine->SetPointPos(0, localVec1);
    mUtlLine->SetPointPos(1, localVec2);
    RndMat *mat = mUtlLine->Mat();
    MILO_ASSERT(mat, 0x178);

    if (!color2) {
        mat->SetColor(color1.red, color1.green, color1.blue);
    } else {
        mUtlLine->SetMat(0);
        mUtlLine->SetPointColor(0, *color2, true);
        mUtlLine->SetPointColor(1, color1, true);
    }
    mUtlLine->SetWidth(mLineWidthScale * f);
    mUtlLine->DrawShowing();
    mUtlLine->SetMat(mat);
}

void SkeletonViz::Poll() {
    if (mPhysicalCamRotation < mCurrentCamRotation) {
        mPhysicalCamRotation += TheTaskMgr.DeltaUISeconds() * 120.0f;
        if (mPhysicalCamRotation <= mCurrentCamRotation) {
            return;
        }
    } else {
        if (mPhysicalCamRotation <= mCurrentCamRotation) {
            return;
        }
        mPhysicalCamRotation -= TheTaskMgr.DeltaUISeconds() * 120.0f;
        if (mPhysicalCamRotation >= mCurrentCamRotation) {
            return;
        }
    }
    mPhysicalCamRotation = mCurrentCamRotation;
}

void SkeletonViz::SetCamera(
    const SkeletonFrame &frame, const Transform &worldXfm, float distance
) {
    if (mUsePhysicalCam) {
        if (unk218) {
            Vector3 pos;
            pos.x = 0.0f;
            pos.y = -distance;
            pos.z = 0.0f;
            RotateAboutZ(pos, mPhysicalCamRotation * DEG2RAD, pos);
            pos.y += distance;
            mPhysicalCam->SetLocalPos(pos);
            float tiltRad = frame.TiltAngle();
            float tilt = tiltRad * RAD2DEG;
            mPhysicalCam->SetLocalRot(Vector3(tilt, 0.0f, mPhysicalCamRotation));
            mPhysicalCam->SetFrustum(0.01f, 10.0f, 0.7955211f, 1.0f);
        } else {
            Transform xfm;
            xfm.Reset();
            xfm.v = unk1d4.v;
            mPhysicalCam->SetFrustum(0.5f, 1000.0f, mPhysicalCam->YFov(), 1.0f);
            mPhysicalCam->SetLocalXfm(xfm);
        }
        mPhysicalCam->Select();
    } else {
        if (mAxesCoordSys == kCoordCamera || !unk218) {
            UtilDrawAxes(
                worldXfm, 5.0f / mLineWidthScale, Hmx::Color(1.0f, 1.0f, 1.0f, 1.0f)
            );
        }
        if (unk218) {
            mCamMesh->SetWorldPos(worldXfm.v);
            mCamMesh->DrawShowing();
            Vector3 normal;
            Multiply(frame.mFloorNormal, unk1d4.m, normal);
            normal.x += worldXfm.v.x;
            normal.y += worldXfm.v.y;
            normal.z += worldXfm.v.z;
            TheRnd.DrawLine(
                worldXfm.v, normal, Hmx::Color(1.0f, 1.0f, 1.0f, 1.0f), false
            );
        }
    }

    if (unk218) {
        Plane plane;
        memcpy(&plane, &frame.mFloorClipPlane, sizeof(Plane));
        Transform localXfm = unk1d4;
        localXfm.v = worldXfm.v;
        Multiply(plane, localXfm, plane);
        Vector3 planePos = worldXfm.v;
        planePos.y += distance;
        UtilDrawPlane(
            plane, planePos, Hmx::Color(1.0f, 1.0f, 0.0f, 1.0f), 5, 0.5f, false
        );
    }
}

void SkeletonViz::DrawPoint3D(
    const Vector3 &vec, float scale, const Hmx::Color &color, float alpha
) {
    Vector3 point;
    Multiply(vec, unk1d4, point);
    if (unk218) {
        Multiply(point, WorldXfm(), point);
    }

    float scaled = mLineWidthScale * scale;
    mSphereMesh->Mat()->SetColor(color.red, color.green, color.blue);
    mSphereMesh->Mat()->SetAlpha(alpha);
    mSphereMesh->Mat()->SetCull(kCullNone);

    Transform sphereXfm;
    sphereXfm.Reset();
    sphereXfm.v = point;
    Scale(Vector3(scaled, scaled, scaled), sphereXfm.m, sphereXfm.m);
    mSphereMesh->SetLocalXfm(sphereXfm);
    mSphereMesh->SetSphere(Sphere(Vector3(0, 0, 0), scaled));
    mSphereMesh->DrawShowing();
}

void SkeletonViz::DrawJoints(
    const BaseSkeleton &skeleton, Vector3 *camPos, Vector3 *drawPos, bool faded
) {
    float tint = 1.0f;
    if (faded) {
        tint = 0.5f;
    }
    Hmx::Color tintColor(tint, tint, tint, 1.0f);

    float len4 = skeleton.BoneLength((SkeletonBone)4, kCoordCamera);
    float len3 = skeleton.BoneLength((SkeletonBone)3, kCoordCamera);
    len3 += len4;
    float len2 = skeleton.BoneLength((SkeletonBone)2, kCoordCamera);

    float minZ = 1.0e30f;
    Vector3 *camIt = camPos - 1;
    for (int i = 0; i < kNumBones; i++) {
        camIt++;
        float z = camIt->z;
        minZ = Min(minZ, z);
    }

    float maxDepth = minZ + len2 + len3;
    float invRange = 1.0f / (minZ - maxDepth);

    Hmx::Color shadedColor;
    RndLine **lineIt = mBoneLines - 1;
    const BoneJoints *boneIt = BaseSkeleton::sBones + 1;
    const BoneJoints *boneEnd = BaseSkeleton::sBones + kNumBones;
    while (boneIt <= boneEnd) {
        const BoneJoints &bone = boneIt[-1];

        float c0 = (camPos[bone.joint1].z - maxDepth) * invRange;
        c0 = Clamp(0.0f, 1.0f, c0);
        c0 = c0 * 0.8f + 0.2f;

        RndLine *line = lineIt[1];
        shadedColor.red = tintColor.red * c0;
        shadedColor.green = tintColor.green * c0;
        shadedColor.blue = tintColor.blue * c0;
        shadedColor.alpha = tintColor.alpha;
        line->SetPointColor(0, shadedColor, true);

        float c1 = (camPos[bone.joint2].z - maxDepth) * invRange;
        c1 = Clamp(0.0f, 1.0f, c1);
        c1 = c1 * 0.8f + 0.2f;
        shadedColor.red = tintColor.red * c1;
        shadedColor.green = tintColor.green * c1;
        shadedColor.blue = tintColor.blue * c1;
        line->SetPointColor(1, shadedColor, true);

        line->SetPointPos(0, drawPos[bone.joint1]);
        line->SetPointPos(1, drawPos[bone.joint2]);
        float baseWidth = line->GetWidth();
        line->SetWidth(mLineWidthScale * baseWidth);
        line->DrawShowing();
        lineIt++;
        lineIt[0]->SetWidth(baseWidth);
        boneIt++;
    }

    float baseScaleZ = mJointMesh->LocalXfm().m.z.z;
    float baseScaleY = mJointMesh->LocalXfm().m.y.y;
    float baseScaleX = mJointMesh->LocalXfm().m.x.x;
    Vector3 scaledScale(
        mLineWidthScale * baseScaleX,
        mLineWidthScale * baseScaleY,
        mLineWidthScale * baseScaleZ
    );
    SetLocalScale(mJointMesh, scaledScale);

    Vector3 *jointIt = drawPos;
    for (int i = 0; i < kNumJoints; i++) {
        JointConfidence conf = skeleton.JointConf((SkeletonJoint)i);
        float red;
        float green;
        if (conf == kConfidenceTracked) {
            red = 0.0f;
            green = tint;
        } else {
            red = tint;
            if (conf == kConfidenceInferred) {
                green = tint;
            } else {
                green = 0.0f;
            }
        }
        mJointMesh->SetLocalPos(*jointIt);
        mJointMat->SetColor(red, green, 0.0f);
        mJointMesh->DrawShowing();
        jointIt++;
    }

    Vector3 baseScale(baseScaleX, baseScaleY, baseScaleZ);
    SetLocalScale(mJointMesh, baseScale);

    int clippingFlags = skeleton.QualityFlags();
    Hmx::Color textColor(1.0f, 1.0f, 1.0f, 1.0f);
    Vector2 screenPos(0.1f, 0.1f);
    if (mUsePhysicalCam) {
        const Hmx::Rect &screenRect = mPhysicalCam->GetScreenRect();
        float sy = screenRect.y;
        float sx = screenRect.x;
        screenPos.x = sx;
        screenPos.y = sy;
    }
    if (clippingFlags & 1) {
        const Vector2 &sz =
            TheRnd.DrawStringScreen("clipped right", screenPos, textColor, true);
        screenPos.y = sz.y;
    }
    if (clippingFlags & 2) {
        const Vector2 &sz =
            TheRnd.DrawStringScreen("clipped left", screenPos, textColor, true);
        screenPos.y = sz.y;
    }
    if (clippingFlags & 4) {
        const Vector2 &sz =
            TheRnd.DrawStringScreen("clipped top", screenPos, textColor, true);
        screenPos.y = sz.y;
    }
    if (clippingFlags & 8) {
        const Vector2 &sz =
            TheRnd.DrawStringScreen("clipped bottom", screenPos, textColor, true);
        screenPos.y = sz.y;
    }

    if (mAxesCoordSys != kCoordCamera && unk218) {
        Transform axesXfm, worldXfm;
        skeleton.CameraToPlayerXfm(mAxesCoordSys, axesXfm);
        Multiply(axesXfm, unk1d4, worldXfm);
        Multiply(worldXfm, WorldXfm(), worldXfm);
        Hmx::Color white(1.0f, 1.0f, 1.0f, 1.0f);
        UtilDrawAxes(worldXfm, mLineWidthScale * 0.25f, white);
    }
}

void SkeletonViz::Visualize(
    const CameraInput &input,
    const BaseSkeleton &skeleton,
    std::vector<SkeletonCallback *> *callbacks,
    bool faded
) {
    if (!mResource) {
#ifdef HX_NATIVE
        Init();
        if (!mResource.IsLoaded()) return;
#else
        MILO_ASSERT(TheLoadMgr.EditMode(), 0x72);
        Init();
#endif
    }
#ifdef HX_NATIVE
    if (!mResource.IsLoaded()) return;
#else
    MILO_ASSERT(mResource.IsLoaded(), 0x76);
#endif

    RndEnvironTracker environTracker(mSkeletonEnv, nullptr);

    unk218 = !input.NatalToWorld(unk1d4);
    if (unk218) {
        unk1d4 = unk194;
    }
    mLineWidthScale = input.DrawScale();

    Transform worldXfm;
        worldXfm = unk218 ? WorldXfm() : unk1d4;

    const SkeletonFrame &cachedFrame = input.CachedFrame();
    RndCam *currentCam = RndCam::Current();
    if (skeleton.IsTracked()) {
        Vector3 camJointPos[kNumJoints];
        Vector3 drawJointPos[kNumJoints];
        for (int i = 0; i < kNumJoints; i++) {
            skeleton.JointPos(kCoordCamera, (SkeletonJoint)i, camJointPos[i]);
            Multiply(camJointPos[i], unk194, drawJointPos[i]);
        }
        SetCamera(cachedFrame, worldXfm, drawJointPos[kJointShoulderCenter].z);
        DrawJoints(skeleton, camJointPos, drawJointPos, faded);

        if (callbacks) {
            FOREACH (it, *callbacks) {
                (*it)->Draw(skeleton, *this);
            }
        }
    } else {
        SetCamera(cachedFrame, worldXfm, 0.0f);
        if (currentCam) {
            currentCam->Select();
        }
    }
}
