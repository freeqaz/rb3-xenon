#include "meta_band/NextSongPanel.h"
#include "meta_band/AppInlineHelp.h"
#include "bandobj/ReviewDisplay.h"
#include "bandobj/ScoreDisplay.h"
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "game/Defines.h"
#include "game/Game.h"
#include "game/NetGameMsgs.h"
#include "game/Player.h"
#include "net/NetSession.h"
#include "meta_band/AccomplishmentManager.h"
#include "meta_band/AppLabel.h"
#include "meta_band/BandProfile.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/MetaPerformer.h"
#include "meta_band/ProfileMgr.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/ObjMacros.h"
#include "obj/ObjPtr_p.h"
#include "obj/Task.h"
#include "obj/Utl.h"
#include "os/Debug.h"
#include "rndobj/Dir.h"
#include "rndobj/Draw.h"
#include "rndobj/Group.h"
#include "rndobj/Trans.h"
#include "ui/UILabel.h"
#include "ui/UIPanel.h"
#include "math/Utl.h"
#include "utl/Locale.h"
#include "utl/MakeString.h"
#include "utl/Std.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"

void NextSongPanel::Enter() {
    unk98 = false;
    for (int i = 0; i < 4; i++) {
        if (TheGame->IsActiveUser(TheBandUserMgr->GetUserFromSlot(i))) {
            DeterminePerformanceAwards(i);
        }
    }
    UIPanel::Enter();
    mEnterTime = TheTaskMgr.UISeconds();
    for (int i = 0; i < 4; i++) {
        unk70[i] = 0;
        unk84[i] = 0;
        unk80[i] = 0;
        HideAllDetailComponents(i);
        if (TheGame->IsActiveUser(TheBandUserMgr->GetUserFromSlot(i))) {
            FillExpandedDetails(i);
        }
        ScrollExpandedDetails(i, 0, true);
    }
}

void NextSongPanel::Exit() {
    UIPanel::Exit();
    DeleteAll(mDetailLabels);
}

bool NextSongPanel::Exiting() const {
    // Boolean-accumulator goto: `ret=false; ...goto done; ret=true; done: return ret;` keeps
    // `ret` materialized in r31. Rewrite to multi-return collapses the var to inline `li r3, 0/1`,
    // drops the r31 save, cascades regswap r30<->r31, flips bge<->blt (100% -> 79.1%). Leave as-is.
    bool ret = false;
    if (!UIPanel::Exiting()) {
        float uisecs = TheTaskMgr.UISeconds();
        float mintimeprop = Property("min_time", true)->Float();
        if (!(uisecs - mEnterTime < mintimeprop))
            goto done;
    }
    done:
    return ret = true;
}

void NextSongPanel::FinishLoad() {
    UIPanel::FinishLoad();
    // laneAY-B: retail's missing statement (target 0x82643FF8+0x18):
    //     mDir->Find<...>("highscore_1.lbl", true) -> +0x1bc = 1.0f
    // The +0x1bc field falls inside UILabel's still-unreconstructed retail tail
    // (UILabel.h `mUnkTU5Tail`), so it is written through a byte offset rather
    // than a named member. The Find<T> instantiation is ICF-folded in retail
    // (its symbol reads as Find<RndAnimatable>); any T emits identical code.
    *(float *)((char *)mDir->Find<UILabel>("highscore_1.lbl", true) + 0x1BC) = 1.0f;
    static Symbol details_page_size("details_page_size");
    static Symbol details_footer_size("details_footer_size");
    static Symbol details_scroll_step("details_scroll_step");
    const DataArray *t = TypeDef();
    MILO_ASSERT(t, 0x5A);
    // laneAX-W7 diagnosis (now RESOLVED by laneAY-B): the four function-local
    // `static Symbol`s are REAL -- guard word 0x82E01854, bits 0x1/0x2/0x4/0x8,
    // static objects at 0x82E01850/4C/48/44. Adding them ALONE measured
    // 52.11% -> 40.6%; the missing piece was the "highscore_1.lbl" statement
    // above. With both applied this function is 99.5% (residue = a pure
    // callee-saved regswap in the loop: target i=r29/rdir=r28/scrollgrpstr=r26,
    // ours i=r28/rdir=r29/scrollgrpstr=r25 -- permuter-class, and hoisting
    // `rdir` out of the loop or fusing the Reserve() args does not move it).
    // NOTE the declaration SHAPE matters: retail declares page/footer/step
    // together BEFORE TypeDef(), and scroll_group separately after the three
    // FindFloat calls; that is what reproduces the guard-block placement.
    mDetailsPageSize = t->FindFloat(details_page_size);
    mDetailsFooterSize = t->FindFloat(details_footer_size);
    mDetailsScrollStep = t->FindFloat(details_scroll_step);
    static Symbol details_scroll_group("details_scroll_group");
    const char *scrollgrpstr = t->FindStr(details_scroll_group);
    for (int i = 0; i < 4; i++) {
        RndDir *rdir = mDir->Find<RndDir>(MakeString("slot%i", i), true);
        mScrollGroups[i] = rdir->Find<RndGroup>(scrollgrpstr, true);
        int hashtablesize = rdir->HashTableSize() + 0x100;
        int strtablesize = rdir->StrTableSize();
        rdir->Reserve(hashtablesize, strtablesize + 0x980);
    }
}

void NextSongPanel::Poll() {
    UIPanel::Poll();
    for (int i = 0; i < 4; i++) {
        float pageSize = mDetailsPageSize;
        if (mDetailsHeight[i] <= pageSize)
            continue;
        float target = pageSize * unk70[i];
        Transform xfm(mScrollGroups[i]->LocalXfm());
        if (!unk98) {
            xfm.v.z = target;
        } else if (xfm.v.z < target) {
            GetMaxScrollPage(i);
            xfm.v.z += mDetailsScrollStep * TheTaskMgr.DeltaUISeconds() * 30.0f;
            if (xfm.v.z > target)
                xfm.v.z = target;
        } else if (xfm.v.z > target) {
            xfm.v.z -= mDetailsScrollStep * TheTaskMgr.DeltaUISeconds() * 30.0f;
            if (xfm.v.z < target)
                xfm.v.z = target;
        }
        mScrollGroups[i]->SetLocalXfm(xfm);
    }
    unk98 = true;
}

int NextSongPanel::GetMaxScrollPage(int i) {
    return mDetailsHeight[i] / mDetailsPageSize;
}

void NextSongPanel::DeterminePerformanceAwards(int i) {
    BandUser *user = TheBandUserMgr->GetUserFromSlot(i);
    MILO_ASSERT(user, 0xAA);
    MILO_ASSERT(TheGame->IsActiveUser(user), 0xAB);
    Player *player = user->mPlayer;
    MILO_ASSERT(player, 0xAD);
    player->DeterminePerformanceAwards();
}

void NextSongPanel::HideAllDetailComponents(int i) {
    const DataArray *t = TypeDef();
    MILO_ASSERT(t, 0xB6);
    const char *parentstr = t->FindStr(detail_component_parent);
    RndDir *rdir = mDir->Find<RndDir>(MakeString("slot%i", i), true);
    RndGroup *grp = rdir->Find<RndGroup>(parentstr, true);
    for (ObjPtrList<Hmx::Object>::iterator it = grp->Objects().begin();
         it != grp->Objects().end();
         ++it) {
        RndDrawable *d = dynamic_cast<RndDrawable *>(*it);
        if (d)
            d->SetShowing(false);
    }
}

bool NextSongPanel::CanChangeSongReview(int i) const {
    BandUser *user = TheBandUserMgr->GetUserFromSlot(i);
    MILO_ASSERT(user, 199);
    LocalBandUser *localUser = user->GetLocalBandUser();
    MILO_ASSERT(localUser, 0xCA);
    return localUser->CanSaveData();
}

void NextSongPanel::InitializeSongReviewDisplay(int i) {
    RndDir *dir = mDir->Find<RndDir>(MakeString("slot%i", i), true);
    MILO_ASSERT(dir, 0xD3);
    ReviewDisplay *reviewDisplay = dir->Find<ReviewDisplay>("song_review.rvw", true);
    MILO_ASSERT(reviewDisplay, 0xD5);
    AppInlineHelp *reviewHelp = dir->Find<AppInlineHelp>("review.ihp", true);
    MILO_ASSERT(reviewHelp, 0xD8);
    BandUser *user = TheBandUserMgr->GetUserFromSlot(i);
    if (!user || !user->IsLocal()) {
        reviewDisplay->SetShowing(false);
        reviewHelp->SetShowing(false);
    } else {
        LocalBandUser *localUser = user->GetLocalBandUser();
        MILO_ASSERT(localUser, 0xE3);
        if (TheGame->IsActiveUser(user) && localUser->CanSaveData()) {
            reviewDisplay->SetShowing(true);
            reviewHelp->SetShowing(true);
            reviewHelp->SetOverrideUser(localUser);
            MetaPerformer *performer = MetaPerformer::Current();
            MILO_ASSERT(performer, 0xEF);
            Symbol completedSong = performer->GetCompletedSong();
            int songID = TheSongMgr.GetSongIDFromShortName(completedSong, true);
            BandProfile *profile = TheProfileMgr.GetProfileForUser(localUser);
            MILO_ASSERT(profile, 0xF5);
            int songReview = profile->GetSongReview(songID);
            reviewDisplay->SetValues(songReview, false);
        } else {
            reviewDisplay->SetShowing(false);
            reviewHelp->SetShowing(false);
        }
    }
}

void NextSongPanel::IncrementSongReview(int i) {
    BandUser *user = TheBandUserMgr->GetUserFromSlot(i);
    MILO_ASSERT(user, 0x106);
    LocalBandUser *localUser = user->GetLocalBandUser();
    MILO_ASSERT(localUser, 0x108);
    MILO_ASSERT(localUser->CanSaveData(), 0x109);
    BandProfile *profile = TheProfileMgr.GetProfileForUser(localUser);
    MILO_ASSERT(profile, 0x10B);
    MetaPerformer *performer = MetaPerformer::Current();
    MILO_ASSERT(performer, 0x10F);
    Symbol completedSong = performer->GetCompletedSong();
    int songID = TheSongMgr.GetSongIDFromShortName(completedSong, true);
    int review = profile->GetSongReview(songID);
    review++;
    if (review > 5)
        review = 0;
    profile->SetSongReview(songID, review);
    SetReviewDisplayValue(i, review);
}

void NextSongPanel::SetReviewDisplayValue(int slot, int reviewValue) {
    RndDir *dir = mDir->Find<RndDir>(MakeString("slot%i", slot), true);
    MILO_ASSERT(dir, 0x131);
    ReviewDisplay *reviewDisplay = dir->Find<ReviewDisplay>("song_review.rvw", true);
    MILO_ASSERT(reviewDisplay, 0x133);
    reviewDisplay->SetValues(reviewValue, true);
}

#pragma pool_data off
void NextSongPanel::UpdateScrollArrows(int i, bool b) {
    int i7 = unk70[i];
    int i4 = GetMaxScrollPage(i);
    bool n1 = unk80[i];
    bool n2 = unk84[i];
    unk84[i] = i7 != i4;
    unk80[i] = i7;

    if ((n1 || b) && !unk80[i]) {
        static Message cMsg("trigger_hide_up_arrow", 0);
        cMsg[0] = i;
        Handle(cMsg, true);
    } else if ((!n1 || b) && unk80[i]) {
        static Message cMsg("trigger_show_up_arrow", 0);
        cMsg[0] = i;
        Handle(cMsg, true);
    }

    if ((n2 || b) && !unk84[i]) {
        static Message cMsg("trigger_hide_down_arrow", 0);
        cMsg[0] = i;
        Handle(cMsg, true);
    } else if ((!n2 || b) && unk84[i]) {
        static Message cMsg("trigger_show_down_arrow", 0);
        cMsg[0] = i;
        Handle(cMsg, true);
    }
}
#pragma pool_data reset

void NextSongPanel::SetScrollExpandedDetails(int i, int j) {
    unk70[i] = j;
    UpdateScrollArrows(i, false);
}

void NextSongPanel::ScrollExpandedDetails(int slot, int delta, bool force) {
    int page = unk70[slot];
    if (delta > 0 && page < GetMaxScrollPage(slot)) {
        unk70[slot] = page + 1;
    } else if (delta < 0 && page > 0) {
        unk70[slot] = page - 1;
    }
    UpdateScrollArrows(slot, force);
    if (TheNetSession) {
        SongResultsScrollMsg msg(slot, unk70[slot]);
        TheNetSession->SendMsgToAll(msg, kReliable);
    }
}

void NextSongPanel::FillExpandedDetails(int slot) {
    float f1;
    DataArrayPtr ptr;
    int numDetails = CountOrCreateExpandedDetails(slot, ptr, true);
    ptr->Resize(numDetails);
    int newNumDetails = CountOrCreateExpandedDetails(slot, ptr, false);
    MILO_ASSERT(newNumDetails == numDetails, 0x18B);
    f1 = 0;
    const DataArray *t = TypeDef();
    MILO_ASSERT(t, 0x19A);
    DataArray *detailTypesArr = t->FindArray(detail_types);
    DataArray *defaultTypeArr = detailTypesArr->FindArray(default_type);
    mDetailCounts.clear();
    Symbol symb4(gNullStr);
    int i12 = 0;
    for (int n = 0; n < ptr->Size(); n++) {
        float fHeight = defaultTypeArr->FindFloat(height);
        float fMarginTop = defaultTypeArr->FindFloat(margin_top);
        float fMarginBottom = defaultTypeArr->FindFloat(margin_bottom);
        float fReqPadOverride = defaultTypeArr->FindFloat(required_padding_override);
        DataArray *arr = ptr->Array(n);
        Symbol dataSym = arr->Sym(0);
        int i9 = f1 / mDetailsPageSize;
        if (dataSym == page_break) {
            f1 = mDetailsPageSize * (i9 + 1);
        } else {
            if (dataSym == header) {
                symb4 = arr->Sym(1);
                i12 = 1;
            }
            DataArray *foundarr = detailTypesArr->FindArray(dataSym, false);
            if (foundarr) {
                foundarr->FindData(height, fHeight, false);
                foundarr->FindData(margin_top, fMarginTop, false);
                foundarr->FindData(margin_bottom, fMarginBottom, false);
                foundarr->FindData(required_padding_override, fReqPadOverride, false);
            }

            float top = fMarginTop;
            float f2 = top + fHeight + fMarginBottom;
            float max = fReqPadOverride;
            if (max > 0.0f)
                f2 = max;

            float f3 = (i9 + 1) * mDetailsPageSize;
            if (f1 + f2 > f3 - mDetailsFooterSize) {
                f1 = f3;
                ptr->Insert(n, DataArrayPtr(header_continued, symb4, ++i12));
                n--;
            } else {
                f1 += top;
                if (dataSym != pad) {
                    SetupDetailLine(arr, slot, foundarr->FindStr(resource_fmt), -f1);
                }
                f1 += fHeight;
                if (n + 1 < ptr->Size()) {
                    f1 += fMarginBottom;
                }
            }
        }
    }
    mDetailsHeight[slot] = f1;
    RndDir *rdir = mDir->Find<RndDir>(MakeString("slot%i", slot), true);
    rdir->SyncObjects();
}

#pragma push
#pragma opt_usedef_mem_limit 300
// retail scratch: https://decomp.me/scratch/xGtc5
int NextSongPanel::CountOrCreateExpandedDetails(int slot, DataArrayPtr &ptr, bool b) {
    // Retail declares these as function-local statics (one shared guard word at
    // 0x82E0193C, storages 0x82E01938 downward) — see the funclet-cascade
    // write-up.  Order is retail's ctor order at 0x82645374..0x8264560C.
    static Symbol vocals_grid("vocals_grid");
    static Symbol label("label");
    static Symbol solo_rank_label("solo_rank_label");
    static Symbol solo_instarank_group("solo_instarank_group");
    static Symbol score("score");
    static Symbol songresults_nodata("songresults_nodata");
    static Symbol generic_string("generic_string");
    static Symbol any("any");
    static Symbol left_label("left_label");
    static Symbol right_label("right_label");
    static Symbol page_break("page_break");
    static Symbol header("header");
    static Symbol pad("pad");
    BandUser *user = TheBandUserMgr->GetUserFromSlot(slot); // fn_8010021C
    MILO_ASSERT(TheGame->IsActiveUser(user), 0x203);
    Player *player = user->GetPlayer(); // fn_800D2D8C
    MILO_ASSERT(player, 0x205);
    const Stats &stats = player->mStats;
    int count = 0; // this is what will ultimately be returned
    int playerTrackType = player->GetTrackType(); // fn_800E3728
    MetaPerformer *performer = MetaPerformer::Current();
    MILO_ASSERT(performer, 0x20A);
    ScoreType ty = performer->GetScoreTypeForUser(user); // fn_8022618C
    static Symbol solo_score("solo_score");

    if (b)
        count++;
    else
        ptr.Node(count++) = DataArrayPtr(header, solo_score);

    if (b)
        count++;
    else
        ptr.Node(count++) = DataArrayPtr(pad);

    if (player->GetQuarantined()) {
        if (b)
            count++;
        else {
            ptr.Node(count++) = DataArrayPtr(label, songresults_nodata);
        }
    } else {
        if (b)
            count++;
        else
            ptr.Node(count++) =
                DataArrayPtr(score, 1 << ty, player->GetIndividualScore());

        if (b)
            count++;
        else
            ptr.Node(count++) = DataArrayPtr(pad);

        if (b)
            count++;
        else
            ptr.Node(count++) = DataArrayPtr(solo_instarank_group, gNullStr);
    }

    if (b)
        count++;
    else
        ptr.Node(count++) = DataArrayPtr(page_break);

    if (player->GetTrackType() == 3) {
        static Symbol completed_double_harmonies("completed_double_harmonies");
        static Symbol completed_triple_harmonies("completed_triple_harmonies");
        static Symbol perfect_harmony("perfect_harmony");
        int harm2hit = stats.GetDoubleHarmonyHit();
        int harm2count = stats.GetDoubleHarmonyPhraseCount();
        int harm3hit = stats.GetTripleHarmonyHit();
        int harm3count = stats.GetTripleHarmonyPhraseCount();
        if (harm2count != 0 || harm3count != 0) {
            static Symbol instrument_specific("instrument_specific");
            if (b)
                count++;
            else
                ptr.Node(count++) = DataArrayPtr(header, instrument_specific);

            if (harm2count != 0) {
                if (b)
                    count++;
                else
                    ptr.Node(count++) = DataArrayPtr(
                        label, completed_double_harmonies, harm2hit, harm2count
                    );
            } else {
                if (b)
                    count++;
                else
                    ptr.Node(count++) = DataArrayPtr(pad);
            }

            if (harm3count != 0) {
                if (b)
                    count++;
                else
                    ptr.Node(count++) = DataArrayPtr(
                        label, completed_triple_harmonies, harm3hit, harm3count
                    );
            } else {
                if (b)
                    count++;
                else
                    ptr.Node(count++) = DataArrayPtr(pad);
            }

            if ((harm2count != 0 && harm2hit == harm2count)
                || (harm3count != 0 && harm3hit == harm3count)) {
                if (b)
                    count++;
                else
                    ptr.Node(count++) = DataArrayPtr(label, perfect_harmony);
            } else {
                if (b)
                    count++;
                else
                    ptr.Node(count++) = DataArrayPtr(pad);
            }

            if (b)
                count++;
            else
                ptr.Node(count++) = DataArrayPtr(vocals_grid);

            if (b)
                count++;
            else
                ptr.Node(count++) = DataArrayPtr(page_break);
        }
    }

    static Symbol performance("performance");
    static Symbol endgame_phrase_streak("endgame_phrase_streak");
    static Symbol endgame_note_streak("endgame_note_streak");
    static Symbol endgame_hit_count("endgame_hit_count");
    static Symbol endgame_avg_multiplier("endgame_avg_multiplier");
    Symbol streakSym = playerTrackType == 3 ? endgame_phrase_streak : endgame_note_streak;
    if (b)
        count++;
    else
        ptr.Node(count++) = DataArrayPtr(header, performance);

    if (b)
        count++;
    else
        ptr.Node(count++) = DataArrayPtr(
            label, streakSym, LocalizeSeparatedInt(stats.GetLongestStreak())
        );

    if (b)
        count++;
    else
        ptr.Node(count++) = DataArrayPtr(
            label, endgame_hit_count, LocalizeSeparatedInt(stats.GetHitCount())
        );

    if (b)
        count++;
    else
        ptr.Node(count++) = DataArrayPtr(
            label,
            endgame_avg_multiplier,
            LocalizeFloat("%.02f", stats.GetAverageMultiplier())
        );

    if (b)
        count++;
    else
        ptr.Node(count++) = DataArrayPtr(page_break);

    Symbol completedSongSym = performer->GetCompletedSong();
    static Symbol achievement_progress("achievement_progress");
    if (b)
        count++;
    else
        ptr.Node(count++) = DataArrayPtr(header, achievement_progress);

    std::vector<Symbol> songGoals;
    if (TheAccomplishmentMgr->InqGoalsAcquiredForSong(user, completedSongSym, songGoals)) {
        for (std::vector<Symbol>::iterator it = songGoals.begin(); it != songGoals.end();
             ++it) {
            Symbol cur = *it;
            if (b)
                count++;
            else
                ptr.Node(count++) = DataArrayPtr(label, cur);
        }
    } else {
        if (b)
            count++;
        else
            ptr.Node(count++) = DataArrayPtr(pad);

        if (b)
            count++;
        else
            ptr.Node(count++) = DataArrayPtr(label, songresults_nodata);
    }

    if (b)
        count++;
    else
        ptr.Node(count++) = DataArrayPtr(page_break);

    static Symbol section_breakdown("section_breakdown");
    static Symbol score_detail_section("score_detail_section");
    static Symbol score_detail_section_no_notes("score_detail_section_no_notes");
    if (b)
        count++;
    else
        ptr.Node(count++) = DataArrayPtr(header, section_breakdown);

    if (player->GetQuarantined()) {
        if (b)
            count++;
        else
            ptr.Node(count++) = DataArrayPtr(pad);

        if (b)
            count++;
        else
            ptr.Node(count++) = DataArrayPtr(label, songresults_nodata);
    } else {
        int numsections = stats.NumSections();
        for (int i = 0; i < numsections; i++) {
            const Stats::SectionInfo &curinfo = stats.GetSectionInfo(i);
            if (curinfo.unk0 != gNullStr) {
                if (curinfo.unk4 < 0) {
                    if (b)
                        count++;
                    else
                        ptr.Node(count++) =
                            DataArrayPtr(left_label, generic_string, curinfo.unk0);

                    if (b)
                        count++;
                    else
                        ptr.Node(count++) = DataArrayPtr(right_label, any);
                } else {
                    if (b)
                        count++;
                    else
                        ptr.Node(count++) =
                            DataArrayPtr(left_label, generic_string, curinfo.unk0);

                    if (b)
                        count++;
                    else
                        ptr.Node(count++) = DataArrayPtr(
                            right_label,
                            score_detail_section,
                            Round(curinfo.unk4 * 100.0f)
                        );
                }
            }
        }
    }

    if (b)
        count++;
    else
        ptr.Node(count++) = DataArrayPtr(page_break);

    static Symbol score_breakdown("score_breakdown");
    static Symbol score_detail_accuracy("score_detail_accuracy");
    static Symbol score_detail_sustains("score_detail_sustains");
    static Symbol score_detail_streak("score_detail_streak");
    static Symbol score_detail_overdrive("score_detail_overdrive");
    static Symbol score_detail_solos("score_detail_solos");
    static Symbol score_detail_surface("score_detail_surface");
    static Symbol score_detail_band("score_detail_band");
    static Symbol score_detail_coda("score_detail_coda");
    static Symbol score_detail_tambourine("score_detail_tambourine");
    static Symbol score_detail_harmony("score_detail_harmony");
    if (b)
        count++;
    else
        ptr.Node(count++) = DataArrayPtr(header, score_breakdown);

    if (player->GetQuarantined()) {
        if (b)
            count++;
        else
            ptr.Node(count++) = DataArrayPtr(pad);

        if (b)
            count++;
        else
            ptr.Node(count++) = DataArrayPtr(label, songresults_nodata);
    } else {
        if (b)
            count++;
        else
            ptr.Node(count++) = DataArrayPtr(left_label, score_detail_accuracy);

        if (b)
            count++;
        else
            ptr.Node(count++) = DataArrayPtr(
                right_label, generic_string, LocalizeSeparatedInt(stats.GetAccuracy())
            );

        if (b)
            count++;
        else
            ptr.Node(count++) = DataArrayPtr(left_label, score_detail_streak);

        if (b)
            count++;
        else
            ptr.Node(count++) = DataArrayPtr(
                right_label,
                generic_string,
                LocalizeSeparatedInt(Round(stats.GetScoreStreak()))
            );

        if (b)
            count++;
        else
            ptr.Node(count++) = DataArrayPtr(left_label, score_detail_overdrive);

        if (b)
            count++;
        else
            ptr.Node(count++) = DataArrayPtr(
                right_label,
                generic_string,
                LocalizeSeparatedInt(Round(stats.GetOverdrive()))
            );

        if (stats.HasCoda()) {
            if (b)
                count++;
            else
                ptr.Node(count++) = DataArrayPtr(left_label, score_detail_coda);

            if (b)
                count++;
            else
                ptr.Node(count++) = DataArrayPtr(
                    right_label,
                    generic_string,
                    LocalizeSeparatedInt(stats.GetCodaPoints())
                );
        }
        if (playerTrackType == 3) {
            if (b)
                count++;
            else
                ptr.Node(count++) = DataArrayPtr(left_label, score_detail_tambourine);

            if (b)
                count++;
            else
                ptr.Node(count++) = DataArrayPtr(
                    right_label,
                    generic_string,
                    LocalizeSeparatedInt(Round(stats.GetTambourine()))
                );

            if (b)
                count++;
            else
                ptr.Node(count++) = DataArrayPtr(left_label, score_detail_harmony);

            if (b)
                count++;
            else
                ptr.Node(count++) = DataArrayPtr(
                    right_label, generic_string, LocalizeSeparatedInt(stats.GetHarmony())
                );
        } else {
            if (stats.HasSolos()) {
                if (b)
                    count++;
                else
                    ptr.Node(count++) = DataArrayPtr(left_label, score_detail_solos);

                if (b)
                    count++;
                else
                    ptr.Node(count++) = DataArrayPtr(
                        right_label, generic_string, LocalizeSeparatedInt(stats.GetSolo())
                    );
            }
            if (playerTrackType != 0) {
                if (b)
                    count++;
                else
                    ptr.Node(count++) = DataArrayPtr(left_label, score_detail_sustains);

                if (b)
                    count++;
                else
                    ptr.Node(count++) = DataArrayPtr(
                        right_label,
                        generic_string,
                        LocalizeSeparatedInt(Round(stats.GetSustain()))
                    );
            }
        }
        if (b)
            count++;
        else
            ptr.Node(count++) = DataArrayPtr(pad);

        if (b)
            count++;
        else
            ptr.Node(count++) = DataArrayPtr(left_label, score_detail_band);

        if (b)
            count++;
        else
            ptr.Node(count++) = DataArrayPtr(
                right_label,
                generic_string,
                LocalizeSeparatedInt(Round(stats.GetBandContribution()))
            );
    }
    return count;
}
#pragma pop

void NextSongPanel::SetupDetailLine(DataArray *detail, int slot, const char *cc, float f) {
    Symbol sym = detail->Sym(0);
    RndDir *rdir = mDir->Find<RndDir>(MakeString("slot%i", slot), true);
    std::hash_map<Symbol, int>::iterator it = mDetailCounts.find(sym);
    if (it == mDetailCounts.end())
        mDetailCounts[sym] = 1;
    else
        mDetailCounts[sym] += 1;
    const char *intstr = MakeString(cc, mDetailCounts[sym]);
    RndTransformable *t;
    RndDrawable *d;
    if (!(sym == label || sym == left_label || sym == right_label || sym == header
          || sym == header_continued)
        || mDetailCounts[sym] == 1) {
        t = rdir->Find<RndTransformable>(intstr, true);
        d = rdir->Find<RndDrawable>(intstr, true);
    } else {
        UILabel *lbl = rdir->Find<UILabel>(MakeString(cc, 1), true);
        UILabel *newLabel = dynamic_cast<UILabel *>(CloneObject(lbl, true));
        MILO_ASSERT(newLabel, 0x2E9);
        newLabel->SetName(intstr, rdir);
        mDetailLabels.push_back(newLabel);
        t = newLabel;
        d = newLabel;
    }
    Transform xfm(t->LocalXfm());
    xfm.v.z = f;
    t->SetLocalXfm(xfm);
    d->SetShowing(true);
    if (sym == score) {
        ScoreDisplay *scoreDisplay = dynamic_cast<ScoreDisplay *>(t);
        MILO_ASSERT(scoreDisplay, 0x2F8);
        scoreDisplay->SetValues(detail->Int(1), detail->Int(2), 0, false);
    } else if (sym == header_continued) {
        AppLabel *lbl = dynamic_cast<AppLabel *>(t);
        MILO_ASSERT(lbl, 0x2FF);
        MILO_ASSERT(detail->Size() >= 3, 0x300);
        lbl->SetTokenFmt(songresults_header_continued, detail->Sym(1), detail->Int(2));
    } else if (sym == label || sym == left_label || sym == right_label || sym == header) {
        AppLabel *lbl = dynamic_cast<AppLabel *>(t);
        MILO_ASSERT(lbl, 0x309);
        DataNode &node = detail->Node(1);
        if (node.Type() == kDataSymbol) {
            if (detail->Size() < 3) {
                lbl->SetTextToken(node.Sym());
            } else {
                detail->Remove(0);
                static Message msg(set_token_fmt, 0);
                msg[0] = DataNode(detail, kDataArray);
                Handle(msg, false);
            }
        } else if (node.Type() == kDataString) {
            lbl->SetTextToken(node.Str());
        } else
            MILO_FAIL("bad type for label detail");
    }
}

Symbol NextSongPanel::GetPerformanceAward(int i) {
    BandUser *user = TheBandUserMgr->GetUserFromSlot(i);
    MILO_ASSERT(TheGame->IsActiveUser(user), 0x329);
    Player *player = user->GetPlayer();
    MILO_ASSERT(player, 0x32B);
    std::vector<Symbol> &awards = player->mStats.AccessPerformanceAwards();
    if (awards.empty())
        return gNullStr;
    else
        return awards.front();
}

BEGIN_HANDLERS(NextSongPanel)
    HANDLE_ACTION(
        scroll_expanded_details, ScrollExpandedDetails(_msg->Int(2), _msg->Int(3), false)
    )
    HANDLE_ACTION(
        set_scroll_expanded_details, SetScrollExpandedDetails(_msg->Int(2), _msg->Int(3))
    )
    HANDLE_ACTION(increment_song_review, IncrementSongReview(_msg->Int(2)))
    HANDLE_ACTION(
        initialize_song_review_display, InitializeSongReviewDisplay(_msg->Int(2))
    )
    HANDLE_EXPR(can_change_song_review, CanChangeSongReview(_msg->Int(2)))
    HANDLE_EXPR(get_performance_award, GetPerformanceAward(_msg->Int(2)))
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(0x349)
END_HANDLERS
