#include "world/World.h"
#include "BeatClock.h"
#include "CameraManager.h"
#include "Crowd.h"
#include "Dir.h"
#include "PhysicsVolume.h"
#include "SpotlightDrawer.h"
#include "obj/Object.h"
#include "world/ColorPalette.h"
#include "world/Crowd3DCharHandle.h"
#include "world/EventAnim.h"
#include "world/Instance.h"
#include "world/LightHue.h"
#include "world/LightPreset.h"
#include "world/PostProcer.h"
#include "world/Reflection.h"
#include "world/Spotlight.h"
#include "world/SpotlightEnder.h"
#include "world/SpotlightDrawer_NG.h"

void WorldInit() {
    WorldDir::Init();
    // ⚠ Retail's WorldInit registers exactly eight classes, in this order:
    // ColorPalette, EventAnim, WorldCrowd, WorldReflection, LightPreset,
    // LightHue, SpotlightEnder, WorldInstance.  Read WITHOUT the symbol map:
    // each slot's StaticClassName callee was decoded out of
    // orig/45410914/band.exe and its .rdata literal read (fn_824AABA8, 8/8
    // slots resolved; tools/reglist_rdata_adjudicate.py).  A whole-binary scan
    // of every call to RegisterFactory confirms BeatClock,
    // WorldCrowd3DCharHandle, PhysicsVolume and PostProcer are registered
    // NOWHERE in retail, i.e. RB3 never shipped them as DTA-creatable types --
    // they are our inherited (newer DC3) engine.  Corroborated arithmetically:
    // our body was 336 B and retail's is 264 B, exactly 3 registrations x 24 B.
    // Kept under HX_NATIVE rather than deleted: the native runtime is the goal.
#ifdef HX_NATIVE
    REGISTER_OBJ_FACTORY(BeatClock)
#endif
    REGISTER_OBJ_FACTORY(ColorPalette)
    EventAnim::Init();
    REGISTER_OBJ_FACTORY(WorldCrowd)
#ifdef HX_NATIVE
    REGISTER_OBJ_FACTORY(WorldCrowd3DCharHandle)
#endif
    CamShot::Init();
    REGISTER_OBJ_FACTORY(WorldReflection)
    Spotlight::Init();
    REGISTER_OBJ_FACTORY(LightPreset)
    REGISTER_OBJ_FACTORY(LightHue)
    SpotlightDrawer::Init();
    REGISTER_OBJ_FACTORY(SpotlightEnder)
    REGISTER_OBJ_FACTORY(WorldInstance)
#ifdef HX_NATIVE
    REGISTER_OBJ_FACTORY(PhysicsVolume)
    REGISTER_OBJ_FACTORY(PostProcer)
#endif
    NgSpotlightDrawer::Init();
    PreloadSharedSubdirs("world");
}
