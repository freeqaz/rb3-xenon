#include "hamobj/MiniLeaderboardDisplay.h"
#include "MiniLeaderboardDisplay.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "ui/LabelShrinkWrapper.h"
#include "ui/UI.h"
#include "ui/UIComponent.h"
#include "ui/UIResource.h" // lane NCCC-0731-ab7e/f8/sonnet: for UIResource::Dir(); UIComponent.h only fwd-declares it

// lane NCCC-0731-ab7e/f8/sonnet: mAllowSoloScores defaults true
// (`MiniLeaderboardDisplay() : mAllowSoloScores(true) {}`).
MiniLeaderboardDisplay::MiniLeaderboardDisplay() : mAllowSoloScores(true) {}
MiniLeaderboardDisplay::~MiniLeaderboardDisplay() {}

BEGIN_HANDLERS(MiniLeaderboardDisplay)
    HANDLE_SUPERCLASS(UIComponent)
END_HANDLERS

BEGIN_PROPSYNCS(MiniLeaderboardDisplay)
    SYNC_PROP_MODIFY(allow_solo_scores, mAllowSoloScores, Update())
    SYNC_SUPERCLASS(UIComponent)
END_PROPSYNCS

BEGIN_SAVES(MiniLeaderboardDisplay)
    SAVE_REVS(1, 0)
    bs << mAllowSoloScores;
    SAVE_SUPERCLASS(UIComponent)
END_SAVES

BEGIN_COPYS(MiniLeaderboardDisplay)
    CREATE_COPY_AS(MiniLeaderboardDisplay, p)
    MILO_ASSERT(p, 0x21);
    COPY_SUPERCLASS_FROM(UIComponent, p)
    COPY_MEMBER_FROM(p, mAllowSoloScores)
END_COPYS

BEGIN_LOADS(MiniLeaderboardDisplay)
    PreLoad(bs);
    PostLoad(bs);
END_LOADS

// RB3 retail keeps no BinStreamRev here: the packed rev is split into two TU
// shorts (alt at +0, rev at +4), there is no version guard, and the bool is
// read only from rev 1 on -- the same dialect as ui/LabelShrinkWrapper.cpp.
#pragma push_macro("INIT_REVS")
#pragma push_macro("LOAD_REVS")
#pragma push_macro("ASSERT_REVS")
#undef INIT_REVS
#undef LOAD_REVS
#undef ASSERT_REVS
// Separate statics are not co-addressed here (measured: two relocs), so they
// live in one aligned(4) aggregate, as in ui/LabelNumberTicker.cpp.
#define INIT_REVS(r_, a_)                                                                \
    static struct {                                                                      \
        __declspec(align(4)) unsigned short altRev;                                      \
        __declspec(align(4)) unsigned short rev;                                         \
    } gRevs_MiniLeaderboardDisplay = { a_, r_ };
#define LOAD_REVS(bs)                                                                    \
    int rev;                                                                             \
    bs >> rev;                                                                           \
    gRevs_MiniLeaderboardDisplay.rev = getHmxRev(rev);                                   \
    gRevs_MiniLeaderboardDisplay.altRev = getAltRev(rev);
#define ASSERT_REVS(rev1, rev2)

INIT_REVS(1, 0)

void MiniLeaderboardDisplay::PreLoad(BinStream &bs) {
    LOAD_REVS(bs)
    ASSERT_REVS(1, 0)
    if (gRevs_MiniLeaderboardDisplay.rev >= 1)
        bs >> mAllowSoloScores;
    UIComponent::PreLoad(bs);
}

#pragma pop_macro("ASSERT_REVS")
#pragma pop_macro("LOAD_REVS")
#pragma pop_macro("INIT_REVS")

void MiniLeaderboardDisplay::PostLoad(BinStream &bs) {
    UIComponent::PostLoad(bs);
    Update();
}

void MiniLeaderboardDisplay::DrawShowing() {
    RndDir *dir = mResource->Dir();
    if (dir) {
        dir->SetWorldXfm(WorldXfm());
        dir->Draw();
    } else {
        MILO_NOTIFY_ONCE("MiniLeaderboardDisplay: %s missing resource dir", Name());
    }
}

void MiniLeaderboardDisplay::OldResourcePreload(BinStream &bs) {
    char name[256];
    bs.ReadString(name, 256);
    // NOTE(lane NCCC-0731-ab7e/f8/sonnet): retail RB3 has no MiniLeaderboardDisplay
    // ::mResourceDir to name here (and no OldResourcePreload of its own -- it is a
    // DC3 addition). The read is kept so the stream position stays correct for
    // whatever follows. Same treatment lane BQ-2 gave MeterDisplay::OldResourcePreload.
}

void LabelShrinkWrapper::OldResourcePreload(BinStream &bs) {
    char name[256];
    bs.ReadString(name, 256);
    // NOTE(laneBS1): retail RB3 has no LabelShrinkWrapper::mResourceDir to name here
    // (see the note in ui/LabelShrinkWrapper.h) -- and RB3 has no
    // OldResourcePreload for this class at all, so it is a DC3 addition. The read is
    // kept so the stream position stays correct for whatever follows. Same treatment
    // lane BQ-2 gave MeterDisplay::OldResourcePreload.
}

void MiniLeaderboardDisplay::Init() {
    REGISTER_OBJ_FACTORY(MiniLeaderboardDisplay);
    TheUI->InitResources("MiniLeaderboardDisplay");
}

void MiniLeaderboardDisplay::Update() {}
