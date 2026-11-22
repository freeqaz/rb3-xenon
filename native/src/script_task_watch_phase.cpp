// rb3-xenon native -- a ScriptTask ends when an object it watches dies.
//
// THE DEFECT. ScriptTask::UpdateVarsObjects collects the objects its script
// names into mObjects, and retail puts the task on each one's ref list with
// Hmx::Object::AddRef(ObjRefOwner *) (bl 0x8275BD08 at 0x82749968). When one
// of them dies, ~Object's ring walk runs ScriptTask::Replace(obj, 0)
// (0x82749450), which deletes the task; ThreadTask::Replace (0x827494B0) only
// drops the object from mObjects while the thread is executing. ~ScriptTask
// releases each entry (bl 0x8275B378 loop at 0x82749F60). Natively mObjects
// held raw pointers and nothing was linked into the object's ring, so only the
// task's mThis was tracked: a watched object's death left the task running.
//
// THE REFERENCES are those retail bodies, read off band.exe. Two controls must
// pass with or without the fix: the task's own mThis dying deletes it (that
// path was always tracked), and an object the task does not watch dying
// leaves it alone. A control that fails means the model, not the fix, is wrong.

#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "rndobj/Mat.h"

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

    // A ScriptTask whose watched-object list is exactly `watched`, with mThis
    // set to `self` (nullptr: none).
    ScriptTask *MakeTask(Hmx::Object *watched, Hmx::Object *self) {
        DataArray *script = new DataArray(1);
        script->Node(0) = DataNode(1);
        DataArray *objs = new DataArray(1);
        objs->Node(0) = DataNode(1);
        if (watched)
            objs->Node(0) = DataNode(watched);
        Hmx::Object *old = DataSetThis(self);
        ScriptTask *task = new ScriptTask(script, false, objs);
        DataSetThis(old);
        objs->Release();
        script->Release();
        return task;
    }

    // Exposes ThreadTask's protected watched list.
    class ThreadProbe : public ThreadTask {
    public:
        ThreadProbe(DataArray *script, DataArray *objs) : ThreadTask(script, objs) {}
        int Watched() const { return mObjects.size(); }
    };

    // A watched object killed by a plain delete or by its dir's cascade.
    void WatchedCase(bool cascade) {
        ObjectDir *dir = Hmx::Object::New<ObjectDir>();
        RndMat *mat = Hmx::Object::New<RndMat>();
        mat->SetName("st_watched.mat", dir);
        ScriptTask *task = MakeTask(mat, nullptr);
        const bool before = Task::IsLive(task);
        if (cascade)
            delete dir;
        else
            delete mat;
        const bool live = Task::IsLive(task);
        Gate(cascade ? "scripttask-watched-cascade" : "scripttask-watched-delete",
             before && !live,
             "%s killed the task's one watched object: task %s (X360 ScriptTask::Replace "
             "0x82749450 deletes it)",
             cascade ? "~ObjectDir's cascade" : "a plain delete",
             live ? "STILL LIVE" : "deleted");
        if (live)
            delete task;
        if (!cascade)
            delete dir;
    }

    // Retail adds one entry to a watched object's ref list and ~ScriptTask
    // removes it again.
    void RefListCase() {
        RndMat *mat = Hmx::Object::New<RndMat>();
        const int base = mat->RefCount();
        ScriptTask *task = MakeTask(mat, nullptr);
        const int watched = mat->RefCount();
        delete task;
        const int after = mat->RefCount();
        Gate("scripttask-reflist", watched == base + 1 && after == base,
             "watched object's ref count: %d alone, %d watched, %d after ~ScriptTask "
             "(X360: +1 via AddRef 0x82749968, back via Release 0x82749F68)",
             base, watched, after);
        delete mat;
    }

    // Control: the task's own mThis dying deletes it (always tracked natively).
    void ThisCase() {
        RndMat *self = Hmx::Object::New<RndMat>();
        ScriptTask *task = MakeTask(nullptr, self);
        const bool before = Task::IsLive(task);
        delete self;
        const bool live = Task::IsLive(task);
        Gate("control-scripttask-this-delete", before && !live,
             "the task's mThis died: task %s", live ? "STILL LIVE" : "deleted");
        if (live)
            delete task;
    }

    // Control: an object the task does not watch dying leaves the task alone.
    void UnwatchedCase() {
        RndMat *watched = Hmx::Object::New<RndMat>();
        RndMat *other = Hmx::Object::New<RndMat>();
        ScriptTask *task = MakeTask(watched, nullptr);
        delete other;
        const bool live = Task::IsLive(task);
        Gate("control-scripttask-unwatched-delete", live,
             "an unwatched object died: task %s", live ? "live" : "DELETED");
        if (live)
            delete task;
        delete watched;
    }

    // A ThreadTask whose own script deletes a watched object: while executing,
    // retail (0x827494B0) only drops the object from mObjects; the task waits
    // on its next command and stays live.
    void ThreadExecutingCase() {
        RndMat *victim = Hmx::Object::New<RndMat>();
        DataNode &var = DataVariable("st_victim");
        DataNode oldVar = var;
        var = DataNode(victim);
        DataArray *script = DataReadString("x {delete $st_victim} {$task wait 1}");
        DataArray *objs = new DataArray(1);
        objs->Node(0) = DataNode(victim);
        ThreadProbe *task = new ThreadProbe(script, objs);
        objs->Release();
        script->Release();
        const int before = task->Watched();
        task->Poll(0.0f);
        const bool live = Task::IsLive(task);
        const int after = live ? task->Watched() : -1;
        var = oldVar;
        Gate("threadtask-executing-drop", live && before == 1 && after == 0,
             "the executing thread deleted its watched object: task %s, watched %d -> %d "
             "(X360: live, 1 -> 0)",
             live ? "live" : "DELETED", before, after);
        if (live)
            delete task;
    }
}

int RunScriptTaskWatchPhase(GateFn gate) {
    gGate = gate;
    printf("\n=== a ScriptTask ends when an object it watches dies ===\n");
    ThisCase();
    UnwatchedCase();
    WatchedCase(false);
    WatchedCase(true);
    RefListCase();
    ThreadExecutingCase();
    return 0;
}
