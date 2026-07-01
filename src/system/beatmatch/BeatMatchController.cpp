#include "beatmatch/BeatMatchController.h"
#include "beatmatch/HitSink.h"
#include "obj/Data.h"
#include "os/Joypad.h"
#include "os/User.h"

// Minimal port of the worklist BeatMatchController member functions from the
// rb3-Wii decomp (MWCC) to MSVC X360. Only the worklist-pinned functions plus
// the private ButtonToSlot(btn, arr) helper the virtual overload dispatches to
// are ported here — the constructor / NewController factory pull in a large set
// of controller subclass headers (JoypadController, RealGuitarController, ...)
// that do not yet exist in rb3-xenon, and are out of scope for these pins.

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

void BeatMatchController::RegisterHit(HitType ty) const {
    if (mHitSink)
        mHitSink->Hit(ty);
}

void BeatMatchController::RegisterRGStrum(int i) const {
    if (mHitSink)
        mHitSink->RGStrum(i);
}

bool BeatMatchController::IsOurPadNum(int i) const {
    return !mUser->IsLocal() ? false : mUser->GetLocalUser()->GetPadNum() == i;
}
