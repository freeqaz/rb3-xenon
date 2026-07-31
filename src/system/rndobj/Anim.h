#pragma once
#include "math/Easing.h"
#include "math/Utl.h"
#include "obj/Data.h"

#include "obj/Object.h"
#include "obj/Task.h"
#include "obj/Object.h"
#include "utl/MemMgr.h"
#include "utl/PoolAlloc.h"
#include <list>

class AnimTask;

// Retail X360 RB3 (rev-11-era) RndAnimatable vtable ends its own virtual slice at
// ListAnimChildren; DC3's newer RndAnimatable appended `OnListFlowLabels` (Flow
// integration). rb3-Wii's Anim.h confirms no such virtual exists in the RB3 era.
// Keeping it virtual adds a 10th own-slot, shifting every later (derived-class)
// slot up 0x4 and breaking EventTrigger::Trigger vcalls (target slot 0x24 vs ours
// 0x28; verified via machine-code anchors in VocalTrackDir::PlayIntro/TrackReset/
// SpotlightPhraseSuccess/CanChat + LightPreset::StartAnim). OnListFlowLabels is
// HANDLE'd directly (Anim.cpp), so dropping the virtual keeps it callable. The
// native engine still wants virtual dispatch, so gate the keyword behind HX_NATIVE
// (same idiom as RND_DC3_VIRTUAL in rndobj/Rnd.h).
#ifdef HX_NATIVE
#define ANIM_DC3_VIRTUAL virtual
#else
#define ANIM_DC3_VIRTUAL
#endif

/**
 * @brief: An object that can be animated.
 * Original _objects description:
 * "Base class for animatable objects. Anim objects change
 * their state or other objects."
 */
class RndAnimatable : public virtual Hmx::Object {
public:
    enum Rate {
        k30_fps = 0,
        k480_fpb = 1,
        k30_fps_ui = 2,
        k1_fpb = 3,
        k30_fps_tutorial = 4,
        k15_fpb = 5
    };

    OBJ_CLASSNAME(Anim);
    OBJ_SET_TYPE(Anim);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual ~RndAnimatable() {}
    /** Determine whether or not this animation should loop. */
    virtual bool Loop() { return false; }
    /** Start the animation. */
    virtual void StartAnim() {}
    /** End the animation. */
    virtual void EndAnim() {}
    virtual void SetFrame(float frame, float blend) { mFrame = frame; }
    /** Get this animatable's first frame. */
    virtual float StartFrame() { return 0; }
    /** Get this animatable's last frame. */
    virtual float EndFrame() { return 0; }
    /** The actual target Object we want to animate. */
    virtual Hmx::Object *AnimTarget() { return this; }
    /** Set any of this Anim's keys values to any relevant anim target properties at the
     * given frame. */
    virtual void SetKey(float frame) {}
    /** Get the list of this Object's children that are animatable. */
    virtual void ListAnimChildren(std::list<RndAnimatable *> &) const {}
    ANIM_DC3_VIRTUAL DataNode OnListFlowLabels(DataArray *) { return 0; }

    OBJ_MEM_OVERLOAD(0x1B)
    NEW_OBJ(RndAnimatable)

    /** Determine if this animatable has any active tasks associated with it. */
    bool IsAnimating();
    /** Kill any active tasks associated with this animatable. */
    void StopAnimation();

    // Retail's 3-arg call sites (UITransitionHandler etc.) resolve to a genuinely
    // distinct, leaner Animate() overload with no listener/easeType/easePower/wrap
    // params at all (see rb3-Wii Anim.h: no such params exist pre-DC3). Restoring
    // that lean overload here (rather than relying on this extended one's defaults)
    // matches retail's call-site codegen exactly (fewer arg-setup instructions).
    Task *Animate(float blend, bool wait, float delay);
    Task *Animate(
        float blend,
        bool wait,
        float delay,
        Hmx::Object *listener,
        EaseType easeType = kEaseLinear,
        float easePower = 0,
        bool wrap = false
    );
    // Same rationale as the 3-arg and 5-arg lean overloads above: RB3-era
    // RndAnimatable has NO listener/easeType/easePower/wrap on this form at all
    // (see rb3-Wii Anim.h, which stops at `Symbol type`). Retail's call sites
    // (BandStarDisplay::SetNumStars, ...) therefore emit a 10-word outgoing
    // parameter area; routing them through the dc3-era extended overload below
    // costs 4 extra 8-byte param slots (+0x20 of arg area), which in turn
    // inflates the caller's frame AND its static-init guard funclets
    // (funclet frame == 0x20 + 8 * max outgoing param count on X360 MSVC).
    // `listener` deliberately has NO default on the extended overload so that a
    // 9-argument call is unambiguous and binds here.
    Task *Animate(
        float blend,
        bool wait,
        float delay,
        Rate rate,
        float start,
        float end,
        float period,
        float scale,
        Symbol type
    );
    Task *Animate(
        float blend,
        bool wait,
        float delay,
        Rate rate,
        float start,
        float end,
        float period,
        float scale,
        Symbol type,
        Hmx::Object *listener,
        EaseType easeType = kEaseLinear,
        float easePower = 0,
        bool wrap = false
    );
    // Same rationale as the 3-arg overload above, for the start/end/units form.
    Task *Animate(float start, float end, TaskUnits units, float period, float blend);
    Task *Animate(
        float start,
        float end,
        TaskUnits units,
        float period,
        float blend,
        Hmx::Object *listener,
        EaseType easeType = kEaseLinear,
        float easePower = 0,
        bool wrap = false
    );

    TaskUnits Units() const;
    float FramesPerUnit();
    bool ConvertFrames(float &frames);

    // weak getters and setters
    Rate GetRate() { return mRate; }
    void SetRate(Rate r) { mRate = r; }
    float GetFrame() const { return mFrame; }
    void ResetFrame() { mFrame = kHugeFloat; }

    static TaskUnits RateToTaskUnits(Rate);

private: // RB2 said so
    /** "Frame of animation". It ranges from 0 to what EndFrame() returns. */
    float mFrame; // 0x8
    /** "Rate to animate" */
    Rate mRate; // 0xc

protected:
    RndAnimatable();
    void FireFlowLabel(Symbol);
    /** Create a new AnimTask using the configuration in the supplied DataArray.
     * @param [in] arr The supplied DataArray.
     * @returns A DataNode housing the newly created task.
     * Expected DataArray contents:
     *     No specific node ordering, but the DataArray can optionally have:
     *     - data for symbols: blend, delay, units, name, wait
     *     - a DataArray for symbol range with floats at nodes 1 and 2
     *     - a DataArray for symbol loop with floats at nodes 1 and 2
     *     - a DataArray for symbol dest with a float at node 1
     *     - a DataArray for symbol period with a float at node 1
     * Example usage: {$this on_animate}
     */
    DataNode OnAnimate(DataArray *arr);
    DataNode OnConvertFrames(DataArray *);
};

/** A task meant for animating. */
class AnimTask : public Task {
public:
    // Retail's lean Animate() overloads (see RndAnimatable::Animate above) never
    // built up a listener/easeType/easePower/wait AnimTask at all (rb3-Wii's
    // AnimTask ctor takes exactly these 6 params, full stop). A forwarding stub
    // that delegates to the 10-arg ctor with hardcoded defaults gets inlined by
    // /Ob2 right back into the exact same call as the extended overload -
    // verified empirically, netting zero codegen change at the caller. This
    // separate, real 6-arg overload is required so the caller genuinely emits
    // fewer argument-setup instructions (matching retail).
    AnimTask(
        RndAnimatable *anim,
        float start,
        float end,
        float fpu,
        bool loop,
        float blend
    );
    AnimTask(
        RndAnimatable *anim,
        float start,
        float end,
        float fpu,
        bool loop,
        float blend,
        Hmx::Object *listener,
        EaseType easeType,
        float easePower,
        bool wait
    );
    virtual ~AnimTask();
    virtual void Replace(ObjRef *, Hmx::Object *);
    OBJ_CLASSNAME(AnimTask);
    virtual void Poll(float);

    float TimeUntilEnd();
    AnimTask *BlendTask() const { return mBlendTask; }
    RndAnimatable *Anim() const { return mAnim; }
    Hmx::Object *AnimTarget() const { return mAnimTarget; }

    POOL_OVERLOAD(AnimTask, 0x75);

    // ── RB3-era layout: sizeof(AnimTask) == 0x6c (108), NOT dc3's 0x90 (144) ──
    // Both AnimTask::operator delete and the scalar deleting destructor embed
    // sizeof(AnimTask) as a literal via POOL_OVERLOAD's PoolFree(sizeof(cls), v);
    // retail emits `li r3, 0x6c` where dc3's field set gives us `li r3, 0x90`.
    // The 36-byte delta is forced, not fitted: dc3's extra scalar members
    // (mPrevFrame/mEaseFunc/mEasePower/mWait/mFrameSpan/mActive) total only 24
    // bytes, so they alone CANNOT account for 36 — the 12-byte ObjPtr mListener
    // must go too. Independently, rb3-Wii's AnimTask (the RB3-era oracle) has
    // exactly this field set, and laying it out on our Task base (mAnim at 0x28,
    // compiler-verified) sums to exactly 0x6c. Easing/listener/wait/active are
    // dc3-newer engine features; RB3 had none of them.
    // Consequence: StartAnim() moves into the ctors (rb3-Wii does it there),
    // since there is no mActive first-poll latch any more.
    /** The animatable this task should be animating. */
    ObjOwnerPtr<RndAnimatable> mAnim; // 0x28
    ObjPtr<Hmx::Object> mAnimTarget; // 0x34
    /** The anim task to blend into. */
    ObjPtr<AnimTask> mBlendTask; // 0x40
    /** Whether or not this animation should blend into another. */
    bool mBlending; // 0x4c
    /** The time it takes to blend into mBlendTask. */
    float mBlendTime; // 0x50
    float mBlendPeriod; // 0x54
    /** Start animation frame. */
    float mMin; // 0x58
    /** End animation frame. */
    float mMax; // 0x5c
    /** Multiplier to speed of animation. */
    float mScale; // 0x60
    /** "Amount to offset frame for animation" */
    float mOffset; // 0x64
    /** Whether or not the animation should loop. */
    bool mLoop; // 0x68
};
