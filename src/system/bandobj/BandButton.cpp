// Ported from rb3-Wii src/system/bandobj/BandButton.cpp (MWCC -> MSVC X360).
#include "bandobj/BandButton.h"
#include "bandobj/BandLabel.h"
#include "rndobj/PropAnim.h"
#include "ui/UI.h"
#include "utl/Symbols.h"

INIT_REVS(BandButton)

void BandButton::Init() {
    TheUI->InitResources("BandButton");
    Register();
}

BandButton::BandButton() : mFocusAnim(0), mPulseAnim(0), mAnimTask(0), mStartTime(0) {}

BandButton::~BandButton() {
    if (mFocusAnim)
        delete mFocusAnim;
    if (mPulseAnim)
        delete mPulseAnim;
}

BEGIN_COPYS(BandButton)
    COPY_SUPERCLASS(UIButton)
END_COPYS

BEGIN_SAVES(BandButton)
    SAVE_REVS(16, 0)
    SAVE_SUPERCLASS(UIButton)
END_SAVES

BEGIN_LOADS(BandButton)
    PreLoad(bs);
    PostLoad(bs);
END_LOADS

// NOTE: the faithful rb3-Wii PreLoad body touches UILabel members that this
// tree has not reconstructed yet (they live inside UILabel::mUnkTU5Tail):
// mFitType/mWidth/mHeight/mLeading/mAlignment/mKerning/mTextSize/mCapsMode.
// Reduced to the parts that compile; this one function is expected NOT to match.
void BandButton::PreLoad(BinStream &bs) {
    LOAD_REVS(bs);
    ASSERT_REVS(0x10, 0);
    UIButton::PreLoad(bs);
}

void BandButton::PostLoad(BinStream &bs) {
    UIButton::PostLoad(bs);
    if (gRev == 13 || gRev == 14 || gRev == 15) {
        ObjPtr<RndMesh> meshPtr(0);
        bs >> meshPtr;
    }
}

void BandButton::DrawShowing() {
    bool focusanimating = mFocusAnim && mFocusAnim->IsAnimating();
    if (mState == kFocused && (focusanimating || mPulseAnim)) {
        if (!focusanimating && !mPulseAnim->IsAnimating())
            StartPulseAnim();
        if (focusanimating) {
            if (!mText->GetFont())
                Update();
            mAnimTask->Poll(TheTaskMgr.UISeconds() - mStartTime);
            UpdateAndDrawHighlightMesh();
            mText->DrawShowing();
            if (UILabel::sDebugHighlight)
                Highlight();
        } else
            UILabel::DrawShowing();
    } else
        UILabel::DrawShowing();
}

void BandButton::SetState(UIComponent::State state) {
    UIComponent::State curstate;
    if (state != mState) {
        curstate = GetState();
        UIComponent::SetState(state);
        if (mState == kFocused && mFocusAnim) {
            if (TheUI->InTransition())
                SkipToFocused();
            else {
                mAnimTask = mFocusAnim->Animate(
                    mFocusAnim->StartFrame(),
                    mFocusAnim->EndFrame(),
                    kTaskUISeconds,
                    0.0f,
                    0.05f
                );
                mStartTime = TheTaskMgr.UISeconds();
            }
        } else if (curstate == kFocused) {
            if (mPulseAnim && mPulseAnim->IsAnimating())
                mPulseAnim->StopAnimation();
            if (mFocusAnim) {
                if (TheUI->InTransition())
                    SkipToUnfocused();
                else {
                    mAnimTask = mFocusAnim->Animate(
                        mFocusAnim->EndFrame(),
                        mFocusAnim->StartFrame(),
                        kTaskUISeconds,
                        0.0f,
                        0.05f
                    );
                    mStartTime = TheTaskMgr.UISeconds();
                }
            }
        }
    }
}

void BandButton::SkipToFocused() {
    if (mFocusAnim) {
        mAnimTask = mFocusAnim->Animate(
            mFocusAnim->EndFrame() - 1.0f,
            mFocusAnim->EndFrame(),
            kTaskUISeconds,
            0.0f,
            0.0f
        );
        mStartTime = TheTaskMgr.UISeconds();
    }
}

void BandButton::SkipToUnfocused() {
    if (mFocusAnim) {
        mAnimTask = mFocusAnim->Animate(
            mFocusAnim->StartFrame() + 1.0f,
            mFocusAnim->StartFrame(),
            kTaskUISeconds,
            0.0f,
            0.0f
        );
        mStartTime = TheTaskMgr.UISeconds();
    }
}

void BandButton::StartPulseAnim() {
    if (mPulseAnim) {
        mAnimTask = mPulseAnim->Animate(
            0.05f,
            false,
            0.0f,
            RndAnimatable::k30_fps_ui,
            mPulseAnim->StartFrame(),
            mPulseAnim->EndFrame(),
            0.0f,
            1.0f,
            loop
        );
        mStartTime = TheTaskMgr.UISeconds();
    }
}

// NOTE: reduced -- mLabelDir / mFontMatVariation are not reconstructed here.
void BandButton::Update() { UILabel::Update(); }

BEGIN_HANDLERS(BandButton)
    HANDLE_ACTION(skip_to_focused, SkipToFocused())
    HANDLE_ACTION(skip_to_unfocused, SkipToUnfocused())
    HANDLE_SUPERCLASS(UIButton)
    HANDLE_CHECK(0x171)
END_HANDLERS

BEGIN_PROPSYNCS(BandButton)
    SYNC_SUPERCLASS(UIButton)
END_PROPSYNCS
