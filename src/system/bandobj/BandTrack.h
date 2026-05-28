#pragma once
// Minimal stub for BandTrack — referenced by game/Player.h as a pointer type.
// Full class body deferred until BandTrack.cpp is ported (depends on many
// bandobj engine components: OverdriveMeter, StreakMeter, UnisonIcon, etc.).
// See docs/plans/bandobj-port.md for full port plan.
#include "obj/Object.h"

class BandTrack : public virtual Hmx::Object {
public:
    BandTrack(Hmx::Object *) {}
    virtual ~BandTrack() {}
};
