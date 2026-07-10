#pragma once
#include "obj/Object.h"
#include "rndobj/EventTrigger.h"
#include "ui/UIComponent.h"
#include "ui/UILabel.h"

/** "a ticker to control counting up or down for a given number based label" */
class LabelNumberTicker : public UIComponent {
public:
    // Hmx::Object
    virtual ~LabelNumberTicker();
    OBJ_CLASSNAME(LabelNumberTicker)
    OBJ_SET_TYPE(LabelNumberTicker)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    // RndPollable
    virtual void Poll();
    virtual void Enter();

    NEW_OBJ(LabelNumberTicker)
    OBJ_MEM_OVERLOAD(0x14)

    static void Init();

    void CountUp();
    void CountUpFromCurrentValue();
    void SnapToValue(int i);

    UILabel *Label() const { return mLabel.Ptr() ? mLabel.Ptr() : nullptr; }
    void SetLabel(UILabel *);

protected:
    LabelNumberTicker();
    void UpdateDisplay();

    /** "label to be shrink wrapped" */
    ObjPtr<UILabel> mLabel; // 0x140
    int mDesiredValue; // 0x14c
    float mAnimTime; // 0x150
    float mAnimDelay; // 0x154
    Symbol mWrapperText; // 0x158
    float mAcceleration; // 0x15c - exponent for acceleration curve: progress^(1 + acceleration)
    int mAnimStartValue; // 0x160
    int mCurrentValue; // 0x164
    Timer mTimer; // 0x168 (8-aligned; retail has NO member between mCurrentValue and mTimer)
    ObjPtr<EventTrigger> mTickTrigger; // 0x198
    int mTickEvery; // 0x1a4 (vbase Hmx::Object at 0x1a8; sizeof(most-derived) = 0x1e0)

private:
    void SetDesiredValue(int);
};
