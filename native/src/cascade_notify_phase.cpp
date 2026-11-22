// rb3-xenon native -- A8: the ~ObjectDir cascade notifies owner-control holders.
//
// THE DEFECT. Native ObjectDir::DeleteObjects pre-nullifies the ref ring of
// every object a dying dir tree names (Phase 0, obj/Dir.cpp) through
// Hmx::Object::NullifyAllRefs. That used to null each ref with
// ObjRef::NullifyObj and nothing else. On X360 the same objects die through
// `delete`, and ~Object (0x8275CBF0) hands every ref Replace(this, 0); for an
// OWNER-CONTROL holder (an ObjOwnerPtr, or a node of an ObjPtrList/ObjPtrVec
// in kObjListOwnerControl mode) that forwards to the owner's Replace, the
// owner's only notice. Natively the ref went NULL and the owner never heard:
// rb3-app's first App::Run frame crashed in AnimTask::Poll on the splash
// task's NULL mAnim after Splash::EndSplasher's ~RndDir (retail's
// AnimTask::Replace deletes the task).
//
// THE REFERENCES are the X360 owners' Replace bodies, modelled here, not
// called:
//   * AnimTask::Replace (Anim.cpp; retail compares `from` against mAnim and
//     deletes the task when the replacement is NULL): the task must be gone.
//   * RndGroup::Replace (Group.cpp): AddObject(to, from) then
//     RemoveObject(from), so a NULL replacement leaves the group without the
//     child, not holding a NULL entry.
// Each cascade case has a plain-`delete` control, which takes the
// ReplaceRefs(nullptr) path and must pass with or without the fix; a control
// that fails means the model, not the cascade, is wrong.

#include "obj/Dir.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "rndobj/Anim.h"
#include "rndobj/Env.h"
#include "rndobj/Group.h"
#include "rndobj/Mat.h"
#include "rndobj/MatAnim.h"

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

    // A dir naming one RndMatAnim, an AnimTask over the anim, and the anim
    // killed either by the dir's cascade or by a plain delete.
    void AnimTaskCase(bool cascade) {
        ObjectDir *dir = Hmx::Object::New<ObjectDir>();
        RndMatAnim *anim = Hmx::Object::New<RndMatAnim>();
        anim->SetName("a8_cascade.mnm", dir);
        AnimTask *task = new AnimTask(anim, 0.0f, 10.0f, 30.0f, false, 0.0f);
        const bool before = Task::IsLive(task);
        if (cascade)
            delete dir;
        else
            delete anim;
        const bool live = Task::IsLive(task);
        const bool nullAnim = live && task->Anim() == nullptr;
        Gate(cascade ? "cascade-animtask-replace" : "control-delete-animtask-replace",
             before && !live,
             "%s killed the AnimTask's anim: task %s (X360 AnimTask::Replace deletes it)",
             cascade ? "~ObjectDir's cascade" : "a plain delete",
             !live ? "deleted"
                   : (nullAnim ? "STILL LIVE with mAnim NULL (the owner never heard)"
                               : "STILL LIVE"));
        if (live)
            delete task; // mAnim is NULL: ~AnimTask touches only mBlendTask
        if (!cascade)
            delete dir;
    }

    // A dir-less RndGroup holding a RndMat named by a dir, the mat killed by
    // the dir's cascade or by a plain delete.
    void GroupCase(bool cascade) {
        ObjectDir *dir = Hmx::Object::New<ObjectDir>();
        RndMat *mat = Hmx::Object::New<RndMat>();
        mat->SetName("a8_cascade.mat", dir);
        RndGroup *group = Hmx::Object::New<RndGroup>();
        group->AddObject(mat, nullptr);
        const int before = group->Objects().size();
        if (cascade)
            delete dir;
        else
            delete mat;
        int after = 0, nulls = 0;
        for (ObjPtrList<Hmx::Object>::iterator it = group->Objects().begin();
             it != group->Objects().end(); ++it) {
            after++;
            if (!*it)
                nulls++;
        }
        Gate(cascade ? "cascade-group-replace" : "control-delete-group-replace",
             before == 1 && after == 0,
             "%s killed a group child: %d child(ren) before, %d after (%d NULL); X360 "
             "RndGroup::Replace removes it",
             cascade ? "~ObjectDir's cascade" : "a plain delete", before, after, nulls);
        delete group;
        if (!cascade)
            delete dir;
    }

    // Collateral: an object destroyed DURING a cascade that the dir does not
    // name. The cascade kills an anim, AnimTask::Replace deletes the task, and
    // a dir-less RndGroup holding the task as a child must hear that the task
    // died (X360: ~Object's ReplaceRefs -> RndGroup::Replace removes it).
    void CollateralCase() {
        ObjectDir *dir = Hmx::Object::New<ObjectDir>();
        RndMatAnim *anim = Hmx::Object::New<RndMatAnim>();
        anim->SetName("a8_collateral.mnm", dir);
        AnimTask *task = new AnimTask(anim, 0.0f, 10.0f, 30.0f, false, 0.0f);
        RndGroup *group = Hmx::Object::New<RndGroup>();
        group->AddObject(task, nullptr);
        delete dir;
        const bool live = Task::IsLive(task);
        int after = 0;
        for (ObjPtrList<Hmx::Object>::iterator it = group->Objects().begin();
             it != group->Objects().end(); ++it)
            after++;
        Gate("cascade-collateral-replace", !live && after == 0,
             "~ObjectDir's cascade killed the anim, its AnimTask was %s; the group "
             "holding the task has %d child(ren) after (X360: 0)",
             live ? "NOT deleted" : "deleted", after);
        // A child left behind is a freed block: leak the group rather than run
        // its ring unlink into it.
        if (after == 0)
            delete group;
        if (live)
            delete task;
    }
}

int RunCascadeNotifyPhase(GateFn gate) {
    gGate = gate;
    printf("\n=== A8: the ~ObjectDir cascade notifies owner-control holders ===\n");
    AnimTaskCase(false);
    AnimTaskCase(true);
    GroupCase(false);
    GroupCase(true);
    CollateralCase();
    return 0;
}
