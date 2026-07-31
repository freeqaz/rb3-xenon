#include "bandtrack/GemTrack.h"

#ifdef HX_NATIVE
#include <cstdio>
#include <cstdlib>
#endif

#include "GemManager.h"
#include "bandobj/BandTrack.h"
#include "bandobj/GemTrackDir.h"
#include "beatmatch/FillInfo.h"
#include "beatmatch/GameGem.h"
#include "beatmatch/PlayerTrackConfig.h"
#include "decomp.h"
#include "game/BandUser.h"
#include "game/Game.h"
#include "game/GameConfig.h"
#include "game/GamePanel.h"
#include "game/GemPlayer.h"
#include "game/RealGuitarGemPlayer.h"
#include "game/SongDB.h"
#include "game/TrainerPanel.h"
#include "math/Utl.h"
#include "meta_band/GameplayOptions.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "os/System.h"
#include "rndobj/Anim.h"
#include "rndobj/EventTrigger.h"
#include "rndobj/PropAnim.h"
#include "track/TrackWidget.h"
#include "utl/BeatMap.h"
#include "utl/RangedDataCollection.h"
#include "utl/Symbols.h"
#include "utl/Messages.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"
#include "utl/TimeConversion.h"

bool sEnableShift = true;
bool sUpdateShifting;

static inline void MinEqPtr(int &x, const int &y) {
    const int *p = &x;
    if (y < x)
        p = &y;
    x = *p;
}

inline bool GemTrack::ShiftsEnabled() const { return mEnableShifting && sEnableShift; }

DataNode ToggleShift(DataArray *arr) {
    sUpdateShifting = true;
    sEnableShift = !sEnableShift;
    return sEnableShift;
}

GemTrack::GemTrack(BandUser *user)
    : Track(user), mResetFills(0),
      mUseFills(SystemConfig("scoring", "overdrive")->FindInt("fills")), mTrackDir(this),
      mLastTopTick(-1), mLastBottomTick(-1), mGemManager(0), mKickPassCounter(0),
      mUpdateShifting(0), mEnableShifting(1), mRange(-1.0f), mOffset(-1.0f),
      mUpcomingShiftMaskAnim(this), mKeyIntroTasks(this) {
    DataRegisterFunc("toggle_key_shifting", ToggleShift);
}

GemTrack::~GemTrack() {
    RELEASE(mGemManager);
    mKeyIntroTasks.DeleteAll();
}

void GemTrack::Init() {
    bool lefty = false;
    BandUser *pUser = (BandUser *)mTrackConfig.GetBandUser();
    MILO_ASSERT(pUser, 0x5F);
    GameplayOptions *options = pUser->GetGameplayOptions();
    MILO_ASSERT(options, 0x61);
    if (options)
        lefty = options->GetLefty();
    PlayerTrackConfigList *pCfg = TheGameConfig->GetConfigList();
    MILO_ASSERT(pCfg, 0x66);
    unsigned int cymlanes = pCfg->GetGameCymbalLanes();
    int tracknum = TheGameConfig->GetTrackNum(pUser->GetUserGuid());
    pUser->GetDifficulty();
    mTrackConfig.SetLefty(lefty);
    mTrackConfig.SetGameCymbalLanes(cymlanes);
    mTrackConfig.SetTrackNum(tracknum);
    if (pUser->GetPlayer()) {
        PlayerInit();
    }
    mUpcomingShiftMaskAnim =
        mTrackDir->Find<RndAnimatable>("keys_upcoming_shift.anim", true);
}

void GemTrack::PlayerInit() {
    BandUser *pUser = (BandUser *)mTrackConfig.GetBandUser();
    MILO_ASSERT(pUser, 0x80);
    GemPlayer *player = dynamic_cast<GemPlayer *>(pUser->GetPlayer());
    if (player) {
        mTrackConfig.SetTrackNum(TheGameConfig->GetTrackNum(pUser->GetUserGuid()));
        mTrackConfig.SetMaxSlots(player->GetMaxSlots());
        mTrackConfig.SetDisableHopos(pUser->GetControllerType() == kControllerKeys);
#ifdef HX_NATIVE
        if (getenv("GEM_DBG")) {
            fprintf(stderr,
                    "[GEM_DBG] GemTrack::PlayerInit: user=%p tracksym='%s' "
                    "tracktype=%d MaxSlots=%d cymlanes=%u\n",
                    (void*)pUser, pUser->GetTrackSym().Str(),
                    (int)pUser->GetTrackType(), player->GetMaxSlots(),
                    mTrackConfig.GetGameCymbalLanes());
        }
#endif
        HandleNewSong();
    }
#ifdef HX_NATIVE
    else if (getenv("GEM_DBG")) {
        fprintf(stderr,
                "[GEM_DBG] GemTrack::PlayerInit: NO GEM PLAYER for user=%p tracksym='%s'\n",
                (void*)pUser, pUser->GetTrackSym().Str());
    }
#endif
}

void GemTrack::PostInit() {
    UpdateShifts();
    RangeShift *shift = mRangeShifts.end();
    if (mCurrentRangeShift != shift) {
        ApplyShiftImmediately(*mCurrentRangeShift);
        mCurrentRangeShift++;
    }
}

void GemTrack::ResetFills(bool reset) { mResetFills = reset; }

void GemTrack::RebuildBeats() {
    GetTrackDir()->ClearAllWidgets();
    int secs = MsToTickInt(
        (TheTaskMgr.Seconds(TaskMgr::kRealTime) + mTrackDir->BottomSeconds()) * 1000.0f
    );
    mLastBottomTick = mLastTopTick = secs;
}

void GemTrack::ApplyShiftImmediately(const RangeShift &shift) {
    float min;
    float f1 = shift.unk14;
    min = Min(shift.unkc, 15.0f - f1);
    MILO_ASSERT(mTrackDir, 0xB6);
    if (f1 != mRange) {
        mTrackDir->SetDisplayRange(f1);
        mRange = f1;
    }
    if (min != mOffset) {
        mTrackDir->SetDisplayOffset(min, true);
        mOffset = min;
    }
}

float GemTrack::TickToOffset(int tick) const {
    float ret = 0;
    for (std::vector<RangeShift>::const_iterator it = mRangeShifts.begin();
         it != mRangeShifts.end() && it->unk0 <= tick; ++it) {
        if (tick >= it->unk0 && tick <= it->unk4) {
            return 0.5f * (it->unkc + it->unk8);
        }
        if (tick > it->unk4) ret = it->unkc;
    }
    return ret;
}

DECOMP_FORCEACTIVE(
    GemTrack,
    "set_drum_fill_complete.trig",
    "key_mash.wid",
    "deploy_beard.anim",
    "Authored keyboard range section at tick %d is unexpected size: %d (should be %d)",
    "Authored range section exceeds displayable keyboard range: %.0f - %.0f @ tick %d",
    "flip_shift_arrows",
    "key_shift_right.wid",
    "key_shift_left.wid",
    "startKey <= endKey",
    "bar_measure.wid",
    "bar_beat.wid",
    "bar_half_beat.wid",
    "mGemManager",
    "guitar_solo_mask.wid",
    "bar_blue_beat.wid",
    "finish_line.wid",
    "keys_upcoming_shift.wid"
)

void GemTrack::UpdateShiftsToTick(int tick) {
    UpdateShifts();
    std::vector<RangeShift>::iterator it = mRangeShifts.begin();
    mCurrentRangeShift = it;
    for (; it != mRangeShifts.end(); ++it) {
        if (it->unk0 <= tick) {
            mCurrentRangeShift = it;
        } else
            break;
    }
    if (mCurrentRangeShift != mRangeShifts.end()) {
        ApplyShiftImmediately(*mCurrentRangeShift);
        mCurrentRangeShift++;
    }
}

void GemTrack::CheckShifts(float ms, int topTick) {
    int nowTick = MsToTick(ms);
    int tickOffset = 0;
    if (TheGame->InTrainer()) {
        nowTick = GetLoopTick(nowTick, tickOffset);
        mCurrentRangeShift = mRangeShifts.begin();
    }
    float newRange = -1.0f;
    float newOffset = -1.0f;
    while (mCurrentRangeShift != mRangeShifts.end()
           && mCurrentRangeShift->unk4 < nowTick) {
        newRange = mCurrentRangeShift->unk14;
        newOffset = mCurrentRangeShift->unkc;
        mCurrentRangeShift++;
        if (mCurrentRangeShift != mRangeShifts.end())
            mCurrentRangeShift->unk18 = false;
    }
    if (mCurrentRangeShift != mRangeShifts.end()) {
        RangeShift &shift = *mCurrentRangeShift;
        if (shift.unk0 + tickOffset < topTick && !shift.unk18) {
            shift.unk18 = true;
            int delta = (int)(shift.unkc - mOffset);
            int clamped;
            if (delta > 5) {
                clamped = 5;
            } else if (delta < -5) {
                clamped = -5;
            } else {
                clamped = delta;
            }
            float secs = TickToSeconds((float)(shift.unk0 + tickOffset));
            int key;
            int frame;
            if (clamped >= 0) {
                key = WhiteKeyToSemitone(2);
                frame = -(5 - clamped);
                shift.unk20 = -5;
            } else {
                key = WhiteKeyToSemitone((int)mRange - 3);
                frame = clamped + 5;
                shift.unk20 = 5;
            }
            MinEqPtr(key, 24);
            mUpcomingShiftMaskAnim->SetFrame((float)frame, 1.0f);
            Transform xfm;
            mTrackDir->MakeWidgetXfm(key, secs, xfm);
            TrackWidget *widget =
                mGemManager->GetWidgetByName("keys_upcoming_shift.wid");
            widget->Clear();
            widget->AddInstance(xfm, 0.0f);
            shift.unk1c = frame;
        }
        if (shift.unk0 < nowTick) {
            if (shift.unk10 < 0.0f) {
                if (newRange > 0.0f) {
                    shift.unk10 = newRange;
                    shift.unk8 = newOffset;
                } else {
                    shift.unk10 = mTrackDir->GetKeyRange();
                    shift.unk8 = mTrackDir->GetKeyOffset();
                }
            }
            float baseRange = shift.unk10;
            float baseOffset = shift.unk8;
            float frac = (float)(nowTick - shift.unk0)
                / (float)(shift.unk4 - shift.unk0);
            newRange = frac * (shift.unk14 - baseRange) + baseRange;
            newOffset = frac * (shift.unkc - baseOffset) + baseOffset;
            mUpcomingShiftMaskAnim->SetFrame(
                frac * (float)(shift.unk20 - shift.unk1c) + (float)shift.unk1c,
                1.0f
            );
        }
    }
    if (newRange > 0.0f) {
        float t = 15.0f - newRange;
        if (t < newOffset)
            newOffset = t;
        if (newRange != mRange) {
            mTrackDir->SetDisplayRange(newRange);
            mRange = newRange;
        }
        if (newOffset != mOffset) {
            mTrackDir->SetDisplayOffset(newOffset, false);
            mOffset = newOffset;
        }
    }
}

void GemTrack::UpdateLeftyFlip() {
    bool lefty = false;
    GameplayOptions *options =
        ((BandUser *)mTrackConfig.GetBandUser())->GetGameplayOptions();
    MILO_ASSERT(options, 0xEA);
    if (options)
        lefty = options->GetLefty();
    mTrackConfig.SetLefty(lefty);
    if (mGemManager)
        mGemManager->UpdateLeftyFlip(true);
    ResetFills(true);
    mTrackDir->UpdateLeftyFlip(lefty);
    mUpdateShifting = true;
}

void GemTrack::UpdateFills() {
    Player *player = mTrackConfig.GetBandUser()->GetPlayer();
    if (!player || !player->IsDeployingBandEnergy())
        return;
    else {
        Symbol s;
        for (int i = 0; i < mTrackConfig.GetMaxSlots(); i++) {
            if (mGemManager->GetWidgetName(s, i, fill)) {
                mGemManager->GetWidgetByName(s)->Clear();
            }
        }
        Symbol symlist[3] = { crash, crash_cymbal, beard };
        for (int i = 0; i < 3U; i++) {
            if (mGemManager->GetWidgetName(s, 4, symlist[i])) {
                mGemManager->GetWidgetByName(s)->Clear();
            }
        }
    }
}

void GemTrack::ChangeDifficulty(Difficulty diff, int iii) {
    mGemManager->ClearGems(true);
    mGemManager->SetupGems(iii);
    UpdateShiftsToTick(iii);
    GetTrackDir()->ClearAllWidgets();
    int secs = MsToTickInt(
        (TheTaskMgr.Seconds(TaskMgr::kRealTime) + mTrackDir->TopSeconds()) * 1000.0f
    );
    mLastBottomTick = mLastTopTick = secs;
}

void GemTrack::DropIn(int tick) { UpdateShiftsToTick(tick); }

void GemTrack::SetPlayerState(const PlayerState &state) { mPlayerState = state; }

void GemTrack::DrawFill(FillInfo *info, int i2, int i3) {
    FillExtent ext154(0, 0, 0);
    Player *player = mTrackConfig.GetBandUser()->GetPlayer();
    int i158 = 0;
    if (!TheGame->InTrainer()) {
        if (!info)
            return;
        if (!info->FillAt(i2, ext154, true) && !info->FillAt(i2 + i3, ext154, true))
            return;
        if (!player->FillsEnabled(ext154.start))
            return;
    } else {
        int ivar3 = GetLoopTick(i2, i158);
        if (!info->FillAt(ivar3, ext154, true) && !info->FillAt(ivar3 + i3, ext154, true))
            return;
        if (!player->FillsEnabled(ext154.start + i158))
            return;
    }
    {
        ext154.start += i158;
        ext154.end += i158;
        Transform tf88;
        int i11 = i2 + i3;
        bool isDrum = mTrackConfig.IsDrumTrack();
        bool isKeys = mTrackConfig.IsKeyboardTrack();
        bool inCoda = TheSongDB->IsInCoda(i11) != 0;
        float startSecs = TickToSeconds(ext154.start);
        float endSecs = TickToSeconds(ext154.end);
        float f15 = endSecs - startSecs;
        bool b9 = false;
        if (ext154.start < i11) {
            bool cond = (i2 <= ext154.start || mResetFills);
            if (cond)
                b9 = true;
        }
        if (!inCoda || TheGame->mProperties.mEnableCoda) {
            bool b1 = false;
            if (!inCoda && isDrum)
                b1 = true;
            bool bi2 = !TheGame->DrumFillsMod();
            if (b9) {
                if (b1) {
                    mTrackDir->ResetDrumFill();
                    if (TheGame->InDrumTrainer()) {
                        EventTrigger *trig = mTrackDir->Find<EventTrigger>(
                            "set_drum_fill_complete.trig", false
                        );
                        if (trig)
                            trig->Trigger();
                    }
                } else
                    mTrackDir->ResetCoda();

                if (mUseFills && (!b1 || !bi2)) {
                    if (isKeys) {
                        tf88.Reset();
                        tf88.v.y = mTrackDir->SecondsToY(startSecs);
                        TrackWidget *w = mGemManager->GetWidgetByName("key_mash.wid");
                        w->Clear();
                        w->AddInstance(tf88, f15);
                    } else {
                        Symbol s15c(b1 ? fill : mash);
                        for (int i = 0; i < mTrackConfig.GetMaxSlots(); i++) {
                            Symbol s160;
                            if (mGemManager->GetWidgetName(s160, i, s15c)) {
                                mTrackDir->MakeWidgetXfm(i, startSecs, tf88);
                                TrackWidget *w = mGemManager->GetWidgetByName(s160);
                                w->Clear();
                                w->AddInstance(tf88, f15);
                            }
                        }
                    }
                } else {
                    Symbol s164;
                    if (mGemManager->GetWidgetName(s164, 4, beard)) {
                        mTrackDir->MakeWidgetXfm(4, startSecs, tf88);
                        TrackWidget *w = mGemManager->GetWidgetByName(s164);
                        w->Clear();
                        w->AddInstance(tf88, 0);
                        float f14 = mTrackDir->SecondsToY(f15);
                        mTrackDir->Find<RndPropAnim>("deploy_beard.anim", true)
                            ->SetFrame(f14, 1);
                        w->SetBaseLength(f14);
                    }
                }
                mResetFills = false;
            }
            if (b1 && i2 < ext154.end && ext154.end <= i11) {
                mTrackDir->MakeWidgetXfm(4, endSecs, tf88);
                Symbol s168 =
                    mTrackConfig.GetGameCymbalLanes() & 0x10 ? crash_cymbal : crash;
                Symbol s16c;
                if (mGemManager->GetWidgetName(s16c, 4, s168)) {
                    TrackWidget *w = mGemManager->GetWidgetByName(s16c);
                    w->AddInstance(tf88, 0);
                    if (isDrum) {
                        mTrackDir->FillHit(3);
                    }
                }
            }
        }
    }
}

void GemTrack::UpdateShifts() {
    RangeShift * &_ref0 = mCurrentRangeShift;
    bool _cond = !mTrackConfig.IsKeyboardTrack();
    if (_cond) {
        _ref0 = mRangeShifts.end();
    } else {
        mRangeShifts.clear();
        std::vector<RangeSection> &rangeSects = TheSongDB->GetData()->GetKeyRangeSections(
            mTrackConfig.GetBandUser()->GetDifficulty()
        );
        if (ShiftsEnabled()) {
            for (int i = 0; i < rangeSects.size(); i++) {
                RangeSection &curSect = rangeSects[i];
                int i6 = SemitoneToWhiteKey(Round(curSect.unk8));
                int i1 = SemitoneToWhiteKey(Round(curSect.unkc)) + 1;
                if (i1 - i6 != 10) {
                    MILO_WARN(
                        "Authored keyboard range section at tick %d is unexpected size: %d (should be %d)",
                        curSect.unk0,
                        i1 - i6,
                        10
                    );
                    i1 = i6 + 10;
                }
                if (i1 > 16) {
                    i6 -= i1 - 16;
                    i1 -= i1 - 16;
                    MILO_WARN(
                        "Authored range section exceeds displayable keyboard range: %.0f - %.0f @ tick %d",
                        curSect.unk8,
                        curSect.unkc,
                        curSect.unk0
                    );
                }
                int i8 = curSect.unk0;
                mRangeShifts.push_back(
                    RangeShift(i8, i8 + MsToTickInt(curSect.unk4), i6, i1 - i6)
                );
            }
        } else {
            int i90 = 0;
            int i8c = 16;
            for (int i = 0; i < rangeSects.size(); i++) {
                RangeSection &curSect = rangeSects[i];
                MinEq(i8c, SemitoneToWhiteKey(Round(curSect.unk8)));
                MaxEq(i90, SemitoneToWhiteKey(Round(curSect.unkc)) + 1);
            }
            MinEq(i90, 16);
            mRangeShifts.push_back(RangeShift(0, 0, i8c, i90 - i8c));
        }
        _ref0 = mRangeShifts.begin();
    }
}

void GemTrack::DrawBeatLine(Symbol s1, int i2, int i3, bool b4) {
    static DataNode &flip_shift_arrows = DataVariable("flip_shift_arrows");
    float i8 = TickToSeconds(i3);
    FillInfo *info = TheSongDB->GetFillInfo(mTrackConfig.TrackNum(), i3);
    if (!TheSongDB->IsInCoda(i3) || !info || !info->FillAt(i3, true)
        || !TheGame->mProperties.mEnableCoda) {
        TrackWidget *w = mGemManager->GetWidgetByName(s1);
        Transform tf68;
        mTrackDir->MakeSecondsXfm(i8, tf68);
        w->AddInstance(tf68, 0);
    }
    if (!b4 && mCurrentRangeShift != mRangeShifts.end()) {
        if (TheGame->InTrainer()) {
            i2 = TickToBeat(GetLoopTick(i3));
        }
        RangeShift *curshift = mCurrentRangeShift;
        if ((int)std::floor(TickToBeat(curshift->unk0)) - i2 <= 3U) {
            int endKey = curshift->unkc - mOffset;
            if (endKey != 0) {
                Symbol sfc;
                int startKey;
                if (endKey < 0) {
                    startKey = 0;
                    endKey = 3;
                    if (flip_shift_arrows.Int()) {
                        sfc = "key_shift_right.wid";
                    } else {
                        sfc = "key_shift_left.wid";
                    }
                } else {
                    endKey = mRange;
                    startKey = endKey - 3;
                    if (flip_shift_arrows.Int()) {
                        sfc = "key_shift_left.wid";
                    } else {
                        sfc = "key_shift_right.wid";
                    }
                }
                MILO_ASSERT(startKey <= endKey, 0x297);
                for (; startKey < endKey; startKey++) {
                    int semitone = WhiteKeyToSemitone(startKey);
                    Transform tf98;
                    mTrackDir->MakeWidgetXfm(semitone, i8, tf98);
                    TrackWidget *w = mGemManager->GetWidgetByName(sfc);
                    w->AddInstance(tf98, 0);
                }
            }
        }
    }
}

#pragma push
#pragma pool_data off
void GemTrack::DrawBeatLines(int from_tick, int to_tick) {
    static Symbol downbeat_line("bar_measure.wid");
    static Symbol beat_line("bar_beat.wid");
    static Symbol offbeat_line("bar_half_beat.wid");
    int subdivision = 0xF0;
    if (TheSongDB->GetData()->mDetailedGrid) {
        subdivision = 0x78;
    }
    int beat = TickToBeat(from_tick);
    int beat_tick = BeatToTick((float)beat);
    while (beat_tick < to_tick) {
        if (beat_tick >= from_tick) {
            if (TheBeatMap->IsDownbeat(beat)) {
                DrawBeatLine(downbeat_line, beat, beat_tick, false);
            } else {
                DrawBeatLine(beat_line, beat, beat_tick, false);
            }
        }
        int sub_tick = beat_tick + subdivision;
        int next_beat = beat + 1;
        beat_tick = BeatToTick((float)next_beat);
        while (sub_tick < beat_tick && sub_tick < to_tick) {
            if (sub_tick >= from_tick) {
                DrawBeatLine(offbeat_line, beat, sub_tick, true);
            }
            sub_tick += subdivision;
        }
        beat = next_beat;
    }
}
#pragma pop

#pragma push
#pragma pool_data off
void GemTrack::DrawTrackElements(int from_tick, int to_tick) {
    Player *player = GetPlayer();
    if (!player)
        return;
    DrawBeatLines(from_tick, to_tick);
    static int res = 0xF0;
    Transform xfm;
    for (int tick = res * ((from_tick + res) / res); tick <= to_tick; tick += res) {
        TickToSeconds(tick);
        DrawFill(TheSongDB->GetFillInfo(mTrackConfig.TrackNum(), tick), tick, res);
        int soloStart = 0;
        int soloEnd = 0;
        if (TheSongDB->GetPhraseExtents(
                kSoloPhrase, mTrackConfig.TrackNum(), tick, soloStart, soloEnd
            )
            && !player->GetQuarantined() && TheGame->mProperties.mCanSolo) {
            static Symbol soloMask("guitar_solo_mask.wid");
            TrackWidget *maskWidget = mGemManager->GetWidgetByName(soloMask);
            if (tick - res < soloStart || maskWidget->Empty()) {
                float soloStartSecs = TickToSeconds(soloStart);
                float soloLen = TickToSeconds(soloEnd) - soloStartSecs;
                mTrackDir->MakeSecondsXfm(soloStartSecs, xfm);
                mTrackDir->SecondsToY(soloLen);
                maskWidget->AddInstance(xfm, soloLen);
            }
            static Symbol boundaryWidName("bar_blue_beat.wid");
            if (tick - res < soloStart) {
                TrackWidget *w = mGemManager->GetWidgetByName(boundaryWidName);
                mTrackDir->MakeSecondsXfm(TickToSeconds(soloStart), xfm);
                w->AddInstance(xfm, 0.0f);
            }
            if (tick + res > soloEnd) {
                TrackWidget *w = mGemManager->GetWidgetByName(boundaryWidName);
                mTrackDir->MakeSecondsXfm(TickToSeconds(soloEnd), xfm);
                w->AddInstance(xfm, 0.0f);
            }
        }
        static Symbol finishLine("finish_line.wid");
        if (tick <= TheSongDB->GetCodaStartTick()
            && TheSongDB->GetCodaStartTick() < tick + res) {
            mTrackDir->MakeSecondsXfm(TickToSeconds(TheSongDB->GetCodaStartTick()), xfm);
            mGemManager->GetWidgetByName(finishLine)->AddInstance(xfm, 0.0f);
        }
        if (TheGame->mProperties.mInPracticeMode) {
            float startMs;
            float endMs;
            int firstSection = 0;
            int lastSection = 0;
            float dummy;
            TheGamePanel->mConfig.GetPracticeSections(firstSection, lastSection);
            TheGamePanel->mConfig.GetSectionBounds(firstSection, startMs, dummy);
            TheGamePanel->mConfig.GetSectionBounds(lastSection, dummy, endMs);
            if (startMs < TickToMs(tick + res)) {
                mTrackDir->MakeSecondsXfm(startMs / 1000.0f, xfm);
                mGemManager->GetWidgetByName(finishLine)->AddInstance(xfm, 0.0f);
            }
            if (endMs < TickToMs(tick + res)) {
                mTrackDir->MakeSecondsXfm(endMs / 1000.0f, xfm);
                mGemManager->GetWidgetByName(finishLine)->AddInstance(xfm, 0.0f);
            }
        }
    }
    mLastTopTick = to_tick;
}
#pragma pop

void GemTrack::Poll(float f) {
    int topTick;
    int bottomTick;
    int kbd;
    Player *player = GetPlayer();
#ifdef HX_NATIVE
    static bool sK8 = !!getenv("K8_DBG");
    static int sCnt = 0;
    if (sK8 && (sCnt++ % 120) == 0) {
        MILO_LOG("K8_DBG: GemTrack::Poll f=%.1f player=%p mGemManager=%p\n",
                 f, (void*)player, (void*)mGemManager);
    }
#endif
    if (player) {
        Track::Poll(f);
        MILO_ASSERT(mGemManager, 0x2E4);
        mGemManager->Poll(f, mPlayerState);
        mTrackConfig.TrackNum();
        topTick = (int)MsToTick(1000.0f * mTrackDir->TopSeconds() + f);
        bottomTick = (int)MsToTick(1000.0f * mTrackDir->BottomSeconds() + f);
        if (TheGame->unkdc != -1.0f) {
            if (bottomTick < mLastBottomTick) {
                DrawBeatLines(bottomTick, mLastBottomTick);
            }
        } else if (topTick < mLastTopTick) {
            RebuildBeats();
            mGemManager->Jump(f);
        }
        mLastBottomTick = bottomTick;
        if (mTrackConfig.IsRealGuitarTrack()) {
            mTrackDir->UpdateFingerFeedback(
                ((RealGuitarGemPlayer *)player)->GetRGState()
            );
        }
        if (topTick >= 0 && topTick > mLastTopTick) {
            if (TheGame->IsWaiting()) return;
            DrawTrackElements(mLastTopTick, topTick);
            if (sUpdateShifting || mUpdateShifting) {
                UpdateShifts();
                mUpdateShifting = false;
                sUpdateShifting = false;
            }
            kbd = (int)mTrackConfig.IsKeyboardTrack();
            bool shifting = kbd & ((GemTrackDir *)kbd)->KeyShifting();
            if (shifting) {
                CheckShifts(f, topTick);
            }
            mLastPlayerState = mPlayerState;
        }
    }
}

void GemTrack::RedrawTrackElements(float f) {
    int top_tick = MsToTick(mTrackDir->TopSeconds() * 1000.0f + f);
    int bottom_tick = MsToTick(mTrackDir->BottomSeconds() * 1000.0f + f);
    DrawTrackElements(bottom_tick, top_tick);
}

void GemTrack::SetDir(RndDir *rdir) {
    mTrackDir = dynamic_cast<GemTrackDir *>(rdir);
    MILO_ASSERT(mTrackDir, 0x42A);
    mTrackDir->Enter();
    mTrackDir->SetShowing(true);
    Init();
}

void GemTrack::HandleNewSong() {
    if (mGemManager) {
        mGemManager->SetupGems(0);
    } else {
        mGemManager = new GemManager(mTrackConfig, mTrackDir);
    }
    if (mTrackConfig.IsKeyboardTrack()) {
        mGemManager->UpdateEnabledSlots();
    }
}

void GemTrack::See(float f, int i) {
    int mask = mGemManager->GetSlotsForGem(i);
    if (mTrackConfig.IsDrumTrack() && ((mask & 1) == 1)) {
        mTrackDir->SeeKick();
    }
}

void GemTrack::Hit(float f, int i1, int i2) {
    mGemManager->Hit(f, i1, i2);
    int slots = mGemManager->GetSlotsForGem(i1);
    mTrackDir->GemHit(slots);
}

void GemTrack::Miss(float f, int i1, int i2) { mGemManager->Miss(f, i1, i2); }

void GemTrack::Pass(int i) {
    mGemManager->Pass(i);
    int slots = mGemManager->GetSlotsForGem(i);
    mTrackDir->GemPass(i, slots);
}

void GemTrack::Ignore(int i) { mGemManager->Ignore(i); }

void GemTrack::PartialHit(float f, int i1, unsigned int ui, int i2) {
    mGemManager->PartialHit(f, i1, ui, i2);
}

void GemTrack::FillHit(int i1, int i2) { mGemManager->FillHit(i1, i2); }

void GemTrack::SetFretButtonPressed(int i1, bool b) {
    mGemManager->SetSmasherGlowing(i1, b);
}

void GemTrack::ReleaseGem(float f, int i) { mGemManager->Released(f, i); }

void GemTrack::Jump(float f) {
    if (mGemManager) {
        mGemManager->Jump(f);
        if (f <= 0 && mTrackDir) {
            mTrackDir->ClearAllWidgets();
            mGemManager->SetGemsEnabled(0);
        }
        mCurrentRangeShift = mRangeShifts.begin();
    }
}

GemManager *GemTrack::GetGemManager() { return mGemManager; }

void GemTrack::OnMissPhrase(int i) {
    if (mGemManager->OnMissPhrase(i)) {
        if (GetBandTrack()) {
            GetBandTrack()->SpotlightFail(true);
        }
    }
}

BandTrack *GemTrack::GetBandTrack() { return mTrackDir; }

float GemTrack::NextKickNoteMs() const {
    if (!mTrackConfig.IsDrumTrack()) {
        return 3.4028234663852886e+38f;
    } else {
        float secs = TheTaskMgr.Seconds(TaskMgr::kRealTime) * 1000.0f;
        BandUser *pUser = (BandUser *)mTrackConfig.GetBandUser();
        MILO_ASSERT(pUser, 0x4D1);
        const std::vector<GameGem> &gems =
            TheSongDB->GetGems(TheGameConfig->GetTrackNum(pUser->GetUserGuid()));
        for (int i = 0; i < gems.size(); i++) {
            const GameGem &gem = gems[i];
            if ((gem.GetSlots() & 1) != 1 || gem.GetMs() < secs)
                continue;
            else
                return gem.GetMs() - secs;
        }
    }
    return 3.4028234663852886e+38f;
}

void GemTrack::SetBonusGems(bool b) {
    if (mGemManager) {
        if (b) {
            static Symbol unisonMask("unison_mask.wid");
            mGemManager->GetWidgetByName(unisonMask)->Clear();
        }
        mGemManager->SetBonusGems(b, mPlayerState);
    }
}

void GemTrack::SetInCoda(bool b) {
    if (mGemManager)
        mGemManager->SetInCoda(b);
}

void GemTrack::UpdateEffects(int i) {
    MILO_ASSERT(mTrackDir, 0x503);
    mTrackDir->OnUpdateFx(i);
}

void GemTrack::SetSmasherGlowing(int i, bool b) { mGemManager->SetSmasherGlowing(i, b); }

void GemTrack::PopSmasher(int i) { mGemManager->PopSmasher(i); }

void GemTrack::SetGemsEnabled(float f) {
    if (mGemManager)
        mGemManager->SetGemsEnabled(f);
}

void GemTrack::SetGemsEnabledByPlayer() {
#ifdef HX_NATIVE
    if (getenv("GEM_DBG"))
        MILO_LOG("GEM_DBG: SetGemsEnabledByPlayer mEnableMs=%.2f enabledState=%d\n",
                 GetPlayer()->mEnableMs, (int)GetPlayer()->GetEnabledState());
#endif
    SetGemsEnabled(GetPlayer()->mEnableMs);
}

void GemTrack::UpdateGems() {
    if (mGemManager) {
        mGemManager->UpdateGemStates();
        UpdateFills();
    }
}

Hmx::Object *GemTrack::GetSmasher(int i) {
    if (mGemManager)
        return mGemManager->GetSmasherObj(i);
    else
        return nullptr;
}

void GemTrack::ResetSmashers(bool b) {
    if (mGemManager)
        mGemManager->ResetSmashers(b);
}

void GemTrack::UpdateSlotXfms() {
    if (mGemManager)
        mGemManager->UpdateSlotPositions();
}

void GemTrack::RefreshCurrentShift() {
    if (mCurrentRangeShift != mRangeShifts.end()) {
        mCurrentRangeShift->unk10 = -1.0f;
        mCurrentRangeShift->unk8 = -1.0f;
    }
}

void GemTrack::PlayKeyIntros() {
    if (mGemManager) {
        DataArray *cfg = SystemConfig("track_graphics");
        float f8 = cfg->FindFloat("key_intro_offset_ms");
        float f9 = cfg->FindFloat("key_intro_delay_ms");
        if (mTrackConfig.IsKeyboardTrack()) {
            mGemManager->UpdateEnabledSlots();
            mKeyIntroTasks.DeleteAll();
            for (int j = 1, i = 0; i < mTrackConfig.GetMaxSlots(); i++) {
                unsigned int slots = mGemManager->EnabledSlots();
                if (slots & 1 << i) {
                    Hmx::Object *obj = mGemManager->GetSmasherObj(i);
                    MessageTask *task = new MessageTask(obj, intro_msg);
                    TheTaskMgr.Start(task, (TaskUnits)2, ((float)(j)*f8 + f9) / 1000.0f);
                    mKeyIntroTasks.push_back(task);
                    j++;
                }
            }
        }
        // int taskidx = 1;
        // for (int i = 0; i < mTrackConfig.GetMaxSlots(); i++) {
        //     MessageTask *task = new MessageTask(mGemManager->GetSmasherObj(i),
        //     intro_msg); TheTaskMgr.Start(task, (TaskUnits)2, ((float)taskidx * f8 + f9)
        //     / 1000.0f); mKeyIntroTasks.push_back(task); taskidx++;
        // }
    }

    //       if (*(this + 0x80) != 0) {
    //     Symbol::Symbol(auStack_80,s_track_graphics_8082f2a7);
    //     uVar1 = SystemConfig();
    //     uVar2 = Symbol::Symbol(auStack_84,s_key_intro_offset_ms_8082f2b6);
    //     dVar7 = DataArray::FindFloat(uVar1,uVar2);
    //     uVar2 = Symbol::Symbol(auStack_88,s_key_intro_delay_ms_8082f2ca);
    //     dVar8 = DataArray::FindFloat(uVar1,uVar2);
    //     iVar3 = TrackConfig::IsKeyboardTrack(this + 0x1c);
    //     if (iVar3 != 0) {
    //       GemManager::UpdateEnabledSlots(*(this + 0x80));
    //       ObjPtrList<>::DeleteAll(this + 0xdc);
    //       iVar3 = 1;
    //       for (uVar6 = 0; iVar5 = TrackConfig::GetMaxSlots(this + 0x1c), uVar6 < iVar5;
    //           uVar6 = uVar6 + 1) {
    //         uVar4 = fn_800DE1B0(*(this + 0x80));
    //         if ((uVar4 & 1 << (uVar6 & 0x3f)) != 0) {
    //           uVar1 = GemManager::GetSmasherObj(*(this + 0x80),uVar6);
    //           iVar5 = operator_new(0x2c);
    //           if (iVar5 != 0) {
    //             uVar2 = MergedGet0x4(MsgIntro);
    //             iVar5 = MessageTask::MessageTask(iVar5,uVar1,uVar2);
    //           }
    //           uStack_74 = iVar3 ^ 0x80000000;
    //           local_78 = 0x43300000;
    //           TaskMgr::Start((iVar3 * dVar7 + dVar8) / 1000.0,TheTaskMgr,iVar5,2);
    //           fn_800DDFD8(this + 0xdc,iVar5);
    //           iVar3 = iVar3 + 1;
    //         }
    //       }
    //     }
    //   }
    //   return;
}

void GemTrack::RemovePlayer() {
    if (mGemManager)
        mGemManager->HideGems();
    GetTrackDir()->ClearAllWidgets();
}

void GemTrack::OverrideRangeShift(float f1, float f2) {
    if (f1 != mRange) {
        mTrackDir->SetDisplayRange(f1);
        mRange = f1;
    }
    if (f2 != mOffset) {
        mTrackDir->SetDisplayOffset(f2, false);
        mOffset = f2;
    }
}

void GemTrack::SetEnableSlot(int slot, bool enable) {
    if (enable)
        mGemManager->EnableSlot(slot);
    else
        mGemManager->DisableSlot(slot);
    BandUser *pUser = (BandUser *)mTrackConfig.GetBandUser();
    MILO_ASSERT(pUser, 0x43C);
    std::vector<GameGem> &gems = (std::vector<GameGem> &)TheSongDB->GetGems(
        TheGameConfig->GetTrackNum(pUser->GetUserGuid())
    );
    float cutoff =
        1000.0f * (TheTaskMgr.Seconds(TaskMgr::kRealTime) + mTrackDir->ViewTimeSeconds());
    for (std::vector<GameGem>::iterator it = gems.begin(); it != gems.end(); ++it) {
        if (it->GetSlot() == slot && it->mMs > cutoff) {
            if (enable)
                it->unk18 = 0;
            else
                it->unk18 = 0x80;
        }
    }
}

BEGIN_HANDLERS(GemTrack)
    HANDLE_ACTION(on_miss_phrase, OnMissPhrase(_msg->Int(2)))
    HANDLE_ACTION(set_bonus_gems, SetBonusGems(_msg->Int(2)))
    HANDLE_EXPR(
        get_smasher, mGemManager ? mGemManager->GetSmasherObj(_msg->Int(2)) : NULL_OBJ
    )
    HANDLE_ACTION(enable_slot, SetEnableSlot(_msg->Int(2), true))
    HANDLE_ACTION(disable_slot, SetEnableSlot(_msg->Int(2), false))
    HANDLE_ACTION(update_gems, UpdateGems())
    HANDLE_ACTION(set_gems_enabled, SetGemsEnabled(_msg->Float(2)))
    HANDLE_ACTION(handle_new_song, HandleNewSong())
    HANDLE_EXPR(lefty, Lefty())
    HANDLE_EXPR(next_kick_ms, NextKickNoteMs())
    HANDLE_SUPERCLASS(Track)
    HANDLE_CHECK(0x5A7)
END_HANDLERS
