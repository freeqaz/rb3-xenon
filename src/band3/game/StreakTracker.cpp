#include "game/StreakTracker.h"
#include "beatmatch/TrackType.h"
#include "game/Game.h"
#include "game/TrackerDisplay.h"
#include "game/TrackerSource.h"
#include "game/TrackerUtils.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "utl/Locale.h"
#include "utl/Symbols.h"

StreakTracker::StreakTracker(
    TrackerSource *src, TrackerBandDisplay &banddisp, TrackerBroadcastDisplay &bcdisp
)
    : Tracker(src, banddisp, bcdisp), unk64(0), unk68(0) {}

StreakTracker::~StreakTracker() {}

void StreakTracker::ConfigureTrackerSpecificData(const DataArray *arr) {
    static Symbol chain_multipliers("chain_multipliers");
    static Symbol streak_length_multiplier("streak_length_multiplier");
    unk88.InitFromDataArray(arr->FindArray(chain_multipliers, false));
    arr->FindData(streak_length_multiplier, unk6c, true);
}

void StreakTracker::TranslateRelativeTargets() {
    mStreakDataMap.clear();
    for (TrackerPlayerID id = mSource->GetFirstPlayer(); id.NotNull();
         id = mSource->GetNextPlayer(id)) {
        Player *pPlayer = mSource->GetPlayer(id);
        MILO_ASSERT(pPlayer, 0x3A);
        int trackNum = pPlayer->GetTrackNum();
        int count;
        TrackType trackType = pPlayer->GetTrackType();
        if (trackType == kTrackVocals) {
            count = TrackerUtils::CountVocalPhrasesInSong(trackNum);
        } else {
            count = TrackerUtils::CountGemsInSong(trackNum, trackType);
        }
        int val = (float)count * unk6c;
        MaxEq(val, 1);
        PlayerStreakData &entry = mStreakDataMap[id];
        entry.unk0 = val;
        entry.unk4 = 0;
        entry.unk8 = 0;
        entry.unkc = 0;
        entry.unk10 = 0;
        entry.unk14 = 0;
        entry.unk18 = 0;
        entry.unk1c = 0;
    }

    float invScale = 1.0f / unk6c;
    for (int i = 0; i < mTargets.size(); i++) {
        mTargets[i] = floorf(mTargets[i] * invScale);
    }
}

void StreakTracker::HandlePlayerSaved_(const TrackerPlayerID &pid) {
    const TrackerPlayerDisplay &disp = GetPlayerDisplay(pid);
    PlayerStreakData &data = mStreakDataMap[pid];
    data.unk4 = 0;
    data.unk8 = 0;
    data.unkc = 0;
    data.unk18 = 0;
    disp.GainFocus(false);
}

void StreakTracker::FirstFrame_(float) {
    unk64 = 0;
    unk68 = 0;
    mBandDisplay.Initialize(mDesc.mName);
    for (TrackerPlayerID id = mSource->GetFirstPlayer(); id.NotNull();
         id = mSource->GetNextPlayer(id)) {
        const TrackerPlayerDisplay &disp = GetPlayerDisplay(id);
        PlayerStreakData &data = mStreakDataMap[id];
        data.unk4 = 0;
        data.unk8 = 0;
        data.unkc = 0;
        data.unk18 = 0;
        disp.GainFocus(true);
    }
}

void StreakTracker::Poll_(float f) {
    if (mSource->IsFinished() || TheGame->unkdc != -1.0f) return;
    for (TrackerPlayerID id = mSource->GetFirstPlayer(); id.NotNull();
         id = mSource->GetNextPlayer(id)) {
        if (mSource->IsPlayerLocal(id)) {
            Player *pPlayer = mSource->GetPlayer(id);
            MILO_ASSERT(pPlayer, 0x9C);
            PlayerStreakData &data = mStreakDataMap[id];
            const TrackerPlayerDisplay &disp = GetPlayerDisplay(id);
            int curStreak = pPlayer->mStats.GetCurrentStreak();
            int hitCount = pPlayer->mStats.mHitCount;
            int streakActive = (unsigned int)(-curStreak & ~curStreak) >> 31;
            if (streakActive && !data.unk4) {
                data.unk4 = 1;
                data.unk8 = hitCount - 1;
            } else if (!streakActive && data.unk4) {
                data.unk4 = 0;
                data.unk8 = hitCount;
                data.unk18 = 0;
                disp.Pulse(false);
            }
            int progress = hitCount - data.unk8;
            if (data.unk10 != progress) {
                SetPlayerProgress(id, (float)progress / (float)data.unk0);
                data.unk10 = progress;
                int multIdx = unk88.GetMultiplierIndex((float)data.unk18);
                if (data.unk14 != multIdx) {
                    disp.SetSecondaryStateLevel(multIdx);
                    data.unk14 = multIdx;
                }
                if (progress >= data.unk0) {
                    disp.FillProgressAndReset(true);
                    data.unk10 = 0;
                    data.unk8 = hitCount;
                    data.unk18++;
                    data.unk1c++;
                    float multiplier = unk88.GetMultiplier((float)data.unk18);
                    LocalEndStreak(id, multiplier);
                    SendEndStreak(pPlayer, multiplier, 0);
                }
            }
        }
    }
}

void StreakTracker::RemoteEndStreak_(Player *p, float f, int) {
    TrackerPlayerID pid = mSource->FindPlayerID(p);
    if (pid.NotNull()) {
        LocalEndStreak(pid, f);
    }
}

void StreakTracker::LocalEndStreak(const TrackerPlayerID &pid, float f) {
    static Symbol streak_tracker_progress_1("streak_tracker_progress_1");
    static Symbol streak_tracker_progress("streak_tracker_progress");
    unk68++;
    unk64 += f;
    GetPlayerDisplay(pid).Pulse(true);
    Symbol sym = unk68 == 1 ? streak_tracker_progress_1 : streak_tracker_progress;
    mBroadcastDisplay.ShowBriefBandMessage(DataArrayPtr(sym, unk68));
}

void StreakTracker::UpdateGoalValueLabel(UILabel &label) const {
    static Symbol tour_goal_band_streak_goal_format("tour_goal_band_streak_goal_format");
    int ceiled = ceilf(mTargets.front() / 1000.0f);
    label.SetTokenFmt(tour_goal_band_streak_goal_format, ceiled / 60, ceiled % 60);
}

void StreakTracker::UpdateCurrentValueLabel(UILabel &label) const {}

String StreakTracker::GetPlayerContributionString(Symbol s) const {
    TrackerPlayerID pid = mSource->GetIDFromInstrument(s);
    int i4 = 0;
    if (pid.NotNull()) {
        Player *pPlayer = mSource->GetPlayer(pid);
        MILO_ASSERT(pPlayer, 299);
        i4 = pPlayer->mStats.unk1c0;
    }
    static Symbol tour_goal_band_streak_player_contribution_format(
        "tour_goal_band_streak_player_contribution_format"
    );
    static Symbol tour_goal_band_streak_player_contribution_format_1(
        "tour_goal_band_streak_player_contribution_format_1"
    );
    Symbol sym = i4 == 1 ? tour_goal_band_streak_player_contribution_format_1
                         : tour_goal_band_streak_player_contribution_format;
    return MakeString(Localize(sym, 0), i4);
}

void StreakTracker::SavePlayerStats() const {
    for (TrackerPlayerID id = mSource->GetFirstPlayer(); id.NotNull();
         id = mSource->GetNextPlayer(id)) {
        Player *pPlayer = mSource->GetPlayer(id);
        MILO_ASSERT(pPlayer, 0x141);
        std::map<TrackerPlayerID, PlayerStreakData>::const_iterator cData =
            mStreakDataMap.find(id);
        MILO_ASSERT(cData != mStreakDataMap.end(), 0x146);
        pPlayer->mStats.unk1c0 = cData->second.unk1c;
    }
}

void StreakTracker::TargetSuccess(int) const {}

DataArrayPtr StreakTracker::GetBroadcastDescription() const {
    static Symbol streak_tracker_explanation("streak_tracker_explanation");
    return DataArrayPtr(streak_tracker_explanation);
}

DataArrayPtr StreakTracker::GetTargetDescription(int idx) const {
    return TrackerDisplay::MakeTimeTargetDescription(mTargets[idx]);
}
