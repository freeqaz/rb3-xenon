#include "game/TrackerDisplay.h"
#include "bandobj/BandTrack.h"
#include "bandtrack/TrackPanel.h"
#include "beatmatch/TrackType.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "utl/Messages.h"
#include "utl/Symbols.h"

// ---------------------------------------------------------------------------
// LOCAL-STATIC Symbol/Message conversion (laneAX-W8).
//
// Retail declares every property Symbol and every Handle()/SendMsg() Message
// in this TU as a FUNCTION-LOCAL static, not a reference to the global in
// Symbols.h / Messages.h.  Each site below was recovered from the dtk-split
// target obj by scripts/harvest/localstatic_patch_gen.py: guard-word test/set
// block + REFHI/REFLO relocation onto the .rdata string in
// orig/45410914/band.exe.  Converted as a whole TU (the guard words, ??__E
// dynamic-init and ??__F atexit thunks are emitted and ordered per-TU, so a
// half-converted TU is the worst state).
//
// NOTE on the target symbol map: this unit's map is off by one over the
// Make*TargetDescription block -- the VA labelled
// ?Initialize@TrackerDisplay@@QAAXVSymbol@@@Z actually carries
// tracker_percentage_target, i.e. it is MakePercentTargetDescription, and the
// real Initialize is the anonymous fn_826D2E80.  Likewise the VA labelled
// ?ShowBriefBandMessage@TrackerBroadcastDisplay@@... carries set_band_message
// (= SetBandMessage) while fn_826D54F0 carries show_brief_band_message.  The
// edits below follow the STRINGS, not the map labels.
// ---------------------------------------------------------------------------

float TrackerDisplay::kMissingPercentage = -1.0f;

TrackerDisplay::TrackerDisplay() {}

TrackerDisplay::~TrackerDisplay() {}

void TrackerDisplay::MsToMinutesSeconds(float ms, int &min, int &sec) {
    int totalsecs = ms / 1000.0f;
    min = totalsecs / 60;
    sec = totalsecs % 60;
}

DataArrayPtr TrackerDisplay::MakeIntegerTargetDescription(int i) {
    static Symbol tracker_integer_target("tracker_integer_target");
    return DataArrayPtr(tracker_integer_target, i);
}

DataArrayPtr TrackerDisplay::MakePercentTargetDescription(float perc) {
    static Symbol tracker_percentage_target("tracker_percentage_target");
    return DataArrayPtr(tracker_percentage_target, perc * 100.0f);
}

DataArrayPtr TrackerDisplay::MakeTimeTargetDescription(float ms) {
    static Symbol tracker_time_target("tracker_time_target");
    int min, sec;
    MsToMinutesSeconds(ms, min, sec);
    return DataArrayPtr(tracker_time_target, min, sec);
}

void TrackerDisplay::Initialize(Symbol s) {
    static Symbol show("show");
    static Message msg(show, 0);
    msg[0] = s;
    SendMsg(msg);
}

void TrackerDisplay::Show() const {
    // Retail (guard 0x82E03128, bits 0x1/0x2) initialises BOTH a static Symbol
    // (storage 0x82E03124) and a static Message built from it (storage
    // 0x82E0311C, with its atexit thunk) -- and then sends a *temporary*
    // Message constructed from the Symbol (built at r31+0x50 and released via
    // DataArray::Release on the way out). The static Message is never passed.
    static Symbol showSym("show");
    static Message show_msg(showSym);
    Message msg(showSym);
    SendMsg(msg);
}
void TrackerDisplay::Hide() const {
    static Symbol hideSym("hide");
    static Message hide_msg(hideSym);
    SendMsg(hide_msg);
}

void TrackerDisplay::SetChallengeType(TrackerChallengeType ty) const {
    static Symbol set_challenge_type("set_challenge_type");
    static Message msg(set_challenge_type, 0);
    msg[0] = ty;
    SendMsg(msg);
}

void TrackerDisplay::SetIntegerProgress(int i) const {
    static Symbol set_int_progress("set_int_progress");
    static Message msg(set_int_progress, 0);
    msg[0] = i;
    SendMsg(msg);
}

#pragma push
#pragma pool_data off
void TrackerDisplay::SetPercentageProgress(float f) const {
    // Retail declares these three as FUNCTION-LOCAL statics at the top of the
    // function (guard word 0x82E03178, bits 0x1/0x2/0x4, storages 0x82E03174 /
    // 0x82E03170 / 0x82E0316C; strings read out of the PE .rdata). Declaration
    // order == ctor address order == guard-bit order.
    static Symbol set_progress("set_progress");
    static Symbol tracker_percentage("tracker_percentage");
    static Symbol tracker_percentage_missing("tracker_percentage_missing");
    if (f < 0) {
        DataArrayPtr ptr(tracker_percentage_missing);
        static Message msg(set_progress, 0);
        msg[0] = ptr;
        SendMsg(msg);
    } else {
        DataArrayPtr ptr(tracker_percentage, std::floor(f * 100.0f));
        static Message msg(set_progress, 0);
        msg[0] = ptr;
        SendMsg(msg);
    }
}
#pragma pop

void TrackerDisplay::SetTimeProgress(float ms) const {
    // Function-local statics, retail guard 0x82E0318C bits 0x1/0x2 (storages
    // 0x82E03188 / 0x82E03184).
    static Symbol set_progress("set_progress");
    static Symbol tracker_time_remaining("tracker_time_remaining");
    int min, sec;
    MsToMinutesSeconds(ms, min, sec);
    DataArrayPtr ptr(tracker_time_remaining, min, sec);
    static Message msg(set_progress, 0);
    msg[0] = ptr;
    SendMsg(msg);
}

void TrackerDisplay::HandleIncrement() {
    static Message msg("target_progress");
    SendMsg(msg);
}

void TrackerDisplay::ShowTarget(DataArrayPtr &ptr) const {
    static Symbol set_target("set_target");
    static Message msg(set_target, 0);
    msg[0] = ptr;
    SendMsg(msg);
}

void TrackerDisplay::HandleTargetPass(int i, DataArrayPtr &ptr) const {
    static Symbol advance_target("advance_target");
    static Message msg(advance_target, 0, 0);
    msg[0] = i;
    msg[1] = ptr;
    SendMsg(msg);
}

void TrackerDisplay::LastTargetPass() const {
    static Message msg("last_target_passed");
    SendMsg(msg);
}

TrackerBandDisplay::TrackerBandDisplay() {}

TrackerBandDisplay::~TrackerBandDisplay() {}

void TrackerBandDisplay::SetType(TrackerBandDisplayType ty) const {
    static Symbol set_display_type("set_display_type");
    static Message msg(set_display_type, 0);
    msg[0] = ty;
    SendMsg(msg);
}

void TrackerBandDisplay::SetStyle(TrackerBandDisplayStyle sty) const {
    static Symbol set_display_style("set_display_style");
    static Message msg(set_display_style, 0);
    msg[0] = sty;
    SendMsg(msg);
}

void TrackerBandDisplay::SetSuccessState(bool b) const {
    static Symbol set_success_state("set_success_state");
    static Message msg(set_success_state, 0);
    msg[0] = b;
    SendMsg(msg);
}

void TrackerBandDisplay::SetProgressPercentage(float f) const {
    static Symbol set_progress_percentage("set_progress_percentage");
    static Message msg(set_progress_percentage, 0.0f);
    msg[0] = f;
    SendMsg(msg);
}

void TrackerBandDisplay::SendMsg(const Message &msg) const {
    GetTrackPanel()->SendTrackerDisplayMessage(msg);
}

TrackerPlayerDisplay::TrackerPlayerDisplay() : mPlayer(0) {}

TrackerPlayerDisplay::~TrackerPlayerDisplay() {}

void TrackerPlayerDisplay::Hide() const { TrackerDisplay::Hide(); }

// fn_826D2870 in retail: NOT inlined into its six callers despite /Ob2.
__declspec(noinline) bool TrackerPlayerDisplay::HasLocalPlayer() const {
    return mPlayer && mPlayer->IsLocal();
}

void TrackerPlayerDisplay::Enable() const {
    static Message enable_msg("enable");
    SendMsg(enable_msg);
    bool cansend = HasLocalPlayer();
    if (cansend) {
        SendPlayerDisplayMsg((NetDisplayMsg)0, 0, 0);
    }
}

void TrackerPlayerDisplay::Disable() const {
    static Message disable_msg("disable");
    return SendMsg(disable_msg);
}

void TrackerPlayerDisplay::GainFocus(bool gain) const {
    static Symbol gain_focus("gain_focus");
    static Message msg(gain_focus, 0);
    msg[0] = gain;
    SendMsg(msg);
    bool cansend = HasLocalPlayer();
    if (cansend) {
        SendPlayerDisplayMsg((NetDisplayMsg)1, gain, 0);
    }
}

void TrackerPlayerDisplay::LoseFocus(bool lose) const {
    static Symbol lose_focus("lose_focus");
    static Message msg(lose_focus, 0);
    msg[0] = lose;
    SendMsg(msg);
    bool cansend = HasLocalPlayer();
    if (cansend) {
        SendPlayerDisplayMsg((NetDisplayMsg)2, lose, 0);
    }
}

void TrackerPlayerDisplay::SetSuccessState(bool succ) const {
    static Symbol set_success_state("set_success_state");
    static Message msg(set_success_state, 0);
    msg[0] = succ;
    SendMsg(msg);
}

void TrackerPlayerDisplay::Pulse(bool topulse) const {
    static Symbol pulse("pulse");
    static Message msg(pulse, 0);
    msg[0] = topulse;
    SendMsg(msg);
    bool cansend = HasLocalPlayer();
    if (cansend) {
        SendPlayerDisplayMsg((NetDisplayMsg)3, topulse, 0);
    }
}

void TrackerPlayerDisplay::SetProgressPercentage(float perc, bool b) const {
    static Symbol set_progress_percentage("set_progress_percentage");
    static Message msg(set_progress_percentage, 0.0f, 0);
    msg[0] = perc;
    msg[1] = b;
    SendMsg(msg);
}

void TrackerPlayerDisplay::FillProgressAndReset(bool b) const {
    if (!b || (mPlayer && mPlayer->GetTrackType() == kTrackVocals)) {
        static Message fill_progress_and_reset_msg("fill_progress_and_reset");
        SendMsg(fill_progress_and_reset_msg);
    }
    bool cansend = HasLocalPlayer();
    if (cansend) {
        SendPlayerDisplayMsg((NetDisplayMsg)4, b, 0);
    }
}

void TrackerPlayerDisplay::SetSecondaryStateLevel(int i) const {
    static Symbol set_secondary_state_level("set_secondary_state_level");
    static Message msg(set_secondary_state_level, 0.0f);
    msg[0] = i;
    SendMsg(msg);
    bool cansend = HasLocalPlayer();
    if (cansend) {
        SendPlayerDisplayMsg((NetDisplayMsg)5, i, 0);
    }
}

void TrackerPlayerDisplay::RemotePlayerDisplayMsg(int i1, int i2, int i3) const {
    switch (i1) {
    case 0:
        Enable();
        break;
    case 1:
        GainFocus(i2);
        break;
    case 2:
        LoseFocus(i2);
        break;
    case 3:
        Pulse(i2);
        break;
    case 4:
        FillProgressAndReset(i2);
        break;
    case 5:
        SetSecondaryStateLevel(i2);
        break;
    default:
        MILO_WARN("Unhandled tracker player display msg: %d %d %d.\n", i1, i2, i3);
        break;
    }
}

void TrackerPlayerDisplay::SendPlayerDisplayMsg(NetDisplayMsg msg, int i1, int i2) const {
    if (mPlayer) {
        static Message displayMsg("send_tracker_player_display", 0, 0, 0);
        displayMsg[0] = msg;
        displayMsg[1] = i1;
        displayMsg[2] = i2;
        mPlayer->HandleType(displayMsg);
    }
}

void TrackerPlayerDisplay::SendMsg(const Message &msg) const {
    MILO_ASSERT(mPlayer, 0x233);
    BandTrack *track = mPlayer->GetBandTrack();
    if (track)
        track->SendTrackerDisplayMessage(msg);
}

TrackerBroadcastDisplay::TrackerBroadcastDisplay() {}

TrackerBroadcastDisplay::~TrackerBroadcastDisplay() {}

void TrackerBroadcastDisplay::Broadcast(const DataArrayPtr &ptr, Symbol s) {
    SetType((BroadcastDisplayType)0);
    static Symbol broadcast("broadcast");
    static Message msg(broadcast, 0, 0);
    msg[0] = ptr;
    msg[1] = s;
    SendMsg(msg);
}

void TrackerBroadcastDisplay::SetType(BroadcastDisplayType ty) const {
    static Symbol set_display_type("set_display_type");
    static Message msg(set_display_type, 0);
    msg[0] = ty;
    SendMsg(msg);
}

void TrackerBroadcastDisplay::SetSecondaryStateLevel(int level) const {
    static Symbol set_secondary_state_level("set_secondary_state_level");
    static Message msg(set_secondary_state_level, 0);
    msg[0] = level;
    SendMsg(msg);
}

void TrackerBroadcastDisplay::SetBandMessage(const DataArrayPtr &ptr) const {
    SetType((BroadcastDisplayType)1);
    static Symbol set_band_message("set_band_message");
    static Message msg(set_band_message, 0);
    msg[0] = ptr;
    SendMsg(msg);
}

void TrackerBroadcastDisplay::ShowBriefBandMessage(const DataArrayPtr &ptr) const {
    SetType((BroadcastDisplayType)1);
    static Symbol show_brief_band_message("show_brief_band_message");
    static Message msg(show_brief_band_message, 0);
    msg[0] = ptr;
    SendMsg(msg);
}

void TrackerBroadcastDisplay::SendMsg(const Message &msg) const {
    GetTrackPanel()->SendTrackerBroadcastDisplayMessage(msg);
}
