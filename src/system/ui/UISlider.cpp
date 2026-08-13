#include "ui/UISlider.h"
#include "UIComponent.h"
#include "math/Mtx.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/JoypadMsgs.h"
#include "rndobj/Draw.h"
#include "ui/UI.h"
#include "ui/UIPanel.h"
#include "ui/UIResource.h"
#include "ui/Utl.h"
#include "utl/BinStream.h"
#include "utl/Symbol.h"

UISlider::UISlider() : mCurrent(0), mNumSteps(10), mVertical(0) {}

BEGIN_HANDLERS(UISlider)
    HANDLE_MESSAGE(ButtonDownMsg)
    HANDLE_EXPR(current, mCurrent)
    HANDLE_EXPR(num_steps, mNumSteps)
    HANDLE_EXPR(frame, Frame())
    HANDLE_ACTION(set_num_steps, SetNumSteps(_msg->Int(2)))
    HANDLE_ACTION(set_current, SetCurrent(_msg->Int(2)))
    HANDLE_ACTION(set_frame, SetFrame(_msg->Float(2)))
    HANDLE_ACTION(store, Store())
    HANDLE_ACTION(undo, RevertScrollSelect(this, _msg->Obj<LocalUser>(2), 0))
    HANDLE_ACTION(
        undo_handled_by,
        RevertScrollSelect(this, _msg->Obj<LocalUser>(2), _msg->Obj<UIPanel>(3))
    )
    HANDLE_ACTION(confirm, Reset())
    HANDLE_SUPERCLASS(ScrollSelect)
    HANDLE_SUPERCLASS(UIComponent)
END_HANDLERS

BEGIN_PROPSYNCS(UISlider)
    // Retail has NO own-property arm here: the target body goes straight from
    // the `Symbol sym = _prop->Sym(_i)` preamble to SyncProperty(ScrollSelect).
    SYNC_SUPERCLASS(ScrollSelect)
    SYNC_SUPERCLASS(UIComponent)
END_PROPSYNCS

BEGIN_SAVES(UISlider)
    SAVE_REVS(3, 0)
    SAVE_SUPERCLASS(UIComponent)
    bs << mSelectToScroll;
    bs << mVertical;
END_SAVES

BEGIN_COPYS(UISlider)
    COPY_SUPERCLASS(UIComponent)
    CREATE_COPY_AS(UISlider, c)
    BEGIN_COPYING_MEMBERS_FROM(c)
        COPY_MEMBER(mSelectToScroll)
        COPY_MEMBER(mVertical)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_LOADS(UISlider)
    PreLoad(bs);
    PostLoad(bs);
END_LOADS

void UISlider::SetTypeDef(DataArray *def) {
    Hmx::Object::SetTypeDef(def);
    Update();
}

INIT_REVS(3, 0)

void UISlider::PreLoad(BinStream &bs) {
    LOAD_REVS(bs);
    ASSERT_REVS(3, 0);
    UIComponent::PreLoad(d.stream);
    d.PushRev(this);
}

void UISlider::PostLoad(BinStream &bs) {
    BinStreamRev d(bs, bs.PopRev(this));
    UIComponent::PostLoad(d.stream);
    if (d.rev > 0) {
        d >> mSelectToScroll;
    }
    if (d.rev > 1) {
        d >> mVertical;
    }
    Update();
}

void UISlider::DrawShowing() { SyncSlider(); }

RndDrawable *UISlider::CollideShowing(const Segment &s, float &fl, Plane &pl) {
    SyncSlider();
    return nullptr;
}

// retail 0x8280A320 (88 B). We returned a bare 0; retail forwards to the
// resource dir. Note the neighbouring DrawShowing/CollideShowing already match
// at 100% with our simpler bodies even though the Wii oracle has richer ones --
// so this was ported per-row off the retail size, not by trusting the oracle.
int UISlider::CollidePlane(const Plane &pl) {
    SyncSlider();
    RndDir *dir = mResource->Dir();
    return dir->CollidePlane(pl);
}

void UISlider::Enter() {
    UIComponent::Enter();
    Reset();
}

void UISlider::SetCurrent(int i) {
    if (i < 0 || i >= mNumSteps) {
        MILO_FAIL("Can't set slider to %i (%i steps)", i, mNumSteps);
    } else
        mCurrent = i;
}

int UISlider::SelectedAux() const { return Current(); }

void UISlider::SetSelectedAux(int i) { SetCurrent(i); }

void UISlider::OldResourcePreload(BinStream &bs) {
    char buf[256];
    bs.ReadString(buf, 256);
}

// retail fn_8280A000 (132 B). This was an empty stub, so MSVC inlined it to
// NOTHING at all three call sites -- which is why CollidePlane above emitted a
// bare tail-call with no `bl` at all. Retail's body, read off the bytes:
//   lwz 0x108 -> mResource, lwz 0x14 -> Dir(), addi 0xc4 = the RndAnimatable
//   sub-object (vtable slot 0xc = SetFrame), with lbl_820009FC = 1.0f;
//   then addi 0xd4 = the RndTransformable sub-object for SetWorldXfm, whose
//   argument is WorldXfm() (the mCached test at 0xc0 + fn_823F7A80).
void UISlider::SyncSlider() {
    mResource->Dir()->SetFrame(Frame(), 1.0f);
    mResource->Dir()->SetWorldXfm(WorldXfm());
}

float UISlider::Frame() const {
    if (mNumSteps == 1)
        return 0.0f;
    else
        return (float)(mCurrent) / (float)(mNumSteps - 1);
}

void UISlider::SetNumSteps(int i) {
    if (i < 1)
        MILO_FAIL("Can't set num steps to %i (must be >= 1)", i);
    else
        mNumSteps = i;
}

void UISlider::SetFrame(float frame) {
    MILO_ASSERT(frame >= 0 && frame <= 1.0f, 0xe2);
    mCurrent = frame * (mNumSteps - 1) + 0.5f;
}

int UISlider::Current() const { return mCurrent; }

void UISlider::Init() { REGISTER_OBJ_FACTORY(UISlider) }

void UISlider::Update() {
    if (TypeDef()) {
        TypeDef()->FindData("vertical", mVertical, false);
    }
}

DataNode UISlider::OnMsg(const ButtonDownMsg &msg) {
    Symbol cnttype = JoypadControllerTypePadNum(msg.GetPadNum());
    if (CanScroll()) {
        int act = ScrollDirection(msg, cnttype, mVertical, 1);
        if (act != kAction_None) {
            if (mVertical) {
                act = (JoypadAction)-act;
            }
            int step = mCurrent + act;
            if (step >= 0 && step < mNumSteps) {
                SetCurrent(step);
                TheUI->Handle(UIComponentScrollMsg(this, msg.GetUser()), false);
            }
            return 1;
        }
        if (CatchNavAction(msg.GetAction())) {
            return 1;
        }
    }
    JoypadAction thisAct = msg.GetAction();
    LocalUser *user = msg.GetUser();
    if (thisAct == kAction_Confirm && SelectScrollSelect(this, user)) {
        return 1;
    } else if (thisAct == kAction_Cancel && RevertScrollSelect(this, user, 0)) {
        return 1;
    }
    return DATA_UNHANDLED;
}

// ---------------------------------------------------------------------------
// lane-AE batch-3 (sw3) scatter-include: retail placed PanelDir's destructor
// COMDATs inside the .text span pinned to default/UISlider:
//   ??1PanelDir@@UAA@XZ   (464 B)
//   ??_DPanelDir@@QAAXXZ  (108 B, vbase destructor)
// They are out-of-line definitions owned by ui/PanelDir.cpp, so an odr-use
// cannot force them here -- the whole owner TU has to be pulled in. gRev /
// gAltRev come from SAVE_REVS and would collide with this TU's own pair.
#define gRev gRev_PanelDir
#define gAltRev gAltRev_PanelDir
#include "ui/PanelDir.cpp"
#undef gRev
#undef gAltRev
