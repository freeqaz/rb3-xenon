#include "game/PerfectSectionTracker.h"
#include "beatmatch/TrackType.h"
#include "game/Game.h"
#include "game/TrackerSource.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "utl/Locale.h"
#include "utl/Symbols.h"
#include "utl/Symbols4.h"
#include "utl/TimeConversion.h"
#include <algorithm>
#include <cmath>

PerfectSectionTracker::PerfectSectionTracker(
    TrackerSource *src, TrackerBandDisplay &banddisp, TrackerBroadcastDisplay &bcdisp
)
    : Tracker(src, banddisp, bcdisp), unk58("chorus"), unkb0(1.0f), unkb4(-1.0f),
      unkb8(0), unkc0(0), unkc4(0), unkc8(-1), unke4(0), unke5(0) {}

PerfectSectionTracker::~PerfectSectionTracker() {}

void PerfectSectionTracker::ConfigureTrackerSpecificData(const DataArray *arr) {
    arr->FindData(required_accuracy, unkb0, false);
    arr->FindData(section_name, unk58, false);
    arr->FindData(require_all_players, unke5, false);
    unke8.InitFromDataArray(arr->FindArray(chain_multipliers, false));
}

void PerfectSectionTracker::HandlePlayerSaved_(const TrackerPlayerID &pid) {
    Player *pPlayer = mSource->GetPlayer(pid);
    MILO_ASSERT(pPlayer, 0x46);
    std::map<TrackType, PlayerStreakData>::iterator it =
        unk5c.find(pPlayer->GetTrackType());
    if (it != unk5c.end()) {
        it->second.unk1c = 0;
    }
    GetPlayerDisplay(pid).GainFocus(false);
}

void PerfectSectionTracker::FirstFrame_(float) {
    mBandDisplay.Initialize(perfect_section_tracker_description);
    mSectionData.clear();
    mSectionData.resize(unk104.GetSectionCount());
    unkac = 0;
    unkb4 = -1.0f;
    unkb8 = 0;
    unkbc = 0;
    unkc0 = 0;
    unkc4 = 0;
    unkc8 = -1;
    unke4 = 0;
    for (TrackerPlayerID id = mSource->GetFirstPlayer(); id.NotNull();
         id = mSource->GetNextPlayer(id)) {
        Player *pPlayer = mSource->GetPlayer(id);
        MILO_ASSERT(pPlayer, 0x6D);
        std::map<TrackType, PlayerStreakData>::iterator it =
            unk5c.find(pPlayer->GetTrackType());
        if (it != unk5c.end()) {
            it->second.unk1c = 0;
            if (pPlayer->IsLocal()) {
                GetPlayerDisplay(id).Enable();
            }
        }
    }
}

void PerfectSectionTracker::Poll_(float f) {
    if (mTargets.front() != 0.0f) {
        if (mSource->IsFinished()) {
            if (!unke4) {
                if (unkc0) {
                    HandleExitExtent(f, unkc4, false);
                }
                unke4 = true;
            }
            CheckForCompletedSections();
            return;
        }
        if (TheGame->unkdc == -1.0f) {
            if (unkc4 >= unk104.GetSectionCount()) {
                return;
            }
            int tick = (int)MsToTick(f);
            bool inSection = unk104.TickInSection(tick, unkc4);
            bool wasInSection = unk104.TickInSection(unkc8, unkc4);
            bool enteredExtent = !wasInSection && inSection;
            bool exitedExtent = wasInSection && !inSection;
            MILO_ASSERT(!(enteredExtent && exitedExtent), 0xAC);
            bool skipSection = false;
            if (exitedExtent) {
                int nextSection = unkc4 + 1;
                if (nextSection < unk104.GetSectionCount()
                    && unk104.TickInSection(tick, nextSection)) {
                    skipSection = true;
                }
            }
            ReachedAnyTarget();
            if (-1.0f == unkb4) {
                unkb4 = 0.0f;
            }
            float bc = unkbc;
            if (bc > 0.0f && f >= bc) {
                HandleEnterExtent(f, unkc4, true);
                unkbc = 0.0f;
                unkc0 = true;
            } else if (enteredExtent) {
                HandleEnterExtent(f, unkc4, false);
                unkc0 = true;
            } else if (exitedExtent) {
                HandleExitExtent(f, unkc4, skipSection);
                unkc0 = (bool)skipSection;
                if (skipSection) {
                    unkbc = f;
                }
            } else if (inSection && 0.0f == bc) {
                HandleInExtent(f, unkc4);
            }
            if (exitedExtent) {
                unkc4++;
            }
            CheckForCompletedSections();
            unkc8 = tick;
        }
    }
}

void PerfectSectionTracker::RemoteSectionComplete(
    Player *p, int iExtentIndex, int flags, int i
) {
    MILO_ASSERT_RANGE(iExtentIndex, 0, mSectionData.size(), 0xFC);
    TrackerPlayerID pid = mSource->FindPlayerID(p);
    if (pid.NotNull()) {
        LocalSectionComplete(pid, iExtentIndex, (SectionFlags)flags, (float)i / 10000.0f);
    }
}

void PerfectSectionTracker::CheckForCompletedSections() {
    if (unkac < mSectionData.size()) {
        int playercount = mSource->GetPlayerCount();
        if (playercount == 0)
            return;
        while (unkac < mSectionData.size() && mSectionData[unkac].unk0 >= playercount) {
            SectionData &cur = mSectionData[unkac];
            float curc = cur.unkc;
            int cur8 = cur.unk8;

            if (cur.unk4 > 0) {
                if (unke5) {
                    if (cur8 == cur.unk4) {
                        curc = unke8.GetMultiplier(++unkb8);
                        mBroadcastDisplay.ShowBriefBandMessage(
                            DataArrayPtr(perfect_section_band_tracker_success)
                        );
                    } else {
                        unkb8 = 0;
                        curc = 0;
                        Symbol sym = cur8 == 1 ? perfect_section_tracker_progress_1
                                               : perfect_section_tracker_progress;
                        mBroadcastDisplay.ShowBriefBandMessage(DataArrayPtr(sym, cur8));
                    }
                } else {
                    if (cur8 == cur.unk4) {
                        mBroadcastDisplay.ShowBriefBandMessage(
                            DataArrayPtr(perfect_section_band_tracker_success)
                        );
                    } else {
                        Symbol sym = cur8 == 1 ? perfect_section_tracker_progress_1
                                               : perfect_section_tracker_progress;
                        mBroadcastDisplay.ShowBriefBandMessage(DataArrayPtr(sym, cur8));
                    }
                }
            }
            unkac++;
            unkb4 += curc;
        }
    }
}

void PerfectSectionTracker::LocalSectionComplete(
    const TrackerPlayerID &pid, int iExtentIndex, SectionFlags flags, float f
) {
    MILO_ASSERT_RANGE(iExtentIndex, 0, mSectionData.size(), 0x16B);
    SectionData &cur = mSectionData[iExtentIndex];
    cur.unk0++;
    if (flags & 2) {
        cur.unk4++;
        if (flags & 1) {
            cur.unk8++;
            cur.unkc += f;
        }
    }
}

void PerfectSectionTracker::HandleEnterExtent(float f, int i, bool b) {
    for (TrackerPlayerID id = mSource->GetFirstPlayer(); id.NotNull();
         id = mSource->GetNextPlayer(id)) {
        if (mSource->IsPlayerLocal(id)) {
            Player *pPlayer = mSource->GetPlayer(id);
            MILO_ASSERT(pPlayer, 0x191);
            TrackType key = pPlayer->GetTrackType();
            std::map<TrackType, PlayerStreakData>::iterator it = unk5c.find(key);
            if (it != unk5c.end()) {
                if (!b) {
                    int iac = 0;
                    int ib0 = 0;
                    int i5 = unk104.GetSectionStartTick(i);
                    unk104.GetGemStatsInRange(pPlayer, i5, MsToTick(f), iac, ib0);
                    it->second.unk0 = pPlayer->mStats.mMissCount;
                    it->second.unk4 = pPlayer->mStats.mHitCount - iac;
                    it->second.unk8 = pPlayer->mStats.m0x0c - ib0;
                    it->second.unkc = unk104.CountGemsInSection(pPlayer, i);
                    it->second.unk10 = -1.0f;
                    it->second.unk14 = -1;
                    it->second.unk18 = false;
                }
                unk74[key] = false;
                if (it->second.unkc > 0) {
                    SetPlayerProgress(id, 0);
                    int multidx = unke8.GetMultiplierIndex(it->second.unk1c);
                    if (it->second.unk14 != multidx) {
                        GetPlayerDisplay(id).SetSecondaryStateLevel(multidx);
                        it->second.unk14 = multidx;
                    }
                    if (!it->second.unk19) {
                        GetPlayerDisplay(id).GainFocus(false);
                        it->second.unk19 = true;
                    }
                }
            }
        }
    }
}

void PerfectSectionTracker::HandleInExtent(float f, int i) {
    for (TrackerPlayerID id = mSource->GetFirstPlayer(); id.NotNull();
         id = mSource->GetNextPlayer(id)) {
        if (mSource->IsPlayerLocal(id)) {
            Player *pPlayer = mSource->GetPlayer(id);
            MILO_ASSERT(pPlayer, 0x1DA);
            TrackType key = pPlayer->GetTrackType();
            std::map<TrackType, PlayerStreakData>::iterator it = unk5c.find(key);
            if (it != unk5c.end()) {
                int total = it->second.unkc;
                if (total) {
                    int gemsLeft = total - (pPlayer->mStats.m0x0c - it->second.unk8);
                    int hits = pPlayer->mStats.mHitCount - it->second.unk4;
                    float fGemsLeft = (float)gemsLeft;
                    float progress;
                    if (0.0f == fGemsLeft) {
                        progress = 0.0f;
                    } else {
                        progress = (float)hits / fGemsLeft;
                    }
                    if (progress != it->second.unk10) {
                        float scaled = progress / unkb0;
                        if (scaled > 1.0f) scaled = 1.0f;
                        else if (scaled < 0.0f) scaled = 0.0f;
                        SetPlayerProgress(id, scaled);
                        if (progress >= unkb0 && !it->second.unk18) {
                            GetPlayerDisplay(id).SetSuccessState(true);
                            it->second.unk18 = true;
                        }
                        it->second.unk10 = progress;
                    }
                }
            }
        }
    }
}

void PerfectSectionTracker::HandleExitExtent(float f, int i, bool b) {
    for (TrackerPlayerID id = mSource->GetFirstPlayer(); id.NotNull();
         id = mSource->GetNextPlayer(id)) {
        if (mSource->IsPlayerLocal(id)) {
            Player *pPlayer = mSource->GetPlayer(id);
            MILO_ASSERT(pPlayer, 0x218);
            TrackType key = pPlayer->GetTrackType();
            std::map<TrackType, PlayerStreakData>::iterator it = unk5c.find(key);
            if (it != unk5c.end()) {
                int i118 = 0;
                int i11c = 0;
                int tick = unk104.GetSectionEndTick(unkc4);
                unk104.GetGemStatsInRange(pPlayer, tick, MsToTick(f), i118, i11c);
                float f17 = 0;
                int i15 = 0;
                bool b1 = false;
                bool b14 = false;
                if (it->second.unkc != 0) {
                    i15 |= 2;
                    b1 = true;
                    int i6 = it->second.unkc - (pPlayer->mStats.m0x0c - it->second.unk8)
                        - i11c;
                    int i12 = it->second.unk8
                        - (pPlayer->mStats.mHitCount - it->second.unk4) - i118;
                    if ((float)i12 / (float)i6 >= unkb0) {
                        i15 |= 3;
                        b14 = true;
                        unk8c[key]++;
                        f17 = unke8.GetMultiplier(it->second.unk1c++);
                    } else {
                        it->second.unk1c = 0;
                    }
                }
                if (b) {
                    it->second.unk0 = pPlayer->mStats.mMissCount;
                    it->second.unk4 = pPlayer->mStats.mHitCount - i118;
                    it->second.unk8 = pPlayer->mStats.m0x0c - i11c;
                    it->second.unkc = unk104.CountGemsInSection(pPlayer, unkc4 + 1);
                    it->second.unk10 = -1.0f;
                    it->second.unk14 = -1;
                    it->second.unk18 = false;
                }
                LocalSectionComplete(id, unkc4, (SectionFlags)i15, f17);
                static Message sectionMsg("send_tracker_section_complete", 0, 0, 0);
                sectionMsg[0] = unkc4;
                sectionMsg[1] = i15;
                sectionMsg[2] = (int)(f17 * 10000.0f);
                pPlayer->HandleType(sectionMsg);
                if (b1) {
                    if (b && it->second.unkc > 0) {
                        GetPlayerDisplay(id).Pulse(b14);
                    } else {
                        GetPlayerDisplay(id).LoseFocus(b14);
                        it->second.unk19 = false;
                    }
                }
            }
        }
    }
}

void PerfectSectionTracker::TranslateRelativeTargets() {
    unk104.Init();
    for (TrackerPlayerID id = mSource->GetFirstPlayer(); id.NotNull();
         id = mSource->GetNextPlayer(id)) {
        Player *player = mSource->GetPlayer(id);
        MILO_ASSERT(player, 0x2A5);
        TrackType tt = player->GetTrackType();
        PlayerStreakData &streak = unk5c[tt];
        streak.unk0 = 0;
        streak.unk4 = 0;
        streak.unk8 = 0;
        streak.unkc = 0;
        streak.unk10 = -1.0f;
        streak.unk14 = -1;
        streak.unk18 = false;
        streak.unk19 = false;
        streak.unk1c = 0;
        unk74[tt] = false;
        unk8c[tt] = 0;
    }
    int sectionCount = unk104.CountNonEmptySections(mSource, unke5 == 0);
    for (unsigned int i = 0; i < mTargets.size(); i++) {
        int trackerCount = (int)std::ceil((float)sectionCount * mTargets[i]);
        mTargets[i] = (float)std::max(1, trackerCount);
    }
}

void PerfectSectionTracker::UpdateGoalValueLabel(UILabel &label) const {
    label.SetTokenFmt(tour_goal_band_perfect_section_goal_format, (int)mTargets.front());
}

void PerfectSectionTracker::UpdateCurrentValueLabel(UILabel &label) const {
    label.SetTokenFmt(tour_goal_band_perfect_section_result_format, unkb4);
}

String PerfectSectionTracker::GetPlayerContributionString(Symbol s) const {
    TrackerPlayerID pid = mSource->GetIDFromInstrument(s);
    int f1 = 0;
    if (pid.NotNull()) {
        Player *pPlayer = mSource->GetPlayer(pid);
        MILO_ASSERT(pPlayer, 0x2D9);
        f1 = pPlayer->mStats.unk1c0;
    }
    Symbol sym = (int)f1 == 1 ? tour_goal_band_perfect_section_result_format_1
                              : tour_goal_band_perfect_section_result_format;
    return MakeString(Localize(sym, 0), f1);
}

void PerfectSectionTracker::SavePlayerStats() const {
    for (TrackerPlayerID id = mSource->GetFirstPlayer(); id.NotNull();
         id = mSource->GetNextPlayer(id)) {
        Player *pPlayer = mSource->GetPlayer(id);
        MILO_ASSERT(pPlayer, 0x2EF);
        TrackType tt = pPlayer->GetTrackType();
        std::map<TrackType, int>::const_iterator it = unk8c.find(tt);
        if (it != unk8c.end()) {
            pPlayer->mStats.unk1c0 = it->second;
        }
    }
}

DataArrayPtr PerfectSectionTracker::GetBroadcastDescription() const {
    return DataArrayPtr(perfect_section_tracker_explanation);
}