// Retail's ??0?$ObjOwnerPtr@VCharWeightable@@ emits {lis, mOwner, mObject,
// cmplwi, addi, vptr-store}: BOTH member stores sit after the vtable
// materialization. Our default spelling initializes mOwner in the base
// mem-init list, which puts its store in the base ctor's scheduling region --
// free to float above the `lis` -- giving {mOwner, lis, mObject, ...}. The
// defer-both gate pins it. See obj/Object.h.
#define RB3_TU_OBJPTR_DEFER_OWNER
#include "char/CharWeightable.h"
#include "obj/Object.h"
#include "synth_xbox/PitchCorrectedVoice.h"

void TrueColor::ExposureRecipe::SetGlobalGain(float f) { mField_0x08 = f; }

CharWeightable::CharWeightable() : mWeight(1), mWeightOwner(this, this) {}

// Retail 0x823ae888 (144 B).  Two corrections, both read off the bytes:
//  1. There is NO Hmx::Object::Replace fallback -- the only calls are
//     __RTDynamicCast and SetOwnerObj (x2), then blr.
//  2. The `restore to this` arm is NOT the else of the match; it is a SECOND,
//     UNCONDITIONAL statement.  Retail re-reads the member after the assignment
//     (`lwz r11,-8(r31); cmpwi r11,0` at 0x823ae8ec) and both paths fall into
//     it -- the non-match branch at 0x823ae8c0 lands exactly there.  So a
//     Replace that did not target mWeightOwner can still repair a null one.
void CharWeightable::Replace(ObjRef *ref, Hmx::Object *obj) {
    if (RefIs(ref, mWeightOwner))
        mWeightOwner.SetOwnerObj(dynamic_cast<CharWeightable *>(obj));
    if (!mWeightOwner.Ptr())
        mWeightOwner.SetOwnerObj(this);
#ifdef HX_NATIVE
    Hmx::Object::Replace(ref, obj);
#endif
}

BEGIN_HANDLERS(CharWeightable)
#ifdef HX_NATIVE
    HANDLE_VIRTUAL_SUPERCLASS(Hmx::Object)
#else
    /* Retail 0x823AEE98 (204 B) is ONE body shared by ?Handle@CharData@@ and
     * ?Handle@CharWeightable@@ -- retail's vtordisp thunks for both classes
     * folded to 0x823AF220, which branches there (lane W16-U, 2026-09-14).
     * Our CharData::Handle is masked-EQUAL to those 204 B; our CharWeightable
     * version was 268 B.  The whole 64 B surplus is the
     * `if (ClassName() == StaticClassName())` guard that
     * HANDLE_VIRTUAL_SUPERCLASS adds, evidenced by the one extra relocation
     * ?StaticClassName@CharWeightable@@ that CharData::Handle does not carry.
     * Same defect as the SyncProperty note below and RndTransformable's. */
    HANDLE_SUPERCLASS(Hmx::Object)
#endif
END_HANDLERS

BEGIN_PROPSYNCS(CharWeightable)
    SYNC_PROP_SET(weight, mWeight, SetWeight(_val.Float()))
    SYNC_PROP_SET(
        weight_owner, mWeightOwner.Ptr(), SetWeightOwner(_val.Obj<CharWeightable>())
    )
    /* Retail's ?SyncProperty@CharWeightable@@UAA... (0x823AEF98, 472 B, 118
     * instrs) has no superclass sync: it ends at 0x823AF164 with `li r3,0` and
     * its only indirect call is a slot-0 vtable dispatch in the weight_owner
     * arm -- not the slot-4 ClassName() dispatch that SYNC_VIRTUAL_SUPERCLASS
     * emits, and there is no StaticClassName() call anywhere in the body.
     * Same defect as RndTransformable::SyncProperty. */
END_PROPSYNCS

// Retail 0x823AEE20: rev 2, no Hmx::Object superclass save.
BEGIN_SAVES(CharWeightable)
    SAVE_REVS(2, 0)
    bs << mWeight;
    bs << mWeightOwner;
END_SAVES

BEGIN_COPYS(CharWeightable)
    /* Retail (0x823AE918, 0x88 B) has NO superclass copy: the body opens
     * straight into __RTDynamicCast, saves only r29-r31 and uses a 0x70 frame.
     * COPY_VIRTUAL_SUPERCLASS expands to `if (ClassName() == StaticClassName())
     * Hmx::Object::Copy(o, ty);`, which cost 26 surplus instructions, two Symbol
     * temps (frame 0x80) and one extra callee-save. Same defect class as this
     * TU's SyncProperty (see the END_PROPSYNCS note above) and as Save/Load
     * below, which have no superclass call either. */
    CREATE_COPY(CharWeightable)
    BEGIN_COPYING_MEMBERS
        if (ty == kCopyShallow) {
            SetWeightOwner(c->mWeightOwner);
        } else {
            SetWeightOwner(this);
            mWeight = c->mWeightOwner->mWeight;
        }
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(2, 0)

// Retail 0x823AF230 keeps no BinStreamRev: the packed rev goes into one aligned
// file-scope aggregate (altRev +0, rev +4), both fields come from the raw
// stream, and there is no Hmx::Object superclass load.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_CharWeightable;

BEGIN_LOADS(CharWeightable)
    int rev;
    bs >> rev;
    gRevs_CharWeightable.rev = getHmxRev(rev);
    gRevs_CharWeightable.altRev = getAltRev(rev);
    bs >> mWeight;
    if (gRevs_CharWeightable.rev > 1) {
        bs >> mWeightOwner;
    }
END_LOADS
