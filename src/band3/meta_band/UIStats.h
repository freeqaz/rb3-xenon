#pragma once
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/JoypadMsgs.h"
#include "os/OnlineID.h"
#include "ui/UI.h"
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

    bool mPublishingPad; // 0x28
    Symbol mLastMode; // 0x2c
    unsigned char mLastWasParticipating[4]; // 0x30
    OnlineID mLastPadID[4]; // 0x38
    String mLastBreedString[4]; // 0x78
    OnlineID mLastRemoteID[4]; // 0xa8
    int mLastControllerType[4]; // 0xe8
    int mLastDroppedScreen; // 0xf8
    int mLastPublishTime; // 0xfc
    void *mPadLogBuffer; // 0x100
    void *mPadLogWritePtr; // 0x104
    int mPadLogCount; // 0x108
};

extern UIStats gUIStats;
extern UIStats *TheUIStats;
