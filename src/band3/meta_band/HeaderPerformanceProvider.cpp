#include "meta_band/HeaderPerformanceProvider.h"
#include "bandobj/ScoreDisplay.h"
#include "game/Defines.h"
#include "meta_band/AppLabel.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/SavedSetlist.h"
#include "meta_band/SongStatusMgr.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "ui/UIListCustom.h"
#include "ui/UIListLabel.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols4.h"

SetlistScoresProvider::SetlistScoresProvider()
    : mProfile(0), mScoreType(kNumScoreTypes) {}

void SetlistScoresProvider::Custom(int, int data, UIListCustom *slot, Hmx::Object *obj)
    const {
    if (slot->Matches("score")) {
        ScoreDisplay *sd = dynamic_cast<ScoreDisplay *>(obj);
        MILO_ASSERT(sd, 0x23);
        short val;
        if (mScoreType == kScoreBand) {
            val = unk30[data];
        } else {
            val = 1 << mScoreType;
        }
        sd->SetValues(val, unk28[data], 0, false);
        sd->SetShowing(true);
    }
}

void SetlistScoresProvider::Text(int, int data, UIListLabel *slot, UILabel *label) const {
    if (slot->Matches("song")) {
        AppLabel *appLabel = dynamic_cast<AppLabel *>(label);
        MILO_ASSERT(appLabel, 0x37);
        appLabel->SetSongNameWithNumber(
            unk20[data], data + 1, unk38[data].empty() ? 0 : unk38[data].c_str()
        );
    } else
        label->SetTextToken(gNullStr);
}

UIListWidgetState
SetlistScoresProvider::ElementStateOverride(int, int data, UIListWidgetState state) const {
    int songID = unk20[data];
    return TheSongMgr.HasSong(songID) ? state : kUIListWidgetInactive;
}

int SetlistScoresProvider::NumData() const { return unk20.size(); }

void SetlistScoresProvider::SetProfile(BandProfile *profile) {
    MILO_ASSERT(profile, 0x65);
    mProfile = profile;
}

void SetlistScoresProvider::SetScoreType(ScoreType type) { mScoreType = type; }

void SetlistScoresProvider::RefreshScores() {
    MILO_ASSERT(mProfile, 0x70);
    int n = unk20.size();
    for (int i = 0; i < n; i++) {
        int songID = unk20[i];
        SongStatusMgr *mgr = mProfile->GetSongStatusMgr();
        MILO_ASSERT(mgr, 0x77);
        // Retail calls 0x825D1840, which GetScore and GetHighScore share (identical bodies).
        unk28[i] = mgr->GetScore(songID, mScoreType);
        unk30[i] = mgr->GetBandInstrumentMask(songID);
    }
}

void SetlistScoresProvider::SetSetlist(SavedSetlist *setlist) {
    unk20 = setlist->mSongs;
    int n = unk20.size();
    unk28.resize(n);
    unk30.resize(n);
    unk38.resize(n);
    NetSavedSetlist *net = dynamic_cast<NetSavedSetlist *>(setlist);
    for (int i = 0; i < n; i++) {
        if (net) {
            unk38[i] = net->GetSongTitle(i);
        } else {
            unk38[i] = gNullStr;
        }
    }
}

BEGIN_HANDLERS(SetlistScoresProvider)
    HANDLE_ACTION(set_score_type, SetScoreType(SymToScoreType(_msg->Sym(2))))
    HANDLE_ACTION(set_profile, SetProfile(_msg->Obj<BandProfile>(2)))
    HANDLE_ACTION(refresh_scores, RefreshScores())
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x87)
END_HANDLERS