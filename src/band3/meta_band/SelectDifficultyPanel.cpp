#include "meta_band/SelectDifficultyPanel.h"
#include "beatmatch/TrackType.h"
#include "decomp.h"
#include "game/BandUserMgr.h"
#include "game/Defines.h"
#include "game/GameMode.h"
#include "meta_band/BandSongMetadata.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/BandUI.h"
#include "meta_band/Campaign.h"
#include "meta_band/MetaPerformer.h"
#include "meta_band/ModifierMgr.h"
#include "meta_band/MusicLibrary.h"
#include "meta_band/OvershellPanel.h"
#include "meta_band/SessionMgr.h"
#include "meta_band/TrainingMgr.h"
#include "obj/Data.h"
#include "obj/ObjMacros.h"
#include "os/ContentMgr.h"
#include "os/Debug.h"
#include "tour/Tour.h"
#include "ui/UIPanel.h"
#include "utl/Symbol.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"

SelectDifficultyPanel::SelectDifficultyPanel()
    : mMarqueeRotationMs(3000.0f), mCurrentSongIx(0) {}

bool SelectDifficultyPanel::IsLoaded() const {
#ifdef HX_NATIVE
    if (getenv("GAME_DBG")) {
        static int spam = 0;
        if ((spam++ % 60) == 0)
            MILO_LOG("GAME_DBG: SelectDifficultyPanel::IsLoaded uiLoaded=%d refreshDone=%d numSongs=%d\n",
                     UIPanel::IsLoaded(), TheContentMgr.RefreshDone(),
                     MetaPerformer::Current()->NumSongs());
    }
#endif
    return UIPanel::IsLoaded() && TheContentMgr.RefreshDone()
        && MetaPerformer::Current()->NumSongs() > 0;
}

void SelectDifficultyPanel::PollForLoading() {
    UIPanel::PollForLoading();
    if (TheContentMgr.RefreshDone() && TheGameMode->InMode(party_shuffle)
        && MetaPerformer::Current()->NumSongs() == 0
        && (TheSessionMgr->IsLocal() || TheSessionMgr->IsLeaderLocal())) {
        TheMusicLibrary->BuildPartySetlist();
        TheMusicLibrary->SendSetlistToMetaPerformer();
    }
}

#pragma push
#pragma auto_inline on
// band aid fix for this particular function
// i think we might need to change the global compiler settings to just O4?
// except doing that breaks inline settings
// whatever the case may be, using O4 seems to fix weird pooling/the dreaded bss meme
// either that, or changing the order of flags to go "-inline noauto -O4,p"
void SelectDifficultyPanel::Enter() {
    static Symbol marquee_rotation_ms("marquee_rotation_ms");
    static Symbol set_list_title("set_list_title");
    UIPanel::Enter();
    mCurrentSongIx = 0;
    MetaPerformer *mp = MetaPerformer::Current();
    if (mp) {
        int numsongs = mp->NumSongs();
        PushSongDetailsToScreen(mp);
        DataArray *rotArr = TypeDef()->FindArray(marquee_rotation_ms, false);
        if (rotArr) {
            mMarqueeRotationMs = rotArr->Float(1);
        }
        if (numsongs > 1)
            mMarqueeTimer.Restart();
        if (TheGameMode->InMode("tour")) {
            static Message updateSetlistLabel("update_tour_setlist_label", 0, 0);
            updateSetlistLabel[0] = TheTour->GetCurrentFilterName();
            updateSetlistLabel[1] = numsongs;
            HandleType(updateSetlistLabel);
        } else if (TheGameMode->InMode("party_shuffle")
                   || TheGameMode->InMode("qp_party_shuffle")) {
            static Message updateSetlistLabel("update_partyshuffle_setlist_label", 0);
            updateSetlistLabel[0] = numsongs;
            HandleType(updateSetlistLabel);
        } else if (mp->HasSetlist()) {
            static Message updateSetlistLabel("update_named_setlist_label", 0, 0);
            updateSetlistLabel[0] = mp->GetSetlistName();
            updateSetlistLabel[1] = numsongs;
            HandleType(updateSetlistLabel);
        } else {
            static Message updateSetlistLabel("update_setlist_label", 0);
            updateSetlistLabel[0] = numsongs;
            HandleType(updateSetlistLabel);
        }
    }

    if (TheModifierMgr) {
        static Symbol mod_auto_vocals("mod_auto_vocals");
        static Message updateAutoVocalsLabel("update_auto_vocals_label", 0, 0, 0);
        updateAutoVocalsLabel[0] = TheBandUserMgr->GetNumParticipants();
        updateAutoVocalsLabel[1] = TheModifierMgr->IsModifierActive(mod_auto_vocals);
        updateAutoVocalsLabel[2] = TheSessionMgr->IsLocal();
        HandleType(updateAutoVocalsLabel);
    }

    OvershellPanel *overshell = TheBandUI.GetOvershell();
    MILO_ASSERT(overshell != NULL, 0x79);
    overshell->ClearTrackTypesFromUsers();
    if (TheGameMode->InMode("campaign") == 1) {
        overshell->SetPartRestrictedUser(TheCampaign->GetLaunchUser());
        overshell->SetPartRestriction(
            TheCampaign->GetRequiredTrackTypeForCurrentAccomplishment()
        );
        overshell->SetMinimumDifficulty(
            TheCampaign->GetMinimumDifficultyForCurrentAccomplishment()
        );
    } else if (TheGameMode->InMode("trainer") == 1) {
        TrainingMgr *trainingMgr = TrainingMgr::GetTrainingMgr();
        MILO_ASSERT(trainingMgr, 0x86);
        overshell->SetPartRestrictedUser(trainingMgr->GetUser());
        overshell->SetPartRestriction(kNumTrackTypes);
        overshell->SetMinimumDifficulty(trainingMgr->GetMinimumDifficulty());
    } else {
        overshell->SetPartRestrictedUser(nullptr);
        overshell->SetPartRestriction(kNumTrackTypes);
        overshell->SetMinimumDifficulty(kDifficultyEasy);
    }
    TheContentMgr.RegisterCallback(this, false);
}
#pragma pop

void SelectDifficultyPanel::Poll() {
    UIPanel::Poll();
    if (mMarqueeTimer.Running() && mMarqueeTimer.SplitMs() > mMarqueeRotationMs) {
        mMarqueeTimer.Restart();
        MetaPerformer *mp = MetaPerformer::Current();
        if (mp) {
            mCurrentSongIx++;
            mCurrentSongIx %= mp->NumSongs();
            PushSongDetailsToScreen(mp);
        }
    }
}

void SelectDifficultyPanel::Exit() {
    mMarqueeTimer.Stop();
    TheContentMgr.UnregisterCallback(this, true);
    UIPanel::Exit();
}

void SelectDifficultyPanel::PushSongDetailsToScreen(const MetaPerformer *mp) {
    Symbol theSong = mp->GetSongSymbol(mCurrentSongIx);
    static Message update_preview_song("update_preview_song", 0, 0, 0);
    update_preview_song[0] = theSong;

    int songID = TheSongMgr.GetSongIDFromShortName(theSong, true);
    BandSongMetadata *data = (BandSongMetadata *)TheSongMgr.Data(songID);
    if (data && data->HasAlbumArt()) {
        if (TheSongMgr.IsSongMounted(theSong)) {
            update_preview_song[1] = TheSongMgr.GetAlbumArtPath(theSong);
        } else {
            update_preview_song[1] = gNullStr;
            // RB3-360 retail only (absent from the rb3-Wii dev source): when the
            // song has album art but isn't mounted, kick off a mount so the art
            // becomes available. Ground truth = retail's TGT-only block after
            // the not-mounted gNullStr store: ContentName(theSong, true), null
            // check, then TheContentMgr vtable slot 0x70 == MountContent(Symbol).
            const char *contentName = TheSongMgr.ContentName(theSong, true);
            if (contentName) {
                TheContentMgr.MountContent(contentName);
            }
        }
    } else
        update_preview_song[1] = gNullStr;

    if (GetNumSongs() > 1) {
        update_preview_song[2] = (int)(mCurrentSongIx + 1);
    } else
        update_preview_song[2] = 0;

    HandleType(update_preview_song);
}

int SelectDifficultyPanel::GetNumSongs() const {
    int songs = 0;
    MetaPerformer *mp = MetaPerformer::Current();
    if (mp)
        songs = mp->NumSongs();
    return songs;
}

bool SelectDifficultyPanel::IsBattle() const {
    MetaPerformer *mp = MetaPerformer::Current();
    if (mp)
        return mp->GetBattleID() > 0;
    else
        return false;
}

void SelectDifficultyPanel::ContentMounted(const char *, const char *) {
    if (!TheContentMgr.RefreshInProgress() && MetaPerformer::Current()) {
        PushSongDetailsToScreen(MetaPerformer::Current());
    }
}

BEGIN_HANDLERS(SelectDifficultyPanel)
    HANDLE_EXPR(is_battle, IsBattle())
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(0xFC)
END_HANDLERS

DECOMP_FORCEFUNC(SelectDifficultyPanel, SelectDifficultyPanel, ContentDir())
DECOMP_FORCEFUNC(SelectDifficultyPanel, SelectDifficultyPanel, ClassName())
DECOMP_FORCEFUNC(SelectDifficultyPanel, SelectDifficultyPanel, SetType(0))