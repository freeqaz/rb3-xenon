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

// RB3-360 retail: RndAnimatable::Load reads the archive rev from a file-scope
// static halfword (lbl_82CCxxxx, `lhz`) populated once at Load entry, not the
// BinStreamRev member. Mirror that so the rev comparisons match.
static unsigned short sAnimRev;

static TaskUnits gRateUnits[6] = { kTaskSeconds, kTaskBeats,           kTaskUISeconds,
                                   kTaskBeats,   kTaskTutorialSeconds, kTaskBeats };
static float gRateFpu[6] = { 30.0f, 480.0f, 30.0f, 1.0f, 30.0f, 15.0f };

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

BEGIN_PROPSYNCS(RndAnimatable)
    SYNC_PROP(rate, (int &)mRate);
    SYNC_PROP_SET(frame, mFrame, SetFrame(_val.Float(), 1.0f))
    SYNC_PROP_SET(start_frame, StartFrame(), )
    SYNC_PROP_SET(end_frame, EndFrame(), )
END_PROPSYNCS

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
    LOAD_REVS(bs)
    ASSERT_REVS(4, 0)
    sAnimRev = d.rev;
    if (sAnimRev > 1)
        d >> mFrame;
    if (sAnimRev > 3) {
        d >> (int &)mRate;
    } else if (sAnimRev > 2) {
        bool rate;
        d >> rate;
        mRate = (Rate)(!rate);
    }
    if (sAnimRev < 1) {
        int count;
        d >> count;
        float theScale = 1.0f;
        float theOffset = 0.0f;
        float theMin = 0.0f;
        float theMax = 0.0f;
        bool theLoop = false;
        int read;
        int unused1, unused2, unused3, unused4, unused5, unused6, unused7;
        while (count-- != 0) {
            d >> read;
            switch (read) {
            case 0:
                d >> theScale >> theOffset;
                break;
            case 1:
                d >> theMin >> theMax;
                d >> theLoop;
                break;
            case 2:
                d >> unused1 >> unused2;
                break;
            case 3:
                d >> unused3 >> unused4;
                break;
            case 4:
                d >> unused5 >> unused6 >> unused7;
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
        d >> animList;
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
__declspec(noinline) TaskUnits RndAnimatable::Units() const { return gRateUnits[mRate]; }
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
        AnimTask *task = dynamic_cast<AnimTask *>(RefPtrOf(it)->RefOwner());
        if (task) {
            delete task;
            it = mRefs.begin();
        } else
            ++it;
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
// Body follows rb3-Wii src/system/rndobj/Anim.cpp:144.
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
        AnimTask *blendTask = task->BlendTask();
        if (blendTask) {
            delay += blendTask->TimeUntilEnd();
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
    // rb3-Wii starts the anim here; dc3 deferred it to the first Poll() behind the
    // mActive latch, which no longer exists in the RB3-era layout.
    mAnim->StartAnim();
}

// Retail's lean-overload path never had a listener/easeType/easePower/wait
// AnimTask at all (see rb3-Wii's AnimTask ctor, exactly these 6 params). Same
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
    // rb3-Wii starts the anim here; dc3 deferred it to the first Poll() behind the
    // mActive latch, which no longer exists in the RB3-era layout.
    mAnim->StartAnim();
}

AnimTask::~AnimTask() { TheTaskMgr.QueueTaskDelete(mBlendTask); }

void AnimTask::Replace(ObjRef *from, Hmx::Object *to) {
    if (RefIs(from, mAnim)) {
        RndAnimatable *myAnim = Anim();
        if (!mAnim.SetObj(to)) {
            if (mBlendTask && mBlendTask->Anim() == myAnim) {
                mBlendTask = nullptr;
            }
            Hmx::Object::Replace(from, to);
            TheTaskMgr.QueueTaskDelete(this);
        }
        return;
    } else
        Hmx::Object::Replace(from, to);
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

// RB3-era Poll, ported from the rb3-Wii oracle: no easing (mEaseFunc/mEasePower),
// no listener dispatch, no wait/active gating and no mFrameSpan — those are all
// dc3-newer additions whose backing fields do not exist in a 0x6c AnimTask.
// StartAnim() now happens in the ctors instead of behind the mActive latch.
void AnimTask::Poll(float time) {
    if (!mAnim)
        return;
    float blend = 1.0f;
    if (mBlendPeriod) {
        blend = time / mBlendPeriod;
        if (blend >= 1.0f) {
            blend = 1.0f;
            TheTaskMgr.QueueTaskDelete(mBlendTask);
            mBlendPeriod = 0.0f;
        } else if (!mBlendTask) {
            float oldtime = mBlendTime;
            mBlendTime = time;
            blend = (time - oldtime) / (mBlendPeriod - oldtime);
        }
    } else {
        if (mBlendTask)
            TheTaskMgr.QueueTaskDelete(mBlendTask);
    }

    // rb3-Wii maps the raw task time into frame space up front, then tests that
    // same mapped value for the end-of-anim condition below.
    time = time * mScale + mOffset;

    float frame;
    if (mLoop) {
        frame = Mod(time - mMin, mMax - mMin) + mMin;
    } else {
        frame = Clamp<float>(mMin, mMax, time);
    }
    mAnim->SetFrame(frame, blend);

    if (!mAnimTarget
        || ((!mLoop && !mBlending && !mBlendPeriod)
            && ((time > mMax || time < mMin) || mScale == 0.0f))) {
        TheTaskMgr.QueueTaskDelete(this);
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
    float local_blend; // 0x88
    float local_ease_power; // 0x84
    EaseType local_ease; // 0x80
    TaskUnits local_units; // 0x7c
    const char *local_name; // 0x78
    float local_delay; // 0x74
    bool local_wait; // 0x72
    bool local_wrap; // 0x71
    bool animTaskLoop; // 0x70

    local_blend = 0.0f;
    float animTaskStart = StartFrame();
    float animTaskEnd = EndFrame();
    animTaskLoop = Loop();
    float p = FramesPerUnit();
    local_units = Units();
    local_delay = 0.0f;
    local_name = nullptr;
    local_wait = false;
    local_wrap = false;
    local_ease_power = 2;
    local_ease = kEaseLinear;
    Hmx::Object *local_listener = nullptr;

    static Symbol blend("blend");
    static Symbol range("range");
    static Symbol loop("loop");
    static Symbol dest("dest");
    static Symbol period("period");
    static Symbol delay("delay");
    static Symbol units("units");
    static Symbol name("name");
    static Symbol wait("wait");
    static Symbol wrap("wrap");
    static Symbol ease_power("ease_power");
    static Symbol ease("ease");
    static Symbol listener("listener");

    arr->FindData(blend, local_blend, false);
    arr->FindData(delay, local_delay, false);
    arr->FindData(units, (int &)local_units, false);
    arr->FindData(name, local_name, false);
    arr->FindData(wait, local_wait, false);
    arr->FindData(wrap, local_wrap, false);
    arr->FindData(ease_power, local_ease_power, false);
    arr->FindData(ease, (int &)local_ease, false);

    if (arr->FindArray(listener, false)) {
        local_listener = arr->FindArray(listener, true)->GetObj(1);
    }
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
        p = periodArr->Float(1);
        MILO_ASSERT(p, 0x1C5);
        float fabs = std::fabs(animTaskEnd - animTaskStart);
        p = fabs / p;
    }
    AnimTask *task = new AnimTask(
        this,
        animTaskStart,
        animTaskEnd,
        p,
        animTaskLoop,
        local_blend,
        local_listener,
        local_ease,
        local_ease_power,
        local_wait
    );
    ObjPtr<AnimTask> taskPtr(nullptr, task);
    if (local_name && taskPtr) {
        MILO_ASSERT(DataThis(), 0x1CD);
        taskPtr->SetName(local_name, DataThis()->DataDir());
    }
    if (local_wait && taskPtr->BlendTask()) {
        if (taskPtr->BlendTask()->Anim()->GetRate() != GetRate()) {
            MILO_NOTIFY("%s: need same rate to wait", Name());
        } else
            local_delay = taskPtr->BlendTask()->TimeUntilEnd();
    }
    static Symbol trigger_anim_task("trigger_anim_task");
    if (!Property(trigger_anim_task, false) || Property(trigger_anim_task)->Int() != 0) {
        TheTaskMgr.Start(taskPtr, local_units, local_delay);
    }

    return DataNode(taskPtr.Ptr());
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
