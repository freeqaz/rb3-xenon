#include "meta_band/OvershellSlotState.h"
#include "meta_band/OvershellSlot.h"
#include "utl/Messages.h"

OvershellSlotState::OvershellSlotState(DataArray *da, OvershellSlot *slot) : mSlot(slot) {
    SetTypeDef(da);
    mStateID = (OvershellSlotStateID)da->Int(0);
}

OvershellSlotStateID OvershellSlotState::GetStateID() const { return mStateID; }

Symbol OvershellSlotState::GetView() {
    static Message view_msg("view");
    return HandleMsg(view_msg).Sym();
}

void OvershellSlotState::UpdateView() {
    static Message update_view_msg("update_view");
    HandleMsg(update_view_msg);
}

bool OvershellSlotState::UsesRemoteStatusView() {
    if (GetRemoteStatus() != gNullStr)
        return true;
    else
        return !InSongSettingsFlow();
}

Symbol OvershellSlotState::GetRemoteStatus() {
    static Message remote_status_msg("remote_status");
    DataNode handled = HandleMsg(remote_status_msg);
    if (handled.Type() != kDataUnhandled)
        return handled.Sym();
    else
        return gNullStr;
}

bool OvershellSlotState::AllowsInputToShell() {
    static Message allows_input_to_shell_msg("allows_input_to_shell");
    DataNode handled = HandleMsg(allows_input_to_shell_msg);
    bool ret = handled.Type() != kDataUnhandled && handled.Int() != 0;
    return ret;
}

bool OvershellSlotState::PreventsOverride() {
    static Message prevents_override_msg("prevents_override");
    DataNode handled = HandleMsg(prevents_override_msg);
    bool ret = handled.Type() != kDataUnhandled && handled.Int() != 0;
    return ret;
}

bool OvershellSlotState::RequiresOnlineSession() {
    static Message requires_online_session_msg("requires_online_session");
    DataNode handled = HandleMsg(requires_online_session_msg);
    bool ret = handled.Type() != kDataUnhandled && handled.Int() != 0;
    return ret;
}

bool OvershellSlotState::RequiresRemoteUsers() {
    static Message requires_remote_users_msg("requires_remote_users");
    DataNode handled = HandleMsg(requires_remote_users_msg);
    bool ret = handled.Type() != kDataUnhandled && handled.Int() != 0;
    return ret;
}

bool OvershellSlotState::AllowsHiding() {
    static Message allows_hiding_msg("allows_hiding");
    DataNode handled = HandleMsg(allows_hiding_msg);
    bool ret = handled.Type() != kDataUnhandled && handled.Int() != 0;
    return ret;
}

bool OvershellSlotState::RetractedPosition() {
    static Message retracted_position_msg("retracted_position");
    DataNode handled = HandleMsg(retracted_position_msg);
    bool ret = handled.Type() != kDataUnhandled && handled.Int() != 0;
    return ret;
}

bool OvershellSlotState::ShowsExtendedMicArrows() {
    static Message shows_extended_mic_arrows_msg("shows_extended_mic_arrows");
    DataNode handled = HandleMsg(shows_extended_mic_arrows_msg);
    bool ret = handled.Type() != kDataUnhandled && handled.Int() != 0;
    return ret;
}

bool OvershellSlotState::InSongSettingsFlow() {
    static Message song_settings_flow_msg("song_settings_flow");
    DataNode handled = HandleMsg(song_settings_flow_msg);
    bool ret = handled.Type() != kDataUnhandled && handled.Int() != 0;
    return ret;
}

bool OvershellSlotState::IsPartUnresolved() {
    if (!InSongSettingsFlow())
        return false;
    else {
        static Message part_unresolved_msg("part_unresolved");
        DataNode handled = HandleMsg(part_unresolved_msg);
        bool ret = handled.Type() != kDataUnhandled && handled.Int() != 0;
        return ret;
    }
}

bool OvershellSlotState::InRegisterOnlineFlow() {
    static Message register_online_flow_msg("register_online_flow");
    DataNode handled = HandleMsg(register_online_flow_msg);
    bool ret = handled.Type() != kDataUnhandled && handled.Int() != 0;
    return ret;
}

bool OvershellSlotState::InChooseCharFlow() {
    static Message choose_char_flow_msg("choose_char_flow");
    DataNode handled = HandleMsg(choose_char_flow_msg);
    bool ret = handled.Type() != kDataUnhandled && handled.Int() != 0;
    return ret;
}

bool OvershellSlotState::InCharEditFlow() {
    static Message char_edit_flow_msg("char_edit_flow");
    DataNode handled = HandleMsg(char_edit_flow_msg);
    bool ret = handled.Type() != kDataUnhandled && handled.Int() != 0;
    return ret;
}

bool OvershellSlotState::IsRemoveUserPrompt() {
    static Message remove_user_prompt_msg("remove_user_prompt");
    DataNode handled = HandleMsg(remove_user_prompt_msg);
    bool ret = handled.Type() != kDataUnhandled && handled.Int() != 0;
    return ret;
}

bool OvershellSlotState::IsReadyToPlay() {
    static Message ready_to_play_msg("ready_to_play");
    DataNode handled = HandleMsg(ready_to_play_msg);
    bool ret = handled.Type() != kDataUnhandled && handled.Int() != 0;
    return ret;
}

DataNode OvershellSlotState::HandleMsg(const Message &msg) {
    DataNode ret(kDataUnhandled, 0);
    if (TypeDef()) {
        DataArray *arr = TypeDef()->FindArray(msg.Type(), false);
        if (arr) {
            ret = arr->ExecuteScript(1, mSlot, msg, 2);
        }
    }
    return ret;
}

OvershellSlotStateMgr::OvershellSlotStateMgr() {}

void OvershellSlotStateMgr::Init(DataArray *data, OvershellSlot *slot) {
    for (int n = 0; n < data->Size(); n++) {
        if (data->Type(n) == kDataArray) {
            mStates.push_back(new OvershellSlotState(data->Array(n), slot));
        } else {
            MILO_ASSERT(n == 0 && data->Type(n) == kDataSymbol, 0xC4);
        }
    }
}

OvershellSlotStateMgr::~OvershellSlotStateMgr() { DeleteAll(mStates); }

OvershellSlotState *OvershellSlotStateMgr::GetSlotState(OvershellSlotStateID id) {
    for (int i = 0; i < mStates.size(); i++) {
        if (id == mStates[i]->mStateID)
            return mStates[i];
    }
    MILO_FAIL("OvershellSlotState %d does not exist", id);
    return 0;
}
