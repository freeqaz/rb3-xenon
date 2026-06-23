#include "meta_band/UIStats.h"
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "game/Defines.h"
#include "game/GameMode.h"
#include "net/Net.h"
#include "net/Server.h"
#include "obj/Data.h"
#include "obj/Msg.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "os/Joypad.h"
#include "os/JoypadMsgs.h"
#include "os/PlatformMgr.h"
#include "os/System.h"
#include "ui/UI.h"
#include "utl/Compress.h"
#include "utl/DataPointMgr.h"
#include "utl/MakeString.h"
#include "utl/MemMgr.h"
#include "utl/Str.h"
#include "utl/Symbols3.h"
#include <string.h>

UIStats gUIStats;
UIStats *TheUIStats = &gUIStats;

UIStats::UIStats() {}

void UIStats::Init() {
    void *mem = _MemAlloc(0x10000, 0);
    mPadLogBuffer = mem;
    mPadLogWritePtr = mem;
    mPadLogCount = 0;
    mLastDroppedScreen = 0;
    mPublishingPad = false;
    mLastPublishTime = SystemMs();
}

void UIStats::Terminate() {
    if (mPadLogBuffer) {
        _MemFree(mPadLogBuffer);
        mPadLogBuffer = 0;
    }
    mPadLogWritePtr = 0;
}

void UIStats::Poll() {}

void UIStats::DropScreen(UIScreen *screen) {
    mPadLogWritePtr = mPadLogBuffer;
    mPadLogCount = 0;
    mLastDroppedScreen++;
}

void UIStats::MaybePublish(UIScreen *from) {
    if (!from) return;
    mLastPublishTime = SystemMs();
    MILO_ASSERT(from, 0x48);

    Server *server = TheNet.mServer;
    bool &_ref0 = mPublishingPad;
    const DataArray *gather = from->TypeDef()->FindArray(gather_uistats, false);
    if (!server->IsConnected()
        || (gather && gather->Node(1).Int(gather) == 0)) {
        if (!server->IsConnected()) {
            _ref0 = false;
        }
        DropScreen(from);
        return;
    }

    if (!_ref0) {
        mLastMode = Symbol("");
        for (int i = 0; i < 4; i++) {
            mLastWasParticipating[i] = 0;
            OnlineID id1;
            mLastPadID[i] = id1;
            mLastBreedString[i] = "00";
            OnlineID id2;
            mLastRemoteID[i] = id2;
            mLastControllerType[i] = kControllerNone;
        }
    }
    _ref0 = true;

    DataPoint screenExit("stats/screen_exit");
    DataPoint padUser("stats/pad_user");

    screenExit.AddPair("name", DataNode(from->Name()));
    padUser.AddPair("name", DataNode(from->Name()));

    Symbol curMode = TheGameMode->mMode;
    if (curMode != mLastMode) {
        screenExit.AddPair("mode", DataNode(curMode));
        mLastMode = curMode;
    }

    if (mPadLogCount != 0) {
        int compressedSize = 0x10100;
        unsigned char stackbuf[0x10100];
        unsigned char *buf = stackbuf + 0x100;
        unsigned char *base = (unsigned char *)mPadLogBuffer;
        unsigned char *write = (unsigned char *)mPadLogWritePtr;
        unsigned int writeOff = (unsigned int)(write - base);
        int size = 0x10000;
        if (write == base || (unsigned int)mPadLogCount < 0x4000) {
            if ((unsigned int)mPadLogCount < 0x4000) size = writeOff;
            memcpy(buf, base, size);
        } else {
            int tail = 0x10000 - writeOff;
            memcpy(buf + tail, base, writeOff);
            memcpy(buf, mPadLogWritePtr, tail);
        }
        if ((unsigned int)mPadLogCount >= 0x1A) {
            CompressMem(buf, size, stackbuf, compressedSize, 0);
        } else {
            memmove(stackbuf, buf, size);
            compressedSize = size;
        }

        String hex(MakeString("%x:", (unsigned int)mPadLogCount));
        int prefixLen = strlen(hex.c_str());
        hex.resize(prefixLen + (compressedSize * 2));
        unsigned char *src = stackbuf;
        char *dst = (char *)hex.c_str() + prefixLen;
        for (int i = 0; i < compressedSize; i++) {
            unsigned char b = *src;
            unsigned int hi = (b >> 4) & 0xF;
            if (hi > 9) {
                dst[0] = (char)(hi + 0x57);
            } else {
                dst[0] = (char)(hi + 0x30);
            }
            unsigned int lo = b & 0xF;
            if (lo > 9) {
                dst[1] = (char)(lo + 0x57);
            } else {
                dst[1] = (char)(lo + 0x30);
            }
            dst += 2;
            src += 1;
        }
        *dst = 0;
        screenExit.AddPair("padlog", DataNode(hex));
        mPadLogWritePtr = mPadLogBuffer;
        mPadLogCount = 0;
    }

    std::vector<BandUser *> users = TheBandUserMgr->GetBandUsers();
    int remoteCount = 0;
    for (std::vector<BandUser *>::iterator it = users.begin(); it != users.end(); ++it) {
        BandUser *user = *it;
        int participating = user->IsParticipating();
        if (user->IsLocal()) {
            int padNum = user->GetLocalBandUser()->GetPadNum();
            const char *breed = JoypadGetBreedString(padNum);
            if (mLastBreedString[padNum] != breed) {
                screenExit.AddPair(
                    MakeString("pad_%d", padNum), DataNode(breed)
                );
                padUser.AddPair(
                    MakeString("pad_%d", padNum), DataNode(breed)
                );
                mLastBreedString[padNum] = breed;
            }

            OnlineID id;
            if (participating) {
                ThePlatformMgr.GetOnlineID(padNum, &id);
            }
            if ((unsigned int)participating != mLastWasParticipating[padNum]
                || !(id == mLastPadID[padNum])) {
                const char *key = MakeString("local_user_%d", padNum);
                screenExit.AddPair(key, DataNode(participating ? id.ToString() : "null"));
                padUser.AddPair(key, DataNode(participating ? id.ToString() : "null"));
                mLastWasParticipating[padNum] = (unsigned char)participating;
                mLastPadID[padNum] = id;
            }
        } else {
            MILO_ASSERT(remoteCount < DIM(mLastRemoteID), 0xEC);
            user->Reset();
            int controllerType = kControllerNone;
            OnlineID id;
            if (participating) {
                controllerType = user->GetControllerType();
                id = *user->mOnlineID;
            }
            if (controllerType != mLastControllerType[remoteCount]
                || !(id == mLastRemoteID[remoteCount])) {
                String key(MakeString("remote_user_%d", remoteCount));
                Symbol ctySym = ControllerTypeToSym((ControllerType)controllerType);
                String val(MakeString("%s:%s", ctySym, id.ToString()));
                screenExit.AddPair(key.c_str(), DataNode(val));
                padUser.AddPair(key.c_str(), DataNode(val));
                mLastRemoteID[remoteCount] = id;
                mLastControllerType[remoteCount] = controllerType;
            }
            remoteCount++;
        }
    }

    static Message msg("exit_stats", DataNode(new DataArray(0), kDataArray));
    from->HandleType(msg.mData);
    DataArray *rslt = msg[0].Array(NULL);
    MILO_ASSERT((rslt->Size() % 2) == 0, 0x10D);
    int pairs = rslt->Size() / 2;
    for (int i = 0; i < pairs; i++) {
        screenExit.AddPair(rslt->Node(i * 2).Str(rslt), rslt->Node(i * 2 + 1));
    }
    rslt->Resize(0);

    if (padUser.mNameValPairs.size() > 1) {
        if (mLastDroppedScreen) {
            padUser.AddPair("dropped_screens", DataNode(mLastDroppedScreen));
        }
        TheDataPointMgr.RecordDataPoint(padUser);
    }
    if (screenExit.mNameValPairs.size() == 1) {
        DropScreen(from);
    } else {
        if (mLastDroppedScreen) {
            screenExit.AddPair("dropped_screens", DataNode(mLastDroppedScreen));
            mLastDroppedScreen = NULL;
        }
        TheDataPointMgr.RecordDataPoint(screenExit);
    }
}

void UIStats::EventLog(unsigned int pad, unsigned int but, unsigned int state) {
    MILO_ASSERT(but < 32, 0x139);
    MILO_ASSERT(pad < 8, 0x13B);
    MILO_ASSERT(state < 2, 0x13D);
    int now = SystemMs();
    int &_ref0 = mLastPublishTime;
    unsigned int elapsed = (unsigned int)(now - _ref0) >> 4;
    if (elapsed > 0x7FFFFF) elapsed = 0x7FFFFF;
    unsigned int packed = (state << 31) | ((pad << 28) & 0x70000000) | ((but << 23) & 0x0F800000) | elapsed;
    *(unsigned int *)mPadLogWritePtr = (unsigned short)packed;
    mPadLogWritePtr = (char *)mPadLogWritePtr + 4;
    int count = mPadLogCount + 1;
    mPadLogCount = count;
    if ((mPadLogCount & 0x3FFF) == 0) {
        mPadLogWritePtr = mPadLogBuffer;
    }
    _ref0 = now;
}

DataNode UIStats::OnMsg(const ButtonDownMsg &msg) {
    EventLog(msg.GetPadNum(), msg.GetButton(), 0);
    return DataNode(kDataUnhandled, 0);
}

DataNode UIStats::OnMsg(const ButtonUpMsg &msg) {
    EventLog(msg.GetPadNum(), msg.GetButton(), 1);
    return DataNode(kDataUnhandled, 0);
}

DataNode UIStats::OnMsg(const JoypadConnectionMsg &msg) {
    MILO_ASSERT(msg.GetUser(), 0x166);
    EventLog(msg.GetUser()->GetPadNum(), 0x18, msg->Int(3) != 0);
    return DataNode(kDataUnhandled, 0);
}

DataNode UIStats::OnMsg(const UIComponentFocusChangeMsg &) {
    return DataNode(kDataUnhandled, 0);
}

DataNode UIStats::OnMsg(const UIScreenChangeMsg &msg) {
    MaybePublish(msg.GetOldScreen());
    return DataNode(kDataUnhandled, 0);
}

BEGIN_HANDLERS(UIStats)
    HANDLE_MESSAGE(ButtonDownMsg)
    HANDLE_MESSAGE(ButtonUpMsg)
    HANDLE_MESSAGE(JoypadConnectionMsg)
    HANDLE_MESSAGE(UIComponentFocusChangeMsg)
    HANDLE_MESSAGE(UIScreenChangeMsg)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x183)
END_HANDLERS
