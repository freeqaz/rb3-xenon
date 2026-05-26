#pragma once
#include "math/DoubleExponentialSmoother.h"
#include "ui/UIListState.h"

class HamNavList;

class HamScrollBehavior {
public:
    HamScrollBehavior(HamNavList *, UIListState *);
    bool ScrollUp(bool);
    bool ScrollDown(bool);
    bool IsScrolling() const;
    bool AtTop() const;
    bool AtBottom() const;
    void Enter();
    void Reset();
    void Exit();
    void Update(float);
    void PlayScrollSound();

    float GetFirstVal() { return mSettleTimer; }

    static void Init();
    static float mNeutralToSlowDownDelay;
    static float mSlowDownFirstTickDelay;
    static float mSlowDownTickDelay;
    static float mFastDownTickDelay;
    static float mNeutralToSlowUpDelay;
    static float mSlowUpFirstTickDelay;
    static float mSlowUpTickDelay;
    static float mFastUpTickDelay;
    static float mSlowScrollSpeed;
    static float mNormalScrollSpeed;
    static float mFastScrollSpeedBase;
    static float mFastScrollSpeedScalar;
    static float mScrollUpCap;
    static float mScrollDownCap;
    static float mSlowFastThreshold;

    friend class HamNavList;

private:
    static float sScrollSettleTime;

    float mSettleTimer; // 0x0
    bool mInputUp; // 0x4
    bool mInputDown; // 0x5
    int mScrollStep; // 0x8
    float mScrollTimeAccum; // 0xc
    float mScrollSpeed; // 0x10
    float mTickDelay; // 0x14
    float mScrollCooldown; // 0x18
    bool mAutoScrollActive; // 0x1c
    bool mFirstTick; // 0x1d
    float mScrollProgress; // 0x20
    int mPendingScrollDir; // 0x24
    int mContinuationDir; // 0x28
    int mLastScrollDir; // 0x2c
    int mScrollDir; // 0x30
    DoubleExponentialSmoother mSmoother; // 0x34
    int mSpeedState; // 0x48
    UIListState *mListState;
    HamNavList *mNavList;
};
