#pragma once
#include "rndobj/Dir.h"
#include "rndobj/EventTrigger.h"
#include "rndobj/Group.h"
#include "rndobj/PropAnim.h"
#include "rndobj/TransAnim.h"
#include "bandobj/BandLabel.h"

enum VocalHUDColor {
    kVocalColorGreen = 0,
    kVocalColorYellow = 1,
    kVocalColorOrange = 2,
    kVocalColorBrown = 3,
    kVocalColorPurple = 4,
    kVocalColorBlue = 5,
    kVocalColorWhite = 6,
    kVocalColorInvalid = -1
};

class StreakMeter : public RndDir {
public:
    StreakMeter();
    OBJ_CLASSNAME(StreakMeterDir);
    OBJ_SET_TYPE(StreakMeterDir);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual ~StreakMeter() {}
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    virtual void SyncObjects();

    void Reset();
    bool SetMultiplier(int);
    void SetBandMultiplier(int);
    void EndOverdrive() const;
    void BreakStreak(bool);
    void Overdrive() const;
    void SetWipe(float);
    void SetPeakState();
    void SetPartColor(int, VocalHUDColor);
    void SetPartActive(int, bool);
    void ForceFadeInactiveParts();
    int NumActiveParts() const;
    void ShowPhraseFeedback(int, bool);
    void SetIsolatedPart(int);
    void CombineMultipliers(bool);
    void MultiplierChanged();
    int GetMultiplierToShow() const;
    void UpdateMultiplierText(int);
    void SetPartPct(int, float, bool);
    void SetNumParts(int);
    void SyncVoxPhraseTriggers();
    void SetPitch(float);

    NEW_OVERLOAD;
    DELETE_OVERLOAD;
    NEW_OBJ(StreakMeter)
    static void Init() { Register(); }
    static void Register() { REGISTER_OBJ_FACTORY(StreakMeter) }

    int mStreakMultiplier; // 0x18c
    int mBandMultiplier; // 0x190
    int mMaxMultiplier; // 0x194
    bool mShowBandMult; // 0x198
    ObjPtr<EventTrigger> mNewStreakTrig; // 0x19c
    ObjPtr<EventTrigger> mEndStreakTrig; // 0x1a8
    ObjPtr<EventTrigger> mPeakStateTrig; // 0x1b4
    ObjPtr<EventTrigger> mBreakOverdriveTrig; // 0x1c0
    ObjPtr<RndTransAnim> mMultiMeterAnim; // 0x1cc
    ObjPtr<BandLabel> mMultiplierLabel; // 0x1d8
    ObjPtr<BandLabel> mXLabel; // 0x1e4
    ObjPtr<RndPropAnim> mMeterWipeAnim; // 0x1f0
    ObjPtr<EventTrigger> mStarDeployTrig; // 0x1fc
    ObjPtr<EventTrigger> mEndOverdriveTrig; // 0x208
    ObjPtr<EventTrigger> mStarDeployStopTrig; // 0x214
    ObjPtr<EventTrigger> mStarDeployPauseTrig; // 0x220
    ObjPtr<EventTrigger> mResetTrig; // 0x22c
    ObjPtr<EventTrigger> mHideMultiplierTrig; // 0x238
    int unk244; // 0x244
    ObjPtr<EventTrigger> mFlashTrig; // 0x248
    ObjPtr<EventTrigger> mFlashSparksTrig; // 0x254
    bool unk260; // 0x260
    ObjPtr<RndGroup> mPartBarsGroup; // 0x264
    bool unk270[3]; // 0x270
    ObjVector<ObjPtr<RndPropAnim> > mPartColorAnims; // 0x274
    ObjVector<ObjPtr<RndPropAnim> > mPartFadeAnims; // 0x280
    ObjVector<ObjPtr<RndPropAnim> > mPartWipeAnims; // 0x28c
    ObjVector<ObjPtr<RndPropAnim> > mPartWipeResidualAnims; // 0x298
    ObjPtr<EventTrigger> mResidueFadeTrig; // 0x2a4
    ObjPtr<RndPropAnim> mNumPartsAnim; // 0x2b0
    ObjVector<ObjPtr<RndPartLauncher> > mPartSparksLaunchers; // 0x2bc
    int unk2c8; // 0x2c8
    bool unk2cc[3]; // 0x2cc
    int unk2d0; // 0x2d0
    int unk2d4; // 0x2d4
};
