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

    bool unk18c; // 0x1dc
    float mScore; // 0x1e0
    float mHarmonyFX; // 0x1e4
    float mVolume; // 0x1e8
    float mTilt; // 0x1ec
    VocalHUDColor mVocalHUDColor; // 0x1f0
    float mColorFade; // 0x1f4
    bool mSpotlight; // 0x1f8
    bool mDeploying; // 0x1f9
    bool mPitched; // 0x1fa
    bool unk1ab; // 0x1fb
    Symbol mTestColor; // 0x1fc
    int mArrowStyle; // 0x200
    ObjPtr<RndPropAnim> mScoreAnim; // 0x204
    ObjPtr<RndPropAnim> mHarmonyFXAnim; // 0x210
    ObjPtr<RndPropAnim> mVolumeAnim; // 0x21c
    ObjPtr<RndPropAnim> mTiltAnim; // 0x228
    ObjPtr<RndPropAnim> mColorAnim; // 0x234
    ObjPtr<RndPropAnim> mColorFadeAnim; // 0x240
    ObjPtr<RndPropAnim> mSplitAnim; // 0x24c
    ObjPtr<RndPropAnim> mArrowStyleAnim; // 0x258
    ObjPtr<EventTrigger> mSetPitchedTrig; // 0x264
    ObjPtr<EventTrigger> mSetUnpitchedTrig; // 0x270
    ObjPtr<EventTrigger> mSpotlightStartTrig; // 0x27c
    ObjPtr<EventTrigger> mSpotlightEndTrig; // 0x288
    ObjPtr<EventTrigger> mDeployStartTrig; // 0x294
    ObjPtr<EventTrigger> mDeployEndTrig; // 0x2a0
    ObjPtr<RndGroup> mGhostGrp; // 0x2ac
    ObjPtr<RndPropAnim> mGhostFadeAnim; // 0x2b8
    ObjPtr<RndGroup> mArrowFXGrp; // 0x2c4
    bool unk280; // 0x2d0
    float mSpinSpeed; // 0x2d4
    ObjPtr<RndPropAnim> mSpinAnim; // 0x2d8
    float mSpinRestFrame; // 0x2e4
    float mSpinBeginFrame; // 0x2e8
    float mSpinEndFrame; // 0x2ec
};

VocalHUDColor GetVocalHUDColor(Symbol s);