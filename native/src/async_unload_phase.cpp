// rb3-xenon native -- A8: an ASYNC ~ObjectDir leaves its objects' refs alone
// until each object actually dies.
//
// Under TheLoadMgr.AsyncUnload() ~ObjectDir hands the dir's objects to a
// DirUnloader (obj/DirUnloader.cpp), which deletes one per LoadMgr poll over
// the following frames. Natively ~ObjectDir used to NullifyAllRefs every one of
// them first, so for the whole unload window refs to LIVE objects read NULL,
// and once the cascade ran owner steps (cascade_notify_phase.cpp) owners were
// told of deaths that had not happened: an AnimTask was deleted while its anim
// was still alive and animating.
//
// THE REFERENCE is X360's ~ObjectDir (Dir.cpp, the AsyncUnload branch:
// `new DirUnloader(this)`, no walk) plus ~Object's ReplaceRefs(0) at each
// object's real death: during the window an external ref still names the
// object and an AnimTask over a resident anim is still alive and still aims at
// it; after the DirUnloader drains, the ref is NULL and the task is gone
// (AnimTask::Replace deletes it).

#include "obj/Dir.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "rndobj/Anim.h"
#include "rndobj/Env.h"
#include "rndobj/MatAnim.h"
#include "utl/Loader.h"

#include <cstdarg>
#include <cstdio>

typedef void (*GateFn)(const char *, bool, const char *);

namespace {

    GateFn gGate = nullptr;

    void Gate(const char *name, bool ok, const char *fmt, ...)
        __attribute__((format(printf, 3, 4)));
    void Gate(const char *name, bool ok, const char *fmt, ...) {
        char buf[512];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        gGate(name, ok, buf);
    }

    void DrainLoaders() {
        float saved = TheLoadMgr.SetLoaderPeriod(10.0f);
        for (int i = 0; i < 1000 && !TheLoadMgr.Loading().empty(); i++)
            TheLoadMgr.Poll();
        TheLoadMgr.SetLoaderPeriod(saved);
    }
}

int RunAsyncUnloadPhase(GateFn gate) {
    gGate = gate;
    printf("\n=== A8: an async ~ObjectDir leaves refs alone until each object dies ===\n");
    DrainLoaders();
    const bool idle = TheLoadMgr.Loading().empty();

    ObjectDir *dir = Hmx::Object::New<ObjectDir>();
    RndMatAnim *anim = Hmx::Object::New<RndMatAnim>();
    anim->SetName("a8_async.mnm", dir);
    Hmx::Object *resident = Hmx::Object::New<Hmx::Object>();
    resident->SetName("a8_async_resident", dir);
    ObjPtr<Hmx::Object> external(nullptr, resident);
    AnimTask *task = new AnimTask(anim, 0.0f, 10.0f, 30.0f, false, 0.0f);

    TheLoadMgr.StartAsyncUnload();
    delete dir;
    TheLoadMgr.FinishAsyncUnload();
    const bool queued = !TheLoadMgr.Loading().empty();

    // The unload window: both objects are alive.
    const bool refKept = external.Ptr() == resident;
    const bool taskLive = Task::IsLive(task);
    const bool taskAimed = taskLive && task->Anim() == anim;
    Gate("async-unload-window-refs-intact", idle && queued && refKept && taskAimed,
         "after an async ~ObjectDir, before its DirUnloader ran (queued=%d): external ref "
         "%s; AnimTask %s (X360: both untouched until the objects die)",
         queued, refKept ? "still names the resident" : "NULLED",
         !taskLive ? "DELETED early" : (taskAimed ? "alive on its anim" : "alive, mAnim NULL"));

    DrainLoaders();
    const bool drained = TheLoadMgr.Loading().empty();
    const bool liveAfter = Task::IsLive(task);
    Gate("async-unload-drained-refs-dead", drained && external.Ptr() == nullptr && !liveAfter,
         "after the DirUnloader drained: external ref %s, AnimTask %s (X360: NULL, deleted)",
         external.Ptr() ? "STILL SET" : "NULL", liveAfter ? "STILL LIVE" : "deleted");
    // A failing run: delete a task left on a NULL anim; one still aimed at a
    // freed anim is leaked rather than unlinked through the freed block.
    if (liveAfter && !task->Anim())
        delete task;
    return 0;
}
