#pragma once
// Ported from rb3-Wii src/system/bandobj/BandTrack.h.
// ObjPtr<T,ObjectDir> -> ObjPtr<T> (retail/dc3 single-arg form).
// UnisonIcon/BandCrowdMeter forward-declared (used only via ObjPtr/pointer) to
// avoid pulling their full headers into every BandTrack.h includer.
#include "obj/Object.h"
#include "bandobj/CrowdMeterIcon.h"
#include "rndobj/Dir.h"
#include "rndobj/EventTrigger.h"
#include "rndobj/Group.h"
#include "bandobj/OverdriveMeter.h"
#include "bandobj/StreakMeter.h"
#include "bandobj/TrackInterface.h"
#include "bandobj/TrackInstruments.h"
#include "obj/Task.h"

class TrackPanelDirBase;
class GemTrackDir;
class VocalTrackDir;
class UnisonIcon;
class BandCrowdMeter;

class BandTrack : public virtual Hmx::Object {
public:
    BandTrack(Hmx::Object *);
    OBJ_CLASSNAME(BandTrack);
    OBJ_SET_TYPE(BandTrack);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void Reset();
    virtual void TrackReset() {}
    virtual void ResetSmashers(bool) {}
    virtual void Retract(bool);
    virtual void Extend(bool) {}
    virtual void SpotlightPhraseSuccess();
    virtual void SetStreak(int, int, int, bool);
    virtual void Deploy();
    virtual void StopDeploy();
    virtual void EnterCoda();
    virtual void PlayIntro();
    virtual void DisablePlayer(int);
    virtual void SavePlayer();
    virtual void SuperStreak(bool, bool);
    virtual void PeakState(bool, bool);
    virtual void SetTambourine(bool);
    virtual void SetPlayerLocal(float) {}
    virtual void SetHasTrackerFocus(bool);
    virtual ObjectDir *ThisDir() {
        MILO_ASSERT(0, 0x8A);
        return 0;
    }
    virtual ObjectDir *ThisDir() const {
        MILO_ASSERT(0, 0x8B);
        return 0;
    }
    virtual GemTrackDir *AsGemTrackDir() { return 0; }
    virtual VocalTrackDir *AsVocalTrackDir() { return 0; }
    virtual RndDir *AsRndDir();
    virtual void RefreshStreakMeter(int, int, int);
    virtual void RefreshOverdrive(float, bool);
    virtual void RefreshCrowdRating(float, CrowdMeterState) {}
    virtual void StartPulseAnims(float);
    virtual void SetupInstrument() {}
    virtual void SetPerformanceMode(bool);
    virtual void SetUsed(bool b) { mInUse = b; }
    virtual void SetInstrument(TrackInstrument);
    virtual void ResetEffectSelector() {}
    virtual void SetupSmasherPlate() {}
    virtual void ReleaseSmasherPlate() {}
    virtual void TutorialReset() {}
    virtual ~BandTrack() {}

    void Init(Hmx::Object *);
    void LoadTrack(BinStream &, bool, bool, bool);
    void CopyTrack(const BandTrack *);
    void ResetStreakMeter();
    void SendTrackerDisplayMessage(const Message &) const;
    void ClearFinaleHelp();
    void FillReset();
    void ResetPopup();
    void SetupPlayerIntro();
    void SetupCrowdMeter();
    void PracticeReset();
    void ShowOverdriveMeter(bool);
    void SetMaxMultiplier(int);
    BandCrowdMeter *GetCrowdMeter();
    void StartFinale(unsigned int);
    void GameWon();
    void GameOver();
    void SpotlightFail(bool);
    void SyncInstrument();
    void EnablePlayer();
    bool SetMultiplier(int);
    void CodaFail(bool);
    void CodaSuccess();
    void PopupHelp(Symbol, bool);
    void PlayerDisabled();
    void PlayerSaved();
    void FailedTask(bool, int);
    bool HasNetPlayer() const;
    bool HasLocalPlayer() const;
    int GetPlayerDifficulty() const;
    void SetCrowdRating(float, CrowdMeterState);
    void SetSuppressSoloDisplay(bool);
    void DropIn();
    void DropOut();
    void SoloHide();
    void UnisonEnd();
    void UnisonStart();
    void SetBandMultiplier(int);
    void SoloEnd(int, Symbol);
    const char *GetTrackIcon() const;
    Symbol GetPlayerDifficultySym() const;
    TrackPanelDirBase *MyTrackPanelDir();
    Symbol GetInstrumentSymbol() const;
    void CombineStreakMultipliers(bool);
    const char *UserName() const;
    void SetQuarantined(bool);
    void SetNetTalking(bool);
    void SetControllerType(const Symbol &);
    void SetPlayerFeedbackShowing(bool) const;
    void ResetPlayerFeedback();
    void SoloStart();
    void SoloHit(int);
    void SetTourMomentGoalText(const char *, const char *);

    TrackInstrument GetInstrument() const { return mTrackInstrument; }
    bool InUse() const { return mInUse; }
    void SetTrackIdx(int idx) { mTrackIdx = idx; }
    void SetSimulatedNet(bool net) { mSimulatedNet = net; }

    DECLARE_REVS;
    NEW_OVERLOAD;
    DELETE_OVERLOAD;

    bool mDisabled; // 0x8
    bool mSimulatedNet; // 0x9
    Symbol mInstrument; // 0xc
    TrackInstrument mTrackInstrument; // 0x10
    int unk14; // 0x14
    bool unk18; // 0x18
    bool unk19; // 0x19
    bool unk1a; // 0x1a
    bool unk1b; // 0x1b
    bool unk1c; // 0x1c
    bool mInUse; // 0x1d
    bool unk1e; // 0x1e
    bool mSoloDisplay; // 0x1f
    ObjPtr<RndDir> mPlayerIntro; // 0x20
    ObjPtr<OverdriveMeter> mStarPowerMeter; // 0x2c
    ObjPtr<StreakMeter> mStreakMeter; // 0x38
    ObjPtr<RndDir> mPopupObject; // 0x44
    ObjPtr<RndDir> mPlayerFeedback; // 0x50
    ObjPtr<RndDir> mFailedFeedback; // 0x5c
    ObjPtr<RndDir> mUnisonIcon; // 0x68  (UnisonIcon; RndDir keeps size/layout, avoids header tail)
    Symbol unk74; // 0x74
    bool unk78; // 0x78
    ObjPtr<RndDir> mEndgameFeedback; // 0x7c
    unsigned int unk88; // 0x88
    bool unk8c; // 0x8c
    ObjPtr<TrackInterface> mParent; // 0x90
    ObjPtr<EventTrigger> mRetractTrig; // 0x9c
    ObjPtr<EventTrigger> mResetTrig; // 0xa8
    ObjPtr<EventTrigger> mDeployTrig; // 0xb4
    ObjPtr<EventTrigger> mStopDeployTrig; // 0xc0
    ObjPtr<EventTrigger> mIntroTrig; // 0xcc
    ObjPtr<Task> unkd8; // 0xd8
    ObjPtr<Task> unke4; // 0xe4
    ObjPtr<Task> unkf0; // 0xf0
    ObjPtr<RndGroup> mBeatAnimsGrp; // 0xfc
    bool mShowOverDriveMeter; // 0x108
    bool mShowCrowdMeter; // 0x109
    int mTrackIdx; // 0x10c
    Symbol unk110; // 0x110
};
