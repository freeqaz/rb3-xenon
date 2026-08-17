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
    void *mem = MemAlloc(0x10000, __FILE__, 0x1F, "UIStats", 0);
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

// NOTE (lane DP-2): the ~61 residual "offset diffs" here are STACK SLOT
// assignments plus a register rotation -- NOT a class-layout defect. Retail's
// prologue is `lis r12,1 / ori r12,r12,0x2f0 / subf r31, r12, r1` (0x8255F9E0)
// => r31 is the FRAME BASE of a 0x102f0-byte (66,288 B) frame, not `this`.
// Measured: 4 user slots DIFFER and 8 are SHIFTED (deltas +0x40/+0x44/+0xc/
// +0x18/+0x20/-0xc) out of 32; on top of that objdiff reports a 6-register
// rotation (r22->r20->r18->r22, r21->r19->r25->r21) and an r29<->r4 swap.
// ==> permuter class, and the permuter is off by directive.
// CAUTION on the instrument: `run_diff_inspect mode=stack-layout` prints
// "Frame size TGT 0x0 BASE 0x0 -> Frame sizes match" for this function with
// "Callee-saved GPRs: TGT 0 BASE 18". It FAILED TO PARSE the target prologue
// (this `lis/ori/subf` form), so that "match" is vacuous 0==0. The slot table
// below it is still meaningful; the frame-size line is not.
void UIStats::MaybePublish(UIScreen *from) {
    if (!from) return;
    mLastPublishTime = SystemMs();
    MILO_ASSERT(from, 0x48);

    bool &_ref0 = mPublishingPad;
    static Symbol gather_uistats("gather_uistats"); // retail: function-local (guard bit 1)
    Server *server = TheNet.mServer;
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
        Symbol empty("");
        mLastMode = empty;
        for (int i = 0; i < 4; i++) {
            mLastWasParticipating[i] = 0;
            mLastPadID[i] = OnlineID();
            mLastBreedString[i] = "00";
            mLastRemoteID[i] = OnlineID();
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

    if ((unsigned int)mPadLogCount != 0) {
        int compressedSize = 0x10100;
        unsigned char stackbuf[0x10100];
        unsigned char *buf = stackbuf + 0x100;
        unsigned char *base = (unsigned char *)mPadLogBuffer;
        unsigned char *write = (unsigned char *)mPadLogWritePtr;
        unsigned int writeOff = (unsigned int)(write - base);
        int size;
        if (write == base || (unsigned int)mPadLogCount < 0x4000) {
            if ((unsigned int)mPadLogCount < 0x4000) size = writeOff;
            else size = 0x10000;
            memcpy(buf, base, size);
        } else {
            size = 0x10000;
            int tail = size - writeOff;
            memcpy(buf + tail, base, writeOff);
            memcpy(buf, write, tail);
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
        char *dst = (char *)hex.c_str() + prefixLen;
        for (int i = 0; i < compressedSize; i++) {
            unsigned char b = stackbuf[i];
            unsigned int hi = (b >> 4) & 0xF;
            if (hi > 9) {
                *dst = (char)(hi + 0x57);
            } else {
                *dst = (char)(hi + 0x30);
            }
            dst++;
            unsigned int lo = b & 0xF;
            if (lo > 9) {
                *dst = (char)(lo + 0x57);
            } else {
                *dst = (char)(lo + 0x30);
            }
            dst++;
        }
        *dst = 0;
        screenExit.AddPair("padlog", DataNode(hex));
        mPadLogWritePtr = mPadLogBuffer;
        mPadLogCount = 0;
    }

    std::vector<BandUser *> users = TheBandUserMgr->GetBandUsers();
    int remoteCount = 0;
    for (std::vector<BandUser *>::iterator it = users.begin(); it != users.end(); ++it) {
        const BandUser *user = *it; // retail: const (selects the const GetLocal/RemoteBandUser vtable slots)
        bool participating = user->IsParticipating();
        // retail (Ghidra @0x8255f9d0): merged condition `!IsLocal() || IsNullUser()`
        // (TU5-added User virtual, vtbl+0x70 — see os/User.h) short-circuits IsLocal() a SECOND time
        // via the nested `if (!user->IsLocal())`; a local user with the TU5 flag set falls through
        // both branches and is silently skipped this iteration.
        if (!user->IsLocal() || user->IsNullUser()) {
            if (!user->IsLocal()) {
                MILO_ASSERT(remoteCount < DIM(mLastRemoteID), 0xEC);
                user->GetRemoteBandUser(); // retail calls this (result unused), not Reset()
                int controllerType = kControllerNone;
                OnlineID id;
                if (participating) {
                    controllerType = user->GetControllerType();
                    id = *user->mOnlineID;
                }
                if (controllerType != mLastControllerType[remoteCount]
                    || !(id == mLastRemoteID[remoteCount])) {
                    String key(MakeString("remote_user_%d", remoteCount));
                    // retail keeps the ControllerTypeToSym sret pointer in r30 and reads the
                    // Symbol via `lwz r4, 0x0(r30)` AFTER the ToString call — i.e. the 1-word
                    // Symbol is loaded as part of the vararg setup, not as a separate
                    // expression. `.Str()` forces the load EARLY (`lwz r30,0(r11)` then
                    // `mr r4,r30`). Passing the UNNAMED temporary BY VALUE reproduces the late
                    // load. A NAMED Symbol local (rb3-Wii's shape, UIStats.cpp:207) measured
                    // WORSE (99.1 vs 99.2) — it homes the Symbol to r31 and costs 0x10 of
                    // frame — but the unnamed-temporary form is a different shape.
                    String val(MakeString(
                        "%s:%s",
                        ControllerTypeToSym((ControllerType)controllerType),
                        id.ToString()
                    ));
                    screenExit.AddPair(key.c_str(), DataNode(val));
                    padUser.AddPair(key.c_str(), DataNode(val));
                    mLastRemoteID[remoteCount] = id;
                    // retail X360: mLastControllerType[remoteCount] is never written back here
                }
                remoteCount++;
            }
        } else {
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
            if (participating != mLastWasParticipating[padNum]
                || !(id == mLastPadID[padNum])) {
                const char *key = MakeString("local_user_%d", padNum);
                screenExit.AddPair(key, DataNode(participating ? id.ToString() : "null"));
                padUser.AddPair(key, DataNode(participating ? id.ToString() : "null"));
                mLastWasParticipating[padNum] = participating;
                mLastPadID[padNum] = id;
            }
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
    } else if (mLastDroppedScreen) {
        // retail X360 (fn_8255F9D0 @.L_8256035C) has exactly ONE `bl RecordDataPoint`
        // in the whole function — the padUser one. rb3-Wii's DEV source records
        // screenExit too (UIStats.cpp:241); retail dropped it. Verified by counting
        // bl fn_827CD110 sites in the target .s: 1.
        screenExit.AddPair("dropped_screens", DataNode(mLastDroppedScreen));
        mLastDroppedScreen = NULL;
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
    // retail passes the BOOL accessor, not the raw `!= 0` comparison: the target
    // masks the argument (`clrlwi r6,rX,24`) before the call, which MSVC only emits
    // when the value has passed through a bool-typed result (here the inlined
    // `Connected()` return) rather than a comparison rvalue it knows is 0/1.
    EventLog(msg.GetUser()->GetPadNum(), 0x18, msg.Connected());
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
