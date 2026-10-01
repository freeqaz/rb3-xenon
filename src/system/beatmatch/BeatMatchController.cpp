#include "beatmatch/BeatMatchController.h"
#include "beatmatch/BeatMatchControllerSink.h"
#include "beatmatch/ButtonGuitarController.h"
#include "beatmatch/GuitarController.h"
#include "beatmatch/HitSink.h"
#include "beatmatch/JoypadController.h"
#include "beatmatch/JoypadGuitarController.h"
#include "beatmatch/JoypadMidiController.h"
#include "beatmatch/KeyboardController.h"
#include "beatmatch/RealGuitarController.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "os/Joypad.h"
#include "os/System.h"
#include "os/User.h"

// Retail BeatMatchController.cpp starts at 0x8278F9A8 (the ctor); vtable
// 0x8210F0D4.

// 0x8278F9A8
BeatMatchController::BeatMatchController(User *user, const DataArray *cfg, bool lefty)
    : mUser(user), mForceMercuryBut(-1), mLefty(lefty), unk25(0),
      mGemMapping(kDefaultGemMapping), mHitSink(0) {
    mSlots = cfg->FindArray("slots");
    mLeftySlots = cfg->FindArray("lefty_slots", false);
    mRightySlots = cfg->FindArray("righty_slots", false);
    cfg->FindData("force_mercury", mForceMercuryBut, false);
}

// 0x8278FAE0: HitSink slot 2 (Key).
void BeatMatchController::RegisterKey(int key) const {
    if (mHitSink)
        mHitSink->Key(key);
}

// 0x8278FBC0
BeatMatchController *NewController(
    User *user,
    const DataArray *cfg,
    BeatMatchControllerSink *sink,
    bool disabled,
    bool lefty,
    TrackType ty
) {
    DataArray *ctrl_cfg =
        SystemConfig("beatmatcher", "controllers", "beatmatch_controller_mapping");
    Symbol instr = ctrl_cfg->FindSym(cfg->Sym(0));
    BeatMatchController *controller = 0;
    if (instr == "guitar") {
        controller = new GuitarController(user, cfg, sink, disabled, lefty);
    } else if (instr == "joypad") {
        controller = new JoypadController(user, cfg, sink, disabled, lefty);
    } else if (instr == "joypad_guitar") {
        controller = new JoypadGuitarController(user, cfg, sink, disabled, lefty);
    } else if (instr == "real_guitar") {
        if (ty == kTrackGuitar || ty == kTrackBass) {
            controller = new ButtonGuitarController(user, cfg, sink, disabled, lefty);
        } else
            controller = new RealGuitarController(user, cfg, sink, disabled, lefty);
    } else if (instr == "keys") {
        if (ty == kTrackRealKeys) {
            controller = new KeyboardController(user, cfg, sink, disabled);
        } else
            controller = new JoypadMidiController(user, cfg, sink, disabled);
    } else
        MILO_FAIL("NewController: Bad controller type %s, %s", cfg->Sym(0), instr.Str());
    sink->SetController(controller);
    return controller;
}

int BeatMatchController::ButtonToSlot(JoypadButton btn, const DataArray *arr) const {
    int thresh = (arr->Size() - 1) / 2;
    for (int i = 0; i < thresh; i++) {
        if (btn == arr->Int(i * 2 + 1))
            return arr->Int(i * 2 + 2);
    }
    return -1;
}

int BeatMatchController::ButtonToSlot(JoypadButton btn) const {
    DataArray *cfg;
    int slot = ButtonToSlot(btn, mSlots);
    if (slot == -1) {
        cfg = mLefty ? mLeftySlots : mRightySlots;
        if (cfg)
            return ButtonToSlot(btn, cfg);
    }
    return slot;
}

// Retail 0x827900C8 (136 B).
int BeatMatchController::SlotToButton(int slot) const {
    int thresh = (mSlots->Size() - 1) / 2;
    for (int i = 0; i < thresh; i++) {
        if (slot == mSlots->Int(i * 2 + 2))
            return mSlots->Int(i * 2 + 1);
    }
    return 0x18;
}

void BeatMatchController::RegisterHit(HitType ty) const {
    if (mHitSink)
        mHitSink->Hit(ty);
}

void BeatMatchController::RegisterRGStrum(int i) const {
    if (mHitSink)
        mHitSink->RGStrum(i);
}

bool BeatMatchController::IsOurPadNum(int i) const {
    if (!mUser->IsLocal())
        return false;
    return mUser->GetLocalUser()->GetPadNum() == i;
}
