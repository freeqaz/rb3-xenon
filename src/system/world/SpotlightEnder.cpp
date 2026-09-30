#include "world/SpotlightEnder.h"
#include "SpotlightDrawer.h"
#include "SpotlightEnder.h"
#include "obj/Object.h"
#include "rndobj/Draw.h"

SpotlightEnder::SpotlightEnder() { mOrder = -900; }

BEGIN_HANDLERS(SpotlightEnder)
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(SpotlightEnder)
    SYNC_SUPERCLASS(RndDrawable)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(SpotlightEnder)
    SAVE_REVS(0, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndDrawable)
END_SAVES

BEGIN_COPYS(SpotlightEnder)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndDrawable)
END_COPYS

// Retail (fn_824E9D20) keeps the revision in two static shorts and hands the
// raw stream to both superclass loads -- no BinStreamRev (UIColor.cpp pattern).
#pragma push_macro("INIT_REVS")
#pragma push_macro("LOAD_REVS")
#pragma push_macro("ASSERT_REVS")
#undef INIT_REVS
#undef LOAD_REVS
#undef ASSERT_REVS
#define INIT_REVS(rev, alt)                                                              \
    static unsigned short gAltRev = alt;                                                 \
    static unsigned short gRev = rev;
#define LOAD_REVS(bs)                                                                    \
    int rev;                                                                             \
    bs >> rev;                                                                           \
    gRev = getHmxRev(rev);                                                               \
    gAltRev = getAltRev(rev);
#define ASSERT_REVS(rev1, rev2)

INIT_REVS(0, 0)

BEGIN_LOADS(SpotlightEnder)
    LOAD_REVS(bs);
    ASSERT_REVS(0, 0);
    Hmx::Object::Load(bs);
    RndDrawable::Load(bs);
END_LOADS

#pragma pop_macro("ASSERT_REVS")
#pragma pop_macro("LOAD_REVS")
#pragma pop_macro("INIT_REVS")

void SpotlightEnder::DrawShowing() { SpotlightDrawer::Current()->UpdateBoxMap(); }
