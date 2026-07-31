#include "game/FreestylePanel.h"
#include "beatmatch/BeatMatchController.h"
#include "beatmatch/BeatMatchUtl.h"
#include "beatmatch/TrackType.h"
#include "game/BandUser.h"
#include "game/GameConfig.h"
#include "game/Metronome.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/TrainingMgr.h"
#include "obj/Data.h"
#include "obj/ObjMacros.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "os/JoypadMsgs.h"
#include "os/System.h"
#include "synth/Sequence.h"
#include "synth/Sfx.h"
#include "os/Joypad.h"
#include "ui/UI.h"
#include "ui/UIPanel.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"

FreestylePanel::FreestylePanel()
    : mController(0), mUser(0), mSecsPerBeat(0.5f), mBeatTimer(0), mBeatCount(0), mMetronome(0), mSoloEnabled(1),
      mSoloStartSecs(-1), mLastSwingSecs(0), mFreestylePaused(0) {
    mMetronome = new Metronome();
}

FreestylePanel::~FreestylePanel() { delete mMetronome; }

void FreestylePanel::Enter() {
    UIPanel::Enter();
    CreateController();
    JoypadSubscribe(this);
    mMetronome->Enter(
        DataDir()->Find<Sfx>("metronome_hi.cue", true),
        DataDir()->Find<Sfx>("metronome_lo.cue", true)
    );
    mFreestylePaused = false;
}

void FreestylePanel::Exit() {
    UIPanel::Exit();
    JoypadUnsubscribe(this);
    RELEASE(mController);
    mUser = 0;
    mMetronome->Exit();
}

void FreestylePanel::Poll() {
    UIPanel::Poll();
    if (TheUI->FocusPanel() != this || !mUser || !mController || mFreestylePaused)
        return;
    mController->SetSecondPedalHiHat(TheProfileMgr.GetSecondPedalHiHat());
    if (mMetronome->Enabled()) {
        mBeatTimer -= TheTaskMgr.DeltaUISeconds();
        if (mBeatTimer < 0) {
            mMetronome->PlayBeat(mBeatCount);
            mBeatCount = (mBeatCount + 1) % 4;
            while (mBeatTimer < 0) {
                mBeatTimer += mSecsPerBeat;
            }
        }
    }
    HandleSolo();
}

void FreestylePanel::HandleSolo() {
    if (mSoloEnabled && mSoloStartSecs >= 0) {
        float uisecs = TheTaskMgr.UISeconds();
        if (uisecs - mLastSwingSecs > 0.7f) {
            float f1 = uisecs - mSoloStartSecs;
            if (f1 > 60.0f) {
                Sequence *seq = mDir->Find<Sequence>("applause_sexy.cue", false);
                if (seq)
                    seq->Play(0, 0, 0);
            } else if (f1 > 10.0f) {
                Sequence *seq = mDir->Find<Sequence>("applause_ultra.cue", false);
                if (seq)
                    seq->Play(0, 0, 0);
            } else if (f1 > 5.0f) {
                Sequence *seq = mDir->Find<Sequence>("applause.cue", false);
                if (seq)
                    seq->Play(0, 0, 0);
            }
            mLastSwingSecs = -1.0f;
            mSoloStartSecs = -1.0f;
        }
    }
    if (!mUser || !mController)
        return;
    else {
        bool lefty = mController->mLefty;
        if (lefty != mUser->GetGameplayOptions()->GetLefty()) {
            mController->mLefty = mUser->GetGameplayOptions()->GetLefty();
        }
    }
}

bool FreestylePanel::Swing(int i1, bool, bool, bool, bool, GemHitFlags) {
    if (TheUI->FocusPanel() != this || mFreestylePaused)
        return false;
    int slot = mController->GetVirtualSlot(i1);
    if (slot == -1)
        return false;
    float db = VelocityBucketToDb(mController->GetVelocityBucket(i1));
    Message pad_hit_msg("pad_hit", slot, db);
    Handle(pad_hit_msg, true);
    if (mSoloEnabled) {
        float secs = TheTaskMgr.UISeconds();
        if (mSoloStartSecs < 0)
            mSoloStartSecs = secs;
        mLastSwingSecs = secs;
    }
    return false;
}

void FreestylePanel::SetBpm(int i) { mSecsPerBeat = 60.0f / i; }

void FreestylePanel::CreateController() {
    RELEASE(mController);
    mUser = GetFreestyleUser();
    MILO_ASSERT(mUser, 0xCB);
    Symbol ctrl = TheGameConfig->GetController(mUser);
    DataArray *cfg = SystemConfig("beatmatcher", "controller", ctrl);
    BandUser *u = mUser;
    mController = NewController(
        u, cfg, this, false, u->GetGameplayOptions()->GetLefty(), kNumTrackTypes
    );
    mController->mGemMapping = kDrumGemMapping;
    mController->UseAlternateMapping(true);
    mController->SetSecondPedalHiHat(TheProfileMgr.GetSecondPedalHiHat());
    mController->SetCymbalConfiguration(TheProfileMgr.GetCymbalConfiguration());
}

BandUser *FreestylePanel::GetFreestyleUser() {
    TrainingMgr *pTrainingMgr = TrainingMgr::GetTrainingMgr();
    MILO_ASSERT(pTrainingMgr, 0xDC);
    LocalBandUser *user = pTrainingMgr->GetUser();
    MILO_ASSERT(user, 0xDF);
    return user;
}

DataNode FreestylePanel::OnMsg(const JoypadConnectionMsg &msg) {
    BandUser *u = GetFreestyleUser();
    if (u && msg.GetUser()) {
        if (msg.GetUser()->GetLocalUser() == u->GetLocalUser()) {
            if ((bool)msg->Int(3) == true) {
                CreateController();
            }
        }
    }
    return DataNode(kDataUnhandled, 0);
}

void FreestylePanel::EnableMetronome(bool enable) { mMetronome->Enable(enable); }
void FreestylePanel::SetMetronomeVolume(int i1, int i2) { mMetronome->SetVolume(i1, i2); }
void FreestylePanel::SetFreestylePaused(bool paused) { mFreestylePaused = paused; }

BEGIN_HANDLERS(FreestylePanel)
    HANDLE_ACTION(set_bpm, SetBpm(_msg->Int(2)))
    HANDLE_ACTION(create_controller, CreateController())
    HANDLE_ACTION(enable_metronome, EnableMetronome(_msg->Int(2)))
    HANDLE_EXPR(metronome_enabled, mMetronome->Enabled())
    HANDLE_ACTION(set_metronome_volume, SetMetronomeVolume(_msg->Int(2), _msg->Int(3)))
    HANDLE_EXPR(get_metronome_volume, mMetronome->GetVolume(_msg->Int(2)))
    HANDLE_ACTION(set_paused, SetFreestylePaused(_msg->Int(2)))
    HANDLE_MESSAGE(JoypadConnectionMsg)
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_MEMBER_PTR(DataDir())
    HANDLE_CHECK(0x121)
END_HANDLERS