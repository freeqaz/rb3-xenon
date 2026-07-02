#include "synth/Faders.h"
#include <functional>
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "utl/BinStream.h"
#include "utl/Symbols.h"
#include "decomp.h"

std::list<FaderTask *> FaderTask::sTasks;

Fader::Fader() : mVal(0.0f), mFaderTask(0), mLocalName(), mMode(kLinear) {}

Fader::~Fader() { CancelFade(); }

void Fader::SetVal(float f) {
    CancelFade();
    UpdateValue(f);
}

void Fader::SetMode(Fader::Mode mode) {
    MILO_ASSERT(mFaderTask == NULL, 0x2C);
    mMode = mode;
}

void Fader::DoFade(float targetDb, float duration) {
    CancelFade();
    MILO_ASSERT(duration >= 0.0f, 0x35);
    mFaderTask = new FaderTask();
    MILO_ASSERT(mFaderTask != NULL, 0x3B);
    MILO_ASSERT(mFaderTask != (FaderTask *)0x01000000, 0x3C);
    switch (mMode) {
    case kLinear:
        mFaderTask->mInterp = new LinearInterpolator(mVal, targetDb, 0.0f, duration);
        break;
    case kExp:
        mFaderTask->mInterp = new ExpInterpolator(mVal, targetDb, 0.0f, duration, 2.0f);
        break;
    case kInvExp:
        mFaderTask->mInterp =
            new InvExpInterpolator(mVal, targetDb, 0.0f, duration, 2.0f);
        break;
    default:
        MILO_ASSERT(false, 0x54);
        break;
    }
    mFaderTask->mTimer.Start();
    mFaderTask->mFader = this;
    mFaderTask->mDone = false;
    FaderTask *thetask = mFaderTask;
    FaderTask::sTasks.push_back(thetask);
}

bool Fader::IsFading() const { return mFaderTask; }

float Fader::GetTargetDb() const {
    if (mFaderTask) {
        switch (mMode) {
        case kExp:
            return static_cast<ExpInterpolator *>(mFaderTask->mInterp)->Y1();
        case kInvExp:
            return static_cast<InvExpInterpolator *>(mFaderTask->mInterp)->Y1();
        case kLinear:
        default:
            return static_cast<LinearInterpolator *>(mFaderTask->mInterp)->Y1();
        }
    } else
        return mVal;
}

void Fader::AddClient(FaderGroup *group) { mClients.insert(group); }

void Fader::RemoveClient(FaderGroup *group) { mClients.erase(group); }

void Fader::CancelFade() {
    if (mFaderTask) {
        Check();
        mFaderTask->mDone = true;
        mFaderTask = 0;
    }
}

void Fader::UpdateValue(float val) {
    mVal = val;
    std::for_each(mClients.begin(), mClients.end(), std::mem_fun(&FaderGroup::SetDirty));
}

SAVE_OBJ(Fader, 0x9B)

BEGIN_LOADS(Fader)
    int rev;
    bs >> rev;
    ASSERT_GLOBAL_REV(rev, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    float f;
    bs >> f;
    SetVal(f);
END_LOADS

BEGIN_COPYS(Fader)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(Fader)
    if (c) {
        SetVal(c->mVal);
    }
END_COPYS

BEGIN_PROPSYNCS(Fader)
    SYNC_PROP_MODIFY(level, mVal, SetVal(mVal))
END_PROPSYNCS

BEGIN_HANDLERS(Fader)
    HANDLE_ACTION(set_val, SetVal(_msg->Float(2)))
    HANDLE_ACTION(fade, DoFade(_msg->Float(2), _msg->Float(3)))
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0xCE)
END_HANDLERS

FaderGroup::FaderGroup(Hmx::Object *o) : mFaders(o), mDirty(true) {}

FaderGroup::~FaderGroup() {
    while (!mFaders.empty()) {
        Fader *frontObj = mFaders.front();
        mFaders.pop_front();
        frontObj->RemoveClient(this);
        if (!frontObj->LocalName().Null()) {
            delete frontObj;
        }
    }
}

Fader *FaderGroup::AddLocal(Symbol name) {
    MILO_ASSERT(!name.Null(), 0xEA);
    ObjPtrList<Fader>::iterator it = mFaders.begin();
    for (; it != mFaders.end(); ++it) {
        if ((*it)->LocalName() == name)
            return *it;
    }
    Fader *f = Hmx::Object::New<Fader>();
    f->SetLocalName(name);
    f->AddClient(this);
    mFaders.push_back(f);
    mDirty = true;
    return f;
}

void FaderGroup::Add(Fader *f) {
    ObjPtrList<Fader>::iterator it = mFaders.begin();
    for (; it != mFaders.end(); ++it) {
        if (*it == f)
            return;
    }
    f->AddClient(this);
    mFaders.push_back(f);
    mDirty = true;
}

void FaderGroup::Remove(Fader *f) {
    for (ObjPtrList<Fader>::iterator it = mFaders.begin(); it != mFaders.end(); ++it) {
        if (*it == f) {
            f->RemoveClient(this);
            mFaders.erase(it);
            mDirty = true;
            return;
        }
    }
}

float FaderGroup::GetVal() {
    float sum = 0.0f;
    for (ObjPtrList<Fader>::iterator it = mFaders.begin(); it != mFaders.end(); ++it) {
        sum += (*it)->GetVal();
    }
    return sum;
}

bool FaderGroup::Dirty() { return mDirty; }
void FaderGroup::SetDirty() { mDirty = true; }
void FaderGroup::ClearDirty() { mDirty = false; }

Fader *FaderGroup::FindLocal(Symbol sym, bool fail) {
    if (!sym.Null()) {
        for (ObjPtrList<Fader>::iterator it = mFaders.begin(); it != mFaders.end();
             ++it) {
            if ((*it)->LocalName() == sym)
                return *it;
        }
    }
    if (fail)
        MILO_FAIL("bad local fader: %s", sym);
    return 0;
}

void FaderGroup::Print(TextStream &ts) {
    ts << "FaderGroup (size " << mFaders.size() << ")\n";
    for (ObjPtrList<Fader>::iterator it = mFaders.begin(); it != mFaders.end(); ++it) {
        String str((*it)->LocalName().Str());
        if (str.empty())
            str = (*it)->Name();
        else
            str += "(local)";
        if (str.empty())
            str = "(anon)";
        float val = (*it)->GetVal();
        ts << "  " << str << ": " << val << "\n";
    }
}

DECOMP_FORCEACTIVE(Faders, "ObjPtr_p.h", "c.Owner()", "")

#define kGroupRev 0

// fn_80670CA8
void FaderGroup::Load(BinStream &bs) {
    int rev;
    bs >> rev;
    MILO_ASSERT(rev <= kGroupRev, 0x187);
    ObjPtrList<Fader> pList(mFaders.Owner(), kObjListNoNull);
    bs >> pList;
    for (ObjPtrList<Fader>::iterator it = mFaders.begin(); it != mFaders.end(); it) {
        Fader *f = *it++;
        if (f->Dir() && f->LocalName().Null())
            Remove(f);
    }
    for (ObjPtrList<Fader>::iterator it = pList.begin(); it != pList.end(); ++it) {
        Add(*it);
    }
}

// fn_80670F64
bool PropSync(FaderGroup &grp, DataNode &node, DataArray *prop, int i, PropOp op) {
    ObjPtrList<Fader> pList(grp.mFaders);
    for (ObjPtrList<Fader>::iterator it = pList.begin(); it != pList.end(); it) {
        if ((*it)->Dir() && (*it)->LocalName().Null())
            ++it;
        else
            it = pList.erase(it);
    }
    bool sync = PropSync(pList, node, prop, i, op);
    for (ObjPtrList<Fader>::iterator it = grp.mFaders.begin(); it != grp.mFaders.end();
         it) {
        Fader *f = *it++;
        if (f->Dir() && f->LocalName().Null())
            grp.Remove(f);
    }
    for (ObjPtrList<Fader>::iterator it = pList.begin(); it != pList.end(); ++it) {
        grp.Add(*it);
    }
    return sync;
}

FaderTask::FaderTask() : mTimer(), mInterp(0), mFader(0), mDone(0) {}

FaderTask::~FaderTask() { RELEASE(mInterp); }

void FaderTask::PollAll() {
    for (std::list<FaderTask *>::iterator it = sTasks.begin(); it != sTasks.end(); it) {
        if ((*it)->mDone) {
            delete *it;
            it = sTasks.erase(it);
        } else {
            (*it)->Poll();
            ++it;
        }
    }
}

// Our verified-360 Interpolator base (see math/Interp.h) carries no mX1/mY1
// fields and doesn't expose them virtually — unlike rb3-Wii's base, which
// stored those endpoints directly on Interpolator and let FaderTask::Poll
// read mInterp->mX1/mInterp->mY1 without knowing the concrete type. Since we
// can't add fields or virtuals to the shared Interpolator/ATanInterpolator
// classes without a layout/vtable change, resolve the concrete interpolator
// type via Fader::mMode (set once in Fader::DoFade, read-only here) and pull
// X1()/Y1() off the concrete class instead.
void FaderTask::Poll() {
    MILO_ASSERT(!mDone, 0x1DE);
    MILO_ASSERT(mInterp != NULL, 0x1DF);
    float f = mTimer.SplitMs();
    float x1, y1;
    switch (mFader->mMode) {
    case Fader::kExp: {
        ExpInterpolator *interp = static_cast<ExpInterpolator *>(mInterp);
        x1 = interp->X1();
        y1 = interp->Y1();
        break;
    }
    case Fader::kInvExp: {
        InvExpInterpolator *interp = static_cast<InvExpInterpolator *>(mInterp);
        x1 = interp->X1();
        y1 = interp->Y1();
        break;
    }
    case Fader::kLinear:
    default: {
        LinearInterpolator *interp = static_cast<LinearInterpolator *>(mInterp);
        x1 = interp->X1();
        y1 = interp->Y1();
        break;
    }
    }
    if (f > x1) {
        mFader->UpdateValue(y1);
        mFader->CancelFade();
    } else {
        mFader->UpdateValue(mInterp->Eval(f));
    }
}

void Fader::Check() {
    MILO_ASSERT(mFaderTask != (FaderTask *)0x01000000, 0x20C);
    MILO_ASSERT(mFaderTask != NULL, 0x20D);
}

DECOMP_FORCEFUNC(Faders, Fader, SetType)
DECOMP_FORCEFUNC_TEMPL(Faders, ObjPtrList, Replace(0, 0), Fader, ObjectDir)
DECOMP_FORCEFUNC_TEMPL(Faders, ObjPtrList, RefOwner(), Fader, ObjectDir)
