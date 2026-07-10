#pragma once
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "obj/Object.h"
#include "os/Joypad.h"
#include "os/JoypadMsgs.h"
#include "os/Timer.h"

class ShellInputInterceptor : public Hmx::Object {
public:
    ShellInputInterceptor(BandUserMgr *);
    virtual ~ShellInputInterceptor() {}
    virtual DataNode Handle(DataArray *, bool);

    DataNode OnMsg(const ButtonDownMsg &);
    DataNode OnMsg(const ButtonUpMsg &);
    JoypadAction FilterAction(LocalBandUser *, JoypadAction);
    bool IsDoubleStrum(LocalBandUser *, int);

    // Retail X360 layout (proven by the ctor fn_82594350: mBandUserMgr stored at
    // +0x28, bools at +0x2c/+0x2d, Timer ctor fn_824FE428 called on this+0x30,
    // inlined Timer::Start touches +0x30/+0x54, mLastUpDown zeroed at
    // +0x60..+0x6c) — sizeof = 0x70 (`li r3, 0x70` at the BandUI::Init new-site,
    // 0x82523548). There is NO int between the bools and the Timer on X360: the
    // rb3-Wii header's `int unk24` sat where alignment padding falls on Wii
    // (indistinguishable from padding there), but on X360 it pushed the 8-aligned
    // Timer from 0x30 to 0x38 and sizeof from 0x70 to 0x78 — the sole source of
    // the BandUI::Init raw diff `li 0x70 vs 0x78` (d4afbf0). Timer itself is 0x30
    // on X360 both here and in Game.h; the two waves' evidence never conflicted.
    BandUserMgr *mBandUserMgr; // 0x28
    bool mButtonDownSwitch; // 0x2c
    bool mButtonUpSwitch; // 0x2d
    Timer mTime; // 0x30
    float mLastUpDown[4]; // 0x60
}; // sizeof 0x70