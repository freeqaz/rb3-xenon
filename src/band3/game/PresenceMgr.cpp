#include "game/PresenceMgr.h"
#include "decomp.h"
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "game/GameConfig.h"
#include "game/GameMode.h"
#include "meta_band/BandSongMetadata.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/SessionMgr.h"
#include "obj/Data.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "os/PlatformMgr.h"
#include "ui/UI.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"
#include "utl/UTF8.h"

PresenceMgr ThePresenceMgr;

DECOMP_FORCEACTIVE(PresenceMgr, __FILE__, "TheSessionMgr", "required_song_options_chosen")

DECOMP_FORCEBLOCK(
    PresenceMgr, (std::vector<Symbol> * dummy, Symbol s),
    dummy->insert(dummy->begin(), 1, s);
)

PresenceMgr::PresenceMgr()
    : unk1c(0), unk20(0), unk24(0), unk34(0), unk38(0), unk39(0), unk3c(0) {}

// Body from retail 0x82680A20; no surviving source has this rich-presence
// code. Per signed-in local user retail:
//   - tests LocalUser vslot 4 (IsSignedIn), not ThePlatformMgr.IsUserSignedIn;
//   - takes the "not in session" flag from BandUser vslot 1 (UnkTU5Virtual),
//     not TheSessionMgr->HasUser;
//   - remaps context 0 to 0xe when the play-mode context is 0xb, then pushes
//     presence, context 2 = play mode, and property 0x40000003 = the current
//     song title (only outside a listed game mode), trimmed to 19 chars plus
//     "..." when longer than 22.
void PresenceMgr::UpdatePresence() {
    if (!TheGameConfig || !unk1c)
        return;
    else {
        Symbol mode = GetPresenceMode();
        bool inMode = false;
        FOREACH (it, unk2c) {
            if (TheGameMode->InMode(*it)) {
                inMode = true;
                break;
            }
        }
        std::vector<LocalBandUser *> &users = TheBandUserMgr->GetLocalBandUsers();
        FOREACH (it, users) {
            LocalBandUser *pUser = *it;
            MILO_ASSERT(pUser, 0xBD);
            if (pUser->IsSignedIn()) {
                int ctx = GetPresenceContextFromMode(mode, !pUser->UnkTU5Virtual());
                int playMode = GetPlayModeContextFromUser(pUser, inMode);
                if (ctx == 0 && playMode == 0xb)
                    ctx = 0xe;
                ThePlatformMgr.SetUserPresence(pUser, ctx);
                ThePlatformMgr.SetUserContext(pUser, 2, playMode);
                String title;
                if (!inMode && unk34 != 0) {
                    title =
                        static_cast<BandSongMetadata *>(TheSongMgr.Data(unk34))->Title();
                }
                if (title.length() > 0x16) {
                    int i = 0x12;
                    while (title.c_str()[i] == ' ' && --i > 0)
                        ;
                    title.resize(i + 1);
                    title += "...";
                }
                const unsigned short *wideTitle = CharToWideChar(title.c_str());
                ThePlatformMgr.SetUserProperty(pUser, 0x40000003, wideTitle);
            }
        }
    }
}

void PresenceMgr::SetNotInGame() {
    if (!unk1c)
        return;
    unk38 = false;
    unk34 = 0;
    UpdatePresence();
}

// retail 0x82680CB8, the SetNotInGame twin (GamePanel::Enter's caller)
void PresenceMgr::SetSongID(int id) {
    if (!unk1c)
        return;
    unk38 = true;
    unk34 = id;
    UpdatePresence();
}

Symbol PresenceMgr::GetPresenceMode() {
    if (!unk1c)
        return gNullStr;
    else {
        static Symbol in_game("in_game");
        static Symbol screens("screens");
        static Symbol gamemode("gamemode");
        static Symbol override_play_mode("override_play_mode");
        int size = unk1c->Size();
        unk39 = false;
        for (int i = 1; i < size; i++) {
            DataArray *arr = unk1c->Array(i);
            int arrSize = arr->Size();
            if (arrSize >= 1) {
                Symbol s50 = arr->Sym(0);
                bool b2 = true;
                for (int j = 1; j < arrSize; j++) {
                    DataArray *jArr = arr->Array(j);
                    Symbol s54 = jArr->Sym(0);
                    if (s54 == in_game) {
                        if (!unk38)
                            b2 = false;
                    } else if (s54 == screens) {
                        bool b1 = false;
                        int jSize = jArr->Size();
                        for (int k = 1; k < jSize; k++) {
                            Symbol s58 = jArr->Sym(k);
                            int depth = TheUI->PushDepth();
                            if (depth < 1) {
                                if (TheUI->CurrentScreen()) {
                                    if (s58 == TheUI->CurrentScreen()->Name()) {
                                        b1 = true;
                                        break;
                                    }
                                }
                            } else {
                                for (int n = 0; n < depth; n++) {
                                    if (TheUI->ScreenAtDepth(n)) {
                                        if (s58 == TheUI->ScreenAtDepth(n)->Name()) {
                                            b1 = true;
                                            break;
                                        }
                                    }
                                }
                                if (b1)
                                    break;
                            }
                        }
                        if (!b1)
                            b2 = false;
                    } else if (s54 == gamemode) {
                        bool inMode = false;
                        int jSize = jArr->Size();
                        for (int k = 1; k < jSize; k++) {
                            if (TheGameMode->InMode(jArr->Sym(k))) {
                                inMode = true;
                                break;
                            }
                        }
                        if (!inMode)
                            b2 = false;
                    } else if (s54 == override_play_mode) {
                        unk39 = true;
                        unk3c = jArr->Int(1);
                    }
                    if (!b2)
                        break;
                }
                if (b2)
                    return s50;
            }
        }
        return gNullStr;
    }
}

int PresenceMgr::GetPresenceContextFromMode(Symbol s, bool b) {
    if (!unk1c)
        return -1;
    else {
        return unk20->FindArray(s)->Int(b ? 2 : 1);
    }
}

int PresenceMgr::GetPlayModeContextFromUser(const LocalBandUser *pUser, bool bLearn) {
    if (!unk1c)
        return -1;
    if (unk39)
        return unk3c;
    static Symbol drums("drums");
    static Symbol guitar("guitar");
    static Symbol bass("bass");
    static Symbol vocals("vocals");
    static Symbol keys("keys");
    bool is_pro = false;
    Symbol trackSym;
    TrackType tt = pUser->GetTrackType();
    switch (tt) {
    case kTrackDrum: {
        trackSym = drums;
        is_pro = (pUser->GetPreferredScoreType() == kScoreRealDrum);
        if (TheGameMode->Property("force_use_cymbals", true)->Int(nullptr) != 0) {
            is_pro = true;
        } else {
            if (TheGameMode->Property("force_dont_use_cymbals", true)->Int(nullptr) != 0) {
                is_pro = false;
            }
        }
        break;
    }
    case kTrackGuitar:
    case kTrackRealGuitar:
    case kTrackRealGuitar22Fret:
        trackSym = guitar;
        is_pro = (tt == kTrackRealGuitar || tt == kTrackRealGuitar22Fret);
        break;
    case kTrackBass:
    case kTrackRealBass:
    case kTrackRealBass22Fret:
        trackSym = bass;
        is_pro = (tt == kTrackRealBass || tt == kTrackRealBass22Fret);
        break;
    case kTrackVocals:
        trackSym = vocals;
        is_pro = false;
        break;
    case kTrackKeys:
    case kTrackRealKeys:
        trackSym = keys;
        is_pro = (tt == kTrackRealKeys);
        break;
    default:
        break;
    }
    if (trackSym.Null()) {
        static Symbol symDefault("default");
        return unk24->FindInt(symDefault);
    }
    DataArray *trackArr = unk24->FindArray(trackSym, true);
    static Symbol play("play");
    static Symbol play_pro("play_pro");
    static Symbol learn("learn");
    static Symbol learn_pro("learn_pro");
    Symbol modeSym = is_pro ? (bLearn ? learn_pro : play_pro) : (bLearn ? learn : play);
    return trackArr->FindArray(modeSym, true)->Int(1);
}

DataNode PresenceMgr::OnPresenceChange(DataArray *a) {
    if (!unk1c)
        return 0;
    else {
        UpdatePresence();
        return 0;
    }
}

BEGIN_HANDLERS(PresenceMgr)
    HANDLE(current_screen_changed, OnPresenceChange)
    HANDLE(session_ready, OnPresenceChange)
    HANDLE(add_local_user_result_msg, OnPresenceChange)
    HANDLE(signin_changed, OnPresenceChange)
    HANDLE(local_user_left, OnPresenceChange)
    HANDLE(required_song_options_chosen, OnPresenceChange)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x22F)
END_HANDLERS
