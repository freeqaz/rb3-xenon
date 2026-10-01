#include "meta_band/Utl.h"
#include "decomp.h"
#include "game/Defines.h"
#include "obj/Utl.h"
#include "SongStatusMgr.h"
#include "beatmatch/TrackType.h"
#include "meta_band/SessionMgr.h"
#include "obj/Data.h"
#include "obj/DataUtl.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/System.h"
#include "utl/Symbols.h"
#include "ui/UIPanel.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include <obj/DataFunc.h>
#include <os/PlatformMgr.h>

static int kMinContentLevel = 2;

// Retail keeps this handler's body (its EH funclets pair) but UtlInit no longer
// registers it; external linkage keeps it emitted in the match build.
DataNode OnToggleFakeLeaderboardUploadFailure(DataArray *da) {
    SongStatusMgr::sFakeLeaderboardUploadFailure =
        !SongStatusMgr::sFakeLeaderboardUploadFailure;
    Hmx::Object *cheatObj = ObjectDir::Main()->Find<Hmx::Object>("cheat_display", true);
    if (cheatObj) {
        static Message msg("show_bool", "Fake leaderboard upload failure", 0);
        msg[1] = SongStatusMgr::sFakeLeaderboardUploadFailure;
        cheatObj->Handle(msg, false);
    }
    return 0;
}

static DataNode OnGetFontCharFromInstrument(DataArray *);
static DataNode OnGetFontCharFromControllerType(DataArray *);
static DataNode OnGetFontCharFromTrackType(DataArray *);
static DataNode OnGetFontCharFromScoreType(DataArray *);
static DataNode OnGetFontCharForHarmonyMics(DataArray *);
static DataNode OnIsLeaderLocal(DataArray *);
static DataNode OnIsVignette(DataArray *);
static DataNode OnSafeName(DataArray *);
static DataNode OnAllowedToAccessContent(DataArray *);

void UtlInit() {
    DataRegisterFunc("cnv_instrumenttoicon", OnGetFontCharFromInstrument);
    DataRegisterFunc(
        "get_font_char_from_controller_type", OnGetFontCharFromControllerType
    );
    // Retail 0x825BF0C0 order, read off the registered strings
    // (lbl_820AE4F4 "get_font_char_from_track_type", 0x820AE4D4
    // "..._from_score_type", 0x820AE4B4 "..._for_harmony_mics") and the callee
    // of each registered handler. The fake-upload-failure cheat is dev-only.
    DataRegisterFunc("get_font_char_from_track_type", OnGetFontCharFromTrackType);
    DataRegisterFunc("get_font_char_from_score_type", OnGetFontCharFromScoreType);
    DataRegisterFunc("get_font_char_for_harmony_mics", OnGetFontCharForHarmonyMics);
    DataRegisterFunc("is_leader_local", OnIsLeaderLocal);
    DataRegisterFunc("is_vignette", OnIsVignette);
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
    DataRegisterFunc(
        "toggle_fake_leaderboard_upload_failure", OnToggleFakeLeaderboardUploadFailure
    );
#endif
    DataRegisterFunc("safe_name", OnSafeName);
    DataRegisterFunc("allowed_to_access_content", OnAllowedToAccessContent);
}

bool IsLeaderLocal() {
    if (TheSessionMgr)
        return TheSessionMgr->IsLeaderLocal();
    else
        return true;
}

static DataNode OnIsLeaderLocal(DataArray *) { return IsLeaderLocal(); }

bool IsVignette(UIPanel *panel) {
    if (!panel)
        return false;
    else {
        static Symbol file("file");
        Hmx::Object *old_this = DataSetThis(panel);
        bool ret = false;
        if (panel->TypeDef()) {
            DataArray *fileArr = panel->TypeDef()->FindArray(file, false);
            if (fileArr) {
                if (strstr(fileArr->Str(1), "world/vignette/")) {
                    ret = true;
                }
            }
        }
        DataSetThis(old_this);
        return ret;
    }
}

static DataNode OnIsVignette(DataArray *da) { return IsVignette(da->Obj<UIPanel>(1)); }

static DataNode OnSafeName(DataArray *da) { return SafeName(da->Obj<Hmx::Object>(1)); }

static DataNode OnGetFontCharFromInstrument(DataArray *da) {
    Symbol inst = da->Sym(1);
    int idx = da->Size() > 2 ? da->Int(2) : 0;
    return GetFontCharFromInstrument(inst, idx);
}

static DataNode OnGetFontCharFromControllerType(DataArray *da) {
    ControllerType cty = (ControllerType)da->Int(1);
    int idx = da->Size() > 2 ? da->Int(2) : 0;
    return GetFontCharFromControllerType(cty, idx);
}

static DataNode OnGetFontCharFromTrackType(DataArray *da) {
    TrackType tty = (TrackType)da->Int(1);
    int idx = da->Size() > 2 ? da->Int(2) : 0;
    return GetFontCharFromTrackType(tty, idx);
}

static DataNode OnGetFontCharFromScoreType(DataArray *da) {
    ScoreType sty = (ScoreType)da->Int(1);
    int idx = da->Size() > 2 ? da->Int(2) : 0;
    return GetFontCharFromScoreType(sty, idx);
}

const char *GetFontCharFromInstrument(Symbol instrument, int idx) {
    MILO_ASSERT(!instrument.Null(), 0xB7);
    return GetFontCharFromTrackType(SymToTrackType(instrument), idx);
}

const char *GetFontCharFromScoreType(ScoreType scoreType, int idx) {
    switch (scoreType) {
    case kScoreBand:
        return "j";
    case kScoreDrum:
    case kScoreBass:
    case kScoreGuitar:
    case kScoreVocals:
    case kScoreKeys:
    case kScoreRealGuitar:
    case kScoreRealBass:
    case kScoreRealKeys:
        return GetFontCharFromTrackType(ScoreTypeToTrackType(scoreType), idx);
    case kScoreRealDrum:
        return GetFontCharForProDrums(idx);
    case kScoreHarmony:
        return GetFontCharForHarmonyMics(3, idx);
    default:
        MILO_WARN("Invalid ScoreType\n");
        return gNullStr;
    }
}

const char *GetFontCharFromTrackType(TrackType trackType, int idx) {
    // Retail 0x825BE650 guards a function-local static Symbol (guard word
    // 0x82DFF640) rather than reading the Symbols*.h global.
    static Symbol instrument_icons("instrument_icons");
    MILO_ASSERT_RANGE(trackType, 0, kNumTrackTypes + 1, 0xD9);
    switch (trackType) {
    case kTrackDrum:
    case kTrackGuitar:
    case kTrackBass:
    case kTrackVocals:
    case kTrackKeys:
    case kTrackRealKeys:
    case kTrackRealGuitar:
    case kTrackRealBass:
        return SystemConfig(instrument_icons, TrackTypeToSym(trackType))->Str(idx + 1);
    case kTrackNone:
        MILO_WARN("GetFontCharFromTrackType passed kTrackNone\n");
        return "_";
    default:
        MILO_FAIL("Invalid TrackType specified: %d", (int)trackType);
        return nullptr;
    }
}

const char *GetFontCharFromControllerType(ControllerType controllerType, int idx) {
    MILO_ASSERT_RANGE(controllerType, 0, kNumControllerTypes, 0xFB);
    return GetFontCharFromTrackType(ControllerTypeToTrackType(controllerType, false), idx);
}

static DataNode OnGetFontCharForHarmonyMics(DataArray *da) {
    int i1 = da->Int(1);
    int idx = da->Size() > 2 ? da->Int(2) : 0;
    return GetFontCharForHarmonyMics(i1, idx);
}

const char *GetFontCharForHarmonyMics(int num_mics, int idx) {
    // Retail 0x825BE760: three function-local statics under one guard word
    // (bits 1/2/4), which also keeps the body out of line in
    // GetFontCharFromScoreType's kScoreHarmony case.
    static Symbol instrument_icons("instrument_icons");
    static Symbol harmony_2("harmony_2");
    static Symbol harmony_3("harmony_3");
    switch (num_mics) {
    case 2:
        return SystemConfig(instrument_icons, harmony_2)->Str(idx + 1);
    case 3:
        return SystemConfig(instrument_icons, harmony_3)->Str(idx + 1);
    default:
        MILO_FAIL("Invalid number of mics: %i", num_mics);
        return nullptr;
    }
}

inline const char *GetFontCharForProDrums(int idx) {
    // Retail 0x825BE8B8: two function-local statics under one guard word.
    static Symbol instrument_icons("instrument_icons");
    static Symbol drum_pro("drum_pro");
    return SystemConfig(instrument_icons, drum_pro)->Str(idx + 1);
}

// enum TrackType {
//     kTrackDrum = 0,
//     kTrackGuitar = 1,
//     kTrackBass = 2,
//     kTrackVocals = 3,
//     kTrackKeys = 4,
//     kTrackRealKeys = 5,
//     kTrackRealGuitar = 6,
//     kTrackRealGuitar22Fret = 7,
//     kTrackRealBass = 8,
//     kTrackRealBass22Fret = 9,
//     kTrackNone = 10,
//     kNumTrackTypes = 10,
//     kTrackPending = 11,
//     kTrackPendingVocals = 12
// };

const char *GetUserFontChar(BandUser *user, MetaPerformer *perf, int idx) {
    // Retail 0x825BE9A0 guards four function-local statics (bits 1/2/4/8)
    // before anything else, rather than using the Symbols*.h globals.
    static Symbol instrument_icons("instrument_icons");
    static Symbol harmony_2("harmony_2");
    static Symbol harmony_3("harmony_3");
    static Symbol drum_pro("drum_pro");
    Symbol inst(gNullStr);
    TrackType ty = user->GetTrackType();
    if (ty == kTrackNone || ty == kTrackPending || ty == kTrackPendingVocals) {
        ty = ControllerTypeToTrackType(user->GetControllerType(), false);
    }
    switch (ty) {
    case kTrackVocals:
        if (perf && !perf->IsSetComplete() && perf->IsNowUsingVocalHarmony()) {
            int parts = perf->GetSetlistMaxVocalParts();
            if (parts == 2)
                inst = harmony_2;
            else
                inst = harmony_3;
        }
        break;
    case kTrackDrum:
        if (user->GetPreferredScoreType() == 6) {
            inst = drum_pro;
        }
    default:
        break;
    }
    if (inst == gNullStr) {
        return GetFontCharFromTrackType(ty, idx);
    } else {
        return SystemConfig(instrument_icons, inst)->Str(idx + 1);
    }
}

int MaxAllowedHmxMaturityLevel();

bool AllowedToAccessContent(int level) {
    if (level <= kMinContentLevel) {
        return true;
    } else {
        int maxAllowedLevel = MaxAllowedHmxMaturityLevel();
        return level <= maxAllowedLevel;
    }
}

static DataNode OnAllowedToAccessContent(DataArray *da) {
    int level = da->Int(1);
    bool allowedToAccessContent = AllowedToAccessContent(level);
    return DataNode(allowedToAccessContent);
}

/** Two XDK calls reached only from MaxAllowedHmxMaturityLevel (retail
    0x82B54328 and 0x82B543A8). Both are unnamed in our map and carry no string
    of their own, so their real XDK names are not recoverable here; the names
    below are descriptive. Only their SHAPE is load-bearing, and retail shows
    it: `(dwUserIndex, DWORD *...) -> DWORD`, called with XUSER_INDEX_ANY (0xff).
    The first fills a flags word whose bit 0x80 gates the rating check; the
    second fills a rating-board index (0-11) and a rating value (0xff = none). */
extern "C" DWORD XContentRestrictionFlags(DWORD dwUserIndex, DWORD *pdwFlags);
extern "C" DWORD XContentRatingLimit(DWORD dwUserIndex, DWORD *pdwBoard, DWORD *pdwRating);

/** Retail 0x825BE310. Translates the console's parental-control game-rating
    limit into an HMX content maturity level (1-3; -1 for a rating with no
    mapping), never below kMinContentLevel. With PlatformMgr's +0x3d override
    set, without the restriction flag, or with no rating set, every level (4)
    is allowed. The board/rating table below is read off retail's two jump
    tables (0x820AE238 by board, 0x820AE228 for boards 1-3) and the compare
    chains of the other boards. */
int MaxAllowedHmxMaturityLevel() {
    int level = -1;
    if (ThePlatformMgr.ParentalControlUnlocked()) {
        return 4;
    }
    DWORD flags = 0;
    XContentRestrictionFlags(0xff, &flags);
    if (!(flags & 0x80)) {
        return 4;
    }
    DWORD board = 0;
    DWORD rating = 0;
    XContentRatingLimit(0xff, &board, &rating);
    if (rating == 0xff) {
        return 4;
    }
    switch (board) {
    case 0:
        switch (rating) {
        case 0: case 2: case 4: level = 1; break;
        case 6: level = 2; break;
        case 8: case 10: level = 3; break;
        }
        break;
    case 1:
    case 2:
    case 3:
        switch (rating) {
        case 0: case 1: case 3: case 4: case 8: level = 1; break;
        case 9: case 12: case 13: level = 2; break;
        case 14: level = 3; break;
        }
        break;
    case 4:
        switch (rating) {
        case 0: case 1: case 4: case 5: level = 1; break;
        case 9: case 12: case 13: level = 2; break;
        case 14: level = 3; break;
        }
        break;
    case 5:
    case 9:
        switch (rating) {
        case 0: level = 1; break;
        case 2: case 4: level = 2; break;
        case 6: level = 3; break;
        }
        break;
    case 6:
        switch (rating) {
        case 0: case 2: case 4: case 6: level = 2; break;
        case 8: level = 3; break;
        }
        break;
    case 7:
        switch (rating) {
        case 0: case 2: level = 1; break;
        case 3: case 4: case 5: case 6: level = 2; break;
        }
        break;
    case 8:
        switch (rating) {
        case 0: case 2: level = 1; break;
        case 4: case 6: level = 2; break;
        }
        break;
    case 11:
        switch (rating) {
        case 0: case 6: case 7: level = 1; break;
        case 10: case 13: level = 2; break;
        case 14: level = 3; break;
        }
        break;
    }
    if (level < kMinContentLevel) {
        level = kMinContentLevel;
    }
    return level;
}
