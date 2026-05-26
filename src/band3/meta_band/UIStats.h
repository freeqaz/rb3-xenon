#pragma once
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/JoypadMsgs.h"
#include "os/OnlineID.h"
#include "ui/UIComponent.h"
#include "ui/UIScreen.h"
#include "utl/Str.h"

class UIStats : public Hmx::Object {
public:
    UIStats();
    virtual ~UIStats() {}
    virtual DataNode Handle(DataArray *, bool);

    void Init();
    void Terminate();
    void Poll();
    void DropScreen(UIScreen *);
    void MaybePublish(UIScreen *);
    void EventLog(unsigned int, unsigned int, unsigned int);

    DataNode OnMsg(const ButtonDownMsg &);
    DataNode OnMsg(const ButtonUpMsg &);
    DataNode OnMsg(const JoypadConnectionMsg &);
    DataNode OnMsg(const UIComponentFocusChangeMsg &);
    DataNode OnMsg(const UIScreenChangeMsg &);

    bool mPublishingPad; // 0x1c
    Symbol mLastMode; // 0x20
    unsigned char mLastWasParticipating[4]; // 0x24
    OnlineID mLastPadID[4]; // 0x28
    String mLastBreedString[4]; // 0x48
    OnlineID mLastRemoteID[4]; // 0x78
    int mLastControllerType[4]; // 0x98
    int mLastDroppedScreen; // 0xa8
    int mLastPublishTime; // 0xac
    void *mPadLogBuffer; // 0xb0
    void *mPadLogWritePtr; // 0xb4
    int mPadLogCount; // 0xb8
};

extern UIStats gUIStats;
extern UIStats *TheUIStats;
