#include "game/FocusTracker.h"
#include "Player.h"
#include "TrackerUtils.h"
#include "beatmatch/TrackType.h"
#include "game/SongDB.h"
#include "game/TrackerDisplay.h"
#include "game/TrackerSource.h"
#include "meta_band/Utl.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "utl/HxGuid.h"
#include "utl/Locale.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols4.h"
#include "utl/TimeConversion.h"
#include <algorithm>

FocusTracker::FocusTracker(
    TrackerSource *src, TrackerBandDisplay &banddisp, TrackerBroadcastDisplay &bcdisp
)
    : Tracker(src, banddisp, bcdisp), mFocusDelayMs(5000.0f), mInFocusDelay(0),
      mFocusFlags((FocusFlags)0), unk74(0), unk78(0), unk7c(0), unk80(0), unk84(0),
      unk88(0), unk8c(0), unkc8(0) {}

FocusTracker::~FocusTracker() {}

void FocusTracker::ConfigureTrackerSpecificData(const DataArray *arr) {
    arr->FindData(focus_delay_ms, mFocusDelayMs, false);
    unka8.InitFromDataArray(arr->FindArray(chain_multipliers, false));
}

void FocusTracker::HandleRemovePlayer_(Player *p) {
    if (mFocusPlayer.NotNull()) {
        if (mSource->GetPlayerCount() == 0) {
            mFocusPlayer = gNullUserGuid;
        } else {
            TrackerPlayerID focus = mFocusPlayer;
            if (!mSource->HasPlayer(mFocusPlayer)) {
                bool b48;
                focus = GetNextFocusPlayer(mFocusPlayer, unk7c, b48);
                int flags = 1;
                if (b48)
                    flags |= 8;
                SetFocusPlayer(focus, unk7c, (FocusFlags)flags);
            }
            mFocusPlayer = focus;
        }
    }
}

void FocusTracker::HandleGameOver_(float) {}

void FocusTracker::Restart_() {
    unk74 = false;
    unk78 = 0;
    unk7c = 0;
    unk80 = 0;
    unk84 = 0;
    unk88 = 0;
    unk8c = 0;
    unkc8 = false;
}

void FocusTracker::HandlePlayerSaved_(const TrackerPlayerID &pid) {
    if (mSource->IsPlayerLocal(pid)) {
        GetPlayerDisplay(pid).Enable();
    }
}

void FocusTracker::FirstFrame_(float f) {
    mFocusCountMap.clear();
    mFocusPlayer = TrackerPlayerID();
    for (TrackerPlayerID id = mSource->GetFirstPlayer(); id.NotNull();
         id = mSource->GetNextPlayer(id)) {
        mFocusCountMap[id] = 0;
        if (mSource->IsPlayerLocal(id)) {
            GetPlayerDisplay(id).Enable();
        }
    }
    if (mSource->GetPlayerCount() > 0) {
        bool b98;
        TrackerPlayerID cFirst = GetFirstFocusPlayer(b98);
        MILO_ASSERT(cFirst.NotNull(), 0x9E);
        if (mSource->IsPlayerLocal(cFirst)) {
            int flags = 5;
            if (b98)
                flags |= 8;
            SetFocusPlayer(cFirst, f, (FocusFlags)flags);
        }
        mFocusPlayer = cFirst;
    }
    mBandDisplay.Initialize(gNullStr);
    unkc4 = -1;
}

void FocusTracker::Poll_(float f) {
    if (!unkc8 && mFocusPlayer.NotNull()) {
        if (mSource->IsPlayerLocal(mFocusPlayer) && !PlayerCanHaveFocus(mFocusPlayer)) {
            bool b75;
            TrackerPlayerID next = GetNextFocusPlayer(mFocusPlayer, f, b75);
            int flags = 1;
            if (b75)
                flags |= 8;
            SetFocusPlayer(next, f, (FocusFlags)flags);
        }
        if (mInFocusDelay) {
            if (f > unk78)
                ActivateFocus(f);
            else
                return;
        }
        bool b76 = false;
        bool b77 = false;
        CheckCondition(f, mSource->IsFinished(), b76, b77);
        if (mSource->IsPlayerLocal(mFocusPlayer)) {
            if (b76) {
                if (b77 && !unk74) {
                    float mult = unka8.GetMultiplier(++unk8c);
                    unk88++;
                    unk84 += mult;
                    mFocusCountMap[mFocusPlayer]++;
                }
                bool b78;
                TrackerPlayerID next = GetNextFocusPlayer(mFocusPlayer, f, b78);
                int flags = 0;
                if (b77)
                    flags = 2;
                if (b78)
                    flags |= 8;
                if (next.mGuid == mFocusPlayer.mGuid && b78 == unk74)
                    flags |= 0x10;
                SetFocusPlayer(next, f, (FocusFlags)flags);
            }
            int idx = unka8.GetMultiplierIndex(unk8c);
            if (unkc4 != idx) {
                if (mFocusPlayer.NotNull()) {
                    GetPlayerDisplay(mFocusPlayer).SetSecondaryStateLevel(idx);
                }
                unkc4 = idx;
            }
        }
        if (f - unk80 > 1000.0f) {
            unk80 = f;
        }
        unk7c = f;
        if (mSource->IsFinished() && b76) {
            unkc8 = true;
        }
    }
}

void FocusTracker::TargetSuccess(int) const {}

DataArrayPtr FocusTracker::GetTargetDescription(int i) const {
    return TrackerDisplay::MakeIntegerTargetDescription(mTargets[i]);
}

void FocusTracker::UpdateGoalValueLabel(UILabel &label) const {
    // Retail function-local static (guard 0x82E03358 bit 0x1, storage
    // 0x82E03354); the storage address is materialised into r30 and passed
    // straight to SetTokenFmt.
    static Symbol tour_goal_focus_goal_format("tour_goal_focus_goal_format");
    label.SetTokenFmt(tour_goal_focus_goal_format, (int)mTargets.front());
}

void FocusTracker::UpdateCurrentValueLabel(UILabel &label) const {
    static Symbol tour_goal_focus_result_format("tour_goal_focus_result_format");
    label.SetTokenFmt(tour_goal_focus_result_format, (int)unk84);
}

String FocusTracker::GetPlayerContributionString(Symbol s) const {
    // Retail (0x826D6C90) initialises this function-local static at the top
    // (guard 0x82E03318 bit 0x1, storage 0x82E03314) and then NEVER READS the
    // storage again -- the only two references to 0x82E03314 in the whole
    // function are the ctor's own lis/addi. The consumer was a MILO_* that
    // retail no-op'd; the Symbol ctor call survives because it has side
    // effects. So the static is declared and deliberately unused.
    static Symbol tour_goal_focus_player_contribution_format(
        "tour_goal_focus_player_contribution_format"
    );
    TrackerPlayerID pid = mSource->GetIDFromInstrument(s);
    int i3 = 0;
    if (pid.NotNull()) {
        Player *pPlayer = mSource->GetPlayer(pid);
        MILO_ASSERT(pPlayer, 0x169);
        i3 = pPlayer->mStats.unk1c0;
    }
    return MakeString(Localize(GetContributionToken(i3), 0), i3);
}

Symbol FocusTracker::GetContributionToken(int) const {
    static Symbol tour_goal_focus_player_contribution_format("tour_goal_focus_player_contribution_format");
    return tour_goal_focus_player_contribution_format;
}

void FocusTracker::SavePlayerStats() const {
    for (TrackerPlayerID id = mSource->GetFirstPlayer(); id.NotNull();
         id = mSource->GetNextPlayer(id)) {
        Player *pPlayer = mSource->GetPlayer(id);
        MILO_ASSERT(pPlayer, 0x17C);
        Stats &stats = pPlayer->mStats;
        std::map<TrackerPlayerID, int>::const_iterator cData = mFocusCountMap.find(id);
        MILO_ASSERT(cData != mFocusCountMap.end(), 0x181);
        stats.unk1c0 = cData->second;
    }
}

TrackerPlayerID
FocusTracker::GetNextFocusPlayer(const TrackerPlayerID &pid, float f, bool &b) const {
    TrackerPlayerID empty;
    TrackerPlayerID ret(pid);
    bool b1;
    do {
        TrackerPlayerID filler(ret);
        ret = mSource->GetNextPlayer(ret);
        if (!ret.NotNull()) {
            ret = mSource->GetFirstPlayer();
        }
        bool wantsfocus = PlayerWantsFocus(ret, f);
        b = !wantsfocus;
        if (ret.mGuid == pid.mGuid)
            break;
        if (!empty.NotNull()) {
            empty = ret;
        } else if (ret.mGuid == empty.mGuid) {
            break;
        }
        // ★ LANE W1b-GAME: THE BOOL MATERIALIZATION IS SETTLED -- the lever is
        // `& 1` AT THE CALL, and it is NOT reachable from the helper at all.
        // Retail materialises the inlined PlayerCanHaveFocus bool as a VALUE
        // (`cntlzw`/`extrwi.`) and branches on THAT; we folded it to
        // `cmpwi`/`bne`.  `x & 1` on a bool is semantically a no-op (a bool is
        // already 0/1) but puts the operand in a VALUE context, which is what
        // forces MSVC to emit the materialisation while KEEPING the branch.
        //
        // MEASURED INERT -- 10 structurally distinct forms, ALL BYTE-IDENTICAL
        // (Streak 98.7%/score 220, Focus 97.5%/score 320 for every one), so do
        // NOT re-attack the helper: `== kPlayerEnabled`, `!x`, ternary, local
        // bool, pointer local, int local, `(bool)` cast, `!!`, `!(x != k)`, and
        // defining the helper INLINE IN THE HEADER (front-end vs back-end
        // inlining).  Also inert at the call site: `(int)`, `== true`, int
        // local, `!= false`, `!!`.  The previous note here said "attack
        // PlayerCanHaveFocus itself" -- that is REFUTED; the helper body is
        // measurably irrelevant.
        //
        // NEGATIVE RESULTS worth keeping: plain `&` instead of `&&` DOES force
        // the materialisation (idx 113 `cntlzw` matches) but collapses the
        // branch into `and.` and scores WORSE overall (97.5 -> 95.5); and
        // swapping the operands to `!(wantsfocus && (canfocus & 1))` scores
        // 93.7 -- independently reproducing lane CN-3e's result that these
        // boolean twins are NOT interchangeable to MSVC.
        //
        // Lane W1-GAME: the LOOP FLAG POLARITY IS SETTLED -- retail's flag is
        // the NEGATION.  Retail (0x826D6DA0 idx 115-121):
        //     beq .E88                                   ; !canfocus -> 1
        //     cmplwi cr6,r28,0; li r11,0; bne cr6,.E8C    ; wantsfocus -> 0
        //   .E88: li r11,1
        //   .E8C: clrlwi. r11,r11,24; bne <loop>
        // i.e. b1 = !(canfocus && wantsfocus), looped on bne.  Writing exactly
        // that closed 2 of the 6 charges (the `li` +/-1 pair at idx 117/119 and
        // the bne/beq at 121).  NOTE lane CN-3e tried the De Morgan twin
        // `!canfocus || !wantsfocus` and measured it WORSE -- the twins are NOT
        // interchangeable to MSVC (same lesson as GemPlayer::LocalSetEnabledState,
        // where three semantically identical spellings scored 96.9 / 97.6 / 100.0).
        //
        // Caveat for whoever reads the number: fuzzy went 98.198 -> 97.500 even
        // though mismatches went 6 -> 4, because one `delete` scores worse than
        // two `diff_arg`s.  Both forms are sub-100, so matched_code is 0 either
        // way; this form is kept because the residual is now ONE mechanism.
        //
        // RESIDUAL (idx 113-117, all that is left): retail materialises the
        // inlined PlayerCanHaveFocus bool with cntlzw/extrwi. and tests THAT,
        // where we fold to cmpwi/bne.  The SAME shape blocks
        // StreakFocusTracker::GetNextFocusPlayer, so the cause is the HELPER,
        // not the call site.  Measured inert here: inlining the call into the
        // && instead of hoisting `canfocus` is BYTE-IDENTICAL (97.500 both,
        // same 4 charges at the same indices) -- independently reproducing
        // INSDEL-1's result on the sibling.  Do not re-try hoist-vs-inline;
        // attack PlayerCanHaveFocus itself.
        bool canfocus = PlayerCanHaveFocus(ret) & 1;
        b1 = !(canfocus && wantsfocus);
    } while (b1);
    return ret;
}

bool FocusTracker::PlayerCanHaveFocus(const TrackerPlayerID &pid) const {
    return mSource->GetPlayer(pid)->mEnabledState == kPlayerEnabled;
}

void FocusTracker::SetFocusPlayer(const TrackerPlayerID &pid, float f, FocusFlags flags) {
    LocalSetFocusPlayer(pid, f, unk88, unk84, flags);
    Player *pPlayer = mSource->GetPlayer(pid);
    MILO_ASSERT(pPlayer, 0x1D6);
    static Message focusMsg("send_tracker_focus", 0, 0, 0);
    focusMsg[0] = unk88;
    focusMsg[1] = (int)(unk84 * 10000.0f);
    focusMsg[2] = flags;
    pPlayer->HandleType(focusMsg);
}

void FocusTracker::LocalSetFocusPlayer(
    const TrackerPlayerID &pid, float f1, int i, float f2, FocusFlags flags
) {
    if (mFocusPlayer.NotNull()) {
        SetTrackFocus(mFocusPlayer, false, flags);
    }
    FocusLeaving(flags);
    float ff = 0;
    unk74 = flags & 8;
    if (!(flags & 1)) {
        ff = mFocusDelayMs;
    }
    // Retail stores the delayed-focus time (0x84) before the pending value
    // (0x90) -- objdiff had our two stfs emitted in the opposite order.
    unk78 = f1 + ff;
    unk84 = f2;
    unk88 = i;
    mBandDisplay.HandleIncrement();
    mFocusFlags = flags;
    mInFocusDelay = true;
    if (flags & 2) {
        BroadcastFocusSuccess();
    }
    mFocusPlayer = pid;
}

void FocusTracker::ActivateFocus(float f) {
    HandleFocusSwitch(f);
    SetTrackFocus(mFocusPlayer, true, mFocusFlags);
    mInFocusDelay = false;
}

void FocusTracker::RemoteSetFocusPlayer(Player *p, int i1, int i2, FocusFlags flags) {
    TrackerPlayerID pid = mSource->FindPlayerID(p);
    if (pid.NotNull()) {
        LocalSetFocusPlayer(pid, unk7c, i1, (float)i2 / 10000.0f, flags);
    }
}

TrackerPlayerID FocusTracker::GetFirstFocusPlayer(bool &b) const {
    TrackerPlayerID pid = mSource->GetFirstPlayer();
    return GetNextFocusPlayer(pid, 0, b);
}

void FocusTracker::SetTrackFocus(const TrackerPlayerID &pid, bool b, FocusFlags flags) {
    if (mSource->IsPlayerLocal(pid)) {
        if (b) {
            if (!(flags & 8) && !(flags & 0x10)) {
                GetPlayerDisplay(pid).GainFocus(flags & 4);
                int idx = unka8.GetMultiplierIndex(unk8c);
                if (mFocusPlayer.NotNull()) {
                    GetPlayerDisplay(pid).SetSecondaryStateLevel(idx);
                }
                unkc4 = idx;
            }
        } else if (!unk74) {
            bool flag2 = flags & 2;
            if (flags & 0x10) {
                GetPlayerDisplay(pid).Pulse(flag2);
            } else {
                GetPlayerDisplay(pid).LoseFocus(flag2);
            }
        }
    }
}

StreakFocusTracker::StreakFocusTracker(
    TrackerSource *src, TrackerBandDisplay &banddisp, TrackerBroadcastDisplay &bcdisp
)
    : FocusTracker(src, banddisp, bcdisp), unkcc(0), unkd0(-1), unkd4(10000.0f), unke0(0),
      unke4(0) {}

void StreakFocusTracker::ConfigureTrackerSpecificData(const DataArray *arr) {
    FocusTracker::ConfigureTrackerSpecificData(arr);
    arr->FindData(focus_streak_length_multiplier, unkcc, true);
    arr->FindData(focus_streak_max_note_gap_ms, unkd4, false);
}

void StreakFocusTracker::TranslateRelativeTargets() {
    unke8.clear();
    for (TrackerPlayerID id = mSource->GetFirstPlayer(); id.NotNull();
         id = mSource->GetNextPlayer(id)) {
        Player *pPlayer = mSource->GetPlayer(id);
        MILO_ASSERT(pPlayer, 0x29A);
        int count;
        if (pPlayer->GetTrackType() == kTrackVocals) {
            count = TheSongDB->GetVocalNoteList(0)->mPhrases.size();
        } else {
            count = TrackerUtils::CountGemsInSong(
                pPlayer->GetTrackNum(), pPlayer->GetTrackType()
            );
        }
        int value = (float)count * unkcc;
        unke8[id] = std::max(value, 1);
    }

    float fcc = 1.0f / unkcc;
    for (int i = 0; i < mTargets.size(); i++) {
        float result = std::floor(fcc * mTargets[i]);
        mTargets[i] = result;
    }
}

bool StreakFocusTracker::PlayerWantsFocus(const TrackerPlayerID &pid, float f) const {
    Player *pPlayer = mSource->GetPlayer(pid);
    MILO_ASSERT(pPlayer, 0x2C6);
    int trackNum = pPlayer->GetTrackNum();
    TrackType trackType = pPlayer->GetTrackType();
    float nextNoteMs = TrackerUtils::GetNextNoteMs(trackNum, trackType, f);
    return nextNoteMs <= unkd4;
}

TrackerPlayerID StreakFocusTracker::GetNextFocusPlayer(
    const TrackerPlayerID &pid, float f, bool &b
) const {
    b = false;
    TrackerPlayerID ret;
    float f5 = 8.999999E+9f;
    TrackerPlayerID iterid(pid);
    do {
        iterid = mSource->GetNextPlayer(iterid);
        if (iterid.mGuid == gNullUserGuid) {
            iterid = mSource->GetFirstPlayer();
        }
        if (ret.NotNull() && ret.mGuid == iterid.mGuid)
            break;
        // NOTE(INSDEL-1): the residual 3 charges here (retail `cntlzw`+`extrwi.`
        // materialising the inlined PlayerCanHaveFocus bool, vs our folded
        // `cmpwi`/`bne`) are the SAME MSVC bool-materialization lane CN-3e
        // characterised at line ~230 for FocusTracker::GetNextFocusPlayer.
        // Re-tested here rather than inherited: hoisting to `bool canfocus = ...;`
        // is BYTE-IDENTICAL to this form (98.728325 both, same 3 charges at the
        // same indices).  ★ W1b-GAME: the "permuter class" label was WRONG -- it
        // was a symptom, not a diagnosis.  `& 1` puts the inlined bool in a VALUE
        // context and crosses this row to 100.0%/score 0.  See the long note in
        // FocusTracker::GetNextFocusPlayer for the 10 inert forms.
        if (PlayerCanHaveFocus(iterid) & 1) {
            Player *pPlayer = mSource->GetPlayer(iterid);
            MILO_ASSERT(pPlayer, 0x2F5);
            int num = pPlayer->GetTrackNum();
            TrackType ty = pPlayer->GetTrackType();
            float nextnotems = TrackerUtils::GetNextNoteMs(num, ty, f);
            if (nextnotems - f < unkd4) {
                ret = iterid;
                break;
            }
            if (ret.mGuid == gNullUserGuid || nextnotems < f5) {
                ret = iterid;
                f5 = nextnotems;
            }
        }
    } while (!(iterid.mGuid == pid.mGuid));
    if (ret.mGuid == gNullUserGuid) {
        ret = iterid;
    }
    return ret;
}

void StreakFocusTracker::CheckCondition(float f1, bool b1, bool &bref1, bool &bref2) {
    bref1 = false;
    bref2 = false;
    Player *pPlayer = mSource->GetPlayer(mFocusPlayer);
    int hitcount = pPlayer->mStats.mHitCount;
    int curstreak = pPlayer->mStats.GetCurrentStreak();
    bool haveStreak = curstreak > 0;
    if (haveStreak && !unke4) {
        unkd8 = hitcount - 1;
        unke4 = true;
    } else if (!haveStreak) {
        if (unke4 && mSource->IsPlayerLocal(mFocusPlayer)) {
            GetPlayerDisplay(mFocusPlayer).Pulse(false);
        }
        unke4 = false;
        unk8c = 0;
    }
    int i2 = 0;
    if (unke4) {
        i2 = hitcount - unkd8;
        if (i2 >= unkd0) {
            bref1 = true;
            bref2 = true;
            int val = unk88 + 1;
            Symbol toUse = val == 1 ? streak_focus_tracker_progress_1
                                    : streak_focus_tracker_progress;
            mBroadcastDisplay.ShowBriefBandMessage(DataArrayPtr(toUse, val));
        }
    }

    if (unke0 != i2) {
        SetPlayerProgress(mFocusPlayer, (float)i2 / (float)unkd0);
        unke0 = i2;
    }
}

void StreakFocusTracker::HandleFocusSwitch(float f) {
    Player *pPlayer = mSource->GetPlayer(mFocusPlayer);
    unke4 = pPlayer->mStats.GetCurrentStreak() > 0;
    unkd8 = pPlayer->mStats.mHitCount;
    unkdc = pPlayer->mStats.mMissCount;
    unkd0 = unke8[mFocusPlayer];
}

DataArrayPtr StreakFocusTracker::GetBroadcastDescription() const {
    static Symbol streak_focus_tracker_explanation("streak_focus_tracker_explanation");
    return DataArrayPtr(streak_focus_tracker_explanation);
}

void StreakFocusTracker::BroadcastFocusSuccess() const {
    Symbol toUse =
        unk88 == 1 ? streak_focus_tracker_progress_1 : streak_focus_tracker_progress;
    mBroadcastDisplay.ShowBriefBandMessage(DataArrayPtr(toUse, unk88));
}
void StreakFocusTracker::BroadcastSuccess(int) const {}

AccuracyFocusTracker::AccuracyFocusTracker(
    TrackerSource *src, TrackerBandDisplay &banddisp, TrackerBroadcastDisplay &bcdisp
)
    : FocusTracker(src, banddisp, bcdisp), unkcc(-1), mRequiredAccuracy(0) {}

void AccuracyFocusTracker::CheckCondition(float ms, bool b1, bool &bref1, bool &bref2) {
    bref1 = false;
    bref2 = false;
    if (mSource->IsPlayerLocal(mFocusPlayer)) {
        int mstick = MsToTick(ms);
        Player *pPlayer = mSource->GetPlayer(mFocusPlayer);
        int i44 = 0;
        int i48 = 0;
        if (mSectionManager.TickAfterSection(mstick, unkcc) != 0 || b1) {
            int sectionEndTick = mSectionManager.GetSectionEndTick(unkcc);
            mSectionManager.GetGemStatsInRange(pPlayer, sectionEndTick, mstick, i44, i48);
        }
        int i5 = unke0 - (pPlayer->mStats.m0x0c - unkdc) + i48;
        float f1 = 0;
        float f2 = i5;
        i5 = (pPlayer->mStats.mHitCount - i44) - unkd4;
        if (f2 > 0) {
            f1 = (float)i5 / f2;
        }
        if (f1 != unke4) {
            SetPlayerProgress(mFocusPlayer, f1);
            if (f1 >= mRequiredAccuracy && !unke8) {
                GetPlayerDisplay(mFocusPlayer).SetSuccessState(true);
                unke8 = true;
            }
            unke4 = f1;
        }
        if (mSectionManager.TickAfterSection(mstick, unkcc) || b1) {
            bref1 = true;
            bref2 = f1 >= mRequiredAccuracy;
        }
    }
}

void AccuracyFocusTracker::HandleFocusSwitch(float f) {
    int i34;
    int i38;
    int msTick = MsToTick(f);
    unkcc = mSectionManager.FindSectionContainingTick(msTick);
    if (unkcc != -1) {
        Player *pPlayer = mSource->GetPlayer(mFocusPlayer);
        i34 = 0;
        i38 = 0;
        if (unkcc > 0) {
            int tick = mSectionManager.GetSectionEndTick(unkcc - 1);
            msTick = MsToTick(f);
            mSectionManager.GetGemStatsInRange(pPlayer, tick, msTick, i34, i38);
        }
        unkd4 = pPlayer->mStats.mHitCount - i34;
        // NOTE: in AccuracyFocusTracker the header declares unke0 before unkdc,
        // so unke0 lives at +0xe8 and unkdc at +0xec (the reverse of what the
        // stale rb3-Wii-derived names suggest -- every member of this class is
        // +0xc from its name). Retail stores the gem-count baseline to +0xe8 and
        // the CountGemsInSection result to +0xec, hence the pairing below.
        // Do NOT "fix" the names without also swapping the uses in
        // CheckCondition(), which currently matches at 100%.
        unke0 = pPlayer->mStats.m0x0c - i38;
        unkd8 = pPlayer->mStats.mMissCount;
        unkdc = mSectionManager.CountGemsInSection(pPlayer, unkcc);
        unke4 = -1.0f;
        unke8 = false;
    }
}

DataArrayPtr AccuracyFocusTracker::GetBroadcastDescription() const {
    return DataArrayPtr(accuracy_focus_tracker_explanation);
}

void AccuracyFocusTracker::ConfigureTrackerSpecificData(const DataArray *arr) {
    FocusTracker::ConfigureTrackerSpecificData(arr);
    if (mFocusDelayMs != 0) {
        MILO_WARN(
            "AccuracyFocusTracker needs to have focus_delay_ms set to 0.0 in quests.dta!"
        );
        mFocusDelayMs = 0;
    }
    mSectionManager.Init();
    static Symbol required_accuracy("required_accuracy");
    arr->FindData(required_accuracy, mRequiredAccuracy, true);
}

void AccuracyFocusTracker::TranslateRelativeTargets() {
    float f31 = (float)mSectionManager.CountNonEmptySections(mSource, false);
    for (int i = 0; i < mTargets.size(); i++) {
        float target = mTargets[i];
        float translated = std::floor(target * f31);
        mTargets[i] = translated;
    }
}

bool AccuracyFocusTracker::PlayerWantsFocus(const TrackerPlayerID &pid, float ms) const {
    return mSectionManager.CountGemsInSection(
               mSource->GetPlayer(pid),
               mSectionManager.FindSectionContainingTick(MsToTick(ms))
           )
        > 0;
}

void AccuracyFocusTracker::FocusLeaving(FocusFlags flags) {
    if (!(flags & 2))
        unk8c = 0;
}

void AccuracyFocusTracker::BroadcastFocusSuccess() const {
    Player *pPlayer = mSource->GetPlayer(mFocusPlayer);
    MILO_ASSERT(pPlayer, 0x43B);
    const char *fontchar = GetFontCharFromTrackType(pPlayer->GetTrackType(), 0);
    mBroadcastDisplay.ShowBriefBandMessage(
        DataArrayPtr(accuracy_focus_tracker_progress, fontchar)
    );
}
