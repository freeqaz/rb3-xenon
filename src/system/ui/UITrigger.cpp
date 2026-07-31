// Retail's ObjPtr owner-ctor inline policy is PER-SITE, not per-TU. In this TU
// retail INLINES the owner-only ctor at the local-variable site in
// UITrigger::Load (three stores, no AddRef), but CALLS it out-of-line at the
// ??0UITrigger@@ member-init `mCallbackObject(this)`. So the TU opts in to the
// inline default below, and that one member-init opts back out by spelling the
// two-arg overload explicitly -- `mCallbackObject(this, nullptr)` binds
// ObjPtr(Hmx::Object*, T*), which is defined out-of-class in obj/ObjPtr_p.h and
// is too big for /Ob2 to inline (its `if (mObject) AddRef(this)` tail).
// ui/ is PCH-excluded, so this #define precedes the Object.h include.
#define RB3_OBJPTR_INLINE_OWNER_CTOR 1
#include "ui/UITrigger.h"
#include "math/Easing.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "rndobj/Anim.h"
#include "rndobj/EventTrigger.h"
#include "ui/UIComponent.h"
#include "utl/Loader.h"

// RB3-360 retail rev storage. Retail's LOAD_REVS keeps NO BinStreamRev: it splits
// the packed rev into two mutable file-scope shorts, and ASSERT_REVS emits nothing.
// The two words must live in ONE aligned(4) aggregate (altRev +0, rev +4) -- MSVC
// does not lay .bss out in declaration order, so two separate statics get other
// globals interleaved between them and will not fold onto one base register.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_UITrigger;
#define gAltRev gRevs_UITrigger.altRev
#define gRev gRevs_UITrigger.rev

UITrigger::UITrigger()
    : mBlockTransition(0), mCallbackObject(this, nullptr), mEndTime(0), mDone(1) {}

BEGIN_PROPSYNCS(UITrigger)
    SYNC_PROP(block_transition, mBlockTransition)
    SYNC_PROP(callback_object, mCallbackObject)
    SYNC_SUPERCLASS(EventTrigger)
END_PROPSYNCS

BEGIN_SAVES(UITrigger)
    bs << 1;
    SAVE_SUPERCLASS(EventTrigger)
    bs << mBlockTransition;
END_SAVES

BEGIN_COPYS(UITrigger)
    COPY_SUPERCLASS(EventTrigger)
    CREATE_COPY(UITrigger)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mBlockTransition)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_LOADS(UITrigger)
    int rev;
    bs >> rev;
    gRev = getHmxRev(rev);
    gAltRev = getAltRev(rev);
    if (gRev < 1) {
        UIComponent *uiCom = Hmx::Object::New<UIComponent>();
        uiCom->Load(bs);
        delete uiCom;
        Symbol sym;
        bs >> sym;
        UnregisterEvents();
        mTriggerEvents.clear();
        mTriggerEvents.push_back(sym);
        RegisterEvents();
        ObjPtr<RndAnimatable> animPtr(this);
        bs >> animPtr;
        mAnims.clear();
        mAnims.push_back();
        EventTrigger::Anim &anim = mAnims.back();
        anim.mAnim = animPtr;
    } else
        EventTrigger::Load(bs);
    bs >> mBlockTransition;
END_LOADS

void UITrigger::Trigger() {
    EventTrigger::Trigger();
    mStartTime = TheTaskMgr.UISeconds();
    mEndTime = 0;
    FOREACH (it, mAnims) {
        if (it->mAnim) {
            float f4;
            if (it->mEnable) {
                f4 = it->mPeriod * 30.0f;
                if (!f4) {
                    f4 = it->mScale;
                    if (!f4) {
                        f4 = 1.0f;
                    }
                    f4 = (float)std::fabs(it->mStart - it->mEnd) / f4;
                }
            } else {
                f4 = std::fabs(it->mAnim->StartFrame() - it->mAnim->EndFrame());
            }
            MaxEq(mEndTime, (it->mDelay * 30.0f + f4) / 30.0f);
        }
    }
    if (mBlockTransition && mEndTime > 5.0f) {
        MILO_WARN(
            "%s (%s) is blocking and really long! (%f seconds)",
            Name(),
            PathName(Dir()),
            mEndTime
        );
    }
    mEndTime += TheTaskMgr.UISeconds();
    mDone = false;
}

DataArray *UITrigger::SupportedEvents() {
    static DataArray *events =
        SystemConfig("objects", "UITrigger", "supported_events")->Array(1);
    return events;
}

void UITrigger::CheckAnims() {
    FOREACH (it, mAnims) {
        Anim &curAnim = *it;
        RndAnimatable *anim = curAnim.mAnim;
        if (anim && anim->GetRate() != RndAnimatable::k30_fps_ui) {
            if (TheLoadMgr.EditMode()) {
                MILO_NOTIFY("Setting animatable rate to k30_fps_ui for %s", anim->Name());
            }
            anim->SetRate(RndAnimatable::k30_fps_ui);
        }
        curAnim.mRate = RndAnimatable::k30_fps_ui;
    }
}

void UITrigger::Poll() {
    if (!mDone) {
        if (IsDone()) {
            mDone = true;
            if (mCallbackObject) {
                mCallbackObject->Handle(UITriggerCompleteMsg(this), true);
            }
        }
    }
}

void UITrigger::Enter() {
    mStartTime = TheTaskMgr.UISeconds();
    mEndTime = 0;
}

bool UITrigger::IsDone() const { return mEndTime <= TheTaskMgr.UISeconds(); }

bool UITrigger::IsBlocking() const {
    if (mStartTime > TheTaskMgr.UISeconds()) {
        const_cast<UITrigger *>(this)->mEndTime = 0;
    }
    return mBlockTransition && mEndTime && !IsDone();
}

void UITrigger::StopAnimations() {
    FOREACH (it, mAnims) {
        RndAnimatable *anim = (*it).mAnim;
        if (anim && anim->IsAnimating())
            anim->StopAnimation();
    }
}

void UITrigger::PlayStartOfAnims() {
    FOREACH (it, mAnims) {
        Anim &curAnim = *it;
        RndAnimatable *anim = curAnim.mAnim;
        if (anim) {
            float f3 = anim->StartFrame();
            float f4 = 0.0099999998f;
            if (curAnim.mEnable) {
                f3 = curAnim.mStart;
                if (f3 > curAnim.mEnd) {
                    f4 *= -1;
                }
            }
            anim->Animate(f3 + f4, f3, kTaskUISeconds, 0, 0, 0, kEaseLinear, 0, 0);
        }
    }
}

void UITrigger::PlayEndOfAnims() {
    FOREACH (it, mAnims) {
        Anim &curAnim = *it;
        RndAnimatable *anim = curAnim.mAnim;
        if (anim) {
            float f3 = anim->EndFrame();
            float f4 = 0.0099999998f;
            if (curAnim.mEnable) {
                f3 = curAnim.mEnd;
                if (curAnim.mStart > f3) {
                    f4 *= -1;
                }
            }
            anim->Animate(f3 - f4, f3, kTaskUISeconds, 0, 0, 0, kEaseLinear, 0, 0);
        }
    }
}

BEGIN_HANDLERS(UITrigger)
    HANDLE_EXPR(end_time, mEndTime)
    HANDLE_ACTION(play_start_of_anims, PlayStartOfAnims())
    HANDLE_ACTION(play_end_of_anims, PlayEndOfAnims())
    HANDLE_ACTION(stop_anims, StopAnimations())
    HANDLE_EXPR(is_done, IsDone())
    HANDLE_EXPR(is_blocking, IsBlocking())
    HANDLE_SUPERCLASS(EventTrigger)
END_HANDLERS
