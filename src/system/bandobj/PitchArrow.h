#pragma once
#include "obj/ObjMacros.h"
#include "rndobj/Dir.h"
#include "rndobj/EventTrigger.h"
#include "rndobj/Group.h"
#include "rndobj/PropAnim.h"
#include "bandobj/StreakMeter.h"

class PitchArrow : public RndDir {
public:
    PitchArrow();
    OBJ_CLASSNAME(PitchArrowDir);
    OBJ_SET_TYPE(PitchArrowDir);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual ~PitchArrow() {}
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    virtual void SyncObjects();
    virtual void Poll();

    void SetArrowStyle(int);
    void Reset(RndGroup *);
    void SetPitched(bool);
    void SetSpotlight(bool);
    void SetDeploying(bool);
    void Clear();
    void ClearParticles();
    void SetTiltDegrees(float);
    void SetFrameScore(float, VocalHUDColor, float);
    void SetColor(VocalHUDColor);
    void SetColorFade(float);
    void SetVolume(float);
    void SetSplit(bool);
    void PollHelix();
    void SetGhostFade(float);

    DataNode OnSyncColor(DataArray *);
    DataNode OnSetupFx(DataArray *);

    static bool NeedSort(PitchArrow *);
    NEW_OVERLOAD;
    DELETE_OVERLOAD;
    NEW_OBJ(PitchArrow)
    static void Init() { Register(); }
    REGISTER_OBJ_FACTORY_FUNC(PitchArrow)

    bool unk18c; // 0x18c
    float mScore; // 0x190
    float mHarmonyFX; // 0x194
    float mVolume; // 0x198
    float mTilt; // 0x19c
    VocalHUDColor mVocalHUDColor; // 0x1a0
    float mColorFade; // 0x1a4
    bool mSpotlight; // 0x1a8
    bool mDeploying; // 0x1a9
    bool mPitched; // 0x1aa
    bool unk1ab; // 0x1ab
    Symbol mTestColor; // 0x1ac
    int mArrowStyle; // 0x1b0
    ObjPtr<RndPropAnim> mScoreAnim; // 0x1b4
    ObjPtr<RndPropAnim> mHarmonyFXAnim; // 0x1c0
    ObjPtr<RndPropAnim> mVolumeAnim; // 0x1cc
    ObjPtr<RndPropAnim> mTiltAnim; // 0x1d8
    ObjPtr<RndPropAnim> mColorAnim; // 0x1e4
    ObjPtr<RndPropAnim> mColorFadeAnim; // 0x1f0
    ObjPtr<RndPropAnim> mSplitAnim; // 0x1fc
    ObjPtr<RndPropAnim> mArrowStyleAnim; // 0x208
    ObjPtr<EventTrigger> mSetPitchedTrig; // 0x214
    ObjPtr<EventTrigger> mSetUnpitchedTrig; // 0x220
    ObjPtr<EventTrigger> mSpotlightStartTrig; // 0x22c
    ObjPtr<EventTrigger> mSpotlightEndTrig; // 0x238
    ObjPtr<EventTrigger> mDeployStartTrig; // 0x244
    ObjPtr<EventTrigger> mDeployEndTrig; // 0x250
    ObjPtr<RndGroup> mGhostGrp; // 0x25c
    ObjPtr<RndPropAnim> mGhostFadeAnim; // 0x268
    ObjPtr<RndGroup> mArrowFXGrp; // 0x274
    bool unk280; // 0x280
    float mSpinSpeed; // 0x284
    ObjPtr<RndPropAnim> mSpinAnim; // 0x288
    float mSpinRestFrame; // 0x294
    float mSpinBeginFrame; // 0x298
    float mSpinEndFrame; // 0x29c
};

VocalHUDColor GetVocalHUDColor(Symbol s);