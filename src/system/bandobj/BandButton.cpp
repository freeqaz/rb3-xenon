// Retail inlines the ObjPtr two-arg ctor at this TU's member-init sites.
#define RB3_OBJPTR_INLINE_TWOARG_CTOR
// BandButton (bandobj/BandButton.cpp), MSVC X360.
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

// Retail folds both rev words onto ONE base register with offsets 0/4
// (lbl_82CBE414: altRev+0, rev+4) where a function reads both (PreLoad), and
// reads rev through its own relocation lbl_82CBE418 where it reads only rev
// (PostLoad).  That is two adjacent internal-linkage statics, not one struct
// (a struct base costs PostLoad an extra addi).  Explicit `= 0` keeps them in
// .data in declaration order.  The class statics from DECLARE_REVS/INIT_REVS
// would each take an external relocation.
static unsigned short sAltRev = 0;
static unsigned short sRev = 0;
#define gAltRev sAltRev
#define gRev sRev

void BandButton::PreLoad(BinStream &bs) {
    LOAD_REVS(bs);
    ASSERT_REVS(0x10, 0);
    bool bbb = false;
    if (gRev < 8) {
        if (gRev <= 4) {
            LOAD_SUPERCLASS(RndTransformable)
            LOAD_SUPERCLASS(RndDrawable)
        } else
            UIButton::PreLoad(bs);
        if (gRev > 2) {
            int i, j, k, l;
            bs >> i >> j >> k >> l;
        }
        if (gRev <= 4) {
            Symbol s;
            bs >> s;
            SetType(s);
        }
        if (gRev < 8) {
            bool b8;
            bs >> b8;
            if (b8)
                mFitType = kFitStretch;
            else
                mFitType = kFitWrap;
        }
        if (gRev < 7 && mFitType == kFitStretch) {
            Hmx::Matrix3 mtx;
            mtx.Identity();
            SetLocalRot(mtx);
        }
        if (gRev != 0)
            bs >> bbb;
        else
            bbb = false;
        bs >> mWidth;
        bs >> mHeight;
        if (gRev <= 4)
            bs >> mTextToken;
        if (gRev > 5)
            bs >> (int &)mAlignment;
    } else if (gRev == 8) {
        UIButton::PreLoad(bs);
        int i;
        bs >> i;
        mFitType = (FitType)i;
        bs >> mWidth;
        bs >> mHeight;
        bs >> mLeading;
        bs >> (int &)mAlignment;
        int w, x, y, z;
        bs >> w >> x >> y >> z;
        bs >> bbb;
        Hmx::Color col;
        bs >> col;
        bs >> mKerning;
        bs >> mTextSize;
    } else {
        UIButton::PreLoad(bs);
        if (gRev < 0xC) {
            int i;
            bs >> i;
            mFitType = (FitType)i;
            bs >> mWidth;
            bs >> mHeight;
            if (mFitType == kFitWrap) {
                mHeight = 0;
                mWidth = 0;
            }
        }
        if (gRev < 0xB) {
            bs >> mLeading;
            bs >> (int &)mAlignment;
        }
        if (gRev < 0xE) {
            int i, j, k, l;
            bs >> i >> j >> k >> l;
        }
        if (gRev < 0xB) {
            bs >> bbb >> mKerning >> mTextSize;
        }
    }
    if (gRev < 0xC) {
        int i;
        bs >> i;
    }
    if (gRev < 0xB) {
        mCapsMode = (RndText::CapsMode)(bbb ? 2 : 0);
    }
    if (gRev == 0xE) {
        BandLabel::LoadOldBandTextComp(bs);
    }
}

void BandButton::PostLoad(BinStream &bs) {
    UIButton::PostLoad(bs);
    if (gRev > 12 && gRev < 16) {
        ObjPtr<RndMesh> meshPtr(0);
        bs >> meshPtr;
    }
}
#undef gRev
#undef gAltRev

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
    static Symbol loop("loop");
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

// Retail 0x82343E10, 524 B. Read off retail bytes: both label-dir anims,
// same order.
//
// The one place needing care is the Replace()
// argument. Retail converts BOTH arguments *and* `this` with the identical
// four-instruction virtual-base adjust (lwz +4 / lwz +4 / add / addi +4), i.e.
// retail's Hmx::Object::Replace takes (Hmx::Object *, Hmx::Object *). This tree
// carries DC3's (ObjRef *, Hmx::Object *) instead. That difference is invisible
// inside the callee -- which is why the ported Replace bodies still match 100% --
// but it is visible here, at the call site, as the missing conversion.
// Changing the signature would cascade through ~27 consumers (lane REFIS-1
// measured that the header must not move), so the conversion is spelled out
// locally: the (Hmx::Object *) cast emits retail's null-guarded vbase adjust and
// the outer cast is a no-op reinterpret that satisfies the declared parameter.
void BandButton::Update() {
    UILabel::Update();
    if (mLabelDir->FocusAnim()) {
        if (!mFocusAnim)
            mFocusAnim = Hmx::Object::New<RndPropAnim>();
        mFocusAnim->Copy(mLabelDir->FocusAnim(), Hmx::Object::kCopyShallow);
        mFocusAnim->Replace(
            (ObjRef *)(Hmx::Object *)mLabelDir->TextObj(mFontMatVariation), mText
        );
    }
    if (mLabelDir->PulseAnim()) {
        if (!mPulseAnim)
            mPulseAnim = Hmx::Object::New<RndPropAnim>();
        mPulseAnim->Copy(mLabelDir->PulseAnim(), Hmx::Object::kCopyShallow);
        mPulseAnim->Replace(
            (ObjRef *)(Hmx::Object *)mLabelDir->TextObj(mFontMatVariation), mText
        );
    }
}

BEGIN_HANDLERS(BandButton)
    HANDLE_ACTION(skip_to_focused, SkipToFocused())
    HANDLE_ACTION(skip_to_unfocused, SkipToUnfocused())
    HANDLE_SUPERCLASS(UIButton)
    HANDLE_CHECK(0x171)
END_HANDLERS

BEGIN_PROPSYNCS(BandButton)
    SYNC_SUPERCLASS(UIButton)
END_PROPSYNCS
