#define RB3_TU_OBJPTR_FORCEINLINE_CTOR
#include "ui/LabelNumberTicker.h"
#include "UIComponent.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "ui/UI.h"
#include "utl/BinStream.h"
#include "utl/Locale.h"
#include "utl/MBT.h"
#include "utl/Symbol.h"

LabelNumberTicker::LabelNumberTicker()
    : mLabel(this), mDesiredValue(0), mAnimTime(0), mAnimDelay(0), mWrapperText(gNullStr),
      mAcceleration(0), mAnimStartValue(0), mCurrentValue(0), mTickTrigger(this), mTickEvery(0) {}

LabelNumberTicker::~LabelNumberTicker() {}

BEGIN_LOADS(LabelNumberTicker)
    PreLoad(bs);
    PostLoad(bs);
END_LOADS

BEGIN_SAVES(LabelNumberTicker)
    SAVE_REVS(2, 0)
    bs << mLabel << mDesiredValue << mAnimTime << mAnimDelay << mWrapperText;
    bs << mAcceleration;
    bs << mTickTrigger << mTickEvery;
    SAVE_SUPERCLASS(UIComponent)
END_SAVES

BEGIN_COPYS(LabelNumberTicker)
    CREATE_COPY_AS(LabelNumberTicker, p)
    MILO_ASSERT(p, 0x2d);
    COPY_MEMBER_FROM(p, mLabel)
    COPY_MEMBER_FROM(p, mDesiredValue)
    COPY_MEMBER_FROM(p, mAnimTime)
    COPY_MEMBER_FROM(p, mAnimDelay)
    COPY_MEMBER_FROM(p, mWrapperText)
    COPY_MEMBER_FROM(p, mAcceleration)
    COPY_MEMBER_FROM(p, mTickTrigger)
    COPY_MEMBER_FROM(p, mTickEvery)
    UIComponent::Copy(p, ty);
END_COPYS

BEGIN_PROPSYNCS(LabelNumberTicker)
    SYNC_PROP_SET(label, Label(), SetLabel(_val.Obj<UILabel>()))
    SYNC_PROP_SET(desired_value, mDesiredValue, SetDesiredValue(_val.Int()))
    SYNC_PROP_MODIFY(wrapper_text, mWrapperText, UpdateDisplay())
    SYNC_PROP_MODIFY(anim_time, mAnimTime, UpdateDisplay())
    SYNC_PROP_MODIFY(anim_delay, mAnimDelay, UpdateDisplay())
    SYNC_PROP(acceleration, mAcceleration)
    SYNC_PROP(tick_trigger, mTickTrigger)
    SYNC_PROP(tick_every, mTickEvery)
    SYNC_SUPERCLASS(UIComponent)
END_PROPSYNCS

// RB3 retail 0x82827B10: nothing was pushed; the base PostLoad is followed by
// the virtual Update() (rb3-Wii shape).
void LabelNumberTicker::PostLoad(BinStream &bs) {
    UIComponent::PostLoad(bs);
    Update();
}

void LabelNumberTicker::Init() {
    REGISTER_OBJ_FACTORY(LabelNumberTicker)
    TheUI->InitResources("LabelNumberTicker");
}

void LabelNumberTicker::UpdateDisplay() {
    if (mLabel) {
        if (mWrapperText != gNullStr) {
            mLabel->SetTokenFmt(mWrapperText, (char *)LocalizeSeparatedInt(mCurrentValue));
        }
    }
}

void LabelNumberTicker::SetLabel(UILabel *l) {
    mLabel = l;
    UpdateDisplay();
}

void LabelNumberTicker::SetDesiredValue(int i) {
    mAnimStartValue = mCurrentValue;
    mDesiredValue = i;
    if (mAnimDelay + mAnimTime <= 0.0f)
        mCurrentValue = i;
    else {
        mTimer.Reset();
        mTimer.Start();
    }
    if (mTickTrigger && i > 0) {
        mTickTrigger->Trigger();
    }
    UpdateDisplay();
}

void LabelNumberTicker::CountUp() {
    int val = mDesiredValue;
    mCurrentValue = 0;
    mDesiredValue = 0;
    UpdateDisplay();
    SetDesiredValue(val);
}

void LabelNumberTicker::CountUpFromCurrentValue() {
    int val = mDesiredValue;
    if (mCurrentValue >= val) {
        mCurrentValue = 0;
        mDesiredValue = 0;
        UpdateDisplay();
    }
    SetDesiredValue(val);
}

void LabelNumberTicker::Enter() {
    UIComponent::Enter();
    UpdateDisplay();
}

// RB3 retail keeps no BinStreamRev here: the packed rev is split into two
// mutable TU shorts (alt at +0, rev at +4), there is no version guard and no
// Push/PopRev -- the same dialect as ui/UIColor.cpp.
#pragma push_macro("INIT_REVS")
#pragma push_macro("LOAD_REVS")
#pragma push_macro("ASSERT_REVS")
#undef INIT_REVS
#undef LOAD_REVS
#undef ASSERT_REVS
// Separate statics are not co-addressed in this TU (measured: two relocs), so
// they live in one aligned(4) aggregate, as in ui/UIListArrow.cpp.
#define INIT_REVS(r_, a_)                                                                \
    static struct {                                                                      \
        __declspec(align(4)) unsigned short altRev;                                      \
        __declspec(align(4)) unsigned short rev;                                         \
    } gRevs_LabelNumberTicker = { a_, r_ };
#define LOAD_REVS(bs)                                                                    \
    int rev;                                                                             \
    bs >> rev;                                                                           \
    gRevs_LabelNumberTicker.rev = getHmxRev(rev);                                        \
    gRevs_LabelNumberTicker.altRev = getAltRev(rev);
#define ASSERT_REVS(rev1, rev2)

INIT_REVS(2, 0)

// RB3 retail 0x82828458 (rb3-Wii shape).
void LabelNumberTicker::PreLoad(BinStream &bs) {
    LOAD_REVS(bs);
    ASSERT_REVS(2, 0)
    bs >> mLabel;
    bs >> mDesiredValue;
    bs >> mAnimTime;
    bs >> mAnimDelay;
    bs >> mWrapperText;
    if (gRevs_LabelNumberTicker.rev >= 1)
        bs >> mAcceleration;
    if (gRevs_LabelNumberTicker.rev >= 2) {
        bs >> mTickTrigger;
        bs >> mTickEvery;
    }
    UIComponent::PreLoad(bs);
}

#pragma pop_macro("ASSERT_REVS")
#pragma pop_macro("LOAD_REVS")
#pragma pop_macro("INIT_REVS")

void LabelNumberTicker::SnapToValue(int i) {
    mCurrentValue = i;
    mDesiredValue = i;
    UpdateDisplay();
}

void LabelNumberTicker::Poll() {
    UIComponent::Poll();
    if (mTimer.Running()) {
        // Get elapsed time and compute animation timing windows
        float elapsedMs = mTimer.SplitMs();
        float animTimeMs = mAnimTime * 1000.0f;
        float delayMs = mAnimDelay * 1000.0f;
        float totalMs = delayMs + animTimeMs;

        // Only animate after initial delay period
        if (elapsedMs >= delayMs) {
            // Calculate animation progress (0.0 to 1.0+)
            float progress = (elapsedMs - delayMs) / animTimeMs;

            // Apply acceleration curve: progress^(1 + acceleration)
            float powered = std::pow(progress, mAcceleration);
            progress *= powered;

            // Interpolate from start value (mAnimStartValue) to desired value
            int valueDelta = mDesiredValue - mAnimStartValue;
            int newValue = mAnimStartValue + (int)(progress * valueDelta);

            // Trigger events when crossing mTickEvery thresholds
            if (mTickTrigger && mTickEvery != 0) {
                if ((newValue / mTickEvery) > (mCurrentValue / mTickEvery)) {
                    mTickTrigger->Trigger();
                }
            }

            // Update current display value (mCurrentValue)
            mCurrentValue = newValue;

            // Stop animation when target reached or time exceeded
            if (mCurrentValue == mDesiredValue || elapsedMs >= totalMs) {
                mCurrentValue = mDesiredValue;
                mTimer.Stop();
            }
        }
        UpdateDisplay();
    }
}

BEGIN_HANDLERS(LabelNumberTicker)
    HANDLE_ACTION(snap_to_value, SnapToValue(_msg->Int(2)))
    HANDLE_ACTION(count_up, CountUp())
#ifdef RB3_KEEP_DC3_ONLY_HANDLERS
    // Retail fn_82828820 builds exactly 2 Symbols (snap_to_value, count_up);
    // both oracles and the rb3-Wii source agree this third arm is DC3-only.
    HANDLE_ACTION(count_up_from_current, CountUpFromCurrentValue())
#endif
    HANDLE_SUPERCLASS(UIComponent)
END_HANDLERS
