#pragma once
#include "game/Player.h"
#include "obj/Data.h"
#include "obj/Msg.h"

// Values and names from config/macros.dta (kTrackerChallengeType_*).
enum TrackerChallengeType {
    kTrackerChallengeType_None = 0,
    kTrackerChallengeType_Accuracy = 1,
    kTrackerChallengeType_Overdrive = 2,
    kTrackerChallengeType_Streak = 3
};

enum TrackerBandDisplayType {
    kTrackerBandDisplay_Graphic = 0,
    kTrackerBandDisplay_Text = 1
};

enum TrackerBandDisplayStyle {
};

class TrackerDisplay {
public:
    TrackerDisplay();
    virtual ~TrackerDisplay();
    virtual void Show() const;
    virtual void Hide() const;
    virtual void SendMsg(const Message &) const = 0;

    void Initialize(Symbol);
    void SetChallengeType(TrackerChallengeType) const;
    void SetIntegerProgress(int) const;
    void SetPercentageProgress(float) const;
    void SetTimeProgress(float) const;
    void HandleIncrement();
    void ShowTarget(DataArrayPtr &) const;
    void HandleTargetPass(int, DataArrayPtr &) const;
    void LastTargetPass() const;

    static float kMissingPercentage;
    static void MsToMinutesSeconds(float, int &, int &);
    static DataArrayPtr MakeIntegerTargetDescription(int);
    static DataArrayPtr MakePercentTargetDescription(float);
    static DataArrayPtr MakeTimeTargetDescription(float);
};

class TrackerBandDisplay : public TrackerDisplay {
public:
    TrackerBandDisplay();
    virtual ~TrackerBandDisplay();
    virtual void SendMsg(const Message &) const;

    void SetType(TrackerBandDisplayType) const;
    void SetStyle(TrackerBandDisplayStyle) const;
    void SetSuccessState(bool) const;
    void SetProgressPercentage(float) const;
};

class TrackerPlayerDisplay : public TrackerDisplay {
public:
    // The message SendPlayerDisplayMsg passes to the remote copy, one per
    // display call (RemotePlayerDisplayMsg switches on it). Retail's names are
    // not known; these name the TrackerPlayerDisplay method that sends each.
    enum NetDisplayMsg {
        kNetDisplay_Enable = 0,
        kNetDisplay_GainFocus = 1,
        kNetDisplay_LoseFocus = 2,
        kNetDisplay_Pulse = 3,
        kNetDisplay_FillProgressAndReset = 4,
        kNetDisplay_SecondaryStateLevel = 5
    };

    TrackerPlayerDisplay();
    virtual ~TrackerPlayerDisplay();
    virtual void Hide() const;
    virtual void SendMsg(const Message &) const;

    void Enable() const;
    void Disable() const;
    void GainFocus(bool) const;
    void LoseFocus(bool) const;
    void SetSuccessState(bool) const;
    void Pulse(bool) const;
    void SetProgressPercentage(float, bool) const;
    void FillProgressAndReset(bool) const;
    void SetSecondaryStateLevel(int) const;
    void RemotePlayerDisplayMsg(int, int, int) const;
    void SendPlayerDisplayMsg(NetDisplayMsg, int, int) const;
    // Retail keeps `mPlayer && mPlayer->IsLocal()` OUT OF LINE as fn_826D2870
    // (own frame, `lwz r3, 0x4(r3)`, virtual IsLocal at vtable+0x1c, bool
    // return) and calls it from all six senders. Inlining it costs each caller
    // 10 instructions plus an 8-byte stack slot.
    bool HasLocalPlayer() const;

    Player *mPlayer; // 0x4
};

class TrackerBroadcastDisplay : public TrackerDisplay {
public:
    enum BroadcastDisplayType {
    };

    TrackerBroadcastDisplay();
    virtual ~TrackerBroadcastDisplay();
    virtual void SendMsg(const Message &) const;

    void Broadcast(const DataArrayPtr &, Symbol);
    void SetType(BroadcastDisplayType) const;
    void SetSecondaryStateLevel(int) const;
    void SetBandMessage(const DataArrayPtr &) const;
    void ShowBriefBandMessage(const DataArrayPtr &) const;
};