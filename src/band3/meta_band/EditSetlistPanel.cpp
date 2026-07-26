#include "meta_band/EditSetlistPanel.h"
#include "meta_band/MusicLibrary.h"
#include "game/BandUser.h"
#include "game/Defines.h"
#include "meta_band/BandProfile.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/SaveLoadManager.h"
#include "meta_band/SavedSetlist.h"
#include "net_band/RockCentral.h"
#include "net_band/RockCentralMsgs.h"
#include "obj/Data.h"
#include "obj/Msg.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "oggvorbis/VorbisMem.h"
#include "ui/UIPanel.h"
#include "ui/UIScreen.h"
#include "utl/Locale.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Messages4.h"
#include "utl/Symbols4.h"
#include "utl/UTF8.h"

extern "C" void XMsgCancelIORequest(void *);
extern "C" unsigned int XStringVerify(
    unsigned int, const char *, unsigned short, const void *, unsigned int, void *, void *
);

EditSetlistPanel::EditSetlistPanel()
    : unk50(kScoreBand), unk54(0), unk58(3), unk64(0), unk80(-1), unk84(-1), mProfile(0),
      mEditingSetlist(0), unk90(0), unk94(0), unk98(0), unk9c(0),
      mEditState((EditState)8), unka4((FailureReason)6) {}

EditSetlistPanel::~EditSetlistPanel() { CleanupStringVerify(); }

void EditSetlistPanel::Enter() {
    UIPanel::Enter();
    SetEditState((EditState)0);
}

DataNode EditSetlistPanel::OnMsg(const UITransitionCompleteMsg &msg) {
    MILO_ASSERT(mEditState == kEntering, 0x3B);
    switch (unk9c) {
    case 0:
        SetEditState((EditState)2);
        break;
    case 1:
        MILO_ASSERT(mEditingSetlist, 0x44);
        SetEditState((EditState)3);
        break;
    case 2:
        SetEditState((EditState)1);
        break;
    default:
        MILO_FAIL("Bad mode %i!");
        break;
    }
    return DataNode(kDataUnhandled, 0);
}

void EditSetlistPanel::Poll() {
    UIPanel::Poll();
    if (mEditState == kCheckingProfanity) {
        unsigned int status = XGetOverlappedResult((XOVERLAPPED *)unk98, 0, 0);
        if (status != 0x3E4) {
            if (status == 0) {
                unsigned int *p = *(unsigned int **)((char *)unk94 + 2);
                bool b1 = p[0] < 1;
                bool b2 = p[1] < 1;
                CleanupStringVerify();
                VerifyStringsComplete(b1, b2);
            } else {
                CleanupStringVerify();
                FailWithReason((FailureReason)7);
            }
        }
    }
}

bool EditSetlistPanel::Exiting() const {
    return UIPanel::Exiting() ||
        (mEditState != 3 && mEditState != 7 && mEditState != 8);
}

void EditSetlistPanel::Unload() {
    CleanupStringVerify();
    unk68.Clear();
    UIPanel::Unload();
}

void EditSetlistPanel::CleanupStringVerify() {
    if (unk98) {
        XMsgCancelIORequest(unk98);
        OggFree(unk98);
        unk98 = 0;
    }
    if (unk90) {
        delete[] *(unsigned short **)((char *)unk90 + 2);
        delete[] *(unsigned short **)((char *)unk90 + 8);
        delete[] unk90;
        unk90 = 0;
    }
    if (unk94) {
        delete[] unk94;
        unk94 = 0;
    }
}

bool EditSetlistPanel::CreateSetlist(bool b1) {
    static Symbol setlist_default_name("setlist_default_name");
    static Symbol setlist_default_desc("setlist_default_desc");
    MILO_ASSERT(GetState() != kUp, 0xC0);
    BandProfile *profile = TheProfileMgr.GetPrimaryProfile();
    if (profile) {
        mProfile = profile;
        unk9c = 0;
        unk64 = b1;
        mSetlistName = Localize(setlist_default_name, nullptr);
        mSetlistDescription = Localize(setlist_default_desc, nullptr);
        mSetlistArt.patchType = 0;
        return true;
    } else
        return false;
}

bool EditSetlistPanel::EditSetlist(LocalBandUser *user, LocalSavedSetlist *setlist) {
    MILO_ASSERT(GetState() != kUp, 0xD5);
    MILO_ASSERT(user, 0xD6);
    MILO_ASSERT(setlist, 0xD7);
    BandProfile *profile = TheProfileMgr.GetProfileForUser((const LocalUser *)user);
    if (user->CanSaveData() && profile) {
        unk9c = 1;
        mSetlistName = setlist->mTitle.c_str();
        mSetlistDescription = setlist->mDescription.c_str();
        mSetlistArt = setlist->mArt;
        mProfile = profile;
        mEditingSetlist = setlist;
        return true;
    } else
        return false;
}

bool EditSetlistPanel::CreateBattle() {
    static Symbol battle_default_name("battle_default_name");
    static Symbol battle_default_desc("battle_default_desc");
    MILO_ASSERT(GetState() != kUp, 0xEE);
    BandProfile *profile = TheProfileMgr.GetPrimaryProfile();
    if (profile) {
        LocalBandUser *user = profile->GetAssociatedLocalBandUser();
        if (user && user->IsSignedInOnline()) {
            unk9c = 2;
            mProfile = profile;
            mSetlistName = Localize(battle_default_name, nullptr);
            mSetlistDescription = Localize(battle_default_desc, nullptr);
            unk50 = kScoreBand;
            unk54 = 7;
            mSetlistArt.patchType = 0;
            unk84 = -1;
            return true;
        }
    }
    return false;
}

Symbol EditSetlistPanel::GetMessageToken() {
    static Symbol create_setlistshared_success("create_setlistshared_success");
    static Symbol create_setlist_success("create_setlist_success");
    static Symbol edit_setlist_success("edit_setlist_success");
    static Symbol create_battle_success("create_battle_success");
    static Symbol error_battle_limit_reached("error_battle_limit_reached");
    static Symbol error_setlist_limit_reached("error_setlist_limit_reached");
    static Symbol error_setlist_title_empty("error_setlist_title_empty");
    static Symbol error_setlist_description_empty("error_setlist_description_empty");
    static Symbol error_setlist_title_profane("error_setlist_title_profane");
    static Symbol error_setlist_description_profane("error_setlist_description_profane");
    static Symbol error_battle_title_profane("error_battle_title_profane");
    static Symbol error_battle_description_profane("error_battle_description_profane");
    static Symbol error_setlist_unknown("error_setlist_unknown");
    switch (mEditState) {
    case 7:
        switch (unk9c) {
        case 0:
            if (unk64)
                return create_setlistshared_success;
            return create_setlist_success;
        case 1:
            return edit_setlist_success;
        case 2:
            return create_battle_success;
        default:
            MILO_FAIL("Bad mode %i!");
            return gNullStr;
        }
        break;
    case 8:
        switch (unka4) {
        case 0:
            return error_battle_limit_reached;
        case 1:
            return error_setlist_limit_reached;
        case 2:
            return error_setlist_title_empty;
        case 3:
            return error_setlist_description_empty;
        case 4:
            if (unk9c != 2)
                return error_setlist_title_profane;
            return error_battle_title_profane;
        case 5:
            if (unk9c != 2)
                return error_setlist_description_profane;
            return error_battle_description_profane;
        case 6:
        case 7:
            return error_setlist_unknown;
        default:
            MILO_FAIL("Bad fail reason %i!");
            return gNullStr;
        }
        break;
    default:
        MILO_FAIL("In bad EditState %i in GetMessageToken!", mEditState);
        return gNullStr;
    }
}

Symbol EditSetlistPanel::GetTitleToken() {
    static Symbol setlist_save_share("setlist_save_share");
    static Symbol setlist_save_local("setlist_save_local");
    static Symbol setlist_save_battle("setlist_save_battle");
    static Symbol edit_setlist("edit_setlist");
    switch (unk9c) {
    case 0:
        return unk64 ? setlist_save_local : setlist_save_share;
    case 1:
        return edit_setlist;
    case 2:
        return setlist_save_battle;
    default:
        MILO_FAIL("Bad mode %i!");
        return gNullStr;
    }
}

RndTex *EditSetlistPanel::GetArtTex() {
    RndTex *tex = nullptr;
    if (mSetlistArt.patchType == 1) {
        MILO_ASSERT(mProfile, 0x18A);
        tex = mProfile->GetTexAtPatchIndex(mSetlistArt.patchIndex);
    }
    return tex;
}

void EditSetlistPanel::DoneEditing() {
    if (mSetlistName.empty()) {
        FailWithReason((FailureReason)2);
    } else if (mSetlistDescription.empty()) {
        FailWithReason((FailureReason)3);
    } else {
        switch (unk9c) {
        case 0:
            MILO_ASSERT(mProfile, 0x19D);
            mEditingSetlist = mProfile->AddSavedSetlist(
                mSetlistName.c_str(),
                mSetlistDescription.c_str(),
                unk64,
                mSetlistArt,
                TheMusicLibrary->GetSetlist()
            );
            TheSaveLoadMgr->AutoSave();
            SetEditState((EditState)7);
            break;
        case 1:
            MILO_ASSERT(mEditingSetlist, 0x1A6);
            MILO_ASSERT(mProfile, 0x1A7);
            mEditingSetlist->SetTitle(mSetlistName.c_str());
            mEditingSetlist->SetDescription(mSetlistDescription.c_str());
            mEditingSetlist->mArt = mSetlistArt;
            mEditingSetlist->unk68++;
            mProfile->SetlistChanged(mEditingSetlist);
            TheSaveLoadMgr->AutoSave();
            SetEditState((EditState)7);
            break;
        case 2:
            SetEditState((EditState)4);
            break;
        default:
            MILO_FAIL("Bad mode %i!");
            break;
        }
    }
}

void EditSetlistPanel::MessageOK() {
    switch (mEditState) {
    case 7:
        switch (unk9c) {
        case 0: {
            TheMusicLibrary->RebuildAndSortSetlists();
            TheMusicLibrary->SetSavedSetlistHighlight(mEditingSetlist);
            TheMusicLibrary->SetSort((SongSortType)8);
            static Symbol leave_setlist("leave_setlist");
            static Message leave_setlist_msg(leave_setlist);
            HandleType(leave_setlist_msg);
            break;
        }
        case 1: {
            TheMusicLibrary->RebuildAndSortSetlists();
            static Symbol leave_setlist("leave_setlist");
            static Message leave_setlist_msg(leave_setlist);
            HandleType(leave_setlist_msg);
            break;
        }
        case 2: {
            TheMusicLibrary->RefreshNetSetlists();
            TheMusicLibrary->SetSort((SongSortType)8);
            static Symbol leave_setlist("leave_setlist");
            static Message leave_setlist_msg(leave_setlist);
            HandleType(leave_setlist_msg);
            break;
        }
        default:
            MILO_FAIL("Bad mode %i!");
            break;
        }
        break;
    case 8:
        switch (unka4) {
        case 0:
        case 1:
        case 6: {
            static Symbol goto_create_dialog("goto_create_dialog");
            static Message goto_create_dialog_msg(goto_create_dialog);
            HandleType(goto_create_dialog_msg);
            break;
        }
        case 2:
        case 3:
        case 4:
        case 5:
        case 7:
            SetEditState((EditState)3);
            break;
        default:
            MILO_FAIL("Bad fail reason %i!");
            break;
        }
        break;
    default:
        MILO_FAIL("In bad EditState %i in MessageOK!", mEditState);
        break;
    }
}

DataNode EditSetlistPanel::OnMsg(const RockCentralOpCompleteMsg &msg) {
    switch (mEditState) {
    case 1:
        if (!msg.Success()) {
            FailWithReason((FailureReason)6);
        } else {
            DataNode statusNode(0);
            unk68.Update(NULL);
            unk68.GetDataResult(0)->GetDataResultValue(String("success"), statusNode);
            switch (statusNode.Int(NULL)) {
            case 0:
                SetEditState((EditState)3);
                break;
            case 22:
                FailWithReason((FailureReason)0);
                break;
            default:
                MILO_FAIL(
                    "Bad retcode %i while checking battle limits!",
                    statusNode.Int(NULL)
                );
                break;
            }
        }
        break;
    case 5: {
        if (!msg.Success() || msg.Arg1() != 12 || msg.Arg2().Int(NULL) <= 0) {
            FailWithReason((FailureReason)7);
        } else {
            unk84 = msg.Arg2().Int(NULL);
            SetEditState((EditState)6);
        }
        break;
    }
    case 6:
        if (!msg.Success()) {
            FailWithReason((FailureReason)7);
        } else {
            DataNode statusNode(0);
            unk68.Update(NULL);
            unk68.GetDataResult(0)->GetDataResultValue(String("success"), statusNode);
            switch (statusNode.Int(NULL)) {
            case 0:
                unk68.GetDataResult(0)->GetDataResultValue(String("battle_id"), statusNode);
                unk80 = statusNode.Int(NULL);
                SetEditState((EditState)7);
                break;
            case 15:
                FailWithReason((FailureReason)4);
                break;
            case 16:
                FailWithReason((FailureReason)5);
                break;
            default:
                MILO_FAIL(
                    "Bad retcode %i while submitting battle!", statusNode.Int(NULL)
                );
                break;
            }
        }
        break;
    default:
        MILO_FAIL("In bad EditState %i with RockCentralOpCompleteMsg!", mEditState);
        break;
    }
    return DataNode(1);
}

DataNode EditSetlistPanel::OnMsg(const DWCProfanityResultMsg &msg) {
    if (mEditState == kCheckingProfanity && unk98) {
        unk98 = 0;
        MILO_ASSERT(unk94, 0x252);
        if (msg.Success()) {
            bool b1 = !unk94[0];
            bool b2 = !unk94[1];
            CleanupStringVerify();
            VerifyStringsComplete(b1, b2);
        } else {
            CleanupStringVerify();
            FailWithReason((FailureReason)7);
        }
    }
    return DataNode(1);
}

void EditSetlistPanel::VerifyStrings(const char *name, const char *desc) {
    unk90 = (unsigned short **)new char[12];
    unsigned short *us = new unsigned short[strlen(name) + 1];
    UTF8toUTF16(us, name);
    *(unsigned short **)((char *)unk90 + 2) = us;
    *(unsigned short *)unk90 = wcslen((const wchar_t *)us) + 1;
    unsigned short *us2 = new unsigned short[strlen(desc) + 1];
    UTF8toUTF16(us2, desc);
    *(unsigned short **)((char *)unk90 + 8) = us2;
    *(unsigned short *)((char *)unk90 + 6) = wcslen((const wchar_t *)us2) + 1;
    unk94 = new char[14];
    memset(unk94, 0, 14);
    unk98 = new char[28];
    memset(unk98, 0, 28);
    unsigned int r = XStringVerify(0, "en-us", 2, unk90, 14, unk94, unk98);
    if (r != 0 && r != 0x3E5) {
        CleanupStringVerify();
        FailWithReason((FailureReason)7);
    }
}

void EditSetlistPanel::VerifyStringsComplete(bool b1, bool b2) {
    MILO_ASSERT(mEditState == kCheckingProfanity, 0x2BD);
    CleanupStringVerify();
    if (!b1)
        FailWithReason((FailureReason)4);
    else if (!b2)
        FailWithReason((FailureReason)5);
    else {
        if (GetArtTex()) {
            SetEditState((EditState)5);
        } else
            SetEditState((EditState)6);
    }
}

void EditSetlistPanel::SetEditState(EditState s) {
    if (mEditState != s) {
        mEditState = s;
        switch (s) {
        case 0:
            SetUIState((UIState)1);
            break;
        case 1:
            SetUIState((UIState)1);
            TheRockCentral.CheckBattleLimits(mProfile, unk68, this);
            break;
        case 2:
            MILO_ASSERT(mProfile, 0x2DD);
            if (mProfile->NumSavedSetlists() < 20) {
                SetEditState((EditState)3);
            } else {
                FailWithReason((FailureReason)1);
            }
            break;
        case 3:
            SetUIState((UIState)0);
            break;
        case 4:
            SetUIState((UIState)1);
            VerifyStrings(mSetlistName.c_str(), mSetlistDescription.c_str());
            break;
        case 5:
            SetUIState((UIState)1);
            unk84 = -1;
            TheRockCentral.UpdateBattleArt(GetArtTex(), this, 0);
            break;
        case 6:
            SetUIState((UIState)1);
            unk80 = -1;
            TheRockCentral.CreateBattle(
                mProfile,
                mSetlistName.c_str(),
                mSetlistDescription.c_str(),
                TheMusicLibrary->GetSetlist(),
                mSetlistArt,
                unk84,
                unk50,
                unk54,
                (BattleTimeUnits)unk58,
                unk68,
                this,
                -1,
                1
            );
            break;
        case 7:
        case 8:
            SetUIState((UIState)2);
            break;
        default:
            MILO_FAIL("Bad edit state %i!");
            break;
        }
    }
}

void EditSetlistPanel::SetUIState(UIState state) {
    switch (state) {
    case 0: {
        static Symbol set_edit_state("set_edit_state");
        static Message msg(set_edit_state, 0);
        msg[0] = unk9c == 2;
        HandleType(msg);
        break;
    }
    case 1: {
        static Symbol set_wait_state("set_wait_state");
        static Message msg(set_wait_state);
        HandleType(msg);
        break;
    }
    case 2: {
        static Symbol set_message_state("set_message_state");
        static Message msg(set_message_state);
        HandleType(msg);
        break;
    }
    default:
        MILO_FAIL("Bad ui state %i!");
    }
}

void EditSetlistPanel::FailWithReason(FailureReason r) {
    unka4 = r;
    SetEditState((EditState)8);
}

int EditSetlistPanel::SymToDayCount(Symbol s) {
    static Symbol expiration_data("expiration_data");
    DataArray *a = Property(expiration_data, true)->Array();
    return a->FindInt(s);
}

int EditSetlistPanel::SymToTimeUnits(Symbol s) {
    static Symbol seconds("seconds");
    static Symbol minutes("minutes");
    static Symbol hours("hours");
    static Symbol weeks("weeks");
    if (s == seconds)
        return 0;
    if (s == minutes)
        return 1;
    if (s == hours)
        return 2;
    return 3 + (s == weeks);
}

Symbol EditSetlistPanel::DayCountToSym(int days) {
    static Symbol expiration_data("expiration_data");
    DataArray *a = Property(expiration_data, true)->Array();
    for (int i = 0; i < a->Size(); i++) {
        DataArray *a2 = a->Array(i);
        if (a2->Int(1) == days)
            return a2->Sym(0);
    }
    MILO_FAIL("No matching sym for %i days in EditSetlistPanel::DayCountToSym", days);
    return gNullStr;
}

#pragma push
#pragma dont_inline on
BEGIN_HANDLERS(EditSetlistPanel)
    HANDLE_EXPR(create_setlist, CreateSetlist(_msg->Int(2)))
    HANDLE_EXPR(
        edit_setlist,
        EditSetlist(_msg->Obj<LocalBandUser>(2), _msg->Obj<LocalSavedSetlist>(3))
    )
    HANDLE_EXPR(create_battle, CreateBattle())
    HANDLE_ACTION(done_editing, DoneEditing())
    HANDLE_EXPR(editing_setlist, unk9c == 1)
    HANDLE_ACTION(message_ok, MessageOK())
    HANDLE_EXPR(get_message_token, GetMessageToken())
    HANDLE_EXPR(get_title_token, GetTitleToken())
    HANDLE_EXPR(get_art_tex, GetArtTex())
    HANDLE_EXPR(get_instrument_list_sym, ScoreTypeToSym(unk50))
    HANDLE_EXPR(get_expiration_list_sym, DayCountToSym(unk54))
    HANDLE_ACTION(set_instrument_to_list_sym, unk50 = SymToScoreType(_msg->Sym(2)))
    HANDLE_ACTION(set_expiration_to_list_sym, unk54 = SymToDayCount(_msg->Sym(2)))
    HANDLE_ACTION(set_expiration_val_cheat, unk54 = _msg->Int(2))
    HANDLE_ACTION(set_expiration_units_cheat, unk58 = SymToTimeUnits(_msg->Sym(2)))
    HANDLE_MESSAGE(RockCentralOpCompleteMsg)
    HANDLE_MESSAGE(UITransitionCompleteMsg)
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(0x37D)
END_HANDLERS
#pragma pop

BEGIN_PROPSYNCS(EditSetlistPanel)
    // The two const char* props are hand-expanded because retail materializes
    // the DataNode temp as a NAMED local (target `addi r4, r31, 0x68` /
    // `addi r4, r31, 0x70`) rather than reusing the ctor's returned `this`
    // (`mr r4, r3`), which is what SYNC_PROP_SET's `_val = DataNode(member)`
    // unnamed temporary emits. Expanded here instead of changing ObjMacros.h,
    // whose SYNC_PROP_SET form is already correct for every other unit.
    {
        _NEW_STATIC_SYMBOL(setlist_name)
        if (sym == _s) {
            if (_op == kPropSet) {
                mSetlistName = _val.Str();
            } else {
                if (_op == (PropOp)0x40)
                    return false;
                DataNode _tmp(mSetlistName.c_str());
                _val = _tmp;
            }
            return true;
        }
    }
    {
        _NEW_STATIC_SYMBOL(setlist_desc)
        if (sym == _s) {
            if (_op == kPropSet) {
                mSetlistDescription = _val.Str();
            } else {
                if (_op == (PropOp)0x40)
                    return false;
                DataNode _tmp(mSetlistDescription.c_str());
                _val = _tmp;
            }
            return true;
        }
    }
    SYNC_PROP_SET_STATIC(setlist_inst, unk50, unk50 = (ScoreType)_val.Int())
    SYNC_PROP_SET_STATIC(setlist_seconds, unk54 * 86400, unk54 = _val.Int() / 86400)
END_PROPSYNCS
