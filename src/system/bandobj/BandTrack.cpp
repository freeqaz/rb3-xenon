// Ported from rb3-Wii src/system/bandobj/BandTrack.cpp (MWCC -> MSVC X360).
#include "bandobj/BandTrack.h"
#include "bandobj/BandCrowdMeter.h"
#include "bandobj/BandLabel.h"
#include "bandobj/CrowdMeterIcon.h"
#include "bandobj/EndingBonus.h"
#include "bandobj/TrackPanelDirBase.h"
#include "bandobj/UnisonIcon.h"
#include "decomp.h"
#include "ui/UILabel.h"
#include "rndobj/Group.h"
#include "obj/Task.h"
#include "os/System.h"
#include "os/Timer.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"
#include "utl/Messages.h"
#include "os/Debug.h"

INIT_REVS(BandTrack)

BandTrack::BandTrack(Hmx::Object *o)
    : mDisabled(0), mSimulatedNet(0), mInstrument(""), unk14(4), unk18(0), unk19(0),
      unk1a(0), unk1b(0), unk1c(0), mInUse(0), unk1e(0), mSoloDisplay(0),
      mPlayerIntro(o, 0), mStarPowerMeter(o, 0), mStreakMeter(o, 0), mPopupObject(o, 0),
      mPlayerFeedback(o, 0), mFailedFeedback(o, 0), mUnisonIcon(o, 0), unk74(""),
      unk78(0), mEndgameFeedback(o, 0), unk88(0), unk8c(0), mParent(o, 0),
      mRetractTrig(o, 0), mResetTrig(o, 0), mDeployTrig(o, 0), mStopDeployTrig(o, 0),
      mIntroTrig(o, 0), unkd8(o, 0), unke4(o, 0), unkf0(o, 0), mBeatAnimsGrp(this, 0),
      mShowOverDriveMeter(1), mShowCrowdMeter(1), mTrackIdx(-1) {}

BEGIN_COPYS(BandTrack)
END_COPYS

BEGIN_LOADS(BandTrack)
END_LOADS

void BandTrack::Save(BinStream &) {}

void BandTrack::StartPulseAnims(float timeTillBeat) {
    static Symbol loop("loop");
    if (mBeatAnimsGrp) {
        mBeatAnimsGrp->Animate(
            0.0f, false, timeTillBeat, RndAnimatable::k480_fpb, 0.0f, 960.0f, 0.0f, 1.0f,
            loop
        );
    }
}

DECOMP_FORCEACTIVE(BandTrack, __FILE__, "0")

void BandTrack::LoadTrack(BinStream &bs, bool b1, bool b2, bool b3) {
    LOAD_REVS(bs);
    ASSERT_REVS(3, 0);
    if (b2) {
        Symbol s;
        bool bbb;
        bs >> bbb;
        bs >> s;
    } else {
        bs >> mSimulatedNet;
        bs >> mInstrument;
    }
    if (gRev >= 1 && !b1) {
        bs >> mStarPowerMeter;
        bs >> mStreakMeter;
    }
    bool finalbool;
    if (gRev < 3) {
        finalbool = false;
        if (!b3 || !b1)
            finalbool = true;
    } else
        finalbool = !b1;
    if (finalbool) {
        bs >> mPlayerIntro;
        if (gRev < 1) {
            bs >> mStarPowerMeter;
            bs >> mStreakMeter;
        }
        bs >> mPopupObject;
        bs >> mPlayerFeedback;
        bs >> mFailedFeedback;
        if (gRev >= 2)
            bs >> mEndgameFeedback;
    }
    if (!b1) {
        bs >> mRetractTrig;
        bs >> mResetTrig;
        bs >> mDeployTrig;
        bs >> mStopDeployTrig;
        bs >> mIntroTrig;
    }
}

void BandTrack::CopyTrack(const BandTrack *c) {
    COPY_MEMBER(mDisabled)
    COPY_MEMBER(mSimulatedNet)
    COPY_MEMBER(mInstrument)
    COPY_MEMBER(mPlayerIntro)
    COPY_MEMBER(mStarPowerMeter)
    COPY_MEMBER(mStreakMeter)
    COPY_MEMBER(mPopupObject)
    COPY_MEMBER(mPlayerFeedback)
    COPY_MEMBER(mFailedFeedback)
    COPY_MEMBER(mEndgameFeedback)
    COPY_MEMBER(mRetractTrig)
    COPY_MEMBER(mResetTrig)
    COPY_MEMBER(mDeployTrig)
    COPY_MEMBER(mStopDeployTrig)
    COPY_MEMBER(mIntroTrig)
}

void BandTrack::Init(Hmx::Object *o) {
    if (o)
        mParent = dynamic_cast<TrackInterface *>(o);
    Reset();
}

void BandTrack::SendTrackerDisplayMessage(const Message &msg) const {
    if (mStarPowerMeter)
        mStarPowerMeter->HandleType(msg);
}

void BandTrack::ResetStreakMeter() {
    if (mStreakMeter) {
        mStreakMeter->Reset();
        if (mParent && mParent->HasPlayer()) {
            mStreakMeter->SetShowing(true);
        }
        SetStreak(0, 1, 1, true);
    }
}

void BandTrack::Reset() {
    if (mStarPowerMeter) {
        mStarPowerMeter->Reset();
        if (mParent && mParent->HasPlayer() && !unk1b) {
            mStarPowerMeter->SetShowing(mShowOverDriveMeter);
        }
        if (mParent)
            mParent->SetCanDeploy(false);
    }
    ResetStreakMeter();
    if (mPlayerFeedback) {
        mPlayerFeedback->HandleType(reset_msg);
        SendTrackerDisplayMessage(disable_msg);
#ifdef HX_NATIVE
        if (UILabel *pctLabel = mPlayerFeedback->Find<UILabel>("solo_percent.lbl", false))
            pctLabel->SetTokenFmt(me_percent_format, 0);
#endif
    }
    if (mFailedFeedback) {
        mFailedFeedback->HandleType(reset_msg);
        if (mParent) {
            if (mParent->GetNoBackFromBrink()) {
                mFailedFeedback->SetProperty(no_saving, DataNode(1));
            }
        }
        delete unkf0;
    }
    ClearFinaleHelp();
    if (mParent) {
        mParent->SetBonusGems(false);
        FillReset();
    }
    if (!mResetTrig)
        mResetTrig = ThisDir()->Find<EventTrigger>("reset.trig", false);
    if (mResetTrig)
        mResetTrig->Trigger();
    unk1c = false;
    TrackReset();
    StopDeploy();
    if (mStreakMeter) {
        mStreakMeter->SetBandMultiplier(1);
        mStreakMeter->EndOverdrive();
    }
    if (mPopupObject) {
        ResetPopup();
        if (mParent) {
            mPopupObject->SetProperty(
                "popup_help_disabled",
                DataNode(mParent->ShouldDisablePopupHelp() || unk1e)
            );
        }
    }
    SetupPlayerIntro();
    if (mParent && mParent->InGame()) {
        SetupCrowdMeter();
    }
    ResetEffectSelector();
}

void BandTrack::DropIn() {
    BandCrowdMeter *meter = GetCrowdMeter();
    if ((meter != nullptr) && meter->Disabled() == 0 && mTrackIdx > -1) {
        meter->DropInPlayer(mTrackIdx);
    }
}

void BandTrack::DropOut() {
    BandCrowdMeter *meter = GetCrowdMeter();
    if ((meter != nullptr) && meter->Disabled() == 0 && mTrackIdx > -1) {
        meter->DropOutPlayer(mTrackIdx);
    }
    if (mParent != nullptr) {
        mParent->DTSPopup(false);
    }
}

int BandTrack::GetPlayerDifficulty() const {
    if (mParent)
        return mParent->GetPlayerDifficulty();
    return 0;
}

bool BandTrack::HasLocalPlayer() const {
    return mParent && mParent->HasLocalPlayer();
}

bool BandTrack::HasNetPlayer() const {
    return mParent && mParent->HasNetPlayer();
}

const char *BandTrack::UserName() const {
    return mParent ? mParent->UserName() : MakeString("");
}

Symbol BandTrack::GetPlayerDifficultySym() const {
    return mParent ? mParent->GetPlayerDifficultySym() : Symbol(gNullStr);
}

TrackPanelDirBase *BandTrack::MyTrackPanelDir() {
    return dynamic_cast<TrackPanelDirBase *>(ThisDir()->Dir());
}

Symbol BandTrack::GetInstrumentSymbol() const { return mInstrument; }

bool BandTrack::SetMultiplier(int mult) {
    if (mult > 0 && mStreakMeter)
        return mStreakMeter->SetMultiplier(mult);
    return false;
}

void BandTrack::SetBandMultiplier(int mult) {
    if (mult > 0 && mStreakMeter)
        mStreakMeter->SetBandMultiplier(mult);
}

void BandTrack::SetControllerType(const Symbol &sym) { unk110 = sym; }

void BandTrack::SetQuarantined(bool b) {
    BandCrowdMeter *meter = GetCrowdMeter();
    if ((meter != nullptr) && meter->Disabled() == 0 && mTrackIdx > -1) {
        meter->SetPlayerQuarantined(mTrackIdx, b);
    }
}

void BandTrack::ResetPlayerFeedback() {
    if (mPlayerFeedback)
        mPlayerFeedback->HandleType(reset_msg);
}

void BandTrack::SetNetTalking(bool talking) {
    // Retail 0x82351E70: both keys are FUNCTION-LOCAL statics sharing guard word
    // 0x82CBE5F4 (talk = bit 0, talk_stop = bit 1), with their own ??__F atexit
    // thunks at 0x82351F74 / 0x82351F94 -- not the file-scope Messages4 globals.
    if (mPlayerIntro) {
        static Message talk_msg("talk");
        static Message talk_stop_msg("talk_stop");
        mPlayerIntro->HandleType(talking ? talk_msg : talk_stop_msg);
    }
}

void BandTrack::SetPlayerFeedbackShowing(bool showing) const {
    if (mPlayerFeedback) {
        if (showing) {
            mPlayerFeedback->HandleType(feedback_on_msg);
        } else {
            mPlayerFeedback->HandleType(feedback_off_msg);
        }
    }
}

void BandTrack::PeakState(bool enabled, bool b2) {
    if (enabled == unk1a)
        return;
    if (enabled && mStreakMeter)
        mStreakMeter->SetPeakState();
    unk1a = enabled;
}

void BandTrack::PlayerDisabled() {
    if (mParent == nullptr) {
        return;
    }
    mParent->DTSPopup(true);
}

void BandTrack::PlayerSaved() {
    if (mParent == nullptr) {
        return;
    }
    mParent->DTSPopup(false);
}

void BandTrack::SuperStreak(bool enabled, bool b2) {
    if (enabled == unk19)
        return;
    if (enabled && mStreakMeter)
        mStreakMeter->SetPeakState();
    unk19 = enabled;
}

void BandTrack::PracticeReset() {
    ThisDir()->Find<EventTrigger>("reset_practice.trig", true)->Trigger();
}

const char *BandTrack::GetTrackIcon() const {
    return mParent ? mParent->GetTrackIcon() : MakeString("G");
}

void BandTrack::ShowOverdriveMeter(bool show) {
    mShowOverDriveMeter = show;
    mStarPowerMeter->SetShowing(show);
}

void BandTrack::SetMaxMultiplier(int mult) {
    unk14 = mult;
    if (mStreakMeter)
        mStreakMeter->mMaxMultiplier = mult;
}

void BandTrack::SetStreak(int i1, int multiplier, int i3, bool b) {
    if (SetMultiplier(multiplier) || b) {
        if (multiplier >= 6) {
            MILO_ASSERT(
                mTrackInstrument == kInstBass || mTrackInstrument == kInstRealBass, 0x1DE
            );
            SuperStreak(true, b);
        } else if (multiplier >= 4) {
            if (mTrackInstrument != kInstBass && mTrackInstrument != kInstRealBass)
                PeakState(true, b);
        } else if (multiplier == 1) {
            if (mStreakMeter)
                mStreakMeter->BreakStreak(!b);
            PeakState(false, b);
            SuperStreak(false, b);
        }
    }
}

void BandTrack::RefreshStreakMeter(int i1, int i2, int i3) {
    if (mStreakMeter) {
        mStreakMeter->SetBandMultiplier(1);
        SetStreak(i1, i2, i3, true);
    }
}

void BandTrack::RefreshOverdrive(float energy, bool b) {
    if (mStarPowerMeter && !unk1b) {
        OverdriveMeter::State state = OverdriveMeter::kFilling;
        if (b)
            state = OverdriveMeter::kReady;
        float delay = 0.0f;
        if (state == OverdriveMeter::kReady) {
            delay = dynamic_cast<TrackPanelDirBase *>(ThisDir()->Dir())
                        ->GetPulseAnimStartDelay(true);
        }
        mStarPowerMeter->SetEnergy(energy, state, mInstrument, delay, true);
        if (mParent)
            mParent->SetCanDeploy(b);
    }
}

void BandTrack::Deploy() {
    if (mDeployTrig)
        mDeployTrig->Trigger();
    BandCrowdMeter *meter = GetCrowdMeter();
    if (meter)
        meter->Deploy(mTrackIdx);
    if (mStreakMeter)
        mStreakMeter->Overdrive();
    if (mParent)
        mParent->SetCanDeploy(false);
    unk18 = true;
}

void BandTrack::StopDeploy() {
    if (mStopDeployTrig)
        mStopDeployTrig->Trigger();
    BandCrowdMeter *meter = GetCrowdMeter();
    if (meter && mTrackIdx >= 0)
        meter->StopDeploy(mTrackIdx);
    if (mStreakMeter) {
        if (mParent && mParent->HasPlayer())
            mStreakMeter->SetBandMultiplier(mParent->GetBandMultiplier());
        mStreakMeter->EndOverdrive();
    }
    unk18 = false;
}

void BandTrack::EnterCoda() {
    EventTrigger *trig = ThisDir()->Find<EventTrigger>("enter_coda.trig", false);
    if (trig)
        trig->Trigger();
    static Message reset_msg("reset");
    if (mPlayerIntro)
        mPlayerIntro->HandleType(reset_msg);
    if (mFailedFeedback)
        mFailedFeedback->HandleType(reset_msg);
    if (mStarPowerMeter)
        mStarPowerMeter->SetShowing(false);
    if (mStreakMeter)
        mStreakMeter->SetShowing(false);
}

void BandTrack::CodaFail(bool guilty) {
    SpotlightFail(guilty);
    if (guilty)
        PopupHelp("rock_ending", false);
}

void BandTrack::CodaSuccess() {
    TrackPanelDirBase *tpd = dynamic_cast<TrackPanelDirBase *>(ThisDir()->Dir());
    if (tpd)
        tpd->CodaSuccess();
    EventTrigger *trig = ThisDir()->Find<EventTrigger>("bre_success.trig", false);
    if (trig)
        trig->Trigger();
    PopupHelp("rock_ending", false);
}

void BandTrack::EnablePlayer() {
    if (mDisabled) {
        delete unkf0;
        EventTrigger *trig = ThisDir()->Find<EventTrigger>("bfb_reset.trig", false);
        if (trig)
            trig->Trigger();
        mDisabled = false;
        if (dynamic_cast<TrackPanelDirBase *>(ThisDir()->Dir())) {
            int idx = mTrackIdx;
            dynamic_cast<TrackPanelDirBase *>(ThisDir()->Dir())->EnablePlayer(idx);
        }
    }
}

void BandTrack::SoloStart() {
    if (!unk1e && !mSoloDisplay) {
        if (mPlayerFeedback)
            mPlayerFeedback->HandleType(start_solo_msg);
#ifdef HX_NATIVE
        if (mPlayerFeedback) {
            UILabel *pctLabel =
                mPlayerFeedback->Find<UILabel>("solo_percent.lbl", true);
            if (pctLabel)
                pctLabel->SetTokenFmt(me_percent_format, 0);
        }
#endif
        EventTrigger *trig =
            ThisDir()->Find<EventTrigger>("guitar_solo_start.trig", false);
        if (trig)
            trig->Trigger();
    }
}

void BandTrack::SetPerformanceMode(bool b) {
    if (mParent && mPopupObject) {
        mPopupObject->SetProperty(
            "popup_help_disabled",
            DataNode(mParent->ShouldDisablePopupHelp() || unk1e)
        );
    }
    unk1e = b;
}

void BandTrack::CombineStreakMultipliers(bool b) {
    if (mStreakMeter)
        mStreakMeter->CombineMultipliers(b);
}

void BandTrack::SetupPlayerIntro() {
    if (mPlayerIntro) {
        mPlayerIntro->HandleType(reset_msg);
        if ((unsigned int)mTrackInstrument <= 7) {
            static Message setIcon = Message("set_icon", DataNode("G"));
            if (mParent) {
                setIcon[0] = DataNode(mParent->GetTrackIcon());
                mParent->SetUserNameLabel(mPlayerIntro, "player_name.lbl");
            }
            mPlayerIntro->HandleType(setIcon);
        }
    }
}

void BandTrack::SetupCrowdMeter() {
    static Hmx::Object *gameMode = ThisDir()->FindObject("gamemode", true);
    mShowCrowdMeter = gameMode->Property("update_crowd_meter", true)->Int();
    BandCrowdMeter *meter = GetCrowdMeter();
    const char *icon = mParent ? mParent->GetTrackIcon() : MakeString("G");
    if (meter && !meter->Disabled() && mTrackIdx > -1) {
        CrowdMeterIcon *micon = meter->PlayerIcon(mTrackIdx);
        micon->SetIcon(icon);
        micon->unk240 = dynamic_cast<TrackPanelDirBase *>(ThisDir()->Dir());
    }
    if (mUnisonIcon)
        ((UnisonIcon *)mUnisonIcon.Ptr())->SetIcon(icon);
}

void BandTrack::SetInstrument(TrackInstrument inst) {
    mTrackInstrument = inst;
    static Symbol guitar("guitar");
    static Symbol bass("bass");
    static Symbol drum("drum");
    static Symbol vocals("vocals");
    static Symbol keys("keys");
    static Symbol real_guitar("real_guitar");
    static Symbol real_bass("real_bass");
    static Symbol real_keys("real_keys");
    static Symbol none("none");
    static Symbol pending("pending");
    switch (inst) {
    case kInstGuitar:
        mInstrument = guitar;
        break;
    case kInstBass:
        mInstrument = bass;
        break;
    case kInstDrum:
        mInstrument = drum;
        break;
    case kInstVocals:
        mInstrument = vocals;
        break;
    case kInstKeys:
        mInstrument = keys;
        break;
    case kInstRealGuitar:
        mInstrument = real_guitar;
        break;
    case kInstRealBass:
        mInstrument = real_bass;
        break;
    case kInstRealKeys:
        mInstrument = real_keys;
        break;
    case kInstNone:
        mInstrument = none;
        break;
    case kInstPending:
        mInstrument = pending;
        break;
    default:
        // Retail keeps the MakeString call here (the notify itself is stripped):
        // target emits `lis/addi fmt; mr r4, inst; bl MakeString` in the default
        // arm.  The default MILO_NOTIFY_ONCE no-op (`(void)sizeof(...)`) does not
        // evaluate it, so the call is written out explicitly.
        MakeString("unrecognized instrument type \"%d\"", inst);
        break;
    }
}

void BandTrack::SyncInstrument() {
    if (mInstrument == guitar) {
        mTrackInstrument = kInstGuitar;
    } else if (mInstrument == bass) {
        mTrackInstrument = kInstBass;
    } else if (mInstrument == drum) {
        mTrackInstrument = kInstDrum;
    } else if (mInstrument == vocals) {
        mTrackInstrument = kInstVocals;
    } else if (mInstrument == keys) {
        mTrackInstrument = kInstKeys;
    } else if (mInstrument == real_guitar) {
        mTrackInstrument = kInstRealGuitar;
    } else if (mInstrument == real_bass) {
        mTrackInstrument = kInstRealBass;
    } else if (mInstrument == real_keys) {
        mTrackInstrument = kInstRealKeys;
    } else {
        MILO_NOTIFY_ONCE(MakeString("unexpected instrument symbol \"%s\"", mInstrument.Str()));

        mTrackInstrument = kInstNone;
    }
}

void BandTrack::Retract(bool b) {
    if (b) {
        Reset();
        EventTrigger *trig =
            ThisDir()->Find<EventTrigger>("immediate_retract.trig", false);
        if (trig)
            trig->Trigger();
    } else {
        if (!mRetractTrig)
            mRetractTrig = ThisDir()->Find<EventTrigger>("retract.trig", false);
        if (mRetractTrig)
            mRetractTrig->Trigger();
        if (mPlayerFeedback) {
            mPlayerFeedback->HandleType(reset_msg);
            SendTrackerDisplayMessage(disable_msg);
        }
    }
}

void BandTrack::StartFinale(unsigned int ui) {
    unk88 = ui;
    unk8c = true;
    GameWon();
    bool bbb = false;
    if (mParent && mParent->HasLocalPlayer())
        bbb = true;
    if (bbb) {
        if (mEndgameFeedback) {
            static Message finale_start("end_game_start_inst", DataNode(""));
            finale_start[0] = DataNode(mInstrument);
            mEndgameFeedback->HandleType(finale_start);
        }
    }
}

void BandTrack::GameWon() {
    if (mPlayerFeedback) {
        mPlayerFeedback->HandleType(reset_msg);
        SendTrackerDisplayMessage(disable_msg);
    }
    GameOver();
}

void BandTrack::GameOver() {
    if (mPlayerIntro)
        mPlayerIntro->HandleType(reset_msg);
    ResetPopup();
    if (mFailedFeedback)
        mFailedFeedback->HandleType(reset_msg);
    Retract(false);
    delete unkf0;
}

BandCrowdMeter *BandTrack::GetCrowdMeter() {
    BandCrowdMeter *meter;
    if (mShowCrowdMeter != false) {
        ObjectDir *objDir = ThisDir()->Dir();
        TrackPanelDirBase *tpDirBase = dynamic_cast<TrackPanelDirBase *>(objDir);
        meter = tpDirBase->GetCrowdMeter();
    } else {
        meter = nullptr;
    }

    return meter;
}

void BandTrack::SetSuppressSoloDisplay(bool b) { mSoloDisplay = b; }

void BandTrack::SoloHide() {
    if (mPlayerFeedback) {
        RndPropAnim *hideSoloAnim =
            mPlayerFeedback->Find<RndPropAnim>("hide_solo.anim", true);
        float endFrame = hideSoloAnim->EndFrame();
        hideSoloAnim->SetFrame(endFrame, 1.0f);
    }
}

void BandTrack::SpotlightFail(bool guilty) {
    if (guilty) {
        EventTrigger *trig =
            ThisDir()->Find<EventTrigger>("spotlight_fail_guilty.trig", false);
        if (trig)
            trig->Trigger();
        if (mUnisonIcon)
            ((UnisonIcon *)mUnisonIcon.Ptr())->Fail();
    } else {
        EventTrigger *trig = ThisDir()->Find<EventTrigger>("spotlight_fail.trig", false);
        if (trig)
            trig->Trigger();
    }
}

void BandTrack::SpotlightPhraseSuccess() {
    if (dynamic_cast<TrackPanelDirBase *>(ThisDir()->Dir())) {
        if (dynamic_cast<TrackPanelDirBase *>(ThisDir()->Dir())->GetEndingBonus()) {
            int trackIdx = mTrackIdx;
            dynamic_cast<TrackPanelDirBase *>(ThisDir()->Dir())->GetEndingBonus()->PlayerSuccess(trackIdx);
        }
    }
    if (mUnisonIcon)
        ((UnisonIcon *)mUnisonIcon.Ptr())->Succeed();
}

void BandTrack::PopupHelp(Symbol sym, bool b) {
    if (sym == unk74 && b == unk78)
        return;
    if (!mPopupObject)
        return;
    if (!mParent)
        return;
    if (mParent->PlayerDisabled())
        return;
    static Message msg("help", DataNode(""), DataNode(0), DataNode(""), DataNode(""));
    msg[0] = sym;
    msg[1] = b;
    msg[2] = mInstrument;
    msg[3] = unk110;
    bool handled = mPopupObject->Handle(msg, true).Int() != 0;
    if (b) {
        unk74 = sym;
        unk78 = handled;
    } else if (sym == unk74) {
        unk74 = gNullStr;
        unk78 = false;
    }
}

void BandTrack::ClearFinaleHelp() {
    if (unk8c) {
        if (mEndgameFeedback) {
        TIMER_GET_CYCLES(cycle);
        float delay = 0;
        float elapsed = Timer::CyclesToMs(cycle - unk88);
        static float kMinFinaleHelpTime =
            SystemConfig("objects", ThisDir()->ClassName(), "min_finale_help_time")->Float(1);
        if (elapsed < kMinFinaleHelpTime)
            delay = kMinFinaleHelpTime - elapsed;
        TheTaskMgr.Start(
            new MessageTask(mEndgameFeedback, end_game_end_msg), kTaskSeconds, delay
        );
        unk8c = false;
    }
    }
}

void BandTrack::ResetPopup() {
    if (mPopupObject) {
        if (unk78) {
            PopupHelp(unk74, false);
        }
        mPopupObject->Find<EventTrigger>("reset.trig", true)->Trigger();
        unk74 = "";
        unk78 = false;
        mPopupObject->Handle(reset_msg, true);
    }
}

void BandTrack::FillReset() {
    EventTrigger *trig = ThisDir()->Find<EventTrigger>("solo_reset.trig", false);
    if (trig)
        trig->Trigger();
}

void BandTrack::UnisonEnd() {
    if (mUnisonIcon)
        ((UnisonIcon *)mUnisonIcon.Ptr())->UnisonEnd();
}

void BandTrack::UnisonStart() {
    if (mUnisonIcon)
        ((UnisonIcon *)mUnisonIcon.Ptr())->UnisonStart();
}

void BandTrack::SoloHit(int i) {
    if (mPlayerFeedback && !unk1e && !mSoloDisplay) {
        mPlayerFeedback->Find<UILabel>("solo_percent.lbl", true)
            ->SetTokenFmt(me_percent_format, i);
    }
}

void BandTrack::SoloEnd(int i, Symbol sym) {
    if (!unk1e && !mSoloDisplay) {
        if (mPlayerFeedback) {
            mPlayerFeedback->Find<BandLabel>("solo_rating.lbl", true)->SetTextToken(sym);
            mPlayerFeedback->Find<UILabel>("score.lbl", true)->SetInt(i, true);
            mPlayerFeedback->HandleType(end_solo_msg);
        }
        EventTrigger *trig =
            ThisDir()->Find<EventTrigger>("guitar_solo_stop.trig", false);
        if (trig)
            trig->Trigger();
    }
}

void BandTrack::PlayIntro() {
    if (mParent && mParent->FailedAtStart()) {
        DisablePlayer(0);
    } else if (mPlayerIntro && mParent && !mParent->InGameMode("practice")) {
        if (mSimulatedNet || (mParent && !mParent->IsLocal())) {
            static Message intro_remote_msg("intro_remote");
            mPlayerIntro->Handle(intro_remote_msg, true);
        } else {
            static Message intro_msg("intro");
            mPlayerIntro->Handle(intro_msg, true);
        }
        if (mParent && mParent->PlayerDisconnected()) {
            DisablePlayer(0);
        }
    }
    if (mParent) {
        mParent->RefreshPlayerHUD();
        mParent->SetPlayingIntro(1000.0f);
    }
}

void BandTrack::SavePlayer() {
    if (!mDisabled)
        return;
    delete unkf0;
    EventTrigger *trig = ThisDir()->Find<EventTrigger>("bfb_saved.trig", false);
    if (trig)
        trig->Trigger();
    if (mFailedFeedback) {
        if (mParent && mParent->HasLocalPlayer()) {
            static Message icon_hide_msg("icon_hide");
            mPlayerIntro->Handle(icon_hide_msg, true);
        }
        static Message saved_msg("saved");
        mFailedFeedback->Handle(saved_msg, true);
    }
    if (mParent)
        mParent->SetGemsEnabledByPlayer();
    if (MyTrackPanelDir()) {
        MyTrackPanelDir()->EnablePlayer(mTrackIdx);
    }
}

void BandTrack::SetCrowdRating(float f, CrowdMeterState state) {
    BandCrowdMeter *meter = GetCrowdMeter();
    int &_ref0 = mTrackIdx;
    if (meter && !meter->Disabled() && _ref0 > -1) {
        meter->SetPlayerValue(_ref0, f);
        if (state != kCrowdMeterInvalidState) {
            meter->SetPlayerIconState(_ref0, state);
            bool isWarning = state == kCrowdMeterWarning;
            bool isFailed = state == kCrowdMeterFailed;
            if (isWarning != (bool)unk1c && !isFailed) {
                unk1c = isWarning;
                RndAnimatable *anim =
                    dynamic_cast<RndAnimatable *>(
                        ThisDir()->FindObject("warning_anims.grp", false)
                    );
                if (anim) {
                    if (unk1c) {
                        anim->SetFrame(0.0f, 1.0f);
                        TrackPanelDirBase *tpd = dynamic_cast<TrackPanelDirBase *>(
                            ThisDir()->Dir()
                        );
                        float startDelay = tpd->GetPulseAnimStartDelay(false);
                        anim->Animate(
                            0.0f, false, startDelay,
                            RndAnimatable::k1_fpb, 0.0f, 1.0f, 0.0f, 1.0f, loop
                        );
                    } else {
                        anim->Animate(
                            0.0f, false, 0.0f, RndAnimatable::k1_fpb, 0.0f, 1.0f,
                            0.0f, 1.0f, dest
                        );
                    }
                }
            }
        }
    }
}

void BandTrack::DisablePlayer(int i) {
    bool disconnected;
    if (mParent)
        disconnected = mParent->PlayerDisconnected();
    else
        disconnected = false;
    if (mDisabled && disconnected && mFailedFeedback) {
        mFailedFeedback->HandleType(reset_msg);
        unkd8 = new MessageTask(mFailedFeedback, disconnected_msg);
        TheTaskMgr.Start(unkd8, kTaskSeconds, 0.0f);
    }
    if (mDisabled && !disconnected) {
        if (!mParent)
            return;
        if (!mParent->FailedAtStart())
            return;
    }
    EventTrigger *trig = ThisDir()->Find<EventTrigger>("bfb_failed.trig", false);
    if (trig)
        trig->Trigger();
    if (mPlayerFeedback) {
        mPlayerFeedback->HandleType(reset_msg);
        SendTrackerDisplayMessage(disable_msg);
    }
    ResetPopup();
    mDisabled = true;
    if (mParent)
        mParent->SetGemsEnabled(-1.0f);
    if (dynamic_cast<TrackPanelDirBase *>(ThisDir()->Dir())) {
        bool atStart = mParent && mParent->PlayerDisconnectedAtStart();
        int idx = mTrackIdx;
        dynamic_cast<TrackPanelDirBase *>(ThisDir()->Dir())->DisablePlayer(idx, atStart);
    }
    static Message failed("failed_task", DataNode(0), DataNode(0));
    failed[0] = disconnected;
    failed[1] = i;
    unkf0 = new MessageTask(ThisDir(), failed);
    TheTaskMgr.Start(unkf0, kTaskSeconds, 1.0f);
}

void BandTrack::FailedTask(bool b, int i) {
    if (!mFailedFeedback)
        return;
    if (mPlayerIntro && mTrackInstrument != kInstVocals && mParent
        && mParent->HasLocalPlayer()) {
        mPlayerIntro->Handle(icon_show_msg, true);
    }
    if (b) {
        mFailedFeedback->Handle(disconnected_msg, true);
    } else {
        static Message failed("failed", DataNode(0));
        failed[0] = i;
        mFailedFeedback->Handle(failed, true);
    }
}

void BandTrack::SetTourMomentGoalText(const char *top, const char *bottom) {
    MILO_ASSERT(top, 0x4E8);
    MILO_ASSERT(bottom, 0x4E9);
    if (mPlayerFeedback) {
        RndDir *goal = mPlayerFeedback->Find<RndDir>("tour_moment_goal", true);
        if (goal) {
            BandLabel *topLabel = goal->Find<BandLabel>("tg_main_text_top.lbl", true);
            BandLabel **topPtr;
            if ((topPtr = new BandLabel*) != 0) {
                *topPtr = topLabel;
                (*topPtr)->SetDisplayText(top, true);
                delete topPtr;
            }
            BandLabel *bottomLabel = goal->Find<BandLabel>("tg_main_text_bottom.lbl", true);
            BandLabel **bottomPtr;
            if ((bottomPtr = new BandLabel*) != 0) {
                *bottomPtr = bottomLabel;
                (*bottomPtr)->SetDisplayText(bottom, true);
                delete bottomPtr;
            }
        }
    }
}

void BandTrack::SetHasTrackerFocus(bool b) {
    if (mPlayerFeedback) {
        RndDir *goal =
            mPlayerFeedback->Find<RndDir>("tour_moment_goal", true);
        if (goal) {
            EventTrigger *trig;
            if (b) {
                trig = goal->Find<EventTrigger>("show.trig", true);
                SetTourMomentGoalText("Spotlight", "");
            } else {
                trig = goal->Find<EventTrigger>("hide.trig", true);
            }
            trig->Trigger();
        }
    }
}

RndDir *BandTrack::AsRndDir() {
    MILO_FAIL("BandTrack::AsRndDir() base impl should never be called");
    return 0;
}

BEGIN_PROPSYNCS(BandTrack)
    SYNC_PROP_MODIFY(instrument, mInstrument, SyncInstrument())
    SYNC_PROP(disabled, mDisabled)
    SYNC_PROP(simulated_net, mSimulatedNet)
    SYNC_PROP(player_intro, mPlayerIntro)
    SYNC_PROP(star_power_meter, mStarPowerMeter)
    SYNC_PROP(streak_meter, mStreakMeter)
    SYNC_PROP(popup_object, mPopupObject)
    SYNC_PROP(player_feedback, mPlayerFeedback)
    SYNC_PROP(failed_feedback, mFailedFeedback)
    SYNC_PROP(endgame_feedback, mEndgameFeedback)
    SYNC_PROP(parent, mParent)
    SYNC_PROP(retract_trig, mRetractTrig)
    SYNC_PROP(reset_trig, mResetTrig)
    SYNC_PROP(deploy_trig, mDeployTrig)
    SYNC_PROP(stop_deploy_trig, mStopDeployTrig)
    SYNC_PROP(intro_trig, mIntroTrig)
    SYNC_PROP(in_use, mInUse)
END_PROPSYNCS

#pragma push
#pragma dont_inline on
BEGIN_HANDLERS(BandTrack)
    HANDLE_ACTION(disable_player, DisablePlayer(_msg->Int(2)))
    HANDLE_ACTION(init, Init(_msg->Obj<Hmx::Object>(2)))
    HANDLE_ACTION(reset, Reset())
    HANDLE_ACTION(tut_reset, TutorialReset())
    HANDLE_ACTION(setup_player_intro, SetupPlayerIntro())
    HANDLE_ACTION(setup_crowd_meter, SetupCrowdMeter())
    HANDLE_ACTION(track_reset, TrackReset())
    HANDLE_ACTION(retract, Retract(false))
    HANDLE_ACTION(immediate_retract, Retract(true))
    HANDLE_ACTION(game_won, GameWon())
    HANDLE_ACTION(game_won_finale, GameWon())
    HANDLE_ACTION(game_over, GameOver())
    HANDLE_ACTION(spotlight_fail, SpotlightFail(false))
    HANDLE_ACTION(spotlight_fail_guilty, SpotlightFail(true))
    HANDLE_ACTION(fill_reset, FillReset())
    HANDLE_ACTION(set_streak, SetStreak(_msg->Int(2), _msg->Int(3), _msg->Int(4), false))
    HANDLE_ACTION(enable_player, EnablePlayer())
    HANDLE_ACTION(save_player, SavePlayer())
    HANDLE_ACTION(super_streak, SuperStreak(_msg->Int(2), false))
    HANDLE_ACTION(peak_state, PeakState(_msg->Int(2), false))
    HANDLE_ACTION(deploy, Deploy())
    HANDLE_ACTION(stop_deploy, StopDeploy())
    HANDLE_ACTION(play_intro, PlayIntro())
    HANDLE_ACTION(set_multiplier, SetMultiplier(_msg->Int(2)))
    HANDLE_ACTION(enter_coda, EnterCoda())
    HANDLE_ACTION(coda_blown, CodaFail(false))
    HANDLE_ACTION(coda_fail, CodaFail(true))
    HANDLE_ACTION(coda_success, CodaSuccess())
    HANDLE_ACTION(popup_help, PopupHelp(_msg->Sym(2), _msg->Int(3)))
    HANDLE_ACTION(player_disabled, PlayerDisabled())
    HANDLE_ACTION(player_saved, PlayerSaved())
    HANDLE_ACTION(failed_task, FailedTask(_msg->Int(2), _msg->Int(3)))
    HANDLE_EXPR(has_net_player, HasNetPlayer())
    HANDLE_EXPR(has_local_player, HasLocalPlayer())
    HANDLE_EXPR(get_player_difficulty, GetPlayerDifficulty())
    HANDLE_ACTION(set_player_local, SetPlayerLocal(_msg->Float(2)))
    HANDLE_ACTION(set_tambourine, SetTambourine(_msg->Int(2)))
    HANDLE_ACTION(setup_instrument, SetupInstrument())
    HANDLE_CHECK(0x5A9)
END_HANDLERS
#pragma pop
