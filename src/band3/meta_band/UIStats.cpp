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
        MemFree(mPadLogBuffer);
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

// Status (W16-RB, 2026-10-06, graded name_check ruler): 2604 B, fuzzy 99.919,
// 53 of 652 instructions charged, every one a frame-slot offset.
// - The two call sites DP-2/W16-EI recorded as alias-gated (idx 246
//   GetBandUsers -> GetContainerName@MemcardXbox, idx 249 vector<BandUser*>
//   copy ctor -> vector<int>) now pass: those fold memberships were installed
//   in symbol_aliases.json after W16-EI. The row is source-collectable.
// - What W16-EI called a "6-register rotation" at idx 263-268 was not a
//   register difference. Reading retail's label strings out of band.exe shows
//   the same assignment on both sides (r22 "%s:%s", r21 "remote_user_%d",
//   r20 "null", r19 "local_user_%d", r18 ThePlatformMgr, r25 "pad_%d"); only the
//   ORDER of the six hoisted lis/addi pairs differed. MSVC emits them in reverse
//   of their first use in source, so the local-user branch has to be written
//   before the remote-user branch (see the loop below).
// - Left: the frame slots. Retail (ascending): static-init DataNode temp 0x78,
//   users / 2nd reset OnlineID temp 0x80, screenExit 0x90, HandleType result
//   0xb0, local id 0xc0, key 0xd0, padUser 0xe0, val / 1st reset OnlineID temp
//   0x100, remote id 0x110. Ours: users 0x78, temp 0x88, local id / 2nd temp
//   0x90, screenExit 0xa0, HandleType 0xc0, remote id / 1st temp 0xd0, val 0xe0,
//   key 0xf0, padUser 0x100.
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
        unsigned char *write = (unsigned char *)mPadLogWritePtr;
        unsigned char *base = (unsigned char *)mPadLogBuffer;
        unsigned int writeOff = (unsigned int)(write - base);
        int size;
        // retail loads mPadLogWritePtr before mPadLogBuffer and compares
        // `cmplw base, write` (W16-RB)
        if (base == write || (unsigned int)mPadLogCount < 0x4000) {
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
        // retail (Ghidra @0x8255f9d0): the condition `IsLocal() && !IsNullUser()`
        // (IsNullUser is a TU5-added User virtual, vtbl+0x70 -- see os/User.h). Its else
        // arm tests IsLocal() a SECOND time via the nested `if (!user->IsLocal())`; a
        // local user with the TU5 flag set falls through both arms and is silently
        // skipped this iteration. The local arm is written FIRST: MSVC hoists the six
        // loop-invariant string/global addresses in reverse of their first use in
        // source, and retail's order (%s:%s, remote_user, null, local_user,
        // ThePlatformMgr, pad) only comes out this way (W16-RB: 99.87 -> 99.92).
        if (user->IsLocal() && !user->IsNullUser()) {
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
        } else {
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
                    // load. A NAMED Symbol local measured
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
        }
    }

    // The DataArray* converts to DataNode implicitly (DataNode(DataArray*,
    // DataType = kDataArray)). Retail passes that temp by its frame address
    // (`addi r5,r31,0x78`); an explicit DataNode(...) temporary is passed as the
    // ctor's returned this (`mr r5,r29`) and costs an extra mr (W16-RB).
    static Message msg("exit_stats", new DataArray(0));
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
        // in the whole function — the padUser one. There is no screenExit
        // recording. Verified by counting
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
