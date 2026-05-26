#include "rndobj/Flare.h"
#include "Rnd.h"
#include "math/Geo.h"
#include "math/Rot.h"
#include "math/Utl.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Cam.h"
#include "rndobj/Draw.h"
#include "rndobj/Env.h"
#include "rndobj/HiResScreen.h"
#include "rndobj/Mat.h"
#include "rndobj/Trans.h"
#include "rndobj/Utl.h"
#include <string.h>

RndFlare::RndFlare()
    : mPointTest(true), mAreaTest(true), mVisible(false), mSizes(0.1, 0.1), mMat(this),
      mRange(0, 0), mOffset(0), mSteps(1), mStep(0), mOcclusionResult(0), mOcclusionReady(false),
      mOcclusionPending(false), mScaleFactors(1, 1) {
    mMatrix.Identity();
}

RndFlare::~RndFlare() { TheRnd.RemovePointTest(this); }

BEGIN_HANDLERS(RndFlare)
    HANDLE_ACTION(set_steps, SetSteps(_msg->Int(2)))
    HANDLE_ACTION(set_point_test, SetPointTest(_msg->Int(2)))
    HANDLE_SUPERCLASS(RndTransformable)
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(RndFlare)
    SYNC_PROP(mat, mMat)
    SYNC_PROP(sizes, mSizes)
    SYNC_PROP(steps, mSteps)
    SYNC_PROP(range, mRange)
    SYNC_PROP(offset, mOffset)
    SYNC_PROP_MODIFY(point_test, mPointTest, TheRnd.RemovePointTest(this))
    SYNC_SUPERCLASS(RndTransformable)
    SYNC_SUPERCLASS(RndDrawable)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(RndFlare)
    SAVE_REVS(7, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndTransformable)
    SAVE_SUPERCLASS(RndDrawable)
    bs << mMat << mSizes << mRange << mSteps;
    bs << mPointTest;
    bs << mOffset;
END_SAVES

BEGIN_COPYS(RndFlare)
    CREATE_COPY_AS(RndFlare, f)
    MILO_ASSERT(f, 0x19);
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndTransformable)
    COPY_SUPERCLASS(RndDrawable)
    COPY_MEMBER_FROM(f, mSizes)
    COPY_MEMBER_FROM(f, mMat)
    COPY_MEMBER_FROM(f, mVisible)
    COPY_MEMBER_FROM(f, mRange)
    COPY_MEMBER_FROM(f, mOffset)
    COPY_MEMBER_FROM(f, mSteps)
    COPY_MEMBER_FROM(f, mPointTest)
    mOcclusionPending = false;
    mOcclusionReady = false;
END_COPYS

INIT_REVS(7, 0)

BEGIN_LOADS(RndFlare)
    LOAD_REVS(bs)
    ASSERT_REVS(7, 0)
    if (d.rev > 3) {
        Hmx::Object::Load(bs);
    }
    RndTransformable::Load(bs);
    RndDrawable::Load(bs);
    if (d.rev > 0) {
        bs >> mMat;
    }
    if (d.rev > 2) {
        bs >> mSizes;
    } else {
        bs >> mSizes.x;
        mSizes.y = mSizes.x;
    }
    if (d.rev > 1) {
        bs >> mRange >> mOffset;
    }
    if (d.rev > 4) {
        d >> mPointTest;
    }
    if (d.rev > 6) {
        bs >> mOffset;
    }
    mOcclusionPending = false;
    mOcclusionReady = false;
    CalcScale();
END_LOADS

void RndFlare::Print() {
    TheDebug << "   mat: " << mMat << "\n";
    TheDebug << "   sizes: " << mSizes << "\n";
    TheDebug << "   range: " << mRange << "\n";
    TheDebug << "   offset:" << mOffset << "\n";
    TheDebug << "   steps: " << mSteps << "\n";
    TheDebug << "   point test: " << mPointTest << "\n";
}

void RndFlare::DrawShowing() {
    if (TheRnd.GetDrawMode() != 0)
        return;
    RndCam *cam = RndCam::Current();

    const Transform &worldXfm = WorldXfm();
    Vector2 screenPos;
    float depth = cam->WorldToScreen(worldXfm.v, screenPos);

    float scale;
    Hmx::Rect &rect = CalcRect(screenPos, scale);
    Hmx::Rect localRect = rect;

    float visibility = 0.0f;
    if (RectOffscreen(localRect) || depth <= 0.0f) {
        mOcclusionResult = 0;
        mStep = 0;
        mOcclusionReady = false;
        mOcclusionPending = false;
    } else {
        bool useOccResult = false;
        if (!mPointTest || TheHiResScreen.IsActive()) {
            mOcclusionResult = scale;
            mVisible = true;
        } else {
            if (mOcclusionPending || (useOccResult = true, !mOcclusionReady)) {
                useOccResult = false;
            }
            bool oldReady = mOcclusionReady;
            mOcclusionReady = false;
            mOcclusionPending = oldReady;

            const Transform &flareXfm = WorldXfm();
            const Transform &camXfm = cam->WorldXfm();
            Vector3 dir;
            dir.z = camXfm.v.z - flareXfm.v.z;
            dir.y = camXfm.v.y - flareXfm.v.y;
            dir.x = camXfm.v.x - flareXfm.v.x;
            Normalize(dir, dir);

            const Transform &flareXfm2 = WorldXfm();
            float offset = mOffset;
            dir.x = offset * dir.x + flareXfm2.v.x;
            dir.y = dir.y * offset + flareXfm2.v.y;
            dir.z = dir.z * offset + flareXfm2.v.z;
            TheRnd.TestPoint(dir, this);
        }

        if (useOccResult) {
            mStep = mVisible ? mSteps : 0;
        } else {
            int steps = mSteps;
            int newStep = ((int)(unsigned char)mVisible * 2 + mStep) - 1;
            if (newStep <= steps) {
                steps = ((unsigned int)newStep >> 31) - 1 & newStep;
            }
            mStep = steps;
        }

        float ratio = (float)mStep / (float)mSteps;
        if (mAreaTest) {
            ratio = (mOcclusionResult / (localRect.w * localRect.h)) * ratio;
        }

        if (visibility < ratio) {
            float alpha = 1.0f;
            if (mMat) {
                float rangeX = mRange.x;
                float t = 1.0f;
                if (rangeX != mRange.y) {
                    t = (depth - mRange.y) / (rangeX - mRange.y);
                }
                alpha = Clamp(0.0f, 1.0f, t * ratio);

                float r = alpha, g = alpha, b = alpha;
                if (mMat->UseEnviron() && RndEnviron::Current()) {
                    const Hmx::Color &ambColor = RndEnviron::Current()->AmbientColor();
                    r = ambColor.red * alpha;
                    g = ambColor.green * alpha;
                    b = ambColor.blue * alpha;
                }
                mMat->SetColor(r, g, b);
            }

            if (mMat && mMat->GetTexGen() == kTexGenXfm) {
                Hmx::Matrix3 texMat;
                memcpy(&texMat, &mMat->TexXfm(), 0x40);
                MakeRotMatrixZ(screenPos.x - 0.5f, texMat);
                memcpy(&mMat->TexXfm(), &texMat, 0x40);
                mMat->MarkDirty(2);
            }

            Hmx::Rect *drawRect;
            if (mAreaTest) {
                drawRect = &mArea;
            } else {
                drawRect = &CalcRect(screenPos, scale);
            }
            localRect = *drawRect;

            Hmx::Color white(1.0f, 1.0f, 1.0f, 1.0f);
            TheRnd.DrawRect(localRect, white, mMat, nullptr, nullptr);
        }
    }
}

void RndFlare::Mats(std::list<RndMat *> &list, bool) {
    if (mMat) {
        MatShaderOptions shaderOpts = GetDefaultMatShaderOpts(this, mMat);
        mMat->SetShaderOpts(shaderOpts);
        list.push_back(mMat);
    }
}

void RndFlare::SetMat(RndMat *mat) { mMat = mat; }

void RndFlare::SetPointTest(bool test) {
    if (!test && mPointTest) {
        TheRnd.RemovePointTest(this);
    }
    mPointTest = test;
}

Hmx::Rect &RndFlare::CalcRect(Vector2 &screenPos, float &visibleArea) {
    float flareSize = mSizes.x;
    if (flareSize != mSizes.y) {
        RndCam *cam = RndCam::Current();
        float dot = Dot(cam->WorldXfm().m.y, WorldXfm().m.y);
        float blend = Max(0.0f, -dot);
        flareSize = Interp(mSizes.x, mSizes.y, blend);
    }

    int width = TheRnd.Width();
    int height = TheRnd.Height();
    if (TheHiResScreen.IsActive()) {
        width *= TheHiResScreen.GetTiling();
        int paddingX = TheHiResScreen.GetPaddingX();
        int tiling = TheHiResScreen.GetTiling();
        width -= paddingX * tiling;
        height *= tiling;
        int paddingY = TheHiResScreen.GetPaddingY();
        height -= paddingY * TheHiResScreen.GetTiling();
        Hmx::Rect screenRect = TheHiResScreen.ScreenRect();
        screenPos.x -= screenRect.x;
        screenPos.y -= screenRect.y;
    }
    CalcScale();

    mArea.w = (flareSize * (width * mScaleFactors.x));
    mArea.h = ((height * (flareSize * (width * mScaleFactors.y)))) / (width * TheRnd.YRatio());
    mArea.x = screenPos.x * width - mArea.w * 0.5f;
    mArea.y = screenPos.y * height - mArea.h * 0.5f;

    auto _tmp0 = Min<float>(width, mArea.x + mArea.w);
    float visibleHeight = Min<float>(height, mArea.y + mArea.h) - Max(0.0f, mArea.y);
    float visibleWidth = _tmp0 - Max(0.0f, mArea.x);
    visibleArea = visibleWidth * visibleHeight;
    return mArea;
}

bool RndFlare::RectOffscreen(const Hmx::Rect &r) const {
    if (r.x + r.w < 0)
        return true;
    else if (r.y + r.h < 0)
        return true;
    else if (r.x > TheRnd.Width())
        return true;
    else if (r.y > TheRnd.Height())
        return true;
    else
        return false;
}

void RndFlare::CalcScale() {
    if (WorldXfm().m != mMatrix) {
        Vector3 v28;
        mMatrix = WorldXfm().m;
        float len = Length(mMatrix.z);
        Cross(mMatrix.x, mMatrix.y, v28);
        mScaleFactors.Set(Length(mMatrix.x), Dot(v28, mMatrix.z) > 0.0f ? len : -len);
    }
}

void RndFlare::SetSteps(int i1) {
    if (i1 < 1) {
        i1 = 1;
    }
    if (mStep == mSteps) {
        mStep = i1;
    } else {
        float stepsFloat = (float)mSteps;
        float i1Float = (float)i1;
        mStep = (int)(i1Float / stepsFloat) * mStep;
    }
    mSteps = i1;
}
