#pragma once
#include "flow/FlowLabelProvider.h"
#include "flow/FlowNode.h"
#include "flow/FlowPtr.h"
#include "math/Easing.h"
#include "obj/Object.h"
#include "rndobj/Anim.h"
#include "utl/MemMgr.h"

/** "Plays an animation" */
class FlowAnimate : public FlowNode, public FlowLabelProvider {
public:
    // Hmx::Object
    virtual ~FlowAnimate();
    virtual void Replace(ObjRef *, Hmx::Object *);
    OBJ_CLASSNAME(FlowAnimate)
    OBJ_SET_TYPE(FlowAnimate)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, CopyType);
    virtual void Load(BinStream &);
    // FlowNode
    virtual bool Activate();
    virtual void Deactivate(bool);
    virtual void ChildFinished(FlowNode *);
    virtual void RequestStop();
    virtual void RequestStopCancel();
    virtual void Execute(QueueState);
    virtual bool IsRunning();

    // retail keeps FlowAnimate::operator new OUT OF LINE and ICF-folded (its
    // `new` site is a single `bl ??2<folded>@@SAPAXI@Z` with NO StaticClassName
    // call), unlike the OBJ_MEM_OVERLOAD majority which retail inlined.
    // Classified from the CTOR relocation, not the symbol name -- confirmed by
    // direct COFF/relocation read of NewObject: target is
    // `li r3,0x104 ; bl ??2CriticalSection@@SAPAXI@Z ; stw r3,0x50(r31)`, no
    // StaticClassName call, matching the FlowSound/FlowDistance/FlowRun pattern.
    MEM_OVERLOAD(FlowAnimate, 0x1C)
    NEW_OBJ(FlowAnimate)

protected:
    FlowAnimate();

    void ResetAnim();
    void OnAnimEvent(Symbol);

    ObjOwnerPtr<AnimTask> mAnimTask; // 0x60
    /** "Anim object to animate" */
    FlowPtr<RndAnimatable> mAnim; // 0x6c
    /** "How should we handle stop requests?" */
    StopMode mStopMode; // 0x84
    bool mBetweenStopMarkers; // 0x88
    int mDeferredStopMode; // 0x8c
    /** "Blend time, does not work on Property Animations!" */
    float mBlend; // 0x90
    /** "wait until current animation finishes before starting" */
    bool mWait; // 0x94
    /** "delay in units before starting this animation" */
    float mDelay; // 0x98
    /** "Enable animation filtering" */
    bool mEnable; // 0x9c
    /** "Rate to animate" */
    RndAnimatable::Rate mRate; // 0xa0
    /** "Start frame of animation" */
    float mStart; // 0xa4
    /** "End frame of animation" */
    float mEnd; // 0xa8
    /** "Period of animation if non-zero" */
    float mPeriod; // 0xac
    /** "Scale of animation" */
    float mScale; // 0xb0
    /** "How the animation is played". Possible options:
        (range "Play from [start] frame to [end] frame, then stop")
        (loop "Loop animation from [start] to [end] frame")
        (dest "Play from current frame to [end] frame")
    */
    Symbol mType; // 0xb4
    bool mStopDeferred; // 0xb8
    /** "Easing to apply to animation" */
    EaseType mEase; // 0xbc
    /** "Modifier to easing equation" */
    float mEasePower; // 0xc0
    /** "Wraps animation frame values into range rather than clamping them.
    This will make the animation loop when the frame is out of range." */
    bool mWrap; // 0xc4
    /** "If true, Flow will not track/stop or otherwise affect this animation again." */
    bool mImmediateRelease; // 0xc5
    // See FlowDistance.h: retail RB3 sizes FlowNode-derived objects 0x10 larger
    // than dc3's newer layout; trailing pad in the virtual-Hmx::Object region.
    // NewObject size immediate (0x104) confirms via the sizeof oracle.
    char _retailTrailingPad[16]; // 0xc6
};
