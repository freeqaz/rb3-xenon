#include "rndobj/TexProc.h"
#include "Rnd.h"
#include "ShaderMgr.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "rndobj/BaseMaterial.h"
#include "rndobj/Cam.h"
#include "rndobj/Draw.h"
#include "rndobj/Mat.h"
#include "rndobj/Rnd_NG.h"
#include "rndobj/Tex.h"
#include "utl/BinStream.h"
#include <cmath>

float gAmpTemp = 0.3f;
float gFreqTemp = 1.0f;
RndCam *TexProc::mCam;

TexProc::TexProc()
    : mInputTex(this), mOutputTex(this), mShaderType(kShaderTwirl), mDrawPreClear(1),
      mStoredParams(0), mFrequency(0), unk78(0), mAmplitude(0), mAmplitudeBump(0), mPhase(0),
      mPhaseVel(0) {}

BEGIN_HANDLERS(TexProc)
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE(set_params, OnSetParams)
END_HANDLERS

BEGIN_PROPSYNCS(TexProc)
    SYNC_PROP(input_texture, mInputTex)
    SYNC_PROP(output_texture, mOutputTex)
    SYNC_PROP(shader, (int &)mShaderType)
    SYNC_PROP_MODIFY(draw_pre_clear, mDrawPreClear, UpdatePreClearState())
    SYNC_PROP(frequency, mFrequency)
    SYNC_PROP(amplitude, mAmplitude)
    SYNC_PROP(amplitude_bump, mAmplitudeBump)
    SYNC_PROP(phase, mPhase)
    SYNC_PROP(phase_vel, mPhaseVel)
    SYNC_SUPERCLASS(RndDrawable)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(TexProc)
    SAVE_REVS(3, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndDrawable)
    bs << mInputTex;
    bs << mOutputTex;
    bs << mShaderType;
    bs << mDrawPreClear;
    bs << mFrequency;
    bs << mAmplitude;
    bs << mAmplitudeBump;
    bs << mPhaseVel;
END_SAVES

BEGIN_COPYS(TexProc)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndDrawable)
    CREATE_COPY(TexProc)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mInputTex)
        COPY_MEMBER(mOutputTex)
        COPY_MEMBER(mShaderType)
        COPY_MEMBER(mDrawPreClear)
        COPY_MEMBER(mFrequency)
        COPY_MEMBER(unk78)
        COPY_MEMBER(mPhase)
        COPY_MEMBER(mPhaseVel)
        COPY_MEMBER(mAmplitude)
        COPY_MEMBER(mAmplitudeBump)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(3, 0)

BEGIN_LOADS(TexProc)
    LOAD_REVS(bs)
    ASSERT_REVS(3, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(RndDrawable)
    bs >> mInputTex;
    bs >> mOutputTex;
    bs >> (int &)mShaderType;
    d >> mDrawPreClear;
    mDrawPreClear = true;
    if (d.rev > 1) {
        bs >> mFrequency;
        bs >> mAmplitude;
        if (d.rev > 2) {
            bs >> mAmplitudeBump;
        } else {
            float x;
            bs >> x;
        }
        bs >> mPhaseVel;
    }
END_LOADS

void TexProc::UpdatePreClearState() {
    TheRnd.PreClearDrawAddOrRemove(this, mDrawPreClear, false);
}

void TexProc::Init() {
    REGISTER_OBJ_FACTORY(TexProc);
    MILO_ASSERT(!mCam, 0x3A);
    mCam = ObjectDir::Main()->New<RndCam>("[tex proc cam]");
}

RndMat *TexProc::SetUpWorkingMat() {
    RndMat *mat = TheShaderMgr.GetWork();
    mat->SetZMode(kZModeDisable);
    mat->SetBlend(BaseMaterial::kBlendSrc);
    mat->SetTexWrap(kTexWrapClamp);
    mat->SetAlphaWrite(true);
    mat->SetAlphaCut(false);
    return mat;
}

void TexProc::SetRegisters() {
    TheShaderMgr.SetPConstant(
        kPS_TexProcFrequency,
        Vector4(
            mFrequency * gFreqTemp,
            mFrequency * gFreqTemp,
            mFrequency * gFreqTemp,
            mFrequency * gFreqTemp
        )
    );
    TheShaderMgr.SetPConstant(
        kPS_TexProcAmplitude,
        Vector4(unk78 * gAmpTemp, unk78 * gAmpTemp, unk78 * gAmpTemp, unk78 * gAmpTemp)
    );
    TheShaderMgr.SetPConstant(
        kPS_TexProcPhase, Vector4(mPhase, mPhase, mPhase, mPhase)
    );
}

DataNode TexProc::OnSetParams(DataArray *a) {
    if (CheckParams(a, true)) {
        if (mStoredParams)
            mStoredParams->Release();
        mStoredParams = new DataArray(a->Size() - 2);
        SetParams(mStoredParams, a);
    } else {
        MILO_NOTIFY("----- TexProc::OnSetParams() - one or more parameters is invalid");
    }
    return 0;
}

bool TexProc::CheckParams(DataArray *a, bool b) {
    int aSize = a->Size();
    bool b7 = true;
    if (!b && (aSize < 1 || aSize > 4))
        b7 = false;
    for (int i = b ? 2 : 0; i < aSize && b7; i++) {
        switch (a->Type(i)) {
        case kDataFloat:
            break;
        case kDataArray:
            b7 &= CheckParams(a->Array(i), false);
            break;
        case kDataProperty: {
            DataNode eval = a->Evaluate(i);
            if (eval.Type() != kDataFloat) {
                if (eval.Type() != kDataArray) {
                    b7 = false;
                } else {
                    b7 &= CheckParams(eval.Array(), false);
                }
            }
            break;
        }
        default:
            b7 = false;
            break;
        }
    }
    return b7;
}

void TexProc::Poll() {
    float beat = TheTaskMgr.Beat();
    mPhase = mPhaseVel * beat * 6.2831855f;
    float frac = beat - (float)(int)beat;
    float t = (1.0f - frac) - 0.5f;
    if (t < 0.0f)
        t = 0.0f;
    float s = sinf(t * 6.2831855f);
    unk78 = mAmplitude + mAmplitudeBump * s * t;
}

void TexProc::DrawToTexture() {
    if (TheRnd.GetDrawMode() != 0)
        return;
    if (!Showing() || !mInputTex || !mOutputTex)
        return;

    RndMat *mat = SetUpWorkingMat();

    ShaderType shaderType = kKillAlphaShader;
    if ((unsigned int)mShaderType < 1u || mShaderType != kShaderKillAlpha) {
        shaderType = kTwirlShader;
    }

    RndCam *prevCam = RndCam::Current();
    RndTex *targetTex = prevCam->TargetTex();
    if (targetTex != 0) {
        MILO_NOTIFY_ONCE(
            "%s: Cannot render to texture (%s) while already rendering to texture (%s).",
            PathName(targetTex),
            PathName(this),
            PathName(targetTex)
        );
    }

    mCam->SetTargetTex(mOutputTex);
    mCam->Select();

    mat->SetDiffuseTex(mInputTex);
    mat->SetNormalMap(mInputTex);

    Poll();
    SetRegisters();

    int h = mOutputTex->Height();
    Hmx::Rect rect;
    rect.x = 0;
    rect.y = 0;
    rect.w = (float)mOutputTex->Width();
    rect.h = (float)h;
    TheNgRnd.DrawRect(rect, mat, shaderType, Hmx::Color(1, 1, 1), nullptr, nullptr);

    mCam->SetTargetTex(nullptr);
    prevCam->Select();
}

void TexProc::SetParams(DataArray *a1, DataArray *a2) {
    int aSize = a2->Size();
    for (int i = 2; i < aSize; i++) {
        switch (a2->Type(i)) {
        case kDataFloat:
            a1->Node(i - 2) = a2->Float(i);
            break;
        case kDataArray: {
            DataArray *arr = new DataArray(a2->Array(i)->Size());
            SetParams(arr, a2->Array(i));
            a1->Node(i - 2) = a2->Array(i);
            break;
        }
        case kDataProperty: {
            DataNode eval = a2->Evaluate(i);
            if (eval.Type() != kDataFloat) {
                if (eval.Type() == kDataArray) {
                    DataArray *arr = new DataArray(eval.Array()->Size());
                    SetParams(arr, eval.Array());
                    a1->Node(i - 2) = a2->Array(i);
                }
            } else {
                a1->Node(i - 2) = a2->Float(i);
            }
            break;
        }
        default:
            break;
        }
    }
}
