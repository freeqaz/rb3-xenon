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
    BandSongMetadata *data = (BandSongMetadata *)TheSongMgr.Data(
        TheSongMgr.GetSongIDFromShortName(MetaPerformer::Current()->Song(), true)
    );
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

void VocalPlayer::SetTrack(int trk) {
    if (mTrackNum != trk) {
        MILO_ASSERT(mTrackNum == -1, 0x128);
        mBeatMaster->GetAudio()->SetTrack(GetUserGuid(), trk);
        mTrackNum = trk;
        bool b1 = false;
        if (IsNet() || mSpoofed)
            b1 = true;
        if (b1) {
            mBeatMaster->GetAudio()->SetNonmutable(trk);
        }
    }
}

void VocalPlayer::PostLoad(bool b1) {
    ObjectDir::Main()->Find<MidiParser>("play_tambourine", true)->AddSink(this);
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
        track->ShowOverdriveMeter(
            !mUser->IsNullUser() && TheGame->AllowOverdrivePhrases()
        );
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
            mCouldChat = !TheNetSession->IsLocal() && PressingToTalk();
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
        if (mTrack)
            mTrack->RebuildHUD();
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
    } else if ((unsigned)(state - kPlayerBeingSaved) <= 1U) {
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
    bool vocalState = ((unsigned)state <= (unsigned)kPlayerDisconnected) &&
                      ((1 << state) & 0x19) != 0;
    mBeatMaster->GetAudio()->SetVocalState(vocalState);
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
    bool notNet = !IsNet();
    if (notNet) {
        float multiplier = 1.0f;
        float ratingFraction = (float)rating * 0.25f;
        if (ratingFraction > mCrowd->mRawValue) {
            multiplier = GetCrowdBoost();
        }
        mCrowd->UpdatePhrase(ratingFraction, multiplier);
        CheckCrowdFailure();
        HandleType(send_update_crowd_msg);
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

    // Reset vocal overlay
    if (mVocalOverlay) {
        mVocalOverlay->Reset((int)mSingers.size());
    }

    // Declaration order matches target register allocation
    float frameMinPitch = 9985.578125f;
    float frameMaxPitch = 0.0f;
    bool bWasInFreestyleSection = InFreestyleSection();

    // scoredPartIndices initialized before SongSectionOnly, reserve mVocalParts.size()
    std::vector<int> scoredPartIndices;

    float var_f24 = 0.0f;
    int iMaximumFreestyleDeploymentSinger = -1;
    scoredPartIndices.reserve(mVocalParts.size());

    // Determine section scoring state
    float fSectionBeginMs = 0.0f;
    float fSectionEndMs = 0.0f;
    bool bInSection = false;
    if (SongSectionOnly(fSectionBeginMs, fSectionEndMs)) {
        if (fCompMS > (100.0f + fSectionBeginMs) && fCompMS < (100.0f + fSectionEndMs)) {
            bInSection = true;
        }
        mScoringEnabled = bInSection;
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

        int dimState = pPart->unk98;
        bSomePitched = bSomePitched | (dimState == 0);
        bSomeUnpitched = bSomeUnpitched | ((dimState - 1) == 0);

        VocalNote *pNote = (VocalNote *)pPart->mVocalNoteList->NoteAt(fCompMS);
        if (pNote && pNote->mUnpitchedNote && !pUnpitchedPart) {
            pUnpitchedPart = pPart;
        }

        bool bCanScore = false;
        if (mScoringEnabled && !InRollback()) {
            bCanScore = true;
        }

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
        mTuningOffset = 0.00109659603f * pitchOffset;
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

        if (mVocalOverlay) {
            mVocalOverlay->AppendSingerPitch(pSinger->mSingerIndex, pSinger->mFrameMicPitch);
        }
        singersArray.push_back(pSinger);

        // Score singer against each part
        float var_f22 = 1000.0f;
        FOREACH (pIt, mVocalParts) {
            VocalPart *pPart = *pIt;
            bool bScoringAllowed = false;
            if (mScoringEnabled && !InRollback()) {
                bScoringAllowed = true;
            }

            if (bScoringAllowed && pPart->ScoringEnabled()) {
                VocalScoreCache &cache = pSinger->AccessScoreCache(pPart->mPartIndex);
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
                if (1000.0f != fDev && fabs(fDev) < fabsf(var_f22)) {
                    var_f22 = fDev;
                }

                float fScore = cache.unk0;
                if (cache.unk20) {
                    fScore *= mNonpitchStickiness;
                }
                pSinger->AppendToScoreHistory(fCompMS, pPart->mPartIndex, fScore, iRating);
            }
        }
        if (1000.0f != var_f22) {
            pSinger->UpdatePitchDeviation(var_f22);
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
                if (mVocalOverlay) {
                    mVocalOverlay->AddPossiblePart(pSinger->mSingerIndex, pBestPart);
                }
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

            // Inlined STL std::remove_if Duff's device: unrolled 4-at-a-time
            // find_if + remove_copy_if + erase. MWCC does not inline the
            // generic std::find_if wrapper, so hand-roll the body to match
            // target's inlined codegen (see Stats.cpp:520 for the same trick).
            {
                VocalPart **partFirst = &partsArray[0];
                VocalPart **partLast = partFirst + partsArray.size();
                std::mem_fun_t<bool, VocalPart> partPred(&VocalPart::HasBestSingerCandidate);
                std::mem_fun_t<bool, VocalPart> partFindPred(partPred);
                VocalPart **partFound;
                int partTrip = (int)(partLast - partFirst) >> 2;
                for (; partTrip > 0; --partTrip) {
                    if (partFindPred(*partFirst)) { partFound = partFirst; goto partRemoved; }
                    ++partFirst;
                    if (partFindPred(*partFirst)) { partFound = partFirst; goto partRemoved; }
                    ++partFirst;
                    if (partFindPred(*partFirst)) { partFound = partFirst; goto partRemoved; }
                    ++partFirst;
                    if (partFindPred(*partFirst)) { partFound = partFirst; goto partRemoved; }
                    ++partFirst;
                }
                switch (partLast - partFirst) {
                case 3:
                    if (partFindPred(*partFirst)) { partFound = partFirst; goto partRemoved; }
                    ++partFirst;
                case 2:
                    if (partFindPred(*partFirst)) { partFound = partFirst; goto partRemoved; }
                    ++partFirst;
                case 1:
                    if (partFindPred(*partFirst)) { partFound = partFirst; goto partRemoved; }
                case 0:
                default:
                    partFound = partLast;
                }
            partRemoved:
                if (partFound != partLast) {
                    VocalPart **partSrc = partFound + 1;
                    std::mem_fun_t<bool, VocalPart> partCopyPred(partPred);
                    for (; partSrc != partLast; ++partSrc) {
                        if (!partCopyPred(*partSrc)) {
                            *partFound = *partSrc;
                            ++partFound;
                        }
                    }
                }
#ifdef HX_NATIVE
                // libstdc++'s vector::iterator is not a raw T**, so erase()
                // can't accept the raw pointers we threaded through the Duff's
                // device above. Convert the pointer range back to iterators by
                // offsetting from begin().
                {
                    ptrdiff_t _foundOff = partFound - &partsArray[0];
                    partsArray.erase(partsArray.begin() + _foundOff, partsArray.end());
                }
#else
                partsArray.erase(partFound, partLast);
#endif
            }
            {
                Singer **singerFirst = &singersArray[0];
                Singer **singerLast = singerFirst + singersArray.size();
                std::const_mem_fun_t<bool, Singer> singerPred(&Singer::HasAssignedPart);
                std::const_mem_fun_t<bool, Singer> singerFindPred(singerPred);
                Singer **singerFound;
                int singerTrip = (int)(singerLast - singerFirst) >> 2;
                for (; singerTrip > 0; --singerTrip) {
                    if (singerFindPred(*singerFirst)) { singerFound = singerFirst; goto singerRemoved; }
                    ++singerFirst;
                    if (singerFindPred(*singerFirst)) { singerFound = singerFirst; goto singerRemoved; }
                    ++singerFirst;
                    if (singerFindPred(*singerFirst)) { singerFound = singerFirst; goto singerRemoved; }
                    ++singerFirst;
                    if (singerFindPred(*singerFirst)) { singerFound = singerFirst; goto singerRemoved; }
                    ++singerFirst;
                }
                switch (singerLast - singerFirst) {
                case 3:
                    if (singerFindPred(*singerFirst)) { singerFound = singerFirst; goto singerRemoved; }
                    ++singerFirst;
                case 2:
                    if (singerFindPred(*singerFirst)) { singerFound = singerFirst; goto singerRemoved; }
                    ++singerFirst;
                case 1:
                    if (singerFindPred(*singerFirst)) { singerFound = singerFirst; goto singerRemoved; }
                case 0:
                default:
                    singerFound = singerLast;
                }
            singerRemoved:
                if (singerFound != singerLast) {
                    Singer **singerSrc = singerFound + 1;
                    std::const_mem_fun_t<bool, Singer> singerCopyPred(singerPred);
                    for (; singerSrc != singerLast; ++singerSrc) {
                        if (!singerCopyPred(*singerSrc)) {
                            *singerFound = *singerSrc;
                            ++singerFound;
                        }
                    }
                }
#ifdef HX_NATIVE
                {
                    ptrdiff_t _foundOff = singerFound - &singersArray[0];
                    singersArray.erase(singersArray.begin() + _foundOff, singersArray.end());
                }
#else
                singersArray.erase(singerFound, singerLast);
#endif
            }
        } while (!partsArray.empty() && !singersArray.empty());
    }

    if (mVocalOverlay) {
        mVocalOverlay->EqualizeSingerStrings();
    }

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
        float fFramePitch = pSinger->mFrameMicPitch;

        if (0.0f != pSinger->mBestTargetPitch) {
            pSinger->ClearFreestyleDeployment();
        }

        if (pSinger->mFrameAssignedPart != -1) {
            VocalPart *pPart = mVocalParts[pSinger->mFrameAssignedPart];
            VocalScoreCache &cache = pSinger->AccessScoreCache(pPart->mPartIndex);
            pPart->AddScore(cache);
            pSinger->mFrameBestHitScore = cache.unk0;

#ifdef HX_NATIVE
            if (mFrameSpewData) {
                mFrameSpewData->mSingerData[pSinger->mSingerIndex].unk8 =
                    (int)pSinger->mFrameAssignedPart;
            }
#endif

            if (0.0f != pSinger->mFrameMicPitch) {
                iOctaveOffset =
                    pSinger->AccessScoreHistory(pPart->mPartIndex).GetOctaveOffset();
            }
        } else {
            float fOld = pSinger->mFrameMicPitch;
            iOctaveOffset = pSinger->mOctaveOffset;
            if (0.0f != fOld) {
                float fPrev = pSinger->mBestTargetPitch;
                if (0.0f != fPrev) {
                    float diff = fPrev - fOld;
                    float absDiff = fabsf(diff);
                    float mod = (float)fmod(absDiff, 12.0);
                    float alt = kSemitone - mod;
                    if (mod >= alt) {
                        mod = alt;
                    }
                    if (mod <= kMaxOctaveDistance) {
                        int octaveAdjust = (int)(kHalfSemitone + absDiff / kSemitone);
                        int sign;
                        if (diff > 0.0f) {
                            sign = 1;
                        } else {
                            sign = -1;
                        }
                        iOctaveOffset = octaveAdjust * sign;
                    }
                }
            }
            pSinger->mFrameBestHitScore = 0.0f;
            pSinger->mFrameTargetPitch = 0.0f;

            bool bNotNet = !IsNet();
            if (bNotNet && InFreestyleSection()) {
                float fDeployAmt = pSinger->AddToFreestyleDeployment(fCompMS);
                if (fDeployAmt > var_f24) {
                    var_f24 = fDeployAmt;
                    iMaximumFreestyleDeploymentSinger = pSinger->mSingerIndex;
                }
            }
        }

        if (0.0f != pSinger->mFrameMicPitch) {
            float fAdjusted = (kSemitone * (float)iOctaveOffset) + pSinger->mFrameMicPitch;
            VocalTrack *pTrack = mTrack;
            MILO_ASSERT(pTrack, 0x501);
            float fBottom = pTrack->GetBottomDisplayPitch() - mTrackWrappingMargin;
            float fTop = mTrackWrappingMargin + pTrack->GetTopDisplayPitch();
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

        if (0.0f != pSinger->mFrameMicPitch) {
            pSinger->SetFrameMicPitch(
                (kSemitone * (float)iOctaveOffset) + pSinger->mFrameMicPitch
            );
        }
        pSinger->SetOctaveOffset(iOctaveOffset);

        if (mVocalOverlay) {
            mVocalOverlay->AppendAssignedPart(pSinger, mVocalParts);
        }

        pSinger->UpdatePitchHistory(fFramePitch);

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
    }

    if (mVocalOverlay) {
        mVocalOverlay->AppendDeploymentMarker(mLastDeploymentSinger);
    }

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

    bool bNotNet = !IsNet();
    if (bNotNet) {
        SendVocalState(fCompMS);
    }

    if (mVocalParts[0]->AtPhraseEnd(fCompMS)) {
        HandlePhraseEnd(fCompMS);
    }

    float fRequiredMs;
    if (GetFreestyleDeploymentRequiredMs(fRequiredMs) && var_f24 > fRequiredMs) {
        DeployBandEnergyIfPossible(false);
        mLastDeploymentSinger = iMaximumFreestyleDeploymentSinger;
    }

    if (mVocalOverlay) {
        mVocalOverlay->AppendPartData(mVocalParts);
    }
    if (mVocalOverlay) {
        mVocalOverlay->AppendPhraseMeter(FrameOverallPhraseMeterFrac());
    }
    if (mVocalOverlay) {
        mVocalOverlay->FinalizeDisplayString();
    }

    // Chat handling
    bool bCanChat = false;
    if (!TheNetSession->IsLocal() && PressingToTalk()) {
        bCanChat = true;
    }
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

    bool bSolo = ((int)mVocalParts.size() - 1) == 0;
    if (TheGameMicManager->GetMicCount() == 1 && bSolo) {
        Singer *pFirstSinger = mSingers[0];
        float fHitPct = pFirstSinger->AccessScoreCache(0).unk14;
        float fAdjusted = fHitPct + (mTuningOffset / 100.0f);
        if (0.0f == fHitPct) {
            fAdjusted = 0.0f;
        }
        bool bEnable = true;
        bool bAdjustedNonzero = true;
        if (0.0f == fAdjusted) {
            bAdjustedNonzero = false;
        }
        if (!bAdjustedNonzero) {
            bEnable = false;
        }
        TheGameMicManager->SetPitchCorrectionTarget(
            bEnable, false,
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
        if (pPart->CouldScoreAgainstPart(
                i_fTime,
                i_pSinger->mTalkyMatcher,
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
    if (TheWorld) {
        if (x == 0) {
            TheWorld->Handle(endgame_vocals_none_msg, false);
        } else if (x == 1) {
            TheWorld->Handle(endgame_vocals_low_msg, false);
        } else if (x == 2) {
            TheWorld->Handle(endgame_vocals_medium_msg, false);
        } else
            TheWorld->Handle(endgame_vocals_high_msg, false);
    }
}

void VocalPlayer::SendCanChat(bool b1) {
    if (mTrack) {
        mTrack->GetVocalTrackDir()->CanChat(b1);
    }
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
        float f10 = cur->GetFrameMicPitch();
        float f9 = Clamp<float>(
            -1, 1, (f10 - cur->mFrameTargetPitch) / mMaxDetune
        );
        moreFsToPack[cur->GetSingerIndex()] = f9;
        boolsToPack[cur->GetSingerIndex()] = f10 != 0;
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
    o_rFractionArray[0] = (float)(int)(unsigned char)i1 / 255.0f * fDifference + f1;
    i1 >>= 8;
    o_rFractionArray[1] = (float)(int)(unsigned char)i1 / 255.0f * fDifference + f1;
    i1 >>= 8;
    o_rFractionArray[2] = (float)(int)(unsigned char)i1 / 255.0f * fDifference + f1;
    i1 >>= 8;
    o_rFractionArray[3] = (float)(int)(unsigned char)i1 / 255.0f * fDifference + f1;
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

void VocalPlayer::HandlePhraseEnd(float f1) {
    std::vector<VocalPart *> voxParts = mVocalParts;
    std::sort(voxParts.begin(), voxParts.end(), VocalPart::FramePhraseMeterFracSorter);
    for (int i = 0.0f; voxParts.size() > i; i++) {
        VocalPart *cur = voxParts[i];
        cur->SetPhraseScoreMultiplier(mPartScoreMultipliers->Float(i + 1));
        cur->SetPhraseRank(i);
    }
    int i4 = GetSpotlightPhraseID();
    float fc4 = -1.0f;
    int ic8 = -1;
    int i15 = 0;
    int i14 = mPhraseActivePartCount;
    mPhraseActivePartCount = 0;
    int id8;
    float fcc;
    float fd0;
    int idc;
    std::vector<float> vec58;
    std::vector<int> vec60(3);
    FOREACH (it, mVocalParts) {
        VocalPart *cur = *it;
        vec58.push_back(cur->MaxPhraseScore());
        float fd4 = FramePhraseMeterFrac(cur->PartIndex());
        if (!cur->InEmptyPhrase() && cur->InPlayablePhrase()
            && !cur->mThisPhrase->mTambourinePhrase && cur->ScoringEnabled()
            && cur->ScoringEnabled()) {
            MaxEq(fc4, fd4);
        }

        cur->HandlePhraseEnd(id8, fcc, fd0, idc, f1);
        if (!cur->InEmptyPhrase() && cur->InPlayablePhrase() && cur->ScoringEnabled()) {
            vec60[cur->PartIndex()] = 1;
            mPhraseActivePartCount++;
        }
        if (id8 > ic8 && ScoringEnabled() && cur->ScoringEnabled()) {
            ic8 = id8;
        }
        if (id8 >= 4)
            i15++;
        mStats.SetVocalPartPercentage(
            cur->PartIndex(), cur->GetOverallPartHitPercentage()
        );
    }
    if (fc4 >= 0 && ScoringEnabled()) {
        mPhrasePercentageCount++;
        mPhrasePercentageTotal += fc4;
    }
    FOREACH (it, mSingers) {
        Singer *cur = *it;
        cur->HandlePhraseEnd(f1, vec58);
        FOREACH (it2, mVocalParts) {
            VocalPart *curPart = *it2;
            mStats.SetSingerPartPercentage(
                cur->GetSingerIndex(),
                curPart->PartIndex(),
                cur->GetPartPercentage(curPart->PartIndex())
            );
        }
        float fe0, fe4;
        cur->GetPitchDeviation(fe0, fe4);
        mStats.SetSingerPitchDeviationInfo(cur->GetSingerIndex(), fe0, fe4);
    }
    if (i14 >= 2) {
        mStats.mDoubleHarmonyPhraseCount++;
        if (i15 >= 2) {
            mStats.mDoubleHarmonyHit++;
        }
        if (!(i14 - 3)) {
            mStats.mTripleHarmonyPhraseCount++;
            if (i15 == 3) {
                mStats.mTripleHarmonyHit++;
            }
        }
    }
    if (i15 > 1)
        ic8 = i15 + 3;
    if (ic8 != -1) {
        int idx = mVocalParts.front()->CurrentPhraseIndex();
        int min = std::min(ic8, 4);
        UpdateCrowdMeter(min, idx);
    }
    mTambourineManager.SetTambourine(mVocalParts.front()->InTambourinePhrase());
    bool b14 = i4 != -1 && ic8 >= 4;
    if (mTrack) {
        int i16 = 0;
        if (ScoringEnabled()) {
            mTrack->OnPhraseComplete(fcc, fd0, i16);
        }
        if (IsLocal()) {
            LocalScorePhrase(ic8, vec60, b14);
            int packedBools = PackBools(vec60);
            static Message msg("send_score_phrase", 0, 0, 0);
            msg[0] = ic8;
            msg[1] = packedBools;
            msg[2] = i14;
            HandleType(msg);
        }
    }
    HandleType(send_vocal_phrase_over_msg);
    Handle(phrase_end_msg, false);
    if (ScoringEnabled() && ic8 != -1) {
        Message msg("phrase_rating", ic8);
        Handle(msg, false);
    }
    if (b14 && ScoringEnabled() && mTrack) {
        mCommonPhraseCapturer->HandleVocalPhrase(
            this, mTrack->mTrackConfig.TrackNum(), i4, b14
        );
    }
    if (ScoringEnabled())
        UpdateSectionStats();
}

void VocalPlayer::LocalScorePhrase(
    int i1, const std::vector<int> &i_rNewPhraseActiveParts, bool b3
) {
    if (mTrack) {
        int iActivePartCount = 0;
        for (std::vector<int>::const_iterator it = i_rNewPhraseActiveParts.begin();
             it != i_rNewPhraseActiveParts.end(); ++it) {
            iActivePartCount += *it;
        }
        MILO_ASSERT(( 0) <= ( iActivePartCount) && ( iActivePartCount) <= ( i_rNewPhraseActiveParts.size()), 0x855);
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
    mTrack = dynamic_cast<VocalTrack *>(GetUser()->GetTrack());
    MILO_ASSERT(mTrack, 0x88A);
    std::vector<VocalPhrase> &phrases = mVocalParts[0]->mVocalNoteList->mPhrases;
    mTrack->Restart(this, phrases[0].unk0 + phrases[0].unk4, phrases[1].unk0 + phrases[1].unk4);
    mCouldChat = !TheNetSession->IsLocal() && PressingToTalk();
    SendCanChat(mCouldChat);
    UpdateMicDisplay();
    mTrack->GetVocalTrackDir()->SetMaxMultiplier(mBehavior->GetMaxMultiplier());
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
    int padNum = -1;
    if (mUser->IsLocal()) {
        padNum = mUser->GetLocalUser()->GetPadNum();
    }
    return ms + TheProfileMgr.GetSyncOffset(padNum);
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

        bool hadAnyMic = hadMic0 || hadMic1 || hadMic2;
        bool noLongerHasMic0 = noMic0 && hadMic0;
        bool noLongerHasMic1 = noMic1 && hadMic1;
        bool noLongerHasMic2 = noMic2 && hadMic2;
        bool b15 = numMics == 0 || noLongerHasMic0 || noLongerHasMic1 || noLongerHasMic2;
        bool combined = b15 & GetUser()->IsLocal();
        if (!combined) {
            pTrackDir->ShowMicDisplay(false);
        } else {
            if (mVocalParts.size() == 1) {
                pTrackDir->SetMicDisplayLabel(vocals_connect_a_mic);
            } else {
                pTrackDir->SetMicDisplayLabel(vocals_mics_disconnected);
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

int VocalPlayer::OnMsg(const ButtonDownMsg &msg) {
    if ((User *)GetUser() != msg.GetUser())
        return 0;
    bool b1 = false;
    if (TheUI->FocusPanel()) {
        b1 = strcmp(TheUI->FocusPanel()->Name(), "world_panel") == 0;
    }
    if (TheUI->FocusPanel() != TheGamePanel && !b1) {
        if (!dynamic_cast<PracticePanel *>(TheUI->FocusPanel()))
            return 0;
    }
    OvershellSlot *slot = TheBandUI.GetOvershell()->FindSlotForUser(GetUser());
    if (slot->IsLeavingOptions() || !slot->IsHidden())
        return 0;
    const DataArray *arr = msg.mData;
    if (mTambourineManager.IsTambourineButton((JoypadButton)arr->Node(3).Int(arr))) {
        mTambourineManager.HandleButtonDown();
    }
    return 0;
}

DECOMP_FORCEACTIVE(
    VocalPlayer,
    "HandleActivateVolume: Couldn't get a VocalParam for supposed volume button %d!\n",
    "HandleDeactivateVolume: Couldn't get a VocalParam for supposed volume button %d!\n",
    "HandleChangeVolume got an unused button! Ignoring it.\n",
    "( 0) <= ( o_rMicNumber) && ( o_rMicNumber) < ( 3)"
)

int VocalPlayer::OnMsg(const ButtonUpMsg &) { return 0; }

bool VocalPlayer::AllowPitchCorrection() const {
    MILO_ASSERT(TheGameMicManager, 0xA46);
    return TheGameMicManager->GetMicCount() == 1 && mVocalParts.size() == 1;
}

bool VocalPlayer::AllowWarningState() const {
    MILO_ASSERT(TheGameMicManager, 0xA56);
    return !GetUser()->IsNullUser() || TheGameMicManager->GetMicCount() > 0;
}

void VocalPlayer::OnGameOver() {
    if (!MetaPerformer::Current()->SongEndsWithEndgameSequence()) {
        TheGameMicManager->SetPlayback(false);
    }
    mTambourineManager.GameOver();
    FOREACH (it, mVocalParts) {
        (*it)->OnGameOver();
    }
}

#pragma push
#pragma pool_data off
void VocalPlayer::ResetScoring() {
    std::vector<int> phraseActiveParts(3);
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
        vec.push_back(std::make_pair((*it)->GetFirstPhraseMarker(), *it));
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

void VocalPlayer::LocalHitCoda() {
    mBand->LocalFinishedCoda(this);
    mTrack->HideCoda();
    Deploy();
}

void VocalPlayer::LocalBlowCoda() { mBand->LocalBlowCoda(this); }

float VocalPlayer::GetHitPercentage(int p) {
    MILO_ASSERT(p < NumVocalParts(), 0xD18);
    return mVocalParts[p]->GetOverallPartHitPercentage();
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
        startPart = endPart = isolatedPart;
    }
    int count = 0;
    unsigned int phraseIdx = 0;
    int byteOffset = 0;
    while (phraseIdx < phraseVec.size()) {
        VocalPhrase *phrase = (VocalPhrase *)((char *)&phraseVec[0] + byteOffset);
        int clampedStart = phrase->unk8;
        if (clampedStart < startTick) clampedStart = startTick;
        int clampedEnd = phrase->unk8 + phrase->unkc;
        if (endTick < clampedEnd) clampedEnd = endTick;
        int part = startPart;
        int found = 0;
        while (part <= endPart && !found) {
            if (phrase && part == 0) {
                if (phrase->unk10 != phrase->unk14) {
                    found = 1;
                    count++;
                }
            } else {
                VocalNoteList *vnl = TheSongDB->GetVocalNoteList(part);
                if (vnl != NULL && vnl->HasNoteInRange(clampedStart, clampedEnd) != -1) {
                    if (part != 0 || phrase->unk10 != phrase->unk14) {
                        found = 1;
                        count++;
                    }
                }
            }
            part++;
        }
        phraseIdx++;
        byteOffset += 0x38;
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
    return AllowPitchCorrection() && TheProfileMgr.mSynapseEnabled;
}

const VocalPhrase *VocalPlayer::GetNextPhraseMarker(const VocalPhrase *const &p) const {
    return mVocalParts.front()->GetNextPhraseMarker(p);
}

bool VocalPlayer::AtFirstPhrase() const {
    VocalPart *vp = mVocalParts.front();
    return vp->mThisPhrase == vp->mVocalNoteList->mPhrases.data();
}

bool VocalPlayer::AtLastPhrase() const {
    VocalPart *vp = mVocalParts.front();
    return vp->mThisPhrase == vp->mVocalNoteList->mPhrases.data() + vp->mVocalNoteList->mPhrases.size();
}

void VocalPlayer::ToggleOverlay() {
    MILO_ASSERT(mOverlay, 0xDF0);
    mOverlay->SetShowing(!mOverlay->Showing());
    RELEASE(mVocalOverlay);
    if (mOverlay->Showing())
        mVocalOverlay = new VocalOverlay();
}

float VocalPlayer::UpdateOverlay(RndOverlay *o, float f2) {
    o->Clear();
    if (mVocalOverlay) {
        o->Print(mVocalOverlay->mDisplayedString.c_str());
    }
    return f2;
}

bool VocalPlayer::SongSectionOnly(float &f1, float &f2) const {
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

// Family-A counterexample: retail VocalPlayer::Handle carries the MessageTimer
// (proven: the unwind funclet fn_826CDB88 only byte-matches with the timer's
// frame; dropping the global timer regresses it 100->stub). Restore the timer
// for this TU only, the inverse of GuitarController.
#ifndef HX_NATIVE
#undef BEGIN_HANDLERS
#define BEGIN_HANDLERS(objType)                                                          \
    DataNode objType::Handle(DataArray *_msg, bool _warn) {                              \
        Symbol sym = _msg->Sym(1);                                                       \
        MessageTimer timer(                                                              \
            (MessageTimer::Active()) ? static_cast<Hmx::Object *>(this) : 0, sym         \
        );
#endif

#pragma push
#pragma dont_inline on
BEGIN_HANDLERS(VocalPlayer)
    HANDLE_EXPR(in_star_mode, 0)
    HANDLE_EXPR(score, GetScore())
    HANDLE_EXPR(percent_hit, 0)
    HANDLE_ACTION(toggle_overlay, ToggleOverlay())
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
