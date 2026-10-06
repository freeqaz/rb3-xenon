#include "game/VocalPlayer.h"
#include "VocalOverlay.h"
#include "bandobj/BandTrack.h"
#include "bandobj/VocalTrackDir.h"
#include "bandtrack/VocalTrack.h"
#include "beatmatch/BeatMaster.h"
#include "beatmatch/MasterAudio.h"
#include "beatmatch/VocalNote.h"
#include "decomp.h"
#include "game/BandUser.h"
#include "game/Game.h"
#include "game/GameConfig.h"
#include "game/GameMic.h"
#include "game/GameMicManager.h"
#include "game/GamePanel.h"
#include "game/Performer.h"
#include "game/PracticePanel.h"
#include "game/Scoring.h"
#include "game/SongDB.h"
#include "game/VocalPart.h"
#include "meta_band/BandSongMetadata.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/BandUI.h"
#include "meta_band/MetaPerformer.h"
#include "meta_band/OvershellPanel.h"
#include "meta_band/OvershellSlot.h"
#include "meta_band/ProfileMgr.h"
#include "midi/MidiParser.h"
#include "net/NetSession.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "os/Joypad.h"
#include "os/JoypadMsgs.h"
#include "os/System.h"
#include "rndobj/Overlay.h"
#include "synth/MicManagerInterface.h"
#include "ui/UI.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"
#include "utl/TextFileStream.h"
#include "utl/TimeConversion.h"
#include "world/Dir.h"
#include "utl/Messages.h"
#include "utl/Messages4.h"
#include <algorithm>
#include <functional>
#include <utility>

const float VocalPlayer::kInvalidPitch = -1000.0f;

// native: each retail TU defines its own sNullMicClientID (all equal to
// MicClientID's default (-1,-1)). Natively several of these TUs link into one
// target, so each copy gets internal linkage instead of a duplicate symbol.
#ifdef HX_NATIVE
static
#endif
MicClientID sNullMicClientID;

VocalPlayer::VocalPlayer(
    BandUser *user, BeatMaster *bmaster, Band *band, int tracknum, Performer *perf, int i7
)
    : Player(user, band, tracknum, bmaster), mBandPerformer(perf), mSpoofed(0), mTrack(0),
      mAutoPlay(0), mVocalPartBias(1.25f), unk2e8(0), unk2ec(0), mNextPacketSendTime(0),
      unk300(0), unk304(0), mTrackWrappingMargin(0), mLastDeploymentSinger(-1),
      mPhraseValue(0), mPartScoreMultipliers(0), mRatingThresholds(0),
      mNonpitchStickiness(0.6f), mCouldChat(0), mCodaEndMs(0), unk344(0),
      mTuningOffset(0), unk34c(-1.0f), mInitialMicCount(0), unk36c(0), unk370(0),
      mPhraseActivePartCount(0), mPhrasePercentageTotal(0), mPhrasePercentageCount(0),
      mOverlay(0), mVocalOverlay(0), mScoringEnabled(1), mTambourineManager(*this),
      mSectionStartPhrasePercentageTotal(0), mSectionStartPhrasePercentageCount(0),
      mSectionStartScore(0)
#ifdef HX_NATIVE
      , mFrameSpewData(0), mFrameSpewStream(0)
#endif
{
    Symbol curSong = MetaPerformer::Current()->Song();
    int songID = TheSongMgr.GetSongIDFromShortName(curSong, true);
    BandSongMetadata *data = (BandSongMetadata *)TheSongMgr.Data(songID);
    mTuningOffset = data->TuningOffset() / 100.0f;
    for (int i = 0; i < i7; i++) {
        mSingers.push_back(new Singer(this, i));
    }
    SetTypeDef(SystemConfig("player", "handlers"));
    SetDifficultyVariables(mUser->GetDifficulty());
    DataArray *cfg = SystemConfig("scoring", "vocals");
    mTrackWrappingMargin = cfg->FindFloat("track_wrapping_margin");
    mFreestyleDeploymentTimes = cfg->FindArray("freestyle_deployment_time")->Array(1);
    mFreestyleMinDurations = cfg->FindArray("freestyle_min_duration")->Array(1);
    mMaxDetune = cfg->FindFloat("max_detune");
    mPacketPeriodMs = cfg->FindFloat("packet_period");
    mPartScoreMultipliers = cfg->FindArray("part_score_multiplier");
    mRatingThresholds = cfg->FindArray("rating_thresholds");
    mNonpitchStickiness = cfg->FindFloat("nonpitch_stickiness");
    JoypadSubscribe(this);
    TheGameMicManager->AddSink(this, GameMicsChangedMsg::Type());
    RememberCurrentMics();
    mOverlay = RndOverlay::Find("vocalplayer", true);
    mOverlay->SetCallback(this);
}

#ifdef HX_NATIVE
// Native-only: the difficulty the native Singer ctor reads when mPlayer->GetUser()
// is null (headless has no BandUser). Set by the native VocalPlayer ctor below
// before the Singers are constructed. X360-inert.
int gNativeVocalDifficulty = 3; // Expert

VocalPlayer::VocalPlayer(
    BandUser *user, BeatMaster *bmaster, Band *band, int tracknum, Performer *perf,
    int nsingers, int nativeDiff, bool /*native_tag*/
)
    : Player(user, band, tracknum, bmaster, true), mBandPerformer(perf), mSpoofed(0),
      mTrack(0), mAutoPlay(0), mVocalPartBias(1.25f), unk2e8(0), unk2ec(0),
      mNextPacketSendTime(0), unk300(0), unk304(0), mTrackWrappingMargin(0),
      mLastDeploymentSinger(-1), mPhraseValue(0), mPartScoreMultipliers(0),
      mRatingThresholds(0), mNonpitchStickiness(0.6f), mCouldChat(0), mCodaEndMs(0),
      unk344(0), mTuningOffset(0), unk34c(-1.0f), mInitialMicCount(0), unk36c(0),
      unk370(0), mPhraseActivePartCount(0), mPhrasePercentageTotal(0),
      mPhrasePercentageCount(0), mOverlay(0), mVocalOverlay(0), mScoringEnabled(1),
      mTambourineManager(*this), mSectionStartPhrasePercentageTotal(0),
      mSectionStartPhrasePercentageCount(0), mSectionStartScore(0),
      mFrameSpewData(0), mFrameSpewStream(0) {
    gNativeVocalDifficulty = nativeDiff;
    NativeInitParams(); // instantiate PlayerParams from scoring.dta (M7 helper) so
                        // the real overdrive/energy gates (CanDeployOverdrive etc.)
                        // have their config; retail builds these in the Player ctor.
    // mTuningOffset: retail reads BandSongMetadata->TuningOffset(); headless has no
    // song-metadata DB, so 0 (no detune). The per-frame TheSongDB->GetPitchOffset-
    // ForTick path in Poll still updates it if the chart carries tuning data.
    for (int i = 0; i < nsingers; i++) {
        mSingers.push_back(new Singer(this, i));
    }
    SetTypeDef(SystemConfig("player", "handlers"));
    SetDifficultyVariables(nativeDiff);
    DataArray *cfg = SystemConfig("scoring", "vocals");
    mTrackWrappingMargin = cfg->FindFloat("track_wrapping_margin");
    mFreestyleDeploymentTimes = cfg->FindArray("freestyle_deployment_time")->Array(1);
    mFreestyleMinDurations = cfg->FindArray("freestyle_min_duration")->Array(1);
    mMaxDetune = cfg->FindFloat("max_detune");
    mPacketPeriodMs = cfg->FindFloat("packet_period");
    mPartScoreMultipliers = cfg->FindArray("part_score_multiplier");
    mRatingThresholds = cfg->FindArray("rating_thresholds");
    mNonpitchStickiness = cfg->FindFloat("nonpitch_stickiness");
    // Skipped vs retail (headless / driver-owned): JoypadSubscribe,
    // TheGameMicManager->AddSink, RememberCurrentMics (mic sink fan-out), and
    // RndOverlay::Find("vocalplayer") + SetCallback (no render overlay).
}
#endif

VocalPlayer::~VocalPlayer() {
    if (mOverlay)
        mOverlay->SetCallback(nullptr);
    TheGameMicManager->RemoveSink(this);
    FOREACH (it, mVocalParts) {
        RELEASE(*it);
    }
    FOREACH (it, mSingers) {
        RELEASE(*it);
    }
    if (mUser->IsLocal()) {
        JoypadKeepAlive(mUser->GetLocalUser()->GetPadNum(), false);
    }
    JoypadUnsubscribe(this);
}

void VocalPlayer::PostDynamicAdd() {
    Player::PostDynamicAdd();
    mBeatMaster->GetAudio()->ResetTrack(mTrackNum, true);
    mTambourineManager.PostDynamicAdd();
    mTambourineManager.SetBank(TheGamePanel->mVocalPercussionBank);
    TheGameMicManager->SetPlayback(true);
    if (mUser->IsLocal()) {
        JoypadKeepAlive(mUser->GetLocalUser()->GetPadNum(), true);
    }
}

void VocalPlayer::Leave() {
    if (mTrackNum != -1) {
        MasterAudio *audio = mBeatMaster->GetAudio();
        audio->ResetTrack(mTrackNum, false);
        audio->SetVocalState(true);
        audio->SetVocalDuckFader(0);
    }
    TheGameMicManager->SetPlayback(false);
    if (mUser->IsLocal()) {
        JoypadKeepAlive(mUser->GetLocalUser()->GetPadNum(), false);
    }
}

void VocalPlayer::SetDifficultyVariables(int diff) {
    DataArray *voxCfg = SystemConfig("scoring", "vocals");
    mPitchMaximumDistance = voxCfg->FindArray("pitch_margin")->Float(diff + 1);
    mSynapseProximitySolo = voxCfg->FindArray("synapse_proximity_solo")->Float(diff + 1);
    mSynapseFocusSolo = voxCfg->FindArray("synapse_focus_solo")->Float(diff + 1);
    mSynapseProximityHarmony =
        voxCfg->FindArray("synapse_proximity_harm")->Float(diff + 1);
    mSynapseFocusHarmony = voxCfg->FindArray("synapse_focus_harm")->Float(diff + 1);
    mPhraseValue = voxCfg->FindArray("phrase_value")->Int(diff + 1);
    FOREACH (it, mVocalParts) {
        (*it)->SetDifficultyVariables(diff);
    }
}

void VocalPlayer::ConfigureBehavior() {
    mBehavior->SetStreakType(mUser->GetTrackSym());
    mBehavior->SetMaxMultiplier(4);
}

// Retail keeps this test out of line (fn_826E3AA8, one caller: SetTrack); the
// name is ours (lane W16-HX4).
#pragma auto_inline(off)
bool VocalPlayer::IsNetOrSpoofed() const { return IsNet() || mSpoofed; }
#pragma auto_inline(on)

void VocalPlayer::SetTrack(int trk) {
    if (mTrackNum != trk) {
        MILO_ASSERT(mTrackNum == -1, 0x128);
        mBeatMaster->GetAudio()->SetTrack(GetUserGuid(), trk);
        mTrackNum = trk;
        if (IsNetOrSpoofed()) {
            mBeatMaster->GetAudio()->SetNonmutable(trk);
        }
    }
}

void VocalPlayer::PostLoad(bool b1) {
#ifndef HX_NATIVE
    // Headless: no ObjectDir::Main() "play_tambourine" MidiParser object; the
    // tambourine MIDI sink isn't wired in the synthetic-mic run.
    MidiParser *midiParser = ObjectDir::Main()->Find<MidiParser>("play_tambourine", true);
    midiParser->AddSink(this);
#endif
    int num = TheSongDB->GetVocalNoteListCount();
    mStats.SetVocalSingerAndPartCounts(mSingers.size(), num);
    for (int i = 0; i < num; i++) {
        mVocalParts.push_back(new VocalPart(this, i));
        mStats.mVocalPartPercentages[i] = 0;
    }
    FOREACH (it, mVocalParts) {
        (*it)->PostLoad();
    }
    FOREACH (it, mSingers) {
        (*it)->PostLoad();
    }
    BuildPhrases(b1);
#ifdef HX_NATIVE
    DataNode spew = DataVariable("vocal_frame_spew");
    if (spew.Int())
        ToggleFrameSpew();
#endif
}

void VocalPlayer::Start() {
    mTambourineManager.Start();
    FOREACH (it, mVocalParts) {
        (*it)->Start();
    }
    FOREACH (it, mSingers) {
        (*it)->Start();
    }
    SetAutoplay(mUser->mAutoplay);
    BandTrack *track = GetBandTrack();
    if (track) {
        bool showMeter = !mUser->IsNullUser() && TheGame->AllowOverdrivePhrases();
        track->ShowOverdriveMeter(showMeter);
    }
}

void VocalPlayer::StartIntro() {
    Player::StartIntro();
    TheGameMicManager->SetPlayback(true);
    FOREACH (it, mVocalParts) {
        (*it)->StartIntro();
    }
    FOREACH (it, mSingers) {
        (*it)->StartIntro();
    }
}

void VocalPlayer::Restart(bool b1) {
    if (!mVocalParts.empty()) {
        FOREACH (it, mVocalParts) {
            (*it)->Restart(b1);
        }
        FOREACH (it, mSingers) {
            (*it)->Restart(b1);
        }
        mNextPacketSendTime = 0;
        mLastDeploymentSinger = -1;
        mBeatMaster->GetAudio()->SetVocalFailFader(0);
        mBeatMaster->GetAudio()->SetVocalDuckFader(0);
        if (mTrack) {
            mCouldChat = CanChat();
            SendCanChat(mCouldChat);
        }
        Player::Restart(b1);
        mPhraseActivePartCount = 0;
        if (!b1) {
            mStats.SetVocalSingerAndPartCounts(mSingers.size(), mVocalParts.size());
            mPhrasePercentageCount = 0;
            mPhrasePercentageTotal = 0;
        }
        mTambourineManager.Restart();
        mSectionStartPhrasePercentageTotal = 0;
        mSectionStartPhrasePercentageCount = 0;
        mSectionStartScore = 0;
        if (mUser->IsLocal()) {
            JoypadKeepAlive(mUser->GetLocalUser()->GetPadNum(), true);
        }
        RememberCurrentMics();
    }
}

void VocalPlayer::SetPaused(bool b1) {
    if (b1)
        CountPause();
    mTambourineManager.SetPaused(b1);
    FOREACH (it, mVocalParts) {
        (*it)->SetPaused(b1);
    }
    FOREACH (it, mSingers) {
        (*it)->SetPaused(b1);
    }
    if (mUser->IsLocal()) {
        JoypadKeepAlive(mUser->GetLocalUser()->GetPadNum(), !b1);
    }
}

void VocalPlayer::Jump(float f1, bool b2) {
    if (!mVocalParts.empty()) {
        FOREACH (it, mVocalParts) {
            (*it)->Jump(f1, b2);
        }
        FOREACH (it, mSingers) {
            (*it)->Jump(f1, b2);
        }
        mNextPacketSendTime = 0;
        mLastDeploymentSinger = -1;
        mPhraseActivePartCount = 0;
        mPhrasePercentageTotal = 0;
        mPhrasePercentageCount = 0;
        mTambourineManager.Jump(f1);
        mTambourineManager.SetTambourine(mVocalParts.front()->InTambourinePhrase());
        BandTrack *track = GetBandTrack();
        if (track)
            track->StopDeploy();
        if (mTrack) {
            mTrack->RebuildHUD();
            mTrack->JumpReset();
        }
    }
}

float VocalPlayer::GetNotesHitFraction(bool *bptr) const {
    if (bptr) {
        *bptr = mPhrasePercentageCount > 0;
    }
    if (mPhrasePercentageCount == 0)
        return 0;
    else
        return mPhrasePercentageTotal / (float)mPhrasePercentageCount;
}

Symbol VocalPlayer::GetStarRating() const {
    return TheScoring->GetStarRating(GetNumStars());
}

void VocalPlayer::LocalSetEnabledState(EnabledState state, int i1, BandUser *user, bool b1) {
    Player::LocalSetEnabledState(state, i1, user, b1);
    if (state == kPlayerEnabled || state == kPlayerDisabled || state == kPlayerDisconnected) {
        int tick = (int)MsToTick(GetSongMs());
        bool enabled = (state == kPlayerEnabled);
        mCommonPhraseCapturer->Enabled(this, mTrackNum, tick, enabled);
    } else if (state == kPlayerBeingSaved || state == kPlayerDroppingIn) {
        std::vector<VocalPhrase> &phrases = mVocalParts[0]->mVocalNoteList->mPhrases;
        for (std::vector<VocalPhrase>::iterator it = phrases.begin(); it != phrases.end(); ++it) {
            if (mEnableMs <= it->unk0) {
            mEnableMs = it->unk0;
            FOREACH (vp, mVocalParts) {
                (*vp)->SetFirstPhraseMsToScore(it->unk0);
            }
            break;
            }
        }
        if (mTrack) {
            mTrack->RebuildHUD();
        }
    }
    mBeatMaster->GetAudio()->SetVocalState(
        state == kPlayerEnabled || state == kPlayerDisconnected
        || state == kPlayerDroppingIn
    );
}

int VocalPlayer::LocalDeployBandEnergy() {
    int ret = Player::LocalDeployBandEnergy();
    FOREACH (it, mVocalParts) {
        (*it)->LocalDeployBandEnergy();
    }
    return ret;
}

void VocalPlayer::RemoteUpdateCrowd(float f1) {
    Performer::RemoteUpdateCrowd(f1);
    mBeatMaster->GetAudio()->SetVocalDuckFader(RemoteVocalVolume());
}

void VocalPlayer::UpdateCrowdMeter(int rating, int phraseIdx) {
    if (!IsNet()) {
        float multiplier = 1.0f;
        float ratingFraction = (float)rating * 0.25f;
        if (ratingFraction > mCrowd->mRawValue) {
            multiplier = GetCrowdBoost();
        }
        mCrowd->UpdatePhrase(ratingFraction, multiplier);
        CheckCrowdFailure();
        static Message msg(Symbol("send_update_crowd"));
        HandleType(msg);
    }
    if (rating >= 4) {
        BuildHitStreak(phraseIdx, 0);
        EndMissStreak();
        Hit();
    } else {
        EndHitStreak();
        BuildMissStreak(phraseIdx);
    }
}

float VocalPlayer::RemoteVocalVolume() const {
    float ret = mCrowd->GetDisplayValue();
    if (ret > 0.33f)
        return 0;
    else if (ret <= 0) {
        return 1.0f;
    } else
        return 1.0f - ret / 0.33f;
}

// W17-CLEAN-VP (2026-09-30): cleanup-only pass, no codegen changes. This
// function was called "too diffuse to price as a single-defect row" in
// docs/decomp/W16_NEXT_WAVE_TARGETING_2026-09-16.md (lane X3, 3,388 B,
// fuzzy ~93.84%). Re-examined block by block via objdiff's auto-diagnosis;
// verified findings (each also commented at its own site below):
//   - Several residuals are the same systemic MSVC instruction-selection
//     choice (record-form `clrrwi.` vs retail's separate compare+branch for
//     `vector::size() != 0`-shaped conditions), not logic bugs.
//   - One residual (singer-score-loop energy read) is a confirmed benign
//     register-allocation/scheduling artifact: retail spills a live value to
//     the stack at this point, we don't need to -- verified against
//     Singer.h's own 0x5c/0x60/0x64 offset comments, not a member-confusion
//     bug.
//   - One residual (solo pitch-correction target) is an unresolved, flagged
//     compiler CSE/scheduling question -- see the TODO(W17) comment at its
//     site. Not fixed here; left for a future matching lane.
//   - Register-swap and prologue/register-save-helper (__savegprlr_14 vs
//     __savegprlr_15) residuals are permuter-class noise, out of scope per
//     standing project directive (permuter OFF).
//   - Several retail callees at this row's Function Call Diff are ICF-folded
//     template instantiations (objdiff shows an arbitrary survivor spelling,
//     not a wrong callee) or genuinely unidentified fn_XXXXXXXX addresses;
//     one candidate (SongSectionOnly) was identified and named this pass --
//     see scripts/target_symbol_map.json and the W17-CLEAN-VP report for the
//     COFF evidence. The remaining ~19 target-only addresses are unverified
//     and left for a future identification pass.
void VocalPlayer::Poll(float ms, const SongPos &pos) {
    if (mGameOver)
        return;
    if (!mTrack || (TheGame && TheGame->mIsPaused)) {
        Player::Poll(ms, pos);
        return;
    }

    float fCompMS = GetCompensatedTime(ms);
#ifdef HX_NATIVE
    VocalFrameSpewData *spewData = mFrameSpewData;

    // Update frame spew data if active
    if (spewData) {
        spewData->mMs = 0.0f;
        spewData->mCompMs = 0.0f;

        int singerDataSize = spewData->mSingerData.size();
        int partDataSize = spewData->mPartData.size();

        spewData->mSingerData.clear();
        VocalFrameSpewData::VocalFrameSingerData defSinger;
        spewData->mSingerData.resize(singerDataSize, defSinger);

        spewData->mPartData.clear();
        VocalFrameSpewData::VocalFramePartData defPart;
        spewData->mPartData.resize(partDataSize, defPart);

        mFrameSpewData->mMs = ms;
        mFrameSpewData->mCompMs = fCompMS;
    }
#endif

    /* Retail's Poll contains NO VocalOverlay code at all. Evidence (map-
       independent, control-validated): scanning all 847 instructions of retail
       fn_826EB030 for memory operands finds ZERO accesses to the mVocalOverlay
       slot at 0x3c8, while every control offset known present in the target
       column of the diff does fire (0x38c x5, 0x374 x2, 0x360 x1, 0x190 x1).
       VocalOverlay is the dev overlay, created only by ToggleOverlay().
       ⚠ Do NOT try to settle this by enumerating retail's `bl` targets: only
       1 of Poll's 80 call sites is in target_symbol_map.json, so a name search
       returns 0 for EVERYTHING (including Singer/VocalPart/Player, which are
       certainly called) -- a vacuous instrument shaped like a decisive negative.
       The member itself is KEPT: retail still uses 0x3cc, so the slot is not
       removed from the class; only the code is gated. */
#ifdef HX_NATIVE
    if (mVocalOverlay) {
        mVocalOverlay->Reset((int)mSingers.size());
    }
#endif

    // Declaration order matches target register allocation
    // Retail's literal is 0x50061C46 (9.0e9f, .rdata 0x820EEF18). 9985.578125f
    // is 0x461C0650 -- the same word read byte-swapped.
    float frameMinPitch = 9.0e9f;
    float frameMaxPitch = 0.0f;
    bool bWasInFreestyleSection = InFreestyleSection();

    // scoredPartIndices initialized before SongSectionOnly, reserve mVocalParts.size()
    std::vector<int> scoredPartIndices;

    float fBestFreestyleDeployAmt = 0.0f;
    int iMaximumFreestyleDeploymentSinger = -1;
    scoredPartIndices.reserve(mVocalParts.size());

    // Determine section scoring state
    float fSectionBeginMs = 0.0f;
    float fSectionEndMs = 0.0f;
    if (SongSectionOnly(fSectionBeginMs, fSectionEndMs)) {
        mScoringEnabled =
            fCompMS > (100.0f + fSectionBeginMs) && fCompMS < (100.0f + fSectionEndMs);
    } else {
        mScoringEnabled = true;
    }

    // Allocate temp vectors for parts/singers that are scoring
    std::vector<VocalPart *> partsArray;
    partsArray.reserve(mVocalParts.size());
    std::vector<Singer *> singersArray;
    singersArray.reserve(mSingers.size());

    VocalPart *pUnpitchedPart = 0;

    // Poll tambourine manager
    mTambourineManager.Poll(fCompMS);

    // Poll each vocal part and build partsArray
    bool bSomePitched = true;
    bool bSomeUnpitched = false;
    FOREACH (it, mVocalParts) {
        VocalPart *pPart = *it;
        pPart->Poll(fCompMS, pos);
        pPart->ClearSingerCandidates();

        // pPart->unk98 is an un-named VocalPart field (no verified member name or
        // offset comment in VocalPart.h) that appears to hold a per-part pitch
        // mode: usage below implies 0 == pitched, 1 == unpitched. Inferred from
        // context only -- not confirmed against retail or the class layout, so
        // the field itself is left as unk98 rather than guessed at.
        int partPitchMode = pPart->unk98;
        bSomePitched = bSomePitched | (partPitchMode == 0);
        bSomeUnpitched = bSomeUnpitched | ((partPitchMode - 1) == 0);

        VocalNote *pNote = (VocalNote *)pPart->mVocalNoteList->NoteAt(fCompMS);
        if (pNote && pNote->mUnpitchedNote && !pUnpitchedPart) {
            pUnpitchedPart = pPart;
        }

        bool bCanScore = mScoringEnabled && !InRollback();

        if (bCanScore && pPart->NearNote(fCompMS) && pPart->ScoringEnabled()) {
            float fPitchAtTime = pPart->mVocalNoteList->PitchAt(fCompMS);
            if (0.0f != fPitchAtTime) {
                if (fPitchAtTime < frameMinPitch) frameMinPitch = fPitchAtTime;
                if (frameMaxPitch < fPitchAtTime) frameMaxPitch = fPitchAtTime;
            }
            partsArray.push_back(pPart);
        }
    }

    // Check if freestyle section changed (for deploy clearing)
    bool bIsFreestyle = InFreestyleSection();
    bool bChangedFreestyle = (bWasInFreestyleSection != bIsFreestyle);
    (void)bChangedFreestyle;

    // Get pitch offset from song DB and update tuning
    float pitchOffset = TheSongDB->GetPitchOffsetForTick((int)MsToTick(ms));
    if ((-50.0f <= pitchOffset) && (pitchOffset <= 50.0f)) {
        mTuningOffset = pitchOffset / 100.0f;
    }

    // Build singer pitch tracker
    std::vector<float> singerPitches;
    singerPitches.resize(mSingers.size(), 0.0f);

    // Poll each singer and score
    FOREACH (sIt, mSingers) {
        Singer *pSinger = *sIt;
        pSinger->mMicPitchOffset = mTuningOffset;
        pSinger->SetMicProcessing(bSomePitched, bSomeUnpitched);
        pSinger->Poll(fCompMS, pos, frameMinPitch, frameMaxPitch);

        // Clear freestyle deployment if section ended
        if (bChangedFreestyle && !InFreestyleSection()) {
            pSinger->ClearFreestyleDeployment();
        }

        singerPitches[pSinger->mSingerIndex] = pSinger->mFrameMicPitch;

#ifdef HX_NATIVE
        if (mVocalOverlay) {
            mVocalOverlay->AppendSingerPitch(pSinger->mSingerIndex, pSinger->mFrameMicPitch);
        }
#endif
        /* Retail guards this push_back; we did it unconditionally. Retail
           0x826EB410..0x826EB434:
             lfs f0,0x5c(pSinger); fcmpu cr6,f0,f29; bne cr6,DO
             lwz r11,0x2b8(pSinger); lfs f0,0x4(r11); fcmpu cr6,f0,f29
             ble cr6,SKIP
           DO: addi r4,r31,0x68; addi r3,r31,0x90; bl push_back
           f29 is loaded once at 0x826EB0AC from 0x82000D78, which decodes to
           0.0f. 0x2b8 is mTalkyMatcher and its +0x4 float is mVoiceBeat.unk4 --
           the same slot the AppendTalkyData call site loads as its float arg. */
        if (pSinger->mFrameMicPitch != 0.0f
            || pSinger->mTalkyMatcher->mVoiceBeat.unk4 > 0.0f) {
            singersArray.push_back(pSinger);
        }

        // Score singer against each part
        float fBestPitchDeviation = kInvalidPitch;
        FOREACH (pIt, mVocalParts) {
            VocalPart *pPart = *pIt;
            bool bScoringAllowed = mScoringEnabled && !InRollback();

            if (bScoringAllowed && pPart->ScoringEnabled()) {
                VocalScoreCache &cache = pSinger->AccessScoreCache(pPart->mPartIndex);
                // W17: objdiff flags a mismatch here (retail spills a live value to
                // a stack slot with `stfs`, ours instead loads straight from
                // pSinger->mLastFrameMicEnergy at the same positional index) --
                // confirmed benign register-allocation/scheduling noise, not a
                // member-confusion bug: mFrameMicPitch/mLastFrameMicEnergy/
                // mSmoothedMicEnergy below are read at 0x5c/0x60/0x64 respectively,
                // an exact match for Singer.h's own `// 0xHEX` comments on those
                // three members. No source change indicated.
                float fEnergy = pSinger->mLastFrameMicEnergy;
                int iRating;
                float fDev;
                pPart->ScoreSinger(
                    fCompMS,
                    pSinger->mFrameMicPitch,
                    fEnergy,
                    fEnergy - pSinger->mSmoothedMicEnergy,
                    pSinger->mOctaveOffset,
                    pSinger->mTalkyMatcher,
                    cache,
                    iRating,
                    fDev
                );
                if (kInvalidPitch != fDev && fabs(fDev) < fabsf(fBestPitchDeviation)) {
                    fBestPitchDeviation = fDev;
                }

                float fScore = cache.unk0;
                if (cache.unk20) {
                    fScore *= mNonpitchStickiness;
                }
                pSinger->AppendToScoreHistory(fCompMS, pPart->mPartIndex, fScore, iRating);
            }
        }
        if (kInvalidPitch != fBestPitchDeviation) {
            pSinger->UpdatePitchDeviation(fBestPitchDeviation);
        }

        // Check ambiguity between pairs of parts
        for (int i = 0; i < mVocalParts.size() - 1; i++) {
            VocalPart *pPartA = mVocalParts[i];
            if (pPartA->PhraseHasUnpitchedNotes()) continue;
            VocalScoreCache &cacheA = pSinger->AccessScoreCache(i);
            float fScoreA = cacheA.unk0;

            for (int j = i + 1; j < mVocalParts.size(); j++) {
                MILO_ASSERT(j != i, 0x3FA);
                VocalPart *pPartB = mVocalParts[j];
                if (pPartB->PhraseHasUnpitchedNotes()) continue;
                VocalScoreCache &cacheB = pSinger->AccessScoreCache(j);
                float fScoreB = cacheB.unk0;

                if (0.0f == fScoreA && 0.0f == fScoreB) continue;

                if (fabsf(fScoreA - fScoreB) < 0.00001f) {
                    if (0.0f != cacheA.unkc || 0.0f != cacheB.unkc) {
                        pSinger->AddAmbiguousPart(i, j);
                    }
                } else {
                    pSinger->DisableAmbiguousPart(i, j);
                }
            }
        }

    }

    // Greedy matching: repeatedly assign singers to parts until no more matches
    {
        bool bAnyAssigned;
        do {
            bAnyAssigned = false;
            // For each singer in singersArray, find best part
            FOREACH (sIt2, singersArray) {
                Singer *pSinger = *sIt2;
                VocalPart *pBestPart = 0;
                float fBestTargetPitch;
                float fBestScore;
                bool bFoundPart = FindBestPart(
                        pSinger->mFrameMicPitch,
                        fCompMS,
                        partsArray,
                        pSinger,
                        pBestPart,
                        fBestTargetPitch,
                        fBestScore
                    );
                if (bFoundPart) {
                    pBestPart->AddSingerCandidate(pSinger, fBestScore);
                    pSinger->mBestTargetPitch = fBestTargetPitch;
                    bAnyAssigned = true;
                }
#ifdef HX_NATIVE
                if (mVocalOverlay) {
                    mVocalOverlay->AddPossiblePart(pSinger->mSingerIndex, pBestPart);
                }
#endif
            }
            if (!bAnyAssigned) break;

            // Assign best singer candidate for each part, build scoredPartIndices
            FOREACH (pIt2, partsArray) {
                VocalPart *pPart = *pIt2;
                Singer *pBestSinger = pPart->GetBestSingerCandidate();
                if (pBestSinger) {
                    pBestSinger->SetAssignedPart(pPart->mPartIndex, mVocalPartBias);
                    pBestSinger->mFrameTargetPitch = pBestSinger->mBestTargetPitch;
                    int partIndex = pPart->mPartIndex;
                    scoredPartIndices.push_back(partIndex);
                }
            }

            /* Retail does NOT inline these two remove_if calls. It materialises
               the predicate as a single word in r5 -- the value of
               &VocalPart::HasBestSingerCandidate, since an MSVC pointer-to-member
               for a non-virtual single-inheritance class is just the code address
               -- then makes one out-of-line `bl remove_if` followed by
               `bl vector<T*>::erase`. Retail 0x826EB030, the two `lis/addi
               ?HasBestSingerCandidate@VocalPart@@QAA_NXZ` + `bl` sites.

               What used to be here was a hand-rolled 4-at-a-time Duff's device
               justified by the comment "MWCC does not inline the generic
               std::find_if wrapper, so hand-roll the body to match target's
               inlined codegen". That reasoning is about the WRONG COMPILER --
               MWCC is the Wii toolchain and this is the MSVC X360 match build --
               and it was measurably backwards: the hand-inlining produced two
               59-instruction blocks (118 instructions) that appear in our object
               and nowhere in retail. */
            partsArray.erase(
                std::remove_if(
                    partsArray.begin(),
                    partsArray.end(),
                    std::mem_fun_t<bool, VocalPart>(&VocalPart::HasBestSingerCandidate)
                ),
                partsArray.end()
            );
            singersArray.erase(
                std::remove_if(
                    singersArray.begin(),
                    singersArray.end(),
                    std::const_mem_fun_t<bool, Singer>(&Singer::HasAssignedPart)
                ),
                singersArray.end()
            );
        // W17: this loop condition is one of several confirmed sites in Poll
        // (also the two vector-size checks feeding the octave-offset loop below)
        // where retail emits a plain compare (`srawi`/`srawi.` + `cmpwi cr6` +
        // branch) but our compiled code folds the comparison into a record-form
        // instruction (`clrrwi.`) that sets CR0 implicitly. This is a systemic
        // MSVC instruction-selection choice for "vector::size() != 0"-shaped
        // comparisons, not a logic difference -- do not chase it as a source bug.
        } while (partsArray.size() != 0 && singersArray.size() != 0);
    }

#ifdef HX_NATIVE
    if (mVocalOverlay) {
        mVocalOverlay->EqualizeSingerStrings();
    }
#endif

    // Finalize octave offsets and assigned parts for each singer
    static bool bDoCorrect = true;
    const float kSemitone = 12.0f;
    const float kHalfSemitone = 0.5f;
    const float kMaxOctaveDistance = 2.5f;

    FOREACH (sIt2, mSingers) {
        Singer *pSinger = *sIt2;
        pSinger->AllScoresAreIn(scoredPartIndices);
        pSinger->ResolveAmbiguity();
        int iOctaveOffset = 0;
        bool bHasBestTarget = (0.0f != pSinger->mBestTargetPitch);
        float fFramePitch = pSinger->mFrameMicPitch;

        if (bHasBestTarget) {
            pSinger->ClearFreestyleDeployment();
        }

        if (pSinger->GetFrameAssignedPart() != -1) {
            VocalPart *pPart = mVocalParts[pSinger->GetFrameAssignedPart()];
            VocalScoreCache &cache = pSinger->AccessScoreCache(pPart->mPartIndex);
            pPart->AddScore(cache);
            pSinger->mFrameBestHitScore = cache.unk0;

#ifdef HX_NATIVE
            if (mFrameSpewData) {
                mFrameSpewData->mSingerData[pSinger->mSingerIndex].unk8 =
                    (int)pSinger->mFrameAssignedPart;
            }
#endif

            if (pSinger->GetFrameMicPitch() > 0.0f) {
                iOctaveOffset =
                    pSinger->AccessScoreHistory(pPart->mPartIndex).GetOctaveOffset();
            }
        } else {
            float fOld = pSinger->GetFrameMicPitch();
            iOctaveOffset = pSinger->mOctaveOffset;
            if (0.0f != fOld) {
                float fPrev = pSinger->mBestTargetPitch;
                bool bHasPrev = (0.0f != fPrev);
                if (bHasPrev) {
                    float diff = fPrev - fOld;
                    float absDiff = fabsf(diff);
                    float mod = (float)fmod(absDiff, 12.0);
                    float alt = kSemitone - mod;
                    mod = ((alt - mod) >= 0.0f) ? mod : alt;
                    if (mod <= kMaxOctaveDistance) {
                        int sign = 1;
                        if (diff <= 0.0f) {
                            sign = -1;
                        }
                        int octaveAdjust = (int)(kHalfSemitone + absDiff / kSemitone);
                        iOctaveOffset = octaveAdjust * sign;
                    }
                }
            }
            pSinger->mFrameBestHitScore = 0.0f;
            pSinger->mFrameTargetPitch = 0.0f;

            if (!IsNet() && InFreestyleSection()) {
                float fDeployAmt = pSinger->AddToFreestyleDeployment(fCompMS);
                if (fDeployAmt > fBestFreestyleDeployAmt) {
                    fBestFreestyleDeployAmt = fDeployAmt;
                    iMaximumFreestyleDeploymentSinger = pSinger->mSingerIndex;
                }
            }
        }

        if (0.0f != pSinger->GetFrameMicPitch()) {
            float fAdjusted = (kSemitone * (float)iOctaveOffset) + pSinger->GetFrameMicPitch();
#ifdef HX_NATIVE
            // Headless: mTrack is a non-null sentinel (no VocalTrackDir render
            // object). Retail clamps the octave to the on-screen display range;
            // with no display track fBottom/fTop are 0, so the wrap is inactive
            // and the REAL SuddenOctaveShift path handles octave folding.
            float fBottom = 0.0f;
            float fTop = 0.0f;
#else
            VocalTrack *pTrack = mTrack;
            MILO_ASSERT(pTrack, 0x501);
            float fBottom = pTrack->GetBottomDisplayPitch() - mTrackWrappingMargin;
            float fTop = mTrackWrappingMargin + pTrack->GetTopDisplayPitch();
#endif
            if ((fBottom > 0.0f) && (fAdjusted < fBottom)) {
                iOctaveOffset += (int)((fBottom - fAdjusted) / kSemitone) + 1;
            } else if ((fTop > 0.0f) && (fAdjusted > fTop)) {
                iOctaveOffset += (int)((fTop - fAdjusted) / kSemitone) - 1;
            } else {
                int iSudden = pSinger->SuddenOctaveShift(fFramePitch);
                if (iSudden && bDoCorrect) {
                    iOctaveOffset -= iSudden;
                }
            }
        }

        if (0.0f != pSinger->GetFrameMicPitch()) {
            pSinger->SetFrameMicPitch(
                (kSemitone * (float)iOctaveOffset) + pSinger->GetFrameMicPitch()
            );
        }
        pSinger->SetOctaveOffset(iOctaveOffset);

#ifdef HX_NATIVE
        if (mVocalOverlay) {
            mVocalOverlay->AppendAssignedPart(pSinger, mVocalParts);
        }
#endif

        pSinger->UpdatePitchHistory(fFramePitch);

#ifdef HX_NATIVE
        if (mVocalOverlay) {
            mVocalOverlay->AppendEnergy(pSinger->mSingerIndex, pSinger->mLastFrameMicEnergy, fCompMS);
        }
        if (mVocalOverlay) {
            TalkyMatcher *pTalky = pSinger->mTalkyMatcher;
            mVocalOverlay->AppendTalkyData(
                pSinger->mSingerIndex,
                pTalky->mVoiceBeat.unk1,
                pTalky->mVoiceBeat.unk0,
                pTalky->mVoiceBeat.unk4
            );
        }
        if (mVocalOverlay) {
            mVocalOverlay->AppendDeploymentTime(pSinger->mSingerIndex, pSinger->mTotalTambourineDeployment);
        }
#endif
    }

#ifdef HX_NATIVE
    if (mVocalOverlay) {
        mVocalOverlay->AppendDeploymentMarker(mLastDeploymentSinger);
    }
#endif

    // AfterPoll each part
    FOREACH (pIt3, mVocalParts) {
        VocalPart *pPart = *pIt3;
        pPart->AfterPoll(fCompMS);
#ifdef HX_NATIVE
        VocalFrameSpewData *frameSpewData = mFrameSpewData;
        if (frameSpewData) {
            frameSpewData->mPartData[pPart->mPartIndex].unk18 = (int)pPart->mPhraseScore;
            frameSpewData->mPartData[pPart->mPartIndex].unk1c =
                (int)pPart->mPhraseScoreMax;
        }
#endif
    }

    if (!IsNet()) {
        SendVocalState(fCompMS);
    }

    if (mVocalParts[0]->AtPhraseEnd(fCompMS)) {
        HandlePhraseEnd(fCompMS);
    }

    float fRequiredMs;
    if (GetFreestyleDeploymentRequiredMs(fRequiredMs) && fBestFreestyleDeployAmt > fRequiredMs) {
        DeployBandEnergyIfPossible(false);
        mLastDeploymentSinger = iMaximumFreestyleDeploymentSinger;
    }

#ifdef HX_NATIVE
    if (mVocalOverlay) {
        mVocalOverlay->AppendPartData(mVocalParts);
    }
    if (mVocalOverlay) {
        mVocalOverlay->AppendPhraseMeter(FrameOverallPhraseMeterFrac());
    }
    if (mVocalOverlay) {
        mVocalOverlay->FinalizeDisplayString();
    }
#endif

    // Chat handling
    bool bCanChat = CanChat();
    if (mCouldChat != bCanChat) {
        SendCanChat(bCanChat);
        mCouldChat = bCanChat;
    }

    // Mic/pitch correction
    TheGameMicManager->SetOverdriveEffectEnable(IsDeployingBandEnergy());
    if (IsDeployingBandEnergy()) {
        TheGameMicManager->Poll(
            TheTempoMap->GetTempo((int)pos.GetTotalTick())
        );
    }

    // Retail tests the byte span: `clrrwi; subi 4; cntlzw` (size() == 1).
    bool bSolo = mVocalParts.size() == 1;
    if (TheGameMicManager->GetMicCount() == 1 && bSolo) {
        Singer *pFirstSinger = mSingers[0];
        float fHitPct = pFirstSinger->AccessScoreCache(0).unk14;
        float fAdjusted = fHitPct + (mTuningOffset / 100.0f);
        if (0.0f == fHitPct) {
            fAdjusted = 0.0f;
        }
        /* Retail 0x826EB030: `fcmpu f3,f29; mr r4,r17(=0); beq; li r4,1` --
           the flag starts false and is set only on the not-equal arm. A bare
           `fAdjusted != 0.0f` argument emits the opposite shape
           (`li r4,1; bne; mr r4,0`). */
        bool bCorrect = false;
        if (fAdjusted != 0.0f) {
            bCorrect = true;
        }
        TheGameMicManager->SetPitchCorrectionTarget(
            bCorrect, false,
            mSynapseProximitySolo, mSynapseFocusSolo,
            fAdjusted, 0.0f, 0.0f
        );
    } else {
        TheGameMicManager->SetPitchCorrectionTarget(
            false, false,
            mSynapseProximitySolo, mSynapseFocusSolo,
            0.0f, 0.0f, 0.0f
        );
    }

    Player::Poll(ms, pos);

#ifdef HX_NATIVE
    // Frame spew output to stream (native-only debug feature)
    if (mFrameSpewStream) {
        MILO_ASSERT(mFrameSpewData, 0x5CC);
        mFrameSpewData->OutputFrame(*mFrameSpewStream);
    }
#endif
}

bool VocalPlayer::FindBestPart(
    float i_fPitch,
    float i_fTime,
    std::vector<VocalPart *> &i_rParts,
    Singer *i_pSinger,
    VocalPart *&o_rpBestPart,
    float &o_rfBestTargetPitch,
    float &o_rfBestScore
) {
    MILO_ASSERT(i_pSinger, 0x5DA);
    o_rpBestPart = NULL;
    o_rfBestScore = 0.0f;
    for (std::vector<VocalPart *>::iterator it = i_rParts.begin(); it != i_rParts.end();
         ++it) {
        VocalPart *pPart = *it;
        float fTargetPitch;
        float fShiftedPitch
            = 12.0f
                * (float
                )i_pSinger->AccessScoreHistory(pPart->PartIndex()).GetOctaveOffset()
            + i_fPitch;
        TalkyMatcher *pMatcher = i_pSinger->mTalkyMatcher;
        if (pPart->CouldScoreAgainstPart(
                i_fTime,
                pMatcher,
                fShiftedPitch,
                mPitchMaximumDistance,
                fTargetPitch
            )) {
            float fScore = i_pSinger->GetHistoricalScore(i_fTime, pPart->PartIndex());
            if (fScore > o_rfBestScore) {
                o_rfBestTargetPitch = fTargetPitch;
                o_rfBestScore = fScore;
                o_rpBestPart = pPart;
            }
        }
    }
    return o_rpBestPart != NULL;
}

void VocalPlayer::LocalEndgameEnergy(int x) {
#ifdef HX_NATIVE
    // Off-path (endgame world-message fan-out); headless has no TheWorld / world
    // messages. Never reached by the scoring Poll loop.
    (void)x;
    return;
#else
    // Retail 360 does NOT use any of the four Messages*.h globals here.
    // Target fn_826E6298 holds ONE
    // function-local `static Message` (slot 0x82E03524, guard 0x82E0352C),
    // built from Symbol("") -> Message(Symbol) -> atexit, then re-typed per
    // branch: each arm calls Message::SetType(Symbol) (out-of-line
    // fn_8228F068) with its own literal, and the compiler tail-merges the four
    // Symbol(const char*) + SetType sequences before TheWorld->Handle.
    // Same idiom as BandDirector::SetCrowd.
    if (TheWorld) {
        static Message msg("");
        if (x == 0) {
            msg.SetType("endgame_vocals_none");
        } else if (x == 1) {
            msg.SetType("endgame_vocals_low");
        } else if (x == 2) {
            msg.SetType("endgame_vocals_medium");
        } else {
            msg.SetType("endgame_vocals_high");
        }
        TheWorld->Handle(msg, false);
    }
#endif
}

void VocalPlayer::SendCanChat(bool b1) {
#ifdef HX_NATIVE
    // Off-path: no VocalTrackDir render object. The Poll chat gate never fires
    // headless (TheNetSession->IsLocal() is true), so this is never called.
    (void)b1;
#else
    if (mTrack) {
        mTrack->GetVocalTrackDir()->CanChat(b1);
    }
#endif
}

void VocalPlayer::SendVocalState(float f1) {
    MILO_ASSERT(IsLocal(), 0x62C);
    if (f1 < mNextPacketSendTime)
        return;
    std::vector<float> framePercs(mVocalParts.size());
    FOREACH (it, mVocalParts) {
        VocalPart *cur = *it;
        float fPercentage = cur->FramePhraseMeterFrac();
        MILO_ASSERT(( 0.0f) <= ( fPercentage) && ( fPercentage) <= ( 1.0f), 0x63E);
        framePercs[cur->PartIndex()] = fPercentage;
    }
    int packed = PackFloats(framePercs, 0, 1);
    std::vector<float> moreFsToPack(mSingers.size());
    std::vector<int> boolsToPack(mSingers.size());
    FOREACH (it, mSingers) {
        Singer *cur = *it;
        // NOTE: GetFrameMicPitch() is called twice on purpose -- retail homes each
        // inlined call's return temp to the same coalesced stack slot (two dead
        // stfs to 0x54(r31)) and keeps the value live in f13. Hoisting it into a
        // single local produces one materialization and drops the match to 99.2%.
        float f9 = Clamp<float>(
            -1, 1, (cur->GetFrameMicPitch() - cur->mFrameTargetPitch) / mMaxDetune
        );
        bool isSinging = cur->GetFrameMicPitch() != 0;
        moreFsToPack[cur->GetSingerIndex()] = f9;
        boolsToPack[cur->GetSingerIndex()] = isSinging;
    }
    int packedFs2 = PackFloats(moreFsToPack, -1, 1);
    int packedBs = PackBools(boolsToPack);
    static Message vocal_state_msg("send_vocal_state", 0.0f, 0, 0.0f);
    vocal_state_msg[0] = packedFs2;
    vocal_state_msg[1] = packedBs;
    vocal_state_msg[2] = packed;
    HandleType(vocal_state_msg);
    mNextPacketSendTime = f1 + mPacketPeriodMs;
}

void VocalPlayer::RemoteVocalState(int i1, int i2, int i3) {
    std::vector<float> fvec1;
    UnpackFloats(i1, -1, 1, fvec1);
    std::vector<int> ivec;
    UnpackBools(i2, ivec);
    std::vector<float> fvec2;
    UnpackFloats(i3, 0, 1, fvec2);
    FOREACH (it, mSingers) {
        Singer *cur = *it;
        int idx = cur->GetSingerIndex();
        float detune = fvec1[idx] * mMaxDetune;
        cur->SetIsSinging(ivec[idx]);
        cur->Detune(detune);
    }
    FOREACH (it, mVocalParts) {
        (*it)->SetRemotePhraseMeterFrac(fvec2[(*it)->PartIndex()]);
    }
}

unsigned int VocalPlayer::PackFloats(
    const std::vector<float> &i_rFractionArray, float f1, float f2
) const {
    MILO_ASSERT(i_rFractionArray.size() <= 4, 0x6BE);
    float fDenominator = f2 - f1;
    MILO_ASSERT(fDenominator > 0.0f, 0x6C1);
    unsigned int packed = 0;
    for (int i = 0; i < i_rFractionArray.size(); i++) {
        float curF = i_rFractionArray[i];
        if (!(curF >= f1)) {
            curF = f1;
        } else if (!(curF <= f2)) {
            curF = f2;
        }
        float fNormalized = (curF - f1) / fDenominator;
        MILO_ASSERT(( 0.0f) <= ( fNormalized) && ( fNormalized) <= ( 1.0f), 0x6D9);
        int iByteValue = fNormalized * 255.0f;
        MILO_ASSERT(( 0) <= ( iByteValue) && ( iByteValue) <= ( 255), 0x6DC);
        packed |= iByteValue << (i * 8);
    }
    return packed;
}

void VocalPlayer::UnpackFloats(
    int i1, float f1, float f2, std::vector<float> &o_rFractionArray
) const {
    MILO_ASSERT(o_rFractionArray.size() == 0, 0x6ED);
    o_rFractionArray.resize(4);
    float fDifference = f2 - f1;
    MILO_ASSERT(fDifference > 0.0f, 0x6F3);
    for (int i = 0; i < 4; i++) {
        o_rFractionArray[i] = fDifference * ((float)(int)(unsigned char)i1 / 255.0f) + f1;
        i1 >>= 8;
    }
}

unsigned int VocalPlayer::PackBools(const std::vector<int> &i_rBoolArray) const {
    MILO_ASSERT(i_rBoolArray.size() <= 4, 0x70D);
    unsigned int packed = 0;
    for (int i = 0; i < i_rBoolArray.size(); i++) {
        packed |= (bool)(i_rBoolArray[i] != 0) << (i * 8);
    }
    return packed;
}

void VocalPlayer::UnpackBools(int i1, std::vector<int> &o_rBoolArray) const {
    MILO_ASSERT(o_rBoolArray.size() == 0, 0x723);
    o_rBoolArray.resize(4);
    for (int i = 0; i < 4; i++) {
        unsigned char b = (unsigned char)i1;
        o_rBoolArray[i] = b != 0 ? 1 : 0;
        i1 >>= 8;
    }
}

int VocalPlayer::GetSpotlightPhraseID() const {
    FOREACH (it, mVocalParts) {
        int phrase = (*it)->GetSpotlightPhrase();
        if (phrase != -1)
            return phrase;
    }
    return -1;
}

// W17-CLEAN-VP (2026-09-30): cleanup-only pass, no codegen changes. This
// function had no prior cleanup applied (2 recorded attempts, 2,332 B, fuzzy
// ~97.94%). Only local/parameter renames applied here -- no source-level
// findings to report beyond the note at fPartPhraseStartMs/fPartPhraseEndMs
// below. Rename basis, in case a future lane needs to re-derive confidence:
//   - iPartRating/fPartPhraseStartMs/fPartPhraseEndMs/iPartPrevScore are the
//     out-parameter names of the callee this calls into,
//     VocalPart::HandlePhraseEnd(int &o_rRating, float &o_rStartMs,
//     float &o_rEndMs, int &o_rPrevScore, float ms) -- read directly from its
//     definition in src/band3/game/VocalPart.cpp.
//   - fPitchDeviationMean/fPitchDeviationDev are read directly from
//     Singer::GetPitchDeviation(float &mean, float &dev)'s definition in
//     src/band3/game/Singer.cpp.
//   - iPrevScoreSum is the running sum of each part's iPartPrevScore this
//     phrase; it flows into VocalTrack::OnPhraseComplete's third (int)
//     parameter, which VocalTrack.cpp only uses in a "last: %i\n" debug
//     string.
//   - iCappedRating/idx and the retail idx-1 note were
//     already present before this pass and are unchanged.
void VocalPlayer::HandlePhraseEnd(float f1) {
    std::vector<VocalPart *> voxParts = mVocalParts;
    std::sort(voxParts.begin(), voxParts.end(), VocalPart::FramePhraseMeterFracSorter);
    for (int i = 0.0f; i < voxParts.size(); i++) {
        VocalPart *cur = voxParts[i];
        cur->SetPhraseScoreMultiplier(mPartScoreMultipliers->Float(i + 1));
        cur->SetPhraseRank(i);
    }
    int iSpotlightPhraseID = GetSpotlightPhraseID();
    float fMaxPhraseMeterFrac = -1.0f;
    int iPhraseRating = -1;
    int iPrevScoreSum = 0;
    int iHighRatingPartCount = 0;
    int iPrevActivePartCount = mPhraseActivePartCount;
    mPhraseActivePartCount = 0;
    // W17: these two are declared once here, outside the per-part FOREACH
    // below, and each iteration's cur->HandlePhraseEnd(...) call overwrites
    // them -- so by the time mTrack->OnPhraseComplete(fPartPhraseStartMs,
    // fPartPhraseEndMs, ...) reads them further down, only the LAST iterated
    // VocalPart's start/end-ms survive. Matches retail's own structure
    // (single stack slot pair, not per-part accumulation), so this is very
    // likely intentional -- noted here rather than treated as a defect.
    float fPartPhraseStartMs;
    float fPartPhraseEndMs;
    std::vector<float> partMaxScores;
    std::vector<int> partActiveFlags(3, 0);
    FOREACH (it, mVocalParts) {
        VocalPart *cur = *it;
        int iPartRating;
        int iPartPrevScore;
        partMaxScores.push_back(cur->MaxPhraseScore());
        float fPartMeterFrac = FramePhraseMeterFrac(cur->PartIndex());
        if (!cur->InEmptyPhrase() && cur->InPlayablePhrase()
            && !cur->mThisPhrase->mTambourinePhrase && cur->ScoringEnabled()
            && cur->ScoringEnabled()) {
            MaxEq(fMaxPhraseMeterFrac, fPartMeterFrac);
        }

        cur->HandlePhraseEnd(iPartRating, fPartPhraseStartMs, fPartPhraseEndMs, iPartPrevScore, f1);
        if (!cur->InEmptyPhrase() && cur->InPlayablePhrase() && cur->ScoringEnabled()) {
            partActiveFlags[cur->PartIndex()] = 1;
            mPhraseActivePartCount++;
        }
        iPrevScoreSum += iPartPrevScore;
        if (iPartRating > iPhraseRating && ScoringEnabled() && cur->ScoringEnabled()) {
            iPhraseRating = iPartRating;
        }
        if (iPartRating >= 4)
            iHighRatingPartCount++;
        mStats.SetVocalPartPercentage(
            cur->PartIndex(), cur->GetOverallPartHitPercentage()
        );
    }
    if (fMaxPhraseMeterFrac >= 0 && ScoringEnabled()) {
        mPhrasePercentageCount++;
        mPhrasePercentageTotal += fMaxPhraseMeterFrac;
    }
    FOREACH (it, mSingers) {
        Singer *cur = *it;
        cur->HandlePhraseEnd(f1, partMaxScores);
        FOREACH (it2, mVocalParts) {
            VocalPart *curPart = *it2;
            float pct = cur->GetPartPercentage(curPart->PartIndex());
            mStats.SetSingerPartPercentage(
                cur->GetSingerIndex(), curPart->PartIndex(), pct
            );
        }
        float fPitchDeviationMean, fPitchDeviationDev;
        cur->GetPitchDeviation(fPitchDeviationMean, fPitchDeviationDev);
        mStats.SetSingerPitchDeviationInfo(cur->GetSingerIndex(), fPitchDeviationMean, fPitchDeviationDev);
    }
    if (iPrevActivePartCount >= 2) {
        Stats &stats = mStats;
        stats.mDoubleHarmonyPhraseCount++;
        if (iHighRatingPartCount >= 2) {
            stats.mDoubleHarmonyHit++;
        }
        if (iPrevActivePartCount == 3) {
            stats.mTripleHarmonyPhraseCount++;
            if (iHighRatingPartCount == 3) {
                stats.mTripleHarmonyHit++;
            }
        }
    }
    if (iHighRatingPartCount > 1)
        iPhraseRating = iHighRatingPartCount + 3;
    if (iPhraseRating != -1) {
        // A named local: retail stores the 4 to 0x50(r31) before the
        // CurrentPhraseIndex call and selects the minimum after it.
        int iMaxRating = 4;
        int idx = mVocalParts.front()->CurrentPhraseIndex();
        // idx - 1, not idx: `subi r5,r3,0x1` right before the call.
        UpdateCrowdMeter(std::min(iPhraseRating, iMaxRating), idx - 1);
    }
    mTambourineManager.SetTambourine(mVocalParts.front()->InTambourinePhrase());
    const bool bSpotlightPhraseHit = iSpotlightPhraseID != -1 && iPhraseRating >= 4;
#ifdef HX_NATIVE
    // Headless: mTrack is a non-null sentinel with no VocalTrackDir render object
    // and no net session, so the retail render/net phrase-end leaves
    // (VocalTrack::OnPhraseComplete, LocalScorePhrase -> VocalTrackDir feedback,
    // the send_score_phrase net message, and the CommonPhraseCapturer overdrive
    // arbiter which needs mTrack->mTrackConfig) are skipped. The REAL per-part
    // rating / stats / points accumulation above (cur->HandlePhraseEnd,
    // CalculatePhraseRating, AddPoints, UpdateCrowdMeter) still runs.
    (void)bSpotlightPhraseHit;
#else
    if (mTrack) {
        if (ScoringEnabled()) {
            mTrack->OnPhraseComplete(fPartPhraseStartMs, fPartPhraseEndMs, iPrevScoreSum);
        }
        if (IsLocal()) {
            LocalScorePhrase(iPhraseRating, partActiveFlags, bSpotlightPhraseHit);
            int packedBools = PackBools(partActiveFlags);
            static Message msg("send_score_phrase", 0, 0, 0);
            msg[0] = iPhraseRating;
            msg[1] = packedBools;
            // Third argument is the spotlight flag (RemoteScorePhrase's bool),
            // masked to a byte: retail clrlwi r11,r26,24 into the DataNode.
            msg[2] = bSpotlightPhraseHit;
            HandleType(msg);
        }
    }
    // Retail holds these two as function-local statics (guard 0x82E03620;
    // target fn_826E97F8 emits Symbol("send_vocal_phrase_over")/Message/atexit
    // then Symbol("phrase_end")/Message/atexit), not as Messages*.h globals.
    // The `phrase_rating` Message below is genuinely NON-static in retail (no
    // guard, no atexit) -- leave it alone.
    static Message send_vocal_phrase_over_msg("send_vocal_phrase_over");
    HandleType(send_vocal_phrase_over_msg);
    static Message phrase_end_msg("phrase_end");
    Handle(phrase_end_msg, false);
    if (ScoringEnabled() && iPhraseRating != -1) {
        Message msg("phrase_rating", iPhraseRating);
        Handle(msg, false);
    }
    if (bSpotlightPhraseHit && ScoringEnabled() && mTrack) {
        mCommonPhraseCapturer->HandleVocalPhrase(
            this, mTrack->mTrackConfig.TrackNum(), iSpotlightPhraseID, bSpotlightPhraseHit
        );
    }
#endif
    if (ScoringEnabled())
        UpdateSectionStats();
}

void VocalPlayer::LocalScorePhrase(
    int i1, const std::vector<int> &i_rNewPhraseActiveParts, bool b3
) {
    if (mTrack) {
        VocalTrackDir *dir = mTrack->GetVocalTrackDir();
        dir->ShowPhraseFeedback(i1, -1, -1, IsDeployingBandEnergy());
        dir->UpdateVocalMeters(
            i_rNewPhraseActiveParts[0],
            i_rNewPhraseActiveParts[1],
            i_rNewPhraseActiveParts[2],
            false
        );
        if (b3 && ScoringEnabled()) {
            dir->SpotlightPhraseSuccess();
        }
    }
}

void VocalPlayer::RemoteScorePhrase(int i1, int i2, bool b3) {
    std::vector<int> bools;
    UnpackBools(i2, bools);
    LocalScorePhrase(i1, bools, b3);
}

void VocalPlayer::HookupTrack() {
#ifdef HX_NATIVE
    // Off-path: the driver installs a sentinel mTrack directly (no BandUser track,
    // no VocalTrack render object, no dynamic_cast RTTI). Never called headless.
    return;
#else
    mTrack = dynamic_cast<VocalTrack *>(GetUser()->GetTrack());
    MILO_ASSERT(mTrack, 0x88A);
    std::vector<VocalPhrase> &phrases = mVocalParts[0]->mVocalNoteList->mPhrases;
    mTrack->Restart(this, phrases[0].unk0 + phrases[0].unk4, phrases[1].unk0 + phrases[1].unk4);
    mCouldChat = CanChat();
    SendCanChat(mCouldChat);
    UpdateMicDisplay();
    mTrack->GetVocalTrackDir()->SetMaxMultiplier(mBehavior->GetMaxMultiplier());
#endif
}

DECOMP_FORCEACTIVE(
    VocalPlayer, "fast song scoring should only be done on PC.", "vocal_score_song_done"
)

void VocalPlayer::UnHookTrack() { mTrack = nullptr; }
bool VocalPlayer::PastFinalNote() const { return AtLastPhrase(); }

void VocalPlayer::SetMusicSpeed(float speed) {
    mBeatMaster->GetAudio()->GetSongStream()->SetSpeed(speed);
}

void VocalPlayer::SetAutoplay(bool play) {
    GetUser()->SetAutoplay(play);
    mAutoPlay = play;
    FOREACH (it, mSingers) {
        Singer *cur = *it;
        cur->SetAutoplayToPart(play ? cur->GetSingerIndex() : -1);
    }
}

void VocalPlayer::RotateSingerAutoplayPart(int x) {
    if (x < mSingers.size()) {
        Singer *cur = mSingers[x];
        int part = cur->GetAutoplayToPart();
        if (part == -1)
            part = 0;
        else {
            part++;
            if (part >= NumVocalParts()) {
                part = -1;
            }
        }
        cur->SetAutoplayToPart(part);
    }
}

int VocalPlayer::GetSingerAutoplayPart(int x) {
    if (x < mSingers.size()) {
        return mSingers[x]->GetAutoplayToPart();
    } else
        return -1;
}

void VocalPlayer::RotateSingerAutoplayVariationMagnitude(int x) {
    if (x < mSingers.size()) {
        Singer *cur = mSingers[x];
        float mag = cur->GetAutoplayVariationMagnitude() + 0.2f;
        if (mag > 2.1f)
            mag = 0.0f;
        cur->SetAutoplayVariationMagnitude(mag);
    }
}

float VocalPlayer::GetSingerAutoplayVariationMagnitude(int x) {
    if (x < mSingers.size()) {
        return mSingers[x]->GetAutoplayVariationMagnitude();
    } else
        return -1.0f;
}

float VocalPlayer::GetCompensatedTime(float ms) {
#ifdef HX_NATIVE
    // Headless: no BandUser and no TheProfileMgr controller-sync-offset table.
    // Retail adds a per-pad audio/display sync offset; native runs uncompensated.
    // (Whole branch #else'd so the native link never references TheProfileMgr.)
    return ms;
#else
    int padNum = -1;
    if (mUser->IsLocal()) {
        padNum = mUser->GetLocalUser()->GetPadNum();
    }
    return ms + TheProfileMgr.GetSyncOffset(padNum);
#endif
}

void VocalPlayer::SetAutoplayOffset(float f1) {
    FOREACH (it, mSingers) {
        (*it)->SetAutoplayOffset(f1);
    }
}

float VocalPlayer::GetAutoplayOffset() const {
    return mSingers.front()->GetAutoplayOffset();
}

void VocalPlayer::SetVocalPartBias(float bias) { mVocalPartBias = bias; }
float VocalPlayer::GetVocalPartBias() const { return mVocalPartBias; }

DataNode VocalPlayer::OnMidiParser(DataArray *arr) {
    Symbol sym = arr->Sym(2);
    if (sym == "tambourine_gem" && mTrack) {
        mTrack->GetVocalTrackDir()->TambourineNote();
    }
    return 0;
}

int VocalPlayer::OnMsg(const GameMicsChangedMsg &) {
    UpdateMicDisplay();
    return 1;
}

void VocalPlayer::UpdateMicDisplay() {
#ifdef HX_NATIVE
    // Off-path: no VocalTrackDir mic-display render object. Never called headless.
    return;
#else
    if (mTrack) {
        VocalTrackDir *pTrackDir = mTrack->GetVocalTrackDir();
        MILO_ASSERT(pTrackDir, 0x977);
        MILO_ASSERT(TheGameMicManager, 0x979);
        int numMics = TheGameMicManager->GetMicCount();
        bool noMic0 = !TheGameMicManager->HasMic(MicClientID(0, -1));
        bool noMic1 = !TheGameMicManager->HasMic(MicClientID(1, -1));
        bool noMic2 = !TheGameMicManager->HasMic(MicClientID(2, -1));
        bool hadMic0 = HadMic(MicClientID(0, -1));
        bool hadMic1 = HadMic(MicClientID(1, -1));
        bool hadMic2 = HadMic(MicClientID(2, -1));

        bool hadAnyMic;
        if (hadMic0 || hadMic1 || hadMic2)
            hadAnyMic = true;
        else
            hadAnyMic = false;
        bool noLongerHasMic0;
        if (noMic0 && hadMic0)
            noLongerHasMic0 = true;
        else
            noLongerHasMic0 = false;
        bool noLongerHasMic1;
        if (noMic1 && hadMic1)
            noLongerHasMic1 = true;
        else
            noLongerHasMic1 = false;
        bool noLongerHasMic2;
        if (noMic2 && hadMic2)
            noLongerHasMic2 = true;
        else
            noLongerHasMic2 = false;
        int b15;
        if (numMics == 0 || noLongerHasMic0 || noLongerHasMic1 || noLongerHasMic2)
            b15 = 1;
        else
            b15 = 0;
        bool combined = b15 & GetUser()->IsLocal();
        if (!combined) {
            pTrackDir->ShowMicDisplay(false);
        } else {
            if (numMics == 0) {
                static Symbol sym_connect_a_mic("vocals_connect_a_mic");
                pTrackDir->SetMicDisplayLabel(sym_connect_a_mic);
            } else {
                static Symbol sym_mics_disconnected("vocals_mics_disconnected");
                pTrackDir->SetMicDisplayLabel(sym_mics_disconnected);
            }
            if (hadAnyMic) {
                pTrackDir->SetMissingMicsForDisplay(
                    noLongerHasMic0, noLongerHasMic1, noLongerHasMic2
                );
            } else
                pTrackDir->SetMissingMicsForDisplay(true, false, false);
            pTrackDir->ShowMicDisplay(true);
        }
    }
#endif
}

void VocalPlayer::RememberCurrentMics() {
    mInitialMicClientIDs.clear();
    mInitialMicCount = 0;
    MILO_ASSERT(TheGameMicManager, 0x9BE);
    for (int i = 0; i < 4; i++) {
        MicClientID id(i, -1);
        if (TheGameMicManager->HasMic(id)) {
            mInitialMicClientIDs.push_back(id);
            mInitialMicCount++;
        }
    }
}

bool VocalPlayer::HadMic(const MicClientID &id) const {
    FOREACH (it, mInitialMicClientIDs) {
        if (it->mClientID == id.mClientID)
            return true;
    }
    return false;
}

// Retail's predicate MATERIALIZES a bool (`li r11,0/1; clrlwi. r11,r11,24; beq`)
// instead of branching straight to the call, so it is an inlined helper returning
// bool -- a raw `if (a || b || c)` branches directly and never builds the 0/1.
// Retail's compare chain is `==1 -> T; <=2 -> F; <=5 -> T; ==7 -> T; else F`,
// i.e. true for {1,3,4,5,7} == {R2, R1, Tri, Circle, Square} (X/A is excluded).
static bool IsVocalVolumeButton(JoypadButton but) {
    switch (but) {
    case kPad_R2:
    case kPad_R1:
    case kPad_Tri:
    case kPad_Circle:
    case kPad_Square:
        return true;
    default:
        return false;
    }
}

// Retail OnMsg(ButtonDownMsg) materializes this predicate too (`li r11,0/1`), with a
// compare chain true for {13, 15, 17, 19} == {DRight, DLeft, LStickRight, LStickLeft}.
static bool IsVolumeChangeButton(JoypadButton but) {
    switch (but) {
    case kPad_DRight:
    case kPad_DLeft:
    case kPad_LStickRight:
    case kPad_LStickLeft:
        return true;
    default:
        return false;
    }
}

// Inlined into retail OnMsg(ButtonDownMsg) (no out-of-line copy): right steps the
// volume +1, left -1, through the track's VocalTrackDir::ChangeVolume (fn_822F61C0).
void VocalPlayer::HandleChangeVolume(JoypadButton but) {
    int delta;
    switch (but) {
    case kPad_DRight:
    case kPad_LStickRight:
        delta = 1;
        break;
    case kPad_DLeft:
    case kPad_LStickLeft:
        delta = -1;
        break;
    default:
        MILO_WARN("HandleChangeVolume got an unused button! Ignoring it.\n");
        return;
    }
    if (mTrack)
        mTrack->GetVocalTrackDir()->ChangeVolume(delta);
}

// Retail (the HANDLE_MESSAGE(ButtonDownMsg) target) has no "world_panel" focus test:
// a focus panel other than TheGamePanel must be a PracticePanel. After the overshell
// checks it reads the button once, ignores R1/R2 while
// TheGame->mProperties.mUnkTU5_movieSync is set, then dispatches in order: tambourine,
// the volume-param buttons (out of line, fn_826E5FB0), the d-pad/left-stick volume
// steps (inlined, see HandleChangeVolume), and Select during a freestyle section
// (vtable slots 0x158 InFreestyleSection and 0x190 DeployBandEnergyIfPossible, then
// a 0 store to +0x360).
bool VocalPlayer::OnMsg(const ButtonDownMsg &msg) {
#ifdef HX_NATIVE
    // Off-path: the tambourine button-down path pulls TheUI / TheBandUI overshell
    // + PracticePanel RTTI (no UI headless). Not wired to the synthetic-mic run.
    (void)msg;
    return 0;
#else
    if ((User *)GetUser() != msg.GetUser())
        return 0;
    if (TheUI->FocusPanel() != TheGamePanel) {
        if (!dynamic_cast<PracticePanel *>(TheUI->FocusPanel()))
            return 0;
    }
    OvershellSlot *slot = TheBandUI.GetOvershell()->FindSlotForUser(GetUser());
    if (slot->IsLeavingOptions() || !slot->IsHidden())
        return 0;
    JoypadButton but = msg.GetButton();
    if (TheGame->mProperties.mUnkTU5_movieSync && (but == kPad_R1 || but == kPad_R2))
        return 0;
    if (mTambourineManager.IsTambourineButton(but)) {
        mTambourineManager.HandleButtonDown();
    }
    if (IsVocalVolumeButton(but))
        HandleActivateVolume(but);
    else if (IsVolumeChangeButton(but))
        HandleChangeVolume(but);
    if (but == kPad_Select && InFreestyleSection()) {
        DeployBandEnergyIfPossible(false);
        mLastDeploymentSinger = 0;
    }
    return 0;
#endif
}

DECOMP_FORCEACTIVE(
    VocalPlayer,
    "HandleActivateVolume: Couldn't get a VocalParam for supposed volume button %d!\n",
    "HandleDeactivateVolume: Couldn't get a VocalParam for supposed volume button %d!\n",
    "HandleChangeVolume got an unused button! Ignoring it.\n",
    "( 0) <= ( o_rMicNumber) && ( o_rMicNumber) < ( 3)"
)

// Retail calls this out of line from Handle()'s HANDLE_MESSAGE(ButtonUpMsg) arm
// (`subi r3,r25,0x47c; bl fn_826E5E98; clrlwi r11,r3,24`), so the arm also emits the
// ButtonUpMsg temp construction and its ~Message() vtable restore.
// Measured: moving the definition below END_HANDLERS does NOT work -- this MSVC
// (16.00.10224) inlines out-of-line member definitions that appear *later* in the TU,
// so parse order is not a lever; only the pragma is. Per OvershellSlot.cpp:1874,
// __declspec(noinline) is not a substitute for auto_inline in this codebase.
//
// *** CORRECTED 2026-08-17 (lane W22-FRAME) *** This note used to end: "/Ob2 folds that
// stub to a constant and deletes the whole temp, costing Handle 12 instructions and
// 0x10 of frame. auto_inline(off) restores retail's call shape." That mechanism is
// WRONG and it sent one lane at the wrong lever. MEASURED, with the pragma in place:
// the temp is NOT deleted -- both sides construct it and both call OUT OF LINE at the
// same instruction index. The pragma had already done its job; the residual was the
// temp's SLOT, not its existence.
// The real mechanism is ESCAPE / MEMORY-EFFECT analysis, which auto_inline(off) does
// not touch -- it stops INLINING, not MSVC's intra-TU reasoning about a callee. With
// an EMPTY body MSVC proved OnMsg neither writes memory nor lets `&msg` escape, so in
// Handle it killed the dead ~Message vptr store (`stw r30,0x88`), forwarded r27
// instead of reloading mData (`lwz r3,0x8c`), and OVERLAID the temp onto the shared
// scratch slot 0x58 instead of giving it 8 private bytes. Retail keeps FOUR 8-byte
// message temps (0x58/0x88/0x90/0x98); the stub left us with three. That missing 8
// rounds the frame down by 0x10 (0xe0 vs 0xf0) and displaces every callee-saved
// register by one -- 212 charged r28<->r29 sites, ALL of which dissolved with the
// frame. Handing `&msg` to an out-of-TU callee (GetUser) is what defeats the analysis.
// Full record + the Poll wall: docs/decomp/w22-frame.md.

// Ported from retail bytes (fn_826E5E98, 0xF4 B): this is not a stub returning
// 0, and the RB2 dump carries only
// signatures. Retail's body, instruction for instruction:
//   lwz 0x260(this) + vbase adjust      -> (User *)GetUser()
//   bl fn_825150C8 + vbase adjust       -> msg.GetUser();  differ => return false
//   guarded static-Symbol init          -> static Symbol vocalist_volume(...)
//                                          (lbl_82E03510, guard lbl_82E03514,
//                                           string lbl_8202B3CC = "vocalist_volume")
//   mData->Node(3).Int(mData)           -> msg.GetButton()
//   compare chain, then bl fn_826E50C0(this, button)
// The static Symbol is initialized and never READ in retail -- its only effect is
// the ctor -- so it is written as an unused local static deliberately; do not
// "tidy" it away, it is load-bearing codegen.
//
// WHY THIS FUNCTION'S BODY DECIDES ?Handle@VocalPlayer@@'s STACK FRAME (lane W22):
// the previous stub let MSVC prove OnMsg neither writes memory nor lets `&msg`
// escape, so in Handle it (a) killed the dead `~Message` vptr store, (b) forwarded
// r27 instead of reloading mData from the temp, and therefore (c) overlaid the
// ButtonUpMsg temp onto the shared scratch slot 0x58 instead of giving it its own
// 8 bytes -- costing 0x10 of frame (0xe0 vs retail 0xf0) and displacing every
// callee-saved register by one (212 charged r28<->r29 sites). `auto_inline(off)`
// alone does NOT fix this: it stops INLINING, not MSVC's intra-TU memory-effect
// analysis. Handing `&msg` to an out-of-TU callee (GetUser) is what defeats it.
#pragma auto_inline(off)
bool VocalPlayer::OnMsg(const ButtonUpMsg &msg) {
    if ((User *)GetUser() != msg.GetUser())
        return false;
    static Symbol vocalist_volume("vocalist_volume");
    JoypadButton but = msg.GetButton();
    if (IsVocalVolumeButton(but))
        HandleDeactivateVolume(but);
    return false;
}

// Retail fn_826E3C88 (unmapped; the name is ours, lane W16-HX4): maps a volume
// button to the VocalParam it edits. `this` is unused but it is a member (r3 is
// passed through). Shared with the HandleChangeVolume-shaped fn_826E5FB0.
bool VocalPlayer::GetVolumeParam(JoypadButton but, VocalParam &param) const {
    switch (but) {
    case kPad_R2:
        param = kVocalParamCueVolume;
        break;
    case kPad_R1:
        param = kVocalParamMicVolume;
        break;
    case kPad_Tri:
        param = kVocalParamMic2Gain;
        break;
    case kPad_Circle:
        param = kVocalParamMic3Gain;
        break;
    case kPad_Square:
        param = kVocalParamMic1Gain;
        break;
    default:
        return false;
    }
    return true;
}

// Retail fn_826E50C0, 72 B, unpaired (the map does not name it). Ported from retail
// bytes (lane W16-HX4): look up the button's VocalParam, then have the track's
// VocalTrackDir hide the vocalist-volume widget. The previous empty stub let MSVC
// delete OnMsg's whole compare chain + call (OnMsg 71.75%).
void VocalPlayer::HandleDeactivateVolume(JoypadButton but) {
    VocalParam param;
    if (!GetVolumeParam(but, param)) {
        MILO_WARN(
            "HandleDeactivateVolume: Couldn't get a VocalParam for supposed volume button %d!\n",
            but
        );
    } else if (mTrack) {
        mTrack->GetVocalTrackDir()->DeactivateVolume(param);
    }
}

// Retail fn_826E5FB0, 180 B (unmapped; TU5-only, no surviving source -- the name is from the
// "HandleActivateVolume:" warning string; body from retail bytes, lane W16-JC-6).
// The three mic-gain params address mic (param - Mic1Gain) and enable the widget only
// if a singer is on that mic (fn_826E57B0); the cue volume is enabled by
// AllowPitchCorrection(); everything else is enabled. Then the track's VocalTrackDir
// shows the widget (fn_822F93D0).
void VocalPlayer::HandleActivateVolume(JoypadButton but) {
    VocalParam param;
    if (!GetVolumeParam(but, param)) {
        MILO_WARN(
            "HandleActivateVolume: Couldn't get a VocalParam for supposed volume button %d!\n",
            but
        );
        return;
    }
    int mic;
    bool isMic;
    if (param == kVocalParamMic1Gain || param == kVocalParamMic2Gain
        || param == kVocalParamMic3Gain) {
        mic = param - kVocalParamMic1Gain;
        isMic = true;
    } else {
        mic = -1;
        isMic = false;
    }
    if (mTrack) {
        bool enabled = !isMic || HasSingerOnMic(mic);
        if (param == kVocalParamCueVolume)
            enabled = AllowPitchCorrection();
        mTrack->GetVocalTrackDir()->ActivateVolume(param, mic, enabled);
    }
}

// Retail fn_826E57B0, 80 B (unmapped; the name is ours, lane W16-JC-6): true if any
// singer's mic id (Singer fn_826F6930) is `mic`.
bool VocalPlayer::HasSingerOnMic(int mic) const {
    FOREACH (it, mSingers) {
        if ((*it)->GetMicID() == mic)
            return true;
    }
    return false;
}
#pragma auto_inline(on)

bool VocalPlayer::AllowPitchCorrection() const {
    MILO_ASSERT(TheGameMicManager, 0xA46);
    return TheGameMicManager->GetMicCount() == 1 && mVocalParts.size() == 1;
}

bool VocalPlayer::AllowWarningState() const {
    MILO_ASSERT(TheGameMicManager, 0xA56);
    return !GetUser()->IsNullUser() || TheGameMicManager->GetMicCount() > 0;
}

void VocalPlayer::OnGameOver() {
#ifndef HX_NATIVE
    if (!MetaPerformer::Current()->SongEndsWithEndgameSequence()) {
        TheGameMicManager->SetPlayback(false);
    }
#endif
    mTambourineManager.GameOver();
    FOREACH (it, mVocalParts) {
        (*it)->OnGameOver();
    }
}

#pragma push
#pragma pool_data off
void VocalPlayer::ResetScoring() {
    std::vector<int> phraseActiveParts(3, 0);
    for (int i = 0; i < mVocalParts.size(); i++) {
        VocalPart *cur = mVocalParts[i];
        if (cur->ScoringEnabled()) {
            mVocalParts[i]->ResetScoring();
        }
        if (!mVocalParts[i]->InEmptyPhrase() && mVocalParts[i]->InPlayablePhrase()
            && mVocalParts[i]->ScoringEnabled()) {
            phraseActiveParts[mVocalParts[i]->PartIndex()] = 1;
        }
    }
    mTrack->GetVocalTrackDir()->UpdateVocalMeters(
        phraseActiveParts[0], phraseActiveParts[1], phraseActiveParts[2], true
    );
}
#pragma pop

void VocalPlayer::EnableController() {
    TheGameMicManager->SetPlayback(true);
    FOREACH (it, mSingers) {
        (*it)->EnableController();
    }
    if (mUser->IsLocal()) {
        JoypadKeepAlive(mUser->GetLocalUser()->GetPadNum(), true);
    }
}

void VocalPlayer::DisableController() { OnDisableController(); }

void VocalPlayer::OnDisableController() {
    TheGameMicManager->SetPlayback(false);
    FOREACH (it, mSingers) {
        (*it)->DisableController();
    }
    if (mUser->IsLocal()) {
        JoypadKeepAlive(mUser->GetLocalUser()->GetPadNum(), false);
    }
}

bool VocalPlayer::ShouldDrainEnergy() const { return true; }

bool VocalPlayer::InFreestyleSection() const {
    FOREACH (it, mVocalParts) {
        if ((*it)->InFreestyleSection())
            return true;
    }
    return false;
}

bool VocalPlayer::GetFreestyleDeploymentRequiredMs(float &out) const {
    float minDuration = -1.0f;
    bool found = false;
    FOREACH (it, mVocalParts) {
        VocalPart *part = *it;
        if (part->InFreestyleSection()) {
            float duration = part->GetFreestyleSectionDurationMs();
            if (minDuration == -1.0f || duration < minDuration) {
                minDuration = duration;
            }
            found = true;
        }
    }
    if (!found || minDuration == 0.0f) {
        return false;
    }
    int matchIdx = -1;
    for (int i = 0; i < mFreestyleDeploymentTimes->Size(); i++) {
        if (minDuration >= mFreestyleMinDurations->Float(i)) {
            matchIdx = i;
            break;
        }
    }
    if (matchIdx == -1) {
        return false;
    }
    out = mFreestyleDeploymentTimes->Float(matchIdx);
    return true;
}

void VocalPlayer::ChangeDifficulty(Difficulty diff) {
    Player::ChangeDifficulty(diff);
    TheSongDB->ChangeDifficulty(mTrackNum, diff);
    mStats.SetVocalSingerAndPartCounts(mSingers.size(), mVocalParts.size());
    SetDifficultyVariables(diff);
}

void VocalPlayer::BuildPhrases(bool b1) {
    std::vector<std::pair<const VocalPhrase *, VocalPart *> > vec;
    FOREACH (it, mVocalParts) {
        VocalPart *part = *it;
        vec.push_back(
            std::pair<const VocalPhrase *, VocalPart *>(part->GetFirstPhraseMarker(), part)
        );
    }
    unk36c = 0;
    unk370 = 0;

    bool b3;
    do {
        b3 = true;
        bool bvar1 = false;
        bool bvar2 = false;
        FOREACH (it, vec) {
            const VocalPhrase *curPhrase = it->first;
            VocalPart *curPart = it->second;
            if (!curPart->IsPhraseMarkerAtEnd(curPhrase)) {
                b3 = false;
                if (!curPart->IsEmptyPhrase(curPhrase))
                    bvar1 = true;
                if (curPhrase->mTambourinePhrase)
                    bvar2 = true;
                it->first = curPart->GetNextPhraseMarker(curPhrase);
            }
        }
        if (bvar1)
            unk36c++;
        if (bvar2)
            unk370++;
    } while (!b3);
    mTambourineManager.PostLoad();
    if (!b1 && NeedsToOverrideBasePoints()) {
        TheSongDB->OverrideBasePoints(
            mTrackNum,
            mTrackType,
            GetUserGuid(),
            GetBaseMaxPoints(),
            GetBaseMaxStreakPoints(),
            GetBaseBonusPoints()
        );
    }
}

bool VocalPlayer::RebuildPhrases() {
    BuildPhrases(false);
    return Player::RebuildPhrases();
}

void VocalPlayer::Rollback(float f1, float f2) {
    FOREACH (it, mVocalParts) {
        (*it)->Rollback(f1, f2);
    }
    FOREACH (it, mSingers) {
        (*it)->Rollback(f1, f2);
    }
    mTambourineManager.Rollback(f1, f2);
    Player::Rollback(f1, f2);
}

bool VocalPlayer::DoneWithSong() const {
    return mQuarantined || GetUser()->IsNullUser() || PastFinalNote();
}

FORCE_LOCAL_INLINE
bool VocalPlayer::ScoringEnabled() const { return mScoringEnabled && !InRollback(); }
END_FORCE_LOCAL_INLINE

const VocalPhrase *const &VocalPlayer::CurrentPhrase() const {
    return mVocalParts.front()->mThisPhrase;
}


void VocalPlayer::HitCoda() {
    if (IsLocal()) {
        LocalHitCoda();
        static Message msg("send_hit_last_coda_gem", 0);
        HandleType(msg);
    }
}

DECOMP_FORCEACTIVE(
    VocalPlayer,
    "**** NEW SONG MIN PITCH: %2.0f (was %2.0f)\n",
    "**** NEW SONG MAX PITCH: %2.0f (was %2.0f)\n"
)

float VocalPlayer::FramePhraseMeterFrac(int idx) const {
    if (idx >= mVocalParts.size())
        return 0;
    else
        return mVocalParts[idx]->FramePhraseMeterFrac();
}

float VocalPlayer::FrameOverallPhraseMeterFrac() const {
    float ret = 0;
    for (int i = 0; i < mVocalParts.size(); i++) {
        float frac = FramePhraseMeterFrac(i);
        if (frac > ret) {
            ret = frac;
        }
    }
    return ret;
}

int VocalPlayer::CalculatePhraseRating(float f1) {
    for (int i = mRatingThresholds->Size() - 1; i >= 1; i--) {
        if (f1 > mRatingThresholds->Float(i)) {
            return i;
        }
    }
    return 0;
}

int VocalPlayer::PhraseScore() const { return mVocalParts.front()->mPhraseScore; }
bool VocalPlayer::IgnorePhrase() const { return false; }

bool VocalPlayer::PressingToTalk() {
    if (!IsLocal())
        return false;
    BandUser *u = GetUser();
#ifdef HX_NATIVE
    // The headless vocal drivers (native rb3-vocal2 / rb3-harmony) build the
    // player through the native scoring-core ctor with NO BandUser. They used to
    // be shielded by a `!TheNetSession->IsLocal()` gate in Poll; f3ec9592d made
    // Poll call this unconditionally (retail shape), and both targets have
    // segfaulted here on their first frame since. No user => no pad => not
    // pressing. Native-only: the X360 build never defines HX_NATIVE.
    if (!u)
        return false;
#endif
    if (!u->IsLocal())
        return false;
    else {
        LocalBandUser *lu = u->GetLocalBandUser();
        if (!UserHasController(lu))
            return false;
        else {
            return JoypadGetPadData(lu->GetPadNum())->IsButtonInMask(0);
        }
    }
}

// Chat is only possible in a networked session: a local session never asks
// the pad. Out of line in retail (0x826E3E90, right after PressingToTalk at
// 0x826E3DB0); Restart, Poll and the track-restart path all call it.
bool VocalPlayer::CanChat() { return !TheNetSession->IsLocal() && PressingToTalk(); }

void VocalPlayer::LocalHitCoda() {
    mBand->LocalFinishedCoda(this);
    mTrack->HideCoda();
    Deploy();
}

void VocalPlayer::LocalBlowCoda() { mBand->LocalBlowCoda(this); }

float VocalPlayer::GetHitPercentage(int p) {
    MILO_ASSERT(p < NumVocalParts(), 0xD18);
    VocalPart *part = mVocalParts[p];
    return part->GetOverallPartHitPercentage();
}

float VocalPlayer::GetPracticeHitPercentage(int p, int i2, int i3) {
    MILO_ASSERT(p < NumVocalParts(), 0xD20);
    std::vector<VocalPhrase> phrases;
    mVocalParts.front()->mVocalNoteList->GetPracticePhrases(phrases, i2, i3);
    if (p == -1) {
        float f7 = 0;
        VocalPart *cur = 0;
        for (int i = 0; i < (int)mVocalParts.size(); i++) {
            cur = mVocalParts[i];
            if (cur->ScoringEnabled()) {
                float pct = cur->GetPartHitPercentage(phrases, i2, i3);
                if (pct > f7)
                    f7 = pct;
            }
        }
        return f7;
    } else
        return mVocalParts[p]->GetPartHitPercentage(phrases, i2, i3);
}

float VocalPlayer::GetNumPhrases(int startTick, int endTick, int isolatedPart) {
    std::vector<VocalPhrase> phraseVec;
    mVocalParts[0]->mVocalNoteList->GetPracticePhrases2(phraseVec, startTick, endTick);
    int startPart = 0;
    int endPart = 0;
    if (isolatedPart < 0) {
        endPart = 2;
    } else if (isolatedPart != 0) {
        endPart = startPart = isolatedPart;
    }
    int count = 0;
    for (unsigned int i = 0; i < phraseVec.size(); i++) {
        int clampedStart = Max(phraseVec[i].unk8, startTick);
        int clampedEnd = Min(phraseVec[i].unk8 + phraseVec[i].unkc, endTick);
        bool found = false;
        for (int part = startPart; part <= endPart && !found; part++) {
            if (part == 0) {
                VocalPhrase &phrase = phraseVec[i];
                if (phrase.unk10 != phrase.unk14) {
                    count++;
                    found = true;
                }
            } else {
                VocalNoteList *vnl = TheSongDB->GetVocalNoteList(part);
                if (vnl && vnl->HasNoteInRange(clampedStart, clampedEnd) != -1) {
                    count++;
                    found = true;
                }
            }
        }
    }
    return (float)count;
}

float VocalPlayer::GetBestPercentage(int p) {
    MILO_ASSERT(p < NumVocalParts(), 0xD79);
    if (!ScoringEnabled())
        return 0;
    else if (p == -1) {
        float f6 = 0;
        for (int i = 0; i < NumVocalParts(); i++) {
            VocalPart *cur = mVocalParts[i];
            if (cur->ScoringEnabled()) {
                if (cur->GetOverallPartHitPercentage() > f6) {
                    f6 = cur->GetOverallPartHitPercentage();
                }
            }
        }
        return f6;
    } else
        return GetHitPercentage(p);
}

void VocalPlayer::UpdateSectionStats() {
    float f1;
    int diff = mPhrasePercentageCount - mSectionStartPhrasePercentageCount;
    float phraseDelta = mPhrasePercentageTotal - mSectionStartPhrasePercentageTotal;
    float scoreDelta = mScore - mSectionStartScore;
    if (diff > 0) {
        f1 = phraseDelta / (float)diff;
    } else {
        f1 = -1;
    }
    Player::UpdateSectionStats(f1, scoreDelta);
}

void VocalPlayer::HandleNewSection(const PracticeSection &s, int i1, int i2) {
    if (ScoringEnabled()) {
        mSectionStartPhrasePercentageCount = mPhrasePercentageCount;
        mSectionStartPhrasePercentageTotal = mPhrasePercentageTotal;
        mSectionStartScore = mScore;
        Player::HandleNewSection(s, i1, i2);
    }
}

void VocalPlayer::UpdateVocalStyle() {
    if (!mTrack || mGameOver)
        return;
    else {
        mTrack->SetVocalStyle(mUser->GetGameplayOptions()->GetVocalStyle());
    }
}

bool VocalPlayer::Freestyling() const {
    bool insection = InFreestyleSection();
    return mIsInCoda || IsDeployingBandEnergy() && insection;
}

bool VocalPlayer::CanDeployCoda() const {
    return mBand->NumNonQuarantinedPlayers() > 1 && !mQuarantined;
}

bool VocalPlayer::AutoplaysCoda() const { return mAutoPlay && !IsNet(); }
bool VocalPlayer::NeedsToOverrideBasePoints() const { return !mUser->IsNullUser(); }

int VocalPlayer::GetBaseMaxPoints() const {
    return mTambourineManager.unk78 + mPhraseValue * (unk36c - unk370);
}

int VocalPlayer::GetBaseMaxStreakPoints() const {
    int n = unk36c - unk370;
    int mult;
    switch (n) {
    case 0: mult = 0; break;
    case 1: mult = 1; break;
    case 2: mult = 3; break;
    default: mult = n * 4 - 6; break;
    }
    return mPhraseValue * mult + mTambourineManager.unk78;
}

int VocalPlayer::GetBaseBonusPoints() const {
    return mTambourineManager.unk7c;
}

bool Singer::HasAssignedPart() const {
    return mFrameAssignedPart != -1;
}

bool VocalPlayer::ShowPitchCorrectionNotice() const {
#ifdef HX_NATIVE
    // Off-path (UI notice); headless has no TheProfileMgr object.
    return false;
#else
    return AllowPitchCorrection() && TheProfileMgr.mSynapseEnabled;
#endif
}

const VocalPhrase *VocalPlayer::GetNextPhraseMarker(const VocalPhrase *const &p) const {
    return mVocalParts.front()->GetNextPhraseMarker(p);
}

bool VocalPlayer::AtFirstPhrase() const {
    VocalPart *vp = mVocalParts.front();
    // begin(), NOT end(). The old comment here read retail's _M_finish load off
    // 0x826e5ae8 and concluded this function is end() -- but the map had this
    // name TRANSPOSED with PastFinalNote (see W40-ACCURACY / scripts/
    // target_symbol_map.json). AtFirstPhrase is 0x826e5688, whose one
    // distinguishing instruction is `lwz r10, 0(r10)` = mPhrases._M_start.
    // This is a REAL behavioural fix, not a codegen one: VocalTrack.cpp:746
    // takes the else branch to hunt for the PREVIOUS phrase, which only makes
    // sense when we are not at the first one.
    return vp->mThisPhrase == vp->mVocalNoteList->mPhrases.begin();
}

bool VocalPlayer::AtLastPhrase() const {
    VocalPart *vp = mVocalParts.front();
    // end(), not data()+size(). Identical VALUE, but data()+size() compiles to a
    // subtract/shift sequence instead of retail's single `lwz r10, 4(r10)`
    // (_M_finish). PastFinalNote forwards here and is retail's 0x826e5ae8.
    return vp->mThisPhrase == vp->mVocalNoteList->mPhrases.end();
}

void VocalPlayer::ToggleOverlay() {
    MILO_ASSERT(mOverlay, 0xDF0);
    mOverlay->SetShowing(!mOverlay->Showing());
    RELEASE(mVocalOverlay);
    if (mOverlay->Showing())
        mVocalOverlay = new VocalOverlay();
}

// Retail inherits Callback's `return y` (a bare blr) at this slot: the vocal
// debug overlay is native-only (lane W16-OT, BODY check).
float VocalPlayer::UpdateOverlay(RndOverlay *o, float f2) {
#ifdef HX_NATIVE
    o->Clear();
    if (mVocalOverlay) {
        o->Print(mVocalOverlay->mDisplayedString.c_str());
    }
#endif
    return f2;
}

bool VocalPlayer::SongSectionOnly(float &f1, float &f2) const {
    // Retail (TU5) constructs a function-local static Symbol here that is never
    // read again: guard lbl_82E03500, storage lbl_82E034FC, string
    // "has_song_sections" (lbl_820DCB18), plus the guard-clear thunk
    // fn_826E42C0 after the function. DC3 does not have it. Its
    // register pressure is also why retail uses r31 as a frame pointer here.
    static Symbol has_song_sections("has_song_sections");
    if (!TheGame->mProperties.mHasSongSections) {
        return false;
    } else {
        int i20, i24;
        TheGameConfig->GetPracticeSections(i20, i24);
        float f28;
        TheGameConfig->GetSectionBounds(i20, f1, f28);
        TheGameConfig->GetSectionBounds(i24, f28, f2);
        TheGame->AdjustForVocalPhrases(f1, f2);
        return true;
    }
}

bool VocalPlayer::ToggleFrameSpew() {
    // DC3-only debug feature; retail rb3-xenon has no frame-spew members.
#ifdef HX_NATIVE
    if (mFrameSpewData) {
        MILO_ASSERT(mFrameSpewStream, 0xE28);
        RELEASE(mFrameSpewData);
        RELEASE(mFrameSpewStream);
        return false;
    } else {
        MILO_ASSERT(mFrameSpewStream == NULL, 0xE38);
        mFrameSpewData = new VocalFrameSpewData(mSingers.size(), mVocalParts.size());
        TextFileStream *s = new TextFileStream("vocals.frame_data.txt", false);
        mFrameSpewStream = s;
        mFrameSpewData->OutputHeader(*s);
        return true;
    }
#else
    return false;
#endif
}

void VocalPlayer::EnablePartScoring(int part, bool b2) {
    MILO_ASSERT(mVocalParts.size() > part, 0xE4D);
    mVocalParts[part]->EnableScoring(b2);
}

void VocalPlayer::SwapAmbiguousPoints(float f1, int i2, int i3) {
    mVocalParts[i2]->ForcePhrasePointDelta(-f1);
    mVocalParts[i3]->ForcePhrasePointDelta(f1);
}

void VocalPlayer::ClearScoreHistories() {
    FOREACH (it, mSingers) {
        (*it)->ClearScoreHistories();
    }
}

void VocalPlayer::AddAccuracyStat(int i) { mStats.AddAccuracy(i); }
void VocalPlayer::AddScoreStreakStat(float f) { mStats.AddScoreStreak(f); }
void VocalPlayer::AddBandContributionStat(float f) { mStats.AddBandContribution(f); }
void VocalPlayer::AddOverdriveStat(float f) { mStats.AddOverdrive(f); }
void VocalPlayer::AddTambourinePointsStat(float f) { mStats.AddTambourine(f); }
void VocalPlayer::AddHarmonyStat(int i) { mStats.AddHarmony(i); }
void VocalPlayer::AddTambourineSeen() { mStats.AddTambourineSeen(); }
void VocalPlayer::AddTambourineHit() { mStats.AddTambourineHit(); }
void VocalPlayer::EndTambourineSection(int i) { mStats.UpdateBestTambourineSection(i); }

// REFUTED (laneAX-2, 2026-07-27): this TU used to #undef BEGIN_HANDLERS to put a
// MessageTimer back into VocalPlayer::Handle, on funclet evidence. The premise was
// built while ?Handle@VocalPlayer@@ was MISPAIRED (the map had 0x826e7ca0 =
// ?Poll@..., so our Handle was unpaired and never measured). With the map rotation
// fixed, the real target Handle (fn_826E7CA0) makes NO MessageTimer::Active /
// Timer::Restart / AddTime call at all, and our timer'd body read frame Δ +0x30
// with 247 target-missing instructions. Retail RB3 drops the per-handler timer,
// exactly as Object.h's default (MILO_MESSAGE_TIMERS undefined) already assumes.

#pragma push
#pragma dont_inline on
BEGIN_HANDLERS(VocalPlayer)
    HANDLE_EXPR(in_star_mode, 0)
    HANDLE_EXPR(score, GetScore())
    HANDLE_EXPR(percent_hit, 0)
    HANDLE_EXPR(toggle_overlay, 0)
#ifdef HX_NATIVE
    HANDLE_EXPR(toggle_frame_spew, ToggleFrameSpew())
#endif
    HANDLE_EXPR(num_stars, GetNumStars())
    HANDLE_EXPR(star_rating, GetStarRating())
    HANDLE_ACTION(set_spoofed, mSpoofed = _msg->Int(2))
    HANDLE_EXPR(auto_play, IsAutoplay())
    HANDLE_ACTION(set_auto_play, SetAutoplay(_msg->Int(2)))
    HANDLE_ACTION(rotate_singer_autoplay_part, RotateSingerAutoplayPart(_msg->Int(2)))
    HANDLE_EXPR(get_singer_autoplay_part, GetSingerAutoplayPart(_msg->Int(2)))
    HANDLE_ACTION(
        rotate_singer_autoplay_variation_magnitude,
        RotateSingerAutoplayVariationMagnitude(_msg->Int(2))
    )
    HANDLE_EXPR(
        get_singer_autoplay_variation_magnitude,
        GetSingerAutoplayVariationMagnitude(_msg->Int(2))
    )
    HANDLE_ACTION(set_autoplay_offset, SetAutoplayOffset(_msg->Float(2)))
    HANDLE_EXPR(get_autoplay_offset, GetAutoplayOffset())
    HANDLE_ACTION(set_vocal_part_bias, SetVocalPartBias(_msg->Float(2)))
    HANDLE_EXPR(get_vocal_part_bias, GetVocalPartBias())
    HANDLE_EXPR(get_hit_percentage, GetHitPercentage(_msg->Int(2)))
    HANDLE_EXPR(get_best_percentage, GetBestPercentage(_msg->Int(2)))
    HANDLE_EXPR(
        get_practice_hit_percentage,
        GetPracticeHitPercentage(_msg->Int(2), _msg->Int(3), _msg->Int(4))
    )
    HANDLE_EXPR(get_num_phrases, GetNumPhrases(_msg->Int(2), _msg->Int(3), _msg->Int(4)))
    HANDLE_ACTION(
        remote_vocal_state, RemoteVocalState(_msg->Int(2), _msg->Int(3), _msg->Int(4))
    )
    HANDLE_ACTION(remote_phrase_over, unk2fc = 0)
    HANDLE_ACTION(remote_hit, SetAutoplay(true))
    HANDLE_ACTION(remote_penalize, SetAutoplay(false))
    HANDLE_ACTION(
        remote_score_phrase, RemoteScorePhrase(_msg->Int(2), _msg->Int(3), _msg->Int(4))
    )
    HANDLE_ACTION(remote_hit_last_coda_gem, LocalHitCoda())
    HANDLE_ACTION(remote_blow_coda, LocalBlowCoda())
    HANDLE_ACTION(remote_vocal_energy, LocalEndgameEnergy(_msg->Int(2)))
    HANDLE_ACTION(on_new_track, HookupTrack())
    HANDLE_ACTION(on_game_over, OnGameOver())
    HANDLE_ACTION(disable_controller, OnDisableController())
    HANDLE_ACTION(enable_part_scoring, EnablePartScoring(_msg->Int(2), _msg->Int(3)))
    HANDLE_ACTION(reset_scoring, ResetScoring())
    HANDLE_EXPR(fill_star_power, 0)
    HANDLE_EXPR(empty_star_power, 0)
    HANDLE_EXPR(set_star_power, 0)
    HANDLE_EXPR(set_star_power_deploy_rate, 0)
    HANDLE_EXPR(set_star_power_phrase_boost, 0)
    HANDLE_EXPR(on_downbeat, 0)
    HANDLE_EXPR(set_auto_solo, 0)
    HANDLE_EXPR(toggle_solo_quantize, 0)
    HANDLE_EXPR(on_start_starpower, 0)
    HANDLE_EXPR(on_stop_starpower, 0)
    HANDLE_EXPR(refresh_track_buttons, 0)
    HANDLE(midi_parser, OnMidiParser)
    HANDLE_MESSAGE(GameMicsChangedMsg)
    HANDLE_MESSAGE(ButtonDownMsg)
    HANDLE_MESSAGE(ButtonUpMsg)
    HANDLE_MEMBER_PTR((&mTambourineManager))
    HANDLE_SUPERCLASS(Player)
    HANDLE_CHECK(0xF30)
END_HANDLERS
#pragma pop
