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
    : mParent(0), mFrozen(0), mPadNum(i), mRotateRate(f1), mSlewRate(f2),
      mUseParentRotateX(1), mUseParentRotateY(1), mUseParentRotateZ(1), mWorld(dir),
      mEnableDOF(1) {
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
    HANDLE_ACTION(enable_depth_of_field, mEnableDOF = _msg->Int(2))
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

    // Apply left stick to rotation
    float lx = padData->mSticks[0][0];
    float ly = padData->mSticks[0][1];
    float rotSpeed = mRotateRate * deltaMs;
    float left0rate = -fabsf(lx) * rotSpeed * lx;
    mRot.z = LimitAng(mRot.z + left0rate);
    float left1rate = fabsf(ly) * rotSpeed * ly;
    mRot.x = LimitAng(left1rate + mRot.x);

    // Rebuild rotation matrix
    MakeRotMatrix(mRot, mXfm.m, true);

    // Compute slew speed
    float slewSpeed = mSlewRate * deltaMs;
    if (padData->IsButtonInMask(kPad_L2)) {
        slewSpeed *= 0.1f;
    }

    float rx = padData->mSticks[1][0];
    float ry = padData->mSticks[1][1];
    float slewX = fabsf(rx * rx) * rx * slewSpeed * 0.5f;
    float slewY = -(fabsf(ry * ry) * ry * slewSpeed);

    // Move along X axis (strafe)
    ScaleAddEq(mXfm.v, mXfm.m.x, slewX);

    // Move along Y (forward) or Z (up) depending on LB
    if (padData->IsButtonInMask(kPad_L1)) {
        // L1/LB pressed - move along Z axis (up/down)
        ScaleAddEq(mXfm.v, mXfm.m.z, slewY);
    } else {
        // Move along Y axis (forward/back)
        ScaleAddEq(mXfm.v, mXfm.m.y, slewY);
    }

    RndCam *cam = mWorld->Cam();

    // FOV adjustment with D-pad Up/Down
    if (padData->IsButtonInMask(kPad_DUp)) {
        mFov = mFov + 0.001f;
    } else if (padData->IsButtonInMask(kPad_DDown)) {
        mFov = mFov - 0.001f;
    }

    if (padData->IsButtonInMask(kPad_X)) {
        // A button - roll rotation
        if (padData->IsButtonInMask(kPad_DLeft)) {
            mRot.y = deltaMs * 0.001f + mRot.y;
        } else if (padData->IsButtonInMask(kPad_DRight)) {
            mRot.y = -(deltaMs * 0.001f - mRot.y);
        }
    } else {
        // Focal plane adjustment
        if (padData->IsButtonInMask(kPad_DLeft)) {
            mFocalPlane /= powf(2.0f, deltaMs * 0.001f);
        } else if (padData->IsButtonInMask(kPad_DRight)) {
            mFocalPlane *= powf(2.0f, deltaMs * 0.001f);
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
    if (cam->TransParent()) {
        Transform invParent;
        Invert(cam->TransParent()->WorldXfm(), invParent);
        Multiply(resultXfm, invParent, resultXfm);
    }

    cam->SetLocalXfm(resultXfm);

    // Handle DOF. Retail (0x824ED954..0x824ED9E4) gates the Set on mEnableDOF and
    // calls UnSet() otherwise (vtable slot 0x58). The float arg order is fixed by
    // retail's register setup: f2=BlurDepth, f3=MaxBlur, f4=MinBlur -- and MSVC's
    // right-to-left evaluation matches (MinBlur is issued first, so it is rightmost).
    if (TheDOFProc->Enabled()) {
        if (mEnableDOF) {
            TheDOFProc->Set(
                cam, mFocalPlane, TheDOFProc->BlurDepth(), TheDOFProc->MaxBlur(),
                TheDOFProc->MinBlur()
            );
        } else {
            TheDOFProc->UnSet();
        }
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
