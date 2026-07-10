#include "GemPlayer.h"
#include "Player.h"
#include "PracticePanel.h"
#include "bandobj/BandTrack.h"
#include "bandobj/EndingBonus.h"
#include "bandobj/GemTrackDir.h"
#include "bandtrack/GemManager.h"
#include "bandtrack/TrackPanel.h"
#include "beatmatch/BeatMatchController.h"
#include "beatmatch/BeatMatchUtl.h"
#include "beatmatch/BeatMatcher.h"
#include "beatmatch/DrumMap.h"
#include "beatmatch/FillInfo.h"
#include "beatmatch/GameGemList.h"
#include "beatmatch/InternalSongParserSink.h"
#include "beatmatch/MasterAudio.h"
#include "beatmatch/RGChords.h"
#include "beatmatch/SongData.h"
#include "beatmatch/TrackType.h"
#include "decomp.h"
#include "game/BandPerformer.h"
#include "game/BandUserMgr.h"
#include "game/DirectInstrument.h"
#include "game/Game.h"
#include "game/GameConfig.h"
#include "game/GameMode.h"
#include "game/GamePanel.h"
#include "game/GemTrainerPanel.h"
#include "game/GuitarFx.h"
#include "game/HeldNote.h"
#include "game/KeysFx.h"
#include "game/Performer.h"
#include "game/Player.h"
#include "game/ScoreUtl.h"
#include "game/Scoring.h"
#include "game/SongDB.h"
#include "math/Utl.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/GameplayOptions.h"
#include "meta_band/MetaPerformer.h"
#include "meta_band/ModifierMgr.h"
#include "meta_band/ProfileMgr.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "obj/ObjMacros.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "os/Joypad.h"
#include "os/PlatformMgr.h"
#include "os/System.h"
#include "rndobj/Overlay.h"
#include "stl/_pair.h"
#include "synth/FxSend.h"
#include "synth/FxSendPitchShift.h"
#include "synth/StandardStream.h"
#include "synth/Synth.h"
// #include "synthwii/FXWii.h" // Wii-only, no Xbox 360 equivalent
#include "ui/UIPanel.h"
#include "utl/Messages2.h"
#include "utl/Messages3.h"
#include "utl/Messages4.h"
#include "utl/SongInfoCopy.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"
#include "utl/BeatMap.h"
#include "utl/TempoMap.h"
#include "utl/TimeConversion.h"
#include "world/Dir.h"
#include <cstdio>

class DeltaTracker;
DeltaTracker *sTracker = nullptr;

static DataNode OnEnableDeltas(DataArray *array) {
    bool enable = array->Int(1);
    RELEASE(sTracker);
    if (enable) {
        sTracker = new DeltaTracker();
    }
    return 0;
}

static DataNode OnPrintDeltas(DataArray *array) {
    if (sTracker) {
        sTracker->PrintDeltas();
    } else
        MILO_LOG("delta tracking not enabled\n");
    return 0;
}

void DeltaTrackerInit() {
    static bool once = true;
    if (once) {
        DataRegisterFunc("enable_deltas", OnEnableDeltas);
        DataRegisterFunc("print_deltas", OnPrintDeltas);
    }
    once = false;
}

bool GetPhraseExtents(BeatmatchPhraseType ty, int i2, int i3, int &i4, int &i5) {
    if (!TheSongDB->GetPhraseExtents(ty, i2, i3, i4, i5))
        return false;
    if (TheGame->mProperties.mHasSongSections) {
        int i20, i24;
        TheGameConfig->GetPracticeSections(i20, i24);
        int i28, i2c;
        TheGameConfig->GetSectionBoundsTick(i20, i28, i2c);
        if (i28 > i4)
            i4 = i28;
        int i30, i34;
        TheGameConfig->GetSectionBoundsTick(i24, i30, i34);
        if (i34 < i5)
            i5 = i34;
        if (ty == kSoloPhrase) {
            if (i3 >= i5)
                return false;
            if (i3 < i4)
                return false;
        }
    }
    if (ty == kSoloPhrase) {
        if (!TheGameMode->Property("can_solo", true)->Int())
            return false;
    }
    return true;
}

GemPlayer::GemPlayer(
    BandUser *user, BeatMaster *bm, Band *band, int i, BandPerformer *perf
)
    : Player(user, band, i, bm), mBandPerformer(perf), mGemStatus(new GemStatus()),
      mDrumSlotWeights(0), mDrumCymbalPointBonus(0), mGameCymbalLanes(0), mFill(0),
      mForceFill(0), mLastFillHitTick(-1), unk2f4(-1), mNumFillSwings(-1),
      mNumCrashFillReadyHits(1),
      mUseFills(SystemConfig("scoring", "overdrive")->FindInt("fills")), unk301(0),
      mTrillSlots(-1, -1), unk30c(0), unk310(-1), unk314(0), unk315(0), unk316(0),
      mCodaPoints(0), mCodaPointRate(200.0f), mCodaMashPeriod(500.0f),
      mMercurySwitchEnabled(1), unk33d(0), mWhammyOverdriveEnabled(1),
      mOverlay(RndOverlay::Find("score", true)),
      mGuitarOverlay(RndOverlay::Find("guitar", true)), unk348(0), unk354(0), unk358(0),
      unk35c(0), mLastTimeWhammyVelWasHigh(-10000.0f), unk364(0), mTrack(0),
      mController(0), mSyncOffset(0), mGuitarFx(0), mKeysFx(0), mFxPos(4), unk388(0),
      mPitchShift(0), unk390(0), unk394(0), unk398(0),
      unk3ac(0), mAutoMissSoundTimeoutMs(kHugeFloat), mFirstGemMs(0),
      mAnnoyingMode(0), unk3b9(0), unk3bc(0), unk3c0(0),
      mAutoMissSoundTimeoutGems(100000), mAutoMissSoundTimeoutGemsRemote(100000),
      mStatCollector(*this), unk3d8(0), unk3dc(-1), unk3e0(0), unk3e1(0),
      mSectionStartHitCount(0), mSectionStartMissCount(0), mSectionStartScore(0),
      mSustainsReleased(0), mSustainHeld(1), unk404(-1), unk408(1) {
    for (int i = 0; i < 6; i++) {
        mLastCodaSwing[i] = 0;
    }
    DataArray *odCfg = SystemConfig("scoring", "overdrive");
    mWhammySpeedThreshold = odCfg->FindFloat("whammy_speed") / 100.0f;
    mWhammySpeedTimeout = odCfg->FindFloat("whammy_timeout") * 1000.0f;
    mNumCrashFillReadyHits = odCfg->FindInt("crash_fill_ready_hits");

    DataArray *codaCfg = SystemConfig("scoring", "coda");
    mCodaPointRate = codaCfg->FindFloat("point_rate");
    mCodaMashPeriod = codaCfg->FindFloat("max_mash_ms");

    if (user->GetTrackType() == kTrackDrum) {
        mDrumSlotWeights = SystemConfig("scoring", "crowd", "drum_slot_weights");
        mDrumSlotWeightMapping = mDrumSlotWeights->FindSym("default_weights");
        DataArray *drumCfg = SystemConfig("scoring", "drums");
        mDrumCymbalPointBonus = drumCfg->FindArray("pro_drum_bonus");
    }

    SongInfoCopy songInfo(TheSongMgr.SongAudioData(MetaPerformer::Current()->Song()));
    mMatcher = new BeatMatcher(
        GetUserGuid(),
        GetSlot(),
        TheBandUserMgr->GetNumParticipants(),
        TheGameConfig->GetController(GetUser()),
        TheSongDB->GetData(),
        songInfo,
        SystemConfig("beatmatcher"),
        bm
    );
    if (DataVariable("no_drum_fills").Int())
        ToggleNoFills();

    int i3 = -1;
    if (GetUser()->IsLocal()) {
        i3 = GetUser()->GetLocalBandUser()->GetPadNum();
    }
    SetSyncOffset(TheProfileMgr.GetSyncOffset(i3));
    mMatcher->RegisterSink(*this);
    mMatcher->DrivePitchBendExternally(IsNet());
    mMatcher->EnableWhammy(TheGame->mProperties.mEnableWhammy);
    mMatcher->EnableCapStrip(TheGame->mProperties.mEnableCapstrip);
    SetFillLogic(TheGame->GetFillLogic());
    SetTypeDef(SystemConfig("player", "handlers"));
    TrackType ty = mUser->GetTrackType();
    if (ty - 1U <= 1 && !TheGame->mProperties.mDisableGuitarFx) {
        mGuitarFx = new GuitarFx(ty);
        mGuitarFx->Load();
        SetGuitarFx();
        // (Wii-only guitar FXCore init removed — retail Xbox has no unk39c member)
    }
    if (ty - 4U <= 1 && !TheGame->mProperties.mDisableKeysFx) {
        mKeysFx = new KeysFx(ty);
        mKeysFx->Load();
    }
    DeltaTrackerInit();
    if (!TheGame->InDrumTrainer()) {
        DataArray *audioCfg = SystemConfig("beatmatcher", "audio");
        float secs = 0;
        if (audioCfg->FindData("auto_miss_sound_timeout_seconds", secs, false)) {
            mAutoMissSoundTimeoutMs = secs * 1000.0f;
        }
        audioCfg->FindData(
            "auto_miss_sound_timeout_gems", mAutoMissSoundTimeoutGems, false
        );
        audioCfg->FindData(
            "auto_miss_sound_timeout_gems_remote", mAutoMissSoundTimeoutGemsRemote, false
        );
    }

    DataArray *ttypeArr =
        SystemConfig("in_game_tutorials")->FindArray(TrackTypeToSym(GetTrackType()));
    mSustainsReleasedBeforePopup = ttypeArr->FindArray("released_before_popup")->Int(1);
    mHeldNotes.resize(5);
    mUpcomingFretReleases.reserve(10);
};

void GemPlayer::DynamicAddBeatmatch() {
    float ms = TheTaskMgr.Seconds(TaskMgr::kRealTime) * 1000.0f;
    UpdateGameCymbalLanes();
    mMatcher->PostDynamicAdd(mTrackNum, ms);
    Player::DynamicAddBeatmatch();
    mGemStatus->mGems.resize(TheSongDB->GetTotalGems(mTrackNum));
}

void GemPlayer::PostDynamicAdd() {
    float ms = TheTaskMgr.Seconds(TaskMgr::kRealTime) * 1000.0f;
    Player::PostDynamicAdd();
    int tick = MsToTick(ms);
    IgnoreGemsUntil(tick);
    if (mTrack) {
        mTrack->GetGemManager()->SetupGems(0);
        mTrack->ResetFills(true);
        mTrack->DropIn(tick);
    }
    if (GetUser()->IsLocal()) {
        JoypadKeepAlive(GetUser()->GetLocalUser()->GetPadNum(), true);
    }
}

void GemPlayer::Leave() {
    Player::Leave();
    if (mTrackNum != -1) {
        mMatcher->Leave();
    }
    if (GetUser()->IsLocal()) {
        JoypadKeepAlive(GetUser()->GetLocalUser()->GetPadNum(), false);
    }
}

GemPlayer::~GemPlayer() {
    delete mGemStatus;
    if (mTrackNum != -1) {
        std::list<int> chans;
        mBeatMaster->GetAudio()->FillChannelList(chans, mTrackNum);
        Stream *stream = mBeatMaster->GetAudio()->GetSongStream();
        FOREACH (it, chans) {
            stream->SetFXSend(*it, nullptr);
        }
    }
    RELEASE(mMatcher);
    RELEASE(mController);
    RELEASE(mGuitarFx);
    RELEASE(mKeysFx);
    TheSynth->DestroyPitchShift(mPitchShift);
    if (GetUser()->IsLocal()) {
        JoypadKeepAlive(GetUser()->GetLocalUser()->GetPadNum(), false);
    }
}

void GemPlayer::SeeGem(int i1, float f2, int i3) {
    if (mFirstGemMs == 0) {
        InputReceived();
        mFirstGemMs = f2;
    }
    if (mEnabledState == 0 && i1 == GetTrackNum() && mTrack) {
        mTrack->See(f2, i3);
    }
    if (mAnnoyingMode && mEnabledState == 0) {
        static Message pass_msg("annoying_pass", 0, 0, 0);
        pass_msg[0] = GetUser();
        pass_msg[1] = TheSongDB->GetGem(mTrackNum, i3).GetSlot();
        pass_msg[2] = i3;
        Handle(pass_msg, false);
    }
    TheSongDB->GetGem(mTrackNum, i3);
    SeeGemHook(i1, f2, i3);
}

void GemPlayer::FinaleSwing(int i1) {
    if (IsLocal()) {
        LocalFinaleSwing(i1);
        if (GetBandTrack()) {
            GetBandTrack()->ClearFinaleHelp();
        }
        static Message msg("send_finale_hit", 0);
        msg[0] = i1;
        HandleType(msg);
    }
}

void GemPlayer::LocalFinaleSwing(int i1) {
    if (TheWorld) {
        static Message msg("");
        msg.SetType(
            MakeString("endgame_swing_%s_%d", TrackTypeToSym(GetTrackType()).Str(), i1)
        );
        TheWorld->Handle(msg, false);
    }
}

void GemPlayer::Swing(int i1, int i2, float f3, bool b4, bool b5) {
    InputReceived();
    if (unk1e1) {
        FinaleSwing(GetTrackSlot(i2));
    } else {
        Export(swing_msg, true);
        if (mTrack && GetUser()->GetTrackType() == 0 && i2 == 0) {
            mTrack->GetTrackDir()->KickSwing();
        }
    }
    SwingHook(i1, i2, f3, b4, b5);
}
__declspec(noinline) int _outline_GetSize(GemStatus* _obj) {
    return _obj->GetSize();
}


#pragma push
#pragma pool_data off
void GemPlayer::Hit(
    int track, float ms, int gem_id, unsigned int gem_hit_slots, GemHitFlags flags
) {
    MILO_ASSERT(gem_hit_slots != 0, 0x23F);
    if (!mGemStatus->Get0x40(gem_id)) {
        if (TheGame->IsPaused())
            return;
        HandleFirstGemAfterRollback(gem_id);
        InputReceived();
        if (gem_id == _outline_GetSize(mGemStatus) - 1) {
            unk3e1 = 1;
        }
        const GameGem &gem = TheSongDB->GetGem(mTrackNum, gem_id);
        unsigned int gemSlots = gem.mSlots;
        float delta = gem.mMs - (ms + mSyncOffset);
        if (sTracker) {
            int cur = sTracker->mCur;
            if (cur < 1000) {
                sTracker->mCur = cur + 1;
                sTracker->mDeltas[cur] = delta;
            } else if (cur == 1000) {
                sTracker->DeltaTracker::PrintDeltas();
                sTracker->mCur = 1001;
            }
        }
        unk1fe = 0;
        unk1fd = 0;
        unk1ff = 0;
        int _tmp1 = gem.GetTick();
        int ignoreAt = IgnoreGemsAt(_tmp1);
        if (ignoreAt) {
            if (mTrack) {
                const GameGem &gem2 = TheSongDB->GetGem(mTrackNum, gem_id);
                int _tmp2 = gem2.GetSlot();
                mTrack->Miss(ms, gem_id, _tmp2);
            }
            IgnoreGem(gem_id);
            goto block_96;
        }
        if (InIgnorableFill(gem.GetTick())) {
            mTrack->Hit(ms, gem_id, 1);
            IgnoreGem(gem_id);
            return;
        }
        if (mTrack) {
            if (IsAutoplay()
                && mUser->GetTrackType() != kTrackRealGuitar
                && mUser->GetTrackType() != kTrackRealBass) {
                unsigned int remaining = gemSlots;
                int slot = 0;
                while (remaining) {
                    unsigned int bit = 1U << slot;
                    if (remaining & bit) {
                        remaining -= bit;
                        FretButtonDown(slot, ms);
                        RemoveFretReleasesInSlot(slot);
                        float msPlusOffset = ms + mSyncOffset;
                        float endMs;
                        if (gem.IgnoreDuration()) {
                            endMs = 200.0f;
                        } else {
                            endMs = (float)gem.mDurationMs;
                        }
                        UpcomingFretRelease release;
                        release.unk0 = slot;
                        release.unk4 = msPlusOffset + endMs;
                        mUpcomingFretReleases.push_back(release);
                    }
                    slot++;
                }
            }
            int hitFlags = 1;
            if (mMatcher && mMatcher->UsingAlternateButtons()) {
                hitFlags |= 8;
            }
            if (gemSlots != gem_hit_slots) {
                mTrack->PartialHit(ms, gem_id, gem_hit_slots, hitFlags);
                EndHitStreak();
                BuildMissStreak(gem_id);
            } else {
                mTrack->Hit(ms, gem_id, hitFlags);
            }
            if ((int)flags & 8) {
                mStats.mUpstrumCount++;
            } else {
                mStats.mDownstrumCount++;
            }
            if (!mTrack->Lefty()) {
                mStats.m0x34 = 1;
            }
        }
        if (gemSlots != gem_hit_slots) {
            mStatCollector.PassGem(ms, gem, gem_id);
            if (gem_id != -1) {
                mGemStatus->mGems[gem_id] |= 2;
            }
            if (gem_id != -1) {
                mGemStatus->mGems[gem_id] |= 4;
            }
        } else {
            mStatCollector.HitGem(ms, gem, gem_id, gem_hit_slots, flags);
            if (gem_id != -1) {
                mGemStatus->mGems[gem_id] |= 1;
            }
            mGemStatus->mHits++;
        }
        if (((int)flags & 2) && gem_id != -1) {
            mGemStatus->mGems[gem_id] |= 0x80;
        }
        UpdateSectionStats();
        if (((int)flags & 4) && gem_id != -1) {
            mGemStatus->mGems[gem_id] |= 0x20;
        }
    block_96:
        unk3c0 = 0;
        SetRemoteAnnoyingMode(false);
#ifdef HX_NATIVE
        // msg[3] below writes mData->Node(5) (Message::operator[](i) == Node(i+2)),
        // but the 3-arg ctor only allocates DataArray(5) (nodes 0-4). On PPC the
        // out-of-bounds Node(5) read/write is benign; native's bounds-checked
        // DataArray::Node OSFatals ("Array doesn't have node 5"). Allocate one more
        // arg slot (DataArray(6)) so Node(5) is in-bounds.
        static Message msg("hit", 0, 0, 0, 0.0f);
#else
        static Message msg("hit", 0, 0, 0.0f);
#endif
        float numTotalF = (float)GameGem::CountBitsInSlotType(gemSlots);
        float numHitF = (float)GameGem::CountBitsInSlotType(gem_hit_slots);
        float ratio = numHitF / numTotalF;
        msg[1] = gem_id;
        msg[2] = (int)flags;
        msg[3] = ratio;
        Export(msg, true);
        if (mIsInCoda) {
            if (gemSlots == gem_hit_slots) {
                CodaHit(ms, gem_id);
            } else {
                mBand->DealWithCodaGem(this, gem_id, false, false);
            }
        } else {
            int numGemSlots = GameGem::CountBitsInSlotType(gem_hit_slots);
            unk354 = ms;
            CheckHeldNotes(gem.mMs);
            if (!ignoreAt) {
                BuildHitStreak(gem_id, delta);
                EndMissStreak();
                if ((int)flags & 8) {
                    mStats.m0x30++;
                }
                Player::Hit();
                AddHeadPoints(ms, gem_id, numGemSlots, flags);
            }
            if (gem.IgnoreDuration()) {
                UpdateCrowdMeter((float)numGemSlots / (float)gem.NumSlots(), gem_id);
            } else {
                HeldNote &note = GetUnusedHeldNote();
                HeldNote tmp(
                    (TrackType)mUser->GetTrackType(), gem_id, gem, gem_hit_slots
                );
                note = tmp;
            }
            HandleCommonPhraseNote(gem.NumSlots() == numGemSlots, gem_id);
            if (mBehavior->GetHasSolos()) {
                HandleSoloGem(gem_id, true, ms, ((unsigned int)flags >> 1) & 1);
            }
            if (TheGame->mProperties.mInDrumTrainer) {
#ifdef HX_NATIVE
                // dtmsg[1] writes Node(3) but the 1-arg ctor only allocates
                // DataArray(3) (nodes 0-2); add one arg slot so Node(3) is in-bounds
                // (same Message::operator[](i)==Node(i+2) overrun as "hit"/"send_hit").
                static Message dtmsg("drum_trainer_unmute", 0, 0);
#else
                static Message dtmsg("drum_trainer_unmute", 0);
#endif
                dtmsg[1] = gem.GetSlot();
                Export(dtmsg, false);
            }
        }
        if (IsLocal()) {
#ifdef HX_NATIVE
            // msg[3] writes Node(5) but the 3-arg ctor only allocates DataArray(5)
            // (nodes 0-4); add one arg slot so Node(5) is in-bounds (see the "hit"
            // Message above — same Message::operator[](i)==Node(i+2) overrun).
            static Message msg("send_hit", 0, 0, 0, 0.0f);
#else
            static Message msg("send_hit", 0, 0, 0.0f);
#endif
            msg[1] = mStats.GetCurrentStreak();
            msg[2] = (int)mScore;
            msg[3] = mCrowd->GetDisplayValue();
            HandleType(msg);
        }
        unk1fe = 1;
        unk1fd = 1;
        unk1ff = 1;
        HitHook(track, ms, gem_id, gem_hit_slots, flags);
    }
}
#pragma pop

void GemPlayer::Miss(
    int track, int slot, float ms, int gem_id, int chordSlotsInProgress, GemHitFlags flags
) {
    if (!TheGame->IsPaused()) {
        InputReceived();
        if (!HandleSpecialMissScenarios(slot, ms) && !mGemStatus->Get0x40(gem_id)) {
            if (InRollback())
                return;
            HandleFirstGemAfterRollback(gem_id);
            TheSongDB->GetGem(mTrackNum, gem_id);
            if (mTrack) {
                mTrack->Miss(ms, gem_id, slot);
            }
            int gemTick = TheSongDB->GetGem(mTrackNum, gem_id).GetTick();
            if (!InIgnorableFill(gemTick)) {
                if (IgnoreGemsAt(gemTick)) {
                    return;
                }
                if (!mGemStatus->Get0x2(gem_id)) {
                    BuildMissStreak(gem_id);
                }
                mGemStatus->Set0x2(gem_id);
                if (!(flags & kGemHitFlagNoPenalize)) {
                    Penalize(ms, gem_id, 0.0f);
                }
                if (mIsInCoda && IsCodaMiss(ms)) {
                    mBand->DealWithCodaGem(this, gem_id, false, false);
                }
                PlayMissSound(slot);
                static Message missMsg("miss", 0, 0, 0);
                missMsg[0] = GetUser();
                missMsg[1] = slot;
                missMsg[2] = gem_id;
                Export(missMsg, true);
                MissHook(track, slot, ms, gem_id, chordSlotsInProgress);
            }
        }
    }
}

bool GemPlayer::HandleSpecialMissScenarios(int i1, float f2) {
    if (CanFlail(f2)) {
        if (mUser->GetTrackType() == 0) {
            int slot = mController->GetVirtualSlot(i1);
            int bucket = mController->GetVelocityBucket(i1);
            PlayDrum(slot, i1, VelocityBucketToDb(bucket), bucket);
        } else {
            static Message miss_msg("miss", 0, 0, 0);
            miss_msg[0] = GetUser();
            miss_msg[1] = i1;
            miss_msg[2] = -1;
            Export(miss_msg, true);
        }
        return true;
    } else
        return false;
}

bool GemPlayer::CanFlail(float ms) {
    int gemTick = TheSongDB->GetGem(mTrackNum, 0).mTick;
    if ((int)MsToTick(ms) <= gemTick - 0xF0) {
        return true;
    }
    if (PastFinalNote() && !TheGame->mProperties.mInDrumTrainer) {
        return true;
    }
    return false;
}

void GemPlayer::SpuriousMiss(int i1, int i2, float f3, int i4) {
    if (!CanFlail(f3) && mTrack) {
        mTrack->Miss(f3, i4, i2);
    }
}

void GemPlayer::Pass(int track, float ms, int gem_id, bool cur_track) {
    if (!mGemStatus->Get0x40(gem_id)) {
        HandleFirstGemAfterRollback(gem_id);
        if (gem_id == mGemStatus->GetSize() - 1) {
            unk3e1 = 1;
        }
        if (cur_track) {
            int gemTick = TheSongDB->GetGem(mTrackNum, gem_id).GetTick();
            if (mIsInCoda) {
                if (!mMatcher->InCodaFreestyle(gemTick, true) && IsLocal()) {
                    ResetCodaPoints();
                    mBand->DealWithCodaGem(this, gem_id, false, false);
                }
            }
            if (IgnoreGemsAt(TheSongDB->GetGem(mTrackNum, gem_id).GetTick())) {
                IgnoreGem(gem_id);
                return;
            }
            if (InIgnorableFill(gemTick)) {
                mTrack->Pass(gem_id);
                IgnoreGem(gem_id);
                return;
            }
            if (!mGemStatus->GetIgnored(gem_id)) {
                if (mTrack) {
                    mTrack->Pass(gem_id);
                }
                mGemStatus->mMisses++;
                UpdateSectionStats();
                const GameGem &gem = TheSongDB->GetGem(mTrackNum, gem_id);
                mStatCollector.PassGem(ms, gem, gem_id);
                if (gem.GetNoStrum()) {
                    mStats.mHopoGemCount++;
                }
            }

            GemStatus *status = mGemStatus;
            int phraseFlags;
            if (gem_id == -1) {
                phraseFlags = 1;
            } else {
                phraseFlags = status->mGems[gem_id] & 0xE;
            }

            if (phraseFlags == 0) {
                status->Set0x4(gem_id);
                if (ShouldPenalizeGem(gem_id)) {
                    Penalize(ms, gem_id, 0.0f);
                } else {
                    EndHitStreak();
                    HandleCommonPhraseNote(0, gem_id);
                }
                BuildMissStreak(gem_id);
                unk3bc++;
            } else {
                bool isPhraseStart = true;
                if (gem_id != 0) {
                    int prevPhrase = TheSongDB->GetPhraseID(mTrackNum, gem_id - 1);
                    if (TheSongDB->GetPhraseID(mTrackNum, gem_id) == prevPhrase) {
                        isPhraseStart = false;
                    }
                }
                bool isPhraseEnd = true;
                if (gem_id != TheSongDB->GetNumPhraseIDs(mTrackNum) - 1) {
                    int nextPhrase = TheSongDB->GetPhraseID(mTrackNum, gem_id + 1);
                    if (TheSongDB->GetPhraseID(mTrackNum, gem_id) == nextPhrase) {
                        isPhraseEnd = false;
                    }
                }

                if ((mGemStatus->Get0x2(gem_id) && isPhraseStart)
                    || (isPhraseEnd && TheGame->mProperties.mAllowOverdrivePhrases)) {
                    HandleCommonPhraseNote(0, gem_id);
                }
                mGemStatus->Set0x4(gem_id);
            }

            mStats.m0x08++;

            bool skipPassMsg = false;
            if (TheGame->mProperties.mInTrainer) {
                if (mGemStatus->GetIgnored(gem_id)) {
                    skipPassMsg = true;
                }
            }

            if (!skipPassMsg) {
                static Message passMsg("pass", 0);
                passMsg[0] = gem_id;
                Export(passMsg, false);
            }

            PassHook(track, ms, gem_id, cur_track);
        }
    }
}

void GemPlayer::Ignore(int i1, float ms, int gem_id, const UserGuid &u) {
    if (!mGemStatus->Get0x40(gem_id)) {
        HandleFirstGemAfterRollback(gem_id);
        int size = mGemStatus->GetSize();
        if (gem_id == size - 1) {
            unk3e1 = 1;
        }
        IgnoreGem(gem_id);
        if (mBehavior->mHasSolos) {
            HandleSoloGem(gem_id, false, ms, false);
        }
        if (mTrack) {
            mTrack->Ignore(gem_id);
        }
    }
}

void GemPlayer::ImplicitGem(int i1, float f2, int i3, const UserGuid &u) {
    if (!mGemStatus->GetEncountered(i3)) {
        IgnoreGem(i3);
    }
    if (mTrack) {
        mTrack->Ignore(i3);
    }
}

void GemPlayer::FretButtonDown(int i1, float f2) {
    if (mUser->GetTrackType()) {
        int slot = GetTrackSlot(i1);
        if (mTrack)
            mTrack->SetFretButtonPressed(slot, true);
        static Message msg("fret_down", 0);
        msg[0] = slot;
        Export(msg, false);
    }
}

void GemPlayer::FretButtonUp(int i1, float f2) {
    if (mUser->GetTrackType()) {
        int slot = GetTrackSlot(i1);
        if (mTrack)
            mTrack->SetFretButtonPressed(slot, false);
        static Message msg("fret_up", 0);
        msg[0] = slot;
        Export(msg, false);
        HeldNote *note = FindHeldNoteFromSlot(i1);
        if (note && note->HasGem()) {
            float maxed = std::max(0.0f, note->SetHoldTime(f2));
            AddPoints(maxed, true, true);
            mStats.AddSustain(maxed);
            note->ReleaseSlot(i1);
            if (mTrack) {
                GemManager *mgr = mTrack->GetGemManager();
                if (mgr) {
                    mgr->ReleaseSlot(note->unk_0x4, i1);
                }
            }
            mSustainsReleased++;
            if (mSustainsReleased >= mSustainsReleasedBeforePopup && mSustainHeld == 0) {
                PopupHelp(hold_note, true);
                mSustainsReleased = 0;
            }
        }
    }
}

void GemPlayer::PlayMissSound(int i1) {
    if (unk408) {
        const char *seq;
        switch (GetTrackType()) {
        case 1:
        case 6:
        case 7:
            seq = "miss_gtr.cue";
            break;
        case 2:
        case 8:
        case 9:
            seq = "miss_bass";
            break;
        case 0:
            seq = i1 == 0 ? "miss_kick.cue" : "miss_drum";
            break;
        case 4:
        case 5:
            seq = "miss_keys.cue";
            break;
        default:
            seq = gNullStr;
            break;
        }
        if (seq != gNullStr) {
            GetTrackPanel()->PlaySequence(seq, 0, 0, 0);
        }
    }
}

void GemPlayer::MercurySwitch(bool b, float f) {
    unk33d = b;
    if (unk388)
        return;
    if (!mMercurySwitchEnabled)
        return;
    if (!b)
        return;
    if (!mBehavior->mTiltDeployBand)
        return;
    DeployBandEnergyIfPossible(false);
}

void GemPlayer::FilteredWhammyBar(float val) {
    if (!IsLocal() || !TheGame->mProperties.mEnableWhammy)
        return;
    else {
#ifdef HX_NATIVE
        // Native robustness: val (f3 from TrackWatcherImpl::CheckForPitchBend,
        // (whammy - mBiggestWhammy) / (1 + mBiggestWhammy)) is analytically in
        // [-1, 0], but float rounding at the -1.0 boundary can slip just past -1
        // and some controller breeds can drive whammy below -1 / leave it
        // uninitialized (GuitarController::GetWhammyBar). On native a MILO_FAIL ->
        // Debug::Fail -> PlatformDebugBreak (abort/SIGABRT, exit 134) where the Wii
        // build only shows a Continue dialog. Clamp so the bounds assert holds.
        val = Clamp<float>(-1.0f, 0.0f, val);
#endif
        MILO_ASSERT_RANGE_EQ(val, -1, 0, 0x4EE);
        HeldNote *note = FindFirstActiveHeldNote();
        if (note != nullptr && mWhammyOverdriveEnabled) {
            int phraseID = TheSongDB->GetPhraseID(mTrackNum, note->unk_0x4);
            if (phraseID != -1
                && !mCommonPhraseCapturer->DidTrackFail(phraseID, mTrackNum)) {
                float ms = PollMs();
                float denominator = ms - unk35c;
                bool movingFast = false;
                if (std::fabs((val - unk358) / denominator) > mWhammySpeedThreshold) {
                    movingFast = true;
                    mLastTimeWhammyVelWasHigh = PollMs();
                }
                bool active = movingFast
                    || ((PollMs() - mLastTimeWhammyVelWasHigh) < mWhammySpeedTimeout);
                active &= TheGame->mProperties.mEnableWhammy;
                if (active && !unk348) {
                    Handle(whammy_start_msg, false);
                } else if (!active && unk348) {
                    Handle(whammy_end_msg, false);
                }
                unk348 = active;
            }
        } else if (unk348) {
            unk348 = false;
            Handle(whammy_end_msg, false);
        }
        SendWhammyBar(val);
        unk358 = val;
        unk35c = PollMs();
    }
}

void GemPlayer::SwingAtHopo(int, float, int) {
    HandleType(swingAtHopo_msg);
    mStats.mHopoGemCount++;
}

void GemPlayer::Hopo(int i1, float ms, int gem_id) {
    if (!mGemStatus->Get0x40(gem_id)) {
        Export(hopo_msg, true);
        const GameGemList *gemList = TheSongDB->GetGemList(mTrackNum);
        const GameGem &gem = gemList->GetGem(gem_id);
        if (gem.GetForceStrum()) {
            mGemStatus->SetHopoed(gem_id);
            mStats.mHopoGemsHopoed++;
            mStats.mHopoGemCount++;
        }
    }
}

void GemPlayer::ReleaseGem(int i1, float ms, int gem_id, float f4) {
    unk354 = ms;
    HeldNote *note = FindHeldNoteFromGemID(gem_id);
    if (note && note->HasGem()) {
        CheckHeldNotes(ms);
        FinishHeldNote(ms, *note);
    }
}

void GemPlayer::FillSwing(int i1, int i2, int i3, int i4, bool b5) {
    if (IsLocal()) {
        if (b5 && !mIsInCoda) {
            EnterCoda();
        }
        if (i2 >= 0) {
            FillInProgress(i2, i3);
        }
        if (mIsInCoda) {
            PopupHelp("rock_ending", false);
        } else {
            PopupHelp("fill", false);
        }
        int tick = TheSongDB->GetCodaStartTick();
        if (!mIsInCoda && (tick >= 0 && (tick - i4 < 0x1E0))) {
            mNumFillSwings = 100;
        }
        mLastFillHitTick = i4;
        if (mTrack && i3 != -1) {
            ShowFillHit(i3);
        }
    }
}

void GemPlayer::ShowFillHit(int i1) {
    if (IsLocal()) {
        mNumFillSwings++;
        LocalShowFillHit(i1, mController->GetVelocityBucket(i1), mIsInCoda);
        static Message msg("send_fill_hit", 0, 0, 0);
        msg[0] = i1;
        msg[1] = mNumFillSwings;
        msg[2] = mIsInCoda;
        HandleType(msg);
    }
}

void GemPlayer::OnRemoteFillHit(int i1, int i2, bool b3) {
    if (b3 && !mIsInCoda) {
        EnterCoda();
    }
    mNumFillSwings = i2;
    SetAutoplay(true);
    LocalShowFillHit(i1, 0, b3);
}

void GemPlayer::LocalShowFillHit(int i1, int i2, bool b3) {
    if (mTrack) {
        GemTrackDir *dir = mTrack->GetTrackDir();
        mTrack->FillHit(i1, i2);
        if (mIsInCoda || b3) {
            dir->Mash(i1);
        } else {
            MILO_ASSERT(mTrackType == kTrackDrum, 0x5B6);
            if (mTrackType == kTrackDrum) {
                if (mNumFillSwings <= mNumCrashFillReadyHits) {
                    dir->FillHit(mNumFillSwings);
                }
                dir->FillMash(i1);
            }
        }
    }
}

void GemPlayer::FillReset() {
    FillInProgress(-1, 0);
    for (int i = 0; i < 6; i++)
        mLastCodaSwing[i] = 0;
    if (mTrack)
        mTrack->GetTrackDir()->FillReset();
}

void GemPlayer::FillComplete(int i1, int i2) {
    if (mBehavior->GetFillsDeployBandEnergy() && TheGame->mProperties.mEnableOverdrive) {
        FillLogic logic = TheGame->GetFillLogic();
        if (i1 - 1 >= mNumCrashFillReadyHits
            || logic == kFillsDeployGemAndDim
            || logic == kFillsDeployGemAndInvisible) {
            mTrack->GetTrackDir()->CrashFill();
            DeployBandEnergyIfPossible(true);
            mStats.AddFillHit();
        }
    }
    SetFilling(false, i2);
}

void GemPlayer::IgnoreGemsUntil(int i1) {
    int total = TheSongDB->GetTotalGems(mTrackNum);
    const GameGemList *gems = TheSongDB->GetGemList(mTrackNum);
    int idx = gems->ClosestMarkerIdxAtOrAfterTick(i1);
    if (idx == -1)
        idx = total - 1;
    else if (gems->GetGem(idx).GetTick() > i1) {
        idx--;
    }
    for (; idx >= 0; idx--) {
        TheSongDB->GetGem(mTrackNum, idx);
        if (mGemStatus->GetEncountered(idx))
            break;
        IgnoreGem(idx);
    }
}

void GemPlayer::IgnoreUntilRollback(float f1) {
    const GameGemList *gems = TheSongDB->GetGemList(mTrackNum);
    for (int i = 0; i < (int)mGemStatus->mGems.size(); i++) {
        if (gems->GetGem(i).GetMs() < f1) {
            mGemStatus->SetIgnored(i);
            mGemStatus->Set0x40(i);
        } else
            break;
    }
}

void GemPlayer::IgnoreGem(int i1) {
    if (!mGemStatus->GetIgnored(i1)) {
        Stats &st = mStats;
        st.m0x0c++;
        mGemStatus->SetIgnored(i1);
        if (mEnabledState == kPlayerEnabled) {
            HandleCommonPhraseNote(1, i1);
        }
    }
}

void GemPlayer::NoteOn(int i1) {
    DirectInstrument *inst = TheGamePanel->GetDirectInstrument();
    if (inst->Enabled()) {
        inst->NoteOn(i1);
    }
}

void GemPlayer::NoteOff(int i1) {
    DirectInstrument *inst = TheGamePanel->GetDirectInstrument();
    if (inst->Enabled()) {
        inst->NoteOff(i1);
    }
}

void GemPlayer::PlayNote(int i1) {
    DirectInstrument *inst = TheGamePanel->GetDirectInstrument();
    if (inst->Enabled()) {
        inst->PlayNote(i1, 5);
    }
}

void GemPlayer::OutOfRangeSwing() {
    if (mTrack) {
        GemTrackDir *dir = mTrack->GetTrackDir();
        if (dir) {
            dir->KeyMissLeft();
            dir->KeyMissRight();
        }
    }
}

Symbol GemPlayer::GetStarRating() const {
    return TheScoring->GetStarRating(GetNumStars());
}

int GemPlayer::GetNumStars() const { return GetStarsForScore(GetScore(), GetUserGuid()); }

int GemPlayer::GetBaseMaxPoints() const {
    return TheSongDB->GetBaseMaxPoints(GetUserGuid());
}

int GemPlayer::GetBaseMaxStreakPoints() const {
    return TheSongDB->GetBaseMaxStreakPoints(GetUserGuid());
}

int GemPlayer::GetBaseBonusPoints() const {
    return TheSongDB->GetBaseBonusPoints(GetUserGuid());
}

void GemPlayer::SetFillLogic(FillLogic fl) {
    if (mMatcher)
        mMatcher->SetFillLogic(fl);
}

bool GemPlayer::DoneWithSong() const {
    if (mQuarantined)
        return true;
    else {
        const std::vector<GameGem> &gems = TheSongDB->GetGems(mTrackNum);
        if (gems.empty())
            return true;
        else {
            return mGemStatus->Get0xD(gems.size() - 1);
        }
    }
}

void GemPlayer::Poll(float ms, const SongPos &pos) {
    BeatMatchController *ctrl = mController;
    if (ctrl->IsDisabled() && ctrl->unk25) {
        ctrl->Disable(false);
        ctrl->unk25 = false;
    }
    CheckFretReleases(ms);
    if (!mGameOver) {
        bool hasHeld = false;
        static DataNode &force_guitar_fx = DataVariable("force_guitar_fx");
        static DataNode &force_keys_fx = DataVariable("force_keys_fx");

        int padNum = -1;
        if (GetUser()->IsLocal()) {
            padNum = GetUser()->GetLocalBandUser()->GetPadNum();
        }
        SetSyncOffset(TheProfileMgr.GetSyncOffset(padNum));
        unk388 = false;
        mMatcher->Poll(ms);

        if (unk348 && TheGame->mProperties.mEnableOverdrive) {
            float tickDiff = pos.GetTotalTick() - mSongPos.GetTotalTick();
            AddEnergy(tickDiff * TheScoring->mOverdriveConfig.whammyRate / 480.0f);
        }

        if (mController->IsDisabled() && TheGame->AllowInput()
            && mEnabledState == kPlayerEnabled && !unk1e1) {
            mController->Disable(false);
        }

        CheckSolo(ms);
        Player::Poll(ms, pos);
        CheckHeldNotes(ms);
        SetFilling(InFillNow(), (int)mMatcher->mSongPos.GetTotalTick());
        PollTrack();

        int tick = (int)pos.GetTotalTick();
        float beat;
        float beatPhase;
        float tempo = 60000.0f / TheTempoMap->GetTempo(tick);

        if (mGuitarFx && !TheGame->mProperties.mDisableGuitarFx) {
            SetGuitarFx();

            beat = TheSongDB->GetData()->GetBeatMap()->Beat(tick);
            beat *= 0.5f;
            beatPhase = beat - (float)std::floor(beat);

            bool deploying = IsDeployingBandEnergy();
            bool soloFx = false;
            if (unk315 && !unk314) {
                soloFx = true;
            }

            if (force_guitar_fx.Int(0) > 0) {
                soloFx = true;
                deploying = true;
                mFxPos = force_guitar_fx.Int(0) - 1;
            }

            // (Wii-only mFxPos->FXCore cache removed — retail Xbox has no unk3a0)

            if (TheGame->mProperties.mEnableWhammy) {
                int fxBank = 4;
                if (mFxPos >= 0) {
                    fxBank = mFxPos;
                }
                hasHeld = HasAnyActiveHeldNotes();
                float whammyBar = mController->GetWhammyBar();
                mGuitarFx->Poll(
                    fxBank, deploying, soloFx, tempo, beatPhase, whammyBar, hasHeld, unk33d
                );
            }

            // (Wii-only guitar-FX-core reverb/SetFX routing removed — retail Xbox
            //  has no unk39c/unk3a4/unk3a8 members; it uses mPitchShift instead.)
        }

        if (mKeysFx && !TheGame->mProperties.mDisableKeysFx) {
            beat = TheSongDB->GetData()->GetBeatMap()->Beat(tick);
            beat *= 0.5f;
            beatPhase = beat - (float)std::floor(beat);

            bool deploying = IsDeployingBandEnergy();
            bool soloFx = false;
            if (unk315 && !unk314) {
                soloFx = true;
            }

            if (force_keys_fx.Int(0) > 0) {
                soloFx = true;
                deploying = true;
            }

            if (TheGame->mProperties.mEnableCapstrip) {
                float capstrip = mController->GetCapStrip();
                if (!HasAnyActiveHeldNotes()) {
                    capstrip = 0.0f;
                }
                mKeysFx->Poll(deploying, soloFx, tempo, beatPhase, capstrip);
            }
        }

        mStatCollector.Poll(ms);
    }
}

void GemPlayer::Restart(bool b1) {
    if (!b1) {
        if (unk315 && TheGame->mProperties.mHasSongSections) {
            BandTrack *trk = GetBandTrack();
            if (trk)
                trk->SoloHide();
            if (mTrack && mTrack->GetGemManager()) {
                mTrack->GetGemManager()->SetupGems(0);
            }
        }
        JumpReset(0);
    }
    UpdateGameCymbalLanes();
    unk390 = 0;
    unk394 = 0;
    unk398 = 0;
    if (mDrumSlotWeights) {
        mDrumSlotWeightMapping = mDrumSlotWeights->FindSym("default_weights");
    }
    FinishAllHeldNotes(0);
    mLastTimeWhammyVelWasHigh = -10000.0f;
    if (!b1) {
        mMatcher->Jump(0);
        mGemStatus->Clear();
        unk3dc = -1;
    } else {
        unk3dc = -1;
        for (int i = 0; i < (int)mGemStatus->mGems.size(); i++) {
            if (mGemStatus->Get0xD(i)) {
                mGemStatus->Set0x40(i);
            } else if (unk3dc == -1) {
                unk3dc = i;
                break;
            }
        }
        SetAutoOn(true);
    }
    mMatcher->Enable(true);
    mMatcher->Restart();
    SetAutoOn(false);
    ResetController(true);
    if (!TheGameMode->InMode("practice")) {
        SetReverb(false);
    }
    if (mUser->GetTrackType() == kTrackDrum && !TheGame->InDrumTrainer()) {
        DisableFills();
    }
    if (mTrack)
        mTrack->SetInCoda(false);
    SetGuitarFx();
    mAnnoyingMode = false;
    unk3b9 = false;
    unk3ac = 0;
    unk3bc = 0;
    unk3c0 = 0;
    mFirstGemMs = 0;
    unk3e0 = false;
    unk3e1 = false;
    mSectionStartHitCount = 0;
    mSectionStartMissCount = 0;
    mSectionStartScore = 0;
    mSustainsReleased = 0;
    unk404 = -1;
    mStatCollector.Reset();
    unk408 = TheModifierMgr->IsModifierActive("mod_miss_sounds");
    if (GetUser()->IsLocal()) {
        JoypadKeepAlive(GetUser()->GetLocalUser()->GetPadNum(), true);
    }
    Player::Restart(b1);
}

void GemPlayer::JumpReset(float f1) {
    SetFilling(false, MsToTickInt(f1));
    mBeatMaster->GetAudio()->RestoreDrums(mTrackNum);
    unk310 = -1;
    mCodaPoints = 0;
    for (int i = 0; i < 6; i++) {
        mLastCodaSwing[i] = 0;
    }
    FinishAllHeldNotes(f1);
    mIsInCoda = false;
    unk315 = false;
    unk316 = false;
    unk314 = false;
    mNumFillSwings = -1;
    mLastFillHitTick = -1;
    unk2f4 = -1;
    mUpcomingFretReleases.clear();
}

void GemPlayer::SetTrack(int track) {
    SetTrackNum(track);
    mMatcher->SetTrack(track);
    if (IsNet()) {
        mBeatMaster->GetAudio()->SetRemoteTrack(track);
    }
}

void GemPlayer::PostLoad(bool b1) {
    ResetController(true);
    mController->Disable(true);
    SetReverb(false);
    mGemStatus->Resize(TheSongDB->GetTotalGems(mTrackNum));
    if (TheGame->mProperties.mEnableCoda) {
        mMatcher->SetCodaStartTick(TheSongDB->GetCodaStartTick());
    }
    if (mGuitarFx && !TheGame->mProperties.mDisableGuitarFx) {
        mGuitarFx->PostLoad();
        std::list<int> chans;
        mBeatMaster->GetAudio()->FillChannelList(chans, mTrackNum);
        Stream *stream = mBeatMaster->GetAudio()->GetSongStream();
        FOREACH (it, chans) {
            FxSend *send = mGuitarFx->GetFxSend();
            stream->SetFXSend(*it, send);
        }
    }
    if (mKeysFx && !TheGame->mProperties.mDisableKeysFx) {
        mKeysFx->PostLoad();
        std::list<int> chans;
        mBeatMaster->GetAudio()->FillChannelList(chans, mTrackNum);
        Stream *stream = mBeatMaster->GetAudio()->GetSongStream();
        FOREACH (it, chans) {
            FxSend *send = mKeysFx->GetFxSend();
            stream->SetFXSend(*it, send);
        }
    }

    if (TheGame->InPracticeMode() || TheGame->InTrainer()) {
        Stream *stream = mBeatMaster->GetAudio()->GetSongStream();
        for (int i = 0; i < (unsigned int)stream->GetNumChanParams(); i++) {
            stream->SetFXSend(i, GetPitchShift());
        }
    }
}

bool GemPlayer::IsReady() const { return mMatcher->IsReady(); }

FxSendPitchShift *GemPlayer::GetPitchShift() {
    if (!mPitchShift) {
        mPitchShift = TheSynth->CreatePitchShift(1, kSendStereo);
    }
    return mPitchShift;
}

void GemPlayer::Start() {
    HookupTrack();
    mMatcher->Start();
    if (mEnabledState == kPlayerEnabled) {
        mController->Disable(false);
    }
    SetAutoplay(IsLocal() ? mUser->mAutoplay : true);
}

void GemPlayer::PollTrack() {
    if (mTrack) {
        PlayerState state;
        GetPlayerState(state);
        mTrack->SetPlayerState(state);
    }
}

void GemPlayer::PollAudio() {}

void GemPlayer::SetPaused(bool b1) {
    if (b1) {
        mController->Disable(true);
    } else if (mEnabledState == kPlayerEnabled) {
        ResetController(false);
        BeatMatchController *ctrl = mController;
        ctrl->unk25 = true;
        ctrl->Disable(true);
        unk388 = true;
    } else
        ResetController(true);

    if (GetUser()->IsLocal()) {
        JoypadKeepAlive(GetUser()->GetLocalUser()->GetPadNum(), !b1);
    }
}

void GemPlayer::UpdateLeftyFlip() {
    if (mTrack) {
        mTrack->UpdateLeftyFlip();
    }
    ResetController(mController->IsDisabled());
}

void GemPlayer::SetRealtime(bool) {}

void GemPlayer::SetMusicSpeed(float f1) {
    mBeatMaster->GetAudio()->GetSongStream()->SetSpeed(f1);
}

void GemPlayer::Jump(float f1, bool b2) {
    JumpReset(f1);
    if (mTrack)
        mTrack->Jump(f1);
    if (b2) {
        mMatcher->Jump(f1);
    }
}

void GemPlayer::SetAutoplay(bool b) {
    GetUser()->SetAutoplay(b);
    mMatcher->SetAutoplay(b);
}

bool GemPlayer::IsAutoplay() const { return mMatcher->IsAutoplay(); }
void GemPlayer::SetAutoOn(bool on) { mMatcher->SetAutoOn(on); }

void GemPlayer::HookupTrack() {
    if (!mTrack && GetTrackPanel()->GetState() == UIPanel::kUp) {
        mTrack = dynamic_cast<GemTrack *>(GetUser()->GetTrack());
        MILO_ASSERT(mTrack, 0x890);
        mTrack->SetInCoda(false);
        mTrack->GetTrackDir()->SetMaxMultiplier(mBehavior->GetMaxMultiplier());
        UpdateLeftyFlip();
    }
}

void GemPlayer::UnHookTrack() { mTrack = nullptr; }

void GemPlayer::EnableFills(float f1, bool b2) {
    if (mMatcher) {
        mMatcher->SetFillsEnabled(MsToTickInt(f1 + GetSongMs()), b2);
    }
}

void GemPlayer::DisableFills() {
    if (mMatcher)
        mMatcher->SetFillsEnabled(false);
}

void GemPlayer::DisableFillsCompletely() {
    if (mMatcher)
        mMatcher->DisableFillsCompletely();
}

void GemPlayer::EnableDrumFills(bool b1) {
    if (b1) {
        if (mTrack && mTrack->GetTrackDir()) {
            EnableFills(mTrack->GetTrackDir()->TopSeconds() * 1000.0f, false);
        }
    } else
        DisableFills();
}

bool GemPlayer::FillsEnabled(int i1) {
    bool ret = true;
    if (!TheSongDB->IsInCoda(i1)) {
        bool fills = false;
        if (mMatcher && mMatcher->FillsEnabled(i1))
            fills = true;
        if (!fills)
            ret = false;
    }
    return ret;
}

void GemPlayer::EnterCoda() {
    if (!mIsInCoda) {
        Player::EnterCoda();
        if (TheGame->mProperties.mEnableCoda) {
            if (mMatcher) {
                mMatcher->SetFillsEnabled(true);
                mMatcher->EnterCoda();
            }
            PopupHelp("rock_ending", true);
            mTrack->SetInCoda(true);
        }
    }
}

void GemPlayer::ResetCodaPoints() { mCodaPoints = 0; }

void GemPlayer::AddCodaPoints() {
    if (!InRollback()) {
        AddPoints(mCodaPoints, false, false);
        mStats.AddCodaPoints(mCodaPoints);
        mCodaPoints = 0;
    }
}

// enum EnabledState {
//     kPlayerEnabled = 0,
//     kPlayerDisabled = 1,
//     kPlayerBeingSaved = 2,
//     kPlayerDroppingIn = 3,
//     kPlayerDisconnected = 4
// };

void GemPlayer::LocalSetEnabledState(EnabledState state, int i2, BandUser *user, bool b4) {
    Player::LocalSetEnabledState(state, i2, user, b4);
    if (state - 3 > 1U) {
        if (state != kPlayerEnabled) {
            if (state != kPlayerDisabled)
                return;
        } else {
            mMatcher->Enable(true);
            mCommonPhraseCapturer->Enabled(this, mTrackNum, i2, true);
            InputReceived();
            SetAutoOn(false);
            return;
        }
    }
    if (TheGamePanel->GetState() != UIPanel::kUnloaded) {
        mMatcher->Enable(false);
        mCommonPhraseCapturer->Enabled(this, mTrackNum, i2, false);
        bool b1 = false;
        if (unk315 && !unk314) {
            b1 = true;
        }
        if (b1)
            unk314 = true;
        if (state == kPlayerDroppingIn) {
            SetAutoOn(true);
        }
    }
}

void GemPlayer::SetSyncOffset(float f1) {
    mSyncOffset = f1;
    mMatcher->SetSyncOffset(f1);
}

void GemPlayer::EnableSwings(bool b1) {
    MILO_ASSERT(mMatcher, 0x926);
    mMatcher->EnableController(b1);
}

void GemPlayer::PlayDrum(int i1, int i2, float f3, int i4) {
    if (mTrack) {
        mTrack->FillHit(i2, i4);
        mMatcher->PlayDrum(i1, false, f3);
    }
}

void GemPlayer::SetDrumKitBank(ObjectDir *bank) {
    MILO_ASSERT(bank, 0x937);
    mMatcher->SetDrumKitBank(bank);
}

DECOMP_FORCEACTIVE(GemPlayer, "seq_list")

void GemPlayer::ResetGemStates(float f1) {
    mMatcher->ResetGemStates(f1);
    mGemStatus->Clear();
}

void GemPlayer::ChangeDifficulty(Difficulty diff) {
    float ms = PollMs();
    int tick = MsToTickInt(ms);
    FinishAllHeldNotes(ms);
    Player::ChangeDifficulty(diff);
    TheSongDB->ClearTrackPhrases(mTrackNum);
    TheSongDB->ChangeDifficulty(mTrackNum, diff);
    TheSongDB->ClearQuarantinedPhrases(mTrackNum);
    ResetGemStates(ms);
    if (mTrack)
        mTrack->ChangeDifficulty(diff, tick);
    mGemStatus->Clear();
    mGemStatus->Resize(TheSongDB->GetTotalGems(mTrackNum));
    IgnoreGemsUntil(tick);
    DisableFillsCompletely();
    BandTrack *trk = GetBandTrack();
    if (trk)
        trk->ResetPlayerFeedback();
    if (TheGame->InTrainer()) {
        TheSongDB->SetupTrackPhrases(mTrackNum);
        TheGemTrainerPanel->RestartSection();
    }
}

void GemPlayer::SetPitchShiftRatio(float f1) {
    MILO_ASSERT(mBeatMaster, 0x975);
    MILO_ASSERT(mBeatMaster->GetAudio(), 0x976);
    MILO_ASSERT(mBeatMaster->GetAudio()->GetSongStream(), 0x977);
    Stream *stream = mBeatMaster->GetAudio()->GetSongStream();
    StandardStream *sStream = dynamic_cast<StandardStream *>(stream);
    if (sStream) {
        std::list<int> chans;
        mBeatMaster->GetAudio()->FillChannelList(chans, mTrackNum);
        FOREACH (it, chans) {
            // sStream->SetPitchShift(*it, true); // Wii-only API
        }
    }
    if (f1 == 1.0f || ThePracticePanel->PlayAllTracks()) {
        for (int i = 0; i < (unsigned int)stream->GetNumChanParams(); i++) {
            stream->SetVolume(i, 0);
        }
    } else {
        std::list<int> chans;
        mBeatMaster->GetAudio()->FillChannelList(chans, mTrackNum);
        for (unsigned int i = 0; i < stream->GetNumChanParams(); i++) {
            if (std::find(chans.begin(), chans.end(), i) != chans.end()) {
                stream->SetVolume(i, 0);
            } else
                stream->SetVolume(i, -96.0f);
        }
    }
}

DECOMP_FORCEACTIVE(GemPlayer, "cymbalGemCount >= 0")

float GemPlayer::CycleAutoplayAccuracy() {
    if (mMatcher)
        return mMatcher->CycleAutoplayAccuracy();
    else
        return 0;
}

void GemPlayer::SetAutoplayAccuracy(float f1) {
    if (mMatcher)
        mMatcher->SetAutoplayAccuracy(f1);
}

bool GemPlayer::HasDealtWithGem(int idx) { return mGemStatus->HasDealtWithGem(idx); }
int GemPlayer::GetMaxSlots() const { return mMatcher->GetMaxSlots(); }

float GemPlayer::GetNotesHitFraction(bool *bp) const {
    return mGemStatus->GetNotesHitFraction(bp);
}

void GemPlayer::FinalizeStats() {
    if (mTrackType == 1) {
        mStats.mHighFretGemCount = TheSongDB->GetSoloGemCount(mTrackNum);
    }
    if (mTrackType - 1U <= 1) {
        mStats.mSustainGemCount = TheSongDB->GetSustainGemCount(mTrackNum);
    }
    RecordTrillStats();
    UpdateSectionStats();
    Player::FinalizeStats();
}

int GemPlayer::GetNumRolls() const {
    SongData *data = TheSongDB->GetData();
    int diff = data->TrackDiffAt(mTrackNum);
    return data->GetRollInfo(mTrackNum)->SizeAt(diff);
}

void GemPlayer::GetRollInfo(int i1, int &startTick, int &endTick) const {
    SongData *data = TheSongDB->GetData();
    int diff = data->TrackDiffAt(mTrackNum);
    data->GetRollInfo(mTrackNum)->GetNthStartEnd(diff, i1, startTick, endTick);
}

int GemPlayer::GetNumTrills() const {
    SongData *data = TheSongDB->GetData();
    int diff = data->TrackDiffAt(mTrackNum);
    return data->GetTrillInfo(mTrackNum)->SizeAt(diff);
}

void GemPlayer::GetTrillInfo(int i1, int &startTick, int &endTick) const {
    SongData *data = TheSongDB->GetData();
    int diff = data->TrackDiffAt(mTrackNum);
    data->GetTrillInfo(mTrackNum)->GetNthStartEnd(diff, i1, startTick, endTick);
}

void GemPlayer::RecordTrillStats() {
    if (mTrackType != kTrackGuitar && mTrackType != kTrackBass)
        return;
    int num = GetNumTrills();
    int i3 = 0;
    for (int i = 0; i < num; i++) {
        int start = 0;
        int end = 0;
        GetTrillInfo(i, start, end);
        int i24 = 0;
        int i28 = 0;
        GetGemsHit(start, end, i24, i28);
        if (i28 == 0)
            i3++;
        else if (i24 > 0) {
            mStats.IncrementTrillsHit(i24 == i28);
        }
    }
    mStats.SetTrillCount(num - i3);
}

void GemPlayer::GetGemsHit(int startTick, int endTick, int &gemsHit, int &gemsTotal) {
    const std::vector<GameGem> &gems = TheSongDB->GetGems(mTrackNum);
    for (int i = 0; i < gems.size(); i++) {
        int gemTick = gems[i].GetTick();
        if (gemTick > endTick)
            break;
        if (startTick <= gemTick && gemTick < endTick) {
            if (!mGemStatus->GetIgnored(i)) {
                gemsTotal++;
                if (mGemStatus->GetHit(i))
                    gemsHit++;
            }
        }
    }
}

void GemPlayer::UpdateSectionStats() {
    int diff = mGemStatus->NumHits() - mSectionStartHitCount;
    float scorediff = mScore - mSectionStartScore;
    float f1 = diff + (mGemStatus->NumMisses() - mSectionStartMissCount);
    if (f1 > 0) {
        f1 = diff / f1;
    } else
        f1 = -1.0f;
    Player::UpdateSectionStats(f1, scorediff);
}

void GemPlayer::HandleNewSection(const PracticeSection &s, int i1, int i2) {
    if (!InRollback()) {
        mSectionStartHitCount = mGemStatus->NumHits();
        mSectionStartMissCount = mGemStatus->NumMisses();
        mSectionStartScore = mScore;
        Player::HandleNewSection(s, i1, i2);
    }
}

void GemPlayer::OnResetCodaPoints() { mCodaPoints = 0; }

int GemPlayer::OnGetPercentHit() const {
    int total = TheSongDB->GetTotalGems(mTrackNum);
    return ((float)mStats.mHitCount / total) * 100.0f;
}

float GemPlayer::OnGetPercentHitGemsPractice(int i1, float f2, float f3) const {
    const std::vector<GameGem> &gems = TheSongDB->GetGems(mTrackNum);
    int i9 = 0;
    int i8 = 0;
    for (int i = 0; i < gems.size(); i++) {
        if (gems[i].PlayableBy(i1)) {
            float f11 = gems[i].GetMs();
            float f10;
            if (gems[i].IgnoreDuration())
                f10 = f11;
            else
                f10 = f11 + gems[i].DurationMs();
            if (f11 >= f2 && f10 < f3) {
                i9++;
                if (mGemStatus->GetHit(i)) {
                    i8++;
                }
            }
        }
    }
    float ret;
    if (i9 == 0)
        ret = 0;
    else {
        float diff = (float)i8 / (float)i9;
        ret = diff * 100.0f;
    }
    return ret;
}

int GemPlayer::OnGetGemResult(int idx) { return mGemStatus->GetHit(idx); }

bool GemPlayer::OnGetGemIsSustained(int idx) {
    std::vector<GameGem> gems = TheSongDB->GetGems(mTrackNum);
    return !gems[idx].IgnoreDuration();
}

void GemPlayer::OnSetWhammyOverdriveEnabled(bool b) { mWhammyOverdriveEnabled = b; }
void GemPlayer::OnSetMercurySwitchEnabled(bool b) { mMercurySwitchEnabled = b; }

void GemPlayer::OnGameOver() {
    if (!MetaPerformer::Current()->SongEndsWithEndgameSequence()) {
        mController->Disable(true);
    }
}

void GemPlayer::OnDisableController() {
    if (mController)
        mController->Disable(true);
    if (GetUser()->IsLocal()) {
        JoypadKeepAlive(GetUser()->GetLocalUser()->GetPadNum(), false);
    }
}

void GemPlayer::Rollback(float f1, float f2) {
    if (mTrack && mMatcher) {
        bool _cond = !InRollback();
        if (_cond) {
            unk3e0 = mFill;
        }
        FinishAllHeldNotes(f2);
        unk3dc = -1;
        for (int i = 0; i < (int)mGemStatus->mGems.size(); i++) {
            if (mGemStatus->Get0xD(i)) {
                mGemStatus->Set0x40(i);
            } else if (unk3dc == -1) {
                unk3dc = i;
                break;
            }
        }
        mTrack->RebuildBeats();
        mTrack->Jump(f1);
        int startTick = mMatcher->GetFillStartTick();
        mMatcher->Jump(f2);
        SetAutoOn(true);

        if (mIsInCoda) {
            FillExtent ext(0, 0, false);
            auto _tmp0 = MsToTickInt(f1);
            TheSongDB->GetData()->GetFillInfo(mTrackNum)->FillAt(
                _tmp0, ext, false
            );
            float f6 = TickToMs(ext.start) - f2;
            EnableFills(f6, true);
            mTrack->ResetFills(true);
        } else {
            if (mTrackType == kTrackDrum && CanDeployOverdrive()) {
                bool u3 = false;
                float f6;
                if (unk3e0) {
                    FillExtent ext(0, 0, false);
                    TheSongDB->GetData()->GetFillInfo(mTrackNum)->FillAt(
                        MsToTickInt(f1), ext, false
                    );
                    f6 = TickToMs(ext.start) - f2;
                    u3 = true;
                } else {
                    if (startTick != INT_MAX) {
                        f6 = TickToMs(startTick);
                    } else
                        f6 = 0;
                    f6 = Max(f1 - f2, f6 - f2);
                }
                EnableFills(f6, u3);
                mTrack->ResetFills(true);
            }
        }

        mTrack->RedrawTrackElements(TheTaskMgr.Seconds(TaskMgr::kRealTime) * 1000.0f);
        mLastFillHitTick = -1;
        unk2f4 = -1;
        Player::Rollback(f1, f2);
    }
}

void GemPlayer::HandleFirstGemAfterRollback(int i1) {
    if (unk3dc != -1 && i1 >= unk3dc && !mDisconnectedAtStart) {
        SetAutoOn(false);
        unk3dc = -1;
    }
}

bool GemPlayer::PastFinalNote() const { return unk3e1 && !HasAnyActiveHeldNotes(); }

void GemPlayer::OnRemoteHit(int i1, int i2, float f3) {
    SetAutoplay(true);
    mMatcher->AutoplayCoda(false);
    SetRemoteStreak(i1);
    UpdateScore(i2);
    RemoteUpdateCrowd(f3);
}

void GemPlayer::OnRemotePenalize(int i1, int i2, float f3) {
    SetAutoplay(false);
    SetRemoteStreak(i1);
    UpdateScore(i2);
    RemoteUpdateCrowd(f3);
}

void GemPlayer::OnRemoteFill(bool) {}

void GemPlayer::OnRemoteCodaHit(int i1, int i2) {
    mCodaPoints = i1;
    BeatMaster *beatMaster = mBeatMaster;
    BeatMatcher *matcher = mMatcher;
    beatMaster->GetAudio()->FillSwing(mTrackNum, 0, i2, (int)matcher->mSongPos.GetTotalTick(), true);
}

void GemPlayer::OnRemoteWhammy(float f1) {
    unk358 = f1;
    mMatcher->SetPitchBend(mTrackNum, f1, true);
}

void GemPlayer::OnRemoteHitLastCodaGem(int i1) {
    mCodaPoints = i1;
    mBand->LocalFinishedCoda(this);
}

void GemPlayer::OnRemoteBlowCoda() { mBand->LocalBlowCoda(this); }
void GemPlayer::OnStartOverdrive() { SetReverb(true); }
void GemPlayer::OnStopOverdrive() { SetReverb(false); }

void GemPlayer::OnRefreshTrackButtons() {
    if (mTrack) {
        MILO_ASSERT(mController, 0xB9C);
        int btns = mController->GetFretButtons();
        for (int i = 0; i < 5; i++) {
            mTrack->SetFretButtonPressed(i, btns & (1 << i));
        }
    }
}

bool GemPlayer::InFillNow() {
    int tick = GetSongPos().GetTotalTick();
    return mUser->GetTrackType() == kTrackDrum
        && (tick <= mLastFillHitTick || mMatcher->InFillNow());
}

bool GemPlayer::InIgnorableFill(int i1) {
    FillLogic logic = TheGame->GetFillLogic();
    return (
        (logic == kFillsDeployGemAndDim || logic == kFillsDeployGemAndInvisible)
        && mMatcher->FillsEnabled(i1) && mMatcher->InFill(i1, true)
    );
}

bool GemPlayer::IgnoreGemsAt(int i1) {
    FillLogic logic = TheGame->GetFillLogic();
    return (
        (logic == kFillsDeployGemAndDim || logic == kFillsDeployGemAndInvisible)
        && mMatcher->IsFillCompletion(i1)
    );
}

void GemPlayer::SetReverb(bool b1) {
    mBeatMaster->GetAudio()->SetFX(mTrackNum, kFXCore1, b1);
}

void GemPlayer::ResetController(bool b1) {
    Symbol controller = TheGameConfig->GetController(GetUser());
    GameplayOptions *options = mUser->GetGameplayOptions();
    MILO_ASSERT(options, 0xBCF);
    bool leftyOptions = options->GetLefty();
    if (controller != mControllerType || leftyOptions != mController->IsLefty()) {
        bool alt = mController ? mController->IsAlternateMapping() : false;
        RELEASE(mController);
        DataArray *cfg = SystemConfig("beatmatcher", "controller", controller);
        BandUser *user = TheBandUserMgr->GetBandUser(GetUserGuid(), true);
        mController = NewController(
            user, cfg, mMatcher, b1, leftyOptions, mMatcher->GetTrackType(mMatcher->CurrentTrack())
        );
        if (GetUser()->IsLocal()) {
            if (GetUser()->GetLocalBandUser()->GetPadNum() == 0) {
                mController->SetHitSink(TheGamePanel->GetHitTracker());
            }
        }
        mControllerType = controller;
        mMatcher->SetControllerType(mControllerType);
        mController->SetMapping((BeatMatchControllerGemMapping)(mUser->GetTrackType()
                                                                == kTrackDrum));
        mController->UseAlternateMapping(alt);
    } else {
        mController->Disable(b1);
    }
    mController->SetSecondPedalHiHat(TheProfileMgr.GetSecondPedalHiHat());
}

void GemPlayer::GetPlayerState(PlayerState &state) const {
    int streak = mStats.GetCurrentStreak();
    float whammy = unk358;
    state.warning = IsInCrowdWarning();
    state.overdriveReady = false;
    state.whammy = whammy;
    state.whammyActive = false;
    state.phraseState = kPhraseNone;
    state.fillState = 0;
    state.streak = streak;
}

void GemPlayer::UpdateCrowdMeter(float noteScore, int gem_id) {
    if (IsNet())
        return;
    MILO_ASSERT(noteScore >= 0 && noteScore <= 1, 0xC09);
    if (mCrowd->mActive && !mFill) {
        float multiplier = 1.0f;
        bool inPhrase = TheSongDB->IsInPhrase(kCommonPhrase, mTrackNum, gem_id);
        if (mDrumSlotWeights) {
            const GameGem &gem = TheSongDB->GetGem(mTrackNum, gem_id);
            int slot = gem.GetSlot();
            const TickedInfoCollection<String> &submixes =
                TheSongDB->GetData()->GetSubmixes(mTrackNum);
            const char *mappingStr = mDrumSlotWeightMapping.Str();
            if (submixes.mInfos.size() != 0) {
                int gemTick = gem.GetTick();
                const TickedInfo<String> *it = std::upper_bound(
                    submixes.mInfos.begin(),
                    submixes.mInfos.end(),
                    gemTick,
                    TickedInfoCollection<String>::Cmp
                );
                if (it != submixes.mInfos.begin()) {
                    --it;
                }
                mappingStr = it->mInfo.c_str();
            }
            const DataArray *arr = mDrumSlotWeights->FindArray(Symbol(mappingStr), false);
            float weight;
            if (arr) {
                const DataArray *inner = arr->Node(1).Array(arr);
                weight = inner->Node(slot).Float(inner);
            } else {
                weight = 1.0f;
            }
            multiplier *= weight;
        }
        if (noteScore > mCrowd->mRawValue) {
            float reward = GetCrowdBoost();
            bool isSoloMod = unk315 && !unk314;
            if (isSoloMod) {
                Symbol trackSym = mUser->GetTrackSym();
                reward *= TheScoring->GetSoloGemReward(trackSym);
            } else if (inPhrase) {
                reward = reward * TheScoring->mCommonPhraseReward;
            }
            multiplier = reward;
        } else {
            bool isSoloMod = unk315 && !unk314;
            if (isSoloMod) {
                Symbol trackSym = mUser->GetTrackSym();
                multiplier *= TheScoring->GetSoloGemPenalty(trackSym);
            } else if (inPhrase) {
                multiplier *= TheScoring->mCommonPhrasePenalty;
            }
        }
        mCrowd->Update(noteScore, multiplier);
        CheckCrowdFailure();
    }
    if (unk1ff) {
        HandleType(send_update_crowd_msg);
    }
}

#define kMaxTrackSlots 6

void GemPlayer::FillInProgress(int i1, int slot) {
    if (mIsInCoda && i1 >= 0) {
        if (!InRollback()) {
            float ms = PollMs();
            if (slot == -1)
                return;
            MILO_ASSERT(0 <= slot && slot < kMaxTrackSlots, 0xC48);
            float f1 = mCodaPointRate;
            if (!mBehavior->GetRequireAllCodaLanes()) {
                slot = 0;
                f1 *= 5.0f;
            }
            float diff = ms - mLastCodaSwing[slot];
            int oldPts = mCodaPoints;
            int i2 =
                (f1 * std::min(mCodaMashPeriod, diff)) / 1000.0f;
            mCodaPoints = oldPts + i2;
            mLastCodaSwing[slot] = ms;
        }
        static Message msg("send_coda_hit", 0, 0);
        msg[0] = mCodaPoints;
        msg[1] = slot;
        HandleType(msg);
    } else {
        int tick = mSongPos.GetTotalTick();
        SetFilling(i1 > 0, tick);
    }
}

void GemPlayer::SendPenalize() {
    if (IsLocal()) {
        static Message msg("send_penalize", 0, 0, 0);
        msg[0] = mStats.GetCurrentStreak();
        msg[1] = (int)mScore;
        msg[2] = mCrowd->GetDisplayValue();
        HandleType(msg);
    }
}

void GemPlayer::Penalize(float f1, int i2, float f3) {
    if (mIsInCoda)
        SendPenalize();
    else {
        unk1fe = false;
        unk1fd = false;
        unk1ff = false;
        FinalizeStats();
        UpdateCrowdMeter(f3, i2);
        unk3c0++;
        int gemId = i2;
        if (i2 != -1
            && TheSongDB->GetGem(mTrackNum, i2).GetMs() - (f1 + mSyncOffset) > 200.0f)
            gemId = i2 - 1;
        HandleCommonPhraseNote(0, gemId);
        FinishAllHeldNotes(f1);
        SendPenalize();
        unk1fe = true;
        unk1fd = true;
        unk1ff = true;
        if (mBehavior->GetHasSolos()) {
            HandleSoloGem(i2, false, f1, false);
        }
    }
}

bool GemPlayer::ShouldPenalizeGem(int gem) const {
    if (mTrackType != 4 && mTrackType != 5)
        return true;
    const std::vector<GameGem> &gems = TheSongDB->GetGems(mTrackNum);
    int tick = gems[gem].GetTick();
    int startGemID;
    for (startGemID = gem; startGemID >= 0 && tick == gems[startGemID].GetTick();
         startGemID--)
        ;
    int endGemID;
    for (endGemID = gem; endGemID < gems.size() && tick == gems[endGemID].GetTick();
         endGemID++)
        ;
    startGemID++;
    if (endGemID - startGemID == 1) {
        MILO_ASSERT(startGemID == gem, 0xCCD);
        MILO_ASSERT(endGemID - 1 == gem, 0xCCE);
        return true;
    } else {
        for (; startGemID < endGemID; startGemID++) {
            if (mGemStatus->GetHit(startGemID) || mGemStatus->Get0x4(startGemID)) {
                return false;
            }
        }
    }
    return true;
}

void GemPlayer::CheckHeldNotes(float f1) {
    FOREACH (it, mHeldNotes) {
        if (it->HasGem()) {
            if (!mGemStatus->GetIgnored(it->unk_0x4)) {
                float max = std::max<float>(0, it->SetHoldTime(f1));
                AddPoints(max, true, true);
                mStats.AddSustain(max);
            }
            if (it->IsDone()) {
                if (IsAutoplay()) {
                    unsigned int slots = it->GetGemSlots();
                    int i = 0;
                    while (slots != 0) {
                        int mask = 1 << i;
                        if (slots & mask) {
                            slots -= mask;
                            FretButtonUp(i, f1);
                        }
                        i++;
                    }
                }
                FinishHeldNote(f1, *it);
            }
        }
    }
}

void GemPlayer::FinishHeldNote(float f1, HeldNote &note) {
    if (note.HasGem()) {
        mSustainHeld = 1;
        PopupHelp(hold_note, false);
        if (mTrack) {
            mTrack->ReleaseGem(f1, note.unk_0x4);
        }
        float frac = note.GetPointFraction();
        Message msg("held_note_released_callback", frac);
        Export(msg, false);
        UpdateCrowdMeter(frac, note.unk_0x4);
        if (unk348) {
            Handle(whammy_end_msg, false);
        }
        unk348 = false;
        PrintFinishHeldNote();
        mStats.IncrementSustainGemsHit(note.HeldCompletely());
        note = HeldNote();
    }
}

void GemPlayer::FinishAllHeldNotes(float f1) {
    FOREACH (it, mHeldNotes) {
        FinishHeldNote(f1, *it);
    }
}

HeldNote &GemPlayer::GetUnusedHeldNote() {
    FOREACH (it, mHeldNotes) {
        if (!it->HasGem())
            return *it;
    }
    MILO_FAIL("All held notes are used! How can this be?!");
    return mHeldNotes.front();
}

HeldNote *GemPlayer::FindHeldNoteFromSlot(int slot) {
    FOREACH (it, mHeldNotes) {
        if (it->GetGemSlots() & (1 << slot))
            return it;
    }
    return nullptr;
}

HeldNote *GemPlayer::FindHeldNoteFromGemID(int gemID) {
    FOREACH (it, mHeldNotes) {
        if (gemID == it->unk_0x4)
            return it;
    }
    return nullptr;
}

HeldNote *GemPlayer::FindFirstActiveHeldNote() {
    FOREACH (it, mHeldNotes) {
        if (it->HasGem())
            return it;
    }
    return nullptr;
}

bool GemPlayer::HasAnyActiveHeldNotes() const {
    FOREACH (it, mHeldNotes) {
        if (it->HasGem())
            return true;
    }
    return false;
}

void GemPlayer::AddHeadPoints(float f1, int i2, int i3, GemHitFlags flags) {
    const GameGem &gem = TheSongDB->GetGem(mTrackNum, i2);
    int i5 = 0;
    if (mGameCymbalLanes != 0) {
        MILO_ASSERT(mUser->GetTrackType() == kTrackDrum, 0xE43);
        const DataNode &node = mDrumCymbalPointBonus->Node(gem.GetSlot() + 1);
        if (node.Type() == kDataInt)
            i5 = node.Int();
        else if (node.Type() == kDataArray) {
            unsigned int gemSlots = gem.GetSlots();
            MILO_ASSERT(GameGem::CountBitsInSlotType( gemSlots ) == 1, 0xE58);
            if (gemSlots & mGameCymbalLanes) {
                i5 = node.Array()->Int(0);
            } else
                i5 = node.Array()->Int(1);
        } else
            MILO_WARN("Unknown data in scoring.dta for pro_drum_bonus.");
    }
    int ivar2;
    if (gem.NumSlots() == 1) {
        ivar2 = i3 * TheScoring->GetHeadPoints(mUser->GetTrackType());
    } else {
        ivar2 = TheScoring->GetChordPoints(mUser->GetTrackType());
        if (ivar2 < 0) {
            ivar2 = i3 * TheScoring->GetHeadPoints(mUser->GetTrackType());
        }
    }
    ivar2 += i5;
    AddPoints(ivar2, true, true);
    mStats.AddAccuracy(ivar2);
    int rounded = Round(gem.GetMs() - (f1 + mSyncOffset));
    unk390 -= rounded;
    unk394 += rounded;
    unk398++;
    PrintAddHead(rounded, i3, ivar2, unk394 / unk398, unk390 + 0.5);
}

void GemPlayer::SetFilling(bool b1, int i2) {
    if (mFill != b1) {
        if (IsLocal()) {
            Message msg("send_fill", b1);
            HandleType(msg);
        }
        mFill = b1;
        if (mFill) {
            if (mUseFills && Performer::IsLocal() && TheGame->DrumFillsMod()) {
                mBeatMaster->GetAudio()->FadeOutDrums(mTrackNum);
            }
            if (i2 - unk2f4 > 0x1e0) {
                mNumFillSwings = 0;
            }
        } else {
            unk2f4 = i2;
            if (mUseFills) {
                if (Performer::IsLocal() && TheGame->DrumFillsMod()) {
                    mBeatMaster->GetAudio()->FadeOutDrums(mTrackNum);
                }
                IgnoreGemsUntil(i2 + 0x1E);
            }
        }
    }
}

void GemPlayer::ForceFill(bool force) {
    mForceFill = force;
    mMatcher->ForceFill(force);
}

FORCE_LOCAL_INLINE
bool GemPlayer::ToggleNoFills() { return mMatcher->mNoFills = !mMatcher->mNoFills; }
END_FORCE_LOCAL_INLINE

void GemPlayer::HandleSoloGem(int i1, bool b2, float f3, bool b4) {
    if (Performer::IsLocal() && TheSongDB->IsInPhrase(kSoloPhrase, mTrackNum, i1)) {
        float f4c = 0;
        float f50 = 0;
        int i54 = 0;
        GetSoloData(TheSongDB->GetGems(mTrackNum)[i1].GetTick(), f4c, f50, i54);
        if (!unk314) {
            if (!unk315) {
                if (f4c == 0)
                    return;
                if (unk316)
                    return;
                unk404 = -1;
                LocalSoloStart();
                HandleType(send_solo_start_msg);
            }
            if (b2) {
                unk3d8 &= b4;
                int ivar1 = f4c;
                if (ivar1 != unk404) {
                    LocalSoloHit(ivar1);
                    static Message send_solo_hit("send_solo_hit", 0);
                    send_solo_hit[0] = ivar1;
                    HandleType(send_solo_hit);
                    unk404 = ivar1;
                }
                if (mTrackType == 1) {
                    mStats.IncrementHighFretGemsHit(b4);
                }
            }
            if (mTrackType - 1U <= 1) {
                mStats.SetSoloButtonedSoloPercentage(f50);
            }
        }
    }
}

int GemPlayer::GetRGFret(int x) const { return mController->GetRGFret(x); }

void GemPlayer::LocalSoloStart() {
    if (GetEnabledState() != kPlayerDisconnected) {
        if (!mBand->MainPerformer()->IsGameOver() && !InRollback()) {
            unk315 = true;
            mStats.SetHasSolos(true);
            unk3d8 = 1;
            unk316 = false;
            BandTrack *track = GetBandTrack();
            if (track) {
                track->SoloStart();
            }
        }
    }
}

void GemPlayer::LocalSoloHit(int x) {
    BandTrack *track = GetBandTrack();
    if (track) {
        track->SoloHit(x);
    }
}

void GemPlayer::LocalSoloEnd(int pct, int numGems) {
    int points = 0;
    Symbol awardSym;
    Symbol trackSym = mUser->GetTrackSym();
    TheScoring->GetSoloAward(pct, trackSym, points, awardSym);
    int total = points * numGems;
    mStats.AddSolo(total);
    Symbol awardSymCopy = awardSym;
    GetTrackPanelDir()->SoloEnd(GetBandTrack(), total, awardSymCopy);
    AddBonusPoints(total);
    mStats.UpdateBestSolo(pct);
    if (unk3d8) {
        mStats.m0x41 = true;
        if (pct == 100) {
            mStats.mPerfectSoloWithSoloButtons = true;
        }
    }
    unk315 = false;
    unk316 = false;
    unk3d8 = false;
}

int GemPlayer::GetSoloData(int tick, float &pct, float &solo_pct, int &numGems) {
    pct = 0;
    numGems = 0;
    int startTick;
    int endTick;
    if (!GetPhraseExtents(kSoloPhrase, mTrackNum, tick, startTick, endTick))
        return 0;
    unk316 = true;
    int hit;
    int solo;
    const GameGemList *gemList = TheSongDB->GetGemList(mTrackNum);
    int idx = gemList->ClosestMarkerIdxAtOrAfterTick(startTick);
    hit = 0;
    solo = 0;
    if (idx != -1) {
        const std::vector<GameGem> &gems = TheSongDB->GetGems(mTrackNum);
        for (; (unsigned int)idx < gems.size(); idx++) {
            if (gems[idx].GetTick() >= endTick) break;
            if (unk316) {
                if (!mGemStatus->GetEncountered(idx)) {
                    unk316 = false;
                }
            }
            numGems++;
            if (!mGemStatus->GetHit(idx)) {
                if (!mGemStatus->GetIgnored(idx))
                    continue;
            }
            hit++;
            if (mGemStatus->GetSolo(idx)) {
                solo++;
            }
        }
    }
    if (numGems == 0) {
        pct = 0;
        solo_pct = 0;
    } else {
        pct = (100.0f * (float)hit) / (float)numGems;
        solo_pct = (100.0f * (float)solo) / (float)numGems;
    }
    return 1;
}

void GemPlayer::HandleCommonPhraseNote(int hit, int gem_id) {
    if (TheGame->mProperties.mAllowOverdrivePhrases) {
        mCommonPhraseCapturer->HandlePhraseNote(this, mTrackNum, gem_id, hit != 0);
        if (hit != 0) {
            int phraseId = TheSongDB->GetPhraseID(mTrackNum, gem_id);
            if (phraseId != -1 && GetBandTrack() != nullptr) {
                int trackIdx;
                EndingBonus *bonus = GetTrackPanelDir()->GetEndingBonus();
                trackIdx = GetBandTrack()->mTrackIdx;
                bonus->SetProgress(trackIdx, GetCommonPhraseFraction(phraseId));
            }
        }
    }
}

float GemPlayer::GetCommonPhraseFraction(int tick) {
    int total = 0;
    int hit = 0;
    Extent ext;
    TheSongDB->GetCommonPhraseExtent(mTrackNum, tick, ext);
    int idx = TheSongDB->GetGemList(mTrackNum)->ClosestMarkerIdxAtOrAfterTick(ext.unk0);
    if (idx != -1) {
        const std::vector<GameGem> &gems = TheSongDB->GetGems(mTrackNum);
        for (int n = (int)gems.size() - idx; (unsigned int)idx < gems.size(); n--, idx++) {
            if (gems[idx].GetTick() >= ext.unk4) break;
            total++;
            if (!mGemStatus->GetHit(idx)) {
                if (!mGemStatus->GetIgnored(idx))
                    continue;
            }
            hit++;
        }
    }
    if (total == 0)
        return 0;
    return (float)hit / (float)total;
}

bool GemPlayer::IsCodaMiss(float ms) {
    int codaStartTick = TheSongDB->GetCodaStartTick();
    if (codaStartTick == -1)
        return false;
    SongData *data = TheSongDB->GetData();
    int curTick = (int)GetSongPos().GetTotalTick();
    FillInfo *fillInfo = data->GetDrumFillInfo(mTrackNum);
    FillExtent curExtent(0, 0, false);
    FillExtent codaExtent(0, 0, false);
    bool hasCoda = fillInfo->NextFillExtents(codaStartTick, codaExtent);
    bool hasCur = fillInfo->NextFillExtents(curTick, curExtent);
    if (!hasCoda)
        return true;
    if (hasCur && curExtent.start == codaExtent.start)
        return false;
    if (curTick < codaExtent.end)
        return false;
    FillExtent checkExtent(0, 0, false);
    return !data->GetFillInfo(mTrackNum)->FillAt(curTick - 0xF0, checkExtent, false);
}

void GemPlayer::CheckSolo(float ms) {
    int startTick;
    int endTick;
    int inSolo;
    unsigned inSoloBool;
    float tickF;
    if (!mQuarantined && TheGame->mProperties.mCanSolo &&
        (tickF = MsToTick(ms),
         startTick = 0,
         inSolo = (int)tickF, endTick = 0,
         inSolo = GetPhraseExtents(kSoloPhrase, mTrackNum, inSolo, startTick, endTick),
         inSolo = TheGame->mProperties.mCanSolo & inSolo,
         inSoloBool = (unsigned)(-inSolo | inSolo) >> 31U,
         (inSoloBool != (((unsigned)unk310 >> 31U) ^ 1U)))) {
        if (inSoloBool) {
            if (mEnabledState != kPlayerEnabled) {
                unk314 = true;
            } else if (IsLocal() && !unk315) {
                unk404 = -1;
                LocalSoloStart();
                HandleType(send_solo_start_msg);
            }
            unk315 = true;
            mStats.SetHasSolos(true);
            unk310 = startTick;
            return;
        }
        if (unk316) {
            if (!unk314) {
                SoloEnd();
            }
            unk314 = false;
            unk315 = false;
            unk310 = -1U;
        }
    }
}

void GemPlayer::UpdateGameCymbalLanes() {
    if (mUser->GetTrackType() != kTrackDrum)
        return;
    bool discoUnflip = false;
    bool hasGHDrums = false;
    if (IsLocal()) {
        LocalBandUser *lu = mUser->GetLocalBandUser();
        if (lu && UserHasGHDrums(lu))
            hasGHDrums = true;
    }
    if (hasGHDrums && !mUser->GetGameplayOptions()->GetLefty()) {
        discoUnflip = true;
    }
    SongData *data = TheSongDB->GetData();
    if (data->GetUsingRealDrums()) {
        mGameCymbalLanes = mUser->GetCymbalConfiguration();
        bool forceUseCymbals = TheGame->mProperties.mForceUseCymbals;
        bool forceDontUseCymbals = TheGame->mProperties.mForceDontUseCymbals;
        if (MetaPerformer::Current()->mRealDrumsOverride) {
            mGameCymbalLanes = 0x1C;
        } else if (forceUseCymbals) {
            mGameCymbalLanes = 0x1C;
        } else if (forceDontUseCymbals) {
            mGameCymbalLanes = 0;
        }
        if (mGameCymbalLanes & 4)
            discoUnflip = true;
    } else {
        mGameCymbalLanes = 0;
        discoUnflip = false;
    }
    data->SetUseDiscoUnflip(discoUnflip);
    data->SetGameCymbalLanes(mGameCymbalLanes);
}

void GemPlayer::SoloEnd() {
    if (IsLocal()) {
        if (!InRollback()) {
            float f4c = 0;
            float f50 = 0;
            int i54 = 0;
            GetSoloData(unk310, f4c, f50, i54);
            LocalSoloEnd(f4c, i54);
            static Message send_solo_end("send_solo_end", 0, 0);
            send_solo_end[0] = (int)f4c;
            send_solo_end[1] = i54;
            HandleType(send_solo_end);
        }
    }
}

void GemPlayer::SetGuitarFx() {
    if (IsLocal() && !ThePlatformMgr.GuideShowing()
        && GetEnabledState() != kPlayerDisconnected) {
        int switchpos = TheGameConfig->GetFxSwitchPosition(GetUser()->GetLocalBandUser());
        if (switchpos != -1 && switchpos != mFxPos) {
            LocalSetGuitarFx(switchpos);
            static Message msg("send_guitar_fx", 0);
            msg[0] = switchpos;
            HandleType(msg);
        }
    }
}

void GemPlayer::LocalSetGuitarFx(int i1) {
    int old = mFxPos;
    if (i1 != -1) {
        mFxPos = i1;
    }
    if (mFxPos != old && mTrack && Performer::IsLocal()) {
        mTrack->UpdateEffects(mFxPos);
    }
}

void GemPlayer::SendWhammyBar(float f1) {
    if ((f1 == 0 && unk364 != 0) || (f1 != 0 && unk364 == 0)
        || (std::fabs(f1 - unk364) >= 0.07999999821186066f)) {
        static int x;
        static Message msg("send_whammy", 0.0f);
        msg[0] = f1;
        HandleType(msg);
        unk364 = f1;
    }
}

bool GemPlayer::AllCodaGemsHit() const {
    int startTick = TheSongDB->GetCodaStartTick();
    if (startTick == -1)
        return false;
    else {
        const GameGemList *gems = TheSongDB->GetGemList(mTrackNum);
        int i3 = TheSongDB->GetTotalGems(mTrackNum);
        for (int i = i3 - 1; i >= 0; i--) {
            int tick = gems->GetGem(i).GetTick();
            if (tick < startTick)
                return true;
            if (TheSongDB->GetData()->GetDrumFillInfo(mTrackNum)->FillAt(tick, true)) {
                return true;
            }
            if (!mGemStatus->GetHit(i))
                return false;
        }
        return true;
    }
    return false;
}

int GemPlayer::GetCodaFreestyleExtents(Extent &extent) const {
    int codaStartTick = TheSongDB->GetCodaStartTick();
    if (codaStartTick == -1)
        return false;
    DrumFillInfo *fillInfo = TheSongDB->GetData()->GetDrumFillInfo(mTrackNum);
    if (fillInfo->mFills.empty())
        return false;
    int lastStart = fillInfo->mFills.back().start;
    if (lastStart >= codaStartTick) {
        extent.unk0 = lastStart;
        extent.unk4 = fillInfo->mFills.back().end;
        return true;
    }
    return false;
}

void GemPlayer::CodaHit(float f1, int i2) {
    if (IsLocal()) {
        mBand->DealWithCodaGem(this, i2, true, AllCodaGemsHit());
    }
}

void GemPlayer::PrintMsg(const char *str) {
    const char *prnt = MakeString("%d %s\n", GetScore(), str);
    *mOverlay << prnt;
    TheDebug << prnt;
}

void GemPlayer::PrintFinishHeldNote() {
    if (mOverlay->Showing()) {
        char buffer[5] = { 0 };
        FOREACH (it, mHeldNotes) {
            if (it->HasGem()) {
                int slots = it->GetGem()->NumSlots();
                if (slots > 1) {
                    sprintf(buffer, " X %d", slots);
                }
                float pct = it->GetAwardedPercent();
                PrintMsg(MakeString(
                    "held%s (%.2f) %.2f pts", buffer, pct, it->GetAwardedPoints()
                ));
            }
        }
    }
}

void GemPlayer::PrintAddHead(int mils, int idx, int pts, int ms_avg, int recent) {
    if (mOverlay->Showing()) {
        char id[5] = { 0 };
        if (idx > 1) {
            sprintf(id, " X %d", idx);
        }
        PrintMsg(MakeString(
            "%s (%dms) %d pts -- %dms avg  -- %d recent", id, mils, pts, ms_avg, recent
        ));
    }
}

void GemPlayer::ConfigureBehavior() {
    bool c1 = TheGame->mProperties.mEnableOverdrive;
    bool single = !TheBandUserMgr->IsMultiplayerGame();
    TrackType ty = mUser->GetTrackType();
    mBehavior->SetMaxMultiplier(ty == kTrackBass || ty == kTrackRealBass ? 6 : 4);
    mBehavior->SetCanDeployOverdrive(single && c1);
    bool tilt = false;
    if ((unsigned)(ty - 1) <= 7U && ((1 << (ty - 1)) & 0xBBU) && c1)
        tilt = true;
    mBehavior->SetTiltDeploysBandEnergy(tilt);
    mBehavior->SetFillsDeployBandEnergy(ty == kTrackDrum && c1);
    mBehavior->SetRequireAllCodaLanes(ty > 9U || !((1 << ty) & 0x3E1U));
    mBehavior->SetCanFreestyleBeforeGems(false);
    mBehavior->SetHasSolos(ty <= 8U && ((1 << ty) & 0x177U));
    mBehavior->SetStreakType(mUser->GetTrackSym());
}

void GemPlayer::EnableController() {
    if (mController)
        mController->Disable(false);
    if (GetUser()->IsLocal()) {
        JoypadKeepAlive(GetUser()->GetLocalUser()->GetPadNum(), true);
    }
}

void GemPlayer::DisableController() { OnDisableController(); }

void GemPlayer::PrintHopoStats() {
    const std::vector<GameGem> &gems = TheSongDB->GetGems(mTrackNum);
    MILO_LOG("Hopoable gems not hopoed:\n");
    for (int i = 0; i < gems.size(); i++) {
        if (gems[i].GetNoStrum()) {
            if (mGemStatus->GetEncountered(i) && !mGemStatus->GetHopoed(i)) {
                MILO_LOG("  %d\n", i);
            }
        }
    }
}

void GemPlayer::InputReceived() {
    unk3ac = TheGame->mLastPollMs;
    unk3bc = 0;
    if (mAnnoyingMode) {
        SetAnnoyingMode(false);
    }
}

void GemPlayer::SetAnnoyingMode(bool b1) {
    if (mAnnoyingMode != b1 && IsLocal()) {
        mAnnoyingMode = b1;
    }
}

void GemPlayer::SetRemoteAnnoyingMode(bool b1) {
    if (unk3b9 != b1 && IsLocal()) {
        unk3b9 = b1;
        static Message msg("send_miss_noises", 0);
        msg[0] = unk3b9;
        HandleType(msg);
    }
}

int GemPlayer::GetTrackSlot(int x) const { return x; }

bool GemPlayer::InTrill(int idx) const {
    std::pair<int, int> trill;
    SongData *data = TheSongDB->GetData();
    return data->GetTrillSlotsAtTick(
        mTrackNum, TheSongDB->GetGem(mTrackNum, idx).GetTick(), trill
    );
}

bool GemPlayer::InRGTrill(int idx) const {
    RGTrill trill;
    SongData *data = TheSongDB->GetData();
    return data->GetRGTrillAtTick(
        mTrackNum, TheSongDB->GetGem(mTrackNum, idx).GetTick(), trill
    );
}

bool GemPlayer::InRoll(int idx) const {
    SongData *data = TheSongDB->GetData();
    return data->GetRollingSlotsAtTick(
        mTrackNum, TheSongDB->GetGem(mTrackNum, idx).GetTick()
    );
}

bool GemPlayer::InRGRoll(int idx) const {
    SongData *data = TheSongDB->GetData();
    RGRollChord chord = data->GetRGRollingSlotsAtTick(
        mTrackNum, TheSongDB->GetGem(mTrackNum, idx).GetTick()
    );
    return chord.mString[0] != -1;
}

void GemPlayer::CheckFretReleases(float f1) {
    for (std::vector<UpcomingFretRelease>::iterator it = mUpcomingFretReleases.begin();
         it != mUpcomingFretReleases.end();) {
        if (f1 > it->unk4) {
            FretButtonUp(it->unk0, f1);
            it = mUpcomingFretReleases.erase(it);
        } else
            ++it;
    }
}

void GemPlayer::RemoveFretReleasesInSlot(int slot) {
    for (std::vector<UpcomingFretRelease>::iterator it = mUpcomingFretReleases.begin();
         it != mUpcomingFretReleases.end();) {
        if (slot == it->unk0) {
            it = mUpcomingFretReleases.erase(it);
        } else
            ++it;
    }
}

#pragma push
#pragma dont_inline on
BEGIN_HANDLERS(GemPlayer)
    HANDLE_ACTION(
        disable_fills_deploy_band_energy, mBehavior->SetFillsDeployBandEnergy(false)
    )
    HANDLE_ACTION(
        enable_fills_deploy_band_energy, mBehavior->SetFillsDeployBandEnergy(true)
    )
    HANDLE_ACTION(set_whammystarpowerenabled, OnSetWhammyOverdriveEnabled(_msg->Int(2)))
    HANDLE_ACTION(set_mercuryswitchenabled, OnSetMercurySwitchEnabled(_msg->Int(2)))
    HANDLE_ACTION(reset_coda_points, OnResetCodaPoints())
    HANDLE_EXPR(score, GetScore())
    HANDLE_EXPR(percent_hit, OnGetPercentHit())
    HANDLE_EXPR(
        percent_hit_gems_practice,
        OnGetPercentHitGemsPractice(_msg->Int(2), _msg->Float(3), _msg->Float(4))
    )
    HANDLE_EXPR(get_gem_count, (int)TheSongDB->GetGems(mTrackNum).size())
    HANDLE_EXPR(get_gem_result, OnGetGemResult(_msg->Int(2)))
    HANDLE_EXPR(get_gem_is_sustained, OnGetGemIsSustained(_msg->Int(2)))
    HANDLE_EXPR(
        get_gem_is_no_strum,
        TheSongDB->GetGemList(mTrackNum)->GetGem(_msg->Int(2)).GetNoStrum()
    )
    HANDLE_ACTION(on_game_over, OnGameOver())
    HANDLE_ACTION(disable_controller, OnDisableController())
    HANDLE_EXPR(num_stars, GetNumStars())
    HANDLE_EXPR(star_rating, GetStarRating())
    HANDLE_ACTION(
        win,
        mBandPerformer ? mBandPerformer->WinGame(_msg->Int(2)) : WinGame(_msg->Int(2))
    )
    HANDLE_ACTION(lose, mBandPerformer ? mBandPerformer->LoseGame() : LoseGame())
    HANDLE_ACTION(enable_fills, EnableFills(_msg->Float(2), false))
    HANDLE_ACTION(disable_fills, DisableFills())
    HANDLE_EXPR(are_fills_forced, mForceFill)
    HANDLE_ACTION(force_fill, ForceFill(_msg->Int(2)))
    HANDLE_EXPR(toggle_no_fills, ToggleNoFills() == 0)
    HANDLE_ACTION(set_fill_audio, mMatcher->SetFillAudio(_msg->Int(2)))
    HANDLE_ACTION(
        set_alternate_fill_mapping, mController->UseAlternateMapping(_msg->Int(2))
    )
    HANDLE_EXPR(auto_play, IsAutoplay())
    HANDLE_ACTION(set_auto_play, SetAutoplay(_msg->Int(2)))
    HANDLE_ACTION(set_auto_play_error, mMatcher->SetAutoplayError(_msg->Int(2)))
    HANDLE_ACTION(remote_hit, OnRemoteHit(_msg->Int(2), _msg->Int(3), _msg->Float(4)))
    HANDLE_ACTION(
        remote_penalize, OnRemotePenalize(_msg->Int(2), _msg->Int(3), _msg->Float(4))
    )
    HANDLE_ACTION(remote_coda_hit, OnRemoteCodaHit(_msg->Int(2), _msg->Int(3)))
    HANDLE_ACTION(remote_whammy, OnRemoteWhammy(_msg->Float(2)))
    HANDLE_ACTION(remote_fill, OnRemoteFill(_msg->Int(2)))
    HANDLE_ACTION(
        remote_fill_hit, OnRemoteFillHit(_msg->Int(2), _msg->Int(3), _msg->Int(4))
    )
    HANDLE_ACTION(remote_hit_last_coda_gem, OnRemoteHitLastCodaGem(_msg->Int(2)))
    HANDLE_ACTION(remote_blow_coda, OnRemoteBlowCoda())
    HANDLE_ACTION(remote_solo_start, LocalSoloStart())
    HANDLE_ACTION(remote_solo_hit, LocalSoloHit(_msg->Int(2)))
    HANDLE_ACTION(remote_solo_end, LocalSoloEnd(_msg->Int(2), _msg->Int(3)))
    HANDLE_ACTION(remote_guitar_fx, LocalSetGuitarFx(_msg->Int(2)))
    HANDLE_ACTION(remote_finale_hit, LocalFinaleSwing(_msg->Int(2)))
    HANDLE_ACTION(remote_miss_noises, mAnnoyingMode = _msg->Int(2))
    HANDLE_ACTION(on_start_starpower, OnStartOverdrive())
    HANDLE_ACTION(on_stop_starpower, OnStopOverdrive())
    HANDLE_ACTION(on_new_track, HookupTrack())
    HANDLE_ACTION(refresh_track_buttons, OnRefreshTrackButtons())
    HANDLE_ACTION(update_guitar_fx, mFxPos = DataVariable("test_guitar_fx").Int())
    HANDLE_EXPR(in_freestyle_section, InFillNow())
    HANDLE_EXPR(in_trill, InTrill(_msg->Int(2)))
    HANDLE_EXPR(in_rg_trill, InRGTrill(_msg->Int(2)))
    HANDLE_EXPR(in_roll, InRoll(_msg->Int(2)))
    HANDLE_EXPR(in_rg_roll, InRGRoll(_msg->Int(2)))
    HANDLE_EXPR(get_notes_hit_fraction, mGemStatus->GetNotesHitFraction(nullptr))
    HANDLE_ACTION(print_hopo_stats, PrintHopoStats())
    HANDLE_ACTION(set_paused, SetPaused(_msg->Int(2)))
    HANDLE_SUPERCLASS(Player)
    HANDLE_CHECK(4668)
END_HANDLERS
#pragma pop
