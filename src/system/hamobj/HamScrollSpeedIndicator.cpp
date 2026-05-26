#include "hamobj/HamScrollSpeedIndicator.h"
#include "math/Easing.h"
#include "math/Mtx.h"
#include "obj/Object.h"
#include "rndobj/Anim.h"
#include "rndobj/Dir.h"
#include "utl/BinStream.h"

HamScrollSpeedIndicator::HamScrollSpeedIndicator()
    : mIsShowing(0), mEnterAnim(this), mExitAnim(this), mIndicatorAnim(this) {}

BEGIN_HANDLERS(HamScrollSpeedIndicator)
    HANDLE_SUPERCLASS(RndDir)
END_HANDLERS

BEGIN_PROPSYNCS(HamScrollSpeedIndicator)
    SYNC_PROP(enter_anim, mEnterAnim)
    SYNC_PROP(exit_anim, mExitAnim)
    SYNC_PROP(indicator_anim, mIndicatorAnim)
    SYNC_PROP(slow_scroll_threshold_frame, mSlowScrollThresholdFrame)
    SYNC_PROP(fast_scroll_threshold_frame, mFastScrollThresholdFrame)
    SYNC_SUPERCLASS(RndDir)
END_PROPSYNCS

BEGIN_SAVES(HamScrollSpeedIndicator)
    SAVE_REVS(2, 0)
    SAVE_SUPERCLASS(RndDir)
    bs << mEnterAnim;
    bs << mExitAnim;
    bs << mIndicatorAnim;
    bs << mSlowScrollThresholdFrame;
    bs << mFastScrollThresholdFrame;
END_SAVES

BEGIN_COPYS(HamScrollSpeedIndicator)
    COPY_SUPERCLASS(RndDir)
    CREATE_COPY(HamScrollSpeedIndicator)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mEnterAnim)
        COPY_MEMBER(mExitAnim)
        COPY_MEMBER(mIndicatorAnim)
        COPY_MEMBER(mSlowScrollThresholdFrame)
        COPY_MEMBER(mFastScrollThresholdFrame)
    END_COPYING_MEMBERS
END_COPYS

void HamScrollSpeedIndicator::DrawShowing() {
    for (std::vector<RndDrawable *>::iterator it = mDraws.begin(); it != mDraws.end();
         ++it) {
        (*it)->Draw();
    }
}

float HamScrollSpeedIndicator::StartFrame() {
    if (mEnterAnim)
        return mEnterAnim->StartFrame();
    else
        return 0;
}

float HamScrollSpeedIndicator::EndFrame() {
    if (mEnterAnim)
        return mEnterAnim->EndFrame();
    else
        return 0;
}

void HamScrollSpeedIndicator::HandleEnter() { Show(true); }
void HamScrollSpeedIndicator::HandleExit() { Show(false); }

void HamScrollSpeedIndicator::Show(bool enter) {
    if (enter) {
        mEnterAnim->Animate(0, false, 0, nullptr, kEaseLinear, 0, false);
    } else
        mExitAnim->Animate(0, false, 0, nullptr, kEaseLinear, 0, false);
    mIsShowing = enter;
}

void HamScrollSpeedIndicator::Draw(const Transform &xfm) {
    SetWorldXfm(xfm);
    for (std::vector<RndDrawable *>::iterator it = mDraws.begin(); it != mDraws.end();
         ++it) {
        (*it)->Draw();
    }
}

void HamScrollSpeedIndicator::Update(float scrollSpeed, float minSpeed, float maxSpeed) {
    RndAnimatable *anim = mIndicatorAnim;
    float endFrame = anim->EndFrame();
    float startFrame = anim->StartFrame();
    float halfRange = (endFrame - startFrame) * 0.5f;
    float frame;
    if (scrollSpeed < 0.1f || scrollSpeed > 0.9f) {
        float threshold = mSlowScrollThresholdFrame;
        float range = halfRange - threshold;
        if (scrollSpeed > 0.5f) {
            frame = -((scrollSpeed - 0.9f) / (maxSpeed + 1.0f - 0.9f)) * range - threshold;
        } else {
            frame = -((scrollSpeed - 0.1f) / (minSpeed + 0.1f)) * range + threshold;
        }
    } else {
        float t = (scrollSpeed - 0.1f) * 1.25f;
        float scaled = t * mSlowScrollThresholdFrame;
        frame = -(scaled * 2.0f - mSlowScrollThresholdFrame);
    }
    if (mIsShowing) {
        anim = mIndicatorAnim;
        endFrame = anim->EndFrame();
        startFrame = anim->StartFrame();
        float clamped = (startFrame - frame < 0.0f) ? frame : startFrame;
        float result = (clamped - endFrame < 0.0f) ? clamped : endFrame;
        mIndicatorAnim->SetFrame(result, 1.0f);
    }
}

INIT_REVS(2, 0)

void HamScrollSpeedIndicator::PreLoad(BinStream &bs) {
    LOAD_REVS(bs);
    ASSERT_REVS(2, 0)
    RndDir::PreLoad(bs);
    bs.PushRev(packRevs(d.altRev, d.rev), this);
}

void HamScrollSpeedIndicator::PostLoad(BinStream &bs) {
    BinStreamRev d(bs, bs.PopRev(this));
    RndDir::PostLoad(bs);
    if ((d.rev & 0xffff) >= 1) {
        bs >> mEnterAnim;
        bs >> mExitAnim;
        bs >> mIndicatorAnim;
    }
    if (2 <= (d.rev & 0xffff)) {
        bs >> mSlowScrollThresholdFrame;
        bs >> mFastScrollThresholdFrame;
    }
}
