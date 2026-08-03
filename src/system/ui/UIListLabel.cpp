// Retail INLINES the ObjPtr<UILabel> owner-only ctor at ??0UIListLabel@@'s
// `mLabel(this)` member-init: 0x8281F7D0 has no `bl ??0?$ObjPtr@VUILabel@@`,
// just the three stores (mOwner 0x74, mObject 0x78 = 0, vtable 0x70).  The _EH
// variant is the right one of the family here because retail's `stw &mLabel,
// 0x50(r31)` $T store is LIVE -- i.e. the EH cleanup region for the
// partially-constructed base survives, which is exactly what that define
// preserves.  Measured on this TU (lane CU-3), unit default/UIListLabel:
//     (none)                            22/24  code 2428  ctor 69.62%
//     RB3_OBJPTR_INLINE_OWNER_CTOR_EH   23/24  code 2468  ctor 87.81%   <== this
//     RB3_OBJPTR_INLINE_OWNER_CTOR      22/24  code 2428  ctor 58.23%
//     RB3_TU_OBJPTR_FORCEINLINE_CTOR    23/24  code 2468  ctor 87.81%
// The +1 is the EH funclet fn_8281F860 (40 B, 99.90% -> 100%), not the ctor.
// The ctor's residual 87.81% is size-exact (104 B both sides, every instruction
// present) and is purely a scheduler tie-break: retail emits $T, vptr, lis,
// li 0, mOwner, addi, mObject, ObjPtr-vptr; we hoist the `li 0` and sink the
// lis/addi pair.  That is VERBATIM the residual obj/Object.h already documents
// for this define ("we emit mObject/mOwner then lis+addi; retail emits mOwner,
// lis, mObject, addi -- NOT source-steerable").
//
// ★ RE-TESTED (lane DI-2/C, ctor now reads 93.08%): the residual is exactly 3
// instructions, a cyclic rotation of {lis, li 0, stw mOwner} -- retail issues
// them in that order, we issue {stw mOwner, lis, li 0}.  THREE structurally
// distinct ObjPtr ctor spellings were compiled and all three emit BYTE-IDENTICAL
// code for this ctor (same 93.08%, same 3 replaces):
//     RB3_OBJPTR_INLINE_OWNER_CTOR_EH                        (this)
//     RB3_OBJPTR_INLINE_TWOARG_CTOR                          (in-class 2-arg body)
//     RB3_OBJPTR_INLINE_OWNER_CTOR + ..._DEFER_OBJECT        (1-arg base + body)
// (The old note's claim that the DEFER pairing "falls back to 58.23% because
// RB3_OBJPTR_INLINE_OWNER_CTOR wins the #ifdef chain" is wrong on both counts --
// DEFER_OBJECT is NESTED inside that define, and the pairing measures 93.08%.)
// Retail's order is precisely a critical-path-first list schedule of the block
// (lis->addi->stw = 3-node chain, li->stw = 2, stw mOwner = 1); ours emits the
// mOwner store first because it belongs to the BASE-ctor scheduling region,
// which the EH state transition separates from the derived body.  Since that
// same EH region is what keeps the $T store (and the +1 funclet) alive, the two
// requirements are in tension and no ObjPtr-ctor spelling satisfies both.
// Conclusion unchanged, now on three-way evidence: scheduler wall, not source.
//
// ★★ RESOLVED (lane DS-4/C): the "cyclic rotation of {lis, li 0, stw mOwner}"
// above IS source-steerable after all, and the three-way byte-identity result
// is what should have given it away -- all three spellings left mOwner in the
// BASE mem-init list, so for THIS store they were one experiment run three
// times, not three experiments. A store emitted from the base mem-init sits in
// the base ctor's scheduling region and may float above the derived vptr
// materialization; a store emitted from the derived BODY is pinned after it.
// That is the identical mechanism obj/Object.h already documents for mObject
// under ..._DEFER_OBJECT -- it simply had never been applied to mOwner.
// Deferring BOTH members takes this ctor 92.3% -> 100%.
#define RB3_TU_OBJPTR_DEFER_OWNER
#define RB3_OBJPTR_INLINE_OWNER_CTOR_EH
#include "ui/UIListLabel.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Utl.h"
#include "ui/UILabel.h"
#include "ui/UIListSlot.h"
#include "utl/Symbol.h"
#ifdef HX_NATIVE
#include <cstdio>
#include <cstdlib>
#include <cstring>
#endif

#ifdef HX_NATIVE
namespace {
bool DebugChooseModeLabel(const UILabel *label) {
    static int enabled = -1;
    if (enabled == -1) {
        const char *env = getenv("MILO_DEBUG_CHOOSE_MODE");
        enabled = (env && env[0] && strcmp(env, "0") != 0) ? 1 : 0;
    }
    if (!enabled || !label) {
        return false;
    }
    const char *path = PathName(label);
    return path && strstr(path, "choose_mode");
}
}
#endif

#pragma region UIListLabel

UIListLabel::UIListLabel() : mLabel(this) {}

RndTransformable *UIListLabel::RootTrans() { return mLabel; }

BEGIN_HANDLERS(UIListLabel)
    HANDLE_SUPERCLASS(UIListSlot)
END_HANDLERS

BEGIN_PROPSYNCS(UIListLabel)
    SYNC_PROP(label, mLabel)
    SYNC_SUPERCLASS(UIListSlot)
END_PROPSYNCS

BEGIN_SAVES(UIListLabel)
    SAVE_REVS(1, 0)
    SAVE_SUPERCLASS(UIListSlot)
    bs << mLabel;
END_SAVES

BEGIN_COPYS(UIListLabel)
    COPY_SUPERCLASS(UIListSlot)
    CREATE_COPY_AS(UIListLabel, l)
    MILO_ASSERT(l, 0xba);
    COPY_MEMBER_FROM(l, mLabel)
END_COPYS

// RB3-360 retail rev storage (same shape already proven in char/CharBone.cpp).
// Retail's Load keeps NO BinStreamRev: it splits the packed rev word into two
// mutable shorts and stores them at base+0 (altRev) / base+4 (rev), then calls
// the superclass Load on the raw stream.  Object.h's LOAD_REVS instead builds a
// BinStreamRev temporary (+0x30 of frame, a ctor/dtor pair and a vtable store),
// which is what dropped ?Load@UIListLabel@@ to 41%.  Written LONGHAND rather
// than switching this TU to the ObjMacros.h dialect, because that dialect also
// redefines SYNC_PROP and would perturb SyncProperty (which matches at 100%).
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_UIListLabel;
#define gAltRev gRevs_UIListLabel.altRev
#define gRev gRevs_UIListLabel.rev

BEGIN_LOADS(UIListLabel)
    int rev;
    bs >> rev;
    gRev = getHmxRev(rev);
    gAltRev = getAltRev(rev);
    UIListSlot::Load(bs);
    bs >> mLabel;
END_LOADS

const char *UIListLabel::GetDefaultText() const {
    if (mLabel)
        return mLabel->GetDefaultText();
    return gNullStr;
}

UILabel *UIListLabel::ElementLabel(int display) const {
    size_t size = mElements.size();
    if (size == 0)
        return 0;

    MILO_ASSERT((0) <= (display) && (display) < (size), 0x74);
    UIListLabelElement *le = dynamic_cast<UIListLabelElement *>(mElements[display]);
    MILO_ASSERT(le, 0x77);
    return le->mLabel;
}

UIListSlotElement *UIListLabel::CreateElement(UIList *uilist) {
    MILO_ASSERT(mLabel, 0x86);
    Hmx::Object *newObj = Hmx::Object::NewObject(mLabel->ClassName());
    UILabel *l = dynamic_cast<UILabel *>(newObj);
    MILO_ASSERT(l, 0x89);
    l->ResourceCopy(mLabel);
    l->SetTextToken(gNullStr);
    return new UIListLabelElement(this, l);
}

#pragma endregion UIListLabel
#pragma region UIListLabelElement

UIListLabelElement::~UIListLabelElement() { delete mLabel; }

// RB3-360 retail implements rb3-Wii's RB3-era algorithm, NOT DC3's newer one.
// Our previous body was a verbatim copy of DC3's multi-style version
// (BoundsLeft/BoundsTop + _alloca over NumStyles()/Style(i)); retail instead
// walks the label's meshes.  This is not an oracle preference -- it is what the
// retail bytes at 0x8281FED8 call: ?CalcBox@@YAXPAVRndMesh@@AAVBox@@@Z,
// ?GrowToContain@Box@@QAAXABVVector3@@_N@Z, ?TextObj@UILabel@@, and
// ?SetColorOverride@UILabel@@, with NO BoundsLeft and NO NumStyles anywhere in
// the 428-byte body.  Single-alpha (mAlpha 0x1bc / mAltAlpha 0x1f4, both
// compiler-verified) rather than DC3's per-style alpha array.
void UIListLabelElement::Draw(const Transform &tf, float f, UIColor *col, Box *box) {
    mLabel->SetWorldXfm(tf);
    if (box) {
        Box localbox(box->mMin, box->mMax);
        std::vector<RndMesh *> vec;
        mLabel->TextObj()->GetMeshes(vec);
        for (int i = 0; i < vec.size(); i++) {
            Box vecbox;
            CalcBox(vec[i], vecbox);
            localbox.GrowToContain(vecbox.mMin, false);
            localbox.GrowToContain(vecbox.mMax, false);
        }
        box->GrowToContain(localbox.mMin, false);
        box->GrowToContain(localbox.mMax, false);
    } else {
        float oldalpha = mLabel->Alpha();
        float oldaltalpha = mLabel->AltAlpha();
        mLabel->SetColorOverride(col);
        mLabel->SetAlpha(f * oldalpha);
        mLabel->SetAltAlpha(f * oldaltalpha);
        mLabel->DrawShowing();
        mLabel->SetAlpha(oldalpha);
        mLabel->SetAltAlpha(oldaltalpha);
    }
}

#pragma endregion UIListLabelElement
