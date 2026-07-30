#include "game/Player.h"
#include "Player.h"
#include <algorithm>
#include "bandobj/BandDirector.h"
#include "bandobj/OverdriveMeter.h"
#include "bandtrack/Track.h"
#include "Band.h"
#include "PlayerBehavior.h"
#include "bandobj/BandTrack.h"
#include "bandtrack/TrackPanel.h"
#include "beatmatch/BeatMaster.h"
#include "utl/SongPos.h"
#include "beatmatch/TrackType.h"
#include "decomp.h"
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "game/Defines.h"
#include "game/Game.h"
#include "game/GamePanel.h"
#include "game/GameConfig.h"
#include "game/NetGameMsgs.h"
#include "game/Performer.h"
#include "game/Scoring.h"
#include "game/SongDB.h"
#include "meta_band/MetaPerformer.h"
#include "net/NetSession.h"
#include "obj/Data.h"
#include "obj/Msg.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "os/System.h"
#include "utl/HxGuid.h"
#include "utl/MakeString.h"
#include "utl/Messages4.h"
#include "utl/Symbols.h"
#include "utl/Messages.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"
#include "utl/TimeConversion.h"

inline PlayerParams::PlayerParams() {
    DataArray *crowdcfg = SystemConfig("scoring", "crowd");
    mCrowdSaveLevel = crowdcfg->FindFloat("save_level");
    mMsToReturnFromBrink = crowdcfg->FindFloat("time_to_return_from_brink") * 1000.0f;
    mCrowdLossPerMs = crowdcfg->FindFloat("crowd_loss_per_sec") / 1000.0f;

    DataArray *unisoncfg = SystemConfig("scoring", "unison_phrase");
    mPointBonus = unisoncfg->FindFloat("point_bonus");

    DataArray *energycfg = SystemConfig("scoring", "band_energy");
    mDeployBeats = 1.0f / energycfg->FindFloat("deploy_beats");
    mDeployBonus = energycfg->FindFloat("deploy_bonus");
    mSpotlightPhrase = energycfg->FindFloat("spotlight_phrase");
    mDeployThreshold = energycfg->FindFloat("deploy_threshold");
    mSaveEnergy = energycfg->FindFloat("save_energy");
}

inline void PlayerParams::SetVocals() {
    DataArray *crowdcfg = SystemConfig("scoring", "crowd");
    mMsToReturnFromBrink = crowdcfg->FindFloat("vocals_to_return_from_brink") * 1000.0f;
}

Player::Player(BandUser *user, Band *band, int tracknum, BeatMaster *bmaster)
    : Performer(user, band), mParams(new PlayerParams()), mBehavior(new PlayerBehavior()),
      mUser(user), mCommonPhraseCapturer(band->mCommonPhraseCapturer), mRemote(0), mTrackNum(tracknum), mTrackType(kTrackNone),
      mEnabledState(kPlayerEnabled), mTimesFailed(0), mIsInCoda(0), mBandEnergy(0),
      mDeployingBandEnergy(0), unk274(1), unk278(0), mPhraseBonus(1),
      mBeatMaster(bmaster), unk284(5000.0f), unk288(0), unk28c(0), unk290(0), unk294(0),
      unk298(0), mDisconnectedAtStart(0), unk2a9(0), unk2ac(0),
      unk2b0(TheGame->mProperties.mEnableOverdrive), mPermanentOverdrive(0),
      mHasFinishedCoda(0), mHasBlownCoda(0), unk2b4(0), unk2b8(0), unk2bc(0), unk2c0(-1),
      unk2c4(1) {
    if (user) {
        mRemote = !user->IsLocal();
        mPlayerName = user->UserName();
        mTrackType = user->GetTrackType();
        if (mTrackType == kTrackVocals) {
            mParams->SetVocals();
        }
    }
    unk29c = 0;
    unk2a0 = 0;
    unk2a4 = 0;
    DataArray *cfg = SystemConfig("track_graphics");
    unk284 = cfg->FindFloat("popup_help_intro_duration_ms");
}

void Player::DynamicAddBeatmatch() { DisableOverdrivePhrases(); }

void Player::PostDynamicAdd() {
    float secs = TheTaskMgr.Seconds(TaskMgr::kRealTime);
    mQuarantined = true;
    mPollMs = secs * 1000.0f;
    SetTrack(mTrackNum);
    PostLoad(true);
    HookupTrack();
    SetEnabledState((EnabledState)3, mUser, false);
    GetBandTrack()->DropIn();
}

#pragma push
#pragma dont_inline on
DECOMP_FORCEFUNC(Player, Player, GetUser())
#pragma pop

void Player::Leave() {
    int tick = MsToTick(PollMs());
    mCommonPhraseCapturer->Enabled(this, mTrackNum, tick, false);
    BandTrack *track = GetBandTrack();
    if (track)
        track->DropOut();
}

Player::~Player() {
    delete mBehavior;
    delete mParams;
}

bool Player::RebuildPhrases() {
    if (mQuarantined) {
        if (mUser != TheBandUserMgr->GetNullUser()) {
            TheSongDB->RebuildPhrases(mTrackNum);
            return true;
        }
    }
    return false;
}

void Player::Restart(bool b) {
    if (mEnabledState == kPlayerDisconnected) {
        mDisconnectedAtStart = true;
        SetEnabledState(kPlayerDisconnected, mUser, false);
        SetAutoplay(true);
        SetAutoOn(true);
    } else
        mEnabledState = kPlayerEnabled;
    SetQuarantined(mUser == TheBandUserMgr->GetNullUser());
    unk2c4 = true;
    mTimesFailed = 0;
    mIsInCoda = false;
    mBandEnergy = 0;
    mDeployingBandEnergy = false;
    unk28c = 0;
    unk294 = 0;
    unk2b4 = 0;
    unk298 = 0;
    unk274 = 1;
    unk278 = 0;
    unk29c = 0;
    unk2a0 = 0;
    unk2a4 = 0;
    unk2b8 = 0;
    unk2bc = 0;
    unk2c0 = -1;
    unk260.clear();
    Performer::Restart(b);
    if (IsLocal()) {
        mStats.SetFinalized(true);
    }
    if (mDisconnectedAtStart) {
        mCrowd->SetDisplayValue(0);
    }
    unk2ac = 0;
    mHasFinishedCoda = 0;
    mHasBlownCoda = 0;
    if (mUser) {
        Track *track = mUser->GetTrack();
        if (track) {
            track->RefreshPlayerHUD();
        }
    }
    SetCrowdMeterActive(!TheGame->ResumedNoScore());
}

void Player::Poll(float f, const SongPos &pos) {
    PollMultiplier();
    if (f < TheSongDB->GetSongDurationMs()) {
        PollEnabledState(f);
    }
    if (IsNet()) {
        PollTalking(unk2ac);
    }
    UpdateEnergy(pos);
    Performer::Poll(f, pos);
    unk2ac++;
    if (unk1e2 && mDeployingBandEnergy) {
        StopDeployingBandEnergy(false);
    }
#ifndef HX_NATIVE
    // Periodic net energy-broadcast leaf (same handler as SetEnergy's); skipped
    // headless (no net session / BandUser). X360 keeps the real broadcast.
    if (IsLocal() && f >= unk2a4 + 2000.0f && !unk1e2) {
        Handle(send_update_energy_msg, true);
        unk2a4 = f;
    }
#endif
    if (unk288 && IsLocal() && f >= unk284) {
        PopupHelp(intro, false);
        unk288 = false;
    }
    if (unk2c4) {
        BandTrack *track = GetBandTrack();
        if (track)
            track->SetQuarantined(mQuarantined);
        unk2c4 = false;
    }
}

void Player::PollMultiplier() {
    if (TheGame->mProperties.mEnableStreak) {
        int i1 = 0, i2 = 0, i3 = 0;
        GetMultiplier(true, i1, i2, i3);
        int oldMult = unk274;
        if (i1 != oldMult) {
            if (i1 == 1) {
                mStats.EndStreakMultiplier(GetSongMs(), oldMult);
            }
            unk274 = i1;
        }
        // Retail materializes the bool (li 1 / mr from the shared zero / clrlwi.)
        // rather than branching on the fcmpu directly -- i.e. the inlined
        // InRollback() accessor, not a raw `unkdc == -1.0f` compare.
        if (!TheGame->InRollback()) {
            unk2b8 += i1 * i3;
            unk2bc++;
        }
    }
}

void Player::PollEnabledState(float f) {
    switch (mEnabledState) {
    case kPlayerEnabled:
        break;
    case kPlayerDisabled:
        if (mTimesFailed < 3) {
            BandTrack *track = GetBandTrack();
            if (track)
                track->PlayerDisabled();
        }
    case kPlayerDisconnected:
        if (!mDisconnectedAtStart) {
            MetaPerformer::Current();
            if (GetCrowdMeterActive()) {
                mCrowd->SetDisplayValue(
                    -(mParams->mCrowdLossPerMs * (TheGame->NumActivePlayers() - 1))
                    * (f - unk25c)
                );
            }
        }
        break;
    case kPlayerBeingSaved:
    case kPlayerDroppingIn:
        if (f >= mEnableMs) {
            SetEnabledState(kPlayerEnabled, mUser, false);
        }
        break;
    default:
        break;
    }
}

void Player::PollTalking(int i) {
    if (i % 5 == 0 && unk290) {
        BandTrack *track = GetBandTrack();
        if (track)
            track->SetNetTalking(false);
        unk290 = false;
    }
}

void Player::AddPoints(float f, bool b1, bool b2) {
    if (IsLocal()) {
        Performer::AddPoints(f, b1, b2);
        BroadcastScore();
    }
}

void Player::StartIntro() {
    if (IsLocal()) {
        LocalBandUser *user = mUser->GetLocalBandUser();
        if (!user->HasShownIntroHelp(mTrackType)) {
            PopupHelp(intro, true);
            user->SetShownIntroHelp(mTrackType, true);
            unk288 = true;
        }
    }
}

#ifdef HX_NATIVE
// M8: headless-audio clock shim. The retail path is mBeatMaster->mAudio->GetTime(),
// where MasterAudio::GetTime() returns mSongStream->GetTime()+mTimeOffset, or just
// mTimeOffset when no stream is loaded. The native run-through has no synth/stream
// (audio hardware is out of scope), so it drives a global ms clock that stands in
// for exactly that no-stream case — GetTime()==advancing mTimeOffset, time without
// sound. A native scoring Player is built with mBeatMaster==null; read the clock.
// X360 preprocessed output is unchanged (this whole guard is HX_NATIVE-only).
// Weak default so native targets that compile Player.cpp without the M8
// driver (rb3-score2 etc.) still link; m8_support.cpp's strong definition
// overrides it in rb3-score4.
float gNativeSongMs __attribute__((weak)) = 0.0f;
float Player::GetSongMs() const {
    if (!mBeatMaster)
        return gNativeSongMs;
    return mBeatMaster->mAudio->GetTime();
}
#else
float Player::GetSongMs() const { return mBeatMaster->mAudio->GetTime(); }
#endif

float Player::GetSongPct() const { return GetSongMs() / TheSongDB->GetSongDurationMs(); }

void Player::BroadcastScore() {
    if (unk1fd) {
        float poll = PollMs();
        float sub = poll - unk29c;
        int isub = (int)mScore - unk2a0;
        if ((isub > 0x13U) || (isub != 0) && (sub >= 250.0f)) {
            HandleType(send_update_score_msg);
            unk29c = poll;
            unk2a0 = mScore;
        }
    }
}

void Player::EnterCoda() {
    if (!mIsInCoda) {
        if (!TheGame->CodaEnabled())
            return;
        if (mEnabledState == kPlayerDisabled || mEnabledState == kPlayerBeingSaved
            || mEnabledState == kPlayerDroppingIn) {
            mEnableMs = PollMs();
            mUser->GetTrack()->SetGemsEnabledByPlayer();
            SetEnabledState(kPlayerEnabled, mUser, false);
        }
        mIsInCoda = true;
        mStats.mHasCoda = true;
    }
}

void Player::AddBonusPoints(int i) {
    BandUser *u = mUser;
    TheGame->AddBonusPoints(u, i, unk28c++);
}

void Player::Rollback(float, float) {
    BandTrack *track = GetBandTrack();
    if (track) {
        track->ResetSmashers(false);
    }
}

bool Player::InRollback() const { return TheGame->InRollback(); }

int Player::GetAccumulatedScore() const { return mScore; }
float Player::GetTotalStars() const { return GetNumStarsFloat(); }

void Player::SetEnabledState(EnabledState estate, BandUser *user, bool b) {
    if (IsLocal()) {
        int tick = MsToTick(GetSongMs());
        LocalSetEnabledState(estate, tick, user, b);
        static Message msg("send_enabled_state", 0, 0, 0);
        msg[0] = estate << 0x10 | b;
        msg[1] = tick;
        msg[2] = user->GetSlot();
        HandleType(msg);
    } else if (estate == 2) {
        unk298 = 1;
    }
}

EnabledState Player::GetEnabledStateAt(float f) const {
    if (mEnabledState == kPlayerBeingSaved || mEnabledState == kPlayerDroppingIn) {
        if (f >= mEnableMs)
            return kPlayerEnabled;
    }
    if (mEnabledState == kPlayerDisconnected)
        return kPlayerDisconnected;
    int tick = (int)MsToTick(f);
    for (unsigned int i = 0; i < unk260.size(); i++) {
        const Extent &e = unk260[i];
        if (tick >= e.unk0 && (e.unk4 < 0 || tick < e.unk4))
            return kPlayerDisabled;
    }
    return mEnabledState;
}

DECOMP_FORCEACTIVE(
    Player, __FILE__, "causer", "send_already_saved", "", "player_saved", "%s_regen.cue"
)

#pragma push
#pragma pool_data off
void Player::LocalSetEnabledState(EnabledState estate, int i, BandUser *causer, bool b) {
    unk298 = 0;
    if (estate == kPlayerBeingSaved && mEnabledState != kPlayerDisabled) {
        MILO_ASSERT(causer, 0x260);
        if (causer->GetPlayer()->IsNet()) {
            static Message msg("send_already_saved", 0);
            msg[0] = causer;
            HandleType(msg);
        }
    }
    if (mEnabledState == kPlayerDisabled && estate == kPlayerEnabled) {
        mCrowd->SetDisplayValue(mParams->mCrowdSaveLevel);
    }
    if (unk2a9 && estate != kPlayerDisabled) {
        unk2a9 = false;
    }
    SetCrowdMeterActive(!b);
    float newpct = GetSongPct();
    if (b) {
        SetNoScorePercent(newpct);
    }
    mEnabledState = estate;
    switch (estate) {
    case kPlayerEnabled: {
        static Message enable_player_msg("enable_player");
        Export(enable_player_msg, true);
        if (unk260.size() != 0) {
            Extent &ext = unk260.back();
            if (ext.unk4 == -1) {
                ext.unk4 = i;
            }
        }
        break;
    }
    case kPlayerDisabled:
    case kPlayerDisconnected:
        SetEnergy(0);
        if (TheGamePanel->GetState() != UIPanel::kUnloaded) {
            if (unk2a9) {
                mTimesFailed = 0;
            } else {
                if (mBand) {
                    std::vector<Player *> &active = mBand->GetActivePlayers();
                    for (std::vector<Player *>::iterator it = active.begin();
                         it != active.end();
                         ++it) {
                        if ((*it)->mDeployingBandEnergy) {
                            (*it)->StopDeployingBandEnergy(true);
                        }
                    }
                }
                mTimesFailed++;
            }
            unk25c = PollMs();
            DisablePlayer(mTimesFailed);
            mStats.AddFailurePoint(newpct);
            {
                Extent ext(i, -1);
                unk260.push_back(ext);
            }
        }
        break;
    case kPlayerDroppingIn:
        DelayReturn(false);
        mUser->GetTrack()->SetGemsEnabledByPlayer();
        break;
    case kPlayerBeingSaved: {
        BandTrack *track = GetBandTrack();
        if (track)
            track->PlayerSaved();
        mStats.AddToTimesSaved(mBand->MainPerformer()->Crowd()->GetValue(), GetSongPct());
        mCrowd->SetDisplayValue(mParams->mCrowdSaveLevel);
        DelayReturn(true);
        if (track)
            track->SavePlayer();
        static Message player_saved("player_saved", "", 0);
        player_saved[0] = causer->GetTrackSym();
        player_saved[1] = b;
        TheBandDirector->HandleType(player_saved);
        GetTrackPanel()->PlaySequence(
            MakeString("%s_regen.cue", mUser->GetTrackSym()), 0.0f, 0.0f, 0.0f
        );
        TheGame->OnPlayerSaved(this);
        break;
    }
    default:
        break;
    }
}
#pragma pop

bool Player::Saveable() const {
    bool ret = false;
    bool result = false;
    if (mEnabledState == kPlayerDisabled && mTimesFailed < 3 && !unk298)
        ret = true;
    if (ret) {
        MetaPerformer::Current();
        result = true;
    }
    return result;
}

void Player::Save(BandUser *user, bool b) { SetEnabledState(kPlayerBeingSaved, user, b); }

// Retail keeps the slot-parity pan OUT OF LINE (fn_826A25B8, inside Player.cpp's
// .text span): under /O1 (size-optimizing) the inlined form — two lis/lfs float
// constant loads plus a branch — is larger than the call, so MSVC declines to
// inline it. The rb3-Wii dev oracle writes it inline inside DisablePlayer.
__declspec(noinline) static float DiedCuePan(BandUser *user) {
    if (user->GetSlot() % 2)
        return 1.0f;
    else
        return -1.0f;
}

#pragma push
#pragma pool_data off
void Player::DisablePlayer(int i) {
    if (!unk2a9) {
        const char *cue = MakeString("%s_died.cue", TrackTypeToSym(mTrackType).Str());
        float f = 0;
        if (TheBandUserMgr->IsMultiplayerGame()) {
            f = DiedCuePan(mUser);
        }
        GetTrackPanel()->PlaySequence(cue, 0.0f, f, 0.0f);
    }
    if (GetBandTrack()) {
        GetBandTrack()->DisablePlayer(i);
    }
    if (mEnabledState == kPlayerDisabled) {
        static Message player_failed("player_failed", "");
        player_failed[0] = TrackTypeToSym(mTrackType);
        TheBandDirector->HandleType(player_failed);
    }

    static Message disable_player("disable_player", 0);
    disable_player[0] = mTimesFailed;
    Export(disable_player, true);
}
#pragma pop

const UserGuid &Player::GetUserGuid() const {
    MILO_ASSERT(GetUser(), 0x330);
    return mUser->GetUserGuid();
}

int Player::GetSlot() const {
#ifdef HX_NATIVE
    // Native scoring Player carries no BandUser; the overdrive-phrase capturer
    // asks for its slot to test gem playability. Slot 0 (all gems playable) is
    // the single-player headless answer. X360 output unchanged.
    if (!mUser)
        return 0;
#endif
    MILO_ASSERT(GetUser(), 0x336);
    return mUser->GetSlot();
}

bool Player::IsNet() const { return mRemote; }

int Player::GetMultiplier(bool b, int &i1, int &i2, int &i3) const {
    if (GetMultiplierActive()) {
        int i1ret;
        if (b) {
            i1ret = GetIndividualMultiplier();
        } else
            i1ret = 1;
        i1 = i1ret;
        i2 = mBand->EnergyMultiplier();
        i3 = (mDeployingBandEnergy != 0) + 1;
        i2 /= i3;
        return i1 * i2 * i3;
    } else {
        i1 = 1;
        i2 = 1;
        i3 = 1;
        return 1;
    }
}

void Player::SetMultiplierActive(bool b) {
    Performer::SetMultiplierActive(b);
    if (!b) {
        SetEnergy(0);
        BandTrack *track = GetBandTrack();
        if (track)
            track->SetStreak(0, 1, 1, false);
    }
}

bool Player::CanDeployOverdrive() const {
    return mBandEnergy >= mParams->mDeployThreshold && !mDeployingBandEnergy && unk2b0;
}

bool Player::IsDeployingBandEnergy() const { return mDeployingBandEnergy; }

void Player::StopDeployingBandEnergy(bool b) {
    int i1 = 0;
    int i2 = 0;
    int i3 = 0;
    GetMultiplier(true, i1, i2, i3);
    mStats.StopDeployingOverdrive(GetSongMs(), i1 * i3);
    mDeployingBandEnergy = false;
    if (mBand) {
        mBand->UpdateBonusLevel(PollMs());
    }
    if (!b) {
#ifdef HX_NATIVE
        // Audio-cue leaf guarded: the real expiry bookkeeping (mStats.Stop-
        // DeployingOverdrive above) runs without a TrackPanel. X360 unchanged.
        if (GetTrackPanel())
#endif
        GetTrackPanel()->PlaySequence(
            MakeString("rp_depleted_%s.cue", TrackTypeToSym(mTrackType).Str()), 0, 0, 0
        );
    }

    if (GetBandTrack()) {
        GetBandTrack()->StopDeploy();
    }
    SetEnergyAutomatically(mBandEnergy);
    if (CanDeployOverdrive()) {
        mUser->GetTrack()->SetCanDeploy(true);
        if (mTrackType == kTrackDrum) {
            EnableDrumFills(true);
        }
    }
}

void Player::DisablePhraseBonus() { mPhraseBonus = false; }
void Player::EnablePhraseBonus() { mPhraseBonus = true; }

void Player::DisableOverdrivePhrases() {
    float ms = PollMs();
    int tick = MsToTick(ms);
    mCommonPhraseCapturer->Enabled(this, mTrackNum, tick, false);
    TheSongDB->ClearTrackPhrases(mTrackNum);
}

void Player::CompleteCommonPhrase(bool b1, bool b2) {
    float energy;
    if (mPhraseBonus)
        energy = mParams->mSpotlightPhrase;
    else
        energy = 0;
    AddEnergy(energy);
    if (b1) {
        if (b2) {
            mStats.mUnisonPhraseCompleted++;
        }
    } else
        mStats.mOverdrivePhrasesCompleted++;
}

int Player::GetIndividualMultiplier() const {
    if (GetMultiplierActive()) {
        int streak = mStats.GetCurrentStreak();
        int mult = TheScoring->GetStreakMult(streak, mBehavior->mStreakType);
        int maxMult = mBehavior->mMaxMultiplier;
        return std::min(maxMult, mult);
    } else
        return 1;
}

int Player::GetMaxIndividualMultipler() const { return mBehavior->mMaxMultiplier; }

int Player::GetNumStars() const {
    return TheScoring->GetSoloNumStars(mScore, mTrackType);
}

float Player::GetNumStarsFloat() const {
    return TheScoring->GetSoloNumStarsFloat(mScore, mTrackType);
}

int Player::GetScoreForStars(int i) const {
    return TheScoring->GetSoloScoreForStars(i, mTrackType);
}

void Player::SavePersistentData(PersistentPlayerData &data) const {
    data.unk0 = mBandEnergy;
    data.unk4 = GetCrowdRating();
    data.unk8 = mStats.m0x150;
    data.unkc = mStats.mLongestPersistentStreak;
}

void Player::LoadPersistentData(const PersistentPlayerData &data) {
    if (!mDisconnectedAtStart) {
        SetEnergyAutomatically(data.unk0);
        mCrowd->SetValue(data.unk4);
        mStats.SetPersistentStreak(data.unk8);
        BandUser *user = mUser;
        mStats.mLongestPersistentStreak = data.unkc;
        MILO_ASSERT(user, 0x402);
        Track *track = user->GetTrack();
        if (track)
            track->RefreshPlayerHUD();
    }
}

DECOMP_FORCEACTIVE(Player, "Non-local player trying to deploy locally\n", "send_deploy")

int Player::LocalDeployBandEnergy() {
    int playersSaved = mBand->DeployBandEnergy(mUser);
    mStats.mDeployCount++;
    mStats.AddToPlayersSaved(playersSaved, mBand->MainPerformer()->Crowd()->GetValue());
    PerformDeployBandEnergy(playersSaved, true);
    return playersSaved;
}

void Player::PerformDeployBandEnergy(int playersSaved, bool local) {
    if (!local)
        mBand->DeployBandEnergy(mUser);
    if (playersSaved == 0) {
        mDeployingBandEnergy = true;
        if (mBand) {
            mBand->UpdateBonusLevel(PollMs());
        }
        Deploy();
    } else
        SubtractEnergy(mParams->mSaveEnergy);
}

DECOMP_FORCEACTIVE(Player, "rp_deployed_%s.cue")

void Player::RemoteAlreadySaved(int i) {
    unk294--;
    if (unk294 == 0) {
        AddEnergy(mParams->mSaveEnergy);
    }
    unk2b4--;
}

void Player::PopupHelp(Symbol s, bool b) {
    BandUser *user = mUser;
    if (user && user->GetDifficulty() == kDifficultyEasy) {
        BandTrack *track = GetBandTrack();
        if (track) {
            track->SetControllerType(user->GetControllerSym());
            track->PopupHelp(s, b);
        }
    }
}

void Player::AddEnergy(float f) {
    TheGame->OnPlayerAddEnergy(this, f);
    SetEnergy(std::min(1.0f, mBandEnergy + f));
}

Symbol Player::GetStreakType() const { return mBehavior->mStreakType; }

void Player::SetEnergy(float f) {
    MetaPerformer::Current();
    if (IsLocal() && unk2b0) {
        float old = mBandEnergy;
        SetEnergyAutomatically(f);
        bool energyIncreased = mBandEnergy > old;
        float poll = PollMs();
        float pollDelta = poll - unk2a4;
        if (energyIncreased || pollDelta >= 100.0f || mBandEnergy == 0) {
#ifndef HX_NATIVE
            // Net energy-broadcast leaf: its handler reads the BandUser's track,
            // which no native headless player has. The real energy bookkeeping
            // (SetEnergyAutomatically above + unk2a4 below) still runs; only the
            // net export is skipped. X360 keeps the real broadcast.
            static Message send_update_energy(Symbol("send_update_energy"));
            Handle(send_update_energy, true);
#endif
            unk2a4 = poll;
        }
    }
}

void Player::SetEnergyFromNet(float f, bool b) {
    SetEnergyAutomatically(f);
    if (mDeployingBandEnergy && !b) {
        StopDeployingBandEnergy(false);
    }
}

bool Player::ShouldDrainEnergy() const { return true; }

void Player::CheckCrowdFailure() {
    if (mCrowd->IsBelowLoseLevel() && mEnabledState == kPlayerEnabled) {
        if (mCrowd->CantFailYet() || mUser->GetDifficulty() == kDifficultyEasy
            || mUser->IsNullUser() || MetaPerformer::Current()->IsNoFailActive()
            || mQuarantined || mBand->MainPerformer()->mGameOver) {
            mCrowd->SetDisplayValue(0);
            return;
        }
        SetEnabledState(kPlayerDisabled, mUser, false);
    }
}

void Player::IgnoreUntilRollback(float) {}

void Player::SubtractEnergy(float f) {
    SetEnergy(std::max(0.0f, mBandEnergy - f));
}

void Player::UpdateEnergy(const SongPos &pos) {
    bool notPracticing = TheGame->unkdc != -1.0f;
    if (!notPracticing) {
        if (unk2b0 && mDeployingBandEnergy) {
            TheSongDB->GetData()->GetBeatMap();
            float curBeat = TickToBeat((int)pos.GetTotalTick());
            float prevBeat = TickToBeat((int)mSongPos.GetTotalTick());
            if (ShouldDrainEnergy()) {
                float delta = curBeat - prevBeat;
                SubtractEnergy(delta * mParams->mDeployBeats);
            }
            if (mBandEnergy == 0.0f) {
                StopDeployingBandEnergy(false);
            }
        }
    }
}

void Player::SetEnergyAutomatically(float f) {
    bool oldCanDeploy = CanDeployOverdrive();
    mBandEnergy = f;
    BandTrack *track = GetBandTrack();
    if (!track)
        return;
    OverdriveMeter *meter = track->mStarPowerMeter;
    if (!meter)
        return;
    if (mTrackType == kTrackDrum) {
        if (!oldCanDeploy) {
            if (CanDeployOverdrive()) {
                EnableDrumFills(true);
            }
        } else {
            if (!CanDeployOverdrive()) {
                EnableDrumFills(false);
            }
        }
    }
    OverdriveMeter::State state;
    if (mDeployingBandEnergy) {
        state = OverdriveMeter::kDeploying;
    } else {
        if (!CanDeployOverdrive())
            state = OverdriveMeter::kFilling;
        else
            state = OverdriveMeter::kReady;
    }
    float pulseDelay = 0.0f;
    if (state == OverdriveMeter::kReady) {
        pulseDelay = GetTrackPanelDir()->GetPulseAnimStartDelay(true);
    }
    meter->SetEnergy(f, state, TrackTypeToSym(mTrackType), pulseDelay, false);

    bool newCanDeploy = CanDeployOverdrive();
    if (newCanDeploy != oldCanDeploy) {
        if (mUser) {
            Track *userTrack = mUser->GetTrack();
            if (userTrack) {
                userTrack->SetCanDeploy(CanDeployOverdrive());
            }
        }
    }
}

void Player::Deploy() {
#ifdef HX_NATIVE
    // Audio-cue leaf guarded: run the real deploy bookkeeping (mStats.Deploy-
    // Overdrive below) without the TrackPanel render surface. X360 unchanged.
    if (GetTrackPanel())
#endif
    GetTrackPanel()->PlaySequence(
        MakeString("rp_deployed_%s.cue", TrackTypeToSym(mTrackType).Str()), 0, 0, 0
    );
    BandTrack *track = GetBandTrack();
    if (track) {
        track->Deploy();
    }
    PopupHelp(deploy, false);
    if (mTrackType == kTrackDrum && !mIsInCoda) {
        EnableDrumFills(false);
    }
    unk278++;
    int i1 = 0, i2 = 0, i3 = 0;
    GetMultiplier(true, i1, i2, i3);
    mStats.DeployOverdrive(GetSongMs(), i1 * i3);
    if (TheGame->mProperties.mInTrainer) {
        Handle(deploy_msg.mData, false);
    }
}

bool Player::DeployBandEnergyIfPossible(bool b) {
    if (!b) {
        bool notPaused = TheGame->unkdc != -1.0f;
        if (notPaused) {
            return false;
        }
    }
    if (!IsLocal()) {
        MILO_NOTIFY("Non-local player trying to deploy locally\n");
        return false;
    }
    if (!unk2b0)
        return false;
    bool finished = (mBand && mBand->GetBand() && mBand->MainPerformer()->mGameOver)
        || unk1e1;
    if (mEnabledState != kPlayerEnabled || finished) {
        return false;
    }
    if (!mDeployingBandEnergy || mBand->AnyoneSaveable()) {
        if (mBandEnergy >= mParams->mDeployThreshold) {
            unk294 = LocalDeployBandEnergy();
            unk2b4 += unk294;
            static Message msg("send_deploy", DataNode(0));
            msg[0] = unk294;
            HandleType(msg);
            return true;
        }
        mStats.mFailedDeploy = true;
    }
    return false;
}

void Player::Hit() {
#ifndef HX_NATIVE
    char slotStr[] = "p0_hit";
    slotStr[1] = (char)(mUser->GetSlot() + 0x30);
    static Message hit("");
    hit.SetType(slotStr);
    TheGamePanel->HandleType(hit.mData);
#endif
    int mult = GetIndividualMultiplier();
    if (mult > unk274 && unk274 == 1) {
        mStats.BeginStreakMultiplier(GetSongMs(), mult);
    }
}

void Player::DelayReturn(bool b) {
    float pollResult = (float)PollMs();
    mEnableMs = std::max(0.0f, pollResult);
    if (b) {
        mEnableMs += mParams->mMsToReturnFromBrink;
    }
    bool hasUnkdc = TheGame->unkdc != -1.0f;
    if (hasUnkdc) {
        float unkdc = TheGame->unkdc;
        mEnableMs = std::max(unkdc, mEnableMs);
        IgnoreUntilRollback(mEnableMs);
    }
    int tick = (int)MsToTick(mEnableMs);
    int phraseId = TheSongDB->GetCommonPhraseID(mTrackNum, tick);
    if (phraseId > -1 && mTrackType != kTrackVocals) {
        const GameGemList *gemList = TheSongDB->GetGemList(mTrackNum);
        int gemIdx = gemList->ClosestMarkerIdxAtOrAfterTick(tick) - 1;
        int gemTick;
        if (gemIdx > -1) {
            gemTick = gemList->GetGem(gemIdx).mTick;
        } else {
            gemTick = -1;
        }
        Extent extent;
        TheSongDB->GetCommonPhraseExtent(mTrackNum, phraseId, extent);
        if (gemTick >= extent.unk0) {
            mUser->GetTrack()->OnMissPhrase(phraseId);
        }
    }
}

void Player::FinalizeStats() {
    mStats.mNotesHitFraction = GetNotesHitFraction(nullptr);
    mStats.m0x14c = mStats.GetCurrentStreak();
    mStats.m0x150 = mStats.mPersistentStreak;
    mStats.mEndGameScore = GetScore();
    mStats.mEndGameCrowdLevel = mCrowd->GetValue();
    mStats.mEndGameOverdrive = mBandEnergy;
    mStats.mOverdrivePhraseCount = TheSongDB->GetNumOverdrivePhrases(mTrackNum);
    mStats.mUnisonPhraseCount = TheSongDB->GetNumUnisonPhrases(mTrackNum);
    int i1 = 0, i2 = 0, i3 = 0;
    GetMultiplier(true, i1, i2, i3);
    float songMs = GetSongMs();
    mStats.EndStreakMultiplier(songMs, i1);
    songMs = GetSongMs();
    mStats.StopDeployingOverdrive(songMs, i1 * i3);
    if (unk2bc > 0) {
        mStats.mAverageMultiplier = (float)unk2b8 / (float)unk2bc;
    }
    mStats.EndHitStreak();
    mStats.EndMissStreak();
    bool noMiss = (mStats.mMissCount == 0);
    bool perfectFraction = (mStats.mNotesHitFraction == 1.0f);
    bool isExpert = (mUser->GetDifficulty() == kDifficultyExpert);
    mStats.mFullCombo = noMiss && perfectFraction && isExpert;
    mStats.SetFinalized(true);
}

void Player::HandleNewSection(const PracticeSection &section, int sectionIdx, int totalSections) {
    if (TheGame->unkdc != -1.0f || mQuarantined)
        return;
    unk2c0 = sectionIdx;
    if (totalSections != (int)mStats.mSections.size()) {
        Stats::SectionInfo info;
        if ((unsigned long)totalSections < mStats.mSections.size()) {
            mStats.mSections.erase(
                mStats.mSections.begin() + totalSections,
                mStats.mSections.end()
            );
        } else {
            mStats.mSections.insert(
                mStats.mSections.end(),
                (unsigned long)totalSections - mStats.mSections.size(),
                info
            );
        }
    }
    mStats.SetSectionInfo(unk2c0, section.unk0, -1.0f, 0.0f);
}

void Player::UpdateSectionStats(float hitFraction, float percentComplete) {
    if (TheGame->unkdc != -1.0f)
        return;
    if (!mQuarantined) {
        if (TheSongDB->mPracticeSections.size() == 0)
            return;
        if (unk2c0 < 0)
            return;
        if ((unsigned int)unk2c0 >= TheSongDB->mPracticeSections.size())
            return;
        int sectionIdx = unk2c0;
        Symbol sectionSym = TheSongDB->mPracticeSections[sectionIdx].unk0;
        mStats.SetSectionInfo(sectionIdx, sectionSym, hitFraction, percentComplete);
    }
}

#ifdef HX_NATIVE
// M7: build the REAL PlayerParams (its ctor is inline/TU-local to Player.cpp, so
// it can only be instantiated here) from the parsed scoring.dta band_energy /
// crowd / unison_phrase config. Supplies mSpotlightPhrase / mDeployThreshold /
// mDeployBeats to the real energy methods. X360 never sees this.
void Player::NativeInitParams() { mParams = new PlayerParams(); }
#endif

BandTrack *Player::GetBandTrack() const {
#ifdef HX_NATIVE
    // Native headless scoring graph builds a Player with a null mUser (the
    // retail BandUser drags the meta_band/tour/net/render domain). SetEnergy-
    // Automatically reaches here purely to feed the OverdriveMeter display; a
    // null user means "no BandTrack yet", the method's real early-return branch.
    // X360 preprocessed output is unchanged (this whole guard is HX_NATIVE-only).
    if (!mUser)
        return nullptr;
#endif
    Track *track = mUser->GetTrack();
    if (track != nullptr) {
        return track->GetBandTrack();
    } else
        return nullptr;
}

void Player::UnisonMiss(int i) const {
#ifdef HX_NATIVE
    // Render leaf: the native scoring player has no BandUser/Track. X360 unchanged.
    if (!mUser)
        return;
#endif
    Track *track = mUser->GetTrack();
    if (track) {
        track->OnMissPhrase(i);
    }
}

void Player::UnisonHit() {
    BandTrack *track = GetBandTrack();
    if (track) {
        track->SpotlightPhraseSuccess();
    }
#ifdef HX_NATIVE
    // Audio-cue leaf: no TrackPanel render surface headless. X360 unchanged.
    if (!GetTrackPanel())
        return;
#endif
    GetTrackPanel()->PlaySequence(
        MakeString("rp_captured_%s.cue", TrackTypeToSym(mTrackType).Str()), 0, 0, 0
    );
}

void Player::SetFinishedCoda() {
    MILO_ASSERT(!mHasBlownCoda, 0x5A8);
    mHasFinishedCoda = true;
    if (TheGame->InTrainer()) {
        Export(finished_coda_msg, true);
    }
}

void Player::ChangeDifficulty(Difficulty diff) {
    mScore = 0;
    mStats = Stats();
    SetEnergy(0);
    mCrowd->ChangeDifficulty(mUser, diff);
    mCrowd->SetLoseLevel(mBand->MainPerformer()->Crowd()->GetLoseLevel());
    SetQuarantined(true);
    TheGameConfig->ChangeDifficulty(GetUser(), diff);
    mBand->UpdateBonusLevel(TheTaskMgr.Seconds(TaskMgr::kRealTime) * 1000.0f);
}

void Player::SetQuarantined(bool b) {
    mQuarantined = b;
    if (b && mTrackNum != -1) {
        DisableOverdrivePhrases();
        TheGame->OnPlayerQuarantined(this);
    }
    BandTrack *track = GetBandTrack();
    if (track) {
        track->SetQuarantined(b);
        unk2c4 = false;
    }
}

void Player::DeterminePerformanceAwards() {
    DataArray *cfg = SystemConfig(performance_awards);
    DataNode &playerNode = DataVariable("player");
    DataNode n30(playerNode);
    DataNode thisNode(this);
    playerNode = thisNode;
    for (int i = 1; i < cfg->Size(); i++) {
        DataArray *arr = cfg->Array(i);
        if (arr->Int(1)) {
            mStats.mPerformanceAwards.push_back(arr->Sym(0));
        }
    }
    playerNode = n30;
}

DataNode Player::OnSendNetGameplayMsg(DataArray *arr) {
    int opcode = arr->Int(2);
    int packet = arr->Int(3) != 0;

    int arg1 = 0;
    int arg2 = 0;
    int arg3 = 0;

    if (arr->Size() > 4)
        arg1 = arr->Int(4);
    if (arr->Size() > 5)
        arg2 = arr->Int(5);
    if (arr->Size() > 6)
        arg3 = arr->Int(6);

    PlayerGameplayMsg msg(mUser, opcode, arg1, arg2, arg3);
    TheNetSession->SendMsgToAll(msg, (PacketType)packet);
    return 1;
}

DataNode Player::OnSendNetGameplayMsgToPlayer(DataArray *arr) {
    User *destUser = arr->Obj<User>(2);
    int opcode = arr->Int(3);
    int packet = arr->Int(4) != 0;

    int arg1 = 0;
    int arg2 = 0;
    int arg3 = 0;

    if (arr->Size() > 5)
        arg1 = arr->Int(5);
    if (arr->Size() > 6)
        arg2 = arr->Int(6);
    if (arr->Size() > 7)
        arg3 = arr->Int(7);

    PlayerGameplayMsg msg(mUser, opcode, arg1, arg2, arg3);
    TheNetSession->SendMsg(destUser, msg, (PacketType)packet);
    return 1;
}

DataNode Player::OnGetOverdriveMeter(DataArray *arr) {
    BandTrack *track = GetBandTrack();
    return track ? (OverdriveMeter *)track->mStarPowerMeter : NULL_OBJ;
}

#pragma push
#pragma dont_inline on
BEGIN_HANDLERS(Player)
    HANDLE_EXPR(track, GetUser()->GetTrack())
    HANDLE_EXPR(get_user, GetUser())
    HANDLE_EXPR(player_name, mPlayerName)
    HANDLE_EXPR(instrument, GetUser()->GetTrackSym())
    HANDLE_EXPR(difficulty, GetUser()->GetDifficulty())
    HANDLE_EXPR(is_net, IsNet())
    HANDLE_EXPR(disconnected_at_start, mDisconnectedAtStart)
    HANDLE_ACTION(enter_coda, EnterCoda())
    HANDLE_EXPR(band_energy, mBandEnergy)
    HANDLE_ACTION(set_band_energy, SetEnergy(_msg->Float(2)))
    HANDLE_ACTION(fill_band_energy, SetEnergy(1.0f))
    HANDLE_ACTION(empty_band_energy, SetEnergy(0))
    HANDLE_EXPR(can_deploy, CanDeployOverdrive())
    HANDLE_EXPR(is_deploying, IsDeployingBandEnergy())
    HANDLE_ACTION(deploy_if_possible, DeployBandEnergyIfPossible(false))
    HANDLE_ACTION_IF(force_deploy, !mDeployingBandEnergy, LocalDeployBandEnergy())
    HANDLE_EXPR(in_freestyle_section, InFreestyleSection())
    HANDLE(star_power_meter, OnGetOverdriveMeter)
    HANDLE_ACTION(fail, SetEnabledState(kPlayerDisabled, GetUser(), false))
    HANDLE_EXPR(enabled_state, mEnabledState)
    HANDLE_EXPR(enable_time, mEnableMs)
    HANDLE_ACTION(enable_swings, EnableSwings(_msg->Int(2)))
    HANDLE_ACTION(update_lefty_flip, UpdateLeftyFlip())
    HANDLE_ACTION(update_vocal_style, UpdateVocalStyle())
    HANDLE_ACTION(reset_controller, ResetController(_msg->Int(2)))
    HANDLE_ACTION(enable_phrase_bonus, EnablePhraseBonus())
    HANDLE_ACTION(disable_phrase_bonus, DisablePhraseBonus())
    HANDLE_ACTION(popup_help, PopupHelp(_msg->Sym(2), _msg->Int(3)))
    HANDLE(send_net_gameplay_msg, OnSendNetGameplayMsg)
    HANDLE(send_net_gameplay_msg_to_player, OnSendNetGameplayMsgToPlayer)
    HANDLE_ACTION(remote_deploy, PerformDeployBandEnergy(_msg->Int(2), false))
    HANDLE_ACTION(
        remote_hit_last_unison_gem,
        mCommonPhraseCapturer->LocalHitLastGem(this, _msg->Int(2), _msg->Int(3))
    )
    HANDLE_ACTION(remote_update_energy, SetEnergyFromNet(_msg->Float(2), _msg->Int(3)))
    HANDLE_ACTION(
        remote_fail_unison_phrase,
        mCommonPhraseCapturer->LocalFail(this, _msg->Int(2), _msg->Int(3))
    )
    HANDLE_ACTION(
        remote_enabled_state,
        LocalSetEnabledState(
            (EnabledState)(_msg->Int(2) >> 0x10),
            _msg->Int(3),
            TheBandUserMgr->GetUserFromSlot(_msg->Int(4)),
            (u16)(_msg->Int(2))
        )
    )
    HANDLE_ACTION(remote_already_saved, RemoteAlreadySaved(_msg->Int(2)))
    HANDLE_ACTION(
        remote_tracker_focus,
        TheGame->OnRemoteTrackerFocus(this, _msg->Int(2), _msg->Int(3), _msg->Int(4))
    )
    HANDLE_ACTION(
        remote_tracker_player_progress,
        TheGame->OnRemoteTrackerPlayerProgress(this, _msg->Float(2))
    )
    HANDLE_ACTION(
        remote_tracker_section_complete,
        TheGame->OnRemoteTrackerSectionComplete(
            this, _msg->Int(2), _msg->Int(3), _msg->Int(4)
        )
    )
    HANDLE_ACTION(
        remote_tracker_player_display,
        TheGame->OnRemoteTrackerPlayerDisplay(
            this, _msg->Int(2), _msg->Int(3), _msg->Int(4)
        )
    )
    HANDLE_ACTION(remote_tracker_deploy, TheGame->OnRemoteTrackerDeploy(this))
    HANDLE_ACTION(
        remote_tracker_end_deploy_streak,
        TheGame->OnRemoteTrackerEndDeployStreak(this, _msg->Int(2))
    )
    HANDLE_ACTION(
        remote_tracker_end_streak,
        TheGame->OnRemoteTrackerEndStreak(this, _msg->Int(2), _msg->Int(3))
    )
    HANDLE_EXPR(get_times_failed, mTimesFailed)
    HANDLE_EXPR(get_deploy_failed, mStats.GetFailedDeploy())
    HANDLE_EXPR(get_saved_count, mStats.GetPlayersSaved())
    HANDLE_ACTION(set_permanent_overdrive, mPermanentOverdrive = _msg->Int(2))
    HANDLE_ACTION(set_energy_automatically, SetEnergyAutomatically(_msg->Float(2)))
    HANDLE_ACTION(clear_score_histories, ClearScoreHistories())
    HANDLE_ACTION(change_difficulty, ChangeDifficulty((Difficulty)_msg->Int(2)))
    HANDLE_SUPERCLASS(Performer)
    HANDLE_SUPERCLASS(MsgSource)
    HANDLE_CHECK(0x6DE)
END_HANDLERS
#pragma pop