// milo_object_factories.cpp -- register every Hmx::Object factory the xenon
// fork can currently supply, so DirLoader can CONSTRUCT a .milo_xbox's object
// graph rather than skip it.
//
// WHY A HAND-ROLLED LIST AND NOT Rnd::Init()
// ------------------------------------------
// rndobj/Rnd.cpp:302-386 registers all of these -- but Rnd::Init() is a
// RENDERER bring-up: it reads SystemConfig ("rnd" section), calls SetupFont,
// RndOverlay::Find x5, `new RndConsole()`, CreateDefaults() and
// InitParticleSystem(), and it is a method on Rnd, of which X2 has no concrete
// instance (NgRnd/WgpuRnd is X3's, and instantiating one is exactly the
// member-layout risk this milestone is designed to avoid -- see
// rndobj/Rnd.h:354-360). REGISTER_OBJ_FACTORY is just
// Hmx::Object::RegisterFactory(StaticClassName(), NewObject)
// (obj/ObjMacros.h:680) -- a name -> allocator map entry with no singleton, no
// GPU and no config dependency. Same split rb3-Wii's native harness makes in
// native/src/main_native.cpp:134 RegisterCommonFactories().
//
// WHY IT MATTERS THAT THE LIST IS COMPLETE (not just "big enough")
// ---------------------------------------------------------------
// An unregistered LEAF class is recoverable: DirLoader::CreateObjects logs
// "Can't make <Class>" and LoadObjs ReadDead-skips its bytes to the next
// 0xADDEADDE marker (obj/DirLoader.cpp:927, :813). An unregistered *Dir
// SUBCLASS is not: it serializes a whole nested directory whose inner objects
// carry their OWN dead markers, so ReadDead stops at the first inner one and
// leaves the rest in the stream. The parent desyncs and the next PreLoad reads
// a string length as a vector count -> runaway resize -> SIGSEGV. (This exact
// failure was diagnosed on the rb3-Wii side; see
// rb3/native/src/rb3_game_object_factories.cpp.) main_milo.cpp therefore
// reports every "Can't make" class it sees, and treats a *Dir among them as a
// hard error rather than a footnote.
//
// GENERATED, THEN REVIEWED: the list is every `NEW_OBJ(X)` in
// src/system/{rndobj,char,world}/*.h, minus the classes whose TU is on
// _MILO_FORK_EXCLUDE in native/CMakeLists.txt (RndCubeTex, RndFont, RndFont3d,
// RndMeshAnim, RndMeshDeform, BeatClock, WorldCrowd) -- registering those would
// be an undefined reference, not a feature.

#include "obj/Dir.h"
#include "obj/Object.h"
#include "obj/ObjMacros.h"

#include "rndobj/AmbientOcclusion.h"
#include "rndobj/Anim.h"
#include "rndobj/AnimFilter.h"
#include "rndobj/BaseMaterial.h"
#include "rndobj/Cam.h"
#include "rndobj/CamAnim.h"
#include "rndobj/DOFProc.h"
#include "rndobj/DOFProc_NG.h"
#include "rndobj/Dir.h"
#include "rndobj/Draw.h"
#include "rndobj/Enter.h"
#include "rndobj/Env.h"
#include "rndobj/EnvAnim.h"
#include "rndobj/Env_NG.h"
#include "rndobj/EventTrigger.h"
#include "rndobj/Flare.h"
#include "rndobj/FontBase.h"
#include "rndobj/Fur.h"
#include "rndobj/Fur_NG.h"
#include "rndobj/Gen.h"
#include "rndobj/Group.h"
#include "rndobj/Line.h"
#include "rndobj/Lit.h"
#include "rndobj/LitAnim.h"
#include "rndobj/Lit_NG.h"
#include "rndobj/Mat.h"
#include "rndobj/MatAnim.h"
#include "rndobj/Mesh.h"
#include "rndobj/MetaMaterial.h"
#include "rndobj/Morph.h"
#include "rndobj/MotionBlur.h"
#include "rndobj/Movie.h"
#include "rndobj/MultiMesh.h"
#include "rndobj/MultiMeshProxy.h"
#include "rndobj/Part.h"
#include "rndobj/PartAnim.h"
#include "rndobj/PartLauncher.h"
#include "rndobj/PollAnim.h"
#include "rndobj/PostProc.h"
#include "rndobj/PostProcMgr.h"
#include "rndobj/PostProc_NG.h"
#include "rndobj/PropAnim.h"
#include "rndobj/Ribbon.h"
#include "rndobj/ScreenMask.h"
#include "rndobj/Set.h"
#include "rndobj/Shockwave.h"
#include "rndobj/SoftParticleBuffer.h"
#include "rndobj/SoftParticles.h"
#include "rndobj/Spline.h"
#include "rndobj/Tex.h"
#include "rndobj/TexBlendController.h"
#include "rndobj/TexBlender.h"
#include "rndobj/TexProc.h"
#include "rndobj/TexRenderer.h"
#include "rndobj/Text.h"
#include "rndobj/Trans.h"
#include "rndobj/TransAnim.h"
#include "rndobj/TransProxy.h"
#include "rndobj/Wind.h"
#include "char/CharBlendBone.h"
#include "char/CharBone.h"
#include "char/CharBoneDir.h"
#include "char/CharBoneOffset.h"
#include "char/CharBoneTwist.h"
#include "char/CharBones.h"
#include "char/CharBonesBlender.h"
#include "char/CharClip.h"
#include "char/CharClipGroup.h"
#include "char/CharClipSet.h"
#include "char/CharCollide.h"
#include "char/CharCuff.h"
#include "char/CharDriver.h"
#include "char/CharDriverMidi.h"
#include "char/CharEyeDartRuleset.h"
#include "char/CharEyes.h"
#include "char/CharFaceServo.h"
#include "char/CharForeTwist.h"
#include "char/CharGuitarString.h"
#include "char/CharHair.h"
#include "char/CharIKFingers.h"
#include "char/CharIKFoot.h"
#include "char/CharIKHand.h"
#include "char/CharIKHead.h"
#include "char/CharIKMidi.h"
#include "char/CharIKRod.h"
#include "char/CharIKScale.h"
#include "char/CharIKSliderMidi.h"
#include "char/CharInterest.h"
#include "char/CharLipSync.h"
#include "char/CharLipSyncDriver.h"
#include "char/CharLookAt.h"
#include "char/CharMeshHide.h"
#include "char/CharMirror.h"
#include "char/CharNeckTwist.h"
#include "char/CharPollGroup.h"
#include "char/CharPosConstraint.h"
#include "char/CharServoBone.h"
#include "char/CharSignalApplier.h"
#include "char/CharSleeve.h"
#include "char/CharTransDraw.h"
#include "char/CharUpperTwist.h"
#include "char/CharWeightSetter.h"
#include "char/CharWeightable.h"
#include "char/Character.h"
#include "char/ClipCollide.h"
#include "char/ClipGraphGen.h"
#include "char/FileMerger.h"
#include "char/FileMergerOrganizer.h"
#include "char/Waypoint.h"
#include "world/CameraShot.h"
#include "world/ColorPalette.h"
#include "world/Crowd.h"
#include "world/Crowd3DCharHandle.h"
#include "world/Dir.h"
#include "world/EventAnim.h"
#include "world/Instance.h"
#include "world/LightHue.h"
#include "world/LightPreset.h"
#include "world/PhysicsVolume.h"
#include "world/PostProcer.h"
#include "world/Reflection.h"
#include "world/Spotlight.h"
#include "world/SpotlightDrawer.h"
#include "world/SpotlightDrawer_NG.h"
#include "world/SpotlightEnder.h"
#include "ui/UIColor.h"

// ---- X7: the band-slot placement pair -----------------------------------
// Both of these are LEAF classes (BandWardrobe is `public virtual Hmx::Object`,
// BandConfiguration is `public Hmx::Object`), NOT ObjectDir subclasses, so
// neither carries the nested-dir desync hazard the header note above warns
// about for an unregistered *Dir.
#include "bandobj/BandCharDesc.h"
#include "bandobj/BandCharacter.h"
#include "bandobj/BandConfiguration.h"
#include "bandobj/BandWardrobe.h"

void RegisterMiloObjectFactories() {
    // The two roots. ObjectDir is what a milo whose dir class has no factory
    // falls back to (DirLoader::LoadHeader prefers RndDir, obj/DirLoader.cpp:996).
    REGISTER_OBJ_FACTORY(Hmx::Object)
    REGISTER_OBJ_FACTORY(ObjectDir)

    // --- rndobj/ -- draw/anim/material/texture scene classes (59 classes)
    REGISTER_OBJ_FACTORY(RndAmbientOcclusion)
    REGISTER_OBJ_FACTORY(RndAnimatable)
    REGISTER_OBJ_FACTORY(RndAnimFilter)
    REGISTER_OBJ_FACTORY(BaseMaterial)
    REGISTER_OBJ_FACTORY(RndCam)
    REGISTER_OBJ_FACTORY(RndCamAnim)
    REGISTER_OBJ_FACTORY(DOFProc)
    REGISTER_OBJ_FACTORY(NgDOFProc)
    REGISTER_OBJ_FACTORY(RndDir)
    REGISTER_OBJ_FACTORY(RndDrawable)
    REGISTER_OBJ_FACTORY(RndEnterable)
    REGISTER_OBJ_FACTORY(RndEnviron)
    REGISTER_OBJ_FACTORY(RndEnvAnim)
    REGISTER_OBJ_FACTORY(NgEnviron)
    REGISTER_OBJ_FACTORY(EventTrigger)
    REGISTER_OBJ_FACTORY(RndFlare)
    REGISTER_OBJ_FACTORY(RndFontBase)
    REGISTER_OBJ_FACTORY(RndFur)
    REGISTER_OBJ_FACTORY(NgFur)
    REGISTER_OBJ_FACTORY(RndGenerator)
    REGISTER_OBJ_FACTORY(RndGroup)
    REGISTER_OBJ_FACTORY(RndLine)
    REGISTER_OBJ_FACTORY(RndLight)
    REGISTER_OBJ_FACTORY(RndLightAnim)
    REGISTER_OBJ_FACTORY(NgLight)
    REGISTER_OBJ_FACTORY(RndMat)
    REGISTER_OBJ_FACTORY(RndMatAnim)
    REGISTER_OBJ_FACTORY(RndMesh)
    REGISTER_OBJ_FACTORY(MetaMaterial)
    REGISTER_OBJ_FACTORY(RndMorph)
    REGISTER_OBJ_FACTORY(RndMotionBlur)
    REGISTER_OBJ_FACTORY(RndMovie)
    REGISTER_OBJ_FACTORY(RndMultiMesh)
    REGISTER_OBJ_FACTORY(RndMultiMeshProxy)
    REGISTER_OBJ_FACTORY(RndParticleSys)
    REGISTER_OBJ_FACTORY(RndParticleSysAnim)
    REGISTER_OBJ_FACTORY(RndPartLauncher)
    REGISTER_OBJ_FACTORY(RndPollAnim)
    REGISTER_OBJ_FACTORY(RndPostProc)
    REGISTER_OBJ_FACTORY(RndPostProcMgr)
    REGISTER_OBJ_FACTORY(NgPostProc)
    REGISTER_OBJ_FACTORY(RndPropAnim)
    REGISTER_OBJ_FACTORY(RndRibbon)
    REGISTER_OBJ_FACTORY(RndScreenMask)
    REGISTER_OBJ_FACTORY(RndSet)
    REGISTER_OBJ_FACTORY(RndShockwave)
    REGISTER_OBJ_FACTORY(RndSoftParticleBuffer)
    REGISTER_OBJ_FACTORY(RndSoftParticles)
    REGISTER_OBJ_FACTORY(RndSpline)
    REGISTER_OBJ_FACTORY(RndTex)
    REGISTER_OBJ_FACTORY(RndTexBlendController)
    REGISTER_OBJ_FACTORY(RndTexBlender)
    REGISTER_OBJ_FACTORY(TexProc)
    REGISTER_OBJ_FACTORY(RndTexRenderer)
    REGISTER_OBJ_FACTORY(RndText)
    REGISTER_OBJ_FACTORY(RndTransformable)
    REGISTER_OBJ_FACTORY(RndTransAnim)
    REGISTER_OBJ_FACTORY(RndTransProxy)
    REGISTER_OBJ_FACTORY(RndWind)

    // --- char/ -- character rig: bones, clips, drivers, IK, lipsync (50 classes)
    REGISTER_OBJ_FACTORY(CharBlendBone)
    REGISTER_OBJ_FACTORY(CharBone)
    REGISTER_OBJ_FACTORY(CharBoneDir)
    REGISTER_OBJ_FACTORY(CharBoneOffset)
    REGISTER_OBJ_FACTORY(CharBoneTwist)
    REGISTER_OBJ_FACTORY(CharBonesObject)
    REGISTER_OBJ_FACTORY(CharBonesBlender)
    REGISTER_OBJ_FACTORY(CharClip)
    REGISTER_OBJ_FACTORY(CharClipGroup)
    REGISTER_OBJ_FACTORY(CharClipSet)
    REGISTER_OBJ_FACTORY(CharCollide)
    REGISTER_OBJ_FACTORY(CharCuff)
    REGISTER_OBJ_FACTORY(CharDriver)
    REGISTER_OBJ_FACTORY(CharDriverMidi)
    REGISTER_OBJ_FACTORY(CharEyeDartRuleset)
    REGISTER_OBJ_FACTORY(CharEyes)
    REGISTER_OBJ_FACTORY(CharFaceServo)
    REGISTER_OBJ_FACTORY(CharForeTwist)
    REGISTER_OBJ_FACTORY(CharGuitarString)
    REGISTER_OBJ_FACTORY(CharHair)
    REGISTER_OBJ_FACTORY(CharIKFingers)
    REGISTER_OBJ_FACTORY(CharIKFoot)
    REGISTER_OBJ_FACTORY(CharIKHand)
    REGISTER_OBJ_FACTORY(CharIKHead)
    REGISTER_OBJ_FACTORY(CharIKMidi)
    REGISTER_OBJ_FACTORY(CharIKRod)
    REGISTER_OBJ_FACTORY(CharIKScale)
    REGISTER_OBJ_FACTORY(CharIKSliderMidi)
    REGISTER_OBJ_FACTORY(CharInterest)
    REGISTER_OBJ_FACTORY(CharLipSync)
    REGISTER_OBJ_FACTORY(CharLipSyncDriver)
    REGISTER_OBJ_FACTORY(CharLookAt)
    REGISTER_OBJ_FACTORY(CharMeshHide)
    REGISTER_OBJ_FACTORY(CharMirror)
    REGISTER_OBJ_FACTORY(CharNeckTwist)
    REGISTER_OBJ_FACTORY(CharPollGroup)
    REGISTER_OBJ_FACTORY(CharPosConstraint)
    REGISTER_OBJ_FACTORY(CharServoBone)
    REGISTER_OBJ_FACTORY(CharSignalApplier)
    REGISTER_OBJ_FACTORY(CharSleeve)
    REGISTER_OBJ_FACTORY(CharTransDraw)
    REGISTER_OBJ_FACTORY(CharUpperTwist)
    REGISTER_OBJ_FACTORY(CharWeightSetter)
    REGISTER_OBJ_FACTORY(CharWeightable)
    REGISTER_OBJ_FACTORY(Character)
    REGISTER_OBJ_FACTORY(ClipCollide)
    REGISTER_OBJ_FACTORY(ClipGraphGenerator)
    REGISTER_OBJ_FACTORY(FileMerger)
    REGISTER_OBJ_FACTORY(FileMergerOrganizer)
    // ⛔ X7 DEFECT FIX -- Waypoint needs its MODULE INIT, not just its factory.
    //
    // This line used to read `REGISTER_OBJ_FACTORY(Waypoint)`, and that was a
    // LATENT NULL-DEREF LANDMINE, live since X2 and reachable by any asset
    // that contains a Waypoint:
    //
    //   Waypoint::Waypoint() (char/Waypoint.cpp:17-25) unconditionally does
    //   `sWaypoints->push_back(this)` with NO null check, and
    //   `Waypoint::sWaypoints` (char/Waypoint.cpp:14) is a POINTER that is
    //   allocated only by Waypoint::Init() (char/Waypoint.cpp:132) --  which
    //   the real game reaches through CharInit() (char/Char.cpp:119) and which
    //   this hand-rolled list replaced without carrying over.
    //
    // Registering the factory without running the init therefore armed a
    // constructor that segfaults on its first call. Nothing had called it: no
    // venue root loaded so far ships a Waypoint object, and the four that
    // BandConfiguration's ctor news up are the first ever constructed here.
    // Measured: SIGSEGV in Waypoint::Waypoint -> list<Waypoint*>::push_front,
    // eight frames under DirLoader::CreateObjects.
    //
    // Calling the engine's own Waypoint::Init() is strictly better than
    // hand-allocating the list: it is the retail body, and it also registers
    // the three waypoint_* DataFuncs and the exit callback that the hand-rolled
    // line silently dropped. Safe here -- RegisterMiloObjectFactories() runs
    // after Symbol::Init() and DataInit() (main_render.cpp:2488, :2496).
    //
    // ★ This is the third instance of the failure mode this file's own header
    // warns about: "a hand-rolled list drifts silently from the module Init()
    // it replaces". X4b found it as two MISSING registrations; this is the
    // same drift producing a registration that is actively unsafe.
    Waypoint::Init();

    // --- world/ -- venue scene: dirs, instances, lights, camera shots (15 classes)
    REGISTER_OBJ_FACTORY(CamShot)
    REGISTER_OBJ_FACTORY(ColorPalette)
    REGISTER_OBJ_FACTORY(WorldCrowd3DCharHandle)
    REGISTER_OBJ_FACTORY(WorldDir)
    REGISTER_OBJ_FACTORY(EventAnim)
    REGISTER_OBJ_FACTORY(WorldInstance)
    REGISTER_OBJ_FACTORY(LightHue)
    REGISTER_OBJ_FACTORY(LightPreset)
    REGISTER_OBJ_FACTORY(PhysicsVolume)
    REGISTER_OBJ_FACTORY(PostProcer)
    REGISTER_OBJ_FACTORY(WorldReflection)
    REGISTER_OBJ_FACTORY(Spotlight)
    REGISTER_OBJ_FACTORY(SpotlightDrawer)
    REGISTER_OBJ_FACTORY(NgSpotlightDrawer)
    REGISTER_OBJ_FACTORY(SpotlightEnder)

    // ★ X4b: two classes whose TUs were ALREADY COMPILED here and which were
    // missing nothing but a line in this list.
    //
    // X4a measured 684 object-factory misses over 14 classes on an RB3 venue
    // root and attributed them to "band3", concluding a venue needs a compiled
    // src/band3/. NONE of the 14 is in src/band3/. These two are in
    // src/system/world/ and src/system/ui/, both of which this target globs in
    // full (ENGINE_WORLD / ENGINE_UI) -- so world/Crowd.cpp and ui/UIColor.cpp
    // were compiled and linked into every rb3-milo and rb3-render binary the
    // whole time, and DirLoader still reported "Can't make WorldCrowd" because
    // nothing had registered them.
    //
    // WorldCrowd is the tell: WorldCrowd3DCharHandle above it comes from a
    // sibling header and WAS registered, while WorldCrowd -- defined in
    // world/Crowd.cpp, registered on X360 by world/World.cpp:24 -- was not.
    // A hand-rolled list drifts silently from the module Init() it replaces;
    // that is its one real cost, and this is an instance of it.
    //
    // Retires 8 of the 684 misses (WorldCrowd 6, UIColor 2). The other 676 need
    // real build work -- see the STEP-0 note in native/CMakeLists.txt.
    REGISTER_OBJ_FACTORY(WorldCrowd)
    REGISTER_OBJ_FACTORY(UIColor)

    // ★ X7: the band-slot placement pair -- and BandWardrobe is X4b's finding
    // repeating EXACTLY.
    //
    // bandobj/BandWardrobe.cpp was ALREADY COMPILED INTO EVERY rb3-render
    // BINARY and had been since X3. It arrives through an UNCONDITIONAL
    // scatter-include chain that is entirely inside this target's own glob:
    //
    //     rndobj/Console.cpp  ->  world/Crowd.cpp:1434  ->  bandobj/BandWardrobe.cpp
    //
    // Measured: 91 BandWardrobe symbols DEFINED in Console.cpp.o (`nm -C`),
    // including FindTarget, GetPlayMode, SetVenueDir and the TheBandWardrobe
    // singleton. They were absent from the linked binary only because
    // --gc-sections drops them as unreferenced -- and nothing referenced them
    // because this list never named the class. So "Can't make BandWardrobe"
    // was a MISSING LINE HERE, not unported code and NOT the ScatterIncludes
    // dedupe lane it has been attributed to for four milestones.
    //
    // BandConfiguration is a genuine (one-line) build add -- see the X7 note
    // in native/CMakeLists.txt. X6 ported the TU and wired objects.json but
    // not the native source list, so the class was unbuildable here.
    // BandConfiguration is SAFE and ON BY DEFAULT: it is a leaf Hmx::Object,
    // it loads clean in all six venue roots, and it is what makes the band's
    // baked stage transforms readable at all (see ReportBandPlacement).
    // RB3_NO_BANDCONFIG=1 is the single-variable A/B control for this lane:
    // with it set, BandConfiguration is unregistered again and the venue is
    // exactly the pre-X7 object graph. Used to root-cause the frame delta
    // against X6's recorded SHAs (see the X7 plan doc, determinism section).
    if (!getenv("RB3_NO_BANDCONFIG")) {
        REGISTER_OBJ_FACTORY(BandConfiguration)
    }

    // ⛔ X7: BandWardrobe + BandCharacter are OFF BY DEFAULT, and this is a
    // MEASURED decision, not caution.
    //
    // Both TUs now compile and link (that was the milestone: 18 -> 0 errors in
    // BandCharacter.cpp, 0 duplicate definitions, 0 undefined references), and
    // registering them does construct real BandCharacters -- "Can't make
    // BandCharacter" disappears. But it then DESYNCS THE STREAM AND CRASHES:
    //
    //   player0 (char/main/main.milo) couldn't find naked_girl in chars
    //   chars (world/shared/chars.milo): Proxy char/main/main.milo class
    //       BandCharacter not RndDir, converting          <- DirLoader.cpp:721
    //   Can't copy type "main" ... different classes RndDir and BandCharacter
    //   FAIL: String chars 290146 > 128                   <- rc=139
    //
    // chars.milo's player0 is a PROXY declared BandCharacter whose proxied file
    // char/main/main.milo declares its own root class RndDir, so
    // DirLoader::SetupDir takes its class-conversion arm (:712-748):
    // NewObject(RndDir), TransferLoaderState, ReplaceObject -- replacing a
    // partially-loaded ObjectDir mid-stream. The next CreateObjects then reads
    // a string length of 290146.
    //
    // ★ THE CHARTER'S RULE GENERALISES, and this is the evidence for it.
    // "Never bind a WRONG class to parse a payload; a miss is strictly better
    // than a wrong parse" (X4d's BandCamShot->CamShot measurement). Here the
    // class is RIGHT and the outcome is the same shape -- because the failure
    // is not in the binding, it is in a load path (proxy class conversion) that
    // no lane has exercised before. An unregistered BandCharacter is
    // ReadDead-skipped cleanly and every prior lane's frame is rc=0; a
    // registered one crashes the load. So the miss is STILL strictly better
    // until the conversion path is fixed, and the default stays off.
    //
    // Turn on with RB3_BAND_MEMBERS=1 to reproduce the desync and work it.
    // ══ X8 UPDATE ══ The desync above is FIXED and its diagnosis is RETRACTED.
    //
    // char/main/main.milo does NOT declare its root as RndDir -- decompressing
    // the asset shows rev 0x1C and root class symbol "BandCharacter". The
    // `RndDir` in that message came from DirLoader::LoadHeader's mRev<=0xC arm
    // after the loader read a garbage rev of 8 out of the four bytes following
    // an 0xADDEADDE object terminator, because ObjectDir::InlineProxy's
    // HX_NATIVE arm read the mInlineProxyType FIELD instead of dispatching
    // through the VIRTUAL AllowsInlineProxy(), which BandCharacter overrides to
    // false. Fixed in obj/Dir.cpp; SetupDir was never at fault. rc=139 -> rc=0
    // and all four members instantiate.
    //
    // Still gated so the ON/OFF A/B stays available, but now DEFAULT-ON with an
    // RB3_NO_BAND_MEMBERS opt-out (the house rule for a native fix once
    // ON-vs-OFF evidence exists -- see the X8 doc's frame table).
    if (getenv("RB3_NO_BAND_MEMBERS") == nullptr) {
        // ⛔ X8: CALL THE ENGINE'S OWN Init(), NOT A BARE REGISTRATION.
        //
        // This is the FOURTH instance in this file of the drift its own header
        // warns about, and the second that was a live crash (X7 found the
        // third: Waypoint's registered-factory-with-a-segfaulting-ctor).
        //
        // BandCharDesc::Init() (bandobj/BandCharDesc.cpp:75-99) is what fills
        // gInstNames[6] = {guitar,bass,drum,mic,keyboard,none}. Retail reaches
        // it through BandInit() (bandobj/Band.cpp:98-113), which this
        // hand-rolled list replaces. Without it every entry of gInstNames is
        // the NULL symbol, so BandWardrobe::LoadMainCharacters assigns every
        // member a null instrument, InstrumentOutfit::GetPiece returns 0 for
        // anything outside {guitar,bass,drum,mic,keyboard}, and
        // BandWardrobe.cpp:620 dereferences it: SIGSEGV in Symbol::Null().
        // MEASURED as exactly that backtrace before this line existed.
        //
        // Init() additionally runs Register(), the two bandchardesc_* DataFuncs
        // and ReloadPrefabs -- all of which the bare factory line dropped.
        BandCharDesc::Init();
        // ⛔ X9: OutfitConfig::Init() BELONGS HERE and CANNOT GO HERE YET.
        //
        // It is the FIFTH instance of this file's documented drift and it gates
        // the band's HEAD AND HANDS: with the band on its marks every member
        // reports nine SHOWN-BUT-EMPTY meshes, and they are exactly
        //   head.mesh eyes.mesh tongue.mesh upper/lowerteeth.mesh
        //   hands_naked.mesh fingernails_resource.mesh eyebrows1_resource.mesh
        //   <hair>_resource.mesh
        // -- selected correctly by the recompose (Showing() true) but carrying
        // zero vertices -- while the log emits `Can't make OutfitConfig` 40
        // times, once per head/hands/hair/facehair/eyebrows resource milo,
        // because OutfitConfig::Init() (bandobj/OutfitConfig.cpp:404-409) never
        // ran its Register().
        //
        // MEASURED: adding the call here does NOT link (rc=1). Referencing
        // Init() retains OutfitConfig.cpp sections that need BandPatchMesh
        // (::Render ::ReProject ::PreRender ::PostRender ::Compress
        // ::ListDrawChildren, its copy-ctor/operator=/operator>>) plus
        // gRB3OutfitComposeActive and the Symbol `recompose` -- none defined in
        // this target.
        //
        // WHY: BandPatchMesh.cpp is NOT compiled standalone. It is
        // SCATTER-INCLUDED into src/system/world/LightPreset.cpp:1503 -- an X360
        // TU-PACKING decision made for objdiff SCORING -- and LightPreset.cpp is
        // not in rb3-render's source list (only LightPresetManager.cpp is). So
        // an X360 match-build packing choice silently decides what the NATIVE
        // link contains. That is X7's "a stub's blast radius is the source list
        // it sits in", inverted: here the TU you need sits in a list your target
        // is not built from. Unblocking it is its own lane. — X9
        BandWardrobe::Init();
        // BandCharacter IS an ObjectDir subclass (-> Character -> RndDir ->
        // ObjectDir), which is why its load failure was a desync rather than a
        // skipped leaf -- exactly the case this file's header describes.
        BandCharacter::Init();
    }

    // X4d MEASUREMENT HARNESS (RB3_BIND_BANDCAMSHOT=1), off by default.
    //
    // BandCamShot is 611 of the 675 outstanding factory misses. It derives from
    // CamShot (bandobj/BandCamShot.h:15), which IS registered above, so the
    // name can be bound to the base allocator without compiling a single TU.
    // Written long-hand because REGISTER_OBJ_FACTORY keys on
    // T::StaticClassName(), which would say "CamShot", not "BandCamShot".
    //
    // The open question X4c could not answer is whether ReadDead absorbs the
    // resulting SHORT READ -- the base Load() consumes fewer bytes than the
    // derived object wrote. That is now measurable: RB3_STREAM_AUDIT reports
    // the per-object ReadDead distance, so the short read shows up as an exact
    // byte count instead of a guess. Left OFF by default because a base-class
    // substitute is a behavioural stand-in, not a port.
    if (getenv("RB3_BIND_BANDCAMSHOT")) {
        Hmx::Object::RegisterFactory(Symbol("BandCamShot"), CamShot::NewObject);
    }
}
