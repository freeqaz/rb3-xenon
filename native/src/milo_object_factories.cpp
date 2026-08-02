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
    REGISTER_OBJ_FACTORY(Waypoint)

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
}
