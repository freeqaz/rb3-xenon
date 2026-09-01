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

    int mStreakMultiplier; // 0x1dc
    int mBandMultiplier; // 0x1e0
    int mMaxMultiplier; // 0x1e4
    bool mShowBandMult; // 0x1e8
    ObjPtr<EventTrigger> mNewStreakTrig; // 0x1ec
    ObjPtr<EventTrigger> mEndStreakTrig; // 0x1f8
    ObjPtr<EventTrigger> mPeakStateTrig; // 0x204
    ObjPtr<EventTrigger> mBreakOverdriveTrig; // 0x210
    ObjPtr<RndTransAnim> mMultiMeterAnim; // 0x21c
    ObjPtr<BandLabel> mMultiplierLabel; // 0x228
    ObjPtr<BandLabel> mXLabel; // 0x234
    ObjPtr<RndPropAnim> mMeterWipeAnim; // 0x240
    ObjPtr<EventTrigger> mStarDeployTrig; // 0x24c
    ObjPtr<EventTrigger> mEndOverdriveTrig; // 0x258
    ObjPtr<EventTrigger> mStarDeployStopTrig; // 0x264
    ObjPtr<EventTrigger> mStarDeployPauseTrig; // 0x270
    ObjPtr<EventTrigger> mResetTrig; // 0x27c
    ObjPtr<EventTrigger> mHideMultiplierTrig; // 0x288
    int unk244; // 0x294
    ObjPtr<EventTrigger> mFlashTrig; // 0x298
    ObjPtr<EventTrigger> mFlashSparksTrig; // 0x2a4
    bool unk260; // 0x2b0
    ObjPtr<RndGroup> mPartBarsGroup; // 0x2b4
    bool unk270[3]; // 0x2c0
    ObjVector<ObjPtr<RndPropAnim> > mPartColorAnims; // 0x2c4
    ObjVector<ObjPtr<RndPropAnim> > mPartFadeAnims; // 0x2d4
    ObjVector<ObjPtr<RndPropAnim> > mPartWipeAnims; // 0x2e4
    ObjVector<ObjPtr<RndPropAnim> > mPartWipeResidualAnims; // 0x2f4
    ObjPtr<EventTrigger> mResidueFadeTrig; // 0x304
    ObjPtr<RndPropAnim> mNumPartsAnim; // 0x310
    ObjVector<ObjPtr<RndPartLauncher> > mPartSparksLaunchers; // 0x31c
    int unk2c8; // 0x32c
    bool unk2cc[3]; // 0x330
    int unk2d0; // 0x334
    int unk2d4; // 0x338
};
