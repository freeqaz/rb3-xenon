#include "rndobj/PostProcMgr.h"
#include "math/Utl.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "rndobj/Poll.h"
#include "rndobj/PostProc.h"

RndPostProcMgr::RndPostProcMgr()
    : mSelectedPostProc(this), mBlendFromPostProc(nullptr), mBlendToPostProc(this), mBlendDuration(-1), mBlendStartTime(-1) {
    mBlendFromPostProc = Hmx::Object::New<RndPostProc>();
}

RndPostProcMgr::~RndPostProcMgr() {
    if (mSelectedPostProc) {
        mSelectedPostProc->Unselect();
    }
    RELEASE(mBlendFromPostProc);
}

BEGIN_HANDLERS(RndPostProcMgr)
    HANDLE(copy_from, OnCopyFromPostProc)
    HANDLE(blend_to, OnBlendToPostProc)
    HANDLE_SUPERCLASS(RndPollable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(RndPostProcMgr)
    SYNC_PROP_SET(
        selected_postproc,
        mSelectedPostProc.Ptr(),
        mSelectedPostProc = _val.Obj<RndPostProc>()
    )
    SYNC_SUPERCLASS(RndPollable)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(RndPostProcMgr)
    SAVE_REVS(0, 0)
    SAVE_SUPERCLASS(RndPollable)
    bs << mSelectedPostProc;
END_SAVES

BEGIN_COPYS(RndPostProcMgr)
    COPY_SUPERCLASS(RndPollable)
    CREATE_COPY(RndPostProcMgr)
    BEGIN_COPYING_MEMBERS
        mSelectedPostProc = c->mSelectedPostProc.Ptr();
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(0, 0)

BEGIN_LOADS(RndPostProcMgr)
    LOAD_REVS(bs)
    ASSERT_REVS(0, 0)
    Hmx::Object::Load(bs);
    bs >> mSelectedPostProc;
END_LOADS

void RndPostProcMgr::Poll() {
    if (mSelectedPostProc) {
        if (IsEnabled()) {
            mSelectedPostProc->Select();
        } else {
            mSelectedPostProc->Unselect();
        }
        if (mBlendToPostProc) {
            float pct = (TheTaskMgr.Seconds(TaskMgr::kRealTime) - mBlendStartTime) / mBlendDuration;
            pct = Clamp(0.0f, 1.0f, pct);
            mSelectedPostProc->Interp(mBlendFromPostProc, mBlendToPostProc, pct);
            if (NearlyOne(pct)) {
                mBlendToPostProc = nullptr;
                mBlendDuration = -1;
                mBlendStartTime = -1;
            }
        }
    }
}

void RndPostProcMgr::Enter() {
    if (mSelectedPostProc && IsEnabled()) {
        mSelectedPostProc->Select();
    }
}

void RndPostProcMgr::Exit() {
    if (mSelectedPostProc) {
        mSelectedPostProc->Unselect();
    }
}

bool RndPostProcMgr::IsEnabled() const {
    static DataNode &n = DataVariable("disable_postproc");
    return !n.Int();
}

void RndPostProcMgr::CopyFromPostProc(RndPostProc *iPostProc) {
    MILO_ASSERT(iPostProc, 0x47);
    if (iPostProc != mSelectedPostProc && mSelectedPostProc) {
        mSelectedPostProc->Copy(iPostProc, kCopyShallow);
    }
}

void RndPostProcMgr::BlendToPostProc(RndPostProc *iPostProc, float iBlendTime) {
    MILO_ASSERT(iBlendTime >= 0.0f, 0x5F);
    MILO_ASSERT(iPostProc, 0x60);
    if (iPostProc != mSelectedPostProc) {
        if (NearlyZero(iBlendTime)) {
            CopyFromPostProc(iPostProc);
        } else if (mSelectedPostProc) {
            mBlendFromPostProc->Copy(mSelectedPostProc, kCopyShallow);
            mBlendToPostProc = iPostProc;
            mBlendDuration = iBlendTime;
            mBlendStartTime = TheTaskMgr.Seconds(TaskMgr::kRealTime);
        }
    }
}

RndPostProc *RndPostProcMgr::MsgToPostProc(DataArray *iMsg) {
    MILO_ASSERT(iMsg, 0x116);
    RndPostProc *result = 0;
    if (iMsg->Size() > 2) {
        DataType t = iMsg->Type(2);
        if (t == kDataObject) {
            result = iMsg->Obj<RndPostProc>(2);
        } else if (t == kDataSymbol || t == kDataString) {
            const char *name = iMsg->Str(2);
            result = Dir()->Find<RndPostProc>(name, false);
            if (!result) {
                MILO_NOTIFY("could not find post-proc %s", (char *)name);
            }
        } else {
            MILO_NOTIFY("unexpected post-proc data type %d", t);
        }
    } else {
        MILO_NOTIFY("not enough arguments supplied to OnSetPostProc");
    }
    return result;
}

DataNode RndPostProcMgr::OnCopyFromPostProc(DataArray *a) {
    RndPostProc *pp = MsgToPostProc(a);
    if (pp) {
        CopyFromPostProc(pp);
    }
    return 0;
}

DataNode RndPostProcMgr::OnBlendToPostProc(DataArray *a) {
    RndPostProc *pp = MsgToPostProc(a);
    if (pp) {
        float blend = 0;
        if (a->Size() > 3) {
            blend = Max(blend, a->Float(3));
        }
        BlendToPostProc(pp, blend);
    }
    return 0;
}
