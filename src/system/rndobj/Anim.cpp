// AnimTask's ctor builds mAnim (ObjOwnerPtr<RndAnimatable>, owner-only) inline:
// retail stores owner, null and the vtable in place, and calls the
// out-of-line ctors only for mAnimTarget and mBlendTask.
#ifndef RB3_OBJOWNERPTR_INLINE_OWNER_CTOR
#define RB3_OBJOWNERPTR_INLINE_OWNER_CTOR 1
#endif
#ifndef RB3_TU_OBJPTR_OWNER_CTOR_DEFER_OBJECT
#define RB3_TU_OBJPTR_OWNER_CTOR_DEFER_OBJECT 1
#endif

#include "rndobj/Anim.h"
#include "math/Easing.h"
#include "math/Utl.h"
#include "obj/Data.h"
#include "obj/DataUtl.h"

#include "obj/Msg.h"
#include "obj/Object.h"
#include "os/File.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "rndobj/AnimFilter.h"
#include "rndobj/Group.h"
#include "rndobj/Env.h"
#include "utl/BinStream.h"

// Retail RndAnimatable::Load keeps no BinStreamRev: it splits the packed rev
// into one aligned file-scope aggregate (altRev +0, rev +4), tests the rev with
// `lhz`, and reads everything from the raw stream.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_RndAnimatable;

// Five rates: retail's two tables are 5 entries each, gRateFpu sitting 0x14
// past gRateUnits.
static TaskUnits gRateUnits[5] = { kTaskSeconds, kTaskBeats, kTaskUISeconds,
                                   kTaskBeats, kTaskTutorialSeconds };
static float gRateFpu[5] = { 30.0f, 480.0f, 30.0f, 1.0f, 30.0f };

#pragma region Hmx::Object

RndAnimatable::RndAnimatable() : mFrame(0.0f), mRate(k30_fps) {}

BEGIN_HANDLERS(RndAnimatable)
    HANDLE_ACTION(set_frame, SetFrame(_msg->Float(2), 1.0f))
    HANDLE_EXPR(frame, mFrame)
    HANDLE_ACTION(set_key, SetKey(_msg->Float(2)))
    HANDLE_EXPR(end_frame, EndFrame())
    HANDLE_EXPR(start_frame, StartFrame())
    HANDLE(animate, OnAnimate)
    HANDLE_ACTION(stop_animation, StopAnimation())
    HANDLE_EXPR(is_animating, IsAnimating())
    HANDLE(convert_frames, OnConvertFrames)
END_HANDLERS

#ifdef HX_NATIVE
BEGIN_PROPSYNCS(RndAnimatable)
    SYNC_PROP(rate, (int &)mRate);
    SYNC_PROP_SET(frame, mFrame, SetFrame(_val.Float(), 1.0f))
    SYNC_PROP_SET(start_frame, StartFrame(), )
    SYNC_PROP_SET(end_frame, EndFrame(), )
END_PROPSYNCS
#else
// 0x82401100: retail syncs only `rate` and `frame` (no start_frame/end_frame);
// frame is PropSync'd into mFrame and then, unless the op is a get/size,
// re-applied through SetFrame(mFrame, 1.0f).
BEGIN_PROPSYNCS(RndAnimatable)
    SYNC_PROP(rate, (int &)mRate);
    SYNC_PROP_MODIFY(frame, mFrame, SetFrame(mFrame, 1.0f))
END_PROPSYNCS
#endif

BEGIN_SAVES(RndAnimatable)
    SAVE_REVS(4, 0)
    bs << mFrame << mRate;
END_SAVES

BEGIN_COPYS(RndAnimatable)
    CREATE_COPY(RndAnimatable)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mFrame)
        COPY_MEMBER(mRate)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(4, 0)

BEGIN_LOADS(RndAnimatable)
    int rev;
    bs >> rev;
    gRevs_RndAnimatable.rev = getHmxRev(rev);
    gRevs_RndAnimatable.altRev = getAltRev(rev);
    if (gRevs_RndAnimatable.rev > 1)
        bs >> mFrame;
    if (gRevs_RndAnimatable.rev > 3) {
        bs >> (int &)mRate;
    } else if (gRevs_RndAnimatable.rev > 2) {
        bool rate;
        bs >> rate;
        mRate = (Rate)(!rate);
    }
    if (gRevs_RndAnimatable.rev < 1) {
        int count;
        bs >> count;
        float theScale = 1.0f;
        float theOffset = 0.0f;
        float theMin = 0.0f;
        float theMax = 0.0f;
        bool theLoop = false;
        int read;
        int unused1, unused2, unused3, unused4, unused5, unused6, unused7;
        while (count-- != 0) {
            bs >> read;
            switch (read) {
            case 0:
                bs >> theScale >> theOffset;
                break;
            case 1:
                bs >> theMin >> theMax;
                bs >> theLoop;
                break;
            case 2:
                bs >> unused1 >> unused2;
                break;
            case 3:
                bs >> unused3 >> unused4;
                break;
            case 4:
                bs >> unused5 >> unused6 >> unused7;
                break;
            default:
                break;
            }
        }
        if (theScale != 1.0f || theOffset != 0.0f || (theMin != theMax)) {
            const char *filt = MakeString("%s.filt", FileGetBase(Name()));
            RndAnimFilter *filtObj = Dir()->New<RndAnimFilter>(filt);
            filtObj->SetProperty("anim", this);
            filtObj->SetProperty("scale", theScale);
            filtObj->SetProperty("offset", theOffset);
            filtObj->SetProperty("min", theMin);
            filtObj->SetProperty("max", theMax);
            filtObj->SetProperty("loop", theLoop);
        }
        ObjPtrList<RndAnimatable> animList(this);
        bs >> animList;
        RndGroup *theGroup = dynamic_cast<RndGroup *>(this);
        FOREACH (it, animList) {
            if (theGroup)
                theGroup->AddObject(*it);
            else
                MILO_NOTIFY("%s not in group", (*it)->Name());
        }
    }
END_LOADS

#pragma endregion
#pragma region RndAnimatable


TaskUnits RndAnimatable::RateToTaskUnits(Rate myRate) { return gRateUnits[myRate]; }
// Retail inlines Units() at every call inside this TU, but code from other TUs
// calls it out of line (20 retail call sites, none in this TU). Those TUs see
// this body through MatAnim.cpp's scatter-include, which defines
// ANIM_UNITS_OUT_OF_LINE to keep their calls out of line.
#ifdef ANIM_UNITS_OUT_OF_LINE
__declspec(noinline)
#endif
TaskUnits RndAnimatable::Units() const { return gRateUnits[mRate]; }
float RndAnimatable::FramesPerUnit() { return gRateFpu[mRate]; }

bool RndAnimatable::ConvertFrames(float &f) {
    f /= FramesPerUnit();
    return (Units() != kTaskBeats);
}

bool RndAnimatable::IsAnimating() {
    FOREACH (it, Refs()) {
        if (dynamic_cast<AnimTask *>(RefPtrOf(it)->RefOwner()))
            return true;
    }
    return false;
}

void RndAnimatable::StopAnimation() {
    for (ObjRef::iterator it = mRefs.begin(); it != mRefs.end();) {
        AnimTask *task = dynamic_cast<AnimTask *>(RefPtrOf(it++)->RefOwner());
        if (task) {
            delete task;
            it = mRefs.begin();
        }
    }
}

void RndAnimatable::FireFlowLabel(Symbol s) {
    if (s.Null()) return;
    FOREACH (it, Refs()) {
        Hmx::Object *owner = RefPtrOf(it)->RefOwner();
        if (owner && owner->ClassName() == "AnimTask") {
            AnimTask *task = static_cast<AnimTask *>(owner);
            if (task->AnimTarget()) {
                owner->Handle(Message("on_anim_event", s), false);
                break;
            }
        }
    }
    static Symbol flow_label_fired("flow_label_fired");
    Message msg(flow_label_fired, s.Str());
    Export(msg, true);
}

Task *RndAnimatable::Animate(float blend, bool wait, float delay) {
    AnimTask *task = new AnimTask(this, StartFrame(), EndFrame(), FramesPerUnit(), Loop(), blend);
    if (wait && task->BlendTask()) {
        delay += task->BlendTask()->TimeUntilEnd();
    }
    TheTaskMgr.Start(task, Units(), delay);
    return task;
}

Task *RndAnimatable::Animate(
    float blend, bool wait, float delay, Hmx::Object *o, EaseType e, float f4, bool b5
) {
    AnimTask *task = new AnimTask(
        this, StartFrame(), EndFrame(), FramesPerUnit(), Loop(), blend, o, e, f4, b5
    );
    ObjPtr<AnimTask> taskPtr(nullptr, task);
    if (wait && taskPtr->BlendTask()) {
        delay += taskPtr->BlendTask()->TimeUntilEnd();
    }
    if (delay == 0) {
        SetFrame(StartFrame(), 1);
    }
    TheTaskMgr.Start(taskPtr, Units(), delay);
    return taskPtr;
}

Task *RndAnimatable::Animate(
    float start, float end, TaskUnits units, float period, float blend
) {
    float fpu;
    if (period) {
        fpu = std::fabs(end - start);
        fpu = fpu / period;
    } else {
        const float fpus[3] = { 30.0f, 480.0f, 30.0f };
        fpu = fpus[units];
    }
    AnimTask *task = new AnimTask(this, start, end, fpu, false, blend);
    TheTaskMgr.Start(task, units, 0.0f);
    return task;
}

Task *RndAnimatable::Animate(
    float start,
    float end,
    TaskUnits units,
    float period,
    float blend,
    Hmx::Object *listener,
    EaseType easeType,
    float f9,
    bool b10
) {
    float fpu;
    if (period) {
        fpu = std::fabs(end - start);
        fpu = fpu / period;
    } else {
        const float fpus[3] = { 30.0f, 480.0f, 30.0f };
        fpu = fpus[units];
    }
    AnimTask *task =
        new AnimTask(this, start, end, fpu, false, blend, listener, easeType, f9, b10);
    ObjPtr<AnimTask> taskPtr(nullptr, task);
    SetFrame(start, 1);
    TheTaskMgr.Start(taskPtr, units, 0);
    return taskPtr;
}

// RB3-era lean overload (no listener/easeType/easePower/wrap) -- see Anim.h.
// Body: the RB3-era form.
Task *RndAnimatable::Animate(
    float blend,
    bool wait,
    float delay,
    Rate rate,
    float start,
    float end,
    float period,
    float scale,
    Symbol type
) {
    static Symbol dest("dest");
    static Symbol loop("loop");
    float fpu;
    float taskStart = start;
    if (type == dest)
        start = mFrame;
    if (period) {
        fpu = std::fabs(end - taskStart);
        fpu = fpu / period;
    } else
        fpu = scale * gRateFpu[rate];

    AnimTask *task = new AnimTask(this, start, end, fpu, type == loop, blend);
    if (wait) {
        if (task->BlendTask()) {
            delay += task->BlendTask()->TimeUntilEnd();
        }
    }
    TheTaskMgr.Start(task, gRateUnits[rate], delay);
    return task;
}

Task *RndAnimatable::Animate(
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
    EaseType easeType,
    float easePower,
    bool b10
) {
    static Symbol dest("dest");
    static Symbol loop("loop");
    float fpu;
    if (type == dest)
        start = mFrame;
    if (period) {
        fpu = std::fabs(end - start);
        fpu = fpu / period;
    } else
        fpu = scale * gRateFpu[rate];

    AnimTask *task = new AnimTask(
        this, start, end, fpu, type == loop, blend, listener, easeType, easePower, b10
    );
    ObjPtr<AnimTask> taskPtr(nullptr, task);
    if (wait) {
        if (taskPtr->BlendTask()) {
            delay += taskPtr->BlendTask()->TimeUntilEnd();
        }
    }
    if (delay == 0) {
        SetFrame(start, 1);
    }
    TheTaskMgr.Start(taskPtr, gRateUnits[rate], delay);
    return taskPtr;
}

#pragma endregion
#pragma region AnimTask

AnimTask::AnimTask(
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
)
    : mAnim(this), mAnimTarget(this), mBlendTask(this), mBlendPeriod(blend),
      mLoop(loop) {
    // listener/easeType/easePower/wait are accepted for source compatibility with
    // the dc3-era call sites still in the tree (FlowAnimate, BandButton, ...), but
    // RB3-era AnimTask stores none of them — see the layout note in Anim.h.
    mBlending = false;
    mBlendTime = 0;
    MILO_ASSERT(anim, 0x213);
    mMin = Min(start, end);
    mMax = Max(start, end);
    if (NearlyZero(fpu)) {
        fpu = 1;
    }
    if (start < end) {
        mScale = fpu;
        mOffset = mMin;
    } else {
        mScale = -fpu;
        mOffset = mMax;
    }
    Hmx::Object *target = anim->AnimTarget();
    if (target) {
        FOREACH (it, target->Refs()) {
            Hmx::Object *owner = RefPtrOf(it)->RefOwner();
            if (owner && owner->ClassName() == StaticClassName()) {
                mBlendTask = static_cast<AnimTask *>(owner);
                MILO_ASSERT(mBlendTask != this, 0x231);
                break;
            }
        }
    }
    if (mBlendPeriod && mBlendTask) {
        mBlendTask->mBlending = true;
    }
    mAnim = anim;
    mAnimTarget = anim->AnimTarget();
    // RB3 starts the anim here; dc3 deferred it to the first Poll() behind the
    // mActive latch, which no longer exists in the RB3-era layout.
    mAnim->StartAnim();
}

// Retail's lean-overload path never had a listener/easeType/easePower/wait
// AnimTask at all (the AnimTask ctor takes exactly these 6 params). Same
// body as the 10-arg ctor above, with the extra fields fixed to their
// no-listener/no-ease defaults rather than accepted as parameters.
AnimTask::AnimTask(
    RndAnimatable *anim, float start, float end, float fpu, bool loop, float blend
)
    : mAnim(this), mAnimTarget(this), mBlendTask(this), mBlendPeriod(blend),
      mLoop(loop) {
    mBlending = false;
    mBlendTime = 0;
    MILO_ASSERT(anim, 0x213);
    mMin = Min(start, end);
    mMax = Max(start, end);
    // No zero-fpu fallback in retail's six-argument ctor.
    if (start < end) {
        mScale = fpu;
        mOffset = mMin;
    } else {
        mScale = -fpu;
        mOffset = mMax;
    }
    Hmx::Object *target = anim->AnimTarget();
    if (target) {
        FOREACH (it, target->Refs()) {
            Hmx::Object *owner = RefPtrOf(it)->RefOwner();
            if (owner && owner->ClassName() == StaticClassName()) {
                mBlendTask = static_cast<AnimTask *>(owner);
                MILO_ASSERT(mBlendTask != this, 0x231);
                break;
            }
        }
    }
    if (mBlendPeriod && mBlendTask) {
        mBlendTask->mBlending = true;
    }
    mAnim = anim;
    mAnimTarget = anim->AnimTarget();
    // RB3 starts the anim here; dc3 deferred it to the first Poll() behind the
    // mActive latch, which no longer exists in the RB3-era layout.
    mAnim->StartAnim();
}

// RB3-era code deletes mBlendTask directly rather
// than routing through TaskMgr::QueueTaskDelete (a dc3-newer-engine
// indirection - see Task.cpp). Retail's actual bytes (Ghidra @0x82401588)
// show a null check + a vtable-slot-0 call with arg 1, i.e. exactly the
// scalar deleting destructor pattern that `delete mBlendTask;` compiles to,
// not an out-of-line call to QueueTaskDelete.
AnimTask::~AnimTask() { delete mBlendTask; }

// Only the animatable going away matters: retail ignores any non-null
// replacement, forwards nothing to Hmx::Object::Replace, and deletes the task
// outright rather than queueing it.
void AnimTask::Replace(ObjRef *from, Hmx::Object *to) {
#ifdef HX_NATIVE
    if (!to && RefIs(from, mAnim)) {
#else
    // `from` is on the left of this compare in retail (cmplw r4, upcast).
    if (!to
        && reinterpret_cast<Hmx::Object *>(from)
            == static_cast<Hmx::Object *>(mAnim.Ptr())) {
#endif
        RndAnimatable *myAnim = Anim();
        if (mBlendTask && mBlendTask->Anim() == myAnim) {
#ifdef HX_NATIVE
            mBlendTask = nullptr;
#else
            mBlendTask.ReleaseObjConcrete();
#endif
        }
        delete this;
    }
}

float AnimTask::TimeUntilEnd() {
    float time;
    if (mScale > 0.0f) {
        float fpu = mAnim->FramesPerUnit();
        time = (mMax - mAnim->GetFrame()) / fpu;
    } else {
        float fpu = mAnim->FramesPerUnit();
        time = (mAnim->GetFrame() - mMin) / fpu;
    }
    return time;
}

// RB3-era Poll: no easing (mEaseFunc/mEasePower),
// no listener dispatch, no wait/active gating and no mFrameSpan — those are all
// dc3-newer additions whose backing fields do not exist in a 0x6c AnimTask.
// StartAnim() now happens in the ctors instead of behind the mActive latch.
// Tasks are deleted directly here (delete through vtable slot 0), not queued.
void AnimTask::Poll(float time) {
    float blend = 1.0f;
    if (mBlendPeriod) {
        blend = time / mBlendPeriod;
        if (blend >= 1.0f) {
            blend = 1.0f;
            delete mBlendTask;
            mBlendPeriod = 0.0f;
        } else if (!mBlendTask) {
            float oldtime = mBlendTime;
            mBlendTime = time;
            blend = (time - oldtime) / (mBlendPeriod - oldtime);
        }
    } else {
        if (mBlendTask)
            delete mBlendTask;
    }

    // RB3 maps the raw task time into frame space up front, then tests that
    // same mapped value for the end-of-anim condition below.
    time = time * mScale + mOffset;

    float frame;
    if (mLoop) {
        float min = mMin;
        frame = Mod(time - min, mMax - min) + min;
    } else {
        frame = Clamp<float>(mMin, mMax, time);
    }
    mAnim->SetFrame(frame, blend);

    if (!mAnimTarget
        || ((!mLoop && !mBlending && !mBlendPeriod)
            && ((time > mMax || time < mMin) || mScale == 0.0f))) {
        delete this;
    }
}

#pragma endregion
#pragma region Handlers

DataNode RndAnimatable::OnConvertFrames(DataArray *arr) {
    float f = arr->Float(2);
    bool conv = ConvertFrames(f);
    *arr->Var(2) = f;
    return conv;
}

DataNode RndAnimatable::OnAnimate(DataArray *arr) {
    // Retail reads exactly nine keys here (blend/range/loop/dest/period/delay/
    // units/name/wait) and builds the six-argument AnimTask: no listener, ease,
    // wrap or trigger_anim_task, and no assert on a zero period.
    TaskUnits local_units = kTaskSeconds; // 0x60
    float local_blend = 0.0f; // 0x5c
    float animTaskStart = StartFrame();
    float animTaskEnd = EndFrame();
    bool animTaskLoop = Loop();
    float local_delay = 0.0f; // 0x58
    const char *local_name = nullptr; // 0x54
    bool local_wait = false; // 0x50
    local_units = Units();
    float p = FramesPerUnit();

    static Symbol blend("blend");
    static Symbol range("range");
    static Symbol loop("loop");
    static Symbol dest("dest");
    static Symbol period("period");
    static Symbol delay("delay");
    static Symbol units("units");
    static Symbol name("name");
    static Symbol wait("wait");

    arr->FindData(blend, local_blend, false);
    arr->FindData(delay, local_delay, false);
    arr->FindData(units, (int &)local_units, false);
    arr->FindData(name, local_name, false);
    arr->FindData(wait, local_wait, false);

    DataArray *rangeArr = arr->FindArray(range, false);
    if (rangeArr) {
        animTaskStart = rangeArr->Float(1);
        animTaskEnd = rangeArr->Float(2);
        animTaskLoop = false;
    }
    DataArray *loopArr = arr->FindArray(loop, false);
    if (loopArr) {
        if (loopArr->Size() > 1)
            animTaskStart = loopArr->Float(1);
        else
            animTaskStart = StartFrame();
        if (loopArr->Size() > 2)
            animTaskEnd = loopArr->Float(2);
        else
            animTaskEnd = EndFrame();
        animTaskLoop = true;
    }
    DataArray *destArr = arr->FindArray(dest, false);
    if (destArr) {
        animTaskStart = GetFrame();
        animTaskEnd = destArr->Float(1);
        animTaskLoop = false;
    }
    DataArray *periodArr = arr->FindArray(period, false);
    if (periodArr) {
        p = std::fabs(animTaskEnd - animTaskStart);
        p = p / periodArr->Float(1);
    }
    AnimTask *task = new AnimTask(
        this, animTaskStart, animTaskEnd, p, animTaskLoop, local_blend
    );
    if (local_name) {
        MILO_ASSERT(DataThis(), 0x1CD);
        task->SetName(local_name, DataThis()->DataDir());
    }
    if (local_wait && task->BlendTask()) {
        if (task->BlendTask()->Anim()->GetRate() != GetRate()) {
            MILO_NOTIFY("%s: need same rate to wait", Name());
        } else
            local_delay = task->BlendTask()->TimeUntilEnd();
    }
    TheTaskMgr.Start(task, local_units, local_delay);
    return DataNode(task);
}

// sw2 scatter-include (default/Anim <- rndobj/Line.cpp)
#define gRev gRev_Line
#define gAltRev gAltRev_Line
#if !HX_NATIVE  // native: skip X360 scatter/COMDAT-pairing include
#include "rndobj/Line.cpp"
#endif
#undef gRev
#undef gAltRev

// sw2 scatter-include (default/Anim <- rndobj/Group.cpp)
#define gRev gRev_Group
#define gAltRev gAltRev_Group
#if !HX_NATIVE  // native: skip X360 scatter/COMDAT-pairing include
#include "rndobj/Group.cpp"
#endif
#undef gRev
#undef gAltRev

// sw2 scatter-include (default/Anim <- rndobj/MotionBlur.cpp)
#define gRev gRev_MotionBlur
#define gAltRev gAltRev_MotionBlur
#if !HX_NATIVE  // native: skip X360 scatter/COMDAT-pairing include
#include "rndobj/MotionBlur.cpp"
#endif
#undef gRev
#undef gAltRev

// sw2 scatter-include (default/Anim <- rndobj/Dir.cpp = RndDir COMDATs)
#define gRev gRev_RndDir
#define gAltRev gAltRev_RndDir
#if !HX_NATIVE  // native: skip X360 scatter/COMDAT-pairing include
#include "rndobj/Dir.cpp"
#endif
#undef gRev
#undef gAltRev
