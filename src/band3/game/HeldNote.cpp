#include "game/HeldNote.h"
#include "game/Scoring.h"
#include "beatmatch/GameGem.h"
#include "beatmatch/TrackType.h"
#include "math/Utl.h"
#include "os/Debug.h"

// Retail RB3 (X360) emits HeldNote.cpp as its own TU at
// [0x826F3B18, 0x826F3E98) -- carved out of VocalPart.cpp's pin by lane CZ-1.
// Retail emits exactly EIGHT out-of-line bodies (plus one shared-tail fragment
// that dtk splits off SetHoldTime); GetGem() and GetAwardedPercent() are NOT
// emitted, so they are deliberately left undefined here.
//
// The definition order below is RETAIL's COMDAT order, which differs from the
// rb3-Wii oracle's source order: retail puts ReleaseSlot BEFORE SetHoldTime.

HeldNote::HeldNote()
    : mGem(0), unk_0x4(-1), mTrackType(kTrackNone), unk_0xc(0.0f), unk_0x10(0),
      unk_0x14(0.0f), unk_0x18(0), unk_0x1c(0), unk_0x20(false) {}

HeldNote::HeldNote(TrackType trackType, int gemID, const GameGem &gem, unsigned int param4)
    : mGem(&gem), unk_0x4(gemID), mTrackType(trackType), unk_0xc(0), unk_0x14(0),
      unk_0x1c(param4), unk_0x20(true) {
    unsigned int ticks = gem.GetDurationTicks();
    int bits = gem.CountBitsInSlotType(param4);
    int slots = gem.NumSlots();
    unk_0x20 = bits == slots;
    int tailPoints = TheScoring->GetTailPoints(trackType, ticks);
    unk_0x10 = tailPoints * bits;
    unk_0x18 = tailPoints * slots;
    if (param4 != gem.mSlots) {
        unk_0x20 = false;
    }
}

unsigned int HeldNote::GetGemSlots() const { return !mGem ? 0 : mGem->GetSlots(); }

// 85.83%: retail masks the bool return (`clrlwi r3, r11, 24`) where MSVC gives us
// an early `beqlr`.  BOOL_MASK / permuter-class; the `bool done = true; if (!=)
// done = false;` restructuring compiles byte-identical to this ternary, so the
// source form is not the lever.  Deferred (permuter banned for lane CZ-1).
bool HeldNote::IsDone() const {
    bool done = unk_0xc == unk_0x10;
    return done;
}

bool HeldNote::HeldCompletely() const { return IsDone() && unk_0x20; }

void HeldNote::ReleaseSlot(int slot) {
    unsigned int mask = 1 << slot;
    if (unk_0x1c & mask) {
        unk_0x1c &= ~mask;
        int bits = GameGem::CountBitsInSlotType(unk_0x1c);
        int tailPts = TheScoring->GetTailPoints(mTrackType, mGem->GetDurationTicks());
        unk_0xc *= ((float)bits / (float)(bits + 1));
        unk_0x10 = tailPts * bits;
        unk_0x20 = false;
    }
}

float HeldNote::SetHoldTime(float time) {
    MILO_ASSERT(mGem, 0x66);
    float f3 = Max<float>(0, time - mGem->GetMs());
    float total = mGem->DurationMs();
    MILO_ASSERT(total > 0, 0x6A);
    float fraction = Min<float>(f3 / total, 1);
    MILO_ASSERT(fraction >= 0 && fraction <= 1, 0x6E);

    float frac = fraction * (float)unk_0x10;
    float f2 = frac - unk_0xc;
    if (f2 > 0) {
        unk_0xc = frac;
        unk_0x14 += f2;
        return f2;
    } else
        return 0;
}

float HeldNote::GetPointFraction() {
    int headPoints = TheScoring->GetHeadPoints(mTrackType);
    float awarded = unk_0x14;
    int pointsPlus = headPoints + unk_0x18;
    if (pointsPlus <= 0)
        return 0;
    else {
        // RETAIL DIVERGES FROM THE rb3-Wii ORACLE HERE.  The Wii dev source is
        //   if (fraction < 0.0f || fraction > 1.0f) { MILO_WARN(...); fraction = 1.0f; }
        // i.e. BOTH out-of-range arms land on 1.0.  Retail's `blt` skips over the
        // 1.0 constant load and reaches `fmr f1, f0` while f0 still holds 0.0, so
        // retail CLAMPS to [0,1] instead.  Retail wins over the oracle.
        float fraction = ((float)headPoints + awarded) / (float)pointsPlus;
        if (fraction < 0.0f)
            fraction = 0.0f;
        else if (fraction > 1.0f)
            fraction = 1.0f;
        return fraction;
    }
}
