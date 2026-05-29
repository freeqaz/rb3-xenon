#include "world/FreeCamera.h"
#include "world/Dir.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "math/Rot.h"
#include "math/Trig.h"
#include "os/Joypad.h"
#include "rndobj/Cam.h"
#include "rndobj/DOFProc.h"
#include <math.h>

extern float gUnitsPerMeter;

FreeCamera::FreeCamera(WorldDir *dir, float f1, float f2, int i)
    : mParent(0), mFrozen(0), mPadNum(i), mRotateRate(f1), mSlewRate(gUnitsPerMeter * f2),
      mUseParentRotateX(1), mUseParentRotateY(1), mUseParentRotateZ(1), mWorld(dir) {
    UpdateFromCamera();
}

BEGIN_HANDLERS(FreeCamera)
    HANDLE_ACTION(set_parent, mParent = _msg->Obj<RndTransformable>(2))
    HANDLE_ACTION(set_pos, mXfm.v.Set(_msg->Float(2), _msg->Float(3), _msg->Float(4)))
    HANDLE_ACTION(
        set_rot,
        mRot.Set(
            _msg->Float(2) * DEG2RAD, _msg->Float(3) * DEG2RAD, _msg->Float(4) * DEG2RAD
        )
    )
    HANDLE_ACTION(set_parent_dof, SetParentDof(_msg->Int(2), _msg->Int(3), _msg->Int(4)))
    HANDLE_ACTION(set_frozen, mFrozen = _msg->Int(2))
END_HANDLERS

void FreeCamera::SetParentDof(bool b1, bool b2, bool b3) {
    mUseParentRotateX = b1;
    mUseParentRotateY = b2;
    mUseParentRotateZ = b3;
}

void FreeCamera::Poll() {
    JoypadData *padData = JoypadGetPadData(mPadNum);
    if (!padData)
        return;

    float deltaMs = TheTaskMgr.DeltaUISeconds() * 1000.0f;
    if (mFrozen) {
        deltaMs = 0.0f;
    }

    float rotSpeed = mRotateRate * deltaMs;

    // Apply left stick to rotation
    float lx = padData->mSticks[0][0];
    float ly = padData->mSticks[0][1];
    mRot.z = LimitAng((-fabsf(lx) * (rotSpeed * lx)) + mRot.z);
    mRot.x = LimitAng(mRot.x + fabsf(ly) * rotSpeed * ly);

    // Rebuild rotation matrix
    MakeRotMatrix(mRot, mXfm.m, true);

    // Compute slew speed
    float slewSpeed = mSlewRate * deltaMs;
    if (padData->mButtons & (1 << kPad_L2)) {
        slewSpeed *= 0.1f;
    }

    float rx = padData->mSticks[1][0];
    float ry = padData->mSticks[1][1];
    float slewX = fabsf(rx * rx) * rx * slewSpeed * 0.5f;
    float slewY = -(fabsf(ry * ry) * ry * slewSpeed);

    // Move along X axis (strafe)
    mXfm.v.y += mXfm.m.x.y * slewX;
    mXfm.v.x += mXfm.m.x.x * slewX;
    mXfm.v.z += mXfm.m.x.z * slewX;

    // Move along Y (forward) or Z (up) depending on LB
    if (padData->mButtons & (1 << kPad_L1)) {
        // L1/LB pressed - move along Z axis (up/down)
        mXfm.v.x += mXfm.m.z.x * slewY;
        mXfm.v.y += mXfm.m.z.y * slewY;
        mXfm.v.z += mXfm.m.z.z * slewY;
    } else {
        // Move along Y axis (forward/back)
        mXfm.v.x += mXfm.m.y.x * slewY;
        mXfm.v.y += mXfm.m.y.y * slewY;
        mXfm.v.z += mXfm.m.y.z * slewY;
    }

    RndCam *cam = mWorld->Cam();

    // FOV adjustment with D-pad Up/Down
    if (padData->mButtons & (1 << kPad_DUp)) {
        mFov = mFov + 0.001f;
    } else if (padData->mButtons & (1 << kPad_DDown)) {
        mFov = mFov - 0.001f;
    }

    unsigned int buttons = padData->mButtons;
    if (buttons & (1 << kPad_X)) {
        // A button - roll rotation
        if (buttons & (1 << kPad_DLeft)) {
            mRot.y = deltaMs * 0.001f + mRot.y;
        } else if (buttons & (1 << kPad_DRight)) {
            mRot.y = -(deltaMs * 0.001f - mRot.y);
        }
    } else {
        // Focal plane adjustment
        if (buttons & (1 << kPad_DLeft)) {
            mFocalPlane = mFocalPlane / (float)pow(2.0, deltaMs * 0.001f);
        } else if (buttons & (1 << kPad_DRight)) {
            mFocalPlane = mFocalPlane * (float)pow(2.0, deltaMs * 0.001f);
        }
    }

    // Apply parent transform
    Transform resultXfm;
    if (mParent) {
        if (!mUseParentRotateX || !mUseParentRotateY || !mUseParentRotateZ) {
            Hmx::Matrix3 parentRot;
            memcpy(&parentRot, &mParent->WorldXfm(), 0x30);
            Vector3 parentEuler(0.0f, 0.0f, 0.0f);
            MakeEuler(parentRot, parentEuler);
            if (!mUseParentRotateX) {
                parentEuler.x = mRot.x;
            }
            if (!mUseParentRotateY) {
                parentEuler.y = mRot.y;
            }
            if (!mUseParentRotateZ) {
                parentEuler.z = mRot.z;
            }
            Hmx::Matrix3 newRot;
            MakeRotMatrix(parentEuler, newRot, false);
            const Transform &parentWorld = mParent->WorldXfm();
            Transform parentXfm;
            memcpy(&parentXfm, &newRot, 0x30);
            parentXfm.v = parentWorld.v;
            Multiply(mXfm, parentXfm, resultXfm);
        } else {
            Multiply(mXfm, mParent->WorldXfm(), resultXfm);
        }
    } else {
        memcpy(&resultXfm, &mXfm, 0x40);
    }

    cam->SetFrustum(cam->NearPlane(), cam->FarPlane(), mFov, 1.0f);

    // If camera has a parent transform, convert to local space
    RndTransformable *camParent = cam->TransParent();
    if (camParent) {
        Transform invParent;
        Invert(camParent->WorldXfm(), invParent);
        Multiply(resultXfm, invParent, resultXfm);
    }

    cam->SetLocalXfm(resultXfm);

    // Handle DOF
    if (TheDOFProc->Enabled()) {
        TheDOFProc->Set(
            cam, mFocalPlane, TheDOFProc->MinBlur(), TheDOFProc->MaxBlur(),
            TheDOFProc->BlurDepth()
        );
    }
}

void FreeCamera::UpdateFromCamera() {
    RndCam *cam = mWorld->Cam();
    mFov = cam->YFov();
    mXfm = cam->WorldXfm();
    MakeEuler(mXfm.m, mRot);
    mParent = 0;
    mFocalPlane = TheDOFProc->FocalPlane();
}
