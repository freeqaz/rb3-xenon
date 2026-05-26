// native_link_glue.cpp - Template instantiations needed by the native build
// These are defined in src/link_glue.cpp for the PPC build but that file
// isn't compiled for native. This provides the same definitions.

#include "obj/Object.h"
#include "obj/Dir.h"



// All ObjRefConcrete<T>::CopyRef instantiations
// These are needed when GCC/Clang instantiate copy operations on containers
// holding ObjPtr/ObjRefConcrete types.

// Forward declare all types
#include "synth/ADSR.h"
#include "rndobj/BaseMaterial.h"
#include "char/CharBone.h"
#include "char/CharBones.h"
#include "char/CharClip.h"
#include "char/CharCollide.h"
#include "char/CharDriver.h"
#include "char/CharEyeDartRuleset.h"
#include "char/CharFaceServo.h"
#include "char/CharIKFoot.h"
#include "char/CharInterest.h"
#include "char/CharLipSync.h"
#include "char/CharLipSyncDriver.h"
#include "char/CharLookAt.h"
#include "char/CharPollable.h"
#include "char/Waypoint.h"
#include "char/CharWeightSetter.h"
#include "char/CharWeightable.h"
#include "char/Character.h"
#include "hamobj/DancerSequence.h"
#include "synth/Faders.h"
#include "flow/Flow.h"
#include "flow/FlowLabel.h"
#include "flow/FlowNode.h"
#include "flow/FlowOutPort.h"
#include "synth/FxSend.h"
#include "synth/FxSendMeterEffect.h"
#include "hamobj/HamCamShot.h"
#include "hamobj/HamCharacter.h"
#include "hamobj/HamIKEffector.h"
#include "hamobj/HamIKSkeleton.h"
#include "hamobj/HamLabel.h"
#include "hamobj/HamMove.h"
#include "hamobj/HamNavProvider.h"
#include "hamobj/HamPhraseMeter.h"
#include "hamobj/RhythmDetector.h"
#include "world/LightHue.h"
#include "world/LightPreset.h"
#include "rndobj/MetaMaterial.h"
#include "hamobj/RhythmBattlePlayer.h"
#include "rndobj/Cam.h"
#include "rndobj/CubeTex.h"
#include "rndobj/Dir.h"
#include "rndobj/Draw.h"
#include "rndobj/Env.h"
#include "rndobj/Font.h"
#include "rndobj/Fur.h"
#include "rndobj/Group.h"
#include "rndobj/Lit.h"
#include "rndobj/Mat.h"
#include "rndobj/Mesh.h"
#include "rndobj/MultiMesh.h"
#include "rndobj/Part.h"
#include "rndobj/PostProc.h"
#include "rndobj/PropAnim.h"
#include "rndobj/Tex.h"
#include "rndobj/Trans.h"
#include "rndobj/TransAnim.h"
#include "rndobj/Wind.h"
#include "world/CameraShot.h"
#include "rndobj/EventTrigger.h"
#include "world/Spotlight.h"
#include "world/SpotlightDrawer.h"
#include "gesture/SkeletonClip.h"
#include "synth/MoggClip.h"
#include "synth/Sequence.h"
#include "synth/Sfx.h"
#include "synth/Sound.h"
#include "synth/SynthSample.h"
#include "ui/UIColor.h"
#include "ui/UIComponent.h"
#include "ui/UILabel.h"
#include "ui/UILabelDir.h"
#include "ui/UIList.h"
#include "world/Crowd.h"

#define OBJREFCONCRETE_COPYREF(T) \
template <> \
void ObjRefConcrete<T, ObjectDir>::CopyRef(const ObjRefConcrete<T, ObjectDir> &o) { \
    SetObjConcrete(o.mObject); \
}

// Single-param versions (default T2 = ObjectDir)
template <>
void ObjRefConcrete<RndTransformable>::CopyRef(const ObjRefConcrete<RndTransformable> &o) {
    SetObjConcrete(o.mObject);
}
template <>
void ObjRefConcrete<RndDrawable>::CopyRef(const ObjRefConcrete<RndDrawable> &o) {
    SetObjConcrete(o.mObject);
}
template <>
void ObjRefConcrete<RndAnimatable>::CopyRef(const ObjRefConcrete<RndAnimatable> &o) {
    SetObjConcrete(o.mObject);
}

// Two-param versions (explicit ObjectDir)
OBJREFCONCRETE_COPYREF(ADSR)
OBJREFCONCRETE_COPYREF(BaseMaterial)
OBJREFCONCRETE_COPYREF(CamShot)
OBJREFCONCRETE_COPYREF(CharBone)
OBJREFCONCRETE_COPYREF(CharBonesObject)
OBJREFCONCRETE_COPYREF(CharClip)
OBJREFCONCRETE_COPYREF(CharCollide)
OBJREFCONCRETE_COPYREF(CharDriver)
OBJREFCONCRETE_COPYREF(CharEyeDartRuleset)
OBJREFCONCRETE_COPYREF(CharFaceServo)
OBJREFCONCRETE_COPYREF(CharIKFoot)
OBJREFCONCRETE_COPYREF(CharInterest)
OBJREFCONCRETE_COPYREF(CharLipSync)
OBJREFCONCRETE_COPYREF(CharLipSyncDriver)
OBJREFCONCRETE_COPYREF(CharLookAt)
OBJREFCONCRETE_COPYREF(CharPollable)
OBJREFCONCRETE_COPYREF(CharWeightable)
OBJREFCONCRETE_COPYREF(CharWeightSetter)
OBJREFCONCRETE_COPYREF(Character)
OBJREFCONCRETE_COPYREF(DancerSequence)
OBJREFCONCRETE_COPYREF(EventTrigger)
OBJREFCONCRETE_COPYREF(Fader)
OBJREFCONCRETE_COPYREF(Flow)
OBJREFCONCRETE_COPYREF(FlowLabel)
OBJREFCONCRETE_COPYREF(FlowNode)
OBJREFCONCRETE_COPYREF(FlowOutPort)
OBJREFCONCRETE_COPYREF(FxSend)
OBJREFCONCRETE_COPYREF(FxSendMeterEffect)
OBJREFCONCRETE_COPYREF(HamCamShot)
OBJREFCONCRETE_COPYREF(HamIKEffector)
OBJREFCONCRETE_COPYREF(HamIKSkeleton)
OBJREFCONCRETE_COPYREF(HamLabel)
OBJREFCONCRETE_COPYREF(HamMove)
OBJREFCONCRETE_COPYREF(HamNavProvider)
OBJREFCONCRETE_COPYREF(HamPhraseMeter)
OBJREFCONCRETE_COPYREF(Hmx::Object)
OBJREFCONCRETE_COPYREF(LightHue)
OBJREFCONCRETE_COPYREF(LightPreset)
OBJREFCONCRETE_COPYREF(MetaMaterial)
OBJREFCONCRETE_COPYREF(MoggClip)
OBJREFCONCRETE_COPYREF(ObjectDir)
OBJREFCONCRETE_COPYREF(RhythmDetector)
OBJREFCONCRETE_COPYREF(RhythmBattlePlayer)
OBJREFCONCRETE_COPYREF(RndCam)
OBJREFCONCRETE_COPYREF(RndCubeTex)
OBJREFCONCRETE_COPYREF(RndDir)
OBJREFCONCRETE_COPYREF(RndEnviron)
OBJREFCONCRETE_COPYREF(RndFontBase)
OBJREFCONCRETE_COPYREF(RndFur)
OBJREFCONCRETE_COPYREF(RndGroup)
OBJREFCONCRETE_COPYREF(RndLight)
OBJREFCONCRETE_COPYREF(RndMat)
OBJREFCONCRETE_COPYREF(RndMesh)
OBJREFCONCRETE_COPYREF(RndMultiMesh)
OBJREFCONCRETE_COPYREF(RndParticleSys)
OBJREFCONCRETE_COPYREF(RndPostProc)
OBJREFCONCRETE_COPYREF(RndPropAnim)
OBJREFCONCRETE_COPYREF(RndTex)
OBJREFCONCRETE_COPYREF(RndTransAnim)
OBJREFCONCRETE_COPYREF(RndWind)
OBJREFCONCRETE_COPYREF(Sfx)
OBJREFCONCRETE_COPYREF(SeqInst)
OBJREFCONCRETE_COPYREF(SkeletonClip)
OBJREFCONCRETE_COPYREF(Sound)
OBJREFCONCRETE_COPYREF(Spotlight)
OBJREFCONCRETE_COPYREF(SpotlightDrawer)
OBJREFCONCRETE_COPYREF(SynthSample)
OBJREFCONCRETE_COPYREF(UIColor)
OBJREFCONCRETE_COPYREF(UIComponent)
OBJREFCONCRETE_COPYREF(UILabel)
OBJREFCONCRETE_COPYREF(UILabelDir)
OBJREFCONCRETE_COPYREF(UIList)
OBJREFCONCRETE_COPYREF(Waypoint)
OBJREFCONCRETE_COPYREF(WorldCrowd)
OBJREFCONCRETE_COPYREF(HamCharacter)

#undef OBJREFCONCRETE_COPYREF

// ============================================================================
// BinStream operator<< for ObjPtrList<T>
// ============================================================================

#include "char/CharClipSet.h"
#include "char/CharInterest.h"
#include "char/Waypoint.h"
#include "hamobj/HamListRibbon.h"
#include "hamobj/HamScrollSpeedIndicator.h"
#include "hamobj/RhythmDetector.h"
#include "rndobj/CamAnim.h"
#include "rndobj/LitAnim.h"
#include "rndobj/MatAnim.h"
#include "rndobj/MeshAnim.h"
#include "rndobj/PartAnim.h"
#include "rndobj/PartLauncher.h"
#include "rndobj/Rnd.h"
#include "rndobj/TexBlendController.h"
#include "ui/UIListDir.h"
#include "world/SpotlightDrawer.h"

#define BINSTREAM_OP_OBJPTRLIST(T) \
template <> \
BinStream &operator<<(BinStream &bs, const ObjPtrList<T, ObjectDir> &list) { \
    bs << list.size(); \
    for (ObjPtrList<T>::iterator it = list.begin(); it != list.end(); ++it) { \
        Hmx::Object *obj = *it; \
        const char *name = obj ? obj->Name() : ""; \
        bs << name; \
    } \
    return bs; \
}

BINSTREAM_OP_OBJPTRLIST(CamShot)
BINSTREAM_OP_OBJPTRLIST(CharBone)
BINSTREAM_OP_OBJPTRLIST(CharPollable)
BINSTREAM_OP_OBJPTRLIST(CharWeightSetter)
BINSTREAM_OP_OBJPTRLIST(Character)
BINSTREAM_OP_OBJPTRLIST(EventTrigger)
BINSTREAM_OP_OBJPTRLIST(Fader)
BINSTREAM_OP_OBJPTRLIST(HamCamShot)
BINSTREAM_OP_OBJPTRLIST(Hmx::Object)
BINSTREAM_OP_OBJPTRLIST(RndAnimatable)
BINSTREAM_OP_OBJPTRLIST(RndDrawable)
BINSTREAM_OP_OBJPTRLIST(RndFontBase)
BINSTREAM_OP_OBJPTRLIST(RndLight)
BINSTREAM_OP_OBJPTRLIST(RndMat)
BINSTREAM_OP_OBJPTRLIST(RndMesh)
BINSTREAM_OP_OBJPTRLIST(RndPartLauncher)
BINSTREAM_OP_OBJPTRLIST(RndTexBlendController)
BINSTREAM_OP_OBJPTRLIST(RndTransformable)
BINSTREAM_OP_OBJPTRLIST(Sequence)
BINSTREAM_OP_OBJPTRLIST(UILabel)
BINSTREAM_OP_OBJPTRLIST(Waypoint)

#undef BINSTREAM_OP_OBJPTRLIST

// ============================================================================
// BinStream operator<< for ObjPtrVec<T>
// ============================================================================

#define BINSTREAM_OP_OBJPTRVEC(T) \
template <> \
BinStream &operator<<(BinStream &bs, const ObjPtrVec<T, ObjectDir> &vec) { \
    bs << (int)vec.size(); \
    for (int i = 0; i < (int)vec.size(); i++) { \
        const Hmx::Object *obj = vec[i]; \
        const char *name = obj ? obj->Name() : ""; \
        bs << name; \
    } \
    return bs; \
}

BINSTREAM_OP_OBJPTRVEC(CharClip)
BINSTREAM_OP_OBJPTRVEC(Flow)
BINSTREAM_OP_OBJPTRVEC(Hmx::Object)
// RhythmDetector: explicit specialization in Character.cpp (custom body)
BINSTREAM_OP_OBJPTRVEC(RndDrawable)
BINSTREAM_OP_OBJPTRVEC(RndEnviron)
BINSTREAM_OP_OBJPTRVEC(RndLight)
// RndMat: explicit specialization in CharClipGroup.cpp (custom body)
BINSTREAM_OP_OBJPTRVEC(Spotlight)
BINSTREAM_OP_OBJPTRVEC(SpotlightDrawer)

#undef BINSTREAM_OP_OBJPTRVEC

// ============================================================================
// BinStream operator<< for ObjOwnerPtr<T>
// ============================================================================

#define BINSTREAM_OP_OBJOWNERPTR(T) \
template <> \
BinStream &operator<<(BinStream &bs, const ObjOwnerPtr<T> &ptr) { \
    Hmx::Object *obj = ptr; \
    const char *name = obj ? obj->Name() : ""; \
    bs << name; \
    return bs; \
}

BINSTREAM_OP_OBJOWNERPTR(CharInterest)
BINSTREAM_OP_OBJOWNERPTR(CharLookAt)
BINSTREAM_OP_OBJOWNERPTR(CharWeightable)
BINSTREAM_OP_OBJOWNERPTR(EventTrigger)
BINSTREAM_OP_OBJOWNERPTR(FxSend)
BINSTREAM_OP_OBJOWNERPTR(Hmx::Object)
BINSTREAM_OP_OBJOWNERPTR(ObjectDir)
BINSTREAM_OP_OBJOWNERPTR(RndAnimatable)
BINSTREAM_OP_OBJOWNERPTR(RndCamAnim)
BINSTREAM_OP_OBJOWNERPTR(RndDrawable)
BINSTREAM_OP_OBJOWNERPTR(RndEnviron)
BINSTREAM_OP_OBJOWNERPTR(RndFont)
BINSTREAM_OP_OBJOWNERPTR(RndFont3d)
BINSTREAM_OP_OBJOWNERPTR(RndLight)
BINSTREAM_OP_OBJOWNERPTR(RndLightAnim)
BINSTREAM_OP_OBJOWNERPTR(RndMatAnim)
BINSTREAM_OP_OBJOWNERPTR(RndMesh)
BINSTREAM_OP_OBJOWNERPTR(RndMeshAnim)
BINSTREAM_OP_OBJOWNERPTR(RndParticleSysAnim)
BINSTREAM_OP_OBJOWNERPTR(RndTex)
BINSTREAM_OP_OBJOWNERPTR(RndTransAnim)
BINSTREAM_OP_OBJOWNERPTR(RndTransformable)
BINSTREAM_OP_OBJOWNERPTR(RndWind)
BINSTREAM_OP_OBJOWNERPTR(Spotlight)

#undef BINSTREAM_OP_OBJOWNERPTR

// ============================================================================
// BinStream operator<< for ObjDirPtr<T>
// ============================================================================

#define BINSTREAM_OP_OBJDIRPTR(T) \
template <> \
BinStream &operator<<(BinStream &bs, const ObjDirPtr<T> &ptr) { \
    T *dir = ptr; \
    const char *name = dir ? dir->Name() : ""; \
    bs << name; \
    return bs; \
}

BINSTREAM_OP_OBJDIRPTR(HamListRibbon)
BINSTREAM_OP_OBJDIRPTR(HamScrollSpeedIndicator)
BINSTREAM_OP_OBJDIRPTR(ObjectDir)
BINSTREAM_OP_OBJDIRPTR(RndDir)
BINSTREAM_OP_OBJDIRPTR(UILabelDir)
BINSTREAM_OP_OBJDIRPTR(UIListDir)

#undef BINSTREAM_OP_OBJDIRPTR

// RndShader stubs — all Select stubs are now defined in Shader.cpp

// RndText::FitTextJust now defined in Text.cpp

// FlowSetProperty — PropertyTask::Poll now defined in FlowSetProperty.cpp

// ============================================================================
// Static member definitions moved to their proper .cpp files (guarded by
// #ifdef HX_NATIVE). See git log for the full list of relocated definitions.
// ============================================================================

// HDCache::Flush — defined outside HDCache.cpp to match original TU split
#include "os/HDCache.h"
void HDCache::Flush() {}

// Ogg/Vorbis allocator stubs. VorbisMem.cpp is empty in this tree, and the
// Xbox link_glue.cpp isn't compiled for native, so we provide them here.
#include <cstdlib>
extern "C" {
void *OggMalloc(int n) { return malloc((size_t)n); }
void *OggCalloc(int n, int sz) { return calloc((size_t)n, (size_t)sz); }
void *OggRealloc(void *p, int n) { return realloc(p, (size_t)n); }
void OggFree(void *p) { free(p); }
}
